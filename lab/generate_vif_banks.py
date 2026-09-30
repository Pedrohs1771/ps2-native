"""Conversion-only compiler for all finite banks observed in one VIF case.

Cache identity includes code, inspector, emitter and this converter. Cached
descriptors are validated again before emission. This is conversion reuse,
not a certificate of semantic correctness or closure of unobserved game code.
"""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import tempfile

EMITTER_PATH = Path(__file__).with_name("generate_vu_bank.py")
SPEC = importlib.util.spec_from_file_location("nexo_vif_bank_emitter", EMITTER_PATH)
EMITTER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(EMITTER)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def canonical(value):
    return json.dumps(value, sort_keys=True, separators=(",", ":")).encode()


def bounded(path, maximum):
    # Read at most the bound even if a file grows after opening it.
    with path.open("rb") as stream:
        data = stream.read(maximum + 1)
    if len(data) > maximum:
        raise ValueError(f"oversized conversion input: {path.name}")
    return data


def write_changed(path, data):
    """Publish atomically without invalidating unchanged C++ object files."""
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.is_file() and path.read_bytes() == data:
        return
    name = None
    try:
        with tempfile.NamedTemporaryFile(dir=path.parent, delete=False) as stream:
            name = stream.name
            stream.write(data)
        os.replace(name, path)
        name = None
    finally:
        if name is not None:
            Path(name).unlink(missing_ok=True)


def observed_banks(case):
    try:
        if bounded(case / ".complete", 1) != b"\x01":
            raise ValueError("VIF case is incomplete")
        manifest = json.loads(bounded(case / "capture.json", 65536))
    except (OSError, json.JSONDecodeError) as error:
        raise ValueError("missing or invalid completed VIF case") from error
    if not isinstance(manifest, dict) or manifest.get("schema") != "nexo.observed.vif.call.v1":
        raise ValueError("unsupported observed VIF case schema")
    count, calls = manifest.get("executed_banks"), manifest.get("vu_calls")
    if type(count) is not int or type(calls) is not int or not 1 <= count <= calls <= 256:
        raise ValueError("invalid observed VIF bank/callback counts")
    if type(manifest.get("callback_horizon")) is not int or manifest["callback_horizon"] != 65536:
        raise ValueError("unsupported observed VIF callback horizon")
    expected = {f"bank-{index}.bin" for index in range(count)}
    if {path.name for path in case.glob("bank-*.bin")} != expected:
        raise ValueError("observed VIF bank file inventory mismatch")
    banks, identities = [], set()
    for index in range(count):
        path = case / f"bank-{index}.bin"
        code = bounded(path, 16384)
        if len(code) != 16384:
            raise ValueError("observed VU1 bank size mismatch")
        identity = digest(code)
        if identity in identities:
            raise ValueError("duplicate observed VU1 bank identity")
        identities.add(identity)
        banks.append((path, code, identity))
    return manifest, banks


def generate(case, inspector, output, cache):
    case, inspector, output, cache = [Path(p).resolve() for p in (case, inspector, output, cache)]
    manifest, banks = observed_banks(case)  # Validate the entire collection first.
    toolchain = {"inspector_sha256": digest(inspector.read_bytes()),
                 "emitter_sha256": digest(EMITTER_PATH.read_bytes()),
                 "converter_sha256": digest(Path(__file__).read_bytes())}
    output.mkdir(parents=True, exist_ok=True)
    cache.mkdir(parents=True, exist_ok=True)
    records, sources, inspector_runs, symbols = [], [], 0, []
    for bank_path, code, code_hash in banks:
        symbol = "nexoVifBank_" + code_hash
        key = {"schema": "nexo.vif.bank.cache.v1", "code_sha256": code_hash, **toolchain}
        cached_path = cache / (digest(canonical(key)) + ".metadata.json")
        metadata = None
        try:
            cached = json.loads(bounded(cached_path, 16 * 1024 * 1024))
            if cached["identity"] != key or digest(canonical(cached["metadata"])) != cached["metadata_sha256"]:
                raise ValueError("cache identity or checksum mismatch")
            EMITTER.validate(code, cached["metadata"], symbol)
            if cached["metadata"]["unit"] != 1:
                raise ValueError("not a VU1 bank")
            metadata = cached["metadata"]
        except (OSError, ValueError, TypeError, KeyError):
            pass  # A damaged cache cannot determine compilation semantics.
        hit = metadata is not None
        if not hit:
            inspected = subprocess.run([str(inspector), str(bank_path), "vu1"],
                                       check=True, capture_output=True, text=True, timeout=30)
            inspector_runs += 1
            if len(inspected.stdout) > 16 * 1024 * 1024:
                raise ValueError("oversized native frontend output")
            metadata = json.loads(inspected.stdout)
            EMITTER.validate(code, metadata, symbol)
            if metadata["unit"] != 1:
                raise ValueError("not a VU1 bank")
            cached = {"identity": key, "metadata": metadata,
                      "metadata_sha256": digest(canonical(metadata))}
            write_changed(cached_path, canonical(cached) + b"\n")
        source = EMITTER.emit_bank(code, metadata, symbol).encode()
        cpp = output / (code_hash + ".cpp")
        write_changed(cpp, source)
        sources.append(str(cpp))
        symbols.append(symbol)
        records.append({"code_sha256": code_hash, "generated_cpp_sha256": digest(source),
                        "metadata_sha256": digest(canonical(metadata)), "metadata_cache_hit": hit,
                        "compiled_entries": sum(not pair["lowerUsage"]["reserved"] and
                                                not pair["upperUsage"]["reserved"] for pair in metadata["pairs"])})
    registry = ['#include "nexo/vu_native.h"\n#include <array>\nnamespace ps2native::nexo {\n']
    registry.extend(f"const VuNativeProgram &{symbol}();\n" for symbol in symbols)
    registry.append("std::span<const VuNativeProgram> compiledVifBanks() {\n")
    registry.append(f"    static const std::array<VuNativeProgram, {len(symbols)}> banks{{\n")
    registry.extend(f"        {symbol}(),\n" for symbol in symbols)
    registry.append("    };\n    return banks;\n}\n}\n")
    registry_path = output / "compiled-vif-banks.cpp"
    write_changed(registry_path, "".join(registry).encode())
    sources.append(str(registry_path))
    result = {"schema": "nexo.vif.native.collection.v1", "assurance": "unknown",
              "case": str(case), "sources": sources, "banks": records, **toolchain,
              "inspector_runs": inspector_runs, "callback_horizon": manifest["callback_horizon"],
              "closure_beyond_observed_case": False, "runtime_guest_compiler": False,
              "registry_sha256": digest(registry_path.read_bytes())}
    write_changed(output / "collection.json", json.dumps(result, indent=2, sort_keys=True).encode() + b"\n")
    write_changed(output / "cmake-sources.txt", (";".join(sources) + "\n").encode())
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--case", type=Path, required=True)
    parser.add_argument("--inspect", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--cache", type=Path, required=True)
    args = parser.parse_args()
    result = generate(args.case, args.inspect, args.output, args.cache)
    print(json.dumps({"banks": len(result["banks"]), "inspector_runs": result["inspector_runs"],
                      "manifest": str(args.output.resolve() / "collection.json"),
                      "closure_beyond_observed_case": False}, sort_keys=True))


if __name__ == "__main__":
    main()

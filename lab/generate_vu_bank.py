"""Compile-time finite VU bank emitter; no invocation belongs in a game runtime."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess


USAGE_LIMITS = {"vfReadCount": 2, "viRead": 65535, "viWrite": 65535,
                "accRead": 15, "accWrite": 15, "latency": 64, "vfLatency": 64,
                "viLatency": 64, "pipeline": 7, "waitQ": 1, "waitP": 1,
                "readsClip": 1, "writesClip": 1, "delaysNextBranchRead": 1, "reserved": 1}
PAIR_LIMITS = {"lower": 0xFFFFFFFF, "upper": 0xFFFFFFFF, "iBit": 1, "eBit": 1,
               "mBit": 1, "dBit": 1, "tBit": 1, "upperVfShadowReg": 31, "suppressedLowerVf": 31}


def integer(value, maximum):
    if type(value) is not int or not 0 <= value <= maximum:
        raise ValueError("invalid native descriptor scalar")
    return value


def access(value):
    if not isinstance(value, list) or len(value) != 2:
        raise ValueError("invalid native vector access")
    integer(value[0], 31); integer(value[1], 15)


def validate_usage(value):
    if not isinstance(value, dict) or set(value) != set(USAGE_LIMITS) | {"vfRead", "vfWrite"}:
        raise ValueError("unknown or missing native usage fields")
    if not isinstance(value["vfRead"], list) or len(value["vfRead"]) != 2:
        raise ValueError("invalid native read array")
    for read in value["vfRead"]: access(read)
    access(value["vfWrite"])
    for name, maximum in USAGE_LIMITS.items(): integer(value[name], maximum)


def validate(code, metadata, symbol):
    if not isinstance(symbol, str) or re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", symbol) is None:
        raise ValueError("invalid native program symbol")
    if not isinstance(metadata, dict) or set(metadata) != {"version", "unit", "code_size", "pairs"}:
        raise ValueError("unknown or missing native frontend fields")
    if metadata["version"] != 1: raise ValueError("unsupported native frontend version")
    unit = integer(metadata["unit"], 1)
    expected = 16384 if unit == 1 else 4096
    if not isinstance(code, bytes) or len(code) != expected or metadata["code_size"] != expected:
        raise ValueError("native frontend code size mismatch")
    if not isinstance(metadata["pairs"], list) or len(metadata["pairs"]) != expected // 8:
        raise ValueError("native frontend pair count mismatch")
    for index, pair in enumerate(metadata["pairs"]):
        if not isinstance(pair, dict) or set(pair) != set(PAIR_LIMITS) | {"lowerUsage", "upperUsage"}:
            raise ValueError("unknown or missing native pair fields")
        for name, maximum in PAIR_LIMITS.items(): integer(pair[name], maximum)
        if struct.unpack_from("<II", code, index * 8) != (pair["lower"], pair["upper"]):
            raise ValueError("native frontend instruction word mismatch")
        for bit, name in [(31, "iBit"), (30, "eBit"), (29, "mBit"), (28, "dBit"), (27, "tBit")]:
            if pair[name] != ((pair["upper"] >> bit) & 1):
                raise ValueError("native frontend control-bit mismatch")
        validate_usage(pair["lowerUsage"]); validate_usage(pair["upperUsage"])


def descriptor(pair, name):
    text = [f"static constexpr VuNativeAccess::DecodedPair {name} = [] {{\n    VuNativeAccess::DecodedPair p{{}};\n"]
    for field in PAIR_LIMITS: text.append(f"    p.{field} = {pair[field]}u;\n")
    for side in ["lowerUsage", "upperUsage"]:
        usage = pair[side]
        for index, (reg, mask) in enumerate(usage["vfRead"]):
            text.append(f"    p.{side}.vfRead[{index}] = {{{reg}u, {mask}u}};\n")
        text.append(f"    p.{side}.vfWrite = {{{usage['vfWrite'][0]}u, {usage['vfWrite'][1]}u}};\n")
        for field in USAGE_LIMITS:
            value = str(usage[field]) + "u"
            if field == "pipeline": value = f"static_cast<VuNativeAccess::Pipeline>({value})"
            text.append(f"    p.{side}.{field} = {value};\n")
    text.append("    return p;\n}();\n")
    return "".join(text)


def emit_bank(code: bytes, metadata: dict, symbol: str = "compiledVuProgram") -> str:
    validate(code, metadata, symbol)
    output = ['#include "nexo/vu_native.h"\n#include "nexo/vu_native_semantics.h"\n'
              'namespace ps2native::nexo {\nnamespace {\n']
    unique = {}
    entries = []
    for pair in metadata["pairs"]:
        if pair["lowerUsage"]["reserved"] or pair["upperUsage"]["reserved"]:
            entries.append("    {nullptr, nullptr, nullptr},\n")
            continue
        key = json.dumps(pair, sort_keys=True, separators=(",", ":"))
        if key not in unique:
            name = f"descriptor{len(unique)}"
            unique[key] = name
            output.append(descriptor(pair, name))
        lower = 0x8000033C if pair["iBit"] else pair["lower"]
        entries.append(f"    {{&{unique[key]}, &VuNativeAccess::upper<0x{pair['upper']:08x}u>, "
                       f"&VuNativeAccess::lower<0x{lower:08x}u>}},\n")
    output.append("static constexpr VuNativeAccess::Entry entries[] = {\n" + "".join(entries) + "};\n")
    output.append("static constexpr uint8_t codeIdentity[] = {\n")
    for offset in range(0, len(code), 32):
        output.append("    " + ",".join(f"0x{byte:02x}" for byte in code[offset:offset + 32]) + ",\n")
    output.append("};\nstatic constexpr VuNativeProgram program{static_cast<VU1Interpreter::Unit>(" +
                  str(metadata["unit"]) + "u), entries, codeIdentity};\n}\n")
    output.append(f"const VuNativeProgram &{symbol}() {{ return program; }}\n}}\n")
    return "".join(output)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inspect", type=Path, required=True)
    parser.add_argument("--code", type=Path, required=True)
    parser.add_argument("--unit", choices=["vu0", "vu1"], default="vu1")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--symbol", default="compiledVuProgram")
    args = parser.parse_args()
    code = args.code.read_bytes()
    result = subprocess.run([str(args.inspect.resolve()), str(args.code.resolve()), args.unit],
                            check=True, capture_output=True, text=True, timeout=30)
    metadata = json.loads(result.stdout)
    source = emit_bank(code, metadata, args.symbol)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(source)
    manifest = {"format": "nexo.vu.native.bank", "version": 1, "unit": metadata["unit"],
                "source_code_sha256": hashlib.sha256(code).hexdigest(),
                "frontend_metadata_sha256": hashlib.sha256(result.stdout.encode()).hexdigest(),
                "generated_cpp_sha256": hashlib.sha256(source.encode()).hexdigest(),
                "pair_count": len(metadata["pairs"]),
                "compiled_entries": sum(not p["lowerUsage"]["reserved"] and not p["upperUsage"]["reserved"] for p in metadata["pairs"]),
                "assurance": "unknown", "runtime_guest_compiler": False, "runtime_guest_instruction_fetch": False,
                "closure_across_code_uploads": False}
    args.manifest.parent.mkdir(parents=True, exist_ok=True)
    args.manifest.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n")


if __name__ == "__main__": main()

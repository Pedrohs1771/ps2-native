#!/usr/bin/env python3
"""Stage a pinned independent engine without changing its instruction semantics.

Network acquisition is explicit (--fetch). CMake only verifies cached files.
The surrounding emulator is replaced by documented, fail-closed lab adapters.
"""
import argparse
import hashlib
import json
from pathlib import Path
import urllib.request

COMMIT = "94d86c891b1621c0b252e4fc2e155bf90274dcc0"
FILES = {
    "pcsx2/VUops.cpp": "37d5098321e062956880637e90b6a975a8f28d88092a2f333f8f0d2e3750bd16",
    "pcsx2/VUflags.cpp": "b839d688b6c0085644b7ab90957a9d3c766862e41a578260b7f47d9755aed0fe",
    "pcsx2/VU1microInterp.cpp": "c08627212c76be223586bd7bac5587b92ff7e818c6acd1fe23e8e94d0b8f9d5d",
    "pcsx2/VU.h": "c77e856f2ebe46a42cb3f1856afdec9f3662ca51fc7950454060b3ca4b2edea9",
    "pcsx2/VUops.h": "624f9b0ec81847f1941826dd05400685fe82ded288de8dd6789122bd8fd07fb1",
    "pcsx2/VUflags.h": "36b407f06b83964cd15111c9fdc6bad061c0d2842f3b59717b426eab7d62767b",
    "pcsx2/VUmicro.h": "c0693f24e226e3b2fd201342ab7edc99cc771aca58276d89670e83ac242f1f13",
    "pcsx2/Gif_Unit.h": "7ef080a9d889e9b015bd4e8f233473a748006b894af7ac40544eddda1b513032",
    "COPYING.GPLv3": "8ceb4b9ee5adedde47b31e975c1d90c73ad27b6b165a1dcd80c7c545eb65b903",
}


def verify(data, expected, name):
    if hashlib.sha256(data).hexdigest() != expected:
        raise ValueError(f"pinned independent reference hash mismatch: {name}")


def extract(data, start, end):
    if data.count(start) != 1 or data.count(end) != 1:
        raise ValueError("reference extraction boundary missing or ambiguous")
    first, last = data.index(start), data.index(end)
    if last <= first:
        raise ValueError("reference extraction boundaries are reversed")
    return data[first:last]


def write_unchanged(path, data):
    if path.exists() and path.read_bytes() == data:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + ".tmp")
    temporary.write_bytes(data)
    temporary.replace(path)


def prepare(source, output, fetch=False):
    originals = {}
    for name, expected in FILES.items():
        path = source / name
        if not path.exists():
            if not fetch:
                raise ValueError(f"missing cached reference file: {path}; acquire with --fetch")
            url = f"https://raw.githubusercontent.com/PCSX2/pcsx2/{COMMIT}/{name}"
            with urllib.request.urlopen(url, timeout=30) as response:
                data = response.read(1024 * 1024 + 1)
            verify(data, expected, name)
            write_unchanged(path, data)
        data = path.read_bytes()
        verify(data, expected, name)
        originals[name] = data
    # Do not stage upstream Vif.h beside VU.h: quoted include lookup must find
    # the explicit lab VIF register adapter, not the full emulator's header.
    staged = {Path(n).name: d for n, d in originals.items() if n != "pcsx2/Gif_Unit.h"}
    gif = originals["pcsx2/Gif_Unit.h"]
    license_header = gif[:gif.index(b"#pragma once")]
    staged["Gif_Unit.upstream.h"] = gif
    staged["Gif_Tag.inc"] = license_header + extract(gif, b"struct Gif_Tag\n", b"\n\nstruct GS_Packet\n")
    staged["Gif_incTag.inc"] = license_header + extract(gif, b"static __fi void incTag(", b"\n\nstruct Gif_Path_MTVU\n")
    staged["Gif_GetGSPacketSize.inc"] = license_header + extract(gif, b"\tu32 GetGSPacketSize(", b"\n\t// Specify the transfer type")
    allowed = set(staged) | {"reference-source.json"}
    if output.exists() and any(p.name not in allowed for p in output.iterdir()):
        raise ValueError("unexpected reference staging entry could override a verified header or adapter")
    manifest = {
        "schema": "nexo.reference.source.v1", "repository": "https://github.com/PCSX2/pcsx2",
        "commit": COMMIT, "assurance": "unknown", "execution_evidence": False,
        "files": [{"path": n, "sha256": FILES[n]} for n in FILES],
        "staged": [{"path": n, "sha256": hashlib.sha256(d).hexdigest(),
                    "bytes": len(d), "mode": "upstream_slice_with_original_license_header" if n.endswith(".inc") else "byte_identical"}
                   for n, d in staged.items()],
    }
    for name, data in staged.items():
        write_unchanged(output / name, data)
    write_unchanged(output / "reference-source.json", (json.dumps(manifest, indent=2) + "\n").encode())
    return manifest


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--fetch", action="store_true")
    args = parser.parse_args()
    result = prepare(args.source, args.output, args.fetch)
    print(json.dumps({"commit": COMMIT, "verified_files": len(FILES), "staged_files": len(result["staged"])}))

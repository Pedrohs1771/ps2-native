#!/usr/bin/env python3
"""Development-only live EE -> C++ -> native DSO driver; no shell evaluation."""
import hashlib
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tempfile


def main():
    source, base, entry, result = sys.argv[1:]
    root = Path(__file__).resolve().parents[2]
    generator = Path(os.environ.get("PS2X_NATIVE_OVERLAY_GENERATOR", root / "build/ps2xRecomp/ps2_native_overlay"))
    compiler = shutil.which(os.environ.get("PS2X_NATIVE_OVERLAY_CXX", "clang++")) or shutil.which("c++")
    if not compiler or not generator.is_file():
        raise RuntimeError("native overlay generator or C++ compiler is unavailable")
    include = root / "ps2xRuntime/include"
    digest = hashlib.sha256()
    for value in (Path(source).read_bytes(), base.encode(), entry.encode(), generator.read_bytes(),
                  Path(compiler).resolve().as_posix().encode(), Path(__file__).read_bytes()):
        digest.update(len(value).to_bytes(8, "little")); digest.update(value)
    digest.update(subprocess.check_output([compiler, "--version"]))
    kernel = root / "ps2xRuntime/src/lib/Kernel"
    headers = list(include.rglob("*.h")) + list((root / "ps2xIOP/include").rglob("*.h")) + list(kernel.rglob("*.h"))
    for header in sorted(headers):
        digest.update(header.relative_to(root).as_posix().encode()); digest.update(header.read_bytes())
    cache = Path(os.environ.get("PS2X_NATIVE_OVERLAY_CACHE", root / "build/native-overlay-cache"))
    cache.mkdir(parents=True, exist_ok=True)
    key = digest.hexdigest()
    library = cache / (key + ".so")
    if not library.is_file():
        with tempfile.TemporaryDirectory(prefix="build-", dir=cache) as temporary:
            cpp = Path(temporary) / "overlay.cpp"
            output = Path(temporary) / "overlay.so"
            log = cache / (key + ".log")
            with log.open("w") as stream:
                subprocess.run([str(generator), source, base, entry, str(cpp)], stdout=stream,
                               stderr=subprocess.STDOUT, check=True, timeout=30)
                args = [compiler, "-std=c++20", "-O0", "-fno-lto", "-shared", "-fPIC",
                        "-I" + str(include), "-I" + str(root / "ps2xIOP/include"),
                        "-I" + str(kernel), str(cpp), "-o", str(output)]
                if platform.machine().lower() in ("x86_64", "amd64"):
                    args.insert(1, "-msse4.1")
                subprocess.run(args, stdout=stream, stderr=subprocess.STDOUT, check=True, timeout=90)
            os.replace(output, library)
    Path(result).write_text(str(library.resolve()) + "\n")


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        print("[native-overlay-driver] " + str(error), file=sys.stderr)
        sys.exit(1)

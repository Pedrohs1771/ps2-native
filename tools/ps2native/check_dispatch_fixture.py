"""Execute a synthetic ELF's generated native dispatch without a window.

Run from the repository root with:
  python3 tools/ps2native/check_dispatch_fixture.py
Requires the built ps2_recomp and a C++20 compiler; no commercial assets.
"""

from __future__ import annotations

import argparse
from pathlib import Path
import struct
import subprocess
import tempfile


MAIN = r'''
#include "ps2_runtime_macros.h"
#include <ps2_recompiled_functions.h>
#include <cassert>
#include <cstdio>
extern const uint32_t g_ps2RecompiledFunctionTableBase;
extern const uint32_t g_ps2RecompiledFunctionTableEnd;
extern const uint32_t g_ps2RecompiledFunctionTableSlotCount;
extern PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[];
int main() {
    assert(g_ps2RecompiledFunctionTableBase == 0x100000);
    assert(g_ps2RecompiledFunctionTableEnd == 0x100040);
    assert(g_ps2RecompiledFunctionTableSlotCount == 16);
    for (unsigned i = 0; i < 16; ++i)
        assert(g_ps2RecompiledFunctionTable[i] == &sub_00100000_0x100000);
    R5900Context context{}; auto* ctx = &context;
    SET_GPR_U32(ctx, 2, 41); SET_GPR_U32(ctx, 31, 0x123456);
    ctx->pc = 0x100004;
    g_ps2RecompiledFunctionTable[1](nullptr, ctx, nullptr);
    assert(GPR_U32(ctx, 2) == 42 && ctx->pc == 0x100008);
    assert(GPR_U32(ctx, 31) == 0x123456 && !ctx->in_delay_slot);
    g_ps2RecompiledFunctionTable[2](nullptr, ctx, nullptr);
    assert(GPR_U32(ctx, 2) == 52 && ctx->pc == 0x123456);
    SET_GPR_U32(ctx, 2, 10); ctx->pc = 0x100000;
    g_ps2RecompiledFunctionTable[0](nullptr, ctx, nullptr);
    assert(GPR_U32(ctx, 2) == 11 && ctx->pc == 0x123456);
    ctx->pc = 0x100010;
    g_ps2RecompiledFunctionTable[4](nullptr, ctx, nullptr);
    assert(GPR_U32(ctx, 2) == 18 && ctx->pc == 0x100014);
    puts("PASS: 16 compressed bindings and 4 native execution cases");
}
'''


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--recompiler", type=Path)
    parser.add_argument("--compiler", default="g++")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    recompiler = (args.recompiler or root / "build/ps2xRecomp/ps2_recomp").resolve()
    with tempfile.TemporaryDirectory(prefix="ps2-native-dispatch-") as directory:
        work = Path(directory)
        words = [0x03E00008, 0x24420001, 0x24420003, 0x03E00008, 0x24420007] + [0] * 11
        code = struct.pack("<16I", *words)
        ident = b"\x7fELF" + bytes([1, 1, 1, 0, 0]) + bytes(7)
        header = struct.pack("<16sHHIIIIIHHHHHH", ident, 2, 8, 1,
                             0x100000, 52, 0, 0, 52, 32, 1, 40, 0, 0)
        segment = struct.pack("<IIIIIIII", 1, 0x100, 0x100000, 0x100000,
                              len(code), len(code), 5, 0x1000)
        elf = work / "fixture.elf"
        elf.write_bytes(header + segment + bytes(0x100 - len(header) - len(segment)) + code)
        output = work / "output"
        config = work / "config.toml"
        config.write_text(f'''[general]
input = '{elf.as_posix()}'
output = '{output.as_posix()}'
single_file_output = false
patch_syscalls = false
patch_cop0 = false
patch_cache = false
stubs = []
''', encoding="utf-8")
        generation = subprocess.run([str(recompiler), str(config)], capture_output=True,
                                    text=True, timeout=60)
        if generation.returncode:
            raise RuntimeError(generation.stdout + generation.stderr)
        (work / "main.cpp").write_text(MAIN, encoding="utf-8")
        includes = [output, root / "ps2xRuntime/include", root / "ps2xRuntime/src/lib/Kernel",
                    root / "ps2xIOP/include", root / "build/_deps/raylib-src/src"]
        executable = work / "dispatch-fixture"
        compile_command = [args.compiler, "-std=c++20", "-O1", "-msse4.1"]
        compile_command.extend(f"-I{path}" for path in includes)
        compile_command.append(str(work / "main.cpp"))
        compile_command.extend(str(path) for path in sorted(output.glob("*.cpp")))
        compile_command.extend(["-o", str(executable)])
        subprocess.run(compile_command, check=True, timeout=120)
        subprocess.run([str(executable)], check=True, timeout=10)


if __name__ == "__main__":
    main()

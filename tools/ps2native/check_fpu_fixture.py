"""Execute generated R5900 SQRT/RSQRT with distinct operand registers.

Positive radicands and finite results only. This checks operand selection and
aliasing, not exceptional-value semantics, FCR31 flags or PS2 rounding modes.
"""
from __future__ import annotations
import argparse
from pathlib import Path
import struct
import subprocess
import tempfile


def cop1(function: int, fs: int, ft: int, fd: int) -> int:
    return 0x46000000 | ft << 16 | fs << 11 | fd << 6 | function

INSTRUCTIONS = [cop1(4,2,3,4),cop1(4,2,3,3),cop1(4,2,3,2),cop1(4,0,1,1),
                cop1(0x16,2,3,4),cop1(0x16,2,3,3),cop1(0x16,2,3,2)]

MAIN = r'''
#include "ps2_runtime_macros.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
extern PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[];
int main() {
    unsigned cases=0;
    const unsigned fsList[]={2,2,2,0,2,2,2};
    const unsigned ftList[]={3,3,3,1,3,3,3};
    const unsigned fdList[]={4,3,2,1,4,3,2};
    const float edges[]={0.0f,0.25f,1.0f,4.0f,9.0f,16.0f,1.0e-20f,1.0e20f};
    for (unsigned variant=0;variant<7;++variant) for(unsigned i=0;i<512;++i) {
        R5900Context context{};auto*ctx=&context;
        for(unsigned j=0;j<32;++j)ctx->f[j]=float(j+1);
        const unsigned fs=fsList[variant],ft=ftList[variant],fd=fdList[variant];
        float denominator=i<8?edges[i]:float(i+1)/8.0f;
        if(variant>=4 && denominator==0.0f)denominator=4.0f;
        const float numerator=float(int(i*17%997)-500)/4.0f;
        ctx->f[fs]=numerator;ctx->f[ft]=denominator;
        float original[32];std::memcpy(original,ctx->f,sizeof(original));
        const float root=float(std::sqrt(double(denominator)));
        const float expected=variant<4?root:numerator/root;
        ctx->pc=0x100000+variant*16;SET_GPR_U32(ctx,31,0x200000);
        g_ps2RecompiledFunctionTable[variant*4](nullptr,ctx,nullptr);
        const float result=ctx->f[fd];
        if(!std::isfinite(result) || std::fabs(result-expected)>std::fabs(expected)*2e-7f || ctx->pc!=0x200000) {
            std::fprintf(stderr,"FAIL variant=%u case=%u fs=%g ft=%g got=%g expected=%g\n",variant,i,numerator,denominator,result,expected);return 1;
        }
        for(unsigned j=0;j<32;++j)if(j!=fd && ctx->f[j]!=original[j])return 2;
        ++cases;
    }
    std::printf("PASS: 7 SQRT/RSQRT operand and alias variants, %u native executions\n",cases);
}
'''

def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--recompiler", type=Path)
    parser.add_argument("--compiler", default="g++")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    recompiler = (args.recompiler or root / "build/ps2xRecomp/ps2_recomp").resolve()
    with tempfile.TemporaryDirectory(prefix="ps2-native-fpu-") as directory:
        work = Path(directory)
        words = [word for instruction in INSTRUCTIONS
                 for word in (instruction, 0x03E00008, 0, 0)]
        code = struct.pack(f"<{len(words)}I", *words)
        ident = b"\x7fELF" + bytes([1, 1, 1, 0, 0]) + bytes(7)
        header = struct.pack("<16sHHIIIIIHHHHHH", ident, 2, 8, 1,
                             0x100000, 52, 0, 0, 52, 32, 1, 40, 0, 0)
        segment = struct.pack("<IIIIIIII", 1, 0x100, 0x100000, 0x100000,
                              len(code), len(code), 5, 0x1000)
        elf = work / "fixture.elf"
        elf.write_bytes(header + segment + bytes(0x100-len(header)-len(segment)) + code)
        output = work / "output"
        config = work / "config.toml"
        config.write_text(f"""[general]
input = '{elf.as_posix()}'
output = '{output.as_posix()}'
single_file_output = false
patch_syscalls = false
patch_cop0 = false
patch_cache = false
stubs = []
""", encoding="utf-8")
        generation = subprocess.run([str(recompiler), str(config)], capture_output=True,
                                    text=True, timeout=60)
        if generation.returncode:
            raise RuntimeError(generation.stdout + generation.stderr)
        (work / "main.cpp").write_text(MAIN, encoding="utf-8")
        includes = [output, root / "ps2xRuntime/include", root / "ps2xRuntime/src/lib/Kernel",
                    root / "ps2xIOP/include", root / "build/_deps/raylib-src/src"]
        executable = work / "fpu-fixture"
        command = [args.compiler, "-std=c++20", "-O1", "-msse4.1"]
        command.extend(f"-I{path}" for path in includes)
        command.append(str(work / "main.cpp"))
        command.extend(str(path) for path in sorted(output.glob("*.cpp")))
        command.extend(["-o", str(executable)])
        subprocess.run(command, check=True, timeout=120)
        subprocess.run([str(executable)], check=True, timeout=10)


if __name__ == "__main__":
    main()

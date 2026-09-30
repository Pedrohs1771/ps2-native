"""Compile and execute packed R5900 instructions against scalar lane oracles.

Requires the built ps2_recomp and a host C++20 compiler. Uses a synthetic ELF;
no game files, graphics window, or CPU interpreter are involved.
"""

from __future__ import annotations

import argparse
from pathlib import Path
import struct
import subprocess
import tempfile


def mmi(function: int, subfunction: int, rs: int = 4, rt: int = 5,
        rd: int = 6) -> int:
    return 0x70000000 | rs << 21 | rt << 16 | rd << 11 | subfunction << 6 | function


# Each entry contains one instruction and JR/NOP, with padding to 16 bytes.
INSTRUCTIONS = [
    mmi(0x09, 0x1C),                    # PMULTH
    mmi(0x09, 0x1C, rd=4),              # destination aliases a source
    mmi(0x09, 0x1C, rd=0),              # HI/LO still change for rd=zero
    mmi(0x09, 0x10),                    # PMADDH
    mmi(0x09, 0x14),                    # PMSUBH
    *[mmi(0x30, mode, rs=0, rt=0) for mode in range(5)],  # PMFHL
    mmi(0x31, 0, rt=0, rd=0),           # PMTHL.LW preserves odd words
    mmi(0x09, 0x08, rs=0, rt=0),        # PMFHI
    mmi(0x09, 0x09, rs=0, rt=0),        # PMFLO
    mmi(0x29, 0x08, rt=0, rd=0),        # PMTHI
    mmi(0x29, 0x09, rt=0, rd=0),        # PMTLO
    mmi(0x09, 0x0A),                    # PINTH
    mmi(0x29, 0x0A),                    # PINTEH
    mmi(0x28, 0x14),                    # PADDUH
    mmi(0x28, 0x15),                    # PSUBUH
    mmi(0x09, 0x1A, rs=0),              # PEXEH
    mmi(0x09, 0x1B, rs=0),              # PREVH
]


MAIN = r'''
#include "ps2_runtime_macros.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
extern PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[];
using Words = std::array<uint32_t, 4>;
using Halves = std::array<uint16_t, 8>;

static uint64_t pair(uint32_t a, uint32_t b) { return uint64_t(a) | uint64_t(b) << 32; }
static Words lanes(__m128i x) { Words v; std::memcpy(v.data(), &x, 16); return v; }
static __m128i vector(const Words& v) { __m128i x; std::memcpy(&x, v.data(), 16); return x; }
static __m128i vector(const Halves& v) { __m128i x; std::memcpy(&x, v.data(), 16); return x; }
static Words words(const Halves& h) { return lanes(vector(h)); }
static Halves halves(const Words& w) { Halves h; std::memcpy(h.data(), w.data(), 16); return h; }
static uint32_t randomWord() {
    static uint32_t state = 0x43c57129;
    state ^= state << 13; state ^= state >> 17; state ^= state << 5;
    return state;
}
static uint16_t clampHalf(int32_t v) {
    return uint16_t(v < -32768 ? -32768 : v > 32767 ? 32767 : v);
}
static uint64_t clampWord(uint32_t hi, uint32_t lo) {
    const uint64_t bits = pair(lo, hi);
    int64_t value; std::memcpy(&value, &bits, 8);
    if (value < INT32_MIN) return uint64_t(int64_t(INT32_MIN));
    if (value > INT32_MAX) return INT32_MAX;
    return uint64_t(value);
}
static void fail(unsigned test, unsigned iteration, const char* field) {
    std::fprintf(stderr, "FAIL MMI case=%u iteration=%u field=%s\n", test, iteration, field);
    std::exit(1);
}
int main() {
    unsigned cases = 0;
    for (unsigned test = 0; test < 21; ++test) for (unsigned iteration = 0; iteration < 512; ++iteration) {
        R5900Context context{}; auto* ctx = &context;
        Words a, b, lo, hi;
        for (unsigned i=0; i<4; ++i) { a[i]=randomWord(); b[i]=randomWord(); lo[i]=randomWord(); hi[i]=randomWord(); }
        if (iteration == 0) {
            a = words(Halves{0, 1, 32767, 32768, 65535, 12, 65000, 7});
            b = words(Halves{65535, 4, 32768, 32768, 65535, 0, 17, 65500});
            lo = {0x7fffffff, 0x80000000, 0xffffffff, 0x3456789a};
            hi = {0, 0x76543210, 0xffffffff, 0x89abcdef};
        }
        SET_GPR_VEC(ctx, 4, vector(a)); SET_GPR_VEC(ctx, 5, vector(b));
        SET_GPR_VEC(ctx, 6, vector(Words{17, 18, 19, 20}));
        ctx->lo=pair(lo[0],lo[1]); ctx->lo1=pair(lo[2],lo[3]);
        ctx->hi=pair(hi[0],hi[1]); ctx->hi1=pair(hi[2],hi[3]);
        Words result{17, 18, 19, 20}; unsigned rd=6;
        auto ah=halves(a), bh=halves(b);
        if (test < 5) {
            uint32_t product[8];
            for (unsigned i=0; i<8; ++i) product[i] = uint32_t(int32_t(int16_t(ah[i])) * int32_t(int16_t(bh[i])));
            for (unsigned i=0; i<8; ++i) {
                auto& target = (i%4 < 2 ? lo : hi)[(i/4)*2 + i%2];
                target = test < 3 ? product[i] : test == 3 ? target + product[i] : target - product[i];
            }
            result = {lo[0], hi[0], lo[2], hi[2]};
            if (test == 1) rd=4;
            if (test == 2) rd=0;
        } else if (test == 5) result = {lo[0], hi[0], lo[2], hi[2]};
        else if (test == 6) result = {lo[1], hi[1], lo[3], hi[3]};
        else if (test == 7) {
            auto x=clampWord(hi[0],lo[0]), y=clampWord(hi[2],lo[2]);
            result={uint32_t(x),uint32_t(x>>32),uint32_t(y),uint32_t(y>>32)};
        } else if (test == 8) result=words(Halves{uint16_t(lo[0]),uint16_t(lo[1]),uint16_t(hi[0]),uint16_t(hi[1]),uint16_t(lo[2]),uint16_t(lo[3]),uint16_t(hi[2]),uint16_t(hi[3])});
        else if (test == 9) result=words(Halves{clampHalf(int32_t(lo[0])),clampHalf(int32_t(lo[1])),clampHalf(int32_t(hi[0])),clampHalf(int32_t(hi[1])),clampHalf(int32_t(lo[2])),clampHalf(int32_t(lo[3])),clampHalf(int32_t(hi[2])),clampHalf(int32_t(hi[3]))});
        else if (test == 10) { lo[0]=a[0]; hi[0]=a[1]; lo[2]=a[2]; hi[2]=a[3]; rd=0; }
        else if (test == 11) result=hi;
        else if (test == 12) result=lo;
        else if (test == 13) { hi=a; rd=0; }
        else if (test == 14) { lo=a; rd=0; }
        else if (test == 15) result=words(Halves{bh[0],ah[4],bh[1],ah[5],bh[2],ah[6],bh[3],ah[7]});
        else if (test == 16) result=words(Halves{bh[0],ah[0],bh[2],ah[2],bh[4],ah[4],bh[6],ah[6]});
        else if (test == 17 || test == 18) {
            Halves h;
            for (unsigned i=0;i<8;++i) h[i] = test==17 ? uint16_t(unsigned(ah[i])+bh[i]>65535 ? 65535 : ah[i]+bh[i]) : uint16_t(ah[i]<bh[i] ? 0 : ah[i]-bh[i]);
            result=words(h);
        } else if (test == 19) result=words(Halves{bh[2],bh[1],bh[0],bh[3],bh[6],bh[5],bh[4],bh[7]});
        else if (test == 20) result=words(Halves{bh[3],bh[2],bh[1],bh[0],bh[7],bh[6],bh[5],bh[4]});
        ctx->pc=0x100000+test*16; SET_GPR_U32(ctx,31,0x200000);
        g_ps2RecompiledFunctionTable[test*4](nullptr,ctx,nullptr);
        if (ctx->pc!=0x200000) fail(test,iteration,"return");
        if (rd && lanes(GPR_VEC(ctx,rd))!=result) fail(test,iteration,"destination");
        if (ctx->lo!=pair(lo[0],lo[1]) || ctx->lo1!=pair(lo[2],lo[3])) fail(test,iteration,"LO lanes");
        if (ctx->hi!=pair(hi[0],hi[1]) || ctx->hi1!=pair(hi[2],hi[3])) fail(test,iteration,"HI lanes");
        if (lanes(GPR_VEC(ctx,0))!=Words{}) fail(test,iteration,"zero register");
        ++cases;
    }
    std::printf("PASS: 21 packed MMI variants, %u native executions\n",cases);
}
'''


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--recompiler", type=Path)
    parser.add_argument("--compiler", default="g++")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    recompiler = (args.recompiler or root / "build/ps2xRecomp/ps2_recomp").resolve()
    with tempfile.TemporaryDirectory(prefix="ps2-native-mmi-") as directory:
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
        executable = work / "mmi-fixture"
        command = [args.compiler, "-std=c++20", "-O1", "-msse4.1"]
        command.extend(f"-I{path}" for path in includes)
        command.append(str(work / "main.cpp"))
        command.extend(str(path) for path in sorted(output.glob("*.cpp")))
        command.extend(["-o", str(executable)])
        subprocess.run(command, check=True, timeout=120)
        subprocess.run([str(executable)], check=True, timeout=10)


if __name__ == "__main__":
    main()

// Lab-only surrounding environment for the unmodified pinned PCSX2 VU core.
// All policy switches below must be included in the evidence identity.
#pragma once
#include <algorithm>
#include <bit>
#include <cfenv>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <xmmintrin.h>

using u8=uint8_t; using s8=int8_t; using u16=uint16_t; using s16=int16_t;
using u32=uint32_t; using s32=int32_t; using u64=uint64_t; using s64=int64_t;
using u128=__uint128_t; using s128=__int128_t; using uint=unsigned int;
#define __fi
#define __ri
#define VUM_LOG(...) ((void)0)
#define CPU_LOG(...) ((void)0)
#define pxFail(message) throw std::runtime_error(message)
#define jNO_DEFAULT default: throw std::runtime_error("unsupported upstream GIF flag")
#define THREAD_VU1 0
#define INSTANT_VU1 1
#define REC_VU1 0
#define CHECK_XGKICKHACK 0
#define CHECK_VUADDSUBHACK 0
#define CHECK_VU_OVERFLOW(index) true

struct ReferenceConsole
{
    template <typename... Args> void Error(const char* message,Args...) const
    { throw std::runtime_error(message); }
    template <typename... Args> void Warning(const char* message,Args...) const
    { throw std::runtime_error(message); }
};
inline ReferenceConsole DbgCon,DevCon;
struct ReferenceCpuRegs { uint64_t cycle=0; uint32_t code=0; };
extern ReferenceCpuRegs cpuRegs;
struct ReferenceEmuConfig { struct { int VU1FPCR=FE_TOWARDZERO; } Cpu; };
inline ReferenceEmuConfig EmuConfig;
class FPControlRegisterBackup
{
    std::fenv_t saved;
    unsigned savedMxcsr;
public:
    explicit FPControlRegisterBackup(int rounding):savedMxcsr(_mm_getcsr())
    {
        if (std::fegetenv(&saved))
            throw std::runtime_error("cannot establish identified VU reference FP policy");
        if (std::fesetround(rounding))
        {
            std::fesetenv(&saved); _mm_setcsr(savedMxcsr);
            throw std::runtime_error("cannot establish identified VU reference FP policy");
        }
        _mm_setcsr(_mm_getcsr() | (1u<<15) | (1u<<6)); // explicit FTZ + DAZ
    }
    ~FPControlRegisterBackup() { std::fesetenv(&saved); _mm_setcsr(savedMxcsr); }
};
inline constexpr int INTC_VU1=1,DMAC_VIF1=1;
inline void hwIntcIrq(int) { throw std::runtime_error("reference domain excludes VU interrupts"); }
inline void CPU_INT(int,int) { throw std::runtime_error("reference domain excludes asynchronous VIF wakeup"); }

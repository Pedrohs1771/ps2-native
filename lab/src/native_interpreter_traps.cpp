#include "runtime/ps2_vu1.h"
#include <stdexcept>

// Native-only replay guard. Shared state/device helpers remain available,
// while any accidental invocation of generic guest execution is fatal.
#define FORBID_METHOD(name, symbol, arguments) \
    extern "C" void forbid_##name arguments asm("__wrap_" symbol); \
    extern "C" void forbid_##name arguments { throw std::runtime_error("generic interpreter call in native-only replay: " #name); }
FORBID_METHOD(upper, "_ZN14VU1Interpreter9execUpperEj", (VU1Interpreter *, uint32_t))
FORBID_METHOD(lower, "_ZN14VU1Interpreter9execLowerEjPhjR2GSP9PS2Memoryj",
    (VU1Interpreter *, uint32_t, uint8_t *, uint32_t, GS &, PS2Memory *, uint32_t))
FORBID_METHOD(resume, "_ZN14VU1Interpreter6resumeEPhjS0_jR2GSP9PS2Memoryjjj",
    (VU1Interpreter *, uint8_t *, uint32_t, uint8_t *, uint32_t, GS &, PS2Memory *, uint32_t, uint32_t, uint32_t))
FORBID_METHOD(execute, "_ZN14VU1Interpreter7executeEPhjS0_jR2GSP9PS2Memoryjjjj",
    (VU1Interpreter *, uint8_t *, uint32_t, uint8_t *, uint32_t, GS &, PS2Memory *, uint32_t, uint32_t, uint32_t, uint32_t))
FORBID_METHOD(run, "_ZN14VU1Interpreter3runEPhjS0_jR2GSP9PS2Memoryj",
    (VU1Interpreter *, uint8_t *, uint32_t, uint8_t *, uint32_t, GS &, PS2Memory *, uint32_t))
FORBID_METHOD(fmac, "_ZN14VU1Interpreter13applyFmacDestEPfS0_h", (VU1Interpreter *, float *, float *, uint8_t))
FORBID_METHOD(fmacAcc, "_ZN14VU1Interpreter16applyFmacDestAccEPfh", (VU1Interpreter *, float *, uint8_t))
FORBID_METHOD(fmacNormalize, "_ZN14VU1Interpreter19normalizeFmacResultEPfhPh", (VU1Interpreter *, float *, uint8_t, uint8_t *))
#undef FORBID_METHOD

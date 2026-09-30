#include "ps2_runtime.h"

// Windowless VU integration fixtures do not execute EE code. All EE lookups
// fail; these are link definitions, not guest-operation success stubs.
extern const uint32_t g_ps2RecompiledFunctionTableBase = 0;
extern const uint32_t g_ps2RecompiledFunctionTableEnd = 4;
extern const uint32_t g_ps2RecompiledFunctionTableSlotCount = 1;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[1] = {nullptr};

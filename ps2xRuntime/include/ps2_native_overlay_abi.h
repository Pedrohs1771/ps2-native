#pragma once

#include "ps2_runtime.h"
#include <cstddef>
#include <cstdint>

// Bump whenever generated overlay code or the runtime interface changes.
inline constexpr uint32_t PS2_NATIVE_OVERLAY_ABI = 1;
struct PS2NativeOverlayBinding
{
    uint32_t address;
    PS2Runtime::RecompiledFunction function;
    uint32_t sourceBegin;
    uint32_t sourceSize;
    const uint8_t *sourceBytes;
};
using PS2NativeOverlayGetBindings = const PS2NativeOverlayBinding *(*)(size_t *, uint32_t *);

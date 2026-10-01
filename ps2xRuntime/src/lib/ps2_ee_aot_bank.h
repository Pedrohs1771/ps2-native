#pragma once

#include "ps2_native_overlay_abi.h"
#include <span>

namespace ps2native::ee_aot
{
    // Keep compiled bank descriptors independent of directory/diagnosis headers.
    struct Bank
    {
        uint32_t base;
        std::span<const uint8_t> image;
        std::span<const PS2NativeOverlayBinding> bindings;
    };
    struct Program { std::span<const Bank> banks; };
}

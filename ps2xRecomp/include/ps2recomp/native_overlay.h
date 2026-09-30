#pragma once

#include <cstdint>
#include <span>
#include <string>

namespace ps2recomp
{
    // Compile reachable basic blocks from a bounded snapshot of live EE RAM.
    // The output uses the same instruction and delay-slot emitters as AOT.
    std::string generateNativeOverlay(std::span<const uint8_t> bytes,
                                      uint32_t base, uint32_t entry);
}

#pragma once

#include <cstdint>
#include <span>
#include <string>

namespace ps2recomp
{
    enum class OverlayDependencyContract { NormalEntry, LegacyWholeBlock };
    // Compile reachable basic blocks from a bounded snapshot of live EE RAM.
    // The output uses the same instruction and delay-slot emitters as AOT.
    // NormalEntry follows compiled resume labels and retains reachable local
    // backedges. A pending architectural delay-slot context is a separate entry
    // contract; it is not covered by an independently entered slot address.
    std::string generateNativeOverlay(std::span<const uint8_t> bytes,
                                      uint32_t base, uint32_t entry,
                                      OverlayDependencyContract contract = OverlayDependencyContract::NormalEntry);
}

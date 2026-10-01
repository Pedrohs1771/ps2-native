#pragma once

#include <cstdint>
#include <span>
#include <string>

namespace ps2recomp
{
    // Offline synthesis of one bounded linear native region, optionally ending
    // in JR/JALR plus its slot. Masks select only typed LUI/SW data immediates.
    // Generated ps2native_data_family checks physical RAM bytes and normal entry,
    // then invokes fixed native code at a relative PC. Fetch, writer/alias and
    // producer-domain proofs remain separate laboratory obligations.
    std::string generateNativeDataFamily(std::span<const uint32_t> words,
                                         std::span<const uint32_t> masks);
}

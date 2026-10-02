#pragma once

#include <cstdint>
#include <span>
#include <string>

namespace ps2recomp
{
    // Offline synthesis of one bounded native region, optionally ending in one
    // integer conditional branch or JR/JALR plus its slot. Masks select the
    // ordinary data fields in ps2_native_data_operands.h; opcode/register and
    // conditional-control encodings stay fixed.
    // Generated ps2native_data_family checks physical RAM bytes and normal entry,
    // then invokes fixed native code at a relative PC. Fetch, writer/alias and
    // producer-domain proofs remain separate laboratory obligations.
    std::string generateNativeDataFamily(std::span<const uint32_t> words,
                                         std::span<const uint32_t> masks);
}

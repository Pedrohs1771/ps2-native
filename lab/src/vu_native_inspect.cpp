#include "nexo/vu_native.h"

#include <memory>
#include <stdexcept>

namespace ps2native::nexo
{
std::vector<VuNativeAccess::DecodedPair> VuNativeAccess::inspectMicrocode(
    std::span<const uint8_t> code, Unit unit)
{
    const size_t expected = unit == Unit::VU1 ? 16384 : 4096;
    if (code.size() != expected) throw std::invalid_argument("VU frontend requires complete microcode memory");
    auto decoder = std::make_unique<VU1Interpreter>(unit);
    std::vector<DecodedPair> result;
    result.reserve(code.size() / 8);
    for (uint32_t pc = 0; pc < code.size(); pc += 8)
        result.push_back(decoder->decodeInstructionPair(code.data(), pc));
    return result;
}
}

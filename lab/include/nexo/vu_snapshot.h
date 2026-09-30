#pragma once

#include <cstdint>
#include <span>
#include <vector>

class VU1Interpreter;

namespace ps2native::nexo
{
// Canonical checkpoint of the current VU runtime at an instruction-pair
// boundary. Code, data, VIF, GIF and GS belong to the enclosing replay case.
// Decode failures throw std::invalid_argument and leave the target unchanged.
class VuSnapshotCodec
{
public:
    static constexpr uint32_t formatVersion = 1;
    static std::vector<uint8_t> encode(const VU1Interpreter &vu);
    static void restore(VU1Interpreter &vu, std::span<const uint8_t> bytes);

private:
    template <typename Archive, typename Vu>
    static void fields(Archive &archive, Vu &vu);
    static void validate(const VU1Interpreter &vu);
};
}

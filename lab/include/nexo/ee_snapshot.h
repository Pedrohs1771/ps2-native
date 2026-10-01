#pragma once

#include "ps2_runtime.h"

#include <span>
#include <vector>

namespace ps2native::nexo
{
    // All fields of the identified R5900Context model, not a full machine checkpoint.
    class EeSnapshotCodec
    {
    public:
        static std::vector<uint8_t> encode(const R5900Context &context);
        static R5900Context decode(std::span<const uint8_t> bytes);
    };
}

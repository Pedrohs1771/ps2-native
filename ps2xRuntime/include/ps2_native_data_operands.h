#pragma once
#include <cstdint>

namespace ps2native
{
    enum class DataOperand { None, Unsigned16, Signed16 };

    // Identity-mask validation only. This never executes a guest instruction.
    // Keep the offline Python proposal classifier covered by the same opcode tests.
    constexpr DataOperand nativeDataOperand(uint32_t word) noexcept
    {
        switch(word >> 26)
        {
        case 0x0f: // LUI requires its reserved source field to remain zero.
            return ((word >> 21) & 31u) == 0u ? DataOperand::Unsigned16 : DataOperand::None;
        case 0x0c: case 0x0d: case 0x0e: // ANDI, ORI, XORI
            return DataOperand::Unsigned16;
        case 0x09: case 0x0a: case 0x0b: // ADDIU, SLTI, SLTIU
        case 0x20: case 0x21: case 0x23: case 0x24: case 0x25: case 0x27: // integer loads
        case 0x37: case 0x1e: // LD, LQ
        case 0x28: case 0x29: case 0x2b: case 0x3f: case 0x1f: // integer stores
            return DataOperand::Signed16;
        default:
            return DataOperand::None;
        }
    }
}

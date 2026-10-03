#pragma once

#include <cstdint>

namespace ps2native::ee
{
    inline constexpr uint32_t rdramBytes = 32u * 1024u * 1024u;

    // Preserve the virtual instruction PC. Only capture and byte guards use
    // this physical offset. TLB, BIOS, scratchpad and cache visibility have
    // separate contracts; other high addresses must not wrap into this RAM.
    constexpr uint32_t rdramCodeOffset(uint32_t address)
    {
        if (address < rdramBytes) return address;
        const uint32_t segment = address & 0xE0000000u;
        if (segment != 0x80000000u && segment != 0xA0000000u) return rdramBytes;
        const uint32_t physical = address & 0x1FFFFFFFu;
        return physical < rdramBytes ? physical : rdramBytes;
    }

    constexpr uint32_t rdramCodePage(uint32_t address, uint32_t pageBytes)
    {
        const uint32_t segment = address & 0xE0000000u;
        const uint32_t bank = segment == 0x80000000u ? 1u : segment == 0xA0000000u ? 2u : 0u;
        return (bank * rdramBytes + rdramCodeOffset(address)) / pageBytes;
    }
}

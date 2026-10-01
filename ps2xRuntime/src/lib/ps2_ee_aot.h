#pragma once

#include "ps2_native_overlay_abi.h"

#include <array>
#include <span>
#include <vector>

namespace ps2native::ee_aot
{
    struct Bank
    {
        uint32_t base;
        std::span<const uint8_t> image;
        std::span<const PS2NativeOverlayBinding> bindings;
    };

    struct Program { std::span<const Bank> banks; };

    enum class Status { Ready, MissingEntry, CodeChanged, MisalignedPc, OutsideRam, NoRam };
    struct Lookup
    {
        PS2Runtime::RecompiledFunction function = nullptr;
        Status status = Status::MissingEntry;
        uint32_t versionsChecked = 0;
    };

    // Finite callbacks only. Guards describe RAM at lookup; cache visibility,
    // entry-context refinements and writes inside a running block are unqualified.
    class Dispatcher
    {
    public:
        explicit Dispatcher(const Program &program);
        [[nodiscard]] Lookup lookup(const uint8_t *ram, uint32_t pc) const;
        [[nodiscard]] size_t bindings() const noexcept { return m_entries.size(); }
        [[nodiscard]] size_t pages() const noexcept { return m_heads.size() / slotsPerPage; }

    private:
        static constexpr uint32_t absent = 0xFFFFFFFFu;
        static constexpr uint32_t pageBytes = 4096u;
        static constexpr uint32_t slotsPerPage = pageBytes / 4u;
        struct Entry
        {
            PS2Runtime::RecompiledFunction function;
            uint32_t bank;
            uint32_t offset;
            uint32_t sourceBegin;
            uint32_t sourceSize;
            uint32_t next;
        };
        std::array<uint32_t, PS2_RAM_SIZE / pageBytes> m_pages;
        std::vector<uint32_t> m_heads;
        std::vector<std::vector<uint8_t>> m_images;
        std::vector<Entry> m_entries;
    };
}

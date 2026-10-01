#pragma once

#include "ps2_ee_aot_bank.h"

#include <array>
#include <span>
#include <vector>

namespace ps2native::ee_aot
{
    enum class Status { Ready, MissingEntry, CodeChanged, MisalignedPc, OutsideRam, NoRam };
    struct Lookup
    {
        PS2Runtime::RecompiledFunction function = nullptr;
        Status status = Status::MissingEntry;
        uint32_t versionsChecked = 0;
    };
    struct CandidateEvidence
    {
        uint32_t bank;
        uint32_t sourceBegin;
        std::span<const uint8_t> expected;
        uint32_t mismatchBytes;
        uint32_t firstMismatchOffset;
    };
    struct Diagnosis
    {
        Lookup lookup;
        // Expected spans borrow the dispatcher's immutable image storage.
        std::vector<CandidateEvidence> candidates;
    };

    // Finite callbacks only. Guards describe RAM at lookup; cache visibility,
    // entry-context refinements and writes inside a running block are unqualified.
    class Dispatcher
    {
    public:
        explicit Dispatcher(const Program &program);
        [[nodiscard]] Lookup lookup(const uint8_t *ram, uint32_t pc) const;
        [[nodiscard]] Diagnosis diagnose(const uint8_t *ram, uint32_t pc) const;
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

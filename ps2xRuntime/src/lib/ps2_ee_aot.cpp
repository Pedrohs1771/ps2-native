#include "ps2_ee_aot.h"

#include <cstring>
#include <limits>
#include <stdexcept>
#include <unordered_set>

namespace ps2native::ee_aot
{
    Lookup Dispatcher::admitNormalEntry(const uint8_t *ram,const R5900Context *context) const
    {
        if (!context || context->in_delay_slot) return {nullptr,Status::UnsupportedEntryContext};
        return lookup(ram,context->pc);
    }
    Diagnosis Dispatcher::diagnose(const uint8_t *ram,uint32_t pc) const
    {
        Diagnosis result{lookup(ram,pc),{}};
        if (!ram || (pc & 3u) || ee::rdramCodeOffset(pc) >= PS2_RAM_SIZE) return result;
        const uint32_t page=m_pages[ee::rdramCodePage(pc,pageBytes)];
        if (page==absent) return result;
        for (uint32_t index=m_heads[page+(pc%pageBytes)/4u];index!=absent;index=m_entries[index].next)
        {
            const auto &entry=m_entries[index];
            const std::span expected(m_images[entry.bank].data()+entry.offset,entry.sourceSize);
            uint32_t count=0,first=absent;
            for (uint32_t offset=0;offset<entry.sourceSize;++offset)
                if (ram[ee::rdramCodeOffset(entry.sourceBegin)+offset]!=expected[offset])
                {
                    ++count;
                    if (first==absent) first=offset;
                }
            result.candidates.push_back({entry.bank,entry.sourceBegin,expected,count,first});
        }
        return result;
    }
    Dispatcher::Dispatcher(const Program &program)
    {
        constexpr size_t maxBanks = 512u;
        constexpr size_t maxBindings = 2u * 1024u * 1024u;
        if (program.banks.size() > maxBanks)
            throw std::invalid_argument("EE AOT bank budget exceeded");
        m_pages.fill(absent);
        size_t total = 0;
        for (const auto &bank : program.banks)
        {
            if (bank.bindings.size() > maxBindings - total)
                throw std::invalid_argument("EE AOT binding budget exceeded");
            total += bank.bindings.size();
        }
        m_entries.reserve(total);
        m_images.reserve(program.banks.size());
        for (const auto &bank : program.banks)
        {
            const uint32_t physicalBase = ee::rdramCodeOffset(bank.base);
            if (bank.image.empty() || bank.image.size() > 65536u || bank.image.size() % 4u ||
                (bank.base & 3u) || physicalBase >= PS2_RAM_SIZE ||
                bank.image.size() > PS2_RAM_SIZE - physicalBase || bank.bindings.empty() ||
                bank.bindings.size() > 32768u)
                throw std::invalid_argument("invalid EE AOT bank dimensions");
            const auto imageBegin = reinterpret_cast<uintptr_t>(bank.image.data());
            std::unordered_set<uint32_t> addresses;
            const uint32_t bankIndex = static_cast<uint32_t>(m_images.size());
            m_images.emplace_back(bank.image.begin(), bank.image.end());
            for (const auto &binding : bank.bindings)
            {
                const auto pointer = reinterpret_cast<uintptr_t>(binding.sourceBytes);
                if (!binding.function || !binding.sourceBytes || pointer < imageBegin ||
                    ((pointer - imageBegin) & 3u) || (binding.sourceBegin & 3u) ||
                    pointer - imageBegin >= bank.image.size() ||
                    !binding.sourceSize || (binding.sourceSize & 3u) ||
                    binding.sourceSize > bank.image.size() - (pointer - imageBegin) ||
                    binding.sourceBegin != bank.base + pointer - imageBegin ||
                    (binding.address & 3u) || binding.address < binding.sourceBegin ||
                    binding.address - binding.sourceBegin >= binding.sourceSize ||
                    !addresses.insert(binding.address).second)
                    throw std::invalid_argument("invalid EE AOT binding identity");
                const uint32_t page = ee::rdramCodePage(binding.address,pageBytes);
                if (m_pages[page] == absent)
                {
                    m_pages[page] = static_cast<uint32_t>(m_heads.size());
                    m_heads.resize(m_heads.size() + slotsPerPage, absent);
                }
                const uint32_t slot = m_pages[page] + (binding.address % pageBytes) / 4u;
                const uint32_t entry = static_cast<uint32_t>(m_entries.size());
                m_entries.push_back({binding.function, bankIndex,
                    static_cast<uint32_t>(pointer - imageBegin), binding.sourceBegin,
                    binding.sourceSize, m_heads[slot]});
                m_heads[slot] = entry;
            }
        }
    }

    Lookup Dispatcher::lookup(const uint8_t *ram, uint32_t pc) const
    {
        if (!ram) return {nullptr, Status::NoRam};
        if (pc & 3u) return {nullptr, Status::MisalignedPc};
        if (ee::rdramCodeOffset(pc) >= PS2_RAM_SIZE) return {nullptr, Status::OutsideRam};
        const uint32_t page = m_pages[ee::rdramCodePage(pc,pageBytes)];
        if (page == absent) return {};
        uint32_t entry = m_heads[page + (pc % pageBytes) / 4u];
        if (entry == absent) return {};
        Lookup result{nullptr, Status::CodeChanged};
        while (entry != absent)
        {
            const auto &candidate = m_entries[entry];
            ++result.versionsChecked;
            if (std::memcmp(ram + ee::rdramCodeOffset(candidate.sourceBegin),
                            m_images[candidate.bank].data() + candidate.offset,
                            candidate.sourceSize) == 0)
                return {candidate.function, Status::Ready, result.versionsChecked};
            entry = candidate.next;
        }
        return result;
    }
}

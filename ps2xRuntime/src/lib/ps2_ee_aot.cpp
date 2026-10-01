#include "ps2_ee_aot.h"

#include <cstring>
#include <limits>
#include <stdexcept>
#include <unordered_set>

namespace ps2native::ee_aot
{
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
            if (bank.image.empty() || bank.image.size() > 65536u || bank.image.size() % 4u ||
                (bank.base & 3u) || bank.base >= PS2_RAM_SIZE ||
                bank.image.size() > PS2_RAM_SIZE - bank.base || bank.bindings.empty() ||
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
                const uint32_t page = binding.address / pageBytes;
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
        if (pc >= PS2_RAM_SIZE) return {nullptr, Status::OutsideRam};
        const uint32_t page = m_pages[pc / pageBytes];
        if (page == absent) return {};
        uint32_t entry = m_heads[page + (pc % pageBytes) / 4u];
        if (entry == absent) return {};
        Lookup result{nullptr, Status::CodeChanged};
        while (entry != absent)
        {
            const auto &candidate = m_entries[entry];
            ++result.versionsChecked;
            if (std::memcmp(ram + candidate.sourceBegin,
                            m_images[candidate.bank].data() + candidate.offset,
                            candidate.sourceSize) == 0)
                return {candidate.function, Status::Ready, result.versionsChecked};
            entry = candidate.next;
        }
        return result;
    }
}

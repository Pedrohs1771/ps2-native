#include "iop_native.h"
#include "iop_cpu.h"
#include "iop_memory.h"
#include "../services/iop_module_loader.h"

#include <algorithm>
#include <stdexcept>

namespace ps2x::iop::detail
{
    IopNativeDispatch::IopNativeDispatch(IopMemory &memory, IopCpuCore &core, const IopNativeProgram &program)
        : m_memory(memory), m_core(core)
    {
        if (program.entries.size() > IopMemory::RamSize / 4u)
            throw std::invalid_argument("IOP native bank exceeds RAM directory size");
        m_entries.assign(program.entries.begin(), program.entries.end());
        std::sort(m_entries.begin(), m_entries.end(), [](const auto &a, const auto &b)
        {
            return a.physicalPc < b.physicalPc;
        });
        std::optional<uint32_t> previous;
        for (const auto &entry : m_entries)
        {
            if ((entry.physicalPc & 3u) != 0u || entry.physicalPc >= IopMemory::RamSize ||
                !entry.execute || entry.executeOperand || (previous && *previous == entry.physicalPc))
                throw std::invalid_argument("invalid or duplicate IOP native entry");
            previous = entry.physicalPc;
        }
        m_absoluteEntries = m_entries;
        for (const auto &module : program.modules)
        {
            if (module.image.empty() || module.image.size() > 64u * 1024u * 1024u ||
                module.size == 0u || module.size > IopMemory::RamSize || (module.size & 3u) ||
                module.entries.size() != module.size / 4u)
                throw std::invalid_argument("invalid IOP native module dimensions");
            for (size_t i = 0; i < module.entries.size(); ++i)
            {
                const auto &entry = module.entries[i];
                if (entry.offset != i * 4u ||
                    (entry.relocationMask != 0u && entry.relocationMask != 0xffffu &&
                     entry.relocationMask != 0x03ffffffu && entry.relocationMask != 0xffffffffu))
                    throw std::invalid_argument("invalid IOP native module entry/mask");
                const bool operand = IopNativeAccess::validRelocationOperand(entry.instruction, entry.relocationMask);
                const bool fixedOperand = IopNativeAccess::validRelocationOperand(entry.instruction, 0xffffu) ||
                                          IopNativeAccess::validRelocationOperand(entry.instruction, 0x03ffffffu);
                if ((entry.relocationMask == 0u &&
                     ((entry.execute != nullptr) == (entry.executeOperand != nullptr) ||
                      (entry.executeOperand && !fixedOperand))) ||
                    (entry.relocationMask != 0u && (entry.execute || (operand != (entry.executeOperand != nullptr)))))
                    throw std::invalid_argument("invalid IOP native module operation");
            }
            for (const auto &existing : m_modules)
                if (std::equal(module.image.begin(), module.image.end(), existing.image.begin(), existing.image.end()))
                    throw std::invalid_argument("duplicate IOP native module identity");
            m_modules.push_back(OwnedModule{{module.image.begin(), module.image.end()}, module.size,
                                            {module.entries.begin(), module.entries.end()}});
        }
    }

    void IopNativeDispatch::reset()
    {
        m_entries = m_absoluteEntries;
        m_bindings.clear();
        resetFault();
    }

    void IopNativeDispatch::unbindRange(uint32_t base, uint32_t size)
    {
        if (base >= IopMemory::RamSize || size > IopMemory::RamSize - base)
            throw std::invalid_argument("IOP native unbind range outside RAM");
        if (size == 0u)
            return;
        auto overlaps = [&](const Binding &binding)
        {
            return base < binding.base + binding.size && binding.base < base + size;
        };
        m_entries.erase(std::remove_if(m_entries.begin(), m_entries.end(), [&](const auto &entry)
        {
            if (entry.physicalPc >= base && entry.physicalPc - base < size)
                return true;
            for (const auto &binding : m_bindings)
                if (overlaps(binding) && entry.physicalPc >= binding.base &&
                    entry.physicalPc - binding.base < binding.size)
                    return true;
            return false;
        }), m_entries.end());
        m_bindings.erase(std::remove_if(m_bindings.begin(), m_bindings.end(), overlaps), m_bindings.end());
    }

    bool IopNativeDispatch::bindModule(std::span<const uint8_t> image, const IopImageLoadResult &loaded)
    {
        if (m_fault)
            return false;
        // Compatibility for the original absolute laboratory bank. A family
        // program admits modules only by their complete source-image identity.
        if (m_modules.empty())
            return true;
        const auto module = std::find_if(m_modules.begin(), m_modules.end(), [&](const auto &candidate)
        {
            return std::equal(image.begin(), image.end(), candidate.image.begin(), candidate.image.end());
        });
        if (module == m_modules.end())
        {
            reject(IopNativeFaultKind::UnknownModule, loaded.entry);
            return false;
        }
        auto invalid = [&]()
        {
            reject(IopNativeFaultKind::InvalidModuleBinding, loaded.entry);
            return false;
        };
        if (!loaded || !loaded.relocationsComplete || loaded.size != module->size ||
            (loaded.base & 3u) || loaded.base >= IopMemory::RamSize ||
            loaded.size > IopMemory::RamSize - loaded.base || (loaded.entry & 3u) ||
            loaded.entry < loaded.base || loaded.entry >= loaded.base + loaded.size)
            return invalid();
        std::vector<uint32_t> masks(module->entries.size(), 0u);
        for (const auto &relocation : loaded.relocationMasks)
        {
            if ((relocation.offset & 3u) || relocation.offset >= loaded.size)
                return invalid();
            masks[relocation.offset / 4u] |= relocation.mask;
        }
        std::vector<IopNativeEntry> bound;
        bound.reserve(module->entries.size());
        for (size_t i = 0; i < module->entries.size(); ++i)
        {
            const auto &entry = module->entries[i];
            const uint32_t pc = loaded.base + entry.offset;
            const uint32_t observed = m_memory.read32(pc);
            if (masks[i] != entry.relocationMask ||
                (observed & ~entry.relocationMask) != (entry.instruction & ~entry.relocationMask))
                return invalid();
            // Retain data identity for service dependencies, without making it executable.
            bound.push_back(IopNativeEntry{pc, observed, entry.execute, entry.executeOperand});
        }
        const Binding next{loaded.base, loaded.size};
        auto overlaps = [](const Binding &a, const Binding &b)
        {
            return a.base < b.base + b.size && b.base < a.base + a.size;
        };
        auto bindings = m_bindings;
        auto entries = m_entries;
        entries.erase(std::remove_if(entries.begin(), entries.end(), [&](const auto &entry)
        {
            if (entry.physicalPc >= next.base && entry.physicalPc - next.base < next.size)
                return true;
            for (const auto &binding : m_bindings)
                if (overlaps(next, binding) && entry.physicalPc >= binding.base &&
                    entry.physicalPc - binding.base < binding.size)
                    return true;
            return false;
        }), entries.end());
        bindings.erase(std::remove_if(bindings.begin(), bindings.end(), [&](const auto &binding)
        {
            return overlaps(next, binding);
        }), bindings.end());
        bindings.push_back(next);
        entries.insert(entries.end(), bound.begin(), bound.end());
        std::sort(entries.begin(), entries.end(), [](const auto &a, const auto &b)
        {
            return a.physicalPc < b.physicalPc;
        });
        m_entries.swap(entries);
        m_bindings.swap(bindings);
        return true;
    }

    const IopNativeEntry *IopNativeDispatch::guardEntry(uint32_t pc, bool requireExecutable)
    {
        if (m_fault)
            return nullptr;
        const uint32_t physical = IopMemory::physicalAddress(pc);
        if ((pc & 3u) != 0u)
        {
            m_fault = IopNativeFault{IopNativeFaultKind::MisalignedPc, pc, 0u, 0u};
            return nullptr;
        }
        const auto entry = std::lower_bound(m_entries.begin(), m_entries.end(), physical,
            [](const IopNativeEntry &candidate, uint32_t pc) { return candidate.physicalPc < pc; });
        // Guard only: the observed word never chooses guest-operation semantics.
        const uint32_t observed = physical < IopMemory::RamSize ? m_memory.read32(pc) : 0u;
        if (entry == m_entries.end() || entry->physicalPc != physical ||
            (requireExecutable && !entry->execute && !entry->executeOperand))
        {
            m_fault = IopNativeFault{IopNativeFaultKind::MissingEntry, pc, 0u, observed};
            return nullptr;
        }
        if (observed != entry->instruction)
        {
            m_fault = IopNativeFault{IopNativeFaultKind::CodeChanged, pc, entry->instruction, observed};
            return nullptr;
        }
        return &*entry;
    }

    bool IopNativeDispatch::guardImport(uint32_t pc, uint32_t tableAddress)
    {
        if (!guardEntry(pc, true))
            return false;
        const uint32_t physicalPc = IopMemory::physicalAddress(pc);
        const uint32_t table = IopMemory::physicalAddress(tableAddress);
        if ((table & 3u) || table > physicalPc || physicalPc - table < 20u ||
            ((physicalPc - table - 20u) & 7u) || physicalPc > IopMemory::RamSize - 8u ||
            physicalPc - table > 0x10000u)
        {
            reject(IopNativeFaultKind::InvalidImportBinding, pc);
            return false;
        }
        if (!guardEntry(pc + 4u, true))
            return false;
        // The decoder examines metadata and preceding stubs while searching backwards.
        // Guard that complete dependency interval, preserving the caller's RAM alias.
        const uint32_t alias = pc & ~0x1fffffffu;
        for (uint32_t word = table; word < physicalPc + 8u; word += 4u)
            if (!guardEntry(alias | word, false))
                return false;
        return true;
    }

    bool IopNativeDispatch::execute(IopCpuState &cpu)
    {
        if (cpu.stopped)
            return false;
        const auto *entry = guardEntry(cpu.pc, true);
        if (!entry)
            return false;
        if (entry->executeOperand)
            return entry->executeOperand(cpu, m_memory, m_core, entry->instruction);
        return entry->execute(cpu, m_memory, m_core);
    }

    void IopNativeDispatch::rejectImport(uint32_t pc)
    {
        reject(IopNativeFaultKind::UnresolvedImport, pc);
    }

    void IopNativeDispatch::reject(IopNativeFaultKind kind, uint32_t pc)
    {
        if (!m_fault)
            m_fault = IopNativeFault{kind, pc, 0u, m_memory.read32(pc)};
    }
}

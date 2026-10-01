#include "iop_native.h"
#include "iop_cpu.h"
#include "iop_memory.h"

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
                !entry.execute || (previous && *previous == entry.physicalPc))
                throw std::invalid_argument("invalid or duplicate IOP native entry");
            previous = entry.physicalPc;
        }
    }

    bool IopNativeDispatch::execute(IopCpuState &cpu)
    {
        if (m_fault || cpu.stopped)
            return false;
        const uint32_t physical = IopMemory::physicalAddress(cpu.pc);
        if ((cpu.pc & 3u) != 0u)
        {
            m_fault = IopNativeFault{IopNativeFaultKind::MisalignedPc, cpu.pc, 0u, 0u};
            return false;
        }
        const auto entry = std::lower_bound(m_entries.begin(), m_entries.end(), physical,
            [](const IopNativeEntry &candidate, uint32_t pc) { return candidate.physicalPc < pc; });
        // Guard only: the observed word never chooses guest-operation semantics.
        const uint32_t observed = physical < IopMemory::RamSize ? m_memory.read32(cpu.pc) : 0u;
        if (entry == m_entries.end() || entry->physicalPc != physical)
        {
            m_fault = IopNativeFault{IopNativeFaultKind::MissingEntry, cpu.pc, 0u, observed};
            return false;
        }
        if (observed != entry->instruction)
        {
            m_fault = IopNativeFault{IopNativeFaultKind::CodeChanged, cpu.pc, entry->instruction, observed};
            return false;
        }
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

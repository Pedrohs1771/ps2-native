#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace ps2x::iop::detail
{
    struct IopCpuState;
    class IopCpuCore;
    class IopMemory;

    // Internal laboratory ABI. Semantics are specialized offline, not decoded
    // from live instruction bytes by this callback.
    class IopNativeAccess
    {
    public:
        template <uint32_t Instruction>
        [[nodiscard]] static bool instruction(IopCpuState &cpu, IopMemory &memory, IopCpuCore &core);
    };

    struct IopNativeEntry
    {
        uint32_t physicalPc;
        uint32_t instruction;
        bool (*execute)(IopCpuState &, IopMemory &, IopCpuCore &);
    };

    struct IopNativeProgram
    {
        std::span<const IopNativeEntry> entries;
    };

    enum class IopNativeFaultKind : uint8_t
    {
        MissingEntry,
        CodeChanged,
        MisalignedPc,
        UnresolvedImport,
        CallBudgetExhausted,
        UnsupportedRelocation,
    };

    struct IopNativeFault
    {
        IopNativeFaultKind kind;
        uint32_t pc;
        uint32_t expectedInstruction;
        uint32_t observedInstruction;
    };

    class IopNativeDispatch
    {
    public:
        IopNativeDispatch(IopMemory &memory, IopCpuCore &core, const IopNativeProgram &program);
        [[nodiscard]] bool execute(IopCpuState &cpu);
        [[nodiscard]] const std::optional<IopNativeFault> &fault() const noexcept { return m_fault; }
        void resetFault() noexcept { m_fault.reset(); }
        void rejectImport(uint32_t pc);
        void reject(IopNativeFaultKind kind, uint32_t pc);

    private:
        IopMemory &m_memory;
        IopCpuCore &m_core;
        std::vector<IopNativeEntry> m_entries;
        std::optional<IopNativeFault> m_fault;
    };
}

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
    struct IopImageLoadResult;

    using IopNativeFunction = bool (*)(IopCpuState &, IopMemory &, IopCpuCore &);
    using IopNativeOperandFunction = bool (*)(IopCpuState &, IopMemory &, IopCpuCore &, uint32_t);

    // Internal laboratory ABI. Semantics are specialized offline, not decoded
    // from live instruction bytes by this callback.
    class IopNativeAccess
    {
    public:
        template <uint32_t Instruction>
        [[nodiscard]] static bool instruction(IopCpuState &cpu, IopMemory &memory, IopCpuCore &core);

        template <uint32_t Instruction, uint32_t Mask>
        [[nodiscard]] static bool instructionRelocated(IopCpuState &cpu, IopMemory &memory,
                                                       IopCpuCore &core, uint32_t boundInstruction);

        static constexpr bool validRelocationOperand(uint32_t instruction, uint32_t mask)
        {
            const uint32_t opcode = instruction >> 26u;
            if (mask == 0x03ffffffu)
                return opcode == 2u || opcode == 3u;
            if (mask != 0xffffu)
                return false;
            return opcode == 1u || (opcode >= 4u && opcode <= 0x0fu) ||
                   (opcode >= 0x20u && opcode <= 0x26u) ||
                   (opcode >= 0x28u && opcode <= 0x2bu) || opcode == 0x2eu;
        }
    };

    struct IopNativeEntry
    {
        uint32_t physicalPc;
        uint32_t instruction;
        IopNativeFunction execute;
        IopNativeOperandFunction executeOperand = nullptr;
    };

    struct IopNativeModuleEntry
    {
        uint32_t offset;
        uint32_t instruction;
        uint32_t relocationMask;
        IopNativeFunction execute = nullptr;
        IopNativeOperandFunction executeOperand = nullptr;
    };

    struct IopNativeModule
    {
        std::span<const uint8_t> image;
        uint32_t size;
        std::span<const IopNativeModuleEntry> entries;
    };

    struct IopNativeProgram
    {
        std::span<const IopNativeEntry> entries;
        std::span<const IopNativeModule> modules{};
    };

    enum class IopNativeFaultKind : uint8_t
    {
        MissingEntry,
        CodeChanged,
        MisalignedPc,
        UnresolvedImport,
        CallBudgetExhausted,
        UnsupportedRelocation,
        UnknownModule,
        InvalidModuleBinding,
        InvalidImportBinding,
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
        // Verify all words read by the identified import decoder before a service runs.
        // Data identities can be guarded without acquiring executable callbacks.
        [[nodiscard]] bool guardImport(uint32_t pc, uint32_t tableAddress);
        [[nodiscard]] bool bindModule(std::span<const uint8_t> image, const IopImageLoadResult &loaded);
        void unbindRange(uint32_t base, uint32_t size);
        [[nodiscard]] const std::optional<IopNativeFault> &fault() const noexcept { return m_fault; }
        void resetFault() noexcept { m_fault.reset(); }
        void reset();
        void rejectImport(uint32_t pc);
        void reject(IopNativeFaultKind kind, uint32_t pc);

    private:
        [[nodiscard]] const IopNativeEntry *guardEntry(uint32_t pc, bool requireExecutable);
        IopMemory &m_memory;
        IopCpuCore &m_core;
        struct OwnedModule
        {
            std::vector<uint8_t> image;
            uint32_t size;
            std::vector<IopNativeModuleEntry> entries;
        };
        struct Binding { uint32_t base; uint32_t size; };
        std::vector<IopNativeEntry> m_absoluteEntries;
        std::vector<IopNativeEntry> m_entries;
        std::vector<OwnedModule> m_modules;
        std::vector<Binding> m_bindings;
        std::optional<IopNativeFault> m_fault;
    };
}

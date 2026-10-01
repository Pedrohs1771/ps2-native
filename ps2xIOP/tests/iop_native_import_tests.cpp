#include "iop_compat_test_support.h"
#include "emulator/core/iop_native.h"
#include "emulator/core/iop_cpu.h"
#include "emulator/core/iop_memory.h"
#include "emulator/services/iop_module_loader.h"
#include "iop_native_semantics.h"

using namespace ps2x::iop::detail;
using iop_test::require;

namespace
{
    template <uint32_t Word>
    IopNativeEntry wordAt(uint32_t pc)
    {
        return {pc, Word, &IopNativeAccess::instruction<Word>};
    }

    std::vector<IopNativeEntry> importEntries(bool known = true)
    {
        return {
            wordAt<0x41e00000u>(0x10000u), wordAt<0u>(0x10004u),
            wordAt<0x0101u>(0x10008u),
            known ? wordAt<0x6d737973u>(0x1000cu) : wordAt<0x6e6b6e75u>(0x1000cu),
            known ? wordAt<0x00006d65u>(0x10010u) : wordAt<0x006e776fu>(0x10010u),
            wordAt<0x03e00008u>(0x10014u), wordAt<0x24000006u>(0x10018u),
        };
    }

    iop_test::Irx imageFor(std::span<const IopNativeEntry> entries, uint32_t start)
    {
        iop_test::Irx image;
        for (const auto &entry : entries)
            image.words(entry.physicalPc - 0x10000u, {entry.instruction});
        for (unsigned byte = 0; byte < 4u; ++byte)
            image.bytes[24u + byte] = static_cast<uint8_t>(start >> (byte * 8u));
        return image;
    }

    template <uint32_t Value, uint32_t Offset>
    void changedDependency()
    {
        auto entries = importEntries();
        entries.push_back(wordAt<0x3c080001u>(0x10040u)); // t0 = table address
        entries.push_back(wordAt<0x3c090000u | (Value >> 16u)>(0x10044u));
        entries.push_back(wordAt<0x35290000u | (Value & 0xffffu)>(0x10048u));
        entries.push_back(wordAt<0xad090000u | Offset>(0x1004cu));
        entries.push_back(wordAt<0x08004005u>(0x10050u)); // jump to import stub
        entries.push_back(wordAt<0u>(0x10054u));
        iop_test::Host host;
        host.file = imageFor(entries, 0x10040u).bytes;
        ps2x::iop::IopSubsystem iop(host, IopNativeProgram{entries});
        require(iop.loadModule("host:changed-import.irx").moduleId < 0,
                "changed import dependency published a native module");
        const auto snapshot = iop.debugSnapshot();
        require(snapshot.nativeFaults == 1u && snapshot.interpretedInstructions == 0u &&
                snapshot.nativeInstructions == 6u && snapshot.emulatorInstructions == 6u,
                "changed import executed a service or used interpreted fallback");
        require(!snapshot.diagnostics.empty() && snapshot.diagnostics.front().find("reason=1 ") != std::string::npos,
                "changed import did not report its identity violation");
    }
}

void nativeImportAdmission()
{
    for (bool known : {false, true})
    {
        const auto entries = importEntries(known);
        iop_test::Host host;
        host.file = imageFor(entries, 0x10014u).bytes;
        ps2x::iop::IopSubsystem iop(host, IopNativeProgram{entries});
        const auto loaded = iop.loadModule("host:admitted-import.irx");
        const auto snapshot = iop.debugSnapshot();
        require(snapshot.interpretedInstructions == 0u && snapshot.nativeInstructions == 0u,
                "import identity checks executed guest instructions");
        if (known)
            require(loaded.moduleId > 0 && loaded.startResult == static_cast<int32_t>(IopMemory::RamSize) &&
                    snapshot.nativeFaults == 0u && snapshot.emulatorInstructions == 1u,
                    "admitted sysmem import no longer dispatches its identified service");
        else
            require(loaded.moduleId < 0 && snapshot.nativeFaults == 1u && snapshot.emulatorInstructions == 0u,
                    "admitted unknown import returned fake success");
    }
    for (size_t missing = 0; missing < 7u; ++missing)
    {
        auto entries = importEntries();
        iop_test::Host host;
        host.file = imageFor(entries, 0x10014u).bytes;
        entries.erase(entries.begin() + missing);
        ps2x::iop::IopSubsystem iop(host, IopNativeProgram{entries});
        const auto loaded = iop.loadModule("host:incomplete-import.irx");
        const auto snapshot = iop.debugSnapshot();
        require(loaded.moduleId < 0 && snapshot.nativeFaults == 1u &&
                snapshot.emulatorInstructions == 0u && snapshot.interpretedInstructions == 0u,
                "missing import metadata/stub identity reached a service");
    }
}

void nativeImportMutation()
{
    changedDependency<0x0102u, 8u>();             // Version changed, recognized library retained.
    changedDependency<0x24000004u, 24u>();        // Ordinal changed, stub pattern retained.
    changedDependency<0x6e6b6e75u, 12u>();        // Library name changed.
    changedDependency<0x03e00008u, 4u>();        // A metadata word also needs an identity guard.
}

void nativeImportDataIdentity()
{
    const auto entries = importEntries();
    std::vector<IopNativeModuleEntry> descriptors;
    for (const auto &entry : entries)
        descriptors.push_back({entry.physicalPc - 0x10000u, entry.instruction, 0u, entry.execute});
    descriptors[1].relocationMask = 0xffffffffu;
    descriptors[1].execute = nullptr; // Identified relocated data, never an executable kernel.
    const std::array<uint8_t, 3> source{1u, 2u, 3u};
    const std::array<IopNativeModule, 1> modules{{{source, 28u, descriptors}}};
    IopImageLoadResult loaded;
    loaded.error = IopImageLoadError::None; loaded.base = 0x10000u;
    loaded.size = 28u; loaded.entry = 0x10014u;
    loaded.relocationMasks.push_back({4u, 0xffffffffu});
    {
        IopMemory memory; IopCpuCore core(memory);
        for (const auto &entry : entries) memory.write32(entry.physicalPc, entry.instruction);
        IopNativeDispatch native(memory, core, IopNativeProgram{entries});
        for (uint32_t table : {0x10001u, 0x10014u, 0x10010u, 0u, 0x1ffffcu})
        {
            native.resetFault();
            require(!native.guardImport(0x10014u, table) &&
                    native.fault()->kind == IopNativeFaultKind::InvalidImportBinding,
                    "invalid import dependency interval admitted");
        }
    }
    for (uint32_t alias : {0u, 0x80000000u, 0xa0000000u})
    {
        IopMemory memory; IopCpuCore core(memory);
        for (const auto &entry : entries) memory.write32(entry.physicalPc, entry.instruction);
        IopNativeDispatch native(memory, core, IopNativeProgram{{}, modules});
        require(native.bindModule(source, loaded) && native.guardImport(alias | 0x10014u, 0x10000u),
                "bound data identity could not guard an aliased import dependency");
        IopCpuState cpu{}; cpu.pc = alias | 0x10004u;
        require(!native.execute(cpu) && native.fault()->kind == IopNativeFaultKind::MissingEntry,
                "import metadata guard made relocated data executable");
        native.resetFault();
        memory.write8(alias | 0x10004u, 1u);
        require(!native.guardImport(alias | 0x10014u, 0x10000u) &&
                native.fault()->kind == IopNativeFaultKind::CodeChanged,
                "changed relocated metadata escaped the import guard");
        native.reset();
        require(!native.guardImport(alias | 0x10014u, 0x10000u), "reset retained import data identities");
    }
}

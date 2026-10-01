#include "iop_compat_test_support.h"
#include "emulator/core/iop_native.h"
#include "emulator/core/iop_cpu.h"
#include "emulator/core/iop_memory.h"

#include <array>
#include <functional>

using namespace ps2x::iop::detail;
using iop_test::require;

const IopNativeProgram &iopFixtureBank();
const IopNativeProgram &iopRpcFixtureBank();
void nativeFamilyBindings();
void nativeFamilyAdmission();
void nativeFamilyStartup();
void nativeFamilyReplacement();
#if PS2X_IOP_ENABLE_INTERPRETER
void nativeFamilyDifferential();
void nativeOperandDifferential();
#endif

namespace
{
    const IopNativeEntry &entryFor(uint32_t word)
    {
        for (const auto &entry : iopFixtureBank().entries)
            if (entry.instruction == word)
                return entry;
        throw std::runtime_error("fixture instruction missing");
    }

    void loadBank(IopMemory &memory)
    {
        for (const auto &entry : iopFixtureBank().entries)
            memory.write32(entry.physicalPc, entry.instruction);
    }

    bool sameCpu(const IopCpuState &a, const IopCpuState &b)
    {
        return a.gpr == b.gpr && a.hi == b.hi && a.lo == b.lo && a.pc == b.pc &&
               a.cop0 == b.cop0 && a.pendingLoadReg == b.pendingLoadReg &&
               a.pendingLoadValue == b.pendingLoadValue && a.pendingLoad == b.pendingLoad &&
               a.branchPending == b.branchPending && a.branchTarget == b.branchTarget &&
               a.stopped == b.stopped && a.yielded == b.yielded && a.exception == b.exception;
    }

    std::vector<uint8_t> moduleImage()
    {
        const auto entries = iopFixtureBank().entries;
        std::vector<uint8_t> image(0x100u + entries.size() * 4u, 0u);
        auto put16 = [&](size_t offset, uint16_t word)
        {
            image[offset] = static_cast<uint8_t>(word);
            image[offset + 1u] = static_cast<uint8_t>(word >> 8u);
        };
        auto put32 = [&](size_t offset, uint32_t word)
        {
            for (size_t byte = 0; byte != 4u; ++byte)
                image[offset + byte] = static_cast<uint8_t>(word >> (byte * 8u));
        };
        image[0] = 0x7fu; image[1] = 'E'; image[2] = 'L'; image[3] = 'F';
        image[4] = 1u; image[5] = 1u; image[6] = 1u;
        put16(16, 2u); put16(18, 8u); put32(20, 1u);
        put32(24, entries.front().physicalPc); put32(28, 52u);
        put16(40, 52u); put16(42, 32u); put16(44, 1u);
        put32(52, 1u); put32(56, 0x100u);
        put32(60, entries.front().physicalPc); put32(64, entries.front().physicalPc);
        put32(68, static_cast<uint32_t>(entries.size() * 4u));
        put32(72, static_cast<uint32_t>(entries.size() * 4u)); put32(76, 5u); put32(80, 4u);
        for (size_t i = 0; i != entries.size(); ++i)
            put32(0x100u + i * 4u, entries[i].instruction);
        return image;
    }

    void startupModule()
    {
        iop_test::Host host;
        host.file = moduleImage();
        ps2x::iop::IopSubsystem iop(host, iopFixtureBank());
        const auto loaded = iop.loadModule("host:native-test.irx");
        require(loaded.moduleId > 0 && loaded.startResult == 7, "native IRX startup/delay slot failed");
        const auto snapshot = iop.debugSnapshot();
        require(snapshot.nativeInstructions == 5u, "startup native instruction accounting");
        require(snapshot.interpretedInstructions == 0u, "startup used an interpreter");
        require(snapshot.nativeFaults == 0u, "startup native fault");
    }

    void unknownModule()
    {
        iop_test::Host host;
        host.file = moduleImage();
        host.file[0x100u] ^= 1u;
        ps2x::iop::IopSubsystem iop(host, iopFixtureBank());
        const auto loaded = iop.loadModule("host:changed-test.irx");
        require(loaded.moduleId < 0, "changed instruction published a module");
        const auto snapshot = iop.debugSnapshot();
        require(snapshot.emulatorLoadedModules == 0u, "failed module was retained");
        require(snapshot.nativeFaults == 1u && snapshot.interpretedInstructions == 0u,
                "changed code did not fail without interpreting");
        require(!snapshot.diagnostics.empty(), "native failure diagnostic missing");
        host.file = moduleImage();
        require(iop.loadModule("host:native-test.irx").moduleId < 0, "native fault was not persistent");
        iop.reset();
        require(iop.loadModule("host:native-test.irx").startResult == 7, "reset lost compiled bank");
    }

    void emptyBank()
    {
        iop_test::Host host;
        host.file = moduleImage();
        const IopNativeProgram empty{};
        ps2x::iop::IopSubsystem iop(host, empty);
        require(iop.loadModule("host:unknown.irx").moduleId < 0, "empty AOT bank interpreted a module");
        require(iop.debugSnapshot().interpretedInstructions == 0u, "empty bank invoked interpreter");
    }

    void importContracts()
    {
        for (const bool known : {false, true})
        {
            iop_test::Host host;
            iop_test::Irx image;
            image.words(0u, {0x41e00000u, 0u, 0x0101u,
                            known ? 0x6d737973u : 0x6e6b6e75u,
                            known ? 0x00006d65u : 0x006e776fu,
                            0x03e00008u, 0x24000006u, 0u, 0u});
            const uint32_t entry = 0x10014u;
            for (size_t i = 0; i != 4u; ++i)
                image.bytes[24u + i] = static_cast<uint8_t>(entry >> (i * 8u));
            host.file = image.bytes;
            const IopNativeProgram empty{};
            ps2x::iop::IopSubsystem iop(host, empty);
            const auto loaded = iop.loadModule("host:import-test.irx");
            const auto snapshot = iop.debugSnapshot();
            require(snapshot.interpretedInstructions == 0u && snapshot.nativeInstructions == 0u,
                    "import contract executed guest instructions");
            if (known)
                require(loaded.moduleId > 0 && loaded.startResult == static_cast<int32_t>(IopMemory::RamSize),
                        "known sysmem contract failed");
            else
                require(loaded.moduleId < 0 && snapshot.nativeFaults == 1u,
                        "unknown import returned fake success");
        }
    }

    void unfinishedStartup()
    {
        iop_test::Host host;
        host.file = moduleImage();
        const uint32_t loop = entryFor(0x1000ffffu).physicalPc;
        for (size_t i = 0; i != 4u; ++i)
            host.file[24u + i] = static_cast<uint8_t>(loop >> (i * 8u));
        ps2x::iop::IopSubsystem iop(host, iopFixtureBank());
        require(iop.loadModule("host:unfinished.irx").moduleId < 0,
                "unfinished native startup published success after its budget");
        require(iop.debugSnapshot().nativeFaults == 1u, "unfinished startup lacked a native fault");
    }

    void unsupportedRelocation()
    {
        iop_test::Host host; host.file = moduleImage(); host.file.resize(0x458u, 0u);
        auto put32 = [&](size_t offset, uint32_t word)
        {
            for (size_t i = 0; i != 4u; ++i)
                host.file[offset + i] = static_cast<uint8_t>(word >> (i * 8u));
        };
        put32(32u, 0x400u); host.file[46u] = 40u; host.file[48u] = 2u;
        put32(0x404u, 1u); put32(0x408u, 2u); put32(0x40cu, 0x10000u);
        put32(0x410u, 0x100u); put32(0x414u, 4u);
        put32(0x42cu, 9u); put32(0x438u, 0x450u); put32(0x43cu, 8u);
        put32(0x448u, 4u); put32(0x44cu, 8u);
        put32(0x450u, 0u); put32(0x454u, 0xfeu);
        ps2x::iop::IopSubsystem iop(host, iopFixtureBank());
        require(iop.loadModule("host:unsupported-reloc.irx").moduleId < 0,
                "unsupported relocation published native module");
        require(iop.debugSnapshot().nativeFaults == 1u && iop.debugSnapshot().nativeInstructions == 0u,
                "unsupported relocation executed native instructions");
    }

    void branchResume()
    {
        IopMemory memory;
        IopCpuCore core(memory);
        IopNativeDispatch native(memory, core, iopFixtureBank());
        loadBank(memory);
        IopCpuState cpu{};
        cpu.pc = 0x10000u;
        cpu.gpr[31] = 0x12340000u;
        require(native.execute(cpu) && cpu.pc == 0x10004u, "first instruction failed");
        require(native.execute(cpu) && cpu.pc == 0x10008u && cpu.branchPending, "branch checkpoint failed");
        const auto saved = cpu;
        require(native.execute(cpu) && cpu.pc == 0x10010u && cpu.gpr[2] == 7u, "delay slot failed");
        const auto expected = cpu;
        cpu = saved;
        require(native.execute(cpu) && sameCpu(cpu, expected), "restored branch state diverged");
        require(native.execute(cpu) && native.execute(cpu) && cpu.pc == 0x12340000u, "indirect JR/delay failed");
    }

    void loadDelay()
    {
        IopMemory memory;
        IopCpuCore core(memory);
        IopNativeDispatch native(memory, core, iopFixtureBank());
        loadBank(memory);
        memory.write32(0x20000u, 0x12345678u);
        IopCpuState cpu{};
        cpu.pc = entryFor(0x8c820000u).physicalPc;
        cpu.gpr[4] = 0x20000u; cpu.gpr[2] = 17u;
        require(native.execute(cpu) && cpu.pendingLoad && cpu.gpr[2] == 17u, "LW published too soon");
        const auto saved = cpu;
        require(native.execute(cpu) && cpu.gpr[3] == 17u && cpu.gpr[2] == 0x12345678u, "load consumer ordering");
        const auto expected = cpu;
        cpu = saved;
        require(native.execute(cpu) && sameCpu(cpu, expected), "restored pending load diverged");
        cpu = saved; cpu.pc = entryFor(0x24020009u).physicalPc;
        require(native.execute(cpu) && cpu.gpr[2] == 9u && !cpu.pendingLoad, "write did not suppress pending load");
    }

    void identityFailure()
    {
        IopMemory memory;
        IopCpuCore core(memory);
        IopNativeDispatch native(memory, core, iopFixtureBank());
        loadBank(memory);
        IopCpuState cpu{}; cpu.pc = 0x10000u;
        memory.write32(cpu.pc, 0u);
        const auto saved = cpu;
        require(!native.execute(cpu) && sameCpu(cpu, saved), "identity guard executed changed code");
        require(native.fault() && native.fault()->kind == IopNativeFaultKind::CodeChanged,
                "changed code fault kind");
        require(native.fault()->observedInstruction == 0u && native.fault()->expectedInstruction == 0x24080001u,
                "changed code evidence");
        loadBank(memory);
        require(!native.execute(cpu), "fault auto-cleared");
        native.resetFault();
        require(native.execute(cpu), "reset fault could not resume");
    }

    void missingAndAlignment()
    {
        IopMemory memory;
        IopCpuCore core(memory);
        IopNativeDispatch native(memory, core, iopFixtureBank());
        IopCpuState cpu{}; cpu.pc = 0x20000u;
        const auto saved = cpu;
        require(!native.execute(cpu) && sameCpu(cpu, saved), "missing target executed");
        require(native.fault()->kind == IopNativeFaultKind::MissingEntry, "missing target fault kind");
        native.resetFault(); cpu.pc = 0x10001u;
        require(!native.execute(cpu) && native.fault()->kind == IopNativeFaultKind::MisalignedPc,
                "misaligned PC was admitted");
    }

    void selfModification()
    {
        IopMemory memory; IopCpuCore core(memory);
        IopNativeDispatch native(memory, core, iopFixtureBank()); loadBank(memory);
        IopCpuState cpu{}; cpu.pc = entryFor(0xac8a0000u).physicalPc;
        cpu.gpr[4] = cpu.pc + 4u; cpu.gpr[10] = 0u;
        require(native.execute(cpu), "compiled guest store failed");
        const auto saved = cpu;
        require(!native.execute(cpu) && sameCpu(cpu, saved), "self modification executed stale next instruction");
        require(native.fault()->kind == IopNativeFaultKind::CodeChanged, "self modification fault kind");
    }

    void memoryWriterGuards()
    {
        for (unsigned writer = 0; writer != 5u; ++writer)
        {
            IopMemory memory; IopCpuCore core(memory);
            IopNativeDispatch native(memory, core, iopFixtureBank()); loadBank(memory);
            const uint32_t address = 0xa0010000u;
            const uint32_t zero = 0u;
            switch (writer)
            {
            case 0: memory.write8(address, 0u); break;
            case 1: memory.write16(address, 0u); break;
            case 2: memory.write32(address, 0u); break;
            case 3: require(memory.writeRam(address, &zero, sizeof(zero)), "bulk code write failed"); break;
            case 4: require(memory.zeroRam(address, sizeof(zero)), "code zero failed"); break;
            }
            IopCpuState cpu{}; cpu.pc = 0x80010000u;
            require(!native.execute(cpu) && native.fault()->kind == IopNativeFaultKind::CodeChanged,
                    "RAM writer bypassed native identity guard");
        }
    }

    void aliasesAndInterior()
    {
        for (const uint32_t alias : {0u, 0x80000000u, 0xa0000000u})
        {
            IopMemory memory; IopCpuCore core(memory);
            IopNativeDispatch native(memory, core, iopFixtureBank()); loadBank(memory);
            IopCpuState cpu{}; cpu.pc = alias | 0x10008u;
            require(native.execute(cpu) && cpu.gpr[2] == 7u && cpu.pc == (alias | 0x1000cu),
                    "alias/interior entry lost virtual PC");
        }
    }

    void invalidBanks()
    {
        auto bad = [&](std::vector<IopNativeEntry> entries)
        {
            IopMemory memory; IopCpuCore core(memory); bool rejected = false;
            try { IopNativeDispatch native(memory, core, IopNativeProgram{entries}); }
            catch (const std::invalid_argument &) { rejected = true; }
            require(rejected, "invalid native bank accepted");
        };
        const auto entry = iopFixtureBank().entries.front();
        bad({entry, entry});
        auto changed = entry; changed.physicalPc += 1u; bad({changed});
        changed = entry; changed.physicalPc = IopMemory::RamSize; bad({changed});
        changed = entry; changed.execute = nullptr; bad({changed});
    }

    void bankOwnership()
    {
        IopMemory memory; IopCpuCore core(memory);
        std::vector<IopNativeEntry> entries(iopFixtureBank().entries.begin(), iopFixtureBank().entries.end());
        std::reverse(entries.begin(), entries.end());
        IopNativeDispatch native(memory, core, IopNativeProgram{entries});
        entries.clear(); entries.shrink_to_fit(); loadBank(memory);
        IopCpuState cpu{}; cpu.pc = 0x10000u;
        require(native.execute(cpu) && cpu.gpr[8] == 1u, "native bank retained borrowed entry storage");
    }

    void nativeRpc()
    {
        constexpr uint32_t sid = 0xf00dcafeu, reply = 0x42424242u;
        iop_test::Host host;
        host.file = iop_test::rpcServer(sid, reply).bytes;
        // This fixture also checks the bank matches the source builder, not just itself.
        const auto image = std::span<const uint8_t>(host.file).subspan(0x100u);
        require(image.size() == iopRpcFixtureBank().entries.size() * 4u, "RPC bank image size");
        for (size_t i = 0; i != iopRpcFixtureBank().entries.size(); ++i)
        {
            uint32_t word = 0;
            for (size_t byte = 0; byte != 4u; ++byte)
                word |= static_cast<uint32_t>(image[i * 4u + byte]) << (byte * 8u);
            require(word == iopRpcFixtureBank().entries[i].instruction, "RPC bank differs from source IRX");
        }
        ps2x::iop::IopSubsystem iop(host, iopRpcFixtureBank());
        const auto loaded = iop.loadModule("host:native-rpc.irx");
        require(loaded.moduleId > 0 && loaded.startResult == 0, "native RPC server startup failed");
        require(iop.canBindRpc(sid), "native RPC endpoint not registered");
        const auto before = iop.debugSnapshot().nativeInstructions;
        const auto result = iop.handleRpc(iop_test::request(sid, 0u));
        require(result.handled && host.word(0x800u) == reply, "native RPC callback reply failed");
        const auto snapshot = iop.debugSnapshot();
        require(snapshot.nativeInstructions == before + 4u && snapshot.interpretedInstructions == 0u,
                "RPC callback was not entirely native");
        const uint32_t zero = 0;
        require(iop.writeMemory(0xa0010300u, &zero, sizeof(zero)), "RPC callback code mutation failed");
        require(!iop.handleRpc(iop_test::request(sid, 0u)).handled, "changed callback RPC published success");
        require(iop.debugSnapshot().nativeFaults == 1u && !iop.canBindRpc(sid), "failed RPC endpoint remained ready");
    }

#if PS2X_IOP_ENABLE_INTERPRETER
    void identifiedModelDifferential()
    {
        uint32_t rng = 0x3197u;
        auto next = [&]() { rng ^= rng << 13u; rng ^= rng >> 17u; rng ^= rng << 5u; return rng; };
        for (const auto &entry : iopFixtureBank().entries)
        {
            for (uint32_t run = 0; run != 4u; ++run)
            {
                IopMemory a, b; IopCpuCore ca(a), cb(b);
                IopNativeDispatch native(b, cb, iopFixtureBank()); loadBank(a); loadBank(b);
                IopCpuState sa{};
                sa.pc = entry.physicalPc;
                for (uint32_t reg = 1u; reg != 32u; ++reg) sa.gpr[reg] = next();
                sa.gpr[4] = 0x20000u + run;
                sa.hi = next(); sa.lo = next();
                for (auto &value : sa.cop0) value = next();
                sa.pendingLoad = (run & 1u) != 0u; sa.pendingLoadReg = 10u; sa.pendingLoadValue = next();
                sa.branchPending = (run & 2u) != 0u; sa.branchTarget = 0x12340000u;
                a.write32(0x20000u, 0x76543210u); b.write32(0x20000u, 0x76543210u);
                auto sb = sa;
                const bool expected = ca.executeInstruction(sa);
                const bool actual = native.execute(sb);
                require(expected == actual && sameCpu(sa, sb), "AOT differed from identified CPU model");
                require(std::equal(a.ram().begin(), a.ram().end(), b.ram().begin()), "AOT RAM differed from model");
            }
        }
    }
#endif
}

int main()
{
    const std::pair<const char *, std::function<void()>> tests[] = {
        {"native IRX startup", startupModule}, {"changed module/reset", unknownModule},
        {"empty strict bank", emptyBank}, {"branch checkpoints", branchResume},
        {"native import contracts", importContracts},
        {"unfinished startup", unfinishedStartup},
        {"unsupported relocation", unsupportedRelocation},
        {"load delay checkpoints", loadDelay}, {"identity guard", identityFailure},
        {"missing/alignment", missingAndAlignment}, {"aliases/interior", aliasesAndInterior},
        {"compiled self modification", selfModification}, {"all RAM writers", memoryWriterGuards},
        {"invalid banks", invalidBanks}, {"bank ownership", bankOwnership},
        {"native RPC and changed callback", nativeRpc},
        {"relocatable family bindings/reset", nativeFamilyBindings},
        {"relocatable family admission", nativeFamilyAdmission},
        {"relocatable family startup", nativeFamilyStartup},
        {"relocatable family replacement", nativeFamilyReplacement},
#if PS2X_IOP_ENABLE_INTERPRETER
        {"identified model differential", identifiedModelDifferential},
        {"identified model family differential", nativeFamilyDifferential},
        {"identified model operand differential", nativeOperandDifferential},
#endif
    };
    unsigned failures = 0;
    for (const auto &[name, test] : tests)
    {
        try { test(); std::cout << "PASS " << name << '\n'; }
        catch (const std::exception &error) { ++failures; std::cerr << "FAIL " << name << ": " << error.what() << '\n'; }
    }
    return failures == 0u ? 0 : 1;
}

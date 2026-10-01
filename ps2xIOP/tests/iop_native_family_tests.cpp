#include "iop_compat_test_support.h"
#include "emulator/core/iop_native.h"
#include "emulator/core/iop_cpu.h"
#include "emulator/core/iop_memory.h"
#include "emulator/services/iop_module_loader.h"
#include "iop_native_semantics.h"

#include <array>

using namespace ps2x::iop::detail;
using iop_test::require;

namespace
{
    struct Fixture
    {
        std::vector<uint8_t> image = std::vector<uint8_t>(0x2a0u, 0u);
        std::vector<IopNativeModuleEntry> entries;
        std::array<IopNativeModule, 1> modules;

        explicit Fixture(uint32_t memoryBytes = 32u)
        {
            auto put16 = [&](size_t offset, uint16_t value)
            {
                image[offset] = static_cast<uint8_t>(value);
                image[offset + 1u] = static_cast<uint8_t>(value >> 8u);
            };
            auto put32 = [&](size_t offset, uint32_t value)
            {
                for (unsigned byte = 0; byte < 4u; ++byte)
                    image[offset + byte] = static_cast<uint8_t>(value >> (byte * 8u));
            };
            image[0] = 0x7fu; image[1] = 'E'; image[2] = 'L'; image[3] = 'F';
            image[4] = 1u; image[5] = 1u; image[6] = 1u;
            put16(16, 0xff80u); put16(18, 8u); put32(20, 1u);
            put32(28, 52u); put32(32, 0x200u);
            put16(40, 52u); put16(42, 32u); put16(44, 1u);
            put16(46, 40u); put16(48, 2u);
            put32(52, 1u); put32(56, 0x100u); put32(68, 32u);
            put32(72, memoryBytes); put32(76, 7u); put32(80, 4u);
            const uint32_t words[] = {0x3c020000u, 0x2442001cu, 0x08000005u, 0u,
                                      0x2402ffffu, 0x03e00008u, 0u, 12u};
            for (unsigned i = 0; i < 8u; ++i) put32(0x100u + i * 4u, words[i]);
            put32(0x204u, 1u); put32(0x208u, 6u); put32(0x210u, 0x100u);
            put32(0x214u, 32u); put32(0x220u, 4u);
            put32(0x22cu, 9u); put32(0x238u, 0x280u); put32(0x23cu, 32u);
            put32(0x248u, 4u); put32(0x24cu, 8u);
            const uint32_t offsets[] = {0u, 4u, 8u, 28u};
            const uint32_t types[] = {5u, 6u, 4u, 2u};
            for (unsigned i = 0; i < 4u; ++i)
            {
                put32(0x280u + i * 8u, offsets[i]);
                put32(0x284u + i * 8u, types[i]);
            }
            IopMemory canonical;
            const auto loaded = IopModuleLoader::load(image, canonical, 0x10000u);
            require(loaded && loaded.relocationsComplete && loaded.size == ((memoryBytes + 255u) & ~255u),
                    "family fixture load failed");
            for (uint32_t offset = 0; offset < loaded.size; offset += 4u)
            {
                const uint32_t word = canonical.read32(loaded.base + offset);
                uint32_t mask = 0u;
                for (const auto &relocation : loaded.relocationMasks)
                    if (relocation.offset == offset) mask |= relocation.mask;
                IopNativeModuleEntry entry{offset, word, mask};
                switch (word)
                {
                case 0x3c020001u: entry.executeOperand = &IopNativeAccess::instructionRelocated<0x3c020001u, 0xffffu>; break;
                case 0x2442001cu: entry.executeOperand = &IopNativeAccess::instructionRelocated<0x2442001cu, 0xffffu>; break;
                case 0x08004005u: entry.executeOperand = &IopNativeAccess::instructionRelocated<0x08004005u, 0x03ffffffu>; break;
                case 0x2402ffffu: entry.execute = &IopNativeAccess::instruction<0x2402ffffu>; break;
                case 0x03e00008u: entry.execute = &IopNativeAccess::instruction<0x03e00008u>; break;
                case 0u: entry.execute = &IopNativeAccess::instruction<0u>; break;
                default: require(mask == 0xffffffffu, "unknown fixture operation"); break;
                }
                entries.push_back(entry);
            }
            modules[0] = IopNativeModule{image, loaded.size, entries};
        }
        IopNativeProgram program() const { return IopNativeProgram{{}, modules}; }
    };
}

void nativeFamilyBindings()
{
    Fixture fixture;
    const auto original = fixture.image;
    IopMemory memory;
    IopCpuCore core(memory);
    IopNativeDispatch native(memory, core, fixture.program());
    fixture.image.assign(fixture.image.size(), 0u);
    fixture.entries.clear(); // The dispatcher must own image and entry metadata.
    for (const uint32_t base : {0x20000u, 0x30000u})
    {
        const auto loaded = IopModuleLoader::load(original, memory, base);
        require(native.bindModule(original, loaded), "relocation-family binding failed");
        IopCpuState cpu{};
        cpu.pc = base | 0x80000000u; cpu.gpr[31] = 0x1ffffcu;
        for (unsigned i = 0; i < 6u; ++i) require(native.execute(cpu), "bound family execution failed");
        require(cpu.gpr[2] == base + 28u && cpu.pc == 0x1ffffcu,
                "HI/LO/J operands or aliased PC were not preserved");
    }
    IopCpuState cpu{}; cpu.pc = 0x3001cu;
    require(!native.execute(cpu) && native.fault()->kind == IopNativeFaultKind::MissingEntry,
            "whole-word relocated data acquired an executable operation");
    native.reset();
    cpu = {}; cpu.pc = 0x20000u;
    require(!native.execute(cpu), "reset retained a previous binding");
    native.reset();
    const auto loaded = IopModuleLoader::load(original, memory, 0x20000u);
    require(native.bindModule(original, loaded), "reset lost configured family");
    memory.write32(loaded.base, memory.read32(loaded.base) ^ 1u);
    cpu = {}; cpu.pc = loaded.base; const auto before = cpu;
    require(!native.execute(cpu) && native.fault()->kind == IopNativeFaultKind::CodeChanged && cpu.pc == before.pc,
            "changing only a bound operand escaped the identity guard");
}

void nativeFamilyAdmission()
{
    for (unsigned variant = 0; variant < 4u; ++variant)
    {
        Fixture fixture; IopMemory memory; IopCpuCore core(memory);
        IopNativeDispatch native(memory, core, fixture.program());
        auto loaded = IopModuleLoader::load(fixture.image, memory, 0x20000u);
        auto image = fixture.image;
        if (variant == 0u) image.back() ^= 1u; // Identity includes non-code bytes.
        if (variant == 1u) loaded.relocationMasks.clear();
        if (variant == 2u) memory.write32(loaded.base, 0x34020002u); // Different operation.
        if (variant == 3u) loaded.size += 4u;
        require(!native.bindModule(image, loaded) && native.fault(), "invalid family admitted");
        IopCpuState cpu{}; cpu.pc = loaded.entry;
        require(!native.execute(cpu) && cpu.pc == loaded.entry, "failed binding executed an instruction");
    }
    Fixture fixture;
    fixture.entries.front().relocationMask = 0xfc000000u;
    IopMemory memory; IopCpuCore core(memory);
    bool rejected = false;
    try { IopNativeDispatch native(memory, core, fixture.program()); }
    catch (const std::invalid_argument &) { rejected = true; }
    require(rejected, "operation-changing mask accepted in compiled manifest");
}

void nativeFamilyStartup()
{
    Fixture fixture; iop_test::Host host; host.file = fixture.image;
    ps2x::iop::IopSubsystem iop(host, fixture.program());
    for (unsigned instance = 0; instance < 2u; ++instance)
    {
        const auto loaded = iop.loadModule("host:family.irx");
        require(loaded.moduleId == static_cast<int32_t>(instance + 1u) &&
                loaded.startResult == static_cast<int32_t>(0x1001cu + instance * 256u),
                "automatic module-base binding failed during startup");
    }
    const auto snapshot = iop.debugSnapshot();
    require(snapshot.nativeInstructions == 12u && snapshot.interpretedInstructions == 0u &&
            snapshot.nativeFaults == 0u && snapshot.emulatorLoadedModules == 2u,
            "family startup used an interpreter or failed accounting");
    require(iop.stopModule(1) && iop.debugSnapshot().emulatorLoadedModules == 1u,
            "family unload did not preserve the other module");
    iop.reset();
    require(iop.loadModule("host:family.irx").startResult == 0x1001c, "subsystem reset lost family");
}

void nativeFamilyReplacement()
{
    Fixture fixture(512u); IopMemory memory; IopCpuCore core(memory);
    IopNativeDispatch native(memory, core, fixture.program());
    auto loaded = IopModuleLoader::load(fixture.image, memory, 0x20000u);
    require(native.bindModule(fixture.image, loaded), "initial replacement bank failed");
    loaded = IopModuleLoader::load(fixture.image, memory, 0x20100u);
    require(native.bindModule(fixture.image, loaded), "overlapping replacement failed");
    IopCpuState cpu{}; cpu.pc = 0x20100u; cpu.gpr[31] = 0x1ffffcu;
    for (unsigned i = 0; i < 6u; ++i) require(native.execute(cpu), "replacement execution failed");
    require(cpu.gpr[2] == 0x2011cu, "replacement used stale operands");
    native.unbindRange(0x20200u, 4u); // A partial unbind invalidates the whole binding.
    cpu = {}; cpu.pc = 0x20100u;
    require(!native.execute(cpu) && native.fault()->kind == IopNativeFaultKind::MissingEntry,
            "partial unload retained a module directory");
    native.reset();
    loaded = IopModuleLoader::load(fixture.image, memory, 0x20000u);
    require(native.bindModule(fixture.image, loaded), "replacement reset failed");
    loaded = IopModuleLoader::load(fixture.image, memory, 0x20100u);
    require(native.bindModule(fixture.image, loaded), "second replacement failed");
    cpu = {}; cpu.pc = 0x20000u;
    require(!native.execute(cpu) && native.fault()->kind == IopNativeFaultKind::MissingEntry,
            "overlap retained part of an invalidated module directory");
}

#if PS2X_IOP_ENABLE_INTERPRETER
void nativeFamilyDifferential()
{
    Fixture fixture;
    for (uint32_t base : {0x20000u, 0x28000u, 0x30000u})
    {
        IopMemory a, b; IopCpuCore ca(a), cb(b);
        const auto la = IopModuleLoader::load(fixture.image, a, base);
        const auto lb = IopModuleLoader::load(fixture.image, b, base);
        IopNativeDispatch native(b, cb, fixture.program());
        require(la && native.bindModule(fixture.image, lb), "differential family setup failed");
        for (uint32_t offset : {0u, 4u, 8u})
        {
            IopCpuState sa{}; sa.pc = base + offset; sa.gpr[2] = 0x7fffffff;
            sa.pendingLoad = true; sa.pendingLoadReg = 10u; sa.pendingLoadValue = 31u;
            sa.branchPending = true; sa.branchTarget = 0x40000u;
            auto sb = sa;
            require(ca.executeInstruction(sa) == native.execute(sb), "family return differs from model");
            require(sa.gpr == sb.gpr && sa.pc == sb.pc && sa.branchPending == sb.branchPending &&
                    sa.branchTarget == sb.branchTarget && sa.pendingLoad == sb.pendingLoad &&
                    sa.pendingLoadReg == sb.pendingLoadReg && sa.pendingLoadValue == sb.pendingLoadValue &&
                    sa.cop0 == sb.cop0 && sa.exception == sb.exception && sa.hi == sb.hi && sa.lo == sb.lo &&
                    sa.stopped == sb.stopped && sa.yielded == sb.yielded,
                    "parameterized operation state differs from identified model");
            require(std::equal(a.ram().begin(), a.ram().end(), b.ram().begin()), "family RAM differs from model");
        }
    }
}

namespace
{
    template <uint32_t Instruction>
    constexpr auto operandCase()
    {
        return std::pair<uint32_t, IopNativeOperandFunction>{Instruction,
            &IopNativeAccess::instructionRelocated<Instruction, 0xffffu>};
    }
}

void nativeOperandDifferential()
{
    const std::array cases = {
        operandCase<0x04800000u>(),
        operandCase<0x10820000u>(), operandCase<0x14820000u>(),
        operandCase<0x18800000u>(), operandCase<0x1c800000u>(),
        operandCase<0x20820000u>(), operandCase<0x24820000u>(),
        operandCase<0x28820000u>(), operandCase<0x2c820000u>(),
        operandCase<0x30820000u>(), operandCase<0x34820000u>(),
        operandCase<0x38820000u>(), operandCase<0x3c020000u>(),
        operandCase<0x80820000u>(), operandCase<0x84820000u>(),
        operandCase<0x88820000u>(), operandCase<0x8c820000u>(),
        operandCase<0x90820000u>(), operandCase<0x94820000u>(),
        operandCase<0x98820000u>(), operandCase<0xa0820000u>(),
        operandCase<0xa4820000u>(), operandCase<0xa8820000u>(),
        operandCase<0xac820000u>(), operandCase<0xb8820000u>()};
    for (const auto &[instruction, operation] : cases)
        for (uint32_t imm : {0u, 1u, 3u, 0x7fffu, 0x8000u, 0xffffu})
        {
            IopMemory a, b; IopCpuCore ca(a), cb(b);
            const uint32_t bound = instruction | imm;
            a.write32(0x10000u, bound); b.write32(0x10000u, bound);
            a.write32(0x20020u, 0x76543210u); b.write32(0x20020u, 0x76543210u);
            IopCpuState sa{}; sa.pc = 0x10000u; sa.gpr[4] = 0x20020u; sa.gpr[2] = 0x12345678u;
            sa.pendingLoad = true; sa.pendingLoadReg = 2u; sa.pendingLoadValue = 0xdeadbeefu;
            sa.branchPending = (imm & 1u) != 0u; sa.branchTarget = 0x40000u;
            auto sb = sa;
            require(ca.executeInstruction(sa) == operation(sb, b, cb, bound), "operand return differs from model");
            require(sa.gpr == sb.gpr && sa.hi == sb.hi && sa.lo == sb.lo && sa.pc == sb.pc &&
                    sa.cop0 == sb.cop0 && sa.pendingLoadReg == sb.pendingLoadReg &&
                    sa.pendingLoadValue == sb.pendingLoadValue && sa.pendingLoad == sb.pendingLoad &&
                    sa.branchPending == sb.branchPending && sa.branchTarget == sb.branchTarget &&
                    sa.stopped == sb.stopped && sa.yielded == sb.yielded && sa.exception == sb.exception,
                    "operand operation state differs from identified model");
            require(std::equal(a.ram().begin(), a.ram().end(), b.ram().begin()), "operand RAM differs from model");
        }
}
#endif

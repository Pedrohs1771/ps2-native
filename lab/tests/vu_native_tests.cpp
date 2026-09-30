#include "MiniTest.h"
#include "nexo/vu_native.h"
#include "nexo/vu_native_semantics.h"
#include "nexo/vu_snapshot.h"
#include "nexo/vu_runtime_binding.h"
#include "nexo/vu_replay.h"
#include "ps2_runtime.h"
#include "runtime/gs/gs_frontend.h"
#include "runtime/ps2_memory.h"

#include <array>
#include <cfenv>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <vector>

using namespace ps2native::nexo;
namespace
{
constexpr uint32_t nop = 0x000002FFu;
constexpr uint32_t lowerNop = 0x8000033Cu;
constexpr uint32_t add = (0x8u << 21) | (2u << 16) | (1u << 11) | (3u << 6) | 0x28;
constexpr uint32_t subtract = (add & ~0x3Fu) | 0x2Cu;
constexpr uint32_t endAdd = add | (1u << 30);
constexpr uint32_t endSubtract = subtract | (1u << 30);
constexpr uint32_t endAddDT = endAdd | (1u << 28) | (1u << 27);
constexpr uint32_t special(uint32_t op, uint32_t is, uint32_t it = 0, uint32_t dest = 0)
{
    return (0x40u << 25) | (dest << 21) | (it << 16) | (is << 11) | ((op & 0x7Cu) << 4) | (op & 3u) | 0x3Cu;
}
constexpr uint32_t divideQ = special(0x38, 1, 2);
constexpr uint32_t waitQ = special(0x3B, 0);
constexpr uint32_t esadd = special(0x70, 1);
constexpr uint32_t eleng = special(0x72, 1);
constexpr uint32_t waitP = special(0x7B, 0);
constexpr uint32_t kick = special(0x6C, 1);
constexpr uint32_t increment = (0x08u << 25) | (1u << 16) | (1u << 11) | 1;
constexpr uint32_t branch = (0x29u << 25) | (2u << 16) | (1u << 11) | 3;
constexpr uint32_t store = (0x01u << 25) | (0xFu << 21) | (3u << 11) | 1;
bool nativeZone = false;
unsigned forbiddenCalls = 0;
struct NativeScope
{
    bool previous = nativeZone;
    NativeScope() { nativeZone = true; }
    ~NativeScope() { nativeZone = previous; }
};

struct Fixture
{
    PS2Memory memory;
    GS gs;
    std::vector<std::vector<uint8_t>> packets;
    Fixture()
    {
        if (!memory.initialize()) throw std::runtime_error("cannot initialize native fixture");
        gs.init(memory.getGSVRAM(), PS2_GS_VRAM_SIZE, &memory.gs());
        std::memset(memory.getVU1Code(), 0, PS2_VU1_CODE_SIZE);
        std::memset(memory.getVU1Data(), 0, PS2_VU1_DATA_SIZE);
        memory.setGifPacketCallback([this](const uint8_t *bytes, uint32_t size)
        { packets.emplace_back(bytes, bytes + size); });
    }
};

void word(uint8_t *bytes, uint32_t value)
{
    for (unsigned i = 0; i < 4; ++i) bytes[i] = uint8_t(value >> (8 * i));
}

struct CaseDirectory
{
    std::filesystem::path path = std::filesystem::temp_directory_path() /
        ("nexo-native-vif-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    CaseDirectory() { std::filesystem::create_directory(path); }
    ~CaseDirectory() { std::error_code error; std::filesystem::remove_all(path, error); }
    void write(const char *name, std::span<const uint8_t> bytes)
    {
        std::ofstream stream(path / name, std::ios::binary);
        stream.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
        if (!stream) throw std::runtime_error("cannot write native VIF fixture");
    }
};

struct SyntheticBank
{
    std::vector<uint8_t> code = std::vector<uint8_t>(PS2_VU1_CODE_SIZE);
    std::vector<VuNativeAccess::DecodedPair> descriptors;
    std::vector<VuNativeAccess::Entry> entries;

    explicit SyntheticBank(bool alternate = false, bool dt = false)
    {
        for (size_t pc = 0; pc < code.size(); pc += 8)
        { word(code.data() + pc, lowerNop); word(code.data() + pc + 4, nop); }
        word(code.data() + 4, dt ? endAddDT : alternate ? endSubtract : endAdd);
        word(code.data() + 12, alternate ? subtract : nop);
        word(code.data() + 20, alternate ? endSubtract : endAdd);
        descriptors = VuNativeAccess::inspectMicrocode(code, VU1Interpreter::Unit::VU1);
        for (const auto &pair : descriptors)
        {
            decltype(VuNativeAccess::Entry::upper) upper = nullptr;
            switch (pair.upper)
            {
#define U(value) case value: upper = &VuNativeAccess::upper<value>; break
                U(nop); U(endAdd); U(endSubtract); U(subtract); U(endAddDT);
#undef U
            default: throw std::runtime_error("uncompiled synthetic upper operation");
            }
            entries.push_back({&pair, upper, &VuNativeAccess::lower<lowerNop>});
        }
    }
    VuNativeProgram program() const { return {VU1Interpreter::Unit::VU1, entries, code}; }
};

void prepareRuntime(PS2Runtime &runtime, const SyntheticBank &bank)
{
    if (!runtime.memory().initialize() || !runtime.syncCoreSubsystems())
        throw std::runtime_error("cannot initialize headless VU runtime");
    std::memcpy(runtime.memory().getVU1Code(), bank.code.data(), bank.code.size());
    runtime.memory().markVU1CodeModified();
    runtime.vu1().state().vf[1][0] = 4;
    runtime.vu1().state().vf[2][0] = 7;
}

void vifCommand(PS2Runtime &runtime, uint32_t opcode, uint16_t immediate = 0)
{
    std::array<uint8_t, 4> command{};
    word(command.data(), (opcode << 24) | immediate);
    runtime.memory().processVIF1Data(command.data(), command.size());
}

void compareRuntime(TestCase &t, PS2Runtime &native, PS2Runtime &reference)
{
    const auto nativeState = VuSnapshotCodec::encode(native.vu1());
    const auto referenceState = VuSnapshotCodec::encode(reference.vu1());
    if (nativeState != referenceState)
    {
        const size_t common = std::min(nativeState.size(), referenceState.size());
        for (size_t offset = 24; offset < common; ++offset)
            if (nativeState[offset] != referenceState[offset])
            { std::cerr << "Canonical runtime divergence: offset=" << offset << " native=" << unsigned(nativeState[offset])
                << " reference=" << unsigned(referenceState[offset]) << '\n'; break; }
        std::cerr << "VF3.x native=" << native.vu1().state().vf[3][0]
            << " reference=" << reference.vu1().state().vf[3][0] << '\n';
    }
    t.Equals(nativeState, referenceState,
        "VIF invokes native VU with the complete reference state and scheduler effects");
    t.IsTrue(std::memcmp(native.memory().getVU1Data(), reference.memory().getVU1Data(), PS2_VU1_DATA_SIZE) == 0,
        "Runtime native VU preserves memory effects");
    t.Equals(native.cpu().vu0_vpu_stat, reference.cpu().vu0_vpu_stat, "CPU-visible VU stop flags match");
    t.Equals(native.memory().vif1_regs.top, reference.memory().vif1_regs.top, "VIF TOP matches");
    t.Equals(native.memory().vif1_regs.itop, reference.memory().vif1_regs.itop, "VIF ITOP matches");
    t.Equals(native.memory().vif1_regs.stat, reference.memory().vif1_regs.stat, "VIF flags match");
    t.Equals(forbiddenCalls, 0u, "Runtime integration never invokes generic VU execution");
}

void compare(TestCase &t, uint32_t budget, bool pending, bool fresh = false)
{
    forbiddenCalls = 0;
    auto referenceMemory = std::make_unique<Fixture>();
    auto nativeMemory = std::make_unique<Fixture>();
    auto reference = std::make_unique<VU1Interpreter>();
    auto native = std::make_unique<VU1Interpreter>();
    word(referenceMemory->memory.getVU1Code(), lowerNop);
    word(referenceMemory->memory.getVU1Code() + 4, add);
    for (unsigned pc = 8; pc < PS2_VU1_CODE_SIZE; pc += 8)
    {
        word(referenceMemory->memory.getVU1Code() + pc, lowerNop);
        word(referenceMemory->memory.getVU1Code() + pc + 4, nop);
    }
    reference->state().vf[1][0] = 4;
    reference->state().vf[2][0] = 7;
    if (pending)
        reference->execute(referenceMemory->memory.getVU1Code(), PS2_VU1_CODE_SIZE,
            referenceMemory->memory.getVU1Data(), PS2_VU1_DATA_SIZE, referenceMemory->gs,
            &referenceMemory->memory, 0, 7, 9, 1);
    VuSnapshotCodec::restore(*native, VuSnapshotCodec::encode(*reference));
    native->state().top = reference->state().top = 7;
    native->state().itop = reference->state().itop = 9;
    const auto metadata = VuNativeAccess::inspectMicrocode(
        {referenceMemory->memory.getVU1Code(), PS2_VU1_CODE_SIZE}, VU1Interpreter::Unit::VU1);
    std::vector<VuNativeAccess::Entry> entries;
    entries.reserve(metadata.size());
    for (size_t i = 0; i < metadata.size(); ++i)
        entries.push_back({&metadata[i], i == 0 ? &VuNativeAccess::upper<add> : &VuNativeAccess::upper<nop>,
            &VuNativeAccess::lower<lowerNop>});
    const VuNativeProgram program{VU1Interpreter::Unit::VU1, entries,
        {referenceMemory->memory.getVU1Code(), PS2_VU1_CODE_SIZE}};
    if (fresh)
        reference->execute(referenceMemory->memory.getVU1Code(), PS2_VU1_CODE_SIZE,
            referenceMemory->memory.getVU1Data(), PS2_VU1_DATA_SIZE, referenceMemory->gs,
            &referenceMemory->memory, 0, 7, 9, budget);
    else
        reference->resume(referenceMemory->memory.getVU1Code(), PS2_VU1_CODE_SIZE,
            referenceMemory->memory.getVU1Data(), PS2_VU1_DATA_SIZE, referenceMemory->gs,
            &referenceMemory->memory, 7, 9, budget);
    {
        NativeScope scope;
        if (fresh)
            VuNativeAccess::execute(*native, program, nativeMemory->memory.getVU1Data(), PS2_VU1_DATA_SIZE,
                nativeMemory->gs, &nativeMemory->memory, 0, 7, 9, budget);
        else
            VuNativeAccess::resume(*native, program, nativeMemory->memory.getVU1Data(), PS2_VU1_DATA_SIZE,
                nativeMemory->gs, &nativeMemory->memory, 7, 9, budget);
    }
    t.Equals(forbiddenCalls, 0u, "Native execution makes no generic interpreter calls");
    t.Equals(VuSnapshotCodec::encode(*native), VuSnapshotCodec::encode(*reference),
        "Native execution preserves all registers and hidden scheduler state");
    t.IsTrue(std::memcmp(nativeMemory->memory.getVU1Data(), referenceMemory->memory.getVU1Data(),
        PS2_VU1_DATA_SIZE) == 0, "Native execution preserves data-memory effects");
    t.Equals(nativeMemory->packets, referenceMemory->packets, "Native execution preserves GIF submissions");
}

void compareLowerProgram(TestCase &t, std::span<const uint32_t> instructions,
    std::span<const uint32_t> budgets, bool changeKickPayload = false)
{
    forbiddenCalls = 0;
    auto referenceMemory = std::make_unique<Fixture>();
    auto nativeMemory = std::make_unique<Fixture>();
    auto reference = std::make_unique<VU1Interpreter>();
    auto native = std::make_unique<VU1Interpreter>();
    for (unsigned pc = 0; pc < PS2_VU1_CODE_SIZE; pc += 8)
    {
        word(referenceMemory->memory.getVU1Code() + pc,
            pc / 8 < instructions.size() ? instructions[pc / 8] : lowerNop);
        word(referenceMemory->memory.getVU1Code() + pc + 4, nop);
    }
    reference->state().vf[1][0] = 1; reference->state().vf[1][1] = 2; reference->state().vf[1][2] = 3;
    reference->state().vf[2][0] = 2;
    reference->state().vi[1] = 4; reference->state().vi[2] = 4;
    reference->state().vf[3][0] = 17; reference->state().vf[3][1] = -2;
    const uint64_t tag = 1u | (1ull << 15) | (2ull << 58);
    for (unsigned i = 0; i < 8; ++i) referenceMemory->memory.getVU1Data()[64 + i] = uint8_t(tag >> (8 * i));
    std::memset(referenceMemory->memory.getVU1Data() + 80, 0xAC, 16);
    std::memcpy(nativeMemory->memory.getVU1Data(), referenceMemory->memory.getVU1Data(), PS2_VU1_DATA_SIZE);
    VuSnapshotCodec::restore(*native, VuSnapshotCodec::encode(*reference));
    const auto metadata = VuNativeAccess::inspectMicrocode(
        {referenceMemory->memory.getVU1Code(), PS2_VU1_CODE_SIZE}, VU1Interpreter::Unit::VU1);
    std::vector<VuNativeAccess::Entry> entries;
    for (const auto &p : metadata)
    {
        decltype(VuNativeAccess::Entry::lower) operation = nullptr;
        switch (p.lower)
        {
#define OP(value) case value: operation = &VuNativeAccess::lower<value>; break
            OP(lowerNop); OP(divideQ); OP(waitQ); OP(esadd); OP(eleng); OP(waitP);
            OP(kick); OP(increment); OP(branch); OP(store);
#undef OP
        default: throw std::runtime_error("uncompiled synthetic fixture operation");
        }
        entries.push_back({&p, &VuNativeAccess::upper<nop>, operation});
    }
    const VuNativeProgram program{VU1Interpreter::Unit::VU1, entries,
        {referenceMemory->memory.getVU1Code(), PS2_VU1_CODE_SIZE}};
    for (size_t step = 0; step < budgets.size(); ++step)
    {
        reference->resume(referenceMemory->memory.getVU1Code(), PS2_VU1_CODE_SIZE,
            referenceMemory->memory.getVU1Data(), PS2_VU1_DATA_SIZE, referenceMemory->gs,
            &referenceMemory->memory, 7, 9, budgets[step]);
        {
            NativeScope scope;
            VuNativeAccess::resume(*native, program, nativeMemory->memory.getVU1Data(), PS2_VU1_DATA_SIZE,
                nativeMemory->gs, &nativeMemory->memory, 7, 9, budgets[step]);
        }
        t.Equals(VuSnapshotCodec::encode(*native), VuSnapshotCodec::encode(*reference),
            "Native state matches at each suspended continuation boundary");
        t.IsTrue(std::memcmp(nativeMemory->memory.getVU1Data(), referenceMemory->memory.getVU1Data(), PS2_VU1_DATA_SIZE) == 0,
            "Native memory effects match at each boundary");
        t.Equals(nativeMemory->packets, referenceMemory->packets, "Native packet effects match at each boundary");
        if (changeKickPayload && step == 0)
        {
            std::memset(referenceMemory->memory.getVU1Data() + 80, 0x5A, 16);
            std::memset(nativeMemory->memory.getVU1Data() + 80, 0x5A, 16);
        }
    }
    t.Equals(forbiddenCalls, 0u, "No generic execution is used for the lower program");
    if (changeKickPayload)
    {
        t.Equals(nativeMemory->packets.size(), size_t(1), "The resumed XGKICK completes exactly once");
        if (!nativeMemory->packets.empty()) t.Equals(nativeMemory->packets[0][16], uint8_t(0x5A),
            "XGKICK reads the future payload after the checkpoint, rather than eagerly copying it");
    }
}
}

void checkNativeTrap()
{
    if (nativeZone)
    {
        ++forbiddenCalls;
        throw std::runtime_error("generic interpreter execution in native zone");
    }
}

// GNU/LLVM link wrappers intercept calls at the reference-operation boundary.
// Reference execution is allowed outside NativeScope. The deliberate trap
// test verifies that this mechanism is actually active in the linked binary.
#define WRAP_METHOD(name, symbol, arguments, call) \
    extern "C" void real_##name arguments asm("__real_" symbol); \
    extern "C" void wrap_##name arguments asm("__wrap_" symbol); \
    extern "C" void wrap_##name arguments { checkNativeTrap(); real_##name call; }
WRAP_METHOD(upper, "_ZN14VU1Interpreter9execUpperEj", (VU1Interpreter *vu, uint32_t word), (vu, word))
WRAP_METHOD(lower, "_ZN14VU1Interpreter9execLowerEjPhjR2GSP9PS2Memoryj",
    (VU1Interpreter *vu, uint32_t word, uint8_t *data, uint32_t size, GS &gs, PS2Memory *memory, uint32_t upper),
    (vu, word, data, size, gs, memory, upper))
WRAP_METHOD(resume, "_ZN14VU1Interpreter6resumeEPhjS0_jR2GSP9PS2Memoryjjj",
    (VU1Interpreter *vu, uint8_t *code, uint32_t codeSize, uint8_t *data, uint32_t dataSize, GS &gs,
        PS2Memory *memory, uint32_t top, uint32_t itop, uint32_t budget),
    (vu, code, codeSize, data, dataSize, gs, memory, top, itop, budget))
WRAP_METHOD(execute, "_ZN14VU1Interpreter7executeEPhjS0_jR2GSP9PS2Memoryjjjj",
    (VU1Interpreter *vu, uint8_t *code, uint32_t codeSize, uint8_t *data, uint32_t dataSize, GS &gs,
        PS2Memory *memory, uint32_t pc, uint32_t top, uint32_t itop, uint32_t budget),
    (vu, code, codeSize, data, dataSize, gs, memory, pc, top, itop, budget))
WRAP_METHOD(run, "_ZN14VU1Interpreter3runEPhjS0_jR2GSP9PS2Memoryj",
    (VU1Interpreter *vu, uint8_t *code, uint32_t codeSize, uint8_t *data, uint32_t dataSize, GS &gs,
        PS2Memory *memory, uint32_t budget), (vu, code, codeSize, data, dataSize, gs, memory, budget))
WRAP_METHOD(fmac, "_ZN14VU1Interpreter13applyFmacDestEPfS0_h",
    (VU1Interpreter *vu, float *dest, float *value, uint8_t mask), (vu, dest, value, mask))
WRAP_METHOD(fmacAcc, "_ZN14VU1Interpreter16applyFmacDestAccEPfh",
    (VU1Interpreter *vu, float *value, uint8_t mask), (vu, value, mask))
WRAP_METHOD(fmacNormalize, "_ZN14VU1Interpreter19normalizeFmacResultEPfhPh",
    (VU1Interpreter *vu, float *value, uint8_t mask, uint8_t *flags), (vu, value, mask, flags))
#undef WRAP_METHOD

int main()
{
    MiniTest::Case("NEXO conservative native VU", [](TestCase &tc)
    {
        tc.Run("interpreter trap catches an intentional generic call in the native zone", [](TestCase &t)
        {
            auto memory = std::make_unique<Fixture>();
            auto vu = std::make_unique<VU1Interpreter>();
            bool caught = false;
            {
                NativeScope scope;
                try
                {
                    vu->resume(memory->memory.getVU1Code(), PS2_VU1_CODE_SIZE,
                        memory->memory.getVU1Data(), PS2_VU1_DATA_SIZE, memory->gs, &memory->memory, 0, 0, 1);
                }
                catch (const std::runtime_error &) { caught = true; }
            }
            t.IsTrue(caught, "The link trap is active, rather than a passive counter");
            t.Equals(forbiddenCalls, 1u, "Intentional generic execution is intercepted");
            forbiddenCalls = 0;
        });
        tc.Run("native FMAC issues without losing pending writes and flags", [](TestCase &t)
        { compare(t, 1, false); });
        tc.Run("native FMAC publishes the result at the reference cycle", [](TestCase &t)
        { compare(t, 5, false); });
        tc.Run("native continuation resumes an already pending reference write", [](TestCase &t)
        { compare(t, 5, true); });
        tc.Run("native fresh execution resets pending work while preserving the clock", [](TestCase &t)
        { compare(t, 1, true, true); });
        tc.Run("native Q latency and WAITQ survive repeated suspension", [](TestCase &t)
        {
            const std::array code{divideQ, waitQ}; const std::array budgets{1u, 1u, 5u, 1u, 3u};
            compareLowerProgram(t, code, budgets);
        });
        tc.Run("native EFU throughput and both P results survive repeated suspension", [](TestCase &t)
        {
            const std::array code{esadd, eleng, waitP}; const std::array budgets{1u, 10u, 1u, 18u};
            compareLowerProgram(t, code, budgets);
        });
        tc.Run("native consecutive VI writes preserve branch history and delay", [](TestCase &t)
        {
            const std::array code{increment, increment, branch, lowerNop}; const std::array budgets{1u, 1u, 1u, 1u, 2u};
            compareLowerProgram(t, code, budgets);
        });
        tc.Run("native stores preserve bytes and commit state", [](TestCase &t)
        {
            const std::array code{store}; const std::array budgets{1u, 1u, 4u};
            compareLowerProgram(t, code, budgets);
        });
        tc.Run("native XGKICK preserves in-flight bytes and future data reads", [](TestCase &t)
        {
            const std::array code{kick}; const std::array budgets{1u, 1u, 1u, 2u};
            compareLowerProgram(t, code, budgets, true);
        });
        tc.Run("an unknown native entry fails without fallback and restores host rounding", [](TestCase &t)
        {
            forbiddenCalls = 0;
            auto memory = std::make_unique<Fixture>(); auto vu = std::make_unique<VU1Interpreter>();
            std::vector<VuNativeAccess::Entry> entries(2048);
            const VuNativeProgram program{VU1Interpreter::Unit::VU1, entries,
                {memory->memory.getVU1Code(), PS2_VU1_CODE_SIZE}};
            const int previous = std::fegetround(); std::fesetround(FE_UPWARD);
            bool caught = false;
            {
                NativeScope scope;
                try { VuNativeAccess::resume(*vu, program, memory->memory.getVU1Data(), PS2_VU1_DATA_SIZE,
                    memory->gs, &memory->memory, 0, 0, 1); }
                catch (const std::runtime_error &error) { caught = std::string(error.what()).find("UNSEEN_CODE") != std::string::npos; }
            }
            t.IsTrue(caught, "Unknown code has a deterministic native failure");
            t.Equals(std::fegetround(), FE_UPWARD, "Error restores the host rounding mode");
            std::fesetround(previous);
            t.Equals(forbiddenCalls, 0u, "Unknown target cannot invoke the interpreter");
        });
        tc.Run("an overflowing native PC is rejected before table lookup", [](TestCase &t)
        {
            auto memory = std::make_unique<Fixture>();
            auto vu = std::make_unique<VU1Interpreter>();
            vu->state().pc = 0xFFFFFFF8u;
            std::vector<VuNativeAccess::Entry> entries(2048);
            const VuNativeProgram program{VU1Interpreter::Unit::VU1, entries,
                {memory->memory.getVU1Code(), PS2_VU1_CODE_SIZE}};
            bool caught = false;
            {
                NativeScope scope;
                try { VuNativeAccess::resume(*vu, program, memory->memory.getVU1Data(), PS2_VU1_DATA_SIZE,
                    memory->gs, &memory->memory, 0, 0, 1); }
                catch (const std::runtime_error &error)
                { caught = std::string(error.what()).find("UNSEEN_CODE VU address") != std::string::npos; }
            }
            t.IsTrue(caught, "PC overflow cannot escape the finite compiled table");
        });
        tc.Run("runtime MSCAL executes a native bank after source views are destroyed", [](TestCase &t)
        {
            forbiddenCalls = 0;
            SyntheticBank bank;
            auto reference = std::make_unique<PS2Runtime>(); auto native = std::make_unique<PS2Runtime>();
            prepareRuntime(*reference, bank); prepareRuntime(*native, bank);
            const std::array programs{bank.program()};
            bindNativeVu1(*native, programs);
            std::fill(bank.code.begin(), bank.code.end(), 0);
            // Poison the live storage before clearing it: clear() alone can
            // leave old bytes intact and hide a borrowed-descriptor bug.
            std::fill(bank.descriptors.begin(), bank.descriptors.end(), VuNativeAccess::DecodedPair{});
            std::fill(bank.entries.begin(), bank.entries.end(), VuNativeAccess::Entry{nullptr, nullptr, nullptr});
            bank.entries.clear(); bank.descriptors.clear();
            vifCommand(*reference, 0x14);
            { NativeScope scope; vifCommand(*native, 0x14); }
            compareRuntime(t, *native, *reference);
            t.Equals(native->vu1().state().vf[3][0], 11.0f, "The compiled ADD executes through real VIF callbacks");
        });
        tc.Run("runtime MSCNT selects changed code and retains pending pipeline state", [](TestCase &t)
        {
            forbiddenCalls = 0;
            SyntheticBank first; SyntheticBank second(true);
            auto reference = std::make_unique<PS2Runtime>(); auto native = std::make_unique<PS2Runtime>();
            prepareRuntime(*reference, first); prepareRuntime(*native, first);
            reference->vu1().execute(reference->memory().getVU1Code(), PS2_VU1_CODE_SIZE,
                reference->memory().getVU1Data(), PS2_VU1_DATA_SIZE, reference->gs(), &reference->memory(), 0, 0, 0, 1);
            t.Equals(reference->vu1().state().vf[3][0], 0.0f, "The seed still has a pending FMAC result");
            t.Equals(reference->vu1().state().pc, 8u, "The seed resumes at the delay instruction");
            VuSnapshotCodec::restore(native->vu1(), VuSnapshotCodec::encode(reference->vu1()));
            const std::array programs{first.program(), second.program()}; bindNativeVu1(*native, programs);
            // Publish the write for the reference's decoded-code cache. The
            // native memory deliberately bypasses its generation counter;
            // exact bank identity must still notice the changed bytes.
            std::memcpy(reference->memory().getVU1Code(), second.code.data(), second.code.size());
            reference->memory().markVU1CodeModified();
            std::memcpy(native->memory().getVU1Code(), second.code.data(), second.code.size());
            vifCommand(*reference, 0x17);
            { NativeScope scope; vifCommand(*native, 0x17); }
            compareRuntime(t, *native, *reference);
            t.Equals(native->vu1().state().vf[3][0], -3.0f, "The new bank's SUB replaces the pending ADD in sequence");
            t.Equals(reference->vu1().state().vf[3][0], -3.0f, "The reference also executes the newly published bank");
        });
        tc.Run("runtime native D and T termination updates the CPU-visible status", [](TestCase &t)
        {
            forbiddenCalls = 0;
            SyntheticBank bank(false, true);
            auto reference = std::make_unique<PS2Runtime>(); auto native = std::make_unique<PS2Runtime>();
            prepareRuntime(*reference, bank); prepareRuntime(*native, bank);
            reference->cpu().vu0_fbrst = native->cpu().vu0_fbrst = (1u << 10) | (1u << 11);
            const std::array programs{bank.program()}; bindNativeVu1(*native, programs);
            vifCommand(*reference, 0x15);
            { NativeScope scope; vifCommand(*native, 0x15); }
            compareRuntime(t, *native, *reference);
            t.Equals(native->cpu().vu0_vpu_stat & 0x600u, 0x600u, "Both enabled stop flags become visible");
        });
        tc.Run("runtime changed unknown code fails before modifying VU state", [](TestCase &t)
        {
            forbiddenCalls = 0;
            SyntheticBank bank;
            auto native = std::make_unique<PS2Runtime>(); prepareRuntime(*native, bank);
            const std::array programs{bank.program()}; bindNativeVu1(*native, programs);
            native->memory().getVU1Code()[0] ^= 1;
            const auto before = VuSnapshotCodec::encode(native->vu1());
            bool caught = false;
            {
                NativeScope scope;
                try { vifCommand(*native, 0x14); }
                catch (const std::runtime_error &e) { caught = std::string(e.what()).find("UNSEEN_CODE") != std::string::npos; }
            }
            t.IsTrue(caught, "A missing bank cannot silently use the reference interpreter");
            t.Equals(VuSnapshotCodec::encode(native->vu1()), before, "Failed bank selection precedes VU normalization");
            t.Equals(forbiddenCalls, 0u, "Unknown code is never interpreted");
        });
        tc.Run("runtime binding rejects ambiguous banks without replacing a valid installation", [](TestCase &t)
        {
            forbiddenCalls = 0;
            SyntheticBank bank;
            auto native = std::make_unique<PS2Runtime>(); prepareRuntime(*native, bank);
            const std::array one{bank.program()}; bindNativeVu1(*native, one);
            const std::array duplicate{bank.program(), bank.program()};
            bool caught = false;
            try { bindNativeVu1(*native, duplicate); }
            catch (const std::invalid_argument &) { caught = true; }
            t.IsTrue(caught, "The same code identity cannot select conflicting native contracts");
            { NativeScope scope; vifCommand(*native, 0x14); }
            t.Equals(native->vu1().state().vf[3][0], 11.0f, "Failed installation preserves the previous native callback");
            t.Equals(forbiddenCalls, 0u, "The previous binding remains native");
        });
        tc.Run("canonical replay executes a compiled bank through real VIF callbacks", [](TestCase &t)
        {
            forbiddenCalls = 0;
            SyntheticBank bank; CaseDirectory directory;
            auto reference = std::make_unique<PS2Runtime>(); prepareRuntime(*reference, bank);
            directory.write("input-state.nexo", VuSnapshotCodec::encode(reference->vu1()));
            directory.write("code.bin", bank.code);
            directory.write("input-data.bin", {reference->memory().getVU1Data(), PS2_VU1_DATA_SIZE});
            reference->memory().setGifArbiter(nullptr);
            vifCommand(*reference, 0x17);
            const auto expected = VuSnapshotCodec::encode(reference->vu1());
            directory.write("output-state.nexo", expected);
            directory.write("output-data.bin", {reference->memory().getVU1Data(), PS2_VU1_DATA_SIZE});
            const std::vector<uint8_t> events{'N','E','X','O','G','I','F',0,1,0,0,0,0,0,0,0};
            directory.write("path1-events.nexo", events);
            VuReplayResult result;
            { NativeScope scope; result = replayVuCaptureNativeVif(directory.path, 65536, bank.program()); }
            t.Equals(result.state, expected, "Native runtime replay matches the complete canonical output");
            t.Equals(result.path1Events, events, "Native runtime replay has the expected timed submissions");
            t.IsTrue(std::memcmp(result.data.data(), reference->memory().getVU1Data(), PS2_VU1_DATA_SIZE) == 0,
                "Native runtime replay preserves memory");
            t.Equals(forbiddenCalls, 0u, "The replay adapter cannot execute the interpreter");
        });
        tc.Run("native VIF replay rejects a budget that differs from the runtime horizon", [](TestCase &t)
        {
            SyntheticBank bank;
            bool caught = false;
            try { replayVuCaptureNativeVif("missing-case", 1, bank.program()); }
            catch (const std::invalid_argument &) { caught = true; }
            t.IsTrue(caught, "The fixed callback budget cannot silently substitute for a recorded budget");
        });
        tc.Run("canonical VIF replay preserves captured D and T enables", [](TestCase &t)
        {
            forbiddenCalls = 0;
            SyntheticBank bank(false, true); CaseDirectory directory;
            auto reference = std::make_unique<PS2Runtime>(); prepareRuntime(*reference, bank);
            reference->cpu().vu0_fbrst = (1u << 10) | (1u << 11);
            reference->vu1().state().dBitEnabled = reference->vu1().state().tBitEnabled = true;
            directory.write("input-state.nexo", VuSnapshotCodec::encode(reference->vu1()));
            directory.write("code.bin", bank.code);
            directory.write("input-data.bin", {reference->memory().getVU1Data(), PS2_VU1_DATA_SIZE});
            reference->memory().setGifArbiter(nullptr);
            vifCommand(*reference, 0x17);
            const auto expected = VuSnapshotCodec::encode(reference->vu1());
            const std::vector<uint8_t> events{'N','E','X','O','G','I','F',0,1,0,0,0,0,0,0,0};
            directory.write("output-state.nexo", expected);
            directory.write("output-data.bin", {reference->memory().getVU1Data(), PS2_VU1_DATA_SIZE});
            directory.write("path1-events.nexo", events);
            VuReplayResult result;
            { NativeScope scope; result = replayVuCaptureNativeVif(directory.path, 65536, bank.program()); }
            t.Equals(result.state, expected, "Default host FBRST cannot overwrite captured enable bits");
            t.Equals(forbiddenCalls, 0u, "Captured stop semantics execute natively");
        });
        tc.Run("canonical VIF replay rejects TOP and ITOP outside its callback domain", [](TestCase &t)
        {
            forbiddenCalls = 0;
            SyntheticBank bank; CaseDirectory directory;
            auto reference = std::make_unique<PS2Runtime>(); prepareRuntime(*reference, bank);
            directory.write("code.bin", bank.code);
            directory.write("input-data.bin", {reference->memory().getVU1Data(), PS2_VU1_DATA_SIZE});
            for (const bool invalidTop : {false, true})
            {
                reference->vu1().state().top = invalidTop ? 0x400u : 0u;
                reference->vu1().state().itop = invalidTop ? 0u : 0x400u;
                directory.write("input-state.nexo", VuSnapshotCodec::encode(reference->vu1()));
                bool caught = false;
                {
                    NativeScope scope;
                    try { replayVuCaptureNativeVif(directory.path, 65536, bank.program()); }
                    catch (const std::invalid_argument &e)
                    { caught = std::string(e.what()).find("TOP/ITOP") != std::string::npos; }
                }
                t.IsTrue(caught, "The integration adapter cannot silently mask a captured input");
            }
            t.Equals(forbiddenCalls, 0u, "Rejected boundary inputs never invoke an interpreter");
        });
    });
    return MiniTest::Run();
}

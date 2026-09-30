#include "nexo/vu_runtime_binding.h"
#include "ps2_runtime.h"
#include "runtime/ee_scheduler.h"

#include <algorithm>
#include <atomic>
#include <memory>
#include <stdexcept>

namespace ps2native::nexo
{
namespace
{
struct OwnedBank
{
    std::vector<uint8_t> code;
    std::vector<VuNativeAccess::DecodedPair> descriptors;
    std::vector<VuNativeAccess::Entry> entries;
    VuNativeProgram program;

    explicit OwnedBank(const VuNativeProgram &source)
    {
        if (source.unit != VU1Interpreter::Unit::VU1 || source.codeIdentity.size() != PS2_VU1_CODE_SIZE ||
            source.entries.size() != PS2_VU1_CODE_SIZE / 8)
            throw std::invalid_argument("invalid VU1 native bank binding");
        code.assign(source.codeIdentity.begin(), source.codeIdentity.end());
        descriptors.resize(source.entries.size());
        entries.reserve(source.entries.size());
        for (size_t i = 0; i < source.entries.size(); ++i)
        {
            const auto &entry = source.entries[i];
            if (!entry.decoded && !entry.upper && !entry.lower)
            { entries.push_back({nullptr, nullptr, nullptr}); continue; }
            if (!entry.decoded || !entry.upper || !entry.lower)
                throw std::invalid_argument("partially bound VU1 native entry");
            descriptors[i] = *entry.decoded;
            entries.push_back({&descriptors[i], entry.upper, entry.lower});
        }
        program = {source.unit, entries, code};
    }
};

class BankCollection
{
    std::vector<std::unique_ptr<OwnedBank>> banks;
    mutable std::atomic<size_t> previous{0};

public:
    explicit BankCollection(std::span<const VuNativeProgram> source)
    {
        if (source.empty()) throw std::invalid_argument("native VU1 binding requires compiled banks");
        for (const auto &program : source)
        {
            auto bank = std::make_unique<OwnedBank>(program);
            for (const auto &old : banks)
                if (old->code == bank->code) throw std::invalid_argument("ambiguous duplicate native VU1 identity");
            banks.push_back(std::move(bank));
        }
    }

    const VuNativeProgram &select(const PS2Memory &memory) const
    {
        const auto *code = memory.getVU1Code();
        if (!code) throw std::invalid_argument("native VU1 memory is not initialized");
        const auto matches = [code](const OwnedBank &bank)
        { return std::equal(bank.code.begin(), bank.code.end(), code); };
        const size_t cached = previous.load(std::memory_order_relaxed);
        if (matches(*banks[cached])) return banks[cached]->program;
        for (size_t i = 0; i < banks.size(); ++i)
            if (i != cached && matches(*banks[i]))
            { previous.store(i, std::memory_order_relaxed); return banks[i]->program; }
        throw std::runtime_error("UNSEEN_CODE VU1 runtime bank identity");
    }
};

void invoke(PS2Runtime &runtime, const BankCollection &banks, bool fresh,
    uint32_t startPC, uint32_t top, uint32_t itop)
{
    const auto &program = banks.select(runtime.memory());
    auto *cpu = runtime.eeScheduler().currentContext();
    if (!cpu) cpu = &runtime.cpu();
    auto &vu = runtime.vu1();
    vu.state().dBitEnabled = (cpu->vu0_fbrst & (1u << 10)) != 0;
    vu.state().tBitEnabled = (cpu->vu0_fbrst & (1u << 11)) != 0;
    if (fresh)
        VuNativeAccess::execute(vu, program, runtime.memory().getVU1Data(), PS2_VU1_DATA_SIZE,
            runtime.gs(), &runtime.memory(), startPC, top, itop, 65536);
    else
        VuNativeAccess::resume(vu, program, runtime.memory().getVU1Data(), PS2_VU1_DATA_SIZE,
            runtime.gs(), &runtime.memory(), top, itop, 65536);
    cpu->vu0_vpu_stat = (cpu->vu0_vpu_stat & ~0x0600u) |
        (vu.state().stoppedByD ? 0x0200u : 0u) | (vu.state().stoppedByT ? 0x0400u : 0u);
}
}

void bindNativeVu1(PS2Runtime &runtime, std::span<const VuNativeProgram> programs)
{
    // Construct and validate everything before replacing either callback.
    auto banks = std::make_shared<const BankCollection>(programs);
    if (!runtime.memory().getRDRAM() || !runtime.memory().getVU1Code() ||
        !runtime.memory().getVU1Data() || !runtime.syncCoreSubsystems())
        throw std::invalid_argument("native VU1 binding requires an initialized runtime");
    PS2Memory::Vu1MscalCallback mscal = [&runtime, banks](uint32_t pc, uint32_t top, uint32_t itop)
    { invoke(runtime, *banks, true, pc, top, itop); };
    PS2Memory::Vu1MscntCallback mscnt = [&runtime, banks](uint32_t top, uint32_t itop)
    { invoke(runtime, *banks, false, 0, top, itop); };
    runtime.memory().setVu1MscalCallback(std::move(mscal));
    runtime.memory().setVu1MscntCallback(std::move(mscnt));
}
}

#include "ps2_runtime.h"
#include "ps2_syscalls.h"
#include "runtime/ee_scheduler.h"
#include <cstdlib>
#include <cstring>
#include <iostream>

namespace
{
    void finishThreadProbe(uint8_t *, R5900Context *ctx, PS2Runtime *runtime)
    {
        ctx->pc = 0u;
        runtime->requestStop();
    }
}

int main(int argc, char **argv)
{
    if (argc != 2 && argc != 3) return 2;
    const bool directLookup = argc == 3 && std::strcmp(argv[2], "lookup") == 0;
    const uint32_t alias = argc == 3 && std::strcmp(argv[2], "thread-kseg0") == 0 ? 0x80000000u
        : argc == 3 && std::strcmp(argv[2], "thread-kseg1") == 0 ? 0xA0000000u : 0u;
    const bool threadEntry = alias != 0u || (argc == 3 && std::strcmp(argv[2], "thread") == 0);
    if (argc == 3 && !directLookup && !threadEntry) return 2;
    setenv("PS2X_EE_MISS_CAPTURE_DIR", argv[1], 1);
    unsetenv("PS2X_NATIVE_OVERLAY_DRIVER");
    auto owner = std::make_unique<PS2Runtime>();
    auto &runtime = *owner;
    if (!runtime.memory().initialize()) return 5;
    runtime.setMissingFunctionPolicy(PS2Runtime::MissingFunctionPolicy::Stop);
    auto *ram = runtime.memory().getRDRAM();
    // Two unseen addresses in separate capture windows. The module has no
    // generated entry bindings: recovery must use the guarded offline banks.
    runtime.activateLoadedEeModule("cdrom0:\\synthetic.elf;1",
                                  {{alias + 0x10000u, alias + 0x1000Cu},
                                   {alias + 0x20000u, alias + 0x2000Cu}});
    uint32_t total = 0;
    for (uint32_t index = 0; index < 2; ++index)
    {
        const uint32_t address = (index + 1u) * 0x10000u;
        const uint32_t words[] = {index == 0 ? 0x2402002Au : 0x2402004Fu,
                                  0x03E00008u, 0x24420001u};
        std::memcpy(ram + address, words, sizeof(words));
    }
    for (uint32_t index = 0; index < 2; ++index)
    {
        R5900Context context;
        context.pc = alias + (index + 1u) * 0x10000u;
        if (threadEntry)
        {
            // The controller is owned host test code. The two thread bodies
            // are unseen MIPS and must pass through the production scheduler,
            // capture, offline compilation and guarded native bank lookup.
            constexpr uint32_t controller = 0x30000u;
            if (!runtime.registerCompiledModuleFunctions("owned-controller", {{controller, finishThreadProbe}}) ||
                !runtime.activateLoadedEeModule("owned-controller", {{controller, controller + 4u}})) return 7;
            R5900Context mainContext{};
            mainContext.pc = controller;
            auto &scheduler = runtime.eeScheduler();
            scheduler.reset(ram, mainContext);
            scheduler.bindMainContextForSyscall(mainContext, ram);
            int oldPriority = 0;
            if (scheduler.changePriority(EeScheduler::kMainThreadId, 64, false, oldPriority) != 0) return 7;
            const int tid = scheduler.createThread({0u, context.pc, 0x001E0000u, 0x1000u, 0u, 50, 0u});
            mainContext.r[4] = _mm_set_epi64x(0, tid);
            try { ps2_syscalls::StartThread(ram, &mainContext, &runtime); }
            catch (const EeDispatcherTransfer &) {}
            scheduler.run();
            if (runtime.hasMissingFunctionReport()) return 73;
            const auto *thread = scheduler.thread(tid);
            if (!thread || thread->status != EeThreadStatus::Dormant) return 6;
            total += static_cast<uint32_t>(_mm_extract_epi32(thread->context.r[2], 0));
            continue;
        }
        if (!directLookup && !runtime.hasFunction(context.pc))
        {
            runtime.reportMissingFunction(ram, &context, context.pc, 0,
                                           PS2Runtime::GuestBranchKind::IndirectCall, "synthetic-JALR");
            return runtime.isStopRequested() ? 73 : 3;
        }
        const auto function = runtime.lookupFunction(context.pc);
        function(ram, &context, &runtime);
        if (runtime.hasMissingFunctionReport()) return 73;
        total += static_cast<uint32_t>(_mm_extract_epi32(context.r[2], 0));
    }
    std::cout << "{\"native_result\":" << total << ",\"live_compiler\":false}\n";
    return total == 123u ? 0 : 4;
}

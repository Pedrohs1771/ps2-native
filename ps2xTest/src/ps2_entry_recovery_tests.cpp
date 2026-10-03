#include "MiniTest.h"
#include "ps2_runtime.h"
#include "ps2_runtime_macros.h"
#include "ps2_syscalls.h"
#include "ps2_stubs.h"
#include "runtime/ee_scheduler.h"
#include "Kernel/Stubs/SIF.h"
#include "Stubs/MPEG.h"
#include <array>
#include <memory>
#include <thread>
#include <chrono>
#include <stdexcept>
#include <cstring>

namespace
{
    constexpr uint32_t base = 0x00172000u;
    constexpr uint32_t unknown = 0x00172100u;
    constexpr uint32_t stack = 0x001E0000u;
    constexpr uint32_t argument = 0x12345678u;
    bool resumed = false;
    bool aliased = false;

    void stopParent(uint8_t *, R5900Context *ctx, PS2Runtime *runtime)
    {
        resumed = true;
        ctx->pc = 0u;
        runtime->requestStop();
    }

    void wrongAlias(uint8_t *, R5900Context *ctx, PS2Runtime *)
    {
        aliased = true;
        ctx->pc = 0u;
    }

    void rpcServer(uint8_t *, R5900Context *ctx, PS2Runtime *)
    {
        SET_GPR_U32(ctx, 2, 0x00130400u);
        ctx->pc = 0u;
    }

    struct EntryEnv
    {
        std::unique_ptr<PS2Runtime> owner = std::make_unique<PS2Runtime>();
        PS2Runtime &runtime = *owner;
        uint8_t *ram = nullptr;
        R5900Context context{};

        explicit EntryEnv(PS2Runtime::MissingFunctionPolicy policy = PS2Runtime::MissingFunctionPolicy::Stop)
        {
            if (!runtime.memory().initialize()) throw std::runtime_error("test RAM initialization failed");
            ram = runtime.memory().getRDRAM();
            runtime.setMissingFunctionPolicy(policy);
            runtime.activateLoadedEeModule("owned-unbound-entry", {{unknown, unknown + 8u}});
            Ps2FastWrite32(ram, unknown, 0x03E00008u); // Owned JR RA + NOP.
            Ps2FastWrite32(ram, unknown + 4u, 0u);
            runtime.registerFunction(base, stopParent);
            if (!runtime.hasFunction(base)) throw std::runtime_error("known parent binding unavailable");
            context.pc = base;
            SET_GPR_U32(&context, 29, stack);
            runtime.eeScheduler().reset(ram, context);
            runtime.eeScheduler().bindMainContextForSyscall(context, ram);
            resumed = false;
            aliased = false;
        }

        ~EntryEnv() { runtime.requestStop(); }

        template<typename F> void syscall(F action)
        {
            try { action(); }
            catch (const EeDispatcherTransfer &) {}
        }

        void run()
        {
            // Missing entry must stop promptly; a refused exit handler can
            // otherwise leave an idle scheduler with no runnable thread.
            std::jthread deadline([&](std::stop_token token)
            {
                for (int i = 0; i < 100 && !token.stop_requested(); ++i)
                    std::this_thread::sleep_for(std::chrono::milliseconds(2));
                if (!token.stop_requested()) runtime.requestStop();
            });
            runtime.eeScheduler().run();
            deadline.request_stop();
        }

        void expectMissing(TestCase &t)
        {
            t.IsTrue(runtime.hasMissingFunctionReport(), "unbound guest entry must enter strict recovery reporting");
            t.IsFalse(resumed, "parent must not continue after an unexecuted guest entry");
            t.IsFalse(aliased, "unknown address must not execute a different compiled address");
        }
    };

    void prepareRpc(EntryEnv &env, uint32_t serverFunction)
    {
        constexpr uint32_t queue = 0x00130000u;
        constexpr uint32_t server = 0x00130100u;
        constexpr uint32_t client = 0x00130200u;
        constexpr uint32_t sid = 0x20007A11u;
        auto &ctx = env.context;
        ps2_syscalls::SifInitRpc(env.ram, &ctx, &env.runtime);
        SET_GPR_U32(&ctx, 4, queue); SET_GPR_U32(&ctx, 5, 1u);
        ps2_syscalls::SifSetRpcQueue(env.ram, &ctx, &env.runtime);
        Ps2FastWrite32(env.ram, stack + 0x10u, 0u);
        Ps2FastWrite32(env.ram, stack + 0x14u, 0u);
        Ps2FastWrite32(env.ram, stack + 0x18u, queue);
        SET_GPR_U32(&ctx, 4, server); SET_GPR_U32(&ctx, 5, sid);
        SET_GPR_U32(&ctx, 6, serverFunction); SET_GPR_U32(&ctx, 7, 0x00130400u);
        ps2_syscalls::SifRegisterRpc(env.ram, &ctx, &env.runtime);
        SET_GPR_U32(&ctx, 4, client); SET_GPR_U32(&ctx, 5, sid); SET_GPR_U32(&ctx, 6, 0u);
        ps2_syscalls::SifBindRpc(env.ram, &ctx, &env.runtime);
    }

    void callRpc(EntryEnv &env, uint32_t callback)
    {
        auto &ctx = env.context;
        SET_GPR_U32(&ctx, 4, 0x00130200u); SET_GPR_U32(&ctx, 5, 0x77u);
        SET_GPR_U32(&ctx, 6, 0u); SET_GPR_U32(&ctx, 7, 0x00130500u);
        SET_GPR_U32(&ctx, 8, 4u); SET_GPR_U32(&ctx, 9, 0x00130600u);
        SET_GPR_U32(&ctx, 10, 4u); SET_GPR_U32(&ctx, 11, callback);
        SET_GPR_U32(&ctx, 12, argument);
        env.syscall([&] { ps2_syscalls::SifCallRpc(env.ram, &ctx, &env.runtime); });
    }
}

void register_ps2_entry_recovery_tests()
{
    MiniTest::Case("Guest entry recovery admission", [](TestCase &tc)
    {
        tc.Run("StartThread delivers unbound entry and its argument", [](TestCase &t)
        {
            EntryEnv env;
            auto &scheduler = env.runtime.eeScheduler();
            int oldPriority = 0;
            t.Equals(scheduler.changePriority(EeScheduler::kMainThreadId, 64, false, oldPriority), 0,
                     "parent priority must allow the child to run first");
            int id = scheduler.createThread({0u, unknown, stack - 0x2000u, 0x1000u, 0u, 50, 0u});
            SET_GPR_U32(&env.context, 4, id); SET_GPR_U32(&env.context, 5, argument);
            env.syscall([&] { ps2_syscalls::StartThread(env.ram, &env.context, &env.runtime); });
            t.IsTrue(scheduler.thread(id)->status != EeThreadStatus::Dormant,
                     "missing native binding must not reject a valid dormant thread");
            env.run(); env.expectMissing(t);
            const auto &active = scheduler.thread(id)->activeContext();
            t.Equals(active.pc, unknown, "recovery must observe the real thread entry");
            t.Equals(getRegU32(&active, 4), argument, "recovery must retain thread argument");
        });

        tc.Run("SetAlarm delivers unbound callback", [](TestCase &t)
        {
            EntryEnv env;
            auto &scheduler = env.runtime.eeScheduler();
            int id = scheduler.setAlarm(1u, unknown, argument, 0u, stack);
            t.IsTrue(id > 0, "alarm admission must not depend on native coverage");
            if (id > 0) scheduler.postEvent({EeEventType::Alarm, static_cast<uint32_t>(id), 0u});
            env.run(); env.expectMissing(t);
        });

        tc.Run("SIF command delivers unbound registered handler", [](TestCase &t)
        {
            EntryEnv env;
            SET_GPR_U32(&env.context, 4, 0x00120000u); SET_GPR_U32(&env.context, 5, 1u);
            ps2_stubs::sceSifSetCmdBuffer(env.ram, &env.context, &env.runtime);
            SET_GPR_U32(&env.context, 4, 0u); SET_GPR_U32(&env.context, 5, unknown);
            SET_GPR_U32(&env.context, 6, argument);
            ps2_stubs::sceSifAddCmdHandler(env.ram, &env.context, &env.runtime);
            const std::array<uint32_t, 4> packet{16u, 0u, 0u, 0u};
            t.IsTrue(ps2_stubs::dispatchSifCommand(env.ram, &env.runtime, 0u, packet.data(), sizeof(packet)),
                     "registered handler must reach scheduler even without compiled binding");
            env.run(); env.expectMissing(t);
        });

        tc.Run("ExitThread retains unbound exit handler", [](TestCase &t)
        {
            EntryEnv env;
            env.runtime.addEeExitHandler(1, unknown, argument);
            env.syscall([&] { ps2_syscalls::ExitThread(env.ram, &env.context, &env.runtime); });
            env.run(); env.expectMissing(t);
        });

        tc.Run("Syscall override delivers unbound registered handler", [](TestCase &t)
        {
            EntryEnv env;
            env.runtime.setEeSyscallOverride(env.ram, 0x83u, unknown);
            env.syscall([&] { env.runtime.handleSyscall(env.ram, &env.context, 0x83u); });
            env.run(); env.expectMissing(t);
        });

        tc.Run("MPEG stream delivers unbound callback with guest data", [](TestCase &t)
        {
            EntryEnv env;
            ps2_stubs::resetMpegStubState(); ps2_stubs::notifyMpegCdStreamStart();
            SET_GPR_U32(&env.context, 4, 0x00140000u); SET_GPR_U32(&env.context, 5, 0u);
            SET_GPR_U32(&env.context, 6, 0u); SET_GPR_U32(&env.context, 7, unknown);
            SET_GPR_U32(&env.context, 8, argument);
            ps2_stubs::sceMpegAddStrCallback(env.ram, &env.context, &env.runtime);
            const std::array<uint8_t, 17> pes{0u,0u,1u,0xE0u,0u,11u,0x80u,0u,0u,0u,0u,1u,0xB3u,0x14u,0u,0xF0u,0x13u};
            std::memcpy(env.ram + 0x00150000u, pes.data(), pes.size());
            SET_GPR_U32(&env.context, 4, 0x00140000u); SET_GPR_U32(&env.context, 5, 0x00150000u);
            SET_GPR_U32(&env.context, 6, pes.size());
            ps2_stubs::sceMpegDemuxPss(env.ram, &env.context, &env.runtime);
            env.run(); env.expectMissing(t);
            ps2_stubs::resetMpegStubState();
        });

        tc.Run("RPC server delivers unbound guest function before completion", [](TestCase &t)
        {
            EntryEnv env;
            prepareRpc(env, unknown);
            callRpc(env, 0u);
            env.run(); env.expectMissing(t);
        });

        tc.Run("RPC callback preserves exact unbound address despite compiled alias", [](TestCase &t)
        {
            EntryEnv env;
            env.runtime.registerFunction(unknown - 0x10000u, wrongAlias);
            env.runtime.registerFunction(base + 0x40u, rpcServer);
            prepareRpc(env, base + 0x40u);
            callRpc(env, unknown);
            env.run(); env.expectMissing(t);
        });

        tc.Run("diagnostic policy still refuses unbound thread and zero alarm", [](TestCase &t)
        {
            EntryEnv env(PS2Runtime::MissingFunctionPolicy::ContinueToTarget);
            auto &scheduler = env.runtime.eeScheduler();
            int id = scheduler.createThread({0u, unknown, stack - 0x2000u, 0x1000u, 0u, 50, 0u});
            SET_GPR_U32(&env.context, 4, id); SET_GPR_U32(&env.context, 5, argument);
            env.syscall([&] { ps2_syscalls::StartThread(env.ram, &env.context, &env.runtime); });
            t.IsTrue(static_cast<int32_t>(getRegU32(&env.context, 2)) < 0, "legacy diagnostic mode must keep rejection");
            t.Equals(scheduler.thread(id)->status, EeThreadStatus::Dormant, "refused thread remains dormant");
            t.Equals(scheduler.setAlarm(1u, 0u, argument, 0u, stack), -1, "zero handler is still invalid");
            t.IsFalse(env.runtime.hasMissingFunctionReport(), "mere registration does not execute a target");
        });
    });
}

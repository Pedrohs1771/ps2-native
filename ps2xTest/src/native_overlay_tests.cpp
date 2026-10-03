#include "MiniTest.h"
#include "ps2recomp/native_overlay.h"
#include "ps2_native_overlay.h"
#include "ps2_ee_aot.h"
#include "ps2_runtime_macros.h"
#include <cstdlib>
#include <cstring>
#include <vector>

namespace
{
    std::vector<uint8_t> words(std::initializer_list<uint32_t> values)
    {
        std::vector<uint8_t> bytes(values.size() * 4u);
        size_t offset = 0;
        for (uint32_t value : values)
        {
            std::memcpy(bytes.data() + offset, &value, 4);
            offset += 4;
        }
        return bytes;
    }

    void physicalBody(uint8_t *, R5900Context *, PS2Runtime *) {}
    void cachedBody(uint8_t *, R5900Context *, PS2Runtime *) {}
    void uncachedBody(uint8_t *, R5900Context *, PS2Runtime *) {}
}

void register_native_overlay_tests()
{
    MiniTest::Case("Native overlay", [](TestCase &suite)
    {
    suite.Run("kernel_alias_banks_keep_distinct_PC_bindings_and_guard_physical_writes", [](TestCase &tc)
    {
        using namespace ps2native::ee_aot;
        const auto image = words({0x2402002Au,0x03E00008u,0u});
        const std::array<PS2NativeOverlayBinding,3> bindings{{
            {0x10000u,physicalBody,0x10000u,12u,image.data()},
            {0x80010000u,cachedBody,0x80010000u,12u,image.data()},
            {0xA0010000u,uncachedBody,0xA0010000u,12u,image.data()}}};
        const std::array<Bank,3> banks{{
            {0x10000u,image,std::span(bindings.data(),1)},
            {0x80010000u,image,std::span(bindings.data()+1,1)},
            {0xA0010000u,image,std::span(bindings.data()+2,1)}}};
        Dispatcher dispatcher(Program{banks});
        std::vector<uint8_t> ram(PS2_RAM_SIZE,0u);
        std::memcpy(ram.data()+0x10000u,image.data(),image.size());
        tc.IsTrue(dispatcher.lookup(ram.data(),0x10000u).function==physicalBody,"physical PC selects its own body");
        tc.IsTrue(dispatcher.lookup(ram.data(),0x80010000u).function==cachedBody,"KSEG0 PC selects its own body");
        tc.IsTrue(dispatcher.lookup(ram.data(),0xA0010000u).function==uncachedBody,"KSEG1 PC selects its own body");
        tc.Equals(dispatcher.lookup(ram.data(),0x80010004u).status,Status::MissingEntry,"alias must not invent an interior binding");
        for (uint32_t bad : {0xC0010000u,0x82010000u,0x9FC00000u,0x70000000u})
            tc.Equals(dispatcher.lookup(ram.data(),bad).status,Status::OutsideRam,"other segments do not wrap to RDRAM");
        ram[0x10000u]^=1u;
        for (uint32_t pc : {0x10000u,0x80010000u,0xA0010000u})
        {
            tc.Equals(dispatcher.lookup(ram.data(),pc).status,Status::CodeChanged,"physical write invalidates every compiled alias");
            const auto diagnosis=dispatcher.diagnose(ram.data(),pc);
            tc.Equals(diagnosis.candidates.size(),size_t{1},"diagnosis selects the virtual PC case");
            if (!diagnosis.candidates.empty()) tc.Equals(diagnosis.candidates[0].mismatchBytes,1u,"diagnosis reads physical bytes safely");
        }
    });
    suite.Run("native_overlay_emits_real_code_and_all_resume_entries", [](TestCase &tc)
    {
        const auto bytes = words({0x2402002a, 0x03e00008, 0x24420001});
        const auto code = ps2recomp::generateNativeOverlay(bytes, 0xf00000, 0xf00000);
        tc.IsTrue(code.find("PS2NativeOverlayBinding") != std::string::npos, "Exports native bindings");
        tc.IsTrue(code.find("0xf00004u") != std::string::npos, "Registers interior branch entry");
        tc.IsTrue(code.find("0xf00008u") != std::string::npos, "Registers independent delay entry");
        tc.IsTrue(code.find("in_delay_slot = true") != std::string::npos, "Reuses delay-slot semantics");
    });
    suite.Run("native_overlay_rejects_unaligned_and_truncated_snapshots", [](TestCase &tc)
    {
        const auto bytes = words({0x03e00008});
        bool rejected = false;
        try { (void)ps2recomp::generateNativeOverlay(bytes, 0xf00000, 0xf00002); }
        catch (const std::exception &) { rejected = true; }
        tc.IsTrue(rejected, "Unaligned entry is rejected");
        rejected = false;
        try { (void)ps2recomp::generateNativeOverlay(bytes, 0xf00000, 0xf00000); }
        catch (const std::exception &) { rejected = true; }
        tc.IsTrue(rejected, "Missing delay slot must not become a synthetic NOP");
    });
    suite.Run("normal_overlay_entry_guards_exclude_unreachable_prefixes", [](TestCase &tc)
    {
        const auto bytes = words({0u,0u,0x2402002au,0x03e00008u,0x24420001u});
        const auto code = ps2recomp::generateNativeOverlay(bytes,0x10000u,0x10000u);
        tc.IsTrue(code.find("{0x10008u,ps2native_block_10000,0x10008u,0xcu,snapshot+0x8u}") != std::string::npos,
                  "Normal interior entry guards its suffix without the skipped prefix");
        tc.IsTrue(code.find("{0x10010u,ps2native_block_10000,0x10010u,0x4u,snapshot+0x10u}") != std::string::npos,
                  "Standalone delay address depends on its own instruction only");
    });
    suite.Run("normal_overlay_entry_guards_retain_reachable_loop_prefixes", [](TestCase &tc)
    {
        // BGTZ t0 branches from 0x10008 back to 0x10004.
        const auto bytes = words({0u,0x2508ffffu,0x1d00fffeu,0u});
        const auto code = ps2recomp::generateNativeOverlay(bytes,0x10000u,0x10000u);
        tc.IsTrue(code.find("{0x10008u,ps2native_block_10000,0x10004u,0xcu,snapshot+0x4u}") != std::string::npos,
                  "A local backedge includes instructions before the entry that can execute later");
        tc.IsTrue(code.find("{0x1000cu,ps2native_block_10000,0x1000cu,0x4u,snapshot+0xcu}") != std::string::npos,
                  "Independent slot entry does not take its preceding branch");
    });
    suite.Run("native_overlay_executes_and_invalidates_replaced_code", [](TestCase &tc)
    {
        // Integration is opt-in because it requires the host C++ compiler.
        #if defined(__linux__) && !defined(__ANDROID__)
        const char *driver = std::getenv("PS2X_TEST_NATIVE_OVERLAY_DRIVER");
        if (!driver) return;
        struct DriverScope
        {
            bool existed = false;
            std::string previous;
            explicit DriverScope(const char *path)
            {
                if (const char *old = std::getenv("PS2X_NATIVE_OVERLAY_DRIVER")) { existed = true; previous = old; }
                setenv("PS2X_NATIVE_OVERLAY_DRIVER", path, 1);
            }
            ~DriverScope()
            {
                if (existed) setenv("PS2X_NATIVE_OVERLAY_DRIVER", previous.c_str(), 1);
                else unsetenv("PS2X_NATIVE_OVERLAY_DRIVER");
            }
        } scope(driver);
        PS2Runtime runtime;
        tc.IsTrue(runtime.memory().initialize(), "Allocates EE RAM");
        uint8_t *ram = runtime.memory().getRDRAM();
        const auto bytes = words({0x2402002a, 0x03e00008, 0x24420001});
        std::memcpy(ram + 0xf00000, bytes.data(), bytes.size());
        R5900Context ctx{};
        ctx.pc = 0xf00000;
        SET_GPR_U32(&ctx, 31, 0x100000);
        const bool ready = runtime.hasFunction(ctx.pc);
        tc.IsTrue(ready, "Compiles a missing native target automatically");
        if (!ready) return;
        runtime.lookupFunction(ctx.pc)(ram, &ctx, &runtime);
        tc.Equals(GPR_U32((&ctx), 2), 43u, "Native arithmetic plus return delay slot");
        tc.Equals(ctx.pc, 0x100000u, "Returns to the AOT caller");
        uint32_t replacement = 0x24020064;
        std::memcpy(ram + 0xf00000, &replacement, 4);
        ctx.pc = 0xf00000;
        runtime.lookupFunction(ctx.pc)(ram, &ctx, &runtime);
        tc.Equals(GPR_U32((&ctx), 2), 101u, "Recompiles overwritten guest code, avoiding stale native code");
        ctx.pc = 0xf00008;
        runtime.lookupFunction(ctx.pc)(ram, &ctx, &runtime);
        tc.Equals(GPR_U32((&ctx), 2), 102u, "Delay slot can execute independently");
        tc.Equals(ctx.pc, 0xf0000cu, "Independent slot advances without repeating JR");
        const auto boundaryBytes = words({0x2402002a, 0u, 0u, 0u, 0u, 0u, 0x03e00008, 0x24420001});
        std::memcpy(ram + 0xf0ffe4u, boundaryBytes.data(), boundaryBytes.size());
        ctx.pc = 0xf0ffe4u;
        SET_GPR_U32(&ctx, 31, 0x100000u);
        const bool boundaryReady = runtime.hasFunction(ctx.pc);
        tc.IsTrue(boundaryReady, "Compiles a branch whose delay slot crosses the snapshot boundary");
        if (boundaryReady)
        {
            runtime.lookupFunction(ctx.pc)(ram, &ctx, &runtime);
            tc.Equals(GPR_U32((&ctx), 2), 43u, "Executes the delay slot beyond the original 64 KiB window");
        }
        const auto invalidBytes = words({0x2402002au, 0u, 0u, 0u, 0u, 0x03e00008u, 0x03e00008u});
        std::memcpy(ram + 0xf20000u, invalidBytes.data(), invalidBytes.size());
        tc.IsTrue(!runtime.hasFunction(0xf20000u), "Rejects a branch inside the root delay slot");
        const uint32_t nop = 0u;
        std::memcpy(ram + 0xf20018u, &nop, 4u);
        tc.IsTrue(runtime.hasFunction(0xf20000u), "Retries a failed overlay after a change beyond its first 16 bytes");
#endif
    });
    });
}

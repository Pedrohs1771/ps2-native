#include "MiniTest.h"
#include "ps2_runtime.h"
#include "ps2_iop_host.h"
#include "emulator/core/iop_memory.h"
#include "../../ps2xIOP/tests/iop_compat_test_support.h"

#include <algorithm>
#include <array>

namespace
{
    using ps2x::iop::detail::IopMemory;

    void setupMixer(IopMemory &memory, uint32_t core)
    {
        const uint32_t base = 0xBF900760u + core * 40u;
        memory.write32(base, 0x3FFF3FFFu);
        memory.write32(base + 12u, 0x7FFF7FFFu);
    }

    void sendDma(IopMemory &memory, uint32_t core, uint32_t base, uint32_t bytes)
    {
        const uint32_t dma = 0xBF8010C0u + core * 1088u;
        memory.write16(0xBF9001B0u + core * 0x400u, static_cast<uint16_t>(1u << core));
        memory.write32(dma, base);
        memory.write32(dma + 4u, ((bytes / 64u) << 16u) | 16u);
        memory.write32(dma + 8u, 0x01000201u);
    }

    std::array<int16_t, 512> tile(int16_t left, int16_t right)
    {
        std::array<int16_t, 512> data{};
        std::fill(data.begin(), data.begin() + 256, left);
        std::fill(data.begin() + 256, data.end(), right);
        return data;
    }
}

void register_ps2_spu2_bus_tests()
{
    MiniTest::Case("SPU2 physical IOP bus", [](TestCase &tc)
    {
        tc.Run("MMIO volume writes reach both core mixers with byte and halfword semantics", [](TestCase &t)
        {
            PS2Runtime runtime;
            PS2IopHostAdapter host(runtime);
            IopMemory memory(&host);
            setupMixer(memory, 0u);
            setupMixer(memory, 1u);
            memory.write8(0xBF900760u, 0x34u);
            memory.write8(0xBF900761u, 0x12u);
            memory.write16(0xBF90078Au, 0x2345u);
            t.Equals(runtime.audioBackend().getSpu2Param(0x0980u), 0x1234u, "byte writes must preserve the other halfword");
            t.Equals(runtime.audioBackend().getSpu2Param(0x0A80u), 0x3FFFu, "core 0 right volume must remain independent");
            t.Equals(runtime.audioBackend().getSpu2Param(0x0A81u), 0x2345u, "core 1 master registers use a 40-byte stride");
            t.Equals(runtime.audioBackend().getSpu2Param(0x0F81u), 0x7FFFu, "core 1 B input volume must reach its mixer");
        });

        tc.Run("successive hardware DMA blocks preserve PCM order and copy the IOP source", [](TestCase &t)
        {
            PS2Runtime runtime;
            PS2IopHostAdapter host(runtime);
            IopMemory memory(&host);
            setupMixer(memory, 0u);
            auto first = tile(1200, -1500), second = tile(2400, -2700);
            t.IsTrue(memory.writeRam(0x10000u, first.data(), sizeof(first)), "first IOP source must be valid");
            t.IsTrue(memory.writeRam(0x11000u, second.data(), sizeof(second)), "second IOP source must be valid");
            sendDma(memory, 0u, 0x10000u, sizeof(first));
            sendDma(memory, 0u, 0x11000u, sizeof(second));
            t.IsTrue(memory.zeroRam(0x10000u, sizeof(first)), "first producer buffer should be reusable");
            t.IsTrue(memory.zeroRam(0x11000u, sizeof(second)), "second producer buffer should be reusable");
            std::array<int16_t, 1024> output{};
            runtime.audioBackend().renderBlockAudio(0u, output.data(), 512u);
            for (uint32_t i = 0u; i < 512u; ++i)
            {
                t.Equals(output[2u*i], static_cast<int16_t>(i < 256u ? 1200 : 2400), "left samples must preserve submission order");
                t.Equals(output[2u*i+1u], static_cast<int16_t>(i < 256u ? -1500 : -2700), "right samples must preserve submission order");
            }
            runtime.audioBackend().renderBlockAudio(0u, output.data(), 512u);
            t.IsTrue(std::all_of(output.begin(), output.end(), [](int16_t v) { return v == 0; }), "a depleted DMA queue must not repeat old PCM");
            sendDma(memory, 0u, 0x00200000u, sizeof(first));
            runtime.audioBackend().renderBlockAudio(0u, output.data(), 512u);
            t.IsTrue(std::all_of(output.begin(), output.end(), [](int16_t v) { return v == 0; }), "out-of-range DMA must not invent audio");
        });

        tc.Run("partial DMA planes assemble without overread and reset stops both cores", [](TestCase &t)
        {
            PS2Runtime runtime;
            PS2IopHostAdapter host(runtime);
            IopMemory memory(&host);
            setupMixer(memory, 1u);
            auto input = tile(4567, -5678);
            t.IsTrue(memory.writeRam(0x18000u, input.data(), sizeof(input)), "split-plane IOP source must be valid");
            sendDma(memory, 1u, 0x18000u, 512u);
            std::array<int16_t, 512> output{};
            runtime.audioBackend().renderBlockAudio(1u, output.data(), 256u);
            t.Equals(output[0], static_cast<int16_t>(0), "an incomplete right plane must not be read");
            sendDma(memory, 1u, 0x18200u, 512u);
            runtime.audioBackend().renderBlockAudio(1u, output.data(), 256u);
            t.Equals(output[0], static_cast<int16_t>(4567), "two half-tiles must assemble left PCM");
            t.Equals(output[1], static_cast<int16_t>(-5678), "two half-tiles must assemble right PCM");
            sendDma(memory, 1u, 0x18000u, sizeof(input));
            memory.reset();
            runtime.audioBackend().renderBlockAudio(1u, output.data(), 256u);
            t.IsTrue(std::all_of(output.begin(), output.end(), [](int16_t v) { return v == 0; }), "IOP reset must discard pending PCM");
            t.Equals(runtime.audioBackend().getSpu2Param(0x0981u), 0u, "reset must clear the old hardware mixer parameters");
            setupMixer(memory, 1u);
            t.IsTrue(memory.writeRam(0x18000u, input.data(), sizeof(input)), "reset IOP RAM must accept a new producer");
            sendDma(memory, 1u, 0x18000u, sizeof(input));
            runtime.audioBackend().renderBlockAudio(1u, output.data(), 256u);
            t.Equals(output[0], static_cast<int16_t>(4567), "reset must discard the previous queue cursor");
        });

        tc.Run("an executed IOP ELF can drive audio without the HLE LIBSD RPC", [](TestCase &t)
        {
            PS2Runtime runtime;
            t.IsTrue(runtime.memory().initialize(), "EE memory must initialize for the loader");
            t.IsTrue(runtime.syncCoreSubsystems(), "IOP loader must bind to EE memory");
            iop_test::Irx image(0x10000u, 0x800u);
            image.words(0u, {
                0x3C08BF90u, 0x34093FFFu, // t0 = SPU2 registers, t1 = master gain
                0xA5090760u, 0xA5090762u,
                0x34097FFFu, 0xA509076Cu, 0xA509076Eu, // B input gains
                0x34090001u, 0xA50901B0u, // core 0 AutoDMA
                0x3C0A0001u, 0x354A0200u, // t2 = IOP source at 0x10200
                0x3C08BF80u, 0xAD0A10C0u, // MADR
                0x3C090010u, 0x35290010u, 0xAD0910C4u, // BCR: 16*16 words
                0x3C090100u, 0x35290201u, 0xAD0910C8u, // start IOP -> SPU
                0x03E00008u, 0x24020000u,
            });
            const auto input = tile(6789, -7890);
            std::memcpy(image.bytes.data() + 0x300u, input.data(), sizeof(input));
            std::memcpy(runtime.memory().getRDRAM() + 0x4000u, image.bytes.data(), image.bytes.size());
            const auto loaded = runtime.loadIopModuleBuffer(0x4000u);
            t.IsTrue(loaded.moduleId > 0, "synthetic sound-producing IOP ELF must execute");
            t.IsTrue(runtime.iopDebugSnapshot().emulatorInstructions >= 21u, "IOP instructions must actually run");
            std::array<int16_t, 512> output{};
            runtime.audioBackend().renderBlockAudio(0u, output.data(), 256u);
            t.Equals(output[0], static_cast<int16_t>(6789), "executed SH/SW and DMA must produce the left plane");
            t.Equals(output[1], static_cast<int16_t>(-7890), "executed SH/SW and DMA must produce the right plane");
        });
    });
}

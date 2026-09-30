#include "nexo/vu_replay.h"
#include "nexo/vu_runtime_binding.h"
#include "ps2_runtime.h"
#include "vu_replay_internal.h"
#include <array>
#include <memory>
#include <stdexcept>

namespace ps2native::nexo
{
VuReplayResult replayVuCaptureNativeVif(const std::filesystem::path &directory, uint32_t budget,
    const VuNativeProgram &program)
{
    if (budget != 65536)
        throw std::invalid_argument("native VIF runtime replay requires its exact 65536-cycle horizon");
    auto runtime = std::make_unique<PS2Runtime>();
    if (!runtime->memory().initialize() || !runtime->syncCoreSubsystems())
        throw std::runtime_error("cannot initialize headless native VIF replay");
    const std::array programs{program};
    bindNativeVu1(*runtime, programs);
    // The capture does not contain GIF arbitration or GS/VRAM state. Observe
    // submissions in the same empty receiver as the standalone VU replay.
    runtime->memory().setGifArbiter(nullptr);
    return detail::replayOnMachine(directory, budget, runtime->vu1(), runtime->memory(), runtime->gs(),
        [&runtime](VU1Interpreter &vu, PS2Memory &memory, GS &, uint32_t)
        {
            if (vu.state().top > 0x3FFu || vu.state().itop > 0x3FFu)
                throw std::invalid_argument("captured TOP/ITOP exceed the VIF callback domain");
            // These enable bits are already part of the canonical VU boundary.
            // Reconstruct their FBRST inputs before the runtime propagates them.
            runtime->cpu().vu0_fbrst =
                (vu.state().dBitEnabled ? (1u << 10) : 0u) |
                (vu.state().tBitEnabled ? (1u << 11) : 0u);
            memory.vif1_regs.tops = vu.state().top;
            memory.vif1_regs.itops = vu.state().itop;
            // A normalized VU checkpoint resumes through MSCNT. This command
            // is synthesized for integration, not the original VIF stream.
            const std::array<uint8_t, 4> mscnt{0, 0, 0, 0x17};
            memory.processVIF1Data(mscnt.data(), mscnt.size());
        }, &program);
}
}

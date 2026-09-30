#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

namespace ps2native::nexo
{
struct VuNativeProgram;
struct VuReplayResult
{
    std::vector<uint8_t> state;
    std::vector<uint8_t> data;
    std::vector<uint8_t> path1Events;
    uint64_t executionNanoseconds = 0;
};

// Replay only the VU boundary of a canonical capture, without a window.
// No VIF, GS or GIF-arbiter state is restored. PATH1 packets are observed with
// an otherwise empty receiver. This is a current-runtime regression baseline,
// not a hardware oracle or a native VU AOT implementation.
// Throws std::invalid_argument for malformed input or an invalid budget.
VuReplayResult replayVuCapture(const std::filesystem::path &directory, uint32_t cycleBudget);
VuReplayResult replayVuCaptureNative(const std::filesystem::path &directory, uint32_t cycleBudget,
    const VuNativeProgram &program);
// Drives the normalized VU capture through a synthesized VIF MSCNT command and
// strict native PS2Runtime callbacks. Original VIF/GIF/GS state is not restored.
// The current runtime callback horizon is exactly 65,536 VU cycles.
VuReplayResult replayVuCaptureNativeVif(const std::filesystem::path &directory, uint32_t cycleBudget,
    const VuNativeProgram &program);
}

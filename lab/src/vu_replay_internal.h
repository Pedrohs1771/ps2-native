#pragma once

#include "nexo/vu_replay.h"
#include <functional>

class VU1Interpreter;
class PS2Memory;
class GS;

namespace ps2native::nexo::detail
{
using ReplayExecutor = std::function<void(VU1Interpreter &, PS2Memory &, GS &, uint32_t)>;

// Shared bounded input, canonical restore and timed submission observation.
// Caller supplies initialized, windowless machine components. The input
// scope remains the normalized VU boundary, regardless of executor choice.
VuReplayResult replayOnMachine(const std::filesystem::path &directory, uint32_t budget,
    VU1Interpreter &vu, PS2Memory &memory, GS &gs, const ReplayExecutor &executor,
    const VuNativeProgram *identity);
}

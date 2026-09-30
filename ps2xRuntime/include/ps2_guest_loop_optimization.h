#pragma once

#include <cstdint>

struct R5900Context;
class PS2Runtime;

bool ps2xFastForwardGuestCountdownLoop(PS2Runtime *runtime,
                                       R5900Context *ctx,
                                       uint32_t counterReg,
                                       uint32_t sentinelReg,
                                       uint32_t loopPc,
                                       uint32_t fallthroughPc) noexcept;

#pragma once
#include "Common.h"
// Only fields used by the pinned VU instruction core. This is not a replacement
// VIF parser or a claim that the emulator's VIF device has been reproduced.
struct VIFregisters
{
    uint32_t top=0,itop=0;
    struct { bool VEW=false,VGW=false; } stat;
};
extern VIFregisters vif0Regs,vif1Regs;

#pragma once
#include "ps2_runtime.h"

// Optional desktop development path: compile live guest code to a native DSO.
// Disabled unless the user explicitly configures PS2X_NATIVE_OVERLAY_DRIVER.
PS2Runtime::RecompiledFunction ps2xResolveNativeOverlay(PS2Runtime *, uint8_t *, uint32_t);
void ps2xReleaseNativeOverlays(PS2Runtime *);

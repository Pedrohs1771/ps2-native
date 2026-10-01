#pragma once
#include "ps2_runtime.h"

// Backend selection is fixed at build time. AOT uses a finite offline catalog;
// the diagnostic backend needs the opt-in PS2X_NATIVE_OVERLAY_DRIVER.
PS2Runtime::RecompiledFunction ps2xResolveNativeOverlay(PS2Runtime *, uint8_t *, uint32_t);
void ps2xReleaseNativeOverlays(PS2Runtime *);
bool ps2xUsesAotEeOverlays();

// Private diagnostic implementation; omitted from the AOT backend build.
PS2Runtime::RecompiledFunction ps2xResolveDiagnosticNativeOverlay(PS2Runtime *, uint8_t *, uint32_t);
void ps2xReleaseDiagnosticNativeOverlays(PS2Runtime *);

#include "ps2_native_overlay.h"
#include "ps2_ee_aot.h"
#include "ps2_ee_overlay_backend.h"

#if PS2X_RUNTIME_AOT_EE_OVERLAYS
const ps2native::ee_aot::Program &compiledEeProgram();
#endif

bool ps2xUsesAotEeOverlays() { return PS2X_RUNTIME_AOT_EE_OVERLAYS != 0; }

const ps2native::ee_aot::Dispatcher *ps2native::ee_aot::compiledDispatcher()
{
#if PS2X_RUNTIME_AOT_EE_OVERLAYS
    static const Dispatcher dispatcher(compiledEeProgram());
    return &dispatcher;
#else
    return nullptr;
#endif
}

PS2Runtime::RecompiledFunction ps2xResolveNativeOverlay(PS2Runtime *runtime, uint8_t *ram, uint32_t address)
{
#if PS2X_RUNTIME_AOT_EE_OVERLAYS
    (void)runtime;
    return ps2native::ee_aot::compiledDispatcher()->lookup(ram, address).function;
#else
    return ps2xResolveDiagnosticNativeOverlay(runtime, ram, address);
#endif
}

void ps2xReleaseNativeOverlays(PS2Runtime *runtime)
{
#if PS2X_RUNTIME_AOT_EE_OVERLAYS
    (void)runtime;
#else
    ps2xReleaseDiagnosticNativeOverlays(runtime);
#endif
}

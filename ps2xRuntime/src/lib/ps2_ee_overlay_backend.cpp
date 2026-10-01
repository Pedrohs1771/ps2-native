#include "ps2_native_overlay.h"
#include "ps2_ee_aot.h"

#if PS2X_RUNTIME_AOT_EE_OVERLAYS
const ps2native::ee_aot::Program &compiledEeProgram();
#endif

bool ps2xUsesAotEeOverlays() { return PS2X_RUNTIME_AOT_EE_OVERLAYS != 0; }

PS2Runtime::RecompiledFunction ps2xResolveNativeOverlay(PS2Runtime *runtime, uint8_t *ram, uint32_t address)
{
#if PS2X_RUNTIME_AOT_EE_OVERLAYS
    (void)runtime;
    static const ps2native::ee_aot::Dispatcher dispatcher(compiledEeProgram());
    return dispatcher.lookup(ram, address).function;
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

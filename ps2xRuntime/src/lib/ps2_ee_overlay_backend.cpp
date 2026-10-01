#include "ps2_native_overlay.h"
#include "ps2_ee_aot.h"
#include "ps2_ee_overlay_backend.h"
#include "ps2_ee_data_family.h"
#include <stdexcept>

#if PS2X_RUNTIME_AOT_EE_OVERLAYS
const ps2native::ee_aot::Program &compiledEeProgram();
#if PS2X_RUNTIME_EE_DATA_FAMILIES
const ps2native::ee_family::Program &compiledEeFamilyProgram();
#endif
namespace
{
    void invokeNormalOverlay(uint8_t *ram,R5900Context *context,PS2Runtime *runtime)
    {
        const auto admission=ps2native::ee_aot::compiledDispatcher()->admitNormalEntry(ram,context);
        if (admission.function)
        {
            admission.function(ram,context,runtime);
            return;
        }
#if PS2X_RUNTIME_EE_DATA_FAMILIES
        const auto family=ps2native::ee_family::compiledDispatcher()->admitNormalEntry(ram,context);
        if(family.function)
        {
            family.function(ram,context,runtime,family.base);
            return;
        }
#endif
        if (!runtime) throw std::invalid_argument("unsupported or uncovered native EE invocation");
        if (!context)
        {
            runtime->requestStop();
            return;
        }
        runtime->reportMissingFunction(ram,context,context->pc,0u,PS2Runtime::GuestBranchKind::IndirectJump,
            admission.status==ps2native::ee_aot::Status::UnsupportedEntryContext
                ? "AOT-normal-entry-context" : "AOT-entry-recheck");
    }
}
#endif

bool ps2xUsesAotEeOverlays() { return PS2X_RUNTIME_AOT_EE_OVERLAYS != 0; }
const ps2native::ee_family::Dispatcher *ps2native::ee_family::compiledDispatcher()
{
#if PS2X_RUNTIME_EE_DATA_FAMILIES
    static const Dispatcher dispatcher(compiledEeFamilyProgram());
    return &dispatcher;
#else
    return nullptr;
#endif
}

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
    if(ps2native::ee_aot::compiledDispatcher()->lookup(ram,address).function) return invokeNormalOverlay;
#if PS2X_RUNTIME_EE_DATA_FAMILIES
    if(ps2native::ee_family::compiledDispatcher()->lookup(ram,address).function) return invokeNormalOverlay;
#endif
    return nullptr;
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

#include "ps2_iop_backend.h"
#include "ps2x/iop/iop_subsystem.h"

#if PS2X_RUNTIME_NATIVE_IOP
const ps2x::iop::detail::IopNativeProgram &compiledIopProgram();
#endif

namespace ps2native::runtime
{
    std::unique_ptr<ps2x::iop::IopSubsystem> createIopSubsystem(ps2x::iop::IopHost &host)
    {
#if PS2X_RUNTIME_NATIVE_IOP
        return std::make_unique<ps2x::iop::IopSubsystem>(host, compiledIopProgram());
#else
        return std::make_unique<ps2x::iop::IopSubsystem>(host);
#endif
    }
}

#pragma once

#include <memory>

namespace ps2x::iop
{
    class IopHost;
    class IopSubsystem;
}

namespace ps2native::runtime
{
    // Internal construction boundary; the choice is fixed at build time.
    std::unique_ptr<ps2x::iop::IopSubsystem> createIopSubsystem(ps2x::iop::IopHost &host);
}

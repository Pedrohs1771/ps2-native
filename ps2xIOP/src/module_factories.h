#pragma once

#include "iop_service.h"

#include <functional>
#include <string_view>

namespace ps2x::iop::detail
{
    std::unique_ptr<IopService> createDbcmanService(IopHost &host);
    std::unique_ptr<IopService> createCdvdfsvService(IopHost &host);
    std::unique_ptr<IopService> createLoadfileService(
        IopHost &host,
        std::function<ModuleLoadResult(std::string_view, const void *, uint32_t)> loadModule,
        std::function<ModuleLoadResult(uint32_t, const void *, uint32_t)> loadIopBuffer);
    std::unique_ptr<IopService> createLibSdService(IopHost &host);
    std::unique_ptr<IopService> createMcservService(IopHost &host);
}

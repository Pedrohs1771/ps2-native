#pragma once

#include "ps2_runtime.h"
#include <string_view>

namespace ps2native::ee_aot { class Dispatcher; }
namespace ps2native::nexo
{
    // Opt-in observation. Exceptions are contained and no guest memory is changed.
    void captureEeMiss(const uint8_t *ram,const R5900Context *context,
                       const ee_aot::Dispatcher *dispatcher,uint32_t targetPc,uint32_t sourcePc,
                       PS2Runtime::GuestBranchKind kind,std::string_view operation,
                       bool moduleOwnsAddress,std::string_view moduleKey) noexcept;
}

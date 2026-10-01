#pragma once

#include "ps2x/iop/iop_host.h"
#include "ps2x/iop/iop_types.h"

#include <filesystem>
#include <span>

namespace ps2native::nexo::iop_lab
{
    // Laboratory-only observation. Failures are logged without changing guest execution.
    void captureModule(ps2x::iop::IopHost &host, std::string_view path,
                       std::span<const uint8_t> image, std::span<const uint8_t> arguments,
                       uint64_t cycles);
    std::filesystem::path captureRpcRequest(ps2x::iop::IopHost &host,
                                            const ps2x::iop::RpcRequest &request, uint64_t cycles);
    void captureRpcResult(ps2x::iop::IopHost &host, const std::filesystem::path &directory,
                          const ps2x::iop::RpcRequest &request, const ps2x::iop::RpcResult &result,
                          uint64_t cycles, uint64_t nativeInstructions, uint64_t interpretedInstructions,
                          bool nativeFault);
    void captureFault(ps2x::iop::IopHost &host, std::string_view diagnostic,
                      std::span<const uint8_t> ram, uint64_t cycles);
}

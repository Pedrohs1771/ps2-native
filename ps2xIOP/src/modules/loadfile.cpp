#include "../iop_service.h"
#include "../module_factories.h"
#include "../rpc_reply.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <mutex>
#include <span>
#include <string_view>
#include <utility>

namespace ps2x::iop::detail
{
    namespace
    {
        constexpr uint32_t kLoadfileSid = 0x80000006u;
        constexpr uint32_t kLoadModule = 0u;
        constexpr uint32_t kLoadModuleBuffer = 6u;
        constexpr uint32_t kGetVersion = 0xFFu;
        constexpr uint32_t kLoadfileVersion = 0x30303133u;
        constexpr size_t kPathBytes = 252u;
        constexpr size_t kArgumentBytes = 252u;
        constexpr size_t kModuleLoadPacketBytes = 8u + kPathBytes + kArgumentBytes;

        static_assert(kModuleLoadPacketBytes == 512u);

        uint32_t readU32Le(const uint8_t *bytes)
        {
            return static_cast<uint32_t>(bytes[0]) |
                   (static_cast<uint32_t>(bytes[1]) << 8u) |
                   (static_cast<uint32_t>(bytes[2]) << 16u) |
                   (static_cast<uint32_t>(bytes[3]) << 24u);
        }

        class LoadfileService final : public IopService
        {
        public:
            LoadfileService(IopHost &host,
                            std::function<ModuleLoadResult(std::string_view, const void *, uint32_t)> loadModule,
                            std::function<ModuleLoadResult(uint32_t, const void *, uint32_t)> loadIopBuffer)
                : m_host(host),
                  m_loadModule(std::move(loadModule)),
                  m_loadIopBuffer(std::move(loadIopBuffer))
            {
            }

            [[nodiscard]] std::string_view name() const override
            {
                return "LOADFILE";
            }

            [[nodiscard]] std::span<const uint32_t> sids() const override
            {
                return kSids;
            }

            [[nodiscard]] std::span<const std::string_view> moduleAliases() const override
            {
                return kModuleAliases;
            }

            void reset() override
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_versionQueries = 0u;
                m_moduleLoadRequests = 0u;
                m_failedModuleLoads = 0u;
                m_unsupportedRequests = 0u;
            }

            [[nodiscard]] RpcResult handleRpc(const RpcRequest &request) override
            {
                if (request.sid != kLoadfileSid)
                    return {};

                if (request.function == kGetVersion)
                {
                    {
                        std::lock_guard<std::mutex> lock(m_mutex);
                        ++m_versionQueries;
                    }
                    const std::array<uint32_t, 1> version{kLoadfileVersion};
                    (void)writeRpcWords(m_host, request.receive, version);
                    return {true, request.receive.address};
                }

                if (request.function == kLoadModule || request.function == kLoadModuleBuffer)
                    return loadModule(request);

                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    ++m_unsupportedRequests;
                }
                return {};
            }

            void appendDebugMetrics(std::vector<DebugMetric> &metrics) const override
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                metrics.push_back({"version_queries", m_versionQueries, false});
                metrics.push_back({"module_load_requests", m_moduleLoadRequests, false});
                metrics.push_back({"failed_module_loads", m_failedModuleLoads, false});
                metrics.push_back({"unsupported_requests", m_unsupportedRequests, false});
            }

        private:
            [[nodiscard]] RpcResult loadModule(const RpcRequest &request)
            {
                std::array<uint8_t, kModuleLoadPacketBytes> packet{};
                ModuleLoadResult loaded{true, -1, -1};
                bool valid = request.send.size >= packet.size() &&
                             m_host.readGuest(request.send.address, packet.data(), packet.size());

                std::string_view path;
                uint32_t argumentSize = 0u;
                if (valid)
                {
                    if (request.function == kLoadModuleBuffer)
                    {
                        argumentSize = readU32Le(packet.data() + 4u);
                        valid = readU32Le(packet.data()) != 0u && argumentSize <= kArgumentBytes;
                    }
                    else
                    {
                        const auto *pathStart = packet.data() + 8u;
                        const auto *pathEnd = std::find(pathStart, pathStart + kPathBytes, uint8_t{0});
                        const uint32_t rawArgumentSize = readU32Le(packet.data());
                        const int32_t signedArgumentSize = std::bit_cast<int32_t>(rawArgumentSize);
                        valid = pathEnd != pathStart && pathEnd != pathStart + kPathBytes &&
                                signedArgumentSize >= 0 &&
                                static_cast<uint32_t>(signedArgumentSize) <= kArgumentBytes;
                        if (valid)
                        {
                            path = std::string_view(reinterpret_cast<const char *>(pathStart),
                                                    static_cast<size_t>(pathEnd - pathStart));
                            argumentSize = static_cast<uint32_t>(signedArgumentSize);
                        }
                    }
                }

                if (valid)
                {
                    const void *arguments = argumentSize == 0u ? nullptr : packet.data() + 8u + kPathBytes;
                    loaded = request.function == kLoadModuleBuffer
                                 ? m_loadIopBuffer(readU32Le(packet.data()), arguments, argumentSize)
                                 : m_loadModule(path, arguments, argumentSize);
                    if (!loaded.handled || loaded.moduleId <= 0)
                        loaded = {true, -1, -1};
                }

                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    ++m_moduleLoadRequests;
                    if (!valid || loaded.moduleId <= 0)
                        ++m_failedModuleLoads;
                }

                const std::array<uint32_t, 2> response{
                    static_cast<uint32_t>(loaded.moduleId),
                    static_cast<uint32_t>(loaded.startResult),
                };
                (void)writeRpcWords(m_host, request.receive, response);
                return {true, request.receive.address};
            }

            inline static constexpr std::array<uint32_t, 1> kSids{kLoadfileSid};
            inline static constexpr std::array<std::string_view, 1> kModuleAliases{"loadfile"};

            IopHost &m_host;
            std::function<ModuleLoadResult(std::string_view, const void *, uint32_t)> m_loadModule;
            std::function<ModuleLoadResult(uint32_t, const void *, uint32_t)> m_loadIopBuffer;
            mutable std::mutex m_mutex;
            uint64_t m_versionQueries = 0u;
            uint64_t m_moduleLoadRequests = 0u;
            uint64_t m_failedModuleLoads = 0u;
            uint64_t m_unsupportedRequests = 0u;
        };
    }

    std::unique_ptr<IopService> createLoadfileService(
        IopHost &host,
        std::function<ModuleLoadResult(std::string_view, const void *, uint32_t)> loadModule,
        std::function<ModuleLoadResult(uint32_t, const void *, uint32_t)> loadIopBuffer)
    {
        return std::make_unique<LoadfileService>(host, std::move(loadModule), std::move(loadIopBuffer));
    }
}

#include "../iop_service.h"
#include "../module_factories.h"
#include "../rpc_reply.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <span>
#include <string_view>

namespace ps2x::iop::detail
{
    namespace
    {
        constexpr uint32_t kCdSearchSid = 0x80000597u;
        constexpr uint32_t kSearchFileFunction = 0u;
        constexpr size_t kSearchNameBytes = 256u;
        constexpr size_t kLegacySearchPacketBytes = 292u;
        constexpr size_t kExtendedSearchPacketBytes = 296u;
        constexpr size_t kLayerSearchPacketBytes = 300u;

        uint32_t readU32Le(const uint8_t *bytes)
        {
            return static_cast<uint32_t>(bytes[0]) |
                   (static_cast<uint32_t>(bytes[1]) << 8u) |
                   (static_cast<uint32_t>(bytes[2]) << 16u) |
                   (static_cast<uint32_t>(bytes[3]) << 24u);
        }

        void writeU32Le(uint8_t *bytes, uint32_t value)
        {
            bytes[0] = static_cast<uint8_t>(value);
            bytes[1] = static_cast<uint8_t>(value >> 8u);
            bytes[2] = static_cast<uint8_t>(value >> 16u);
            bytes[3] = static_cast<uint8_t>(value >> 24u);
        }

        class CdvdfsvService final : public IopService
        {
        public:
            explicit CdvdfsvService(IopHost &host) : m_host(host) {}

            [[nodiscard]] std::string_view name() const override { return "CDVDFSV"; }
            [[nodiscard]] std::span<const uint32_t> sids() const override { return kSids; }
            [[nodiscard]] std::span<const std::string_view> moduleAliases() const override { return kModuleAliases; }

            void reset() override
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_searchRequests = 0u;
                m_searchHits = 0u;
                m_malformedRequests = 0u;
            }

            [[nodiscard]] RpcResult handleRpc(const RpcRequest &request) override
            {
                if (request.sid != kCdSearchSid)
                    return {};

                uint32_t resultCode = 0u;
                // CDVDFSV has three known packet ABIs. The first words are
                // output file metadata, not a layer selector. Extended packets
                // add a metadata flag; only the 300-byte variant has a layer.
                // Reference: Play! CCdvdfsv::SearchFile (0x124/0x128/0x12C).
                const size_t packetBytes = request.send.size;
                const bool knownLayout = packetBytes == kLegacySearchPacketBytes ||
                                         packetBytes == kExtendedSearchPacketBytes ||
                                         packetBytes == kLayerSearchPacketBytes;
                bool malformed = request.function != kSearchFileFunction ||
                                 request.send.address == 0u || !knownLayout;
                std::array<uint8_t, kLayerSearchPacketBytes> packet{};
                if (!malformed && !m_host.readGuest(request.send.address, packet.data(), packetBytes))
                    malformed = true;

                if (!malformed)
                {
                    const size_t nameOffset = packetBytes == kLegacySearchPacketBytes ? 32u : 36u;
                    const uint32_t layer = packetBytes == kLayerSearchPacketBytes
                                               ? readU32Le(packet.data() + 296u) : 0u;
                    const uint32_t destination = readU32Le(packet.data() + nameOffset + kSearchNameBytes);
                    const auto *nameStart = packet.data() + nameOffset;
                    const auto *nameEnd = std::find(nameStart, nameStart + kSearchNameBytes, uint8_t{0});
                    if (nameStart == nameEnd || nameEnd == nameStart + kSearchNameBytes || destination == 0u)
                    {
                        malformed = true;
                    }
                    else
                    {
                        const std::string_view path(reinterpret_cast<const char *>(nameStart),
                                                    static_cast<size_t>(nameEnd - nameStart));
                        CdFileInfo info{};
                        if (m_host.searchCdFile(path, layer, info))
                        {
                            std::array<uint8_t, 36> packed{};
                            writeU32Le(packed.data(), info.lsn);
                            writeU32Le(packed.data() + 4u, info.sizeBytes);
                            std::memcpy(packed.data() + 8u, info.name.data(), info.name.size());
                            std::memcpy(packed.data() + 24u, info.date.data(), info.date.size());
                            if (m_host.writeGuest(destination, packed.data(), nameOffset))
                                resultCode = 1u;
                        }
                    }
                }

                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    ++m_searchRequests;
                    if (resultCode != 0u)
                        ++m_searchHits;
                    if (malformed)
                        ++m_malformedRequests;
                }

                const std::array<uint32_t, 1> response{resultCode};
                (void)writeRpcWords(m_host, request.receive, response);
                return {true, request.receive.address};
            }

            void appendDebugMetrics(std::vector<DebugMetric> &metrics) const override
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                metrics.push_back({"search_requests", m_searchRequests, false});
                metrics.push_back({"search_hits", m_searchHits, false});
                metrics.push_back({"malformed_requests", m_malformedRequests, false});
            }

        private:
            inline static constexpr std::array<uint32_t, 1> kSids{kCdSearchSid};
            inline static constexpr std::array<std::string_view, 2> kModuleAliases{"cdvdfsv", "xcdvdfsv"};

            IopHost &m_host;
            mutable std::mutex m_mutex;
            uint64_t m_searchRequests = 0u;
            uint64_t m_searchHits = 0u;
            uint64_t m_malformedRequests = 0u;
        };
    }

    std::unique_ptr<IopService> createCdvdfsvService(IopHost &host)
    {
        return std::make_unique<CdvdfsvService>(host);
    }
}

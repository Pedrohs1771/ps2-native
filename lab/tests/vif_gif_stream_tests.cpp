#include "runtime/ps2_memory.h"
#include "runtime/gs/ps2_gif_arbiter.h"
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

namespace
{
void append(std::vector<uint8_t> &out, uint64_t value, size_t size = 8)
{
    const auto offset = out.size(); out.resize(offset + size);
    std::memcpy(out.data() + offset, &value, size);
}
std::vector<uint8_t> direct(const std::vector<uint8_t> &gif)
{
    std::vector<uint8_t> result;
    append(result, 0x50000000u | uint32_t(gif.size() / 16), 4);
    result.insert(result.end(), gif.begin(), gif.end()); return result;
}
void require(bool condition, const char *message) { if (!condition) throw std::runtime_error(message); }
}

int main(int argc, char **argv)
{
    try
    {
        auto memory = std::make_unique<PS2Memory>();
        require(memory->initialize(), "Memory initialization failed");
        std::vector<std::vector<uint8_t>> received;
        GifArbiter arbiter([&](const uint8_t *bytes, uint32_t size)
        {
            require(GifArbiter::currentDeliveryPath() == GifPathId::Path2, "DIRECT path differs");
            received.emplace_back(bytes, bytes + size);
        });
        memory->setGifArbiter(&arbiter);
        const bool legacyReceiver = argc == 2 && std::strcmp(argv[1], "--legacy-negative") == 0;
        memory->setGifStreamTransport(!legacyReceiver);
        // A guest parser reset must not change which host receiver is bound.
        memory->write32(0x10003C10u, 1u);
        std::vector<uint8_t> header;
        append(header, 2ull | (2ull << 58) | (1ull << 15)); append(header, 0);
        std::vector<uint8_t> payload(32);
        for (size_t i = 0; i < payload.size(); ++i) payload[i] = uint8_t(i + 0x20);
        const auto first = direct(header), second = direct(payload);
        // VIF command/header fragmentation and separate IMAGE DIRECT commands.
        memory->processVIF1Data(first.data(), 7);
        memory->processVIF1Data(first.data() + 7, first.size() - 7);
        memory->processVIF1Data(second.data(), second.size());
        require(received == std::vector<std::vector<uint8_t>>{header, payload},
                "Streaming receiver must get original GIF bytes, without synthetic tags");
        memory->setGifArbiter(nullptr);
        std::cout << "{\"original_gif_bytes\":true,\"path\":2,\"direct_fragments\":3,\"scope\":\"VIF raw receiver binding\"}\n";
        return 0;
    }
    catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}

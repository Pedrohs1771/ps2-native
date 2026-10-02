#include "runtime/gs/gs_parallel_device.h"
#include "runtime/gs/ps2_gs_psmct32.h"

#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <thread>

using ps2native::gs::ParallelDevice;

namespace
{
void require(bool value, const char *message)
{
    if (!value) throw std::runtime_error(message);
}
void append(std::vector<uint8_t> &bytes, uint64_t value)
{
    const size_t offset = bytes.size(); bytes.resize(offset + 8);
    std::memcpy(bytes.data() + offset, &value, 8);
}
void ad(std::vector<uint8_t> &bytes, uint8_t address, uint64_t value)
{
    append(bytes, value); append(bytes, address);
}
template<class F> void rejects(F &&function, const char *message)
{
    bool rejected = false;
    try { function(); } catch (const std::invalid_argument &) { rejected = true; }
    require(rejected, message);
}
}

int main()
{
    try
    {
        ParallelDevice device;
        rejects([&] { device.readVram(0, 4); }, "Uninitialized access must fail");
        device.initialize();
        const std::vector<uint8_t> zero(ParallelDevice::VramBytes, 0);
        device.uploadVram(0, zero);

        // Complete PACKED A+D packet: untextured flat sprite, 8..24 exclusive,
        // known RGBA, no depth or alpha tests and no blending. No CPU rasterizer.
        std::vector<uint8_t> packet;
        append(packet, 10ull | (1ull << 15) | (1ull << 60)); append(packet, 0xe);
        ad(packet, 0x4c, 1ull << 16); // FRAME_1, PSMCT32, width 64.
        ad(packet, 0x40, (63ull << 16) | (31ull << 48));
        ad(packet, 0x18, 0); // XYOFFSET_1.
        ad(packet, 0x47, 0); // TEST_1.
        ad(packet, 0x4e, (1ull << 32)); // ZBUF_1 write mask.
        ad(packet, 0x1a, 1); // PRMODECONT.
        ad(packet, 0x00, 6); // PRIM sprite.
        constexpr uint32_t expected = 0x80403020u;
        ad(packet, 0x01, uint64_t(expected) | (0x3f800000ull << 32));
        ad(packet, 0x05, (8ull << 4) | ((8ull << 4) << 16));
        ad(packet, 0x05, (24ull << 4) | ((24ull << 4) << 16));
        device.submitGif(packet);
        device.flush();
        const auto vram = device.readVram(0, ParallelDevice::VramBytes);
        for (uint32_t y = 0; y < 32; ++y)
            for (uint32_t x = 0; x < 64; ++x)
            {
                uint32_t actual;
                std::memcpy(&actual, vram.data() + GSPSMCT32::addrPSMCT32(0, 1, x, y), 4);
                const uint32_t wanted = x >= 8 && x < 24 && y >= 8 && y < 24 ? expected : 0;
                require(actual == wanted, "GPU raw GIF sprite pixels differ");
            }
        auto count = device.stats();
        require(count.packets == 1 && count.primitives > 0 && count.renderPasses > 0,
                "Real GPU primitives and render passes must be recorded");
        rejects([&] { device.submitGif(std::span(packet).first(17)); }, "Partial qword must fail");
        rejects([&] { device.submitGif(std::span(packet).first(32)); }, "Truncated payload must fail");
        rejects([&] { device.submitGif({}); }, "Empty packet must fail");
        rejects([&] { device.submitGif(packet, 0); }, "Path zero must fail");
        rejects([&] { device.submitGif(packet, 4); }, "Invalid GIF path must fail");
        rejects([&] { device.writeRegister(128, 0); }, "Invalid register must fail");
        rejects([&] { device.initialize(); }, "Repeated initialization must fail");
        // A valid prefix followed by a truncated tag must be rejected atomically.
        auto malformed = packet;
        append(malformed, 1ull | (1ull << 60)); append(malformed, 0xe);
        rejects([&] { device.submitGif(malformed); }, "Bad later tag must fail");
        rejects([&] { device.readVram(ParallelDevice::VramBytes - 1, 2); }, "Read past VRAM must fail");
        rejects([&] { device.uploadVram(ParallelDevice::VramBytes, std::span(zero).first(1)); }, "Upload past VRAM must fail");
        require(device.stats().packets == 1, "Rejected packets must not be submitted");

        std::vector<uint8_t> nop;
        append(nop, 1ull); append(nop, ~0ull); // NREG=0 means 16 PACKED NOPs.
        for (unsigned i = 0; i < 16; ++i) { append(nop, 0); append(nop, 0); }
        device.submitGif(nop, 1);
        std::vector<uint8_t> odd;
        append(odd, 3ull | (1ull << 58) | (1ull << 60)); append(odd, 0xf);
        for (unsigned i = 0; i < 4; ++i) append(odd, 0); // Three reglist words and padding.
        append(odd, 3ull << 58); append(odd, 0); // Zero-loop IMAGE_RESERVED tag.
        device.submitGif(odd, 2);
        std::vector<uint8_t> unaligned(1, 0);
        append(unaligned, 0); append(unaligned, 0);
        device.submitGif(std::span(unaligned).subspan(1), 3);
        require(device.readVram(0, ParallelDevice::VramBytes) == vram,
                "Rejected prefix or NOPs must not change VRAM");
        require(device.stats().packets == 4, "All GIF paths and alignment must be supported");

        // Host-to-local image transfer through one original multi-tag packet.
        std::vector<uint8_t> image;
        append(image, 4ull | (1ull << 60)); append(image, 0xe);
        ad(image, 0x50, (128ull << 32) | (1ull << 48)); // DBP=128, DBW=1, CT32.
        ad(image, 0x51, 0);
        ad(image, 0x52, 2ull | (2ull << 32));
        ad(image, 0x53, 0);
        append(image, 1ull | (1ull << 15) | (2ull << 58)); append(image, 0);
        constexpr uint32_t colors[4] = {0x80706050, 0x80123456, 0x80887766, 0x80445566};
        append(image, uint64_t(colors[0]) | (uint64_t(colors[1]) << 32));
        append(image, uint64_t(colors[2]) | (uint64_t(colors[3]) << 32));
        device.submitGif(image);
        const auto transferred = device.readVram(0, ParallelDevice::VramBytes);
        for (uint32_t y = 0; y < 2; ++y)
            for (uint32_t x = 0; x < 2; ++x)
            {
                uint32_t actual;
                std::memcpy(&actual, transferred.data() + GSPSMCT32::addrPSMCT32(128, 1, x, y), 4);
                require(actual == colors[y * 2 + x], "Multi-tag IMAGE pixels differ");
            }

        device.resetRegisters();
        device.uploadVram(0, zero);
        device.writeRegister(0x4c, 1ull << 16);
        device.writeRegister(0x40, (63ull << 16) | (31ull << 48));
        device.writeRegister(0x4e, 1ull << 32);
        device.writeRegister(0x1a, 1);
        device.writeRegister(0x00, 0); // Point.
        std::vector<uint8_t> fragment;
        append(fragment, 2ull | (1ull << 15) | (1ull << 60)); append(fragment, 0xe);
        ad(fragment, 0x01, uint64_t(expected) | (0x3f800000ull << 32));
        ad(fragment, 0x05, (4ull << 4) | ((4ull << 4) << 16));
        // Backing storage contains the next vertex, but it has not been sent.
        device.submitGifStream(std::span(fragment).first(32), 3);
        rejects([&] { device.submitGif(packet, 3); }, "Complete packet must not replace a partial path");
        rejects([&] { device.submitGifStream(std::span(fragment).first(17), 3); }, "Partial stream qword must fail");
        auto partial = device.readVram(0, ParallelDevice::VramBytes);
        uint32_t point;
        const auto pointAddress = GSPSMCT32::addrPSMCT32(0, 1, 4, 4);
        std::memcpy(&point, partial.data() + pointAddress, 4);
        require(point == 0, "GIF fragment consumed an unsent future vertex");
        device.submitGifStream(std::span(fragment).subspan(32), 3);
        partial = device.readVram(0, ParallelDevice::VramBytes);
        std::memcpy(&point, partial.data() + pointAddress, 4);
        require(point == expected, "GIF path continuation lost its vertex");

        // Fewer than NREG payload qwords must use the scalar continuation, not
        // an optimized handler which would make zero progress or read ahead.
        device.resetRegisters();
        std::vector<uint8_t> grouped;
        append(grouped, 1ull | (2ull << 60)); append(grouped, 0xee);
        ad(grouped, 0x01, uint64_t(expected) | (0x3f800000ull << 32));
        ad(grouped, 0x00, 6);
        device.submitGifStream(std::span(grouped).first(32), 1);
        device.submitGifStream(std::span(grouped).subspan(32), 1);
        device.submitGif(nop, 1); // A complete packet is now legal again.
        // A reglist's odd final 64-bit register keeps its 128-bit padding.
        device.submitGifStream(std::span(odd).first(32), 2);
        device.submitGifStream(std::span(odd).subspan(32), 2);
        for (size_t offset = 0; offset < image.size(); offset += 16)
            device.submitGifStream(std::span(image).subspan(offset, 16), 3);

        // A host update and a smaller readback must honor their exact ranges.
        const std::vector<uint8_t> patch{0xa1, 0xb2, 0xc3, 0xd4};
        device.uploadVram(4097, patch);
        require(device.readVram(4097, 4) == patch, "Host VRAM range did not round-trip");
        std::exception_ptr workerError;
        std::thread worker([&]
        {
            try { require(device.readVram(4097, 4) == patch, "Worker readback differs"); }
            catch (...) { workerError = std::current_exception(); }
        });
        worker.join();
        if (workerError) std::rethrow_exception(workerError);
        device.nextFrame();
        device.resetRegisters();
        require(device.readVram(4097, 4) == patch, "Register reset must keep VRAM");
        count = device.stats();
        std::cout << "{\"gpu\":\"" << device.deviceName() << "\",\"raster_backend\":\"parallel-vulkan\","
                  << "\"present_backend\":\"none\",\"pixels_checked\":2048,\"packets\":" << count.packets
                  << ",\"stream_chunks\":" << count.streamChunks
                  << ",\"gpu_primitives\":" << count.primitives << ",\"gpu_render_passes\":" << count.renderPasses
                  << ",\"strict_approval\":false,\"scope\":\"synthetic raw-GIF transport and VRAM ranges\"}\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n'; return 1;
    }
}

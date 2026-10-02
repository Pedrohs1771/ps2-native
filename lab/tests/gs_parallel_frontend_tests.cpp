#include "runtime/gs/gs_parallel_backend.h"
#include "runtime/gs/gs_frontend.h"
#include "runtime/gs/ps2_gif_arbiter.h"
#include "runtime/ps2_memory.h"
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace
{
void require(bool condition, const char *message) { if (!condition) throw std::runtime_error(message); }
void append(std::vector<uint8_t> &out, uint64_t value)
{
    const size_t offset = out.size(); out.resize(offset + 8); std::memcpy(out.data() + offset, &value, 8);
}
void ad(std::vector<uint8_t> &out, uint8_t reg, uint64_t value) { append(out, value); append(out, reg); }
}

int main()
{
    try
    {
        std::vector<uint8_t> memory(ps2native::gs::ParallelDevice::VramBytes);
        GSRegisters registers{};
        registers.pmode = 1;
        registers.dispfb1 = 1ull << 9;
        // The current presentation contract requires at least 64 scanlines.
        registers.display1 = (63ull << 32) | (63ull << 44);
        GS gs; gs.init(memory.data(), memory.size(), &registers);
        auto backend = std::make_unique<GSParallelBackend>(); auto *gpu = backend.get();
        gs.setRasterBackend(std::move(backend));
        std::vector<uint8_t> sprite;
        append(sprite, 10ull | (1ull << 15) | (1ull << 60)); append(sprite, 0xe);
        ad(sprite, GS_REG_FRAME_1, 1ull << 16);
        ad(sprite, GS_REG_SCISSOR_1, (63ull << 16) | (31ull << 48));
        ad(sprite, GS_REG_XYOFFSET_1, 0);
        ad(sprite, GS_REG_TEST_1, 0);
        ad(sprite, GS_REG_ZBUF_1, 1ull << 32);
        ad(sprite, GS_REG_PRMODECONT, 1);
        ad(sprite, GS_REG_PRIM, GS_PRIM_SPRITE);
        constexpr uint32_t color = 0x80403020;
        ad(sprite, GS_REG_RGBAQ, uint64_t(color) | (0x3f800000ull << 32));
        ad(sprite, GS_REG_XYZ2, (8ull << 4) | ((8ull << 4) << 16));
        ad(sprite, GS_REG_XYZ2, (24ull << 4) | ((24ull << 4) << 16));
        require(gs.processNativePackedGIFPacket(sprite.data(), sprite.size()), "Native PACKED route rejected");
        require(gs.ReadVram(GS_PSM_CT32, 0, 1, 10, 10) == color, "Frontend sprite pixel differs");
        require(gs.ReadVram(GS_PSM_CT32, 0, 1, 1, 1) == 0, "Frontend sprite wrote outside bounds");
        auto stats = gpu->stats();
        require(stats.streamChunks == 1 && stats.primitives == 1, "Frontend must send one raw GIF primitive once");
        require(gs.getDebugSnapshot().ctx[0].frame.fbw == 1, "Frontend shadow state differs");

        // Interleave PATH2 with a suspended PATH1; no tag reconstruction/reset.
        gs.writeRegister(GS_REG_PRIM, GS_PRIM_POINT);
        std::vector<uint8_t> point;
        append(point, 2ull | (1ull << 60)); append(point, 0xe);
        ad(point, GS_REG_RGBAQ, uint64_t(color) | (0x3f800000ull << 32));
        ad(point, GS_REG_XYZ2, (4ull << 4) | ((4ull << 4) << 16));
        std::vector<uint8_t> nop; append(nop, 0); append(nop, 0);
        GifArbiter arbiter([&](const uint8_t *data, uint32_t size) { gs.processGIFPacket(data, size); });
        arbiter.submit(GifPathId::Path1, point.data(), 32); arbiter.drain();
        require(gs.ReadVram(GS_PSM_CT32, 0, 1, 4, 4) == 0, "Fragment drew its future vertex");
        arbiter.submit(GifPathId::Path2, nop.data(), nop.size()); arbiter.drain();
        arbiter.submit(GifPathId::Path1, point.data() + 32, 16); arbiter.drain();
        require(gs.ReadVram(GS_PSM_CT32, 0, 1, 4, 4) == color, "PATH1 continuation was lost");
        require(GifArbiter::currentDeliveryPath() == GifPathId::Path3, "Delivery path leaked outside callback");
        require(gpu->GifStreamState(1)->loopsRemaining == 0, "Frontend PATH1 remains incomplete");

        const std::vector<uint8_t> texels{0x11,0x22,0x33,0x80,0x44,0x55,0x66,0x80};
        gs.uploadImageNative(128ull << 32 | 1ull << 48, 0, 2ull | 1ull << 32, 0, texels.data(), texels.size());
        require(gs.ReadVram(GS_PSM_CT32, 128, 1, 0, 0) == 0x80332211u, "Native image first pixel differs");
        require(gs.ReadVram(GS_PSM_CT32, 128, 1, 1, 0) == 0x80665544u, "Native image second pixel differs");
        gs.writeRegister(GS_REG_BITBLTBUF, 128ull | 1ull << 16);
        gs.writeRegister(GS_REG_TRXREG, 2ull | 1ull << 32);
        gs.writeRegister(GS_REG_TRXDIR, 1);
        std::vector<uint8_t> fifo(8);
        require(gs.consumeLocalToHostBytes(fifo.data(), fifo.size()) == 8 && fifo == texels,
                "Local-to-host FIFO differs");
        require(gs.consumeLocalToHostBytes(fifo.data(), fifo.size()) == 0, "FIFO cursor did not advance");
        gs.WriteVram(GS_PSM_CT32, 128, 1, 1, 0, 0x80776655u);
        require(gs.ReadVram(GS_PSM_CT32, 128, 1, 1, 0) == 0x80776655u, "Host write did not reach GPU VRAM");
        gs.latchHostPresentationFrame();
        std::vector<uint8_t> pixels; uint32_t width=0, height=0;
        require(gs.copyLatchedHostPresentationFrame(pixels, width, height), "GPU readback was not presented");
        require(width == 64 && height == 64, "Presentation dimensions differ");
        uint32_t presented; std::memcpy(&presented, pixels.data() + (10 * width + 10) * 4, 4);
        require((presented & 0xffffffu) == (color & 0xffffffu), "Presented GPU color differs");
        stats = gpu->stats();
        require(stats.primitives == 2 && stats.streamChunks == 4, "GPU primitives were duplicated or dropped");
        std::cout << "{\"gpu\":\"" << gpu->deviceName() << "\",\"raster_backend\":\"parallel-vulkan\","
                  << "\"present_backend\":\"cpu-readback\",\"gpu_primitives\":" << stats.primitives
                  << ",\"stream_chunks\":" << stats.streamChunks << ",\"input_causal\":false,\"strict_approval\":false,"
                  << "\"scope\":\"GS frontend synthetic rendering, paths, transfers and presentation\"}\n";
        return 0;
    }
    catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}

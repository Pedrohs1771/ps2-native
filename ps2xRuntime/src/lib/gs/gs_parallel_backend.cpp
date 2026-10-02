#include "runtime/gs/gs_parallel_backend.h"
#include "runtime/gs/gs_cpu_backend.h"
#include <array>
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <mutex>
#include <stdexcept>

struct GSParallelBackend::Impl
{
    mutable std::recursive_mutex mutex;
    ps2native::gs::ParallelDevice device;
    GSCpuBackend host;
    std::array<GSGifStreamState, 4> paths{};
    uint8_t *mirror = nullptr;
    bool mirrorDirty = false;
    bool trace = false;
    uint64_t presentations = 0;
    void readback()
    {
        if (!mirrorDirty) return;
        const auto bytes = device.readVram(0, ps2native::gs::ParallelDevice::VramBytes);
        std::memcpy(mirror, bytes.data(), bytes.size());
        mirrorDirty = false;
    }
    void upload()
    {
        device.uploadVram(0, {mirror, ps2native::gs::ParallelDevice::VramBytes});
        mirrorDirty = false;
    }
};

GSParallelBackend::GSParallelBackend() : impl(std::make_unique<Impl>()) {}
GSParallelBackend::~GSParallelBackend() = default;
void GSParallelBackend::Initialize(uint8_t *memory, uint32_t size)
{
    std::lock_guard lock(impl->mutex);
    if (!memory || size != ps2native::gs::ParallelDevice::VramBytes)
        throw std::invalid_argument("Vulkan GS requires initialized 4 MiB host VRAM");
    impl->device.initialize();
    impl->mirror = memory;
    impl->host.Initialize(memory, size);
    impl->upload();
    impl->trace = std::getenv("PS2X_GS_GPU_TRACE") != nullptr;
    if (impl->trace)
        std::fprintf(stderr, "[gs:gpu] initialized gpu=%s raster=parallel-vulkan present=opengl-readback\n",
                     impl->device.deviceName().c_str());
}
void GSParallelBackend::Reset()
{
    std::lock_guard lock(impl->mutex);
    impl->device.resetRegisters(); impl->host.Reset(); impl->paths = {};
}
void GSParallelBackend::Submit(const GSPrimitiveBatch &)
{
    throw std::invalid_argument("Raw Vulkan GS requires register/GIF submission through the frontend");
}
void GSParallelBackend::LoadClut(const GSTex0Reg &, const GSTexClutReg &)
{
    // Original TEX0/TEX2 writes already perform the renderer's CLUT operation.
}
GSGifStreamState *GSParallelBackend::GifStreamState(uint32_t path)
{
    if (path < 1 || path > 3) throw std::invalid_argument("Invalid frontend GIF path");
    return &impl->paths[path];
}
void GSParallelBackend::SubmitGifStream(const uint8_t *data, uint32_t bytes, uint32_t path)
{
    std::lock_guard lock(impl->mutex);
    impl->device.submitGifStream({data, bytes}, path); impl->mirrorDirty = true;
}
void GSParallelBackend::WriteRegister(uint8_t address, uint64_t value)
{
    std::lock_guard lock(impl->mutex);
    // The frontend expands HWREG into UploadImage; sending it here too would
    // apply the same eight image bytes twice.
    if (address != GS_REG_HWREG) impl->device.writeRegister(address, value);
    impl->mirrorDirty = true;
}
void GSParallelBackend::BeginTransfer(const GSTransferCommand &command)
{
    std::lock_guard lock(impl->mutex);
    if (command.direction == 1) impl->readback();
    impl->host.BeginTransfer(command);
}
void GSParallelBackend::UploadImage(const uint8_t *data, uint32_t bytes)
{
    std::lock_guard lock(impl->mutex);
    if (!data && bytes) throw std::invalid_argument("Null image transfer");
    for (uint32_t offset = 0; offset < bytes;)
    {
        uint64_t value = 0;
        const auto count = std::min(uint32_t(8), bytes - offset);
        std::memcpy(&value, data + offset, count);
        impl->device.writeRegister(GS_REG_HWREG, value); offset += count;
    }
    impl->host.UploadImage(data, bytes); impl->mirrorDirty = true;
}
void GSParallelBackend::ObserveImageData(const uint8_t *data, uint32_t bytes)
{
    std::lock_guard lock(impl->mutex);
    impl->host.UploadImage(data, bytes);
}
void GSParallelBackend::Flush() { std::lock_guard lock(impl->mutex); impl->device.flush(); }
void GSParallelBackend::TextureFlush() { std::lock_guard lock(impl->mutex); impl->host.TextureFlush(); }
void GSParallelBackend::Sync(GSSyncReason) { Flush(); }
PresentationFrame GSParallelBackend::Present(const GSPresentationRequest &request)
{
    std::lock_guard lock(impl->mutex); impl->readback();
    auto frame = impl->host.Present(request);
    if (impl->trace && ++impl->presentations % 120u == 0)
    {
        const auto s = impl->device.stats();
        std::fprintf(stderr, "[gs:gpu] presents=%llu chunks=%llu gpu_primitives=%llu gpu_passes=%llu readbacks=%llu readback_bytes=%llu\n",
            static_cast<unsigned long long>(impl->presentations), static_cast<unsigned long long>(s.streamChunks),
            static_cast<unsigned long long>(s.primitives), static_cast<unsigned long long>(s.renderPasses),
            static_cast<unsigned long long>(s.readbacks), static_cast<unsigned long long>(s.readbackBytes));
    }
    return frame;
}
bool GSParallelBackend::ClearFramebuffer(const GSContext &context, uint32_t rgba)
{
    std::lock_guard lock(impl->mutex); impl->readback();
    const bool result = impl->host.ClearFramebuffer(context, rgba);
    if (result) impl->upload(); return result;
}
uint32_t GSParallelBackend::ConsumeLocalToHostBytes(uint8_t *data, uint32_t bytes)
{
    std::lock_guard lock(impl->mutex); return impl->host.ConsumeLocalToHostBytes(data, bytes);
}
uint32_t GSParallelBackend::ReadVram(uint32_t psm, uint32_t base, uint32_t width, uint32_t x, uint32_t y) const
{
    std::lock_guard lock(impl->mutex); impl->readback(); return impl->host.ReadVram(psm, base, width, x, y);
}
void GSParallelBackend::WriteVram(uint32_t psm, uint32_t base, uint32_t width, uint32_t x, uint32_t y, uint32_t value)
{
    std::lock_guard lock(impl->mutex); impl->readback();
    impl->host.WriteVram(psm, base, width, x, y, value); impl->upload();
}
void GSParallelBackend::SnapshotVram(std::vector<uint8_t> &out) const
{
    std::lock_guard lock(impl->mutex); out = impl->device.readVram(0, ps2native::gs::ParallelDevice::VramBytes);
}
GSTransferSnapshot GSParallelBackend::GetTransferSnapshot() const
{
    std::lock_guard lock(impl->mutex); return impl->host.GetTransferSnapshot();
}
ps2native::gs::ParallelDeviceStats GSParallelBackend::stats() const { return impl->device.stats(); }
std::string GSParallelBackend::deviceName() const { return impl->device.deviceName(); }

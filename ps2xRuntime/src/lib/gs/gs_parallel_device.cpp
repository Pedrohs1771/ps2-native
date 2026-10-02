#include "runtime/gs/gs_parallel_device.h"
#include "gs_interface.hpp"
#include "context.hpp"
#include "thread_id.hpp"

#include <bit>
#include <array>
#include <algorithm>
#include <cstring>
#include <mutex>
#include <stdexcept>

namespace ps2native::gs
{
struct ParallelDevice::Impl
{
    mutable std::mutex mutex;
    Vulkan::Context context;
    Vulkan::Device device;
    ParallelGS::GSInterface gs;
    bool ready = false;
    ParallelDeviceStats totals;
    std::array<size_t, 4> pendingQwords{};

    void requireReady() const
    {
        if (!ready) throw std::invalid_argument("Vulkan GS device is not initialized");
        // One serialized command-pool index is shared by EE and presentation
        // callers. Granite's TLS registration is required on each host thread.
        Util::register_thread_index(0);
    }
    static void range(size_t offset, size_t size)
    {
        if (offset > VramBytes || size > VramBytes - offset)
            throw std::invalid_argument("Vulkan GS access exceeds 4 MiB VRAM");
    }
    void account()
    {
        const auto counters = gs.consume_flush_stats();
        totals.primitives += counters.num_primitives;
        totals.renderPasses += counters.num_render_passes;
    }
    void sendGif(std::span<const uint8_t> bytes, uint32_t path)
    {
        // Upstream accesses typed register objects; callers can supply any byte
        // alignment. Copy only when needed, retaining all original tag bytes.
        struct alignas(16) Qword { uint64_t words[2]; };
        std::vector<Qword> aligned;
        const void *data = bytes.data();
        if (reinterpret_cast<uintptr_t>(data) % 16u != 0)
        {
            aligned.resize(bytes.size() / sizeof(Qword));
            std::memcpy(aligned.data(), bytes.data(), bytes.size());
            data = aligned.data();
        }
        gs.gif_transfer(path, data, bytes.size());
    }
};

ParallelDevice::ParallelDevice() : impl(std::make_unique<Impl>()) {}
ParallelDevice::~ParallelDevice() { Util::register_thread_index(0); }

void ParallelDevice::initialize()
{
    std::lock_guard lock(impl->mutex);
    if (impl->ready) throw std::invalid_argument("Vulkan GS device is already initialized");
    if (!Vulkan::Context::init_loader(nullptr))
        throw std::runtime_error("Cannot initialize the Vulkan loader");
    impl->context.set_num_thread_indices(1);
    if (!impl->context.init_instance_and_device(nullptr, 0, nullptr, 0,
          Vulkan::CONTEXT_CREATION_ENABLE_PUSH_DESCRIPTOR_BIT |
          Vulkan::CONTEXT_CREATION_ENABLE_DESCRIPTOR_HEAP_BIT |
          Vulkan::CONTEXT_CREATION_ENABLE_DESCRIPTOR_BUFFER_BIT))
        throw std::runtime_error("Cannot initialize a Vulkan GS device");
    impl->device.set_context(impl->context);
    const auto type = impl->device.get_gpu_properties().deviceType;
    if (type != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && type != VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
        throw std::runtime_error("Vulkan GS requires a physical GPU; software rendering was selected");
    impl->device.init_frame_contexts(4);
    const ParallelGS::GSOptions options{};
    if (!impl->gs.init(&impl->device, options))
        throw std::runtime_error("GPU lacks features required by paraLLEl-GS");
    impl->ready = true;
}

void ParallelDevice::resetRegisters()
{
    std::lock_guard lock(impl->mutex); impl->requireReady();
    impl->gs.flush(); impl->account(); impl->gs.reset_context_state();
    impl->pendingQwords.fill(0);
}

void ParallelDevice::submitGif(std::span<const uint8_t> packet, uint32_t path)
{
    std::lock_guard lock(impl->mutex); impl->requireReady();
    static_assert(std::endian::native == std::endian::little,
                  "The upstream GIF register layout requires a little-endian host");
    constexpr size_t maxPacketBytes = 64u * 1024u * 1024u;
    if (path < 1 || path > 3 || packet.empty() || packet.size() % 16 != 0 ||
        packet.size() > maxPacketBytes)
        throw std::invalid_argument("Invalid complete GIF packet extent or path");
    if (impl->pendingQwords[path] != 0)
        throw std::invalid_argument("Complete GIF packet requires a path at a tag boundary");

    // Validate every tag before any submission. A bad later tag must not leave
    // registers or VRAM modified by an earlier, valid prefix of this packet.
    for (size_t offset = 0; offset < packet.size();)
    {
        uint64_t tag;
        std::memcpy(&tag, packet.data() + offset, sizeof(tag));
        const size_t loops = tag & 0x7fffu;
        const size_t registers = (tag >> 60) ? (tag >> 60) : 16u;
        const uint32_t format = (tag >> 58) & 3u;
        const size_t payload = format == 0 ? loops * registers * 16u :
                               format == 1 ? ((loops * registers * 8u + 15u) & ~size_t(15u)) :
                                             loops * 16u;
        // Upstream treats both IMAGE and IMAGE_RESERVED as image data.
        if (payload > packet.size() - offset - 16u)
            throw std::invalid_argument("Truncated GIF tag payload");
        offset += 16u + payload;
    }

    impl->sendGif(packet, path);
    ++impl->totals.packets;
}

void ParallelDevice::writeRegister(uint8_t address, uint64_t value)
{
    std::lock_guard lock(impl->mutex); impl->requireReady();
    if (address > 127) throw std::invalid_argument("Invalid GS register address");
    impl->gs.write_register(static_cast<ParallelGS::RegisterAddr>(address), value);
}

void ParallelDevice::submitGifStream(std::span<const uint8_t> chunk, uint32_t path)
{
    std::lock_guard lock(impl->mutex); impl->requireReady();
    if (path < 1 || path > 3 || chunk.empty() || chunk.size() % 16 != 0 ||
        chunk.size() > 64u * 1024u * 1024u)
        throw std::invalid_argument("Invalid GIF stream chunk extent or path");
    size_t pending = impl->pendingQwords[path];
    for (size_t offset = 0; offset < chunk.size();)
    {
        if (pending)
        {
            const size_t count = std::min(pending, (chunk.size() - offset) / 16u);
            pending -= count;
            offset += count * 16u;
        }
        else
        {
            uint64_t tag;
            std::memcpy(&tag, chunk.data() + offset, sizeof(tag));
            const size_t loops = tag & 0x7fffu;
            const size_t registers = (tag >> 60) ? (tag >> 60) : 16u;
            const uint32_t format = (tag >> 58) & 3u;
            pending = format == 0 ? loops * registers :
                      format == 1 ? (loops * registers + 1u) / 2u : loops;
            offset += 16u;
        }
    }
    impl->sendGif(chunk, path);
    impl->pendingQwords[path] = pending;
    ++impl->totals.streamChunks;
}

void ParallelDevice::uploadVram(size_t offset, std::span<const uint8_t> bytes)
{
    std::lock_guard lock(impl->mutex); impl->requireReady(); Impl::range(offset, bytes.size());
    if (bytes.empty()) return;
    std::memcpy(impl->gs.map_vram_write(offset, bytes.size()), bytes.data(), bytes.size());
    impl->gs.end_vram_write(offset, bytes.size()); ++impl->totals.uploads;
}

std::vector<uint8_t> ParallelDevice::readVram(size_t offset, size_t bytes)
{
    std::lock_guard lock(impl->mutex); impl->requireReady(); Impl::range(offset, bytes);
    if (!bytes) return {};
    impl->gs.flush();
    const auto *mapped = static_cast<const uint8_t *>(impl->gs.map_vram_read(offset, bytes));
    std::vector<uint8_t> result(mapped, mapped + bytes);
    impl->account(); ++impl->totals.readbacks; impl->totals.readbackBytes += bytes;
    return result;
}

void ParallelDevice::flush()
{
    std::lock_guard lock(impl->mutex); impl->requireReady();
    impl->gs.flush(); impl->account();
}

void ParallelDevice::nextFrame()
{
    std::lock_guard lock(impl->mutex); impl->requireReady();
    impl->gs.flush(); impl->account(); impl->device.next_frame_context();
}

std::string ParallelDevice::deviceName() const
{
    std::lock_guard lock(impl->mutex); impl->requireReady();
    return impl->device.get_gpu_properties().deviceName;
}

ParallelDeviceStats ParallelDevice::stats() const
{
    std::lock_guard lock(impl->mutex); return impl->totals;
}
}

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace ps2native::gs
{
struct ParallelDeviceStats
{
    uint64_t packets = 0;
    uint64_t streamChunks = 0;
    uint64_t primitives = 0;
    uint64_t renderPasses = 0;
    uint64_t uploads = 0;
    uint64_t readbacks = 0;
    uint64_t readbackBytes = 0;
};

// Owns one Vulkan device and the upstream GS renderer. No guest CPU execution
// and no window/presentation are provided by this transport. Calls are serialized.
// Initialization requires a physical GPU; software Vulkan is a diagnostic error.
class ParallelDevice final
{
public:
    static constexpr size_t VramBytes = 4u * 1024u * 1024u;
    ParallelDevice();
    ~ParallelDevice();
    ParallelDevice(const ParallelDevice &) = delete;
    ParallelDevice &operator=(const ParallelDevice &) = delete;

    void initialize();
    void resetRegisters(); // Keeps VRAM; does not change the guest clock.
    // Complete tags and payloads, paths 1..3, <=64 MiB. Invalid input is rejected
    // before any register/VRAM changes; unaligned caller storage is supported.
    void submitGif(std::span<const uint8_t> packet, uint32_t path = 3);
    // Qword-aligned fragments of a GIF path; tag/payload state spans calls.
    void submitGifStream(std::span<const uint8_t> chunk, uint32_t path);
    void writeRegister(uint8_t address, uint64_t value);
    void uploadVram(size_t offset, std::span<const uint8_t> bytes);
    std::vector<uint8_t> readVram(size_t offset, size_t bytes);
    void flush();
    void nextFrame();
    std::string deviceName() const;
    ParallelDeviceStats stats() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
}

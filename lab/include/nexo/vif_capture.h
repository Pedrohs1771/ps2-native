#pragma once
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <vector>

class PS2Memory;
class PS2Runtime;
enum class GifPathId : uint8_t;

namespace ps2native::nexo
{
struct VuNativeProgram;
// Laboratory binding only; no public runtime/memory layout changes. A reset
// of parser state does not erase the owner. Memory destruction does.
void bindVifCapture(PS2Runtime &runtime);
void unbindVifCapture(PS2Memory &memory) noexcept;

class VifPresentationScope
{
    struct Impl;
    std::unique_ptr<Impl> impl;
public:
    explicit VifPresentationScope(PS2Runtime &runtime);
    ~VifPresentationScope();
};

// Observe the real parser callbacks and submissions without replacing any
// callback. The observer belongs to the current thread and memory instance.
class VifObservation
{
    struct Impl;
    std::unique_ptr<Impl> impl;
    friend void observeVifVuCall(PS2Memory &, uint8_t, uint32_t, uint32_t, uint32_t);
    friend void observeVifGifSubmission(PS2Memory &, GifPathId, const uint8_t *, uint32_t, bool, bool);
    friend void observeVifGifDelivery(PS2Memory &, const uint8_t *, uint32_t);
    friend class VifCaptureScope;
public:
    explicit VifObservation(PS2Runtime &runtime, bool strict = true);
    ~VifObservation();
    std::vector<uint8_t> events() const;
    const std::vector<std::vector<uint8_t>> &codeBanks() const;
    uint32_t vuCalls() const;
    bool failed() const;
};

class VifCaptureScope
{
    struct Impl;
    std::unique_ptr<Impl> impl;
public:
    VifCaptureScope(PS2Memory &memory, std::span<const uint8_t> input) noexcept;
    ~VifCaptureScope();
};
void observeVifVuCall(PS2Memory &memory, uint8_t opcode, uint32_t pc, uint32_t top, uint32_t itop);
void observeVifGifSubmission(PS2Memory &memory, GifPathId path, const uint8_t *data,
    uint32_t bytes, bool drain, bool directHl);
void observeVifGifDelivery(PS2Memory &memory, const uint8_t *data, uint32_t bytes);

// Composite boundary of the current VIF1/VU1/GIF/CPU-GS model. Other machine
// components and external writers must remain quiescent. See the schema.
std::vector<uint8_t> encodeVifBoundary(PS2Runtime &runtime);
struct VifReplayResult
{
    std::vector<uint8_t> state, events;
    std::vector<std::vector<uint8_t>> codeBanks;
    uint64_t executionNanoseconds = 0;
};
VifReplayResult replayVifCase(const std::filesystem::path &directory);
VifReplayResult replayVifCaseNative(const std::filesystem::path &directory,
    std::span<const VuNativeProgram> programs);
}

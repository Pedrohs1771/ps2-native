#include "nexo/vu_replay.h"
#include "nexo/vu_snapshot.h"
#include "nexo/vu_native.h"
#include "vu_replay_internal.h"
#include "runtime/gs/gs_frontend.h"
#include "runtime/ps2_memory.h"
#include "runtime/ps2_vu1.h"

#include <algorithm>
#include <cfenv>
#include <chrono>
#include <cstring>
#include <fstream>
#include <functional>
#include <limits>
#include <memory>
#include <stdexcept>

namespace ps2native::nexo
{
namespace
{
std::vector<uint8_t> readBounded(const std::filesystem::path &path, size_t minimum, size_t maximum)
{
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    if (error || size < minimum || size > maximum)
        throw std::invalid_argument("missing or invalid replay blob: " + path.string());
    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    std::ifstream stream(path, std::ios::binary);
    stream.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!stream || stream.peek() != std::char_traits<char>::eof())
        throw std::invalid_argument("cannot read exact replay blob: " + path.string());
    return bytes;
}
}

namespace detail
{
VuReplayResult replayOnMachine(const std::filesystem::path &directory, uint32_t cycleBudget,
    VU1Interpreter &vuState, PS2Memory &memoryState, GS &gsState,
    const ReplayExecutor &executor, const VuNativeProgram *identity)
{
    if (cycleBudget == 0 || cycleBudget > 1048576)
        throw std::invalid_argument("cycle budget must be 1..1048576");
    const auto input = readBounded(directory / "input-state.nexo", 24, 80 * 1024);
    const auto code = readBounded(directory / "code.bin", PS2_VU1_CODE_SIZE, PS2_VU1_CODE_SIZE);
    const auto data = readBounded(directory / "input-data.bin", PS2_VU1_DATA_SIZE, PS2_VU1_DATA_SIZE);
    if (identity && (identity->unit != VU1Interpreter::Unit::VU1 ||
        identity->codeIdentity.size() != code.size() ||
        !std::equal(code.begin(), code.end(), identity->codeIdentity.begin())))
        throw std::runtime_error("UNSEEN_CODE VU bank identity mismatch");
    auto *vu = &vuState;
    auto *memory = &memoryState;
    auto *gs = &gsState;
    if (!memory->getVU1Code() || !memory->getVU1Data())
        throw std::invalid_argument("VU replay memory is not initialized");
    VuSnapshotCodec::restore(*vu, input);
    const auto startCycle = vu->state().cycles;
    if (startCycle > std::numeric_limits<uint64_t>::max() - cycleBudget - 64u)
        throw std::invalid_argument("VU replay clock would overflow");
    std::memcpy(memory->getVU1Code(), code.data(), code.size());
    std::memcpy(memory->getVU1Data(), data.data(), data.size());
    memory->markVU1CodeModified();
    VuReplayResult result;
    result.path1Events = {'N','E','X','O','G','I','F',0, 1,0,0,0, 0,0,0,0};
    uint32_t count = 0;
    memory->setGifPacketCallback([&](const uint8_t *bytes, uint32_t size)
    {
        if (result.path1Events.size() + 12u + size > 64u * 1024u * 1024u)
            throw std::invalid_argument("VU replay PATH1 output exceeds its bound");
        const auto append = [&result](uint64_t value, unsigned width)
        {
            for (unsigned i = 0; i < width; ++i) result.path1Events.push_back(uint8_t(value >> (8 * i)));
        };
        append(vu->state().cycles - startCycle, 8);
        append(size, 4);
        result.path1Events.insert(result.path1Events.end(), bytes, bytes + size);
        ++count;
        for (unsigned i = 0; i < 4; ++i) result.path1Events[12 + i] = uint8_t(count >> (8 * i));
    });
    struct CallbackScope
    {
        PS2Memory &memory;
        ~CallbackScope() { memory.setGifPacketCallback({}); }
    } callbackScope{*memory};
    struct RoundingScope
    {
        int previous = std::fegetround();
        ~RoundingScope() { if (previous != -1) std::fesetround(previous); }
    } rounding;
    const auto begin = std::chrono::steady_clock::now();
    executor(*vu, *memory, *gs, cycleBudget);
    result.executionNanoseconds = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - begin).count());
    result.state = VuSnapshotCodec::encode(*vu);
    result.data.assign(memory->getVU1Data(), memory->getVU1Data() + PS2_VU1_DATA_SIZE);
    return result;
}
}

namespace
{
struct BaselineMachine
{
    VU1Interpreter vu;
    PS2Memory memory;
    GS gs;
    BaselineMachine()
    {
        if (!memory.initialize()) throw std::runtime_error("cannot initialize VU replay memory");
        gs.init(memory.getGSVRAM(), PS2_GS_VRAM_SIZE, &memory.gs());
    }
};
}

VuReplayResult replayVuCapture(const std::filesystem::path &directory, uint32_t cycleBudget)
{
    auto machine = std::make_unique<BaselineMachine>();
    return detail::replayOnMachine(directory, cycleBudget, machine->vu, machine->memory, machine->gs,
        [](VU1Interpreter &vu, PS2Memory &memory, GS &gs, uint32_t budget)
    {
        vu.resume(memory.getVU1Code(), PS2_VU1_CODE_SIZE, memory.getVU1Data(), PS2_VU1_DATA_SIZE,
            gs, &memory, vu.state().top, vu.state().itop, budget);
    }, nullptr);
}

VuReplayResult replayVuCaptureNative(const std::filesystem::path &directory, uint32_t cycleBudget,
    const VuNativeProgram &program)
{
    auto machine = std::make_unique<BaselineMachine>();
    return detail::replayOnMachine(directory, cycleBudget, machine->vu, machine->memory, machine->gs,
        [&program](VU1Interpreter &vu, PS2Memory &memory, GS &gs, uint32_t budget)
    {
        VuNativeAccess::resume(vu, program, memory.getVU1Data(), PS2_VU1_DATA_SIZE,
            gs, &memory, vu.state().top, vu.state().itop, budget);
    }, &program);
}
}

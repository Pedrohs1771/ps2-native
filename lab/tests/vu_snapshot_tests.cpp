#include "MiniTest.h"
#include "nexo/vu_snapshot.h"
#include "nexo/vu_replay.h"
#include "runtime/gs/gs_frontend.h"
#include "runtime/ps2_memory.h"
#include "runtime/ps2_vu1.h"

#include <algorithm>
#include <bit>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>

using ps2native::nexo::VuSnapshotCodec;

namespace
{
constexpr uint32_t nop = 0x000002FFu;
constexpr uint32_t lowerNop = 0x8000033Cu;

void word(uint8_t *destination, uint32_t value)
{
    for (unsigned i = 0; i < 4; ++i) destination[i] = static_cast<uint8_t>(value >> (8 * i));
}

void pair(uint8_t *code, uint32_t address, uint32_t lower, uint32_t upper = nop)
{
    word(code + address, lower);
    word(code + address + 4, upper);
}

uint32_t upper(uint8_t op, uint8_t mask, uint8_t ft, uint8_t fs, uint8_t fd)
{
    return (uint32_t(mask) << 21) | (uint32_t(ft) << 16) |
        (uint32_t(fs) << 11) | (uint32_t(fd) << 6) | op;
}

uint32_t special(uint8_t op, uint8_t is, uint8_t it = 0, uint8_t dest = 0)
{
    return (0x40u << 25) | (uint32_t(dest) << 21) | (uint32_t(it) << 16) |
        (uint32_t(is) << 11) | (uint32_t(op & 0x7Cu) << 4) | (op & 3u) | 0x3Cu;
}

struct Fixture
{
    PS2Memory memory;
    GS gs;
    std::vector<std::vector<uint8_t>> packets;

    Fixture()
    {
        if (!memory.initialize()) throw std::runtime_error("cannot initialize snapshot fixture");
        gs.init(memory.getGSVRAM(), PS2_GS_VRAM_SIZE, &memory.gs());
        std::memset(memory.getVU1Code(), 0, PS2_VU1_CODE_SIZE);
        std::memset(memory.getVU1Data(), 0, PS2_VU1_DATA_SIZE);
        memory.setGifPacketCallback([this](const uint8_t *bytes, uint32_t size)
        {
            packets.emplace_back(bytes, bytes + size);
        });
    }

    void copyInputFrom(const Fixture &other)
    {
        std::memcpy(memory.getVU1Code(), other.memory.getVU1Code(), PS2_VU1_CODE_SIZE);
        std::memcpy(memory.getVU1Data(), other.memory.getVU1Data(), PS2_VU1_DATA_SIZE);
        memory.markVU1CodeModified();
    }

    void run(VU1Interpreter &vu, uint32_t cycles, bool fresh)
    {
        if (fresh)
            vu.execute(memory.getVU1Code(), PS2_VU1_CODE_SIZE, memory.getVU1Data(),
                PS2_VU1_DATA_SIZE, gs, &memory, 0, 7, 9, cycles);
        else
            vu.resume(memory.getVU1Code(), PS2_VU1_CODE_SIZE, memory.getVU1Data(),
                PS2_VU1_DATA_SIZE, gs, &memory, 7, 9, cycles);
    }
};

struct CaptureDirectory
{
    std::filesystem::path path;
    std::optional<std::string> previous;

    CaptureDirectory()
    {
        if (const auto *value = std::getenv("PS2X_CAPTURE_SCENE")) previous = value;
        path = std::filesystem::temp_directory_path() /
            ("nexo-vu-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directory(path);
        set(path.string());
        std::ofstream(path / ".vu-request").put('\n');
    }

    static void set(const std::string &value)
    {
#ifdef _WIN32
        if (_putenv_s("PS2X_CAPTURE_SCENE", value.c_str()) != 0)
#else
        if (setenv("PS2X_CAPTURE_SCENE", value.c_str(), 1) != 0)
#endif
            throw std::runtime_error("cannot configure capture fixture");
    }

    ~CaptureDirectory()
    {
#ifdef _WIN32
        _putenv_s("PS2X_CAPTURE_SCENE", previous ? previous->c_str() : "");
#else
        if (previous) setenv("PS2X_CAPTURE_SCENE", previous->c_str(), 1);
        else unsetenv("PS2X_CAPTURE_SCENE");
#endif
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }

    std::vector<std::filesystem::path> captures() const
    {
        std::vector<std::filesystem::path> result;
        for (const auto &entry : std::filesystem::directory_iterator(path))
            if (entry.is_directory()) result.push_back(entry.path());
        return result;
    }
};

std::vector<uint8_t> read(const std::filesystem::path &path)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw std::runtime_error("cannot read capture file " + path.string());
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

void reseal(std::vector<uint8_t> &bytes)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < bytes.size(); ++i)
    {
        if (i >= 20 && i < 24) continue;
        crc ^= bytes[i];
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    word(bytes.data() + 20, ~crc);
}

void checkCapture(TestCase &t, const std::filesystem::path &directory,
    const Fixture &originalMemory, const VU1Interpreter &original, uint32_t cycles)
{
    const auto inputPath = directory / "input-state.nexo";
    const auto outputPath = directory / "output-state.nexo";
    t.IsTrue(std::filesystem::exists(inputPath), "Captures canonical input with hidden pipelines");
    t.IsTrue(std::filesystem::exists(outputPath), "Captures canonical output with hidden pipelines");
    if (!std::filesystem::exists(inputPath) || !std::filesystem::exists(outputPath)) return;
    auto replayMemory = std::make_unique<Fixture>();
    const auto code = read(directory / "code.bin");
    const auto data = read(directory / "input-data.bin");
    t.Equals(code.size(), size_t(PS2_VU1_CODE_SIZE), "Captures the complete microcode memory");
    t.Equals(data.size(), size_t(PS2_VU1_DATA_SIZE), "Captures the complete VU data memory");
    if (code.size() != PS2_VU1_CODE_SIZE || data.size() != PS2_VU1_DATA_SIZE) return;
    std::memcpy(replayMemory->memory.getVU1Code(), code.data(), code.size());
    std::memcpy(replayMemory->memory.getVU1Data(), data.data(), data.size());
    auto replay = std::make_unique<VU1Interpreter>();
    VuSnapshotCodec::restore(*replay, read(inputPath));
    replayMemory->run(*replay, cycles, false);
    t.Equals(VuSnapshotCodec::encode(*replay), read(outputPath), "Captured state replays to the exact output");
    t.Equals(read(outputPath), VuSnapshotCodec::encode(original), "Output snapshot is the actual live state");
    const std::vector<uint8_t> expectedData(originalMemory.memory.getVU1Data(),
        originalMemory.memory.getVU1Data() + PS2_VU1_DATA_SIZE);
    t.Equals(read(directory / "output-data.bin"), expectedData, "Captured output data is the actual VU memory");
    const auto result = ps2native::nexo::replayVuCapture(directory, cycles);
    t.Equals(result.state, read(outputPath), "Standalone replay preserves the canonical state");
    t.Equals(result.data, expectedData, "Standalone replay preserves memory effects");
    t.Equals(result.path1Events, read(directory / "path1-events.nexo"),
        "Standalone replay preserves timed PATH1 submissions");
}

void compareContinuation(TestCase &t, Fixture &originalMemory, VU1Interpreter &original,
    uint32_t cycles)
{
    const auto checkpoint = VuSnapshotCodec::encode(original);
    t.IsTrue(!checkpoint.empty(), "Writes a canonical checkpoint");
    auto restoredMemory = std::make_unique<Fixture>();
    restoredMemory->copyInputFrom(originalMemory);
    auto restored = std::make_unique<VU1Interpreter>();
    VuSnapshotCodec::restore(*restored, checkpoint);
    t.Equals(VuSnapshotCodec::encode(*restored), checkpoint, "Re-encoding is byte-identical");
    originalMemory.packets.clear();
    originalMemory.run(original, cycles, false);
    restoredMemory->run(*restored, cycles, false);
    t.Equals(VuSnapshotCodec::encode(*restored), VuSnapshotCodec::encode(original),
        "All registers, clocks, hidden pipelines and pending effects continue identically");
    t.IsTrue(std::memcmp(originalMemory.memory.getVU1Data(), restoredMemory->memory.getVU1Data(),
        PS2_VU1_DATA_SIZE) == 0, "Preserves data-memory effects");
    t.Equals(restoredMemory->packets, originalMemory.packets, "Preserves future PATH1 packets");
}
}

int main()
{
    MiniTest::Case("NEXO canonical VU snapshots", [](TestCase &tc)
    {
        tc.Run("MSCAL diagnostics include portable before and after checkpoints", [](TestCase &t)
        {
            auto fx = std::make_unique<Fixture>();
            auto vu = std::make_unique<VU1Interpreter>();
            pair(fx->memory.getVU1Code(), 0, lowerNop, upper(0x28, 0x8, 2, 1, 3));
            vu->state().vf[1][0] = 4;
            vu->state().vf[2][0] = 7;
            CaptureDirectory capture;
            fx->run(*vu, 5, true);
            const auto directories = capture.captures();
            t.Equals(directories.size(), size_t(1), "Fresh call consumes the capture request once");
            if (directories.size() != 1) return;
            checkCapture(t, directories.front(), *fx, *vu, 5);
            fx->run(*vu, 1, false);
            t.Equals(capture.captures().size(), size_t(1), "No additional capture without another request");
        });

        tc.Run("MSCNT diagnostics capture the pending pipeline before resuming", [](TestCase &t)
        {
            auto fx = std::make_unique<Fixture>();
            auto vu = std::make_unique<VU1Interpreter>();
            pair(fx->memory.getVU1Code(), 0, lowerNop, upper(0x28, 0x8, 2, 1, 3));
            vu->state().vf[1][0] = 4;
            vu->state().vf[2][0] = 7;
            fx->run(*vu, 1, true);
            const auto paused = VuSnapshotCodec::encode(*vu);
            CaptureDirectory capture;
            fx->run(*vu, 5, false);
            const auto directories = capture.captures();
            t.Equals(directories.size(), size_t(1), "Continuation consumes the capture request");
            if (directories.size() != 1) return;
            checkCapture(t, directories.front(), *fx, *vu, 5);
            if (std::filesystem::exists(directories.front() / "input-state.nexo"))
                t.Equals(read(directories.front() / "input-state.nexo"), paused,
                    "MSCNT input includes pending writes instead of resetting them");
            t.Equals(vu->state().vf[3][0], 11.0f, "Continuation publishes the pending FMAC result");
        });

        tc.Run("MSCNT capture records causal PATH1 submissions from an in-flight XGKICK", [](TestCase &t)
        {
            auto fx = std::make_unique<Fixture>();
            auto vu = std::make_unique<VU1Interpreter>();
            const uint64_t tag = 1u | (1ull << 15) | (2ull << 58);
            for (unsigned i = 0; i < 8; ++i) fx->memory.getVU1Data()[i] = uint8_t(tag >> (8 * i));
            std::memset(fx->memory.getVU1Data() + 16, 0xAC, 16);
            pair(fx->memory.getVU1Code(), 0, special(0x6C, 1));
            fx->run(*vu, 1, true);
            CaptureDirectory capture;
            fx->run(*vu, 4, false);
            const auto directories = capture.captures();
            t.Equals(directories.size(), size_t(1), "Captures the resumed XGKICK");
            if (directories.size() != 1) return;
            const auto eventsPath = directories.front() / "path1-events.nexo";
            t.IsTrue(std::filesystem::exists(eventsPath), "Captures a portable PATH1 event stream");
            if (!std::filesystem::exists(eventsPath)) return;
            const auto events = read(eventsPath);
            t.Equals(fx->packets.size(), size_t(1), "The in-flight packet is submitted once");
            if (fx->packets.size() != 1) return;
            std::vector<uint8_t> expected{'N','E','X','O','G','I','F',0, 1,0,0,0, 1,0,0,0,
                2,0,0,0,0,0,0,0, 32,0,0,0};
            expected.insert(expected.end(), fx->packets.front().begin(), fx->packets.front().end());
            t.Equals(events, expected, "Preserves the submission cycle, size, ordering and raw packet bytes");
            checkCapture(t, directories.front(), *fx, *vu, 4);
        });

        tc.Run("fixed little-endian fields preserve every float bit pattern", [](TestCase &t)
        {
            auto vu = std::make_unique<VU1Interpreter>();
            for (unsigned r = 0; r < 32; ++r)
                for (unsigned lane = 0; lane < 4; ++lane)
                    vu->state().vf[r][lane] = std::bit_cast<float>(0x80000000u | (r * 4 + lane));
            vu->state().vf[0][0] = std::bit_cast<float>(0x7FC12345u);
            vu->state().vi[7] = -12345;
            const auto bytes = VuSnapshotCodec::encode(*vu);
            t.IsTrue(bytes.size() > 65000, "Includes the fixed-width XGKICK buffer and hidden state");
            if (bytes.size() < 28) return;
            const std::vector<uint8_t> magic{'N', 'E', 'X', 'O', 'V', 'U', 0, 0};
            t.IsTrue(std::equal(magic.begin(), magic.end(), bytes.begin()), "Stable format magic");
            t.IsTrue(bytes[8] == 1 && bytes[9] == 0 && bytes[10] == 0 && bytes[11] == 0,
                "Version is a little-endian uint32");
            t.IsTrue(bytes[24] == 0x45 && bytes[25] == 0x23 && bytes[26] == 0xC1 && bytes[27] == 0x7F,
                "NaN payload uses raw little-endian bits, without host padding");
            auto target = std::make_unique<VU1Interpreter>();
            VuSnapshotCodec::restore(*target, bytes);
            t.Equals(std::bit_cast<uint32_t>(target->state().vf[0][0]), 0x7FC12345u,
                "Preserves NaN payload exactly");
            t.Equals(target->state().vi[7], -12345, "Preserves signed register bits");
            t.Equals(VuSnapshotCodec::encode(*target), bytes, "Canonical byte round trip");
        });

        tc.Run("pending FMAC destinations and flags survive an MSCNT checkpoint", [](TestCase &t)
        {
            auto fx = std::make_unique<Fixture>();
            auto vu = std::make_unique<VU1Interpreter>();
            pair(fx->memory.getVU1Code(), 0, lowerNop, upper(0x28, 0xF, 2, 1, 3));
            for (unsigned lane = 0; lane < 4; ++lane)
            {
                vu->state().vf[1][lane] = 1.0f + lane;
                vu->state().vf[2][lane] = 10.0f + lane;
            }
            fx->run(*vu, 1, true);
            t.Equals(vu->state().vf[3][0], 0.0f, "The destination is still pending at the checkpoint");
            compareContinuation(t, *fx, *vu, 5);
            t.Equals(vu->state().vf[3][0], 11.0f, "Pending FMAC becomes visible on schedule");
        });

        tc.Run("pending Q and its scalar status survive a checkpoint", [](TestCase &t)
        {
            auto fx = std::make_unique<Fixture>();
            auto vu = std::make_unique<VU1Interpreter>();
            pair(fx->memory.getVU1Code(), 0, special(0x38, 1, 2));
            pair(fx->memory.getVU1Code(), 8, special(0x3B, 0));
            vu->state().vf[1][0] = 12.0f;
            vu->state().vf[2][0] = 2.0f;
            fx->run(*vu, 1, true);
            t.Equals(vu->state().q, 1.0f, "Q is not yet published");
            compareContinuation(t, *fx, *vu, 8);
            t.Equals(vu->state().q, 6.0f, "FDIV publishes the correct result after restore");
        });

        tc.Run("EFU throughput and both pending P slots survive restore", [](TestCase &t)
        {
            auto fx = std::make_unique<Fixture>();
            auto vu = std::make_unique<VU1Interpreter>();
            pair(fx->memory.getVU1Code(), 0, special(0x70, 1));
            pair(fx->memory.getVU1Code(), 8, special(0x72, 1));
            pair(fx->memory.getVU1Code(), 16, special(0x7B, 0));
            vu->state().vf[1][0] = 1;
            vu->state().vf[1][1] = 2;
            vu->state().vf[1][2] = 3;
            fx->run(*vu, 11, true);
            t.Equals(vu->state().p, 14.0f, "First P result commits while the second is still pending");
            compareContinuation(t, *fx, *vu, 18);
            t.IsTrue(vu->state().p > 3.7f && vu->state().p < 3.8f,
                "Second EFU result commits after the restored resource latency");
        });

        tc.Run("pending integer load and accumulator flag dependencies survive restore", [](TestCase &t)
        {
            auto fx = std::make_unique<Fixture>();
            auto vu = std::make_unique<VU1Interpreter>();
            word(fx->memory.getVU1Data() + 32, 0x1234);
            const uint32_t adda = (0x8u << 21) | (2u << 16) | (1u << 11) |
                (uint32_t(0x28 & 0x7C) << 4) | (0x28 & 3) | 0x3C;
            pair(fx->memory.getVU1Code(), 0,
                (0x04u << 25) | (0x8u << 21) | (2u << 16) | (1u << 11), adda);
            pair(fx->memory.getVU1Code(), 8,
                (0x08u << 25) | (3u << 16) | (2u << 11) | 1,
                upper(0x29, 0x8, 4, 3, 5));
            vu->state().vi[1] = 2;
            vu->state().vf[1][0] = 1;
            vu->state().vf[2][0] = 10;
            vu->state().vf[3][0] = 2;
            vu->state().vf[4][0] = 3;
            fx->run(*vu, 1, true);
            t.Equals(vu->state().vi[2], 0, "Integer load is still in flight");
            compareContinuation(t, *fx, *vu, 8);
            t.Equals(vu->state().vi[3], 0x1235, "Load-to-IALU dependency is preserved");
            t.Equals(vu->state().vf[5][0], 17.0f, "ACC forwarding and FMAC flags are preserved");
        });

        tc.Run("branch delay and consecutive VI write history survive restore", [](TestCase &t)
        {
            auto fx = std::make_unique<Fixture>();
            auto vu = std::make_unique<VU1Interpreter>();
            pair(fx->memory.getVU1Code(), 0, (0x08u << 25) | (1u << 16) | (1u << 11) | 1u);
            pair(fx->memory.getVU1Code(), 8, (0x08u << 25) | (1u << 16) | (1u << 11) | 1u);
            pair(fx->memory.getVU1Code(), 16, (0x29u << 25) | (2u << 16) | (1u << 11) | 3u);
            pair(fx->memory.getVU1Code(), 24, lowerNop);
            vu->state().vi[1] = 4;
            vu->state().vi[2] = 4;
            fx->run(*vu, 2, true);
            compareContinuation(t, *fx, *vu, 3);
        });

        tc.Run("an in-flight XGKICK preserves buffered bytes and future reads", [](TestCase &t)
        {
            auto fx = std::make_unique<Fixture>();
            auto vu = std::make_unique<VU1Interpreter>();
            const uint64_t tag = 1u | (1ull << 15) | (2ull << 58);
            for (unsigned i = 0; i < 8; ++i) fx->memory.getVU1Data()[i] = uint8_t(tag >> (8 * i));
            std::memset(fx->memory.getVU1Data() + 16, 0xAC, 16);
            pair(fx->memory.getVU1Code(), 0, special(0x6C, 1));
            pair(fx->memory.getVU1Code(), 8, lowerNop);
            fx->run(*vu, 1, true);
            t.IsTrue(fx->packets.empty(), "The packet has not been published at the checkpoint");
            compareContinuation(t, *fx, *vu, 4);
            t.Equals(fx->packets.size(), size_t(1), "The active transfer completes after resume");
            if (!fx->packets.empty())
                t.Equals(fx->packets.front()[16], uint8_t(0xAC), "Packet payload remains intact");
        });

        tc.Run("corruption truncation trailing bytes and version mismatch cannot alter state", [](TestCase &t)
        {
            auto vu = std::make_unique<VU1Interpreter>();
            vu->state().vi[5] = 4321;
            const auto good = VuSnapshotCodec::encode(*vu);
            t.IsTrue(!good.empty(), "Produces a nonempty snapshot");
            if (good.empty()) return;
            std::vector<std::vector<uint8_t>> bad;
            bad.push_back({});
            bad.emplace_back(good.begin(), good.begin() + 16);
            bad.emplace_back(good.begin(), good.end() - 1);
            bad.push_back(good); bad.back().push_back(0);
            bad.push_back(good); bad.back()[8] = 99;
            bad.push_back(good); bad.back().back() ^= 0x20;
            for (const auto &bytes : bad)
            {
                bool rejected = false;
                try { VuSnapshotCodec::restore(*vu, bytes); }
                catch (const std::invalid_argument &) { rejected = true; }
                t.IsTrue(rejected, "Rejects malformed snapshot before changing the live VU");
                t.Equals(VuSnapshotCodec::encode(*vu), good, "Failed restore leaves state byte-identical");
            }
        });

        tc.Run("a checksum-valid impossible scalar deadline is rejected before execution", [](TestCase &t)
        {
            auto vu = std::make_unique<VU1Interpreter>();
            const auto good = VuSnapshotCodec::encode(*vu);
            auto impossible = good;
            // Version-1 schema: 24-byte header, 655 architectural bytes,
            // eight 37-byte flag entries, then FDIV ready/value/status/valid.
            constexpr size_t fdiv = 24 + 655 + 8 * 37;
            std::fill_n(impossible.begin() + fdiv, 8, uint8_t(0xFF));
            impossible[fdiv + 16] = 1;
            reseal(impossible);
            bool rejected = false;
            try { VuSnapshotCodec::restore(*vu, impossible); }
            catch (const std::invalid_argument &) { rejected = true; }
            t.IsTrue(rejected, "An impossible deadline cannot drive an unbounded pipeline flush");
            t.Equals(VuSnapshotCodec::encode(*vu), good, "Malformed deadline leaves the live VU unchanged");
        });

        tc.Run("VU0 and VU1 snapshots cannot be restored into the wrong unit", [](TestCase &t)
        {
            auto source = std::make_unique<VU1Interpreter>(VU1Interpreter::Unit::VU0);
            auto target = std::make_unique<VU1Interpreter>();
            const auto initial = VuSnapshotCodec::encode(*target);
            t.IsTrue(!initial.empty(), "Unit test uses a real encoded snapshot");
            bool rejected = false;
            try { VuSnapshotCodec::restore(*target, VuSnapshotCodec::encode(*source)); }
            catch (const std::invalid_argument &) { rejected = true; }
            t.IsTrue(rejected, "Unit identity is part of the checkpoint contract");
            t.Equals(VuSnapshotCodec::encode(*target), initial, "Unit mismatch is transactional");
        });

        tc.Run("restoration invalidates derived decode caches and rebinds host memory", [](TestCase &t)
        {
            auto sourceMemory = std::make_unique<Fixture>();
            auto targetMemory = std::make_unique<Fixture>();
            auto source = std::make_unique<VU1Interpreter>();
            auto target = std::make_unique<VU1Interpreter>();
            pair(sourceMemory->memory.getVU1Code(), 0, lowerNop);
            pair(sourceMemory->memory.getVU1Code(), 8, (0x08u << 25) | (3u << 16) | 9);
            pair(targetMemory->memory.getVU1Code(), 0, lowerNop);
            pair(targetMemory->memory.getVU1Code(), 8, (0x08u << 25) | (3u << 16) | 111);
            sourceMemory->run(*source, 1, true);
            targetMemory->run(*target, 1, true); // Populate a different cache at the same target pointer.
            std::memcpy(targetMemory->memory.getVU1Code(), sourceMemory->memory.getVU1Code(), PS2_VU1_CODE_SIZE);
            // Deliberately do not change the generation: restoring a checkpoint
            // must discard host-derived caches even if their pointers still match.
            VuSnapshotCodec::restore(*target, VuSnapshotCodec::encode(*source));
            sourceMemory->run(*source, 1, false);
            targetMemory->run(*target, 1, false);
            t.Equals(target->state().vi[3], 9, "Does not execute a stale decoded instruction");
            t.Equals(VuSnapshotCodec::encode(*target), VuSnapshotCodec::encode(*source),
                "Future execution binds to the restored host machine");
        });
    });
    return MiniTest::Run();
}

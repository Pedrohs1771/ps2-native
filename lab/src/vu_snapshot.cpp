#include "nexo/vu_snapshot.h"
#include "runtime/ps2_vu1.h"

#include <algorithm>
#include <array>
#include <bit>
#include <concepts>
#include <limits>
#include <memory>
#include <stdexcept>
#include <type_traits>

namespace ps2native::nexo
{
namespace
{
constexpr std::array<uint8_t, 8> magic{'N', 'E', 'X', 'O', 'V', 'U', 0, 0};
constexpr size_t headerSize = 24;
constexpr size_t maximumSize = 80 * 1024;
static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);

uint32_t checksum(std::span<const uint8_t> bytes)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < bytes.size(); ++i)
    {
        if (i >= 20 && i < 24) continue; // The checksum field itself.
        crc ^= bytes[i];
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

struct Writer
{
    std::vector<uint8_t> bytes;

    template <std::integral T> requires (!std::same_as<T, bool>)
    void operator()(T value)
    {
        const auto bits = std::bit_cast<std::make_unsigned_t<T>>(value);
        for (unsigned i = 0; i < sizeof(T); ++i)
            bytes.push_back(static_cast<uint8_t>(bits >> (8 * i)));
    }

    void operator()(bool value) { bytes.push_back(value ? 1 : 0); }
    void operator()(float value) { (*this)(std::bit_cast<uint32_t>(value)); }

    template <typename T, size_t N> void operator()(const std::array<T, N> &values)
    {
        for (const auto &value : values) (*this)(value);
    }
    template <typename T, size_t N> void operator()(const T (&values)[N])
    {
        for (const auto &value : values) (*this)(value);
    }
};

struct Reader
{
    std::span<const uint8_t> bytes;
    size_t offset = 0;

    template <std::integral T> requires (!std::same_as<T, bool>)
    void operator()(T &value)
    {
        if (bytes.size() - offset < sizeof(T))
            throw std::invalid_argument("truncated canonical VU snapshot field");
        std::make_unsigned_t<T> bits = 0;
        for (unsigned i = 0; i < sizeof(T); ++i)
            bits |= std::make_unsigned_t<T>(bytes[offset++]) << (8 * i);
        value = std::bit_cast<T>(bits);
    }

    void operator()(bool &value)
    {
        uint8_t byte = 0;
        (*this)(byte);
        if (byte > 1) throw std::invalid_argument("noncanonical VU snapshot boolean");
        value = byte != 0;
    }
    void operator()(float &value)
    {
        uint32_t bits = 0;
        (*this)(bits);
        value = std::bit_cast<float>(bits);
    }
    template <typename T, size_t N> void operator()(std::array<T, N> &values)
    {
        for (auto &value : values) (*this)(value);
    }
    template <typename T, size_t N> void operator()(T (&values)[N])
    {
        for (auto &value : values) (*this)(value);
    }
};

void putU32(std::vector<uint8_t> &bytes, size_t offset, uint32_t value)
{
    for (unsigned i = 0; i < 4; ++i) bytes[offset + i] = static_cast<uint8_t>(value >> (8 * i));
}
}

template <typename Archive, typename Vu>
void VuSnapshotCodec::fields(Archive &a, Vu &vu)
{
    auto &s = vu.m_state;
    a(s.vf); a(s.vi); a(s.acc); a(s.q); a(s.p); a(s.i);
    a(s.r); a(s.pc); a(s.mac); a(s.clip); a(s.status); a(s.cycles);
    a(s.ebit); a(s.haltAfterDelaySlot); a(s.dBitEnabled); a(s.tBitEnabled);
    a(s.stoppedByD); a(s.stoppedByT); a(s.top); a(s.itop);
    a(s.branchPending); a(s.branchTarget); a(s.branchDelay);

    for (auto &entry : vu.m_flagPipeline)
    {
        a(entry.readyCycle); a(entry.issueCycle);
        a(entry.mac); a(entry.status); a(entry.extraSticky); a(entry.clip);
        a(entry.valid); a(entry.writesMac); a(entry.writesStatus);
        a(entry.writesSticky); a(entry.writesClip);
    }
    const auto scalar = [&a](auto &entry)
    {
        a(entry.readyCycle); a(entry.value); a(entry.statusDi); a(entry.valid);
    };
    scalar(vu.m_fdiv);
    for (auto &entry : vu.m_efu) scalar(entry);
    for (auto &entry : vu.m_storePipeline)
    {
        a(entry.readyCycle); a(entry.address); a(entry.words); a(entry.laneMask); a(entry.valid);
    }
    for (auto &entry : vu.m_vfWritePipeline)
    {
        a(entry.readyCycle); a(entry.sequence); a(entry.value);
        a(entry.reg); a(entry.laneMask); a(entry.valid);
    }
    for (auto &entry : vu.m_viWritePipeline)
    {
        a(entry.readyCycle); a(entry.sequence); a(entry.value); a(entry.reg); a(entry.valid);
    }
    for (auto &entry : vu.m_accWritePipeline)
    {
        a(entry.readyCycle); a(entry.sequence); a(entry.value); a(entry.laneMask); a(entry.valid);
    }

    auto &kick = vu.m_xgkick;
    a(kick.packet); a(kick.sourceAddress); a(kick.totalBytes); a(kick.copiedBytes);
    a(kick.currentTagEnd); a(kick.cycleCredit); a(kick.issueCycle);
    a(kick.active); a(kick.currentTagEop);

    a(vu.m_vfReady); a(vu.m_viReady); a(vu.m_accReady);
    a(vu.m_vfLatestWrite); a(vu.m_viLatestWrite); a(vu.m_accLatestWrite);
    a(vu.m_cycle); a(vu.m_nextWriteSequence); a(vu.m_efuResourceReady);
    a(vu.m_workingClip); a(vu.m_currentUpperInstruction);
    a(vu.m_viBranchBackupValue); a(vu.m_viBranchBackupReg); a(vu.m_viBranchBackupValid);
    a(vu.m_stopRequested); a(vu.m_pendingHaltD); a(vu.m_pendingHaltT);
    // No host pointers or caches: active bindings are supplied by run(), and
    // decoded code must be rebuilt for the restored machine's code identity.
}

void VuSnapshotCodec::validate(const VU1Interpreter &vu)
{
    const auto fail = [](bool invalid, const char *reason)
    {
        if (invalid) throw std::invalid_argument(reason);
    };
    fail(vu.m_state.cycles != vu.m_cycle, "inconsistent VU snapshot clock");
    // Current scalar latency tops out at 54 cycles; all other writeback and
    // issue-resource latencies are shorter. Keep a small model-specific bound
    // so a checksum-valid fabricated deadline cannot make flushPipelines()
    // advance billions of cycles outside the caller's execution budget.
    constexpr uint64_t scheduleHorizon = 64;
    fail(vu.m_cycle > std::numeric_limits<uint64_t>::max() - scheduleHorizon,
        "VU snapshot clock cannot schedule another modeled instruction");
    const auto deadline = [&](uint64_t cycle)
    {
        fail(cycle > vu.m_cycle && cycle - vu.m_cycle > scheduleHorizon,
            "VU snapshot deadline exceeds the current scheduler model");
    };
    for (const auto &entry : vu.m_flagPipeline)
        if (entry.valid)
        {
            deadline(entry.readyCycle);
            fail(entry.issueCycle > vu.m_cycle, "VU flag was issued in the future");
        }
    if (vu.m_fdiv.valid) deadline(vu.m_fdiv.readyCycle);
    for (const auto &entry : vu.m_efu)
        if (entry.valid) deadline(entry.readyCycle);
    deadline(vu.m_efuResourceReady);
    for (const auto &reg : vu.m_vfReady)
        for (const auto cycle : reg) deadline(cycle);
    for (const auto cycle : vu.m_viReady) deadline(cycle);
    for (const auto cycle : vu.m_accReady) deadline(cycle);
    fail((vu.m_state.pc & 7u) != 0 || vu.m_state.pc > vu.microAddressMask(),
        "invalid VU snapshot instruction-pair address");
    fail(vu.m_viBranchBackupReg >= 16 ||
        (vu.m_viBranchBackupValid && vu.m_viBranchBackupReg == 0), "invalid VU branch backup register");
    for (const auto &entry : vu.m_vfWritePipeline)
    {
        if (entry.valid) deadline(entry.readyCycle);
        fail(entry.reg >= 32 || (entry.valid && entry.reg == 0) || entry.laneMask > 15,
            "invalid VU vector write descriptor");
        fail(entry.sequence > vu.m_nextWriteSequence, "invalid VU vector write sequence");
    }
    for (const auto &entry : vu.m_viWritePipeline)
    {
        if (entry.valid) deadline(entry.readyCycle);
        fail(entry.reg >= 16 || (entry.valid && entry.reg == 0), "invalid VU integer write descriptor");
        fail(entry.sequence > vu.m_nextWriteSequence, "invalid VU integer write sequence");
    }
    for (const auto &entry : vu.m_accWritePipeline)
    {
        if (entry.valid) deadline(entry.readyCycle);
        fail(entry.laneMask > 15, "invalid VU accumulator lane mask");
        fail(entry.sequence > vu.m_nextWriteSequence, "invalid VU accumulator write sequence");
    }
    for (const auto &entry : vu.m_storePipeline)
    {
        if (entry.valid) deadline(entry.readyCycle);
        fail(entry.laneMask > 15, "invalid VU store lane mask");
    }
    const auto &kick = vu.m_xgkick;
    fail(kick.totalBytes > kick.packet.size() || kick.copiedBytes > kick.packet.size() ||
        kick.currentTagEnd > kick.packet.size() || (kick.copiedBytes & 15u) != 0 ||
        (kick.totalBytes & 15u) != 0 || (kick.currentTagEnd & 15u) != 0,
        "invalid VU XGKICK extent");
}

std::vector<uint8_t> VuSnapshotCodec::encode(const VU1Interpreter &vu)
{
    validate(vu);
    Writer writer;
    writer.bytes.reserve(maximumSize);
    writer.bytes.insert(writer.bytes.end(), magic.begin(), magic.end());
    writer(formatVersion);
    writer(static_cast<uint32_t>(vu.m_unit));
    writer(uint32_t{0});
    writer(uint32_t{0});
    fields(writer, vu);
    if (writer.bytes.size() > maximumSize) throw std::logic_error("VU snapshot exceeds its format bound");
    putU32(writer.bytes, 16, static_cast<uint32_t>(writer.bytes.size() - headerSize));
    putU32(writer.bytes, 20, checksum(writer.bytes));
    return std::move(writer.bytes);
}

void VuSnapshotCodec::restore(VU1Interpreter &vu, std::span<const uint8_t> bytes)
{
    if (bytes.size() < headerSize || bytes.size() > maximumSize ||
        !std::equal(magic.begin(), magic.end(), bytes.begin()))
        throw std::invalid_argument("invalid canonical VU snapshot header");
    Reader reader{bytes, 8};
    uint32_t version = 0, unit = 0, length = 0, crc = 0;
    reader(version); reader(unit); reader(length); reader(crc);
    if (version != formatVersion) throw std::invalid_argument("unsupported canonical VU snapshot version");
    if (unit > 1 || unit != static_cast<uint32_t>(vu.m_unit))
        throw std::invalid_argument("canonical VU snapshot unit mismatch");
    if (length != bytes.size() - headerSize || crc != checksum(bytes))
        throw std::invalid_argument("canonical VU snapshot length or checksum mismatch");

    auto candidate = std::make_unique<VU1Interpreter>(vu.m_unit);
    fields(reader, *candidate);
    if (reader.offset != bytes.size()) throw std::invalid_argument("extra canonical VU snapshot fields");
    validate(*candidate);
    // The candidate's host bindings are null and its decoded cache is invalid.
    // Commit only after the entire snapshot has passed decoding and validation.
    vu = std::move(*candidate);
}
}

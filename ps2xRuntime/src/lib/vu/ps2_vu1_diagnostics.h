#pragma once

#include "runtime/ps2_vu1.h"
#if PS2X_NEXO_LAB
#include "nexo/vu_snapshot.h"
#include "nexo/canonical_binary.h"
#endif
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <vector>

// Local, opt-in diagnostics. NEXO laboratory builds additionally capture the
// full current-runtime state, including pending pipelines on MSCNT.
namespace ps2_vu_diagnostics
{
struct TraceEntry
{
    uint64_t cycle;
    uint32_t pc, lower, upper, status, mac, clip;
    std::array<int32_t, 16> vi;
    VU1State registers;
};

struct Capture
{
    VU1State input{};
    std::vector<uint8_t> code, data;
#if PS2X_NEXO_LAB
    std::vector<uint8_t> inputCanonical;
    std::vector<uint8_t> path1Events{'N','E','X','O','G','I','F',0, 1,0,0,0, 0,0,0,0};
    uint32_t path1Count = 0;
    bool path1RecordingFailed = false;
    struct Issue { uint64_t cycle; uint32_t pc,lower,upper; };
    std::vector<Issue> fullIssues;
    bool issueTraceEnabled=false,continuousIssueCapture=false,issueRecordingFailed=false;
#endif
    std::array<TraceEntry, 512> trace{};
    uint64_t issues = 0, startCycle = 0, codeGeneration = 0;
    uint32_t budget = 0;
    bool fresh = true;
    std::filesystem::path directory;

    static std::unique_ptr<Capture> request(bool vu1, const VU1Interpreter &vu,
        const uint8_t *code, uint32_t codeSize, const uint8_t *data, uint32_t dataSize,
        uint64_t cycle, uint64_t generation, uint32_t maxCycles, bool fresh = true) noexcept
    {
        if (!vu1 || !code || !data) return {};
        try
        {
            const char *base = std::getenv("PS2X_CAPTURE_SCENE");
            if (!base || !*base) return {};
            std::error_code ec;
            if (!std::filesystem::remove(std::filesystem::path(base) / ".vu-request", ec)) return {};
            auto result = std::make_unique<Capture>();
            result->input = vu.state();
#if PS2X_NEXO_LAB
            result->inputCanonical = ps2native::nexo::VuSnapshotCodec::encode(vu);
            const char* issues=std::getenv("PS2X_CAPTURE_VU_ISSUES");
            result->issueTraceEnabled=issues && std::strcmp(issues,"1")==0;
            const char* continuous=std::getenv("PS2X_CAPTURE_VU_CONTINUOUS");
            result->continuousIssueCapture=result->issueTraceEnabled && continuous && std::strcmp(continuous,"1")==0;
#endif
            result->code.assign(code, code + codeSize);
            result->data.assign(data, data + dataSize);
            result->startCycle = cycle;
            result->codeGeneration = generation;
            result->budget = maxCycles;
            result->fresh = fresh;
            result->directory = std::filesystem::path(base) /
                ("vu-input-" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()));
            return result;
        }
        catch (const std::exception &error)
        {
            std::fprintf(stderr, "[vu-capture:error] %s\n", error.what());
            return {};
        }
    }

    void record(const VU1State &state, uint64_t cycle, uint32_t lower, uint32_t upper)
    {
        auto &entry = trace[issues++ % trace.size()];
        entry = {cycle, state.pc, lower, upper, state.status, state.mac, state.clip, {}, state};
        std::copy(std::begin(state.vi), std::end(state.vi), entry.vi.begin());
#if PS2X_NEXO_LAB
        if (issueTraceEnabled && !issueRecordingFailed)
        {
            try
            {
                if (fullIssues.size()>=262144) throw std::runtime_error("VU issue trace exceeded its bound");
                fullIssues.push_back({cycle,state.pc,lower,upper});
            }
            catch (...) { issueRecordingFailed=true; }
        }
#endif
    }

#if PS2X_NEXO_LAB
    // Observe submission at the VU -> GIF boundary, before arbitration or GS
    // consumption. This is not a checkpoint of either receiving device.
    void recordPacket(uint64_t cycle, const uint8_t *bytes, uint32_t size) noexcept
    {
        if (path1RecordingFailed) return;
        try
        {
            constexpr size_t maximumEventsSize = 64u * 1024u * 1024u;
            if (!bytes || size < 16 || size > 65536 || (size & 15u) != 0 ||
                cycle < startCycle || path1Events.size() + 12u + size > maximumEventsSize)
                throw std::runtime_error("invalid or oversized PATH1 capture");
            const auto append = [this](uint64_t value, unsigned width)
            {
                for (unsigned i = 0; i < width; ++i) path1Events.push_back(uint8_t(value >> (8 * i)));
            };
            append(cycle - startCycle, 8);
            append(size, 4);
            path1Events.insert(path1Events.end(), bytes, bytes + size);
            ++path1Count;
            for (unsigned i = 0; i < 4; ++i) path1Events[12 + i] = uint8_t(path1Count >> (8 * i));
        }
        catch (const std::exception &error)
        {
            path1RecordingFailed = true;
            std::fprintf(stderr, "[vu-capture:error] %s\n", error.what());
        }
    }
#endif

    void finish(const VU1Interpreter &vu, const uint8_t *finalData, uint32_t dataSize,
                bool programEnded, bool stopped) const noexcept
    {
        try
        {
            const auto &state = vu.state();
            std::filesystem::create_directories(directory);
            const auto binary = [this](const char *name, const void *bytes, size_t size)
            {
                std::ofstream stream(directory / name, std::ios::binary | std::ios::trunc);
                stream.write(static_cast<const char *>(bytes), static_cast<std::streamsize>(size));
                stream.close();
                if (!stream) throw std::runtime_error(std::string("cannot write ") + name);
            };
            binary("input-state.bin", &input, sizeof(input));
            binary("input-data.bin", data.data(), data.size());
            binary("code.bin", code.data(), code.size());
            binary("output-state.bin", &state, sizeof(state));
            binary("output-data.bin", finalData, dataSize);
#if PS2X_NEXO_LAB
            if (path1RecordingFailed) throw std::runtime_error("incomplete PATH1 capture");
            const auto outputCanonical = ps2native::nexo::VuSnapshotCodec::encode(vu);
            binary("input-state.nexo", inputCanonical.data(), inputCanonical.size());
            binary("output-state.nexo", outputCanonical.data(), outputCanonical.size());
            binary("path1-events.nexo", path1Events.data(), path1Events.size());
            if (issueTraceEnabled)
            {
                if (issueRecordingFailed || fullIssues.size()!=issues) throw std::runtime_error("incomplete VU issue history");
                ps2native::nexo::binary::Writer a({'N','E','X','O','V','P','I',0},1,6u*1024u*1024u);
                a(uint32_t(fullIssues.size()));
                for (const auto& issue:fullIssues) { a(issue.cycle); a(issue.pc); a(issue.lower); a(issue.upper); }
                const auto encoded=a.finish(); binary("issues.nexo",encoded.data(),encoded.size());
            }
#endif
            const uint64_t count = std::min<uint64_t>(issues, trace.size());
            // One local-ABI VU1State per ISSUE, in the same chronological
            // order as trace.txt. Keep raw VF bits, ACC, I/Q/P and flags so
            // a packed GIF field is distinguishable from numeric arithmetic.
            std::ofstream registers(directory / "trace-state.bin", std::ios::binary | std::ios::trunc);
            for (uint64_t i = issues - count; i < issues; ++i)
                registers.write(reinterpret_cast<const char *>(&trace[i % trace.size()].registers), sizeof(VU1State));
            registers.close();
            if (!registers) throw std::runtime_error("cannot write VU register history");
            std::ofstream out(directory / "trace.txt", std::ios::trunc);
            out << "VU1 " << (fresh ? "MSCAL" : "MSCNT")
                << " diagnostics; legacy .bin state uses local host ABI; "
                << (fresh ? "initially empty pipelines" : "pending pipelines retained") << '\n'
                << "startPc=0x" << std::hex << input.pc << " endPc=0x" << state.pc << std::dec
                << " top=" << input.top << " itop=" << input.itop << " codeGeneration=" << codeGeneration
                << " cycles=" << state.cycles - startCycle << " budget=" << budget
                << " programEnded=" << programEnded << " stopped=" << stopped
                << " budgetReached=" << (!programEnded && !stopped && state.cycles - startCycle >= budget)
                << " instructions=" << issues << " traceEntries=" << count << '\n';
            for (unsigned i = 0; i < 16; ++i) out << "INPUT_VI " << i << ' ' << input.vi[i] << '\n';
            for (uint64_t i = issues - count; i < issues; ++i)
            {
                const auto &entry = trace[i % trace.size()];
                out << "ISSUE cycle=" << entry.cycle - startCycle << " pc=0x" << std::hex << entry.pc
                    << " lower=0x" << entry.lower << " upper=0x" << entry.upper
                    << " clip=0x" << entry.clip << " mac=0x" << entry.mac << " status=0x" << entry.status
                    << std::dec << " vi=";
                for (const auto value : entry.vi) out << value << ',';
                out << '\n';
            }
            out.close();
            if (!out) throw std::runtime_error("cannot write VU instruction trace");
#if PS2X_NEXO_LAB
            if (issueTraceEnabled)
            {
                const uint8_t complete=1; binary(".issue-complete",&complete,1);
                if (continuousIssueCapture)
                {
                    // Rearm only this explicitly opted-in laboratory case.
                    // Consumers must still check all expected callback receipts.
                    std::ofstream marker(directory.parent_path()/".vu-request",std::ios::binary|std::ios::trunc);
                    marker.put(1); marker.close();
                    if (!marker) throw std::runtime_error("cannot rearm continuous VU issue capture");
                }
            }
#endif
            std::fprintf(stderr, "[vu-capture] directory=%s\n", directory.string().c_str());
        }
        catch (const std::exception &error)
        {
            std::fprintf(stderr, "[vu-capture:error] %s\n", error.what());
        }
    }
};

inline thread_local Capture *active = nullptr;
struct Scope
{
    Capture *previous;
    explicit Scope(Capture *capture) : previous(active) { active = capture; }
    ~Scope() { active = previous; }
};
}

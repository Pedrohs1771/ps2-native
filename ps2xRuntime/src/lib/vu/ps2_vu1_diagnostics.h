#pragma once

#include "runtime/ps2_vu1.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <vector>

// Local, opt-in diagnostics for fresh MSCAL execution, with empty pipelines.
// This deliberately does not pretend to capture the hidden MSCNT pipelines.
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
    std::array<TraceEntry, 512> trace{};
    uint64_t issues = 0, startCycle = 0, codeGeneration = 0;
    uint32_t budget = 0;
    std::filesystem::path directory;

    static std::unique_ptr<Capture> request(bool vu1, const VU1State &state,
        const uint8_t *code, uint32_t codeSize, const uint8_t *data, uint32_t dataSize,
        uint64_t cycle, uint64_t generation, uint32_t maxCycles) noexcept
    {
        if (!vu1 || !code || !data) return {};
        try
        {
            const char *base = std::getenv("PS2X_CAPTURE_SCENE");
            if (!base || !*base) return {};
            std::error_code ec;
            if (!std::filesystem::remove(std::filesystem::path(base) / ".vu-request", ec)) return {};
            auto result = std::make_unique<Capture>();
            result->input = state;
            result->code.assign(code, code + codeSize);
            result->data.assign(data, data + dataSize);
            result->startCycle = cycle;
            result->codeGeneration = generation;
            result->budget = maxCycles;
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
    }

    void finish(const VU1State &state, const uint8_t *finalData, uint32_t dataSize,
                bool programEnded, bool stopped) const noexcept
    {
        try
        {
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
            out << "VU1 MSCAL diagnostics; local host ABI; initially empty pipelines\n"
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

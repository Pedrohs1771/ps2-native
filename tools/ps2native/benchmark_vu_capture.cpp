// Benchmark a local fresh-MSCAL capture using the same host runtime ABI.
// This measures VU execution plus its GIF callbacks, not game frame rate.
#include "runtime/ps2_memory.h"
#include "runtime/ps2_vu1.h"
#include "runtime/gs/gs_frontend.h"
#include <array>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace
{
void readExact(const std::filesystem::path &path, void *destination, size_t size)
{
    if (std::filesystem::file_size(path) != size)
        throw std::runtime_error("incompatible capture size: " + path.string());
    std::ifstream stream(path, std::ios::binary);
    stream.read(static_cast<char *>(destination), static_cast<std::streamsize>(size));
    if (!stream) throw std::runtime_error("cannot read capture: " + path.string());
}

uint64_t hashBytes(const void *data, size_t size)
{
    const auto *bytes = static_cast<const uint8_t *>(data);
    uint64_t hash = 14695981039346656037ull;
    for (size_t i = 0; i < size; ++i) hash = (hash ^ bytes[i]) * 1099511628211ull;
    return hash;
}
}

int main(int argc, char **argv)
{
    try
    {
        if (argc != 3) throw std::runtime_error("usage: benchmark-vu-capture <vu-input-directory> <iterations>");
        const std::filesystem::path directory(argv[1]);
        size_t parsed = 0;
        const auto iterations = std::stoul(argv[2], &parsed);
        if (parsed != std::strlen(argv[2])) throw std::runtime_error("invalid iteration count");
        if (iterations < 1 || iterations > 100000) throw std::runtime_error("iterations must be 1..100000");
        VU1State input{}, recordedOutput{};
        std::array<uint8_t, PS2_VU1_DATA_SIZE> data{}, recordedData{};
        readExact(directory / "input-state.bin", &input, sizeof(input));
        readExact(directory / "output-state.bin", &recordedOutput, sizeof(recordedOutput));
        readExact(directory / "input-data.bin", data.data(), data.size());
        readExact(directory / "output-data.bin", recordedData.data(), recordedData.size());
        PS2Memory memory;
        if (!memory.initialize()) throw std::runtime_error("cannot initialize replay memory");
        readExact(directory / "code.bin", memory.getVU1Code(), PS2_VU1_CODE_SIZE);
        memory.markVU1CodeModified();
        GS gs;
        gs.init(memory.getGSVRAM(), PS2_GS_VRAM_SIZE, &memory.gs());
        VU1Interpreter vu;

        auto execute = [&]
        {
            // execute() deliberately keeps its absolute scheduler clock across
            // calls. Reset it between independent replays, retaining decode cache.
            vu.reset();
            std::memcpy(&vu.state(), &input, sizeof(input));
            std::memcpy(memory.getVU1Data(), data.data(), data.size());
            vu.execute(memory.getVU1Code(), PS2_VU1_CODE_SIZE,
                       memory.getVU1Data(), PS2_VU1_DATA_SIZE, gs, &memory,
                       input.pc, input.top, input.itop, 65536u);
        };
        execute();
        VU1State expected{};
        std::memcpy(&expected, &vu.state(), sizeof(expected));
        const auto expectedHash = hashBytes(memory.getVU1Data(), PS2_VU1_DATA_SIZE);
        if (expected.pc != recordedOutput.pc || expected.cycles >= 65536u ||
            std::memcmp(memory.getVU1Data(), recordedData.data(), recordedData.size()) != 0)
            throw std::runtime_error("replay differs from recorded output or exceeds budget");

        std::chrono::nanoseconds elapsed{};
        for (unsigned long i = 0; i < iterations; ++i)
        {
            vu.reset();
            std::memcpy(&vu.state(), &input, sizeof(input));
            std::memcpy(memory.getVU1Data(), data.data(), data.size());
            const auto begin = std::chrono::steady_clock::now();
            vu.execute(memory.getVU1Code(), PS2_VU1_CODE_SIZE,
                       memory.getVU1Data(), PS2_VU1_DATA_SIZE, gs, &memory,
                       input.pc, input.top, input.itop, 65536u);
            elapsed += std::chrono::steady_clock::now() - begin;
            if (std::memcmp(&vu.state(), &expected, sizeof(expected)) != 0 ||
                hashBytes(memory.getVU1Data(), PS2_VU1_DATA_SIZE) != expectedHash)
                throw std::runtime_error("nondeterministic replay output at iteration " + std::to_string(i));
        }
        std::cout << "iterations=" << iterations
                  << " cycles=" << expected.cycles
                  << " endPc=0x" << std::hex << expected.pc
                  << " stateHash=0x" << hashBytes(&expected, sizeof(expected))
                  << " dataHash=0x" << expectedHash << std::dec
                  << " total_ms=" << std::chrono::duration<double, std::milli>(elapsed).count()
                  << " mean_us=" << std::chrono::duration<double, std::micro>(elapsed).count() / iterations
                  << " identical_outputs=1\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "[vu-benchmark:error] " << error.what() << '\n';
        return 1;
    }
}

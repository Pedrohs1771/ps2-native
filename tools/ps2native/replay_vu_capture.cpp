// Replay one local MSCAL input without booting a game or opening a window.
// Build against the same host runtime ABI that wrote input-state.bin.
#include "runtime/ps2_memory.h"
#include "runtime/ps2_vu1.h"
#include "runtime/gs/gs_frontend.h"
#include <cstdlib>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

static void readExact(const std::filesystem::path &path, void *output, size_t bytes)
{
    if (std::filesystem::file_size(path) != bytes)
        throw std::runtime_error("unexpected capture size: " + path.string());
    std::ifstream stream(path, std::ios::binary);
    stream.read(static_cast<char *>(output), static_cast<std::streamsize>(bytes));
    if (!stream) throw std::runtime_error("cannot read " + path.string());
}

int main(int argc, char **argv)
{
    try
    {
        if (argc < 2 || argc > 4)
        {
            std::cerr << "usage: replay-vu-capture <vu-input-directory> [cycle-budget] [trace-output-directory]\n";
            return 1;
        }
        const auto requested = argc > 2 ? std::stoul(argv[2]) : 65536ul;
        if (!requested || requested > 1048576ul) throw std::runtime_error("cycle budget must be 1..1048576");
        const uint32_t budget = static_cast<uint32_t>(requested);
        const std::filesystem::path input(argv[1]);
        PS2Memory memory;
        if (!memory.initialize()) throw std::runtime_error("cannot initialize replay memory");
        GS gs;
        gs.init(memory.getGSVRAM(), PS2_GS_VRAM_SIZE, &memory.gs());
        VU1Interpreter vu;
        readExact(input / "input-state.bin", &vu.state(), sizeof(VU1State));
        readExact(input / "input-data.bin", memory.getVU1Data(), PS2_VU1_DATA_SIZE);
        readExact(input / "code.bin", memory.getVU1Code(), PS2_VU1_CODE_SIZE);
        memory.markVU1CodeModified();
        const auto stamp = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        const auto output = argc > 3 ? std::filesystem::absolute(argv[3]) / ("replay-" + stamp)
            : std::filesystem::temp_directory_path() / ("ps2-vu-replay-" + stamp);
        struct Cleanup
        {
            std::filesystem::path path;
            bool remove;
            ~Cleanup() { if (remove) { std::error_code ec; std::filesystem::remove_all(path, ec); } }
        } cleanup{output, argc <= 3};
        std::filesystem::create_directories(output);
#if defined(_WIN32)
        _putenv_s("PS2X_CAPTURE_SCENE", output.string().c_str());
#else
        setenv("PS2X_CAPTURE_SCENE", output.c_str(), 1);
#endif
        std::ofstream(output / ".vu-request").close();
        const auto start = vu.state();
        vu.execute(memory.getVU1Code(), PS2_VU1_CODE_SIZE,
            memory.getVU1Data(), PS2_VU1_DATA_SIZE, gs, &memory,
            start.pc, start.top, start.itop, budget);
        std::cout << "startPc=0x" << std::hex << start.pc << " endPc=0x" << vu.state().pc
            << std::dec << " cycles=" << vu.state().cycles << " budget=" << budget << '\n';
        for (unsigned i = 0; i < 16; ++i) std::cout << "VI " << i << ' ' << vu.state().vi[i] << '\n';
        std::string metadata;
        for (const auto &entry : std::filesystem::directory_iterator(output))
            if (entry.is_directory())
            {
                std::ifstream stream(entry.path() / "trace.txt");
                std::string header;
                std::getline(stream, header);
                std::getline(stream, metadata);
            }
        std::cout << metadata << '\n';
        // A reserved/XGKICK error can stop early. Cycle count alone is never
        // evidence of successful E-bit or configured D/T termination.
        if (metadata.find("programEnded=1") != std::string::npos) return 0;
        return metadata.find("budgetReached=1") != std::string::npos ? 2 : 1;
    }
    catch (const std::exception &error)
    {
        std::cerr << "[vu-replay:error] " << error.what() << '\n';
        return 1;
    }
}

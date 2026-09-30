#include "nexo/vu_replay.h"
#ifdef NEXO_COMPILED_VU_BANK
#include "nexo/vu_native.h"
namespace ps2native::nexo { const VuNativeProgram &compiledVuProgram(); }
#endif

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace
{
uint32_t number(std::string_view value, uint32_t maximum)
{
    uint32_t result = 0;
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || result == 0 || result > maximum)
        throw std::invalid_argument("invalid cycle budget or iteration count");
    return result;
}

std::vector<uint8_t> expected(const std::filesystem::path &path, size_t maximum)
{
    const auto size = std::filesystem::file_size(path);
    if (size == 0 || size > maximum) throw std::invalid_argument("invalid recorded output size");
    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    std::ifstream stream(path, std::ios::binary);
    stream.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!stream || stream.peek() != std::char_traits<char>::eof())
        throw std::invalid_argument("cannot read exact recorded output");
    return bytes;
}

void comparison(const char *name, const std::vector<uint8_t> &actual, const std::vector<uint8_t> &recorded)
{
    const auto common = std::min(actual.size(), recorded.size());
    size_t offset = 0;
    while (offset < common && actual[offset] == recorded[offset]) ++offset;
    const bool equal = offset == common && actual.size() == recorded.size();
    std::cout << '"' << name << "\":{\"equal\":" << (equal ? "true" : "false")
        << ",\"first_byte_difference\":";
    if (equal) std::cout << "null";
    else std::cout << offset;
    std::cout << '}';
}
}

int main(int argc, char **argv)
{
    try
    {
        if (argc < 3 || argc > 4)
            throw std::invalid_argument("usage: nexo_vu_replay <canonical-capture-directory> <cycle-budget> [iterations]");
        const std::filesystem::path directory(argv[1]);
        const auto budget = number(argv[2], 1048576);
        const auto iterations = argc == 4 ? number(argv[3], 10000) : 1;
        // Measurements must not consume scene-capture requests from the caller.
#ifdef _WIN32
        _putenv_s("PS2X_CAPTURE_SCENE", "");
#else
        unsetenv("PS2X_CAPTURE_SCENE");
#endif
        const auto state = expected(directory / "output-state.nexo", 80 * 1024);
        const auto data = expected(directory / "output-data.bin", 16384);
        const auto events = expected(directory / "path1-events.nexo", 64 * 1024 * 1024);
        uint64_t totalNanoseconds = 0;
        ps2native::nexo::VuReplayResult result;
        bool allMatch = true;
        uint32_t executed = 0;
        for (; executed < iterations; ++executed)
        {
#ifdef NEXO_VU_RUNTIME_CALLBACK_REPLAY
            result = ps2native::nexo::replayVuCaptureNativeVif(directory, budget, ps2native::nexo::compiledVuProgram());
#elif defined(NEXO_COMPILED_VU_BANK)
            result = ps2native::nexo::replayVuCaptureNative(directory, budget, ps2native::nexo::compiledVuProgram());
#else
            result = ps2native::nexo::replayVuCapture(directory, budget);
#endif
            totalNanoseconds += result.executionNanoseconds;
            if (result.state != state || result.data != data || result.path1Events != events)
            {
                allMatch = false;
                ++executed;
                break;
            }
        }
        std::cout << "{\"scope\":\"vu_state_data_path1_submissions\",\"assurance\":\"tested_only\","
#ifdef NEXO_VU_RUNTIME_CALLBACK_REPLAY
            << "\"execution_backend\":\"compiled_vu_bank_vif_callbacks\","
            << "\"vif_input\":\"synthesized_mscnt_from_normalized_vu_capture\","
#elif defined(NEXO_COMPILED_VU_BANK)
            << "\"execution_backend\":\"compiled_vu_bank\","
#else
            << "\"execution_backend\":\"current_interpreter\","
#endif
            << "\"matches_current_runtime_recording\":" << (allMatch ? "true" : "false")
            << ",\"iterations\":" << executed << ",\"mean_execution_us\":"
            << double(totalNanoseconds) / (1000.0 * executed) << ',';
        comparison("state", result.state, state); std::cout << ',';
        comparison("data", result.data, data); std::cout << ',';
        comparison("path1", result.path1Events, events); std::cout << "}\n";
        return allMatch ? 0 : 2;
    }
    catch (const std::exception &error)
    {
        std::cerr << "[nexo-vu-replay:error] " << error.what() << '\n';
        return 1;
    }
}

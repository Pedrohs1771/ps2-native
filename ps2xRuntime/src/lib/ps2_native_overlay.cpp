#include "ps2_native_overlay.h"
#include "ps2_native_overlay_abi.h"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <unordered_map>
#include <vector>

#if defined(__linux__) && !defined(__ANDROID__)
#include <dlfcn.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char **environ;
namespace
{
    struct OverlayState
    {
        struct Failure
        {
            uint32_t sourceBegin;
            std::vector<uint8_t> sourceBytes;
        };
        std::unordered_map<uint32_t, PS2NativeOverlayBinding> bindings;
        std::unordered_map<uint32_t, Failure> failed;
        std::vector<void *> handles;
    };
    std::mutex overlayMutex;
    std::unordered_map<PS2Runtime *, OverlayState> overlays;
    bool validBinding(const PS2NativeOverlayBinding &binding)
    {
        return binding.function && binding.sourceBytes && !(binding.address & 3u) &&
               binding.sourceSize && binding.sourceBegin < PS2_RAM_SIZE &&
               binding.sourceSize <= PS2_RAM_SIZE - binding.sourceBegin &&
               binding.address >= binding.sourceBegin &&
               binding.address - binding.sourceBegin < binding.sourceSize;
    }
}
#endif

PS2Runtime::RecompiledFunction ps2xResolveDiagnosticNativeOverlay(PS2Runtime *runtime, uint8_t *ram, uint32_t address)
{
#if defined(__linux__) && !defined(__ANDROID__)
    const char *driver = std::getenv("PS2X_NATIVE_OVERLAY_DRIVER");
    if (!driver || !*driver || !ram || (address & 3u) || address < 0x1000u || address >= PS2_RAM_SIZE - 16u)
        return nullptr;
    constexpr uint32_t windowSize = 0x10000;
    uint32_t base = address & ~(windowSize - 1u);
    // A root block may contain 128 instructions plus a branch delay slot.
    // Overlap windows near the end so that valid cross-boundary code keeps
    // its actual delay instruction rather than failing a truncated snapshot.
    if (address - base >= windowSize - 512u)
        base += windowSize / 2u;
    const uint32_t snapshotSize = std::min(windowSize, PS2_RAM_SIZE - base);
    std::lock_guard<std::mutex> lock(overlayMutex);
    auto &state = overlays[runtime];
    auto found = state.bindings.find(address);
    if (found != state.bindings.end() &&
        std::memcmp(ram + found->second.sourceBegin, found->second.sourceBytes, found->second.sourceSize) == 0)
        return found->second.function;
    auto failed = state.failed.find(address);
    if (failed != state.failed.end() && failed->second.sourceBegin == base &&
        failed->second.sourceBytes.size() == snapshotSize &&
        std::memcmp(ram + base, failed->second.sourceBytes.data(), snapshotSize) == 0)
        return nullptr;
    uint32_t nonzero = 0;
    for (unsigned offset = 0; offset < 16; offset += 4)
    {
        uint32_t word;
        std::memcpy(&word, ram + address + offset, 4);
        nonzero |= word;
    }
    if (!nonzero) return nullptr;
    // Bound executable memory retained by malformed guest programs.
    if (state.handles.size() >= 512) return nullptr;
    const auto started = std::chrono::steady_clock::now();
    std::string workspace = (std::filesystem::temp_directory_path() / "ps2-native-overlay-XXXXXX").string();
    std::vector<char> workspaceBuffer(workspace.begin(), workspace.end());
    workspaceBuffer.push_back('\0');
    if (!mkdtemp(workspaceBuffer.data())) return nullptr;
    const std::filesystem::path directory(workspaceBuffer.data());
    const std::string input = (directory / "snapshot.bin").string();
    const std::string result = (directory / "result.txt").string();
    const std::string baseArg = std::to_string(base);
    const std::string entryArg = std::to_string(address);
    bool success = false;
    std::string error = "driver failed";
    try
    {
        {
            std::ofstream file(input, std::ios::binary);
            file.write(reinterpret_cast<const char *>(ram + base), snapshotSize);
            if (!file) throw std::runtime_error("cannot save EE code snapshot");
        }
        char *argv[] = {const_cast<char *>(driver), const_cast<char *>(input.c_str()),
                        const_cast<char *>(baseArg.c_str()), const_cast<char *>(entryArg.c_str()),
                        const_cast<char *>(result.c_str()), nullptr};
        pid_t child;
        const int spawnResult = posix_spawn(&child, driver, nullptr, nullptr, argv, environ);
        if (spawnResult != 0) throw std::runtime_error("cannot launch native overlay driver");
        int status;
        pid_t waited;
        do { waited = waitpid(child, &status, 0); } while (waited < 0 && errno == EINTR);
        if (waited < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0)
            throw std::runtime_error("native overlay driver returned an error");
        std::ifstream resultFile(result);
        std::string library;
        std::getline(resultFile, library);
        if (library.empty()) throw std::runtime_error("driver did not return a native library");
        void *handle = dlopen(library.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (!handle) throw std::runtime_error(dlerror());
        auto getBindings = reinterpret_cast<PS2NativeOverlayGetBindings>(dlsym(handle, "ps2xOverlayGetBindings"));
        size_t count = 0;
        uint32_t abi = 0;
        const auto *bindings = getBindings ? getBindings(&count, &abi) : nullptr;
        bool rootFound = false;
        bool valid = bindings && abi == PS2_NATIVE_OVERLAY_ABI && count && count <= 32768;
        for (size_t index = 0; valid && index < count; ++index)
        {
            valid = validBinding(bindings[index]);
            rootFound |= bindings[index].address == address;
        }
        if (!valid || !rootFound)
        {
            dlclose(handle);
            throw std::runtime_error("invalid native overlay ABI or bindings");
        }
        state.handles.push_back(handle);
        for (size_t index = 0; index < count; ++index) state.bindings[bindings[index].address] = bindings[index];
        const auto &binding = state.bindings.at(address);
        success = std::memcmp(ram + binding.sourceBegin, binding.sourceBytes, binding.sourceSize) == 0;
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count();
        std::cerr << "[native-overlay] target=0x" << std::hex << address << std::dec
                  << " bindings=" << count << " elapsed_ms=" << ms << " library=" << library << '\n';
    }
    catch (const std::exception &exception) { error = exception.what(); }
    std::error_code cleanupError;
    std::filesystem::remove_all(directory, cleanupError);
    if (success)
    {
        state.failed.erase(address);
        return state.bindings.at(address).function;
    }
    state.failed[address] = {base, std::vector<uint8_t>(ram + base, ram + base + snapshotSize)};
    std::cerr << "[native-overlay:error] target=0x" << std::hex << address << std::dec << " " << error << '\n';
#else
    (void)runtime; (void)ram; (void)address;
#endif
    return nullptr;
}

void ps2xReleaseDiagnosticNativeOverlays(PS2Runtime *runtime)
{
#if defined(__linux__) && !defined(__ANDROID__)
    std::lock_guard<std::mutex> lock(overlayMutex);
    auto found = overlays.find(runtime);
    if (found == overlays.end()) return;
    for (void *handle : found->second.handles) dlclose(handle);
    overlays.erase(found);
#else
    (void)runtime;
#endif
}

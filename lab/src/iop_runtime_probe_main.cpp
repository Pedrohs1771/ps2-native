#include "nexo/iop_lab_io.h"
#include "ps2_runtime.h"

#include <algorithm>
#include <iostream>

// This probe constructs the actual runtime/IOP adapter, initializes RAM only,
// and never calls the window/audio initialization or executes EE/VU code.
int main(int argc, char **argv)
{
    if (argc != 3)
    {
        std::cerr << "Usage: nexo_iop_runtime_probe IRX NEW_OUTPUT_DIRECTORY\n";
        return 2;
    }
    try
    {
        using namespace ps2native::nexo::iop_lab;
        const auto source = std::filesystem::absolute(argv[1]);
        const std::filesystem::path output(argv[2]);
        if (std::filesystem::exists(output))
            throw std::runtime_error("output directory already exists");
        (void)readImage(source); // Same bounded input admission as the lab probe.
        if (!std::filesystem::create_directory(output))
            throw std::runtime_error("cannot create output directory");
        PS2Runtime::IoPaths paths{};
        paths.hostRoot = source.parent_path(); paths.cdRoot = source.parent_path();
        paths.elfDirectory = source.parent_path(); paths.mcRoot = output / "memory-card";
        PS2Runtime::setIoPaths(paths);
        PS2Runtime runtime;
        if (!runtime.memory().initialize())
            throw std::runtime_error("cannot initialize runtime RAM");
        std::vector<ps2x::iop::ModuleLoadResult> loads;
        std::string executionError;
        try
        {
            for (unsigned instance = 0; instance < 2u; ++instance)
            {
                loads.push_back(runtime.loadIopModule("host:" + source.filename().string()));
                if (loads.back().moduleId < 0) break;
            }
        }
        catch (const std::exception &error) { executionError = error.what(); }
        const auto snapshot = runtime.iopDebugSnapshot();
        const bool startupsResident = std::all_of(loads.begin(), loads.end(), [](const auto &loaded)
        {
            return loaded.moduleId > 0 && (loaded.startResult == 0 || loaded.startResult == 2);
        });
        const bool accepted = loads.size() == 2u && startupsResident &&
                              snapshot.nativeFaults == 0u && executionError.empty() &&
                              snapshot.emulatorInstructions < 4'000'000u &&
                              (!NEXO_IOP_RUNTIME_PROBE_NATIVE || snapshot.interpretedInstructions == 0u);
        std::vector<uint8_t> ram(2u * 1024u * 1024u);
        if (!runtime.readIopMemory(0, ram.data(), ram.size()))
            throw std::runtime_error("cannot capture runtime IOP RAM");
        writeBytes(output / "iop-ram.bin", ram);
        writeBytes(output / "ee-ram.bin", std::span(runtime.memory().getRDRAM(), PS2_RAM_SIZE));
        std::ofstream report(output / "report.json");
        report << "{\n\"schema_version\":1,\"native_requested\":" << (NEXO_IOP_RUNTIME_PROBE_NATIVE ? "true" : "false")
               << ",\"startup_observed\":" << (accepted ? "true" : "false")
               << ",\"cycles\":" << snapshot.emulatorCycles << ",\"instructions\":" << snapshot.emulatorInstructions
               << ",\"native_instructions\":" << snapshot.nativeInstructions
               << ",\"interpreted_instructions\":" << snapshot.interpretedInstructions
               << ",\"native_faults\":" << snapshot.nativeFaults
               << ",\"loaded_modules\":" << snapshot.emulatorLoadedModules
               << ",\"threads\":" << snapshot.emulatorThreads << ",\"rpc_servers\":" << snapshot.emulatorRpcServers
               << ",\"execution_error\":" << jsonString(executionError) << ",\"module_loads\":[";
        for (size_t i = 0; i < loads.size(); ++i)
        {
            if (i) report << ',';
            report << "{\"module_id\":" << loads[i].moduleId << ",\"start_result\":" << loads[i].startResult << '}';
        }
        report << "],\"diagnostics\":[";
        for (size_t i = 0; i < snapshot.diagnostics.size(); ++i)
        {
            if (i) report << ',';
            report << jsonString(snapshot.diagnostics[i]);
        }
        report << "],\"window_initialized\":false,\"scope\":\"runtime adapter startup and RAM only; EE/VU, presentation, hidden state, hardware and whole game unqualified\"}\n";
        report.close();
        if (!report) throw std::runtime_error("cannot write runtime probe report");
        return accepted ? 0 : 1;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

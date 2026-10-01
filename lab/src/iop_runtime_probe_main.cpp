#include "nexo/iop_lab_io.h"
#include "ps2_runtime.h"
#include "ps2_iop_transport.h"

#include <algorithm>
#include <iostream>

// This probe constructs the actual runtime/IOP adapter, initializes RAM only,
// and never calls the window/audio initialization or executes EE/VU code.
int main(int argc, char **argv)
{
    const bool sequence = argc >= 2 && std::string_view(argv[1]) == "--sequence";
    if ((!sequence && argc != 3) || (sequence && (argc < 5 || argc > 36)))
    {
        std::cerr << "Usage: nexo_iop_runtime_probe IRX NEW_OUTPUT_DIRECTORY\n"
                  << "       nexo_iop_runtime_probe --sequence NEW_OUTPUT_DIRECTORY EE_CYCLES_PER_LOAD IRX [IRX ...]\n";
        return 2;
    }
    try
    {
        using namespace ps2native::nexo::iop_lab;
        uint64_t eeCycles = 0u;
        std::vector<std::filesystem::path> sources;
        if (sequence)
        {
            size_t consumed = 0u;
            const std::string_view value(argv[3]);
            eeCycles = std::stoull(std::string(value), &consumed, 10);
            if (value.empty() || value.front() == '-' || consumed != value.size() || eeCycles > 16'000'000u)
                throw std::runtime_error("EE cycles per load outside 0..16000000");
            for (int index = 4; index < argc; ++index)
                sources.push_back(std::filesystem::absolute(argv[index]));
        }
        else
            sources.assign(2u, std::filesystem::absolute(argv[1]));
        const std::filesystem::path output(argv[2]);
        if (std::filesystem::exists(output))
            throw std::runtime_error("output directory already exists");
        for (const auto &source : sources)
        {
            if (source.parent_path() != sources.front().parent_path())
                throw std::runtime_error("sequence modules must share a host directory");
            (void)readImage(source); // Same bounded input admission as the lab probe.
        }
        if (!std::filesystem::create_directory(output))
            throw std::runtime_error("cannot create output directory");
        PS2Runtime::IoPaths paths{};
        paths.hostRoot = sources.front().parent_path(); paths.cdRoot = sources.front().parent_path();
        paths.elfDirectory = sources.front().parent_path(); paths.mcRoot = output / "memory-card";
        PS2Runtime::setIoPaths(paths);
        PS2Runtime runtime;
        if (!runtime.memory().initialize())
            throw std::runtime_error("cannot initialize runtime RAM");
        std::vector<ps2x::iop::ModuleLoadResult> loads;
        std::vector<ps2x::iop::DebugSnapshot> stages;
        uint64_t scheduledInstructions = 0u;
        bool budgetUnqualified = false;
        std::string executionError;
        try
        {
            for (const auto &source : sources)
            {
                const auto beforeLoad = runtime.iopDebugSnapshot().emulatorInstructions;
                loads.push_back(runtime.loadIopModule("host:" + source.filename().string()));
                const auto beforeSchedule = runtime.iopDebugSnapshot().emulatorInstructions;
                budgetUnqualified |= beforeSchedule - beforeLoad >= 2'000'000u;
                if (loads.back().moduleId > 0 && !budgetUnqualified)
                    PS2IopTransport::advanceEeCycles(&runtime, eeCycles);
                stages.push_back(runtime.iopDebugSnapshot());
                scheduledInstructions += stages.back().emulatorInstructions - beforeSchedule;
                if (loads.back().moduleId < 0 || stages.back().nativeFaults || budgetUnqualified) break;
            }
        }
        catch (const std::exception &error) { executionError = error.what(); }
        const auto snapshot = runtime.iopDebugSnapshot();
        const bool startupsResident = std::all_of(loads.begin(), loads.end(), [](const auto &loaded)
        {
            return loaded.moduleId > 0 && (loaded.startResult == 0 || loaded.startResult == 2);
        });
        const bool accepted = loads.size() == sources.size() && startupsResident &&
                              snapshot.nativeFaults == 0u && executionError.empty() &&
                              !budgetUnqualified &&
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
               << ",\"requested_module_loads\":" << sources.size()
               << ",\"ee_cycles_per_load\":" << eeCycles
               << ",\"scheduled_instructions\":" << scheduledInstructions
               << ",\"unqualified_budget\":" << (budgetUnqualified ? "true" : "false")
               << ",\"execution_error\":" << jsonString(executionError) << ",\"module_loads\":[";
        for (size_t i = 0; i < loads.size(); ++i)
        {
            if (i) report << ',';
            report << "{\"module_id\":" << loads[i].moduleId << ",\"start_result\":" << loads[i].startResult
                   << ",\"source_basename\":" << jsonString(sources[i].filename().string());
            if (i < stages.size())
                report << ",\"instructions_after_schedule\":" << stages[i].emulatorInstructions
                       << ",\"threads_after_schedule\":" << stages[i].emulatorThreads
                       << ",\"rpc_servers_after_schedule\":" << stages[i].emulatorRpcServers
                       << ",\"native_faults_after_schedule\":" << stages[i].nativeFaults;
            report << '}';
        }
        report << "],\"diagnostics\":[";
        for (size_t i = 0; i < snapshot.diagnostics.size(); ++i)
        {
            if (i) report << ',';
            report << jsonString(snapshot.diagnostics[i]);
        }
        report << "],\"window_initialized\":false,\"scope\":\"runtime adapter module sequence and bounded IOP scheduling only; no EE/VU execution, presentation, hidden-state or hardware/whole-game qualification\"}\n";
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

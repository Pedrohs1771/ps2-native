#include "nexo/iop_lab_io.h"
#include "nexo/iop_lab_host.h"
#include "emulator/core/iop_native.h"
#include "emulator/core/iop_memory.h"
#include "ps2x/iop/iop_subsystem.h"

#include <iostream>

#if NEXO_IOP_NATIVE
const ps2x::iop::detail::IopNativeProgram &compiledIopProgram();
#endif

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        std::cerr << "Usage: nexo_iop_probe IRX NEW_OUTPUT_DIRECTORY\n";
        return 2;
    }
    try
    {
        using namespace ps2native::nexo::iop_lab;
        const std::filesystem::path output(argv[2]);
        if (std::filesystem::exists(output))
            throw std::runtime_error("output directory already exists");
        Host host(readImage(argv[1]));
#if NEXO_IOP_NATIVE
        ps2x::iop::IopSubsystem iop(host, compiledIopProgram());
#else
        ps2x::iop::IopSubsystem iop(host);
#endif
        ps2x::iop::ModuleLoadResult loaded{true, -1, -1};
        std::string executionError;
        try { loaded = iop.loadModule("host:probe.irx"); }
        catch (const std::exception &error) { executionError = error.what(); }
        const auto snapshot = iop.debugSnapshot();
        const bool budgetUnqualified = snapshot.emulatorInstructions >= 2'000'000u;
        const bool accepted = loaded.moduleId > 0 && snapshot.nativeFaults == 0u &&
                              host.unsupported.empty() && executionError.empty() && !budgetUnqualified;
        if (!std::filesystem::create_directory(output))
            throw std::runtime_error("cannot create output directory");
        std::vector<uint8_t> ram(ps2x::iop::detail::IopMemory::RamSize);
        if (!iop.readMemory(0, ram.data(), ram.size()))
            throw std::runtime_error("cannot capture physical IOP RAM");
        writeBytes(output / "iop-ram.bin", ram);
        writeBytes(output / "ee-ram.bin", host.eeRam);
        std::ofstream report(output / "report.json");
        report << "{\n\"schema_version\":1,\"native\":" << (NEXO_IOP_NATIVE ? "true" : "false")
               << ",\"startup_observed\":" << (accepted ? "true" : "false")
               << ",\"module_id\":" << loaded.moduleId << ",\"start_result\":" << loaded.startResult
               << ",\"cycles\":" << snapshot.emulatorCycles << ",\"instructions\":" << snapshot.emulatorInstructions
               << ",\"native_instructions\":" << snapshot.nativeInstructions
               << ",\"interpreted_instructions\":" << snapshot.interpretedInstructions
               << ",\"native_faults\":" << snapshot.nativeFaults
               << ",\"loaded_modules\":" << snapshot.emulatorLoadedModules
               << ",\"threads\":" << snapshot.emulatorThreads << ",\"rpc_servers\":" << snapshot.emulatorRpcServers
               << ",\"unqualified_budget\":" << (budgetUnqualified ? "true" : "false")
               << ",\"unsupported_host_operation\":" << jsonString(host.unsupported)
               << ",\"execution_error\":" << jsonString(executionError)
               << ",\"dropped_logs\":" << host.droppedLogs << ",\"logs\":[";
        for (size_t i = 0; i != host.logs.size(); ++i)
        {
            if (i) report << ',';
            report << jsonString(host.logs[i]);
        }
        report << "],\"diagnostics\":[";
        for (size_t i = 0; i != snapshot.diagnostics.size(); ++i)
        {
            if (i) report << ',';
            report << jsonString(snapshot.diagnostics[i]);
        }
        report << "],\"scope\":\"startup observation and RAM only; hidden state, hardware fidelity, services and whole game unqualified\"}\n";
        report.close();
        if (!report) throw std::runtime_error("cannot write probe report");
        return accepted ? 0 : 1;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

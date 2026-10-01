#include "nexo/iop_lab_io.h"
#include "emulator/core/iop_memory.h"
#include "emulator/services/iop_module_loader.h"

#include <iostream>

int main(int argc, char **argv)
{
    if (argc != 3 && argc != 4)
    {
        std::cerr << "Usage: nexo_iop_inspect IRX NEW_OUTPUT_DIRECTORY [MODULE_CURSOR]\n";
        return 2;
    }
    try
    {
        using namespace ps2native::nexo::iop_lab;
        using namespace ps2x::iop::detail;
        const auto image = readImage(argv[1]);
        const uint32_t cursor = argc == 4 ? parseCursor(argv[3]) : 0x10000u;
        const std::filesystem::path output(argv[2]);
        if (std::filesystem::exists(output))
            throw std::runtime_error("output directory already exists");
        IopMemory memory;
        const auto loaded = IopModuleLoader::load(image, memory, cursor);
        if (!loaded || !loaded.relocationsComplete)
            throw std::runtime_error("IRX load/relocations incomplete; no native bank emitted");
        if ((loaded.base & 3u) != 0u || (loaded.size & 3u) != 0u || loaded.size == 0u ||
            loaded.base >= IopMemory::RamSize || loaded.size > IopMemory::RamSize - loaded.base ||
            (loaded.entry & 3u) != 0u || loaded.entry < loaded.base || loaded.entry >= loaded.base + loaded.size)
            throw std::runtime_error("loaded module range/entry cannot form a native bank");
        if (!std::filesystem::create_directory(output))
            throw std::runtime_error("cannot create output directory");
        try
        {
            writeBytes(output / "relocated-ram.bin", memory.ram().subspan(loaded.base, loaded.size));
            std::ofstream metadata(output / "module.json");
            metadata << "{\n\"schema_version\":1,\"base\":" << loaded.base
                     << ",\"size\":" << loaded.size << ",\"entry\":" << loaded.entry
                     << ",\"gp\":" << loaded.gp << ",\"next_module_cursor\":" << loaded.nextModuleCursor
                     << ",\"image_bytes\":" << image.size()
                     << ",\"relocations_complete\":true,\"instruction_words\":" << loaded.size / 4u
                     << ",\"scope\":\"identified loader, absolute relocated bank, no guest execution\"\n}\n";
            metadata.close();
            if (!metadata)
                throw std::runtime_error("cannot write module metadata");
        }
        catch (...)
        {
            std::filesystem::remove_all(output);
            throw;
        }
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

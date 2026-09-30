#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <map>

namespace ps2x::iop
{
    class IopHost;
}

namespace ps2x::iop::detail
{
    struct IopCpuState;
    class IopGuestExecutor;
    class IopMemory;

    class IopIoman
    {
    public:
        IopIoman(IopHost &host, IopMemory &memory) noexcept;
        ~IopIoman();

        IopIoman(const IopIoman &) = delete;
        IopIoman &operator=(const IopIoman &) = delete;

        void reset();
        [[nodiscard]] bool dispatchImport(uint16_t ordinal, IopCpuState &cpu, IopGuestExecutor &executor);

    private:
        struct Device
        {
            uint32_t address = 0u;
            uint32_t gp = 0u;
            std::string name;
        };

        struct File
        {
            uint64_t handle;
            uint64_t size;
            uint64_t position;
        };

        IopHost &m_host;
        IopMemory &m_memory;
        std::vector<Device> m_devices;
        std::map<uint32_t, File> m_files;
    };
}

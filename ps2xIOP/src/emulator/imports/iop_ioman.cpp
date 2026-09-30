#include "iop_ioman.h"

#include "../core/iop_cpu.h"
#include "../core/iop_memory.h"
#include "../services/iop_rpc.h"
#include "ps2x/iop/iop_host.h"

#include <algorithm>
#include <limits>

namespace ps2x::iop::detail
{
    IopIoman::IopIoman(IopHost &host, IopMemory &memory) noexcept
        : m_host(host), m_memory(memory)
    {
    }

    IopIoman::~IopIoman()
    {
        reset();
    }

    void IopIoman::reset()
    {
        for (const auto &[id, file] : m_files)
            m_host.closeHostFile(file.handle);
        m_files.clear();
        m_devices.clear();
    }

    bool IopIoman::dispatchImport(uint16_t ordinal, IopCpuState &cpu, IopGuestExecutor &executor)
    {
        constexpr size_t kMaxDevices = 16u;
        const uint32_t a0 = cpu.gpr[4];
        const auto setV0 = [&](uint32_t value)
        {
            cpu.gpr[2] = value;
        };

        switch (ordinal)
        {
        case 4: // open(path, flags): read-only CD/host files through the host VFS.
        {
            if ((cpu.gpr[5] & 3u) != 1u || (cpu.gpr[5] & 0xF00u) != 0u)
            {
                setV0(static_cast<uint32_t>(-30)); // EROFS
                return true;
            }
            const std::string path = m_memory.readString(a0, 1024u);
            const std::string translated = m_host.translateGuestPath(path);
            if (path.empty() || path.size() == 1024u || translated.empty())
            {
                setV0(static_cast<uint32_t>(-2)); // ENOENT
                return true;
            }
            uint32_t id = 3u; // Reserve the standard descriptors.
            while (m_files.contains(id) && id < 67u)
                ++id;
            if (id == 67u)
            {
                setV0(static_cast<uint32_t>(-24)); // EMFILE
                return true;
            }
            const uint64_t handle = m_host.openHostFile(translated);
            uint64_t size = 0u;
            if (handle == 0u || !m_host.hostFileSize(handle, size))
            {
                if (handle != 0u)
                    m_host.closeHostFile(handle);
                setV0(static_cast<uint32_t>(-2));
                return true;
            }
            m_files.emplace(id, File{handle, size, 0u});
            setV0(id);
            return true;
        }
        case 5: // close
        {
            const auto file = m_files.find(a0);
            if (file == m_files.end())
                setV0(static_cast<uint32_t>(-9)); // EBADF
            else
            {
                m_host.closeHostFile(file->second.handle);
                m_files.erase(file);
                setV0(0u);
            }
            return true;
        }
        case 6: // read(fd, IOP buffer, byte count)
        {
            const auto file = m_files.find(a0);
            const uint32_t destination = cpu.gpr[5];
            const uint32_t count = cpu.gpr[6];
            if (file == m_files.end())
                setV0(static_cast<uint32_t>(-9));
            else if (static_cast<int32_t>(count) < 0 ||
                     IopMemory::physicalAddress(destination) > IopMemory::RamSize ||
                     count > IopMemory::RamSize - IopMemory::physicalAddress(destination))
                setV0(static_cast<uint32_t>(-22)); // EINVAL
            else
            {
                const uint64_t remaining = file->second.size > file->second.position
                                               ? file->second.size - file->second.position : 0u;
                const size_t requested = static_cast<size_t>(std::min<uint64_t>(count, remaining));
                std::vector<uint8_t> buffer(requested);
                size_t read = 0u;
                if (requested == 0u)
                    setV0(0u);
                else if (!m_host.readHostFile(file->second.handle, file->second.position,
                                             buffer.data(), requested, read) || read > requested ||
                         !m_memory.writeRam(destination, buffer.data(), read))
                    setV0(static_cast<uint32_t>(-5)); // EIO
                else
                {
                    file->second.position += read;
                    setV0(static_cast<uint32_t>(read));
                }
            }
            return true;
        }
        case 7: // write: the current host file bridge is read-only.
            setV0(static_cast<uint32_t>(m_files.contains(a0) ? -30 : -9));
            return true;
        case 8: // lseek(fd, signed offset, SEEK_SET/CUR/END)
        {
            const auto file = m_files.find(a0);
            if (file == m_files.end())
            {
                setV0(static_cast<uint32_t>(-9));
                return true;
            }
            const uint32_t whence = cpu.gpr[6];
            const uint64_t origin = whence == 1u ? file->second.position : (whence == 2u ? file->second.size : 0u);
            const int64_t offset = static_cast<int32_t>(cpu.gpr[5]);
            if (whence > 2u || origin > static_cast<uint64_t>(INT32_MAX) + (1ull << 31u))
                setV0(static_cast<uint32_t>(-22));
            else
            {
                const int64_t position = static_cast<int64_t>(origin) + offset;
                if (position < 0 || position > INT32_MAX)
                    setV0(static_cast<uint32_t>(-22));
                else
                {
                    file->second.position = static_cast<uint64_t>(position);
                    setV0(static_cast<uint32_t>(position));
                }
            }
            return true;
        }
        case 20: // AddDrv
        {
            if (a0 == 0u || m_devices.size() >= kMaxDevices)
            {
                setV0(0xFFFFFFFFu);
                return true;
            }

            const uint32_t nameAddress = m_memory.read32(a0);
            const uint32_t operations = m_memory.read32(a0 + 16u);
            const std::string name = m_memory.readString(nameAddress, 64u);
            if (nameAddress == 0u || operations == 0u || name.empty())
            {
                setV0(0xFFFFFFFFu);
                return true;
            }

            m_devices.push_back({a0, cpu.gpr[28], name});
            const uint32_t init = m_memory.read32(operations);
            if (init != 0u)
            {
                const int32_t result = static_cast<int32_t>(
                    executor.executeGuestFunction(init, a0, 0u, 0u, 0u, cpu.gpr[28]));
                if (result < 0)
                {
                    m_devices.pop_back();
                    setV0(0xFFFFFFFFu);
                    return true;
                }
            }

            setV0(0u);
            return true;
        }
        case 21: // DelDrv
        {
            const std::string name = m_memory.readString(a0, 64u);
            const auto device = std::find_if(
                m_devices.begin(), m_devices.end(),
                [&](const Device &candidate)
                { return candidate.name == name; });
            if (device == m_devices.end())
            {
                setV0(0xFFFFFFFFu);
                return true;
            }

            const uint32_t operations = m_memory.read32(device->address + 16u);
            const uint32_t deinit = operations != 0u ? m_memory.read32(operations + 4u) : 0u;
            if (deinit != 0u)
                (void)executor.executeGuestFunction(deinit, device->address, 0u, 0u, 0u, device->gp);
            m_devices.erase(device);
            setV0(0u);
            return true;
        }
        default:
            return false;
        }
    }
}

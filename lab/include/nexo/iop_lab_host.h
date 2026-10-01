#pragma once

#include "ps2x/iop/iop_host.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <utility>
#include <vector>

namespace ps2native::nexo::iop_lab
{
    // A bounded startup/RPC laboratory host, not a game environment. Unknown
    // external operations are explicit failures, never fake successful services.
    class Host final : public ps2x::iop::IopHost
    {
    public:
        explicit Host(std::vector<uint8_t> module)
            : image(std::move(module)), eeRam(32u * 1024u * 1024u, 0u) {}

        bool readGuest(uint32_t address, void *destination, size_t size) const override
        {
            address &= 0x1fffffffu;
            if ((!destination && size) || address > eeRam.size() || size > eeRam.size() - address)
                return false;
            if (size) std::memcpy(destination, eeRam.data() + address, size);
            return true;
        }
        bool writeGuest(uint32_t address, const void *source, size_t size) override
        {
            address &= 0x1fffffffu;
            if ((!source && size) || address > eeRam.size() || size > eeRam.size() - address)
                return false;
            if (size) std::memcpy(eeRam.data() + address, source, size);
            return true;
        }
        bool zeroGuest(uint32_t address, size_t size) override
        {
            address &= 0x1fffffffu;
            if (address > eeRam.size() || size > eeRam.size() - address) return false;
            std::fill_n(eeRam.begin() + address, size, uint8_t{0}); return true;
        }
        bool normalizeGuestAddress(uint32_t address, uint32_t &normalized) const override
        {
            normalized = address & 0x1fffffffu;
            return normalized < eeRam.size();
        }
        bool readIopMemory(uint32_t, void *, size_t) const override { reject("host IOP RAM read"); }
        bool writeIopMemory(uint32_t, const void *, size_t) override { reject("host IOP RAM write"); }
        bool zeroIopMemory(uint32_t, size_t) override { reject("host IOP RAM zero"); }
        bool normalizeIopAddress(uint32_t, uint32_t &) const override { reject("host IOP address normalization"); }
        uint32_t allocateIopHandle(ps2x::iop::IopHandleKind) override { reject("IOP handle allocation"); }
        uint32_t allocateGuest(uint32_t, uint32_t) override { reject("EE allocation"); }
        void freeGuest(uint32_t) override { reject("EE allocation release"); }
        void audioCommand(uint32_t, uint32_t, ps2x::iop::GuestBuffer, ps2x::iop::GuestBuffer) override { reject("audio command"); }
        std::string hostPath(ps2x::iop::HostPathKind) const override { reject("external host path"); }
        std::string translateGuestPath(std::string_view path) const override { return std::string(path); }
        bool searchCdFile(std::string_view, uint32_t, ps2x::iop::CdFileInfo &) override { reject("CD file search"); }
        uint64_t openHostFile(std::string_view path) override
        {
            if (path != "host:probe.irx") reject("external file open");
            return 1u;
        }
        bool hostFileSize(uint64_t handle, uint64_t &size) const override
        {
            if (handle != 1u) return false;
            size = image.size(); return true;
        }
        bool readHostFile(uint64_t handle, uint64_t offset, void *destination, size_t size, size_t &bytesRead) override
        {
            bytesRead = 0;
            if (handle != 1u || (!destination && size) || offset > image.size() || size > image.size() - offset)
                return false;
            if (size) std::memcpy(destination, image.data() + offset, size);
            bytesRead = size; return true;
        }
        void closeHostFile(uint64_t) override {}
        int32_t memoryCard(const ps2x::iop::MemoryCardRequest &) override { reject("memory card operation"); }
        bool hasGuestFunction(uint32_t) const override { return false; }
        bool invokeGuestFunction(uint64_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t *) override
        {
            reject("EE function invocation");
        }
        bool sendSifCommand(uint32_t, const void *, size_t) override { reject("SIF command delivery to EE"); }
        void log(ps2x::iop::LogLevel, std::string_view message) override
        {
            if (logs.size() < 256u) logs.emplace_back(message);
            else ++droppedLogs;
        }

        std::vector<uint8_t> image;
        std::vector<uint8_t> eeRam;
        std::vector<std::string> logs;
        size_t droppedLogs = 0;
        mutable std::string unsupported;

    private:
        [[noreturn]] void reject(std::string_view operation) const
        {
            unsupported = operation;
            throw std::runtime_error("laboratory host does not implement " + std::string(operation));
        }
    };
}

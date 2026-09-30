#include "emulator/core/iop_cpu.h"
#include "emulator/core/iop_kernel.h"
#include "emulator/core/iop_memory.h"
#include "emulator/imports/iop_cdvd.h"
#include "emulator/imports/iop_imports.h"
#include "emulator/imports/iop_loadcore.h"
#include "emulator/imports/iop_timrman.h"
#include "emulator/imports/iop_sysclib.h"
#include "emulator/imports/iop_ioman.h"
#include "emulator/services/iop_rpc.h"
#include "ps2x/iop/iop_host.h"

#include <cstdint>
#include <chrono>
#include <cstring>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <string_view>

namespace
{
    using namespace ps2x::iop;
    using namespace ps2x::iop::detail;

    constexpr uint32_t kExportMagic = 0x41C00000u;
    constexpr int32_t kLibraryNotFound = -213;
    constexpr int32_t kIllegalLibrary = -214;

    class NullHost : public IopHost
    {
    public:
        bool readGuest(uint32_t, void *, size_t) const override { return false; }
        bool writeGuest(uint32_t, const void *, size_t) override { return false; }
        bool zeroGuest(uint32_t, size_t) override { return false; }
        bool normalizeGuestAddress(uint32_t, uint32_t &) const override { return false; }
        uint32_t allocateIopHandle(IopHandleKind) override { return 1u; }
        uint32_t allocateGuest(uint32_t, uint32_t) override { return 0u; }
        void freeGuest(uint32_t) override {}
        void audioCommand(uint32_t, uint32_t, GuestBuffer, GuestBuffer) override {}
        std::string hostPath(HostPathKind) const override { return {}; }
        std::string translateGuestPath(std::string_view path) const override { return std::string(path); }
        uint64_t openHostFile(std::string_view) override { return 0u; }
        bool hostFileSize(uint64_t, uint64_t &) const override { return false; }
        bool readHostFile(uint64_t, uint64_t, void *, size_t, size_t &) override { return false; }
        void closeHostFile(uint64_t) override {}
        int32_t memoryCard(const MemoryCardRequest &) override { return 0; }
        bool hasGuestFunction(uint32_t) const override { return false; }
        bool invokeGuestFunction(uint64_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t *) override { return false; }
        void log(LogLevel, std::string_view) override {}
    };

    class CdRootHost final : public NullHost
    {
    public:
        explicit CdRootHost(std::filesystem::path rootPath)
            : root(std::move(rootPath))
        {
        }

        std::string hostPath(HostPathKind kind) const override
        {
            return kind == HostPathKind::CdRoot ? root.string() : std::string{};
        }

    private:
        std::filesystem::path root;
    };

    class RecordingExecutor final : public IopGuestExecutor
    {
    public:
        uint32_t executeGuestFunction(uint32_t address,
                                      uint32_t a0,
                                      uint32_t,
                                      uint32_t,
                                      uint32_t,
                                      uint32_t gp) override
        {
            ++calls;
            lastAddress = address;
            lastArgument = a0;
            lastGp = gp;
            return callbackResult;
        }

        uint32_t callbackResult = 0u;
        uint32_t calls = 0u;
        uint32_t lastAddress = 0u;
        uint32_t lastArgument = 0u;
        uint32_t lastGp = 0u;
    };

    bool expect(bool condition, std::string_view message)
    {
        if (condition)
            return true;
        std::cerr << "FAIL: " << message << '\n';
        return false;
    }

    bool testLoadcoreRebootLibraryMode()
    {
        IopMemory memory;
        IopImportRegistry imports(memory);
        IopLoadcore loadcore(memory, imports);

        IopCpuState cpu{};
        cpu.gpr[4] = 0u;
        cpu.gpr[5] = 2u;
        if (!expect(loadcore.dispatchImport(27u, cpu), "loadcore:27 was not handled") ||
            !expect(static_cast<int32_t>(cpu.gpr[2]) == kIllegalLibrary,
                    "loadcore:27 did not reject a null export table"))
            return false;

        constexpr uint32_t table = 0x1000u;
        memory.write32(table, kExportMagic);
        memory.write16(table + 8u, 0x0101u);
        memory.write16(table + 10u, 0x1234u);
        const char name[8] = {'t', 'e', 's', 't', 'l', 'i', 'b', '\0'};
        (void)memory.writeRam(table + 12u, name, sizeof(name));
        memory.write32(table + 20u, 0u);

        cpu = {};
        cpu.gpr[4] = table;
        cpu.gpr[5] = 2u;
        if (!expect(loadcore.dispatchImport(27u, cpu), "loadcore:27 rejected a valid export table") ||
            !expect(cpu.gpr[2] == 0u, "loadcore:27 returned an error for a valid export table") ||
            !expect(memory.read16(table + 10u) == 0x1232u,
                    "loadcore:27 did not replace only export mode bits 1 and 2"))
            return false;

        constexpr uint32_t invalidTable = 0x1100u;
        memory.write32(invalidTable, 0xDEADBEEFu);
        cpu = {};
        cpu.gpr[4] = invalidTable;
        cpu.gpr[5] = 6u;
        if (!expect(loadcore.dispatchImport(27u, cpu), "loadcore:27 did not consume an invalid-table call") ||
            !expect(static_cast<int32_t>(cpu.gpr[2]) == kLibraryNotFound,
                    "loadcore:27 returned the wrong invalid-table error"))
            return false;

        if (!expect(imports.registerExportTable(table), "test export table did not register"))
            return false;
        memory.write32(table, 0u);
        cpu = {};
        cpu.gpr[4] = table;
        cpu.gpr[5] = 6u;
        return expect(loadcore.dispatchImport(27u, cpu), "loadcore:27 rejected a registered table") &&
               expect(cpu.gpr[2] == 0u, "loadcore:27 returned an error for a registered table") &&
               expect(memory.read16(table + 10u) == 0x1236u,
                      "loadcore:27 did not update a registered table's mode");
    }

    bool pollEvent(IopKernel &kernel, int eventId, uint32_t bits, uint32_t resultAddress, int32_t expected)
    {
        IopCpuState cpu{};
        cpu.gpr[4] = static_cast<uint32_t>(eventId);
        cpu.gpr[5] = bits;
        cpu.gpr[6] = 0u; // WEF_AND
        cpu.gpr[7] = resultAddress;
        return expect(kernel.dispatchEventImport(11u, cpu), "PollEventFlag was not handled") &&
               expect(static_cast<int32_t>(cpu.gpr[2]) == expected, "PollEventFlag returned an unexpected result");
    }

    bool testCdvdSpecialControl()
    {
        NullHost host;
        IopMemory memory;
        IopKernel kernel(memory);
        kernel.reset();
        IopCdvd cdvd(host, memory, kernel);
        cdvd.reset();

        constexpr uint32_t param = 0x2000u;
        constexpr uint32_t eventResult = 0x2010u;
        IopCpuState cpu{};
        cpu.gpr[4] = static_cast<uint32_t>(-11); // sceCdSC: return cdvdman interrupt event flag
        cpu.gpr[5] = param;
        if (!expect(cdvd.dispatchImport(50u, cpu), "cdvdman:50 was not handled") ||
            !expect(static_cast<int32_t>(cpu.gpr[2]) > 0, "sceCdSC(-11) did not return a valid event flag"))
            return false;
        const int eventId = static_cast<int>(cpu.gpr[2]);

        if (!pollEvent(kernel, eventId, 0x29u, eventResult, 0) ||
            !expect(memory.read32(eventResult) == 0x29u, "cdvdman event flag did not start with bits 0x29"))
            return false;

        IopCpuState clear{};
        clear.gpr[4] = static_cast<uint32_t>(eventId);
        clear.gpr[5] = ~0x29u;
        if (!expect(kernel.dispatchEventImport(8u, clear), "ClearEventFlag was not handled") ||
            !pollEvent(kernel, eventId, 0x29u, eventResult, -418))
            return false;

        cpu = {};
        cpu.gpr[4] = 0x12345u;
        if (!expect(cdvd.dispatchImport(7u, cpu), "sceCdSeek was not handled") ||
            !pollEvent(kernel, eventId, 0x29u, eventResult, 0))
            return false;

        memory.write8(param, 0x30u);
        cpu = {};
        cpu.gpr[4] = static_cast<uint32_t>(-2);
        cpu.gpr[5] = param;
        if (!expect(cdvd.dispatchImport(50u, cpu), "sceCdSC(-2) was not handled") ||
            !expect(cpu.gpr[2] == 0x30u, "sceCdSC(-2) did not store the low-byte error"))
            return false;

        memory.write32(param, 0u);
        cpu = {};
        cpu.gpr[4] = static_cast<uint32_t>(-1);
        cpu.gpr[5] = param;
        if (!expect(cdvd.dispatchImport(50u, cpu), "sceCdSC(-1) was not handled") ||
            !expect(cpu.gpr[2] == 0u, "sceCdSC(-1) returned the wrong initial stream state") ||
            !expect(memory.read32(param) == 0x30u, "sceCdSC(-1) did not publish the last error"))
            return false;

        cpu = {};
        cpu.gpr[4] = 2u;
        cpu.gpr[5] = param;
        if (!expect(cdvd.dispatchImport(50u, cpu), "sceCdSC(2) was not handled") ||
            !expect(cpu.gpr[2] == 2u, "sceCdSC(2) did not update the stream state"))
            return false;

        cpu = {};
        cpu.gpr[4] = static_cast<uint32_t>(-1);
        cpu.gpr[5] = param;
        return expect(cdvd.dispatchImport(50u, cpu), "second sceCdSC(-1) was not handled") &&
               expect(cpu.gpr[2] == 2u, "sceCdSC(-1) did not preserve the stream state");
    }

    bool testCdvdGetReadPos()
    {
        NullHost host;
        IopMemory memory;
        IopKernel kernel(memory);
        kernel.reset();
        IopCdvd cdvd(host, memory, kernel);
        cdvd.reset();

        IopCpuState seek{};
        seek.gpr[4] = 0x12345u;
        if (!expect(cdvd.dispatchImport(7u, seek), "sceCdSeek was not handled"))
            return false;

        IopCpuState getReadPos{};
        return expect(cdvd.dispatchImport(44u, getReadPos), "cdvdman:44 (sceCdGetReadPos) was not handled") &&
               expect(getReadPos.gpr[2] == 0u, "an idle seek must not report bytes being read");
    }

    bool testCdvdTrayRequest()
    {
        NullHost host;
        IopMemory memory;
        IopKernel kernel(memory);
        kernel.reset();
        IopCdvd cdvd(host, memory, kernel);
        cdvd.reset();
        constexpr uint32_t countAddress = 0x2480u;
        memory.write32(countAddress, 0xFFFFFFFFu);
        IopCpuState poll{};
        poll.gpr[4] = 2u;
        poll.gpr[5] = countAddress;
        return expect(cdvd.dispatchImport(14u, poll), "cdvdman:14 (sceCdTrayReq) was not handled") &&
               expect(poll.gpr[2] == 1u, "checking a closed mounted disc must succeed") &&
               expect(memory.read32(countAddress) == 0u, "a stable disc must report no tray changes");
    }

    bool testCdvdInitialDiscChange()
    {
        CdRootHost host("mounted-disc-root");
        IopMemory memory;
        IopKernel kernel(memory);
        kernel.reset();
        IopCdvd cdvd(host, memory, kernel);
        cdvd.reset();
        constexpr uint32_t countAddress = 0x2480u;
        memory.write32(countAddress, 0xFFFFFFFFu);
        IopCpuState poll{};
        poll.gpr[4] = 2u;
        poll.gpr[5] = countAddress;
        if (!expect(cdvd.dispatchImport(14u, poll) && poll.gpr[2] == 1u,
                    "initial mounted-disc query failed") ||
            !expect(memory.read32(countAddress) == 1u, "a newly mounted disc must trigger filesystem initialization"))
            return false;
        return expect(cdvd.dispatchImport(14u, poll) && poll.gpr[2] == 1u,
                      "second mounted-disc query failed") &&
               expect(memory.read32(countAddress) == 0u, "the disc-change notification must be consumed once");
    }

    bool testCdvdSearchFile()
    {
        const auto suffix = std::to_string(
            static_cast<unsigned long long>(std::chrono::steady_clock::now().time_since_epoch().count()));
        const std::filesystem::path root =
            std::filesystem::temp_directory_path() / ("ps2x-iop-cdvd-search-" + suffix);
        const std::filesystem::path movieDirectory = root / "MOVIE";
        const std::filesystem::path moviePath = movieDirectory / "OPENING.PSS";
        std::error_code error;
        std::filesystem::create_directories(movieDirectory, error);
        if (!expect(!error, "could not create the temporary CD root"))
            return false;
        {
            std::ofstream movie(moviePath, std::ios::binary);
            movie.write("PSS!", 4);
        }

        CdRootHost host(root);
        IopMemory memory;
        IopKernel kernel(memory);
        kernel.reset();
        IopCdvd cdvd(host, memory, kernel);
        cdvd.reset();

        constexpr uint32_t resultAddress = 0x2400u;
        constexpr uint32_t pathAddress = 0x2480u;
        const char path[] = "cdrom0:\\movie\\opening.pss;1";
        (void)memory.writeRam(pathAddress, path, sizeof(path));

        IopCpuState cpu{};
        cpu.gpr[4] = resultAddress;
        cpu.gpr[5] = pathAddress;
        const bool handled = cdvd.dispatchImport(10u, cpu);
        const bool passed =
            expect(handled, "cdvdman:10 was not handled") &&
            expect(cpu.gpr[2] == 1u, "sceCdSearchFile did not find a case-insensitive ISO path") &&
            expect(memory.read32(resultAddress) >= 20u, "sceCdSearchFile returned an invalid LSN") &&
            expect(memory.read32(resultAddress + 4u) == 4u, "sceCdSearchFile returned the wrong size") &&
            expect(memory.readString(resultAddress + 8u, 16u) == "OPENING.PSS",
                   "sceCdSearchFile returned the wrong file name");

        std::filesystem::remove_all(root, error);
        return passed;
    }

    bool testTimrmanPeriodicCallback()
    {
        IopTimrman timrman;
        timrman.reset();
        IopCpuState cpu{};

        cpu.gpr[4] = 1u; // SYSCLK
        cpu.gpr[5] = 32u;
        cpu.gpr[6] = 1u;
        if (!expect(timrman.dispatchImport(4u, cpu, 100u), "AllocHardTimer was not handled") ||
            !expect(static_cast<int32_t>(cpu.gpr[2]) > 0, "AllocHardTimer did not allocate a 32-bit timer"))
            return false;
        const uint32_t timerId = cpu.gpr[2];

        cpu = {};
        cpu.gpr[4] = timerId;
        cpu.gpr[5] = 100u;
        cpu.gpr[6] = 0x12340u;
        cpu.gpr[7] = 0x45670u;
        cpu.gpr[28] = 0x89AB0u;
        if (!expect(timrman.dispatchImport(20u, cpu, 100u), "SetTimerHandler was not handled") ||
            !expect(cpu.gpr[2] == 0u, "SetTimerHandler failed"))
            return false;

        cpu = {};
        cpu.gpr[4] = timerId;
        cpu.gpr[5] = 1u;
        cpu.gpr[6] = 0u;
        cpu.gpr[7] = 1u;
        if (!expect(timrman.dispatchImport(22u, cpu, 100u), "SetupHardTimer was not handled") ||
            !expect(cpu.gpr[2] == 0u, "SetupHardTimer failed"))
            return false;

        cpu = {};
        cpu.gpr[4] = timerId;
        if (!expect(timrman.dispatchImport(23u, cpu, 100u), "StartHardTimer was not handled") ||
            !expect(cpu.gpr[2] == 0u, "StartHardTimer failed") ||
            !expect(timrman.nextEventCycle(1000u) == 200u, "timer compare was scheduled at the wrong cycle"))
            return false;

        RecordingExecutor executor;
        executor.callbackResult = 100u;
        timrman.serviceDue(199u, executor);
        if (!expect(executor.calls == 0u, "timer callback ran too early"))
            return false;
        timrman.serviceDue(200u, executor);
        return expect(executor.calls == 1u, "timer callback did not run") &&
               expect(executor.lastAddress == 0x12340u, "timer called the wrong handler") &&
               expect(executor.lastArgument == 0x45670u, "timer passed the wrong common argument") &&
               expect(executor.lastGp == 0x89AB0u, "timer callback lost the registering module GP") &&
               expect(timrman.nextEventCycle(1000u) == 300u, "timer callback return did not rearm compare");
    }

    bool testThreadAlarms()
    {
        IopMemory memory;
        IopKernel kernel(memory);
        RecordingExecutor executor;
        IopCpuState cpu{};
        memory.write32(0x1000u, 10u);
        memory.write32(0x1004u, 0u);
        cpu.gpr[4] = 0x1000u;
        cpu.gpr[5] = 0x12340u;
        cpu.gpr[6] = 0x88u;
        cpu.gpr[28] = 0x44u;
        if (!expect(kernel.dispatchThreadImport(35u, cpu, 100u) && cpu.gpr[2] == 0u,
                    "SetAlarm failed") ||
            !expect(kernel.nextWakeCycle(1000u) == 110u, "Idle IOP missed alarm deadline")) return false;
        kernel.serviceAlarms(109u, executor);
        if (!expect(executor.calls == 0u, "Alarm fired early")) return false;
        kernel.serviceAlarms(110u, executor);
        if (!expect(executor.calls == 1u && executor.lastAddress == 0x12340u &&
                    executor.lastArgument == 0x88u && executor.lastGp == 0x44u,
                    "Alarm callback ABI mismatch") ||
            !expect(kernel.nextWakeCycle(1000u) == 1000u, "One-shot alarm remained active")) return false;
        executor.callbackResult = 15u;
        (void)kernel.dispatchThreadImport(36u, cpu, 110u);
        kernel.serviceAlarms(120u, executor);
        if (!expect(executor.calls == 2u && kernel.nextWakeCycle(1000u) == 135u,
                    "Periodic alarm did not use callback return ticks")) return false;
        cpu.gpr[4] = 0x12340u;
        cpu.gpr[5] = 0x88u;
        (void)kernel.dispatchThreadImport(38u, cpu, 121u);
        kernel.serviceAlarms(135u, executor);
        if (!expect(cpu.gpr[2] == 0u && executor.calls == 2u, "iCancelAlarm did not cancel")) return false;
        memory.write32(0x1004u, 1u);
        cpu.gpr[4] = 0x1000u;
        cpu.gpr[5] = 0x12340u;
        cpu.gpr[6] = 0x88u;
        (void)kernel.dispatchThreadImport(35u, cpu, 200u);
        if (!expect(kernel.nextWakeCycle(UINT64_MAX) == 200u + (1ull << 32u) + 10u,
                    "Alarm lost the high clock word")) return false;
        kernel.reset();
        return expect(kernel.nextWakeCycle(1000u) == 1000u, "Reset retained an alarm");
    }

    bool testSysclibWordMemoryByteCount()
    {
        IopMemory memory;
        IopSysclib library(memory);
        IopCpuState cpu{};
        for (uint32_t i = 0; i < 32; i += 4)
            memory.write32(0x1000u + i, 0xABCDEF01u);
        cpu.gpr[4] = 0x1000u;
        cpu.gpr[5] = 0x12345678u;
        cpu.gpr[6] = 8u;
        if (!expect(library.dispatchImport(41u, cpu), "wmemset not handled") ||
            !expect(memory.read32(0x1004u) == 0x12345678u && memory.read32(0x1008u) == 0xABCDEF01u,
                    "wmemset treated its byte count as a word count")) return false;
        cpu.gpr[4] = 0x2000u;
        cpu.gpr[5] = 0x1000u;
        cpu.gpr[6] = 8u;
        memory.write32(0x2008u, 0xDEADBEEFu);
        if (!expect(library.dispatchImport(40u, cpu) && memory.read32(0x2004u) == 0x12345678u &&
                      memory.read32(0x2008u) == 0xDEADBEEFu,
                      "wmemcopy overwrote bytes outside the requested buffer")) return false;
        for (uint32_t count : {0u, 1u, 3u, 5u, 7u})
        {
            for (uint32_t ordinal : {40u, 41u})
            {
                memory.write32(0x2000u, 0xDEADBEEFu);
                memory.write32(0x2004u, 0xDEADBEEFu);
                cpu.gpr[4] = 0x2000u;
                cpu.gpr[5] = ordinal == 40u ? 0x1000u : 0x12345678u;
                cpu.gpr[6] = count;
                if (!expect(library.dispatchImport(ordinal, cpu) && cpu.gpr[2] == 0x2000u &&
                            memory.read32(0x2000u) == (count >= 4u ? 0x12345678u : 0xDEADBEEFu) &&
                            memory.read32(0x2004u) == 0xDEADBEEFu,
                            "word memory helper changed a trailing partial word")) return false;
            }
        }
        cpu.gpr[4] = 0x10000u;
        cpu.gpr[5] = 0u;
        cpu.gpr[6] = 0x1000u;
        memory.write32(0x10FFCu, 0xFFFFFFFFu);
        memory.write32(0x11000u, 0xDEADBEEFu);
        return expect(library.dispatchImport(41u, cpu) && memory.read32(0x10FFCu) == 0u &&
                      memory.read32(0x11000u) == 0xDEADBEEFu,
                      "4096-byte wmemset corrupted the adjacent allocation");
    }

    bool testSpuAutoDmaSampleTiming()
    {
        IopMemory memory;
        for (uint32_t core : {0u, 1u})
        {
            const uint32_t chcr = core == 0u ? 0xBF8010C8u : 0xBF801508u;
            const uint32_t admas = 0xBF9001B0u + core * 0x400u;
            memory.write32(chcr - 4u, (32u << 16u) | 16u); // 2048-byte stereo PCM buffer.
            memory.write16(admas, static_cast<uint16_t>(core + 1u));
            memory.write32(chcr, 0x01000201u);
            const auto pcm = memory.takeDmaStart();
            if (!expect(pcm && pcm->delayCycles == 512u * 768u,
                        "SPU AutoDMA completed at RAM DMA speed instead of PCM sample cadence")) return false;
            memory.write16(admas, 0u);
            memory.write32(chcr, 0x01000201u);
            const auto normal = memory.takeDmaStart();
            if (!expect(normal && normal->delayCycles == 1024u,
                        "ordinary SPU DMA incorrectly used PCM timing")) return false;
        }
        return true;
    }

    bool testIopHeapReusesFreedBuffers()
    {
        IopMemory memory;
        const uint32_t capacity = IopMemory::HeapLimit - IopMemory::HeapBase;
        for (unsigned i = 0; i < 1000u; ++i)
        {
            const uint32_t address = memory.allocate(0x1000u, 64u);
            if (!expect(address != 0u && memory.freeAllocation(address),
                        "temporary IOP buffers exhausted the heap after being freed")) return false;
        }
        if (!expect(memory.maxFreeMemory() == capacity, "free memory ignored released allocations")) return false;
        const uint32_t first = memory.allocate(0x1000u);
        const uint32_t middle = memory.allocate(0x1000u);
        const uint32_t tail = memory.allocate(capacity - 0x2000u);
        if (!expect(first && middle && tail && memory.allocate(16u) == 0u, "heap fixture did not fill RAM")) return false;
        if (!expect(memory.freeAllocation(middle) && memory.allocate(0x1000u) == middle,
                    "IOP allocator failed to reuse a hole between live allocations")) return false;
        return expect(memory.allocate(UINT32_MAX) == 0u && memory.allocate(16u, 6u) == 0u &&
                      memory.allocate(32u, 16u, UINT32_MAX - 15u) == 0u,
                      "IOP allocator accepted overflowing size/address or invalid alignment");
    }

    bool testIomanRejectsInvalidFileDescriptors()
    {
        NullHost host;
        IopMemory memory;
        IopIoman library(host, memory);
        RecordingExecutor executor;
        IopCpuState cpu{};
        cpu.gpr[4] = 0u;
        cpu.gpr[5] = 0x1000u;
        cpu.gpr[6] = 4u;
        return expect(library.dispatchImport(6u, cpu, executor) && static_cast<int32_t>(cpu.gpr[2]) == -9,
                      "IOMAN read silently succeeded with an invalid file descriptor");
    }

    bool testIomanHostFileReadSeekAndReset()
    {
        class FileHost final : public NullHost
        {
        public:
            std::string translateGuestPath(std::string_view path) const override
            { return path == "host0:test.bin" ? "/test.bin" : ""; }
            uint64_t openHostFile(std::string_view path) override
            { return path == "/test.bin" ? 42u : 0u; }
            bool hostFileSize(uint64_t handle, uint64_t &size) const override
            { size = contents.size(); return handle == 42u; }
            bool readHostFile(uint64_t handle, uint64_t offset, void *destination,
                              size_t size, size_t &read) override
            {
                if (handle != 42u || offset > contents.size()) return false;
                read = std::min(size, contents.size() - static_cast<size_t>(offset));
                std::memcpy(destination, contents.data() + offset, read);
                return true;
            }
            void closeHostFile(uint64_t handle) override { if (handle == 42u) ++closed; }
            const std::string contents = "PS2IOP";
            unsigned closed = 0u;
        } host;
        IopMemory memory;
        IopIoman library(host, memory);
        RecordingExecutor executor;
        IopCpuState cpu{};
        const std::string path = "host0:test.bin";
        (void)memory.writeRam(0x1000u, path.c_str(), path.size() + 1u);
        const auto call = [&](uint16_t ordinal, uint32_t a0, uint32_t a1, uint32_t a2)
        {
            cpu.gpr[4] = a0; cpu.gpr[5] = a1; cpu.gpr[6] = a2;
            if (!library.dispatchImport(ordinal, cpu, executor)) return INT32_MIN;
            return static_cast<int32_t>(cpu.gpr[2]);
        };
        if (!expect(call(4u, 0x1000u, 2u, 0u) == -30, "IOMAN accepted writable host open")) return false;
        const int32_t fd = call(4u, 0x1000u, 1u, 0u);
        memory.write8(0x2002u, 0xFFu);
        if (!expect(fd == 3 && call(6u, fd, 0x2000u, 2u) == 2 &&
                    memory.read8(0x2000u) == 'P' && memory.read8(0x2001u) == 'S' && memory.read8(0x2002u) == 0xFFu,
                    "IOMAN read did not write exactly the requested physical IOP bytes")) return false;
        if (!expect(call(8u, fd, 1u, 1u) == 3 && call(6u, fd, 0x2000u, 10u) == 3 &&
                    memory.readString(0x2000u, 3u) == "IOP",
                    "IOMAN relative seek or short read failed")) return false;
        if (!expect(call(8u, fd, UINT32_MAX, 2u) == 5 && call(6u, fd, 0x2000u, 1u) == 1 &&
                    memory.read8(0x2000u) == 'P' && call(6u, fd, 0x2000u, 1u) == 0,
                    "IOMAN end-relative seek or EOF failed")) return false;
        if (!expect(call(8u, fd, UINT32_MAX, 0u) == -22 && call(8u, fd, 0u, 3u) == -22 &&
                    call(6u, fd, 0x1FFFFFu, 2u) == -22 && call(6u, fd, 0x2000u, UINT32_MAX) == -22,
                    "IOMAN accepted invalid seek or out-of-bounds read")) return false;
        if (!expect(call(5u, fd, 0u, 0u) == 0 && host.closed == 1u &&
                    call(6u, fd, 0x2000u, 1u) == -9 && call(4u, 0x1000u, 1u, 0u) == fd,
                    "IOMAN close did not invalidate and recycle its descriptor")) return false;
        library.reset();
        return expect(host.closed == 2u && call(6u, fd, 0x2000u, 1u) == -9,
                      "IOP reset retained an open host file");
    }

    bool testSifCommandTablesMirrorGuestMemory()
    {
        NullHost host;
        IopMemory memory;
        IopKernel kernel(memory);
        IopRpcBridge bridge(host, memory, kernel);
        IopCpuState cpu{};
        cpu.gpr[4] = 0x1000u; cpu.gpr[5] = 16u;
        if (!expect(bridge.dispatchSifCmdImport(8u, cpu), "SetCmdBuffer not handled")) return false;
        cpu.gpr[4] = 0u; cpu.gpr[5] = 0x12340u; cpu.gpr[6] = 0x55u; cpu.gpr[28] = 0x66u;
        if (!expect(bridge.dispatchSifCmdImport(10u, cpu) && memory.read32(0x1000u) == 0x12340u &&
                    memory.read32(0x1004u) == 0x55u,
                    "SIF callback was invisible in the IOP command table")) return false;
        if (!expect(bridge.dispatchSifCmdImport(11u, cpu) && memory.read32(0x1000u) == 0u,
                    "SIF RemoveCmdHandler left the guest slot occupied")) return false;
        cpu.gpr[4] = 0x2000u; cpu.gpr[5] = 4u;
        (void)bridge.dispatchSifCmdImport(9u, cpu);
        cpu.gpr[4] = 0x80000001u; cpu.gpr[5] = 0x12340u; cpu.gpr[6] = 0x77u;
        if (!expect(bridge.dispatchSifCmdImport(10u, cpu) && memory.read32(0x2008u) == 0x12340u &&
                    memory.read32(0x200Cu) == 0x77u && memory.read32(0x1008u) == 0u,
                    "SIF system and user command tables aliased")) return false;
        RecordingExecutor executor;
        uint32_t packet[4]{};
        if (!expect(bridge.receiveSifCommand(0x80000001u, packet, sizeof(packet), executor) &&
                    executor.lastAddress == 0x12340u && executor.lastGp == 0x66u,
                    "SIF callback lost the registration GP")) return false;
        memory.write32(0x2008u, 0u);
        if (!expect(!bridge.receiveSifCommand(0x80000001u, packet, sizeof(packet), executor),
                    "SIF dispatched a command cleared directly in guest RAM")) return false;
        memory.write32(0x2008u, 0x12340u);
        bridge.removeServersInRange(0x12300u, 0x100u);
        return expect(memory.read32(0x2008u) == 0u &&
                      !bridge.receiveSifCommand(0x80000001u, packet, sizeof(packet), executor),
                      "Unloading an IOP module left its command callback active");
    }

}

int main()
{
    if (!testLoadcoreRebootLibraryMode() || !testCdvdSpecialControl() || !testCdvdGetReadPos() ||
        !testCdvdTrayRequest() || !testCdvdInitialDiscChange() || !testCdvdSearchFile() ||
        !testTimrmanPeriodicCallback() || !testThreadAlarms() || !testSysclibWordMemoryByteCount() ||
        !testSpuAutoDmaSampleTiming() || !testIopHeapReusesFreedBuffers() ||
        !testIomanRejectsInvalidFileDescriptors() || !testIomanHostFileReadSeekAndReset() ||
        !testSifCommandTablesMirrorGuestMemory())
        return 1;
    std::cout << "ps2xIOP import tests passed\n";
    return 0;
}

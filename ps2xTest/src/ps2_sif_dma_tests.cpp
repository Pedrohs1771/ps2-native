#include "MiniTest.h"
#include "ps2_runtime.h"
#include "ps2_iop_host.h"
#include "ps2_iop_transport.h"
#include "ps2_syscalls.h"
#include "ps2_stubs.h"
#include "Kernel/Stubs/SIF.h"
#include "runtime/ee_scheduler.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

namespace ps2_stubs
{
    void resetSifState();
}

namespace
{
    constexpr int KE_OK = 0;

    struct TestEnv
    {
        std::vector<uint8_t> rdram;
        R5900Context ctx{};
        PS2Runtime runtime;

        TestEnv() : rdram(PS2_RAM_SIZE, 0u)
        {
            ps2_stubs::resetSifState();
            std::memset(&ctx, 0, sizeof(ctx));
        }
    };

    #pragma pack(push, 1)
    struct Ps2SifDmaTransfer
    {
        uint32_t src;
        uint32_t dest;
        int32_t size;
        int32_t attr;
    };

    struct SifRpcHeader
    {
        uint32_t pkt_addr;
        uint32_t rpc_id;
        int32_t sema_id;
        uint32_t mode;
    };

    struct SifRpcReceiveData
    {
        SifRpcHeader hdr;
        uint32_t src;
        uint32_t dest;
        int32_t size;
    };
    #pragma pack(pop)

    static_assert(sizeof(Ps2SifDmaTransfer) == 16u, "Unexpected Ps2SifDmaTransfer size.");
    static_assert(sizeof(SifRpcReceiveData) == 28u, "Unexpected SifRpcReceiveData size.");

    void setRegU32(R5900Context &ctx, int reg, uint32_t value)
    {
        ctx.r[reg] = _mm_set_epi64x(0, static_cast<int64_t>(value));
    }

    int32_t getRegS32(const R5900Context &ctx, int reg)
    {
        return static_cast<int32_t>(::getRegU32(&ctx, reg));
    }

    void writeGuestU32(uint8_t *rdram, uint32_t addr, uint32_t value)
    {
        std::memcpy(rdram + addr, &value, sizeof(value));
    }

    uint32_t readGuestU32(const uint8_t *rdram, uint32_t addr)
    {
        uint32_t value = 0;
        std::memcpy(&value, rdram + addr, sizeof(value));
        return value;
    }

    uint32_t g_dmacHandlerWriteAddr = 0u;
    uint32_t g_dmacHandlerValue = 0u;
    uint32_t g_dmacHandlerLastCause = 0u;
    uint32_t g_dmacHandlerLastArg = 0u;
    int32_t g_sifDmaResult = 0;

    constexpr uint32_t kSchedulerSifDmaEntryPc = 0x00101000u;
    constexpr uint32_t kSchedulerSifDmaResumePc = 0x00101010u;
    constexpr uint32_t kSchedulerSifDmaHandlerPc = 0x00101020u;
    constexpr uint32_t kSchedulerSifDmaDescAddr = 0x00020300u;
    constexpr uint32_t kSchedulerSifDmaHandlerArg = 0x12345678u;

    void testDmacHandler(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        (void)runtime;
        g_dmacHandlerLastCause = ::getRegU32(ctx, 4);
        g_dmacHandlerLastArg = ::getRegU32(ctx, 5);
        if (g_dmacHandlerWriteAddr != 0u)
        {
            writeGuestU32(rdram, g_dmacHandlerWriteAddr, g_dmacHandlerValue);
        }
        ctx->pc = 0u;
    }

    void schedulerSifDmaEntry(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        runtime->eeScheduler().addIrqHandler(true,
                                             5u,
                                             kSchedulerSifDmaHandlerPc,
                                             true,
                                             kSchedulerSifDmaHandlerArg,
                                             0u,
                                             0u);
        setRegU32(*ctx, 4, kSchedulerSifDmaDescAddr);
        setRegU32(*ctx, 5, 1u);
        ctx->pc = kSchedulerSifDmaResumePc;
        ps2_stubs::sceSifSetDma(rdram, ctx, runtime);
    }

    void schedulerSifDmaResume(uint8_t *, R5900Context *ctx, PS2Runtime *runtime)
    {
        g_sifDmaResult = getRegS32(*ctx, 2);
        ctx->pc = 0u;
        runtime->requestStop();
    }
}

void register_ps2_sif_dma_tests()
{
    MiniTest::Case("SIF command transport", [](TestCase &suite)
    {
        suite.Run("EE command handler tables remain visible in guest memory", [](TestCase &t)
        {
            TestEnv env;
            setRegU32(env.ctx, 4, 0x1000u); setRegU32(env.ctx, 5, 16u);
            ps2_stubs::sceSifSetCmdBuffer(env.rdram.data(), &env.ctx, &env.runtime);
            setRegU32(env.ctx, 4, 0u); setRegU32(env.ctx, 5, 0x12340u); setRegU32(env.ctx, 6, 0x55u);
            ps2_stubs::sceSifAddCmdHandler(env.rdram.data(), &env.ctx, &env.runtime);
            uint32_t function = 0u, argument = 0u;
            std::memcpy(&function, env.rdram.data() + 0x1000u, 4u);
            std::memcpy(&argument, env.rdram.data() + 0x1004u, 4u);
            t.Equals(function, 0x12340u, "AddCmdHandler marks the guest slot occupied");
            t.Equals(argument, 0x55u, "Guest table retains handler userdata");
            ps2_stubs::sceSifRemoveCmdHandler(env.rdram.data(), &env.ctx, &env.runtime);
            std::memcpy(&function, env.rdram.data() + 0x1000u, 4u);
            t.Equals(function, 0u, "RemoveCmdHandler releases the guest slot");
            t.Equals(readGuestU32(env.rdram.data(), 0x1004u), 0x55u, "Removing the function preserves userdata");

            setRegU32(env.ctx, 4, 0x2000u); setRegU32(env.ctx, 5, 4u);
            ps2_stubs::sceSifSetSysCmdBuffer(env.rdram.data(), &env.ctx, &env.runtime);
            setRegU32(env.ctx, 4, 0x80000001u); setRegU32(env.ctx, 5, 0x12340u); setRegU32(env.ctx, 6, 0x77u);
            ps2_stubs::sceSifAddCmdHandler(env.rdram.data(), &env.ctx, &env.runtime);
            t.Equals(readGuestU32(env.rdram.data(), 0x2008u), 0x12340u, "System IDs select their separate table");
            t.Equals(readGuestU32(env.rdram.data(), 0x200Cu), 0x77u, "System handler userdata remains visible");
            t.Equals(readGuestU32(env.rdram.data(), 0x1008u), 0u, "System registration leaves the user table intact");

            writeGuestU32(env.rdram.data(), 0x2020u, 0xABCDEFu);
            setRegU32(env.ctx, 4, 0x80000004u);
            ps2_stubs::sceSifAddCmdHandler(env.rdram.data(), &env.ctx, &env.runtime);
            t.Equals(getRegS32(env.ctx, 2), -1, "Rejects a command outside the configured table");
            t.Equals(readGuestU32(env.rdram.data(), 0x2020u), 0xABCDEFu, "Out-of-range registration preserves adjacent RAM");
            setRegU32(env.ctx, 4, PS2_RAM_SIZE - 8u); setRegU32(env.ctx, 5, 4u);
            ps2_stubs::sceSifSetSysCmdBuffer(env.rdram.data(), &env.ctx, &env.runtime);
            t.Equals(getRegU32(&env.ctx, 2), 0x2000u, "Invalid replacement retains the prior table");
        });
        suite.Run("EE SIF dispatch observes prepopulated tables and direct guest edits", [](TestCase &t)
        {
            TestEnv env;
            constexpr uint32_t handler = kSchedulerSifDmaHandlerPc;
            t.IsTrue(env.runtime.registerFunction(handler, testDmacHandler), "Registers a native callback");
            writeGuestU32(env.rdram.data(), 0x1000u, handler);
            writeGuestU32(env.rdram.data(), 0x1004u, 0x55u);
            setRegU32(env.ctx, 4, 0x1000u); setRegU32(env.ctx, 5, 16u);
            ps2_stubs::sceSifSetCmdBuffer(env.rdram.data(), &env.ctx, &env.runtime);
            const std::array<uint32_t, 4> packet{};
            t.IsTrue(ps2_stubs::dispatchSifCommand(env.rdram.data(), &env.runtime, 0u, packet.data(), sizeof(packet)),
                     "Dispatches a callback already present when the table is installed");
            writeGuestU32(env.rdram.data(), 0x1000u, 0u);
            t.IsFalse(ps2_stubs::dispatchSifCommand(env.rdram.data(), &env.runtime, 0u, packet.data(), sizeof(packet)),
                      "Direct guest removal disables dispatch despite the cached registration");
            writeGuestU32(env.rdram.data(), 0x1008u, handler);
            t.IsTrue(ps2_stubs::dispatchSifCommand(env.rdram.data(), &env.runtime, 1u, packet.data(), sizeof(packet)),
                     "Direct guest insertion enables a previously empty slot");
        });
        suite.Run("EE SendCmd uses n32 extra arguments and the IOP address space", [](TestCase &t)
        {
            TestEnv env;
            constexpr uint32_t packet = 0x1000, source = 0x2000, size = 16;
            const uint32_t destination = env.runtime.allocateIopMemory(size, 16);
            t.IsTrue(destination != 0, "Allocates IOP destination");
            std::array<uint8_t, size> payload{};
            payload.fill(0x5a);
            std::memcpy(env.rdram.data() + source, payload.data(), size);
            std::memset(env.rdram.data() + destination, 0xa5, size);
            const auto setup = [&]
            {
                setRegU32(env.ctx, 4, 0x2a);
                setRegU32(env.ctx, 5, packet);
                setRegU32(env.ctx, 6, 20);
                setRegU32(env.ctx, 7, source);
                setRegU32(env.ctx, 8, destination);
                setRegU32(env.ctx, 9, size);
                setRegU32(env.ctx, 29, 0x400);
            };
            setup();
            ps2_stubs::sceSifSendCmd(env.rdram.data(), &env.ctx, &env.runtime);
            std::array<uint8_t, size> copied{};
            t.IsTrue(env.runtime.readIopMemory(destination, copied.data(), size) && copied == payload,
                     "Stub copies extra data into physical IOP RAM");
            t.Equals(env.rdram[destination], uint8_t(0xa5), "Equal-numbered EE address stays intact");
            env.runtime.zeroIopMemory(destination, size);
            setup();
            ps2_syscalls::sceSifSendCmd(env.rdram.data(), &env.ctx, &env.runtime);
            t.IsTrue(env.runtime.readIopMemory(destination, copied.data(), size) && copied == payload,
                     "Syscall and stub use the same directional transport");
            t.Equals(env.rdram[destination], uint8_t(0xa5), "Syscall does not alias IOP and EE memory");
            env.runtime.zeroIopMemory(destination, size);
            setup();
            setRegU32(env.ctx, 6, 8);
            ps2_stubs::sceSifSendCmd(env.rdram.data(), &env.ctx, &env.runtime);
            t.Equals(getRegU32(&env.ctx, 2), 0u, "Rejects a truncated command header");
        });
    });
    MiniTest::Case("PS2SifDma", [](TestCase &tc)
    {
        tc.Run("sceSifSetDma copies payload and sceSifDmaStat reports complete", [](TestCase &t)
        {
            TestEnv env;

            constexpr uint32_t kDescAddr = 0x00020000u;
            constexpr uint32_t kSrcAddr = 0x00020100u;
            constexpr uint32_t kDstAddr = 0x00020200u;

            std::array<uint8_t, 16> payload{};
            for (size_t i = 0; i < payload.size(); ++i)
            {
                payload[i] = static_cast<uint8_t>(0x30u + i);
            }
            std::memcpy(env.rdram.data() + kSrcAddr, payload.data(), payload.size());
            std::memset(env.rdram.data() + kDstAddr, 0x5A, payload.size());

            const Ps2SifDmaTransfer desc{
                kSrcAddr,
                kDstAddr,
                static_cast<int32_t>(payload.size()),
                0};
            std::memcpy(env.rdram.data() + kDescAddr, &desc, sizeof(desc));

            setRegU32(env.ctx, 4, kDescAddr);
            setRegU32(env.ctx, 5, 1u);
            ps2_stubs::sceSifSetDma(env.rdram.data(), &env.ctx, &env.runtime);
            const int32_t dmaId = getRegS32(env.ctx, 2);
            t.IsTrue(dmaId > 0, "sceSifSetDma should return a positive transfer id on success");

            std::array<uint8_t, 16> iopReadback{};
            t.IsTrue(env.runtime.readIopMemory(kDstAddr, iopReadback.data(), iopReadback.size()) &&
                         iopReadback == payload,
                     "sceSifSetDma should copy EE payload into IOP RAM");
            const std::array<uint8_t, 16> eeSentinel = {
                0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A,
                0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A};
            t.IsTrue(std::memcmp(env.rdram.data() + kDstAddr, eeSentinel.data(), eeSentinel.size()) == 0,
                     "sceSifSetDma must not alias an equal-numbered EE address");

            setRegU32(env.ctx, 4, static_cast<uint32_t>(dmaId));
            ps2_stubs::sceSifDmaStat(env.rdram.data(), &env.ctx, &env.runtime);
            t.IsTrue(getRegS32(env.ctx, 2) < 0, "sceSifDmaStat should be negative when transfer is complete");
        });

        tc.Run("IOP heap DMA uses private backing instead of aliasing EE RDRAM", [](TestCase &t)
        {
            TestEnv env;

            constexpr uint32_t kDescAddr = 0x00020040u;
            constexpr uint32_t kSrcAddr = 0x00020140u;
            constexpr uint32_t kRoundTripAddr = 0x00020240u;
            constexpr uint32_t kIopBlockSize = 0x880u;

            std::array<uint8_t, 32> payload{};
            for (size_t i = 0; i < payload.size(); ++i)
            {
                payload[i] = static_cast<uint8_t>(0x80u + i);
            }
            std::memcpy(env.rdram.data() + kSrcAddr, payload.data(), payload.size());
            std::memset(env.rdram.data() + kRoundTripAddr, 0, payload.size());

            setRegU32(env.ctx, 4, kIopBlockSize);
            ps2_stubs::sceSifAllocIopHeap(env.rdram.data(), &env.ctx, &env.runtime);
            const uint32_t iopAddress = ::getRegU32(&env.ctx, 2);
            t.IsTrue(iopAddress >= 0x00120000u && iopAddress < 0x00200000u,
                     "sceSifAllocIopHeap should return an address in physical IOP RAM");
            std::memset(env.rdram.data() + iopAddress, 0x5Au, payload.size());

            Ps2SifDmaTransfer desc{
                kSrcAddr,
                iopAddress,
                static_cast<int32_t>(payload.size()),
                0};
            std::memcpy(env.rdram.data() + kDescAddr, &desc, sizeof(desc));
            setRegU32(env.ctx, 4, kDescAddr);
            setRegU32(env.ctx, 5, 1u);
            ps2_stubs::sceSifSetDma(env.rdram.data(), &env.ctx, &env.runtime);
            t.IsTrue(getRegS32(env.ctx, 2) > 0,
                     "EE-to-IOP DMA should accept a private IOP heap destination");

            const std::array<uint8_t, 32> aliasSentinel{
                0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A,
                0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A,
                0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A,
                0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A};
            t.IsTrue(std::memcmp(env.rdram.data() + iopAddress,
                                 aliasSentinel.data(), aliasSentinel.size()) == 0,
                     "IOP DMA must not overwrite the equal-numbered EE range");

            PS2IopHostAdapter host(env.runtime);
            auto scope = host.enterCall(&env.ctx, env.rdram.data());
            std::array<uint8_t, 32> hostReadback{};
            t.IsTrue(host.readIopMemory(iopAddress, hostReadback.data(), hostReadback.size()) &&
                         hostReadback == payload,
                     "IOP modules should read the shared physical IOP RAM");

            constexpr uint32_t kRdAddr = 0x00020340u;
            setRegU32(env.ctx, 4, kRdAddr);
            setRegU32(env.ctx, 5, iopAddress);
            setRegU32(env.ctx, 6, kRoundTripAddr);
            setRegU32(env.ctx, 7, static_cast<uint32_t>(payload.size()));
            ps2_stubs::sceSifGetOtherData(env.rdram.data(), &env.ctx, &env.runtime);
            t.Equals(getRegS32(env.ctx, 2), 0,
                     "IOP-to-EE transfer should accept a physical IOP source");
            t.IsTrue(std::memcmp(env.rdram.data() + kRoundTripAddr,
                                 payload.data(), payload.size()) == 0,
                     "IOP-to-EE DMA should round-trip the payload");
        });

        tc.Run("isceSifSetDma and isceSifSetDChain alias the SIF DMA helpers", [](TestCase &t)
        {
            TestEnv env;

            constexpr uint32_t kDescAddr = 0x00020240u;
            constexpr uint32_t kSrcAddr = 0x00020340u;
            constexpr uint32_t kDstAddr = 0x00020440u;

            std::array<uint8_t, 12> payload{};
            for (size_t i = 0; i < payload.size(); ++i)
            {
                payload[i] = static_cast<uint8_t>(0x50u + i);
            }
            std::memcpy(env.rdram.data() + kSrcAddr, payload.data(), payload.size());
            std::memset(env.rdram.data() + kDstAddr, 0x5A, payload.size());

            const Ps2SifDmaTransfer desc{
                kSrcAddr,
                kDstAddr,
                static_cast<int32_t>(payload.size()),
                0};
            std::memcpy(env.rdram.data() + kDescAddr, &desc, sizeof(desc));

            setRegU32(env.ctx, 4, kDescAddr);
            setRegU32(env.ctx, 5, 1u);
            ps2_stubs::isceSifSetDma(env.rdram.data(), &env.ctx, &env.runtime);
            t.IsTrue(getRegS32(env.ctx, 2) > 0, "isceSifSetDma should report a successful transfer id");
            std::array<uint8_t, 12> iopReadback{};
            t.IsTrue(env.runtime.readIopMemory(kDstAddr, iopReadback.data(), iopReadback.size()) &&
                         iopReadback == payload,
                     "isceSifSetDma should copy EE payload into IOP RAM");

            ps2_stubs::isceSifSetDChain(env.rdram.data(), &env.ctx, &env.runtime);
            t.Equals(getRegS32(env.ctx, 2), 0, "isceSifSetDChain should mirror sceSifSetDChain");
        });

        tc.Run("sceSifSetDma dispatches enabled DMAC handlers for cause 5", [](TestCase &t)
        {
            TestEnv env;

            constexpr uint32_t kSrcAddr = 0x00020400u;
            constexpr uint32_t kDstAddr = 0x00020500u;
            constexpr uint32_t kHandlerWriteAddr = 0x00020600u;

            g_dmacHandlerWriteAddr = kHandlerWriteAddr;
            g_dmacHandlerValue = 0xCAFEBABEu;
            g_dmacHandlerLastCause = 0u;
            g_dmacHandlerLastArg = 0u;
            g_sifDmaResult = 0;
            env.runtime.registerFunction(kSchedulerSifDmaEntryPc, schedulerSifDmaEntry);
            env.runtime.registerFunction(kSchedulerSifDmaResumePc, schedulerSifDmaResume);
            env.runtime.registerFunction(kSchedulerSifDmaHandlerPc, testDmacHandler);

            std::array<uint8_t, 16> payload{};
            for (size_t i = 0; i < payload.size(); ++i)
            {
                payload[i] = static_cast<uint8_t>(0x40u + i);
            }
            std::memcpy(env.rdram.data() + kSrcAddr, payload.data(), payload.size());

            const Ps2SifDmaTransfer desc{
                kSrcAddr,
                kDstAddr,
                static_cast<int32_t>(payload.size()),
                0};
            std::memcpy(env.rdram.data() + kSchedulerSifDmaDescAddr, &desc, sizeof(desc));

            R5900Context mainContext{};
            mainContext.pc = kSchedulerSifDmaEntryPc;
            env.runtime.eeScheduler().reset(env.rdram.data(), mainContext);
            env.runtime.eeScheduler().run();

            t.IsTrue(g_sifDmaResult > 0, "sceSifSetDma should still report success");
            t.Equals(readGuestU32(env.rdram.data(), kHandlerWriteAddr), g_dmacHandlerValue,
                     "the scheduler should execute the queued DMAC invocation");
            t.Equals(g_dmacHandlerLastCause, 5u, "DMAC handler should observe cause 5");
            t.Equals(g_dmacHandlerLastArg, kSchedulerSifDmaHandlerArg,
                     "DMAC handler should receive registered argument");
        });

        tc.Run("resetSifState seeds boot-ready SIF registers", [](TestCase &t)
        {
            TestEnv env;

            auto getReg = [&](uint32_t reg) -> uint32_t
            {
                setRegU32(env.ctx, 4, reg);
                ps2_stubs::sceSifGetReg(env.rdram.data(), &env.ctx, &env.runtime);
                return ::getRegU32(&env.ctx, 2);
            };

            t.Equals(getReg(0x4u), 0x00020000u, "SIF boot status register should expose ready bit by default");
            t.Equals(getReg(0x80000000u), 0u, "SIF main-address register should default to zero");
            t.Equals(getReg(0x80000001u), 0u, "SIF sub-address register should default to zero");
            t.Equals(getReg(0x80000002u), 0u, "SIF mscom register should default to zero");
        });

        tc.Run("sceSifExitCmd restores default boot-ready SIF registers", [](TestCase &t)
        {
            TestEnv env;

            setRegU32(env.ctx, 4, 0x4u);
            setRegU32(env.ctx, 5, 0x12340000u);
            ps2_stubs::sceSifSetReg(env.rdram.data(), &env.ctx, &env.runtime);

            setRegU32(env.ctx, 4, 0x80000002u);
            setRegU32(env.ctx, 5, 0x89ABCDEFu);
            ps2_stubs::sceSifSetReg(env.rdram.data(), &env.ctx, &env.runtime);

            ps2_stubs::sceSifExitCmd(env.rdram.data(), &env.ctx, &env.runtime);
            t.Equals(getRegS32(env.ctx, 2), 0, "sceSifExitCmd should succeed");

            auto getReg = [&](uint32_t reg) -> uint32_t
            {
                setRegU32(env.ctx, 4, reg);
                ps2_stubs::sceSifGetReg(env.rdram.data(), &env.ctx, &env.runtime);
                return ::getRegU32(&env.ctx, 2);
            };

            t.Equals(getReg(0x4u), 0x00020000u, "sceSifExitCmd should restore the boot-ready status bit");
            t.Equals(getReg(0x80000002u), 0u, "sceSifExitCmd should clear transient mscom state");
        });

        tc.Run("sceSifSetDma rejects invalid descriptors without partial writes", [](TestCase &t)
        {
            TestEnv env;

            constexpr uint32_t kDescAddr = 0x00021000u;
            constexpr uint32_t kSrcA = 0x00021100u;
            constexpr uint32_t kDstA = 0x00021200u;
            constexpr uint32_t kSrcB = 0x00021300u;
            constexpr uint32_t kInvalidDstB = 0xE0000100u; // unsupported guest segment

            std::array<uint8_t, 8> payloadA{};
            for (size_t i = 0; i < payloadA.size(); ++i)
            {
                payloadA[i] = static_cast<uint8_t>(0x70u + i);
            }
            std::array<uint8_t, 8> payloadB{};
            for (size_t i = 0; i < payloadB.size(); ++i)
            {
                payloadB[i] = static_cast<uint8_t>(0x90u + i);
            }

            std::memcpy(env.rdram.data() + kSrcA, payloadA.data(), payloadA.size());
            std::memcpy(env.rdram.data() + kSrcB, payloadB.data(), payloadB.size());
            std::memset(env.rdram.data() + kDstA, 0x5Au, payloadA.size());

            const Ps2SifDmaTransfer descs[2] = {
                {kSrcA, kDstA, static_cast<int32_t>(payloadA.size()), 0},
                {kSrcB, kInvalidDstB, static_cast<int32_t>(payloadB.size()), 0}};
            std::memcpy(env.rdram.data() + kDescAddr, descs, sizeof(descs));

            setRegU32(env.ctx, 4, kDescAddr);
            setRegU32(env.ctx, 5, 2u);
            ps2_stubs::sceSifSetDma(env.rdram.data(), &env.ctx, &env.runtime);
            t.Equals(getRegS32(env.ctx, 2), 0, "sceSifSetDma should fail when any descriptor is invalid");

            const std::array<uint8_t, 8> expectedUnchanged{
                0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A};
            t.IsTrue(std::memcmp(env.rdram.data() + kDstA, expectedUnchanged.data(), expectedUnchanged.size()) == 0,
                     "failed multi-descriptor sceSifSetDma should not partially write earlier descriptors");
        });

        tc.Run("sceSifSetDma enforces descriptor count limit", [](TestCase &t)
        {
            TestEnv env;
            constexpr uint32_t kDescAddr = 0x00022000u;

            setRegU32(env.ctx, 4, kDescAddr);
            setRegU32(env.ctx, 5, 33u);
            ps2_stubs::sceSifSetDma(env.rdram.data(), &env.ctx, &env.runtime);
            t.Equals(getRegS32(env.ctx, 2), 0, "sceSifSetDma should reject count > 32");
        });

        tc.Run("sceSifGetOtherData copies payload and writes receive metadata", [](TestCase &t)
        {
            TestEnv env;

            constexpr uint32_t kRdAddr = 0x00023000u;
            constexpr uint32_t kSrcAddr = 0x00023100u;
            constexpr uint32_t kDstAddr = 0x00023200u;
            constexpr uint32_t kSize = 20u;

            std::array<uint8_t, kSize> payload{};
            for (size_t i = 0; i < payload.size(); ++i)
            {
                payload[i] = static_cast<uint8_t>((i * 7u) & 0xFFu);
            }
            t.IsTrue(env.runtime.writeIopMemory(kSrcAddr, payload.data(), payload.size()),
                     "test setup should populate physical IOP RAM");
            std::memset(env.rdram.data() + kDstAddr, 0, payload.size());
            std::memset(env.rdram.data() + kRdAddr, 0, sizeof(SifRpcReceiveData));

            setRegU32(env.ctx, 4, kRdAddr);
            setRegU32(env.ctx, 5, kSrcAddr);
            setRegU32(env.ctx, 6, kDstAddr);
            setRegU32(env.ctx, 7, kSize);
            ps2_stubs::sceSifGetOtherData(env.rdram.data(), &env.ctx, &env.runtime);
            t.Equals(getRegS32(env.ctx, 2), 0, "sceSifGetOtherData should succeed for valid transfer");

            t.IsTrue(std::memcmp(env.rdram.data() + kDstAddr, payload.data(), payload.size()) == 0,
                     "sceSifGetOtherData should copy payload");

            const SifRpcReceiveData rd = *reinterpret_cast<const SifRpcReceiveData *>(env.rdram.data() + kRdAddr);
            t.Equals(rd.src, kSrcAddr, "receive metadata src should be populated");
            t.Equals(rd.dest, kDstAddr, "receive metadata dest should be populated");
            t.Equals(static_cast<uint32_t>(rd.size), kSize, "receive metadata size should be populated");
        });

        tc.Run("sceSifGetOtherData rejects unsupported guest segments", [](TestCase &t)
        {
            TestEnv env;

            constexpr uint32_t kRdAddr = 0x00024000u;
            constexpr uint32_t kDstAddr = 0x00024100u;
            constexpr uint32_t kInvalidSrcAddr = 0xE0000200u;
            constexpr uint32_t kSize = 16u;

            std::memset(env.rdram.data() + kDstAddr, 0xA5, kSize);
            writeGuestU32(env.rdram.data(), kRdAddr + 0x10u, 0x11111111u);
            writeGuestU32(env.rdram.data(), kRdAddr + 0x14u, 0x22222222u);
            writeGuestU32(env.rdram.data(), kRdAddr + 0x18u, 0x33333333u);

            setRegU32(env.ctx, 4, kRdAddr);
            setRegU32(env.ctx, 5, kInvalidSrcAddr);
            setRegU32(env.ctx, 6, kDstAddr);
            setRegU32(env.ctx, 7, kSize);
            ps2_stubs::sceSifGetOtherData(env.rdram.data(), &env.ctx, &env.runtime);
            t.Equals(getRegS32(env.ctx, 2), -1, "sceSifGetOtherData should fail for unsupported source segment");

            std::array<uint8_t, kSize> expected{};
            expected.fill(0xA5u);
            t.IsTrue(std::memcmp(env.rdram.data() + kDstAddr, expected.data(), expected.size()) == 0,
                     "failed sceSifGetOtherData should not modify destination");
            t.Equals(readGuestU32(env.rdram.data(), kRdAddr + 0x10u), 0x11111111u,
                     "failed sceSifGetOtherData should not overwrite rd metadata");
        });
    });
}

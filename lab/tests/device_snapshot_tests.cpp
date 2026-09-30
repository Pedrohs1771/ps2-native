#include "MiniTest.h"
#include "nexo/device_snapshot.h"
#include "nexo/canonical_binary.h"
#include "runtime/ps2_memory.h"
#include "runtime/gs/gs_frontend.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <vector>

using namespace ps2native::nexo;
namespace
{
void append(std::vector<uint8_t> &bytes, uint64_t value, unsigned width)
{ for (unsigned i = 0; i < width; ++i) bytes.push_back(uint8_t(value >> (8 * i))); }
void cmd(std::vector<uint8_t> &bytes, uint8_t opcode, uint16_t immediate = 0, uint8_t count = 0)
{ append(bytes, (uint32_t(opcode) << 24) | (uint32_t(count) << 16) | immediate, 4); }
std::vector<uint8_t> image(uint8_t value)
{
    std::vector<uint8_t> bytes;
    append(bytes, 1ull | (1ull << 15) | (2ull << 58), 8); append(bytes, 0, 8);
    bytes.insert(bytes.end(), 16, value); return bytes;
}
struct MemoryFixture
{
    PS2Memory memory;
    std::vector<std::vector<uint8_t>> packets;
    MemoryFixture()
    {
        if (!memory.initialize()) throw std::runtime_error("cannot initialize device fixture");
        memory.setGifPacketCallback([this](const uint8_t *p, uint32_t size) { packets.emplace_back(p, p + size); });
    }
};
void compareFragments(TestCase &t, const std::vector<uint8_t> &stream)
{
    for (size_t cut = 0; cut <= stream.size(); ++cut)
    {
        auto reference = std::make_unique<MemoryFixture>(); auto restored = std::make_unique<MemoryFixture>();
        reference->memory.processVIF1Data(stream.data(), cut);
        const auto checkpoint = Vif1SnapshotCodec::encode(reference->memory);
        Vif1SnapshotCodec::restore(restored->memory, checkpoint);
        std::memcpy(restored->memory.getVU1Code(), reference->memory.getVU1Code(), PS2_VU1_CODE_SIZE);
        std::memcpy(restored->memory.getVU1Data(), reference->memory.getVU1Data(), PS2_VU1_DATA_SIZE);
        reference->packets.clear();
        reference->memory.processVIF1Data(stream.data() + cut, stream.size() - cut);
        restored->memory.processVIF1Data(stream.data() + cut, stream.size() - cut);
        t.Equals(Vif1SnapshotCodec::encode(restored->memory), Vif1SnapshotCodec::encode(reference->memory),
            "All VIF parser/transport fields survive each byte boundary");
        t.IsTrue(std::memcmp(restored->memory.getVU1Data(), reference->memory.getVU1Data(), PS2_VU1_DATA_SIZE) == 0,
            "Restored UNPACK data matches continuation");
        t.IsTrue(std::memcmp(restored->memory.getVU1Code(), reference->memory.getVU1Code(), PS2_VU1_CODE_SIZE) == 0,
            "Restored MPG data matches continuation");
        t.Equals(restored->packets, reference->packets, "Restored DIRECT submits the same packet bytes");
    }
}
struct GsFixture
{
    PS2Memory memory;
    GS gs;
    GsFixture()
    {
        if (!memory.initialize()) throw std::runtime_error("cannot initialize GS fixture");
        gs.init(memory.getGSVRAM(), PS2_GS_VRAM_SIZE, &memory.gs());
    }
};
void resign(std::vector<uint8_t> &bytes) { binary::put32(bytes, 20, binary::checksum(bytes)); }
template <typename F> bool rejects(F &&f)
{
    try { f(); } catch (const std::invalid_argument &) { return true; }
    return false;
}
void setupSprite(GS &gs, uint64_t tex0, uint64_t scissor = 0)
{
    gs.writeRegister(GS_REG_FRAME_1, 1ull << 16);
    gs.writeRegister(GS_REG_ZBUF_1, 1ull << 32);
    gs.writeRegister(GS_REG_SCISSOR_1, scissor);
    gs.writeRegister(GS_REG_TEST_1, 0x30000);
    gs.writeRegister(GS_REG_TEX0_1, tex0);
    gs.writeRegister(GS_REG_PRIM, GS_PRIM_SPRITE | (1ull << 4) | (1ull << 8));
    gs.writeRegister(GS_REG_RGBAQ, 0x80808080);
}
void drawPixel(GS &gs, uint32_t x)
{
    gs.writeRegister(GS_REG_UV, 0); gs.writeRegister(GS_REG_XYZ2, uint64_t(x) * 16);
    gs.writeRegister(GS_REG_UV, 0);
    gs.writeRegister(GS_REG_XYZ2, uint64_t(x + 1) * 16 | (16ull << 16));
}
}

int main()
{
    MiniTest::Case("NEXO device checkpoints", [](TestCase &tc)
    {
        tc.Run("VIF UNPACK and MPG survive every fragmented input boundary", [](TestCase &t)
        {
            std::vector<uint8_t> bytes; cmd(bytes, 0x01, 0x0202); cmd(bytes, 0x6C, 3, 2);
            for (unsigned i = 0; i < 8; ++i) append(bytes, 0x14000000u + i, 4);
            cmd(bytes, 0x4A, 0, 2); for (unsigned i = 0; i < 4; ++i) append(bytes, 0x60000000u + i, 4);
            cmd(bytes, 0x07, 0x1234); compareFragments(t, bytes);
        });
        tc.Run("VIF DIRECTHL survives every header and payload boundary", [](TestCase &t)
        {
            std::vector<uint8_t> bytes; cmd(bytes, 0x51, 2); const auto packet = image(0xA7);
            bytes.insert(bytes.end(), packet.begin(), packet.end()); cmd(bytes, 0x07, 0x5678);
            compareFragments(t, bytes);
        });
        tc.Run("VIF masked PATH3 packets survive checkpoint and later unmask", [](TestCase &t)
        {
            auto reference = std::make_unique<MemoryFixture>(); auto restored = std::make_unique<MemoryFixture>();
            std::vector<uint8_t> mask; cmd(mask, 0x06, 0x8000);
            reference->memory.processVIF1Data(mask.data(), mask.size()); const auto packet = image(0x5A);
            reference->memory.submitGifPacket(GifPathId::Path3, packet.data(), packet.size());
            Vif1SnapshotCodec::restore(restored->memory, Vif1SnapshotCodec::encode(reference->memory));
            std::vector<uint8_t> unmask; cmd(unmask, 0x06);
            reference->memory.processVIF1Data(unmask.data(), unmask.size());
            restored->memory.processVIF1Data(unmask.data(), unmask.size());
            t.Equals(restored->packets, reference->packets, "Queued PATH3 bytes are released once in order");
            t.Equals(restored->packets.size(), size_t(1), "One packet was retained");
        });
        tc.Run("VIF corrupt checkpoint restore leaves all parser state unchanged", [](TestCase &t)
        {
            auto f = std::make_unique<MemoryFixture>(); std::vector<uint8_t> bytes; cmd(bytes, 0x51, 2);
            bytes.push_back(0xAA); f->memory.processVIF1Data(bytes.data(), bytes.size());
            const auto before = Vif1SnapshotCodec::encode(f->memory); auto bad = before; bad.back() ^= 1;
            bool rejected = false; try { Vif1SnapshotCodec::restore(f->memory, bad); }
            catch (const std::invalid_argument &) { rejected = true; }
            t.IsTrue(rejected, "Checksum failure is rejected");
            t.Equals(Vif1SnapshotCodec::encode(f->memory), before, "Rejected input cannot partially restore VIF");
        });
        tc.Run("GIF queued path priority and DIRECTHL IMAGE ordering survive restore", [](TestCase &t)
        {
            std::vector<std::vector<uint8_t>> first, second;
            GifArbiter reference([&](const uint8_t *p, uint32_t n) { first.emplace_back(p, p + n); });
            GifArbiter restored([&](const uint8_t *p, uint32_t n) { second.emplace_back(p, p + n); });
            const auto p1 = image(1), p2 = image(2), p3 = image(3);
            reference.submit(GifPathId::Path2, p2.data(), p2.size(), true);
            reference.submit(GifPathId::Path3, p3.data(), p3.size());
            reference.submit(GifPathId::Path1, p1.data(), p1.size());
            const auto checkpoint = GifSnapshotCodec::encode(reference); GifSnapshotCodec::restore(restored, checkpoint);
            t.Equals(GifSnapshotCodec::encode(restored), checkpoint, "All queued identities and priority metadata survive");
            reference.drain(); restored.drain(); t.Equals(second, first, "Drained packet order and bytes match");
            t.Equals(second.size(), size_t(3), "All queued packets reach the receiving callback");
        });
        tc.Run("GS in-flight image transfer and VRAM survive a checkpoint", [](TestCase &t)
        {
            auto first = std::make_unique<GsFixture>(); auto second = std::make_unique<GsFixture>();
            first->gs.writeRegister(GS_REG_BITBLTBUF, 1ull << 48);
            first->gs.writeRegister(GS_REG_TRXPOS, 0);
            first->gs.writeRegister(GS_REG_TRXREG, 4ull | (1ull << 32));
            first->gs.writeRegister(GS_REG_TRXDIR, 0);
            first->gs.writeRegister(GS_REG_HWREG, 0x0807060504030201ull);
            const auto checkpoint = GsSnapshotCodec::encode(first->gs); GsSnapshotCodec::restore(second->gs, checkpoint);
            t.Equals(GsSnapshotCodec::encode(second->gs), checkpoint, "GS registers, transfer progress and VRAM round trip");
            first->gs.writeRegister(GS_REG_HWREG, 0x1817161514131211ull);
            second->gs.writeRegister(GS_REG_HWREG, 0x1817161514131211ull);
            t.Equals(GsSnapshotCodec::encode(second->gs), GsSnapshotCodec::encode(first->gs), "Partial transfer resumes without rewinding");
            t.Equals(second->gs.ReadVram(GS_PSM_CT32, 0, 1, 3, 0), 0x18171615u, "The final pixel comes from the resumed bytes");
        });
        tc.Run("VIF semantically invalid checksummed state is transactional", [](TestCase &t)
        {
            auto f = std::make_unique<MemoryFixture>(); std::vector<uint8_t> bytes; cmd(bytes, 0x51, 2);
            bytes.push_back(0xAA); f->memory.processVIF1Data(bytes.data(), bytes.size());
            const auto before = Vif1SnapshotCodec::encode(f->memory); auto bad = before;
            binary::put32(bad, 24 + 23 * 4, UINT32_MAX); resign(bad);
            t.IsTrue(rejects([&] { Vif1SnapshotCodec::restore(f->memory, bad); }), "Oversized DIRECT residue is rejected after a valid CRC");
            t.Equals(Vif1SnapshotCodec::encode(f->memory), before, "Semantic rejection preserves registers and parser bytes");
        });
        tc.Run("GIF invalid metadata and counts preserve the queue and callback", [](TestCase &t)
        {
            unsigned delivered = 0; GifArbiter arbiter([&](const uint8_t *, uint32_t) { ++delivered; });
            const auto packet = image(1); arbiter.submit(GifPathId::Path1, packet.data(), packet.size());
            const auto before = GifSnapshotCodec::encode(arbiter); auto bad = before; bad[29] = 1; resign(bad);
            t.IsTrue(rejects([&] { GifSnapshotCodec::restore(arbiter, bad); }), "PATH1 cannot carry DIRECTHL metadata");
            t.Equals(GifSnapshotCodec::encode(arbiter), before, "Rejected metadata cannot replace the queue");
            bad = before; binary::put32(bad, 24, UINT32_MAX); resign(bad);
            t.IsTrue(rejects([&] { GifSnapshotCodec::restore(arbiter, bad); }), "Invalid count fails before queue allocation");
            t.Equals(GifSnapshotCodec::encode(arbiter), before, "Rejected count preserves packet identity");
            arbiter.drain(); t.Equals(delivered, 1u, "The original receiving callback is retained");
        });
        tc.Run("GS invalid cursor and boolean with valid CRC cannot partly publish", [](TestCase &t)
        {
            auto f = std::make_unique<GsFixture>(); f->gs.WriteVram(GS_PSM_CT32, 0, 1, 2, 3, 0x12345678);
            f->memory.gs().csr.store(0x5555); const auto before = GsSnapshotCodec::encode(f->gs);
            auto bad = before; std::fill(bad.end() - 8, bad.end(), 0xFF); resign(bad);
            t.IsTrue(rejects([&] { GsSnapshotCodec::restore(f->gs, bad); }), "A past-end local-to-host cursor fails semantic validation");
            t.Equals(GsSnapshotCodec::encode(f->gs), before, "No VRAM, private register or backend state changed");
            bad = before; bad[24] = 2; resign(bad);
            t.IsTrue(rejects([&] { GsSnapshotCodec::restore(f->gs, bad); }), "Boolean byte 2 is noncanonical");
            t.Equals(GsSnapshotCodec::encode(f->gs), before, "Boolean rejection is also transactional");
        });
        tc.Run("GS local-to-host staged bytes and subpixel read cursor survive restore", [](TestCase &t)
        {
            auto first = std::make_unique<GsFixture>(); auto second = std::make_unique<GsFixture>();
            for (uint32_t x = 0; x < 4; ++x) first->gs.WriteVram(GS_PSM_CT32, 0, 1, x, 0, 0xA3A2A1A0u + x * 0x04040404u);
            first->gs.writeRegister(GS_REG_BITBLTBUF, (1ull << 16) | (1ull << 48));
            first->gs.writeRegister(GS_REG_TRXREG, 4ull | (1ull << 32)); first->gs.writeRegister(GS_REG_TRXDIR, 1);
            std::array<uint8_t, 6> prefix{}; t.Equals(first->gs.consumeLocalToHostBytes(prefix.data(), 6), 6u, "Pause after six of sixteen staged bytes");
            const auto checkpoint = GsSnapshotCodec::encode(first->gs); GsSnapshotCodec::restore(second->gs, checkpoint);
            std::array<uint8_t, 16> a{}, b{};
            t.Equals(first->gs.consumeLocalToHostBytes(a.data(), 16), 10u, "Reference has ten unread bytes");
            t.Equals(second->gs.consumeLocalToHostBytes(b.data(), 16), 10u, "Restored read starts at the saved byte cursor");
            t.Equals(a, b, "Staged bytes do not get regenerated or rewound");
            t.Equals(uint32_t(b[0]), 0xA6u, "The next byte is the seventh source byte");
            t.Equals(GsSnapshotCodec::encode(second->gs), GsSnapshotCodec::encode(first->gs), "Post-read hidden state matches");
        });
        tc.Run("GS latched CLUT and half-built sprite survive source VRAM reuse", [](TestCase &t)
        {
            auto first = std::make_unique<GsFixture>(); auto second = std::make_unique<GsFixture>();
            constexpr uint64_t tex0 = 64ull | (1ull << 14) | (uint64_t(GS_PSM_T4) << 20) |
                (1ull << 34) | (1ull << 35) | (128ull << 37) | (2ull << 61);
            first->gs.WriteVram(GS_PSM_T4, 64, 1, 0, 0, 8);
            first->gs.WriteVram(GS_PSM_CT32, 128, 1, 0, 1, 0x800000FF);
            setupSprite(first->gs, tex0);
            first->gs.WriteVram(GS_PSM_CT32, 128, 1, 0, 1, 0x8000FF00);
            first->gs.writeRegister(GS_REG_UV, 0); first->gs.writeRegister(GS_REG_XYZ2, 0);
            GsSnapshotCodec::restore(second->gs, GsSnapshotCodec::encode(first->gs));
            for (GS *g : {&first->gs, &second->gs})
            {
                g->writeRegister(GS_REG_TEX0_1, (tex0 & ~(7ull << 61)) | (4ull << 61));
                g->writeRegister(GS_REG_UV, 0); g->writeRegister(GS_REG_XYZ2, 0);
            }
            t.Equals(second->gs.ReadVram(GS_PSM_CT32, 0, 1, 0, 0), 0x800000FFu, "Remembered CBP and loaded CLUT retain the original red texel");
            t.Equals(GsSnapshotCodec::encode(second->gs), GsSnapshotCodec::encode(first->gs), "Completing the retained vertex produces the same full model state");
        });
        tc.Run("GS stale texture page survives restore until explicit TEXFLUSH", [](TestCase &t)
        {
            auto first = std::make_unique<GsFixture>(); auto second = std::make_unique<GsFixture>();
            first->gs.WriteVram(GS_PSM_CT32, 32, 1, 0, 0, 0x80112233);
            setupSprite(first->gs, 32ull | (1ull << 14) | (1ull << 34) | (1ull << 35), 2ull << 16);
            drawPixel(first->gs, 0); first->gs.WriteVram(GS_PSM_CT32, 32, 1, 0, 0, 0x80445566);
            GsSnapshotCodec::restore(second->gs, GsSnapshotCodec::encode(first->gs));
            for (GS *g : {&first->gs, &second->gs})
            { drawPixel(*g, 1); g->writeRegister(GS_REG_TEXFLUSH, 0); drawPixel(*g, 2); }
            t.Equals(second->gs.ReadVram(GS_PSM_CT32, 0, 1, 1, 0), 0x80112233u, "The cached page still holds the old texel");
            t.Equals(second->gs.ReadVram(GS_PSM_CT32, 0, 1, 2, 0), 0x80445566u, "TEXFLUSH then exposes the new VRAM bytes");
            t.Equals(GsSnapshotCodec::encode(second->gs), GsSnapshotCodec::encode(first->gs), "Texture visibility and output agree after continuation");
        });
        tc.Run("GS float payloads private atomics and presentation buffers round trip", [](TestCase &t)
        {
            auto first = std::make_unique<GsFixture>(); auto second = std::make_unique<GsFixture>();
            auto &priv = first->memory.gs(); priv.csr.store(0x321); priv.vsyncTick.store(77);
            priv.pmode = 1; priv.dispfb1 = 1ull << 9; priv.display1 = (1ull << 32) | (1ull << 44);
            first->gs.WriteVram(GS_PSM_CT32, 0, 1, 0, 0, 0x80ABCDEF);
            first->gs.refreshDisplaySnapshot(); first->gs.latchHostPresentationFrame();
            first->gs.writeRegister(GS_REG_RGBAQ, (0x7FC12345ull << 32) | 0x80808080);
            first->gs.writeRegister(GS_REG_ST, (0xFFC12345ull << 32) | 0x80000000);
            const auto checkpoint = GsSnapshotCodec::encode(first->gs); GsSnapshotCodec::restore(second->gs, checkpoint);
            t.Equals(GsSnapshotCodec::encode(second->gs), checkpoint, "NaN payloads, signed zero and presentation state are canonical");
            t.Equals(second->memory.gs().csr.load(), uint64_t(0x321), "CSR preserves its sampled bits");
            t.Equals(second->memory.gs().vsyncTick.load(), uint64_t(77), "The absolute presentation tick is restored");
            std::vector<uint8_t> a, b; uint32_t aw=0, ah=0, bw=0, bh=0;
            t.IsTrue(first->gs.copyLatchedHostPresentationFrame(a, aw, ah), "The source actually has a latched frame");
            t.IsTrue(second->gs.copyLatchedHostPresentationFrame(b, bw, bh), "The restored target also has the frame");
            t.Equals(a, b, "Latched RGBA bytes match"); t.Equals(aw, bw, "Widths match"); t.Equals(ah, bh, "Heights match");
        });
        tc.Run("GS target binding mismatch and uninitialized source are rejected", [](TestCase &t)
        {
            auto f = std::make_unique<GsFixture>(); std::vector<uint8_t> vram(PS2_GS_VRAM_SIZE); GS noPriv, uninitialized;
            noPriv.init(vram.data(), vram.size(), nullptr); const auto before = GsSnapshotCodec::encode(noPriv);
            const auto bound = GsSnapshotCodec::encode(f->gs);
            t.IsTrue(rejects([&] { GsSnapshotCodec::restore(noPriv, bound); }), "Restore cannot silently change private register ownership");
            t.Equals(GsSnapshotCodec::encode(noPriv), before, "Target model and VRAM remain unchanged");
            t.IsTrue(rejects([&] { GsSnapshotCodec::encode(uninitialized); }), "No checkpoint success from an unbound backend");
        });
    });
    return MiniTest::Run();
}

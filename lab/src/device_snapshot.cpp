#include "nexo/device_snapshot.h"
#include "nexo/canonical_binary.h"
#include "nexo/vif_parser_checkpoint.h"
#include "runtime/ps2_memory.h"
#include "runtime/gs/ps2_gif_arbiter.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <stdexcept>

namespace ps2native::nexo
{
namespace
{
constexpr std::array<uint8_t, 8> vifMagic{'N','E','X','O','V','I','F',0};
constexpr std::array<uint8_t, 8> gifMagic{'N','E','X','O','G','A','R',0};
constexpr size_t transportBound = 64u * 1024u * 1024u;
constexpr size_t directBound = 65536u * 16u;
template <typename A, typename R> void vifRegisters(A &a, R &r)
{
    a(r.stat); a(r.fbrst); a(r.err); a(r.mark); a(r.cycle); a(r.mode); a(r.num); a(r.mask); a(r.code);
    a(r.itops); a(r.base); a(r.ofst); a(r.tops); a(r.itop); a(r.top); a(r.row); a(r.col);
}
void validateParser(const Vif1ParserCheckpoint &s)
{
    if (s.payload.size() > directBound || s.remainingBytes > directBound - s.payload.size() ||
        s.pendingCommand.size() > 4099u ||
        (s.remainingBytes != 0 && (!s.pendingCommand.empty() || ((s.remainingBytes + s.payload.size()) & 15u) != 0)) ||
        (s.remainingBytes == 0 && !s.payload.empty()))
        throw std::invalid_argument("invalid VIF parser checkpoint extent");
}
template <typename A, typename S> void parserFields(A &a, S &s)
{ a(s.remainingBytes); a(s.directHl); a(s.payload); a(s.pendingCommand); }
void validateGifPacket(const GifArbiterPacket &p)
{
    const auto id = static_cast<uint8_t>(p.pathId);
    if (id < 1 || id > 3 || p.data.size() < 16 || p.data.size() > transportBound ||
        (p.path2DirectHl && p.pathId != GifPathId::Path2))
        throw std::invalid_argument("invalid GIF packet checkpoint descriptor");
    uint64_t tag = 0;
    for (unsigned i = 0; i < 8; ++i) tag |= uint64_t(p.data[i]) << (8 * i);
    const bool image = p.pathId == GifPathId::Path3 && ((tag >> 58) & 3) == 2;
    if (p.path3Image != image) throw std::invalid_argument("inconsistent GIF IMAGE checkpoint descriptor");
}
}

std::vector<uint8_t> Vif1SnapshotCodec::encode(const PS2Memory &memory)
{
    const auto parser = snapshotVif1Parser(memory); validateParser(parser);
    if (memory.m_vif1PendingPath2ImageQwc > 32767u || memory.m_path3MaskedFifo.size() > transportBound / 16)
        throw std::invalid_argument("invalid VIF transport checkpoint extent");
    binary::Writer a(vifMagic, 1, transportBound);
    vifRegisters(a, memory.vif1_regs); parserFields(a, parser);
    a(memory.m_path3Masked); a(memory.m_vif1PendingPath2ImageQwc); a(memory.m_vif1PendingPath2DirectHl);
    a(uint32_t(memory.m_path3MaskedFifo.size()));
    for (const auto &packet : memory.m_path3MaskedFifo)
    {
        if (packet.size() < 16) throw std::invalid_argument("invalid masked PATH3 checkpoint packet");
        a(packet);
    }
    return a.finish();
}
void Vif1SnapshotCodec::restore(PS2Memory &memory, std::span<const uint8_t> bytes)
{
    binary::Reader a(bytes, vifMagic, 1, transportBound);
    VIFRegisters regs{}; Vif1ParserCheckpoint parser; bool masked, directHl; uint32_t imageQwc, count;
    vifRegisters(a, regs); parserFields(a, parser); a(masked); a(imageQwc); a(directHl); a(count);
    validateParser(parser);
    if (imageQwc > 32767u || count > a.remaining() / 20)
        throw std::invalid_argument("invalid VIF transport checkpoint extent");
    std::vector<std::vector<uint8_t>> fifo; fifo.reserve(count);
    for (uint32_t i = 0; i < count; ++i)
    {
        std::vector<uint8_t> packet; a(packet);
        if (packet.size() < 16) throw std::invalid_argument("invalid masked PATH3 checkpoint packet");
        fifo.push_back(std::move(packet));
    }
    a.finish();
    // The only publication that may allocate is done first, before modifying
    // registers or swapping transport storage. All execution must be paused.
    restoreVif1Parser(memory, std::move(parser));
    memory.vif1_regs = regs; memory.m_path3Masked = masked;
    memory.m_vif1PendingPath2ImageQwc = imageQwc; memory.m_vif1PendingPath2DirectHl = directHl;
    memory.m_path3MaskedFifo.swap(fifo);
}
std::vector<uint8_t> GifSnapshotCodec::encode(const GifArbiter &arbiter)
{
    if (arbiter.m_queue.size() > transportBound / 23)
        throw std::invalid_argument("GIF checkpoint packet count exceeds its byte budget");
    binary::Writer a(gifMagic, 0, transportBound); a(uint32_t(arbiter.m_queue.size()));
    for (const auto &p : arbiter.m_queue)
    { validateGifPacket(p); a(p.pathId); a(p.path2DirectHl); a(p.path3Image); a(p.data); }
    return a.finish();
}
void GifSnapshotCodec::restore(GifArbiter &arbiter, std::span<const uint8_t> bytes)
{
    binary::Reader a(bytes, gifMagic, 0, transportBound); uint32_t count; a(count);
    if (count > a.remaining() / 23) throw std::invalid_argument("invalid GIF checkpoint packet count");
    std::vector<GifArbiterPacket> queue; queue.reserve(count);
    for (uint32_t i = 0; i < count; ++i)
    {
        GifArbiterPacket p; a(p.pathId); a(p.path2DirectHl); a(p.path3Image); a(p.data);
        validateGifPacket(p); queue.push_back(std::move(p));
    }
    a.finish(); arbiter.m_queue.swap(queue);
}
}

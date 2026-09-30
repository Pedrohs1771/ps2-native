#pragma once
#include "nexo/canonical_binary.h"
#include <filesystem>

namespace ps2native::nexo::detail
{
inline constexpr size_t vifStateBound = 272u * 1024u * 1024u;
inline constexpr size_t vifInputBound = 32u * 1024u * 1024u;
inline constexpr size_t vifEventBound = 64u * 1024u * 1024u;
inline constexpr std::array<uint8_t,8> vifStateMagic{'N','E','X','O','V','C','S',0};
struct VifBoundary
{
    uint32_t fbrst=0, vpuStat=0;
    uint64_t codeGeneration=0;
    std::vector<uint8_t> vif, gif, gs, vu, code, data;
};
template <typename A> void boundaryFields(A &a, VifBoundary &s)
{ a(s.fbrst); a(s.vpuStat); a(s.codeGeneration); a(s.vif); a(s.gif); a(s.gs); a(s.vu); a(s.code); a(s.data); }
inline VifBoundary decodeVifBoundary(std::span<const uint8_t> bytes)
{
    binary::Reader a(bytes,vifStateMagic,1,vifStateBound); VifBoundary s; boundaryFields(a,s); a.finish();
    if (s.vif.size()<24 || s.vif.size()>64u*1024u*1024u || s.gif.size()<24 || s.gif.size()>64u*1024u*1024u ||
        s.gs.size()<24 || s.gs.size()>128u*1024u*1024u || s.vu.size()<24 || s.vu.size()>80u*1024u ||
        s.code.size()!=16384 || s.data.size()!=16384)
        throw std::invalid_argument("invalid composite VIF component extent");
    return s;
}
std::vector<uint8_t> readVifFile(const std::filesystem::path &path,size_t minimum,size_t maximum);
}

#include "nexo/vif_capture.h"
#include "nexo/device_snapshot.h"
#include "nexo/vu_snapshot.h"
#include "nexo/vu_native.h"
#include "nexo/vu_runtime_binding.h"
#include "vif_case_internal.h"
#include "ps2_runtime.h"
#include <cfenv>
#include <chrono>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>
namespace ps2native::nexo
{
namespace detail
{
std::vector<uint8_t> readVifFile(const std::filesystem::path &p,size_t minimum,size_t maximum)
{
    std::error_code ec; const auto size=std::filesystem::file_size(p,ec);
    if (ec || size<minimum || size>maximum) throw std::invalid_argument("missing or invalid VIF replay artifact");
    std::vector<uint8_t> b(size); std::ifstream f(p,std::ios::binary);
    f.read(reinterpret_cast<char *>(b.data()),std::streamsize(b.size()));
    if (!f || f.peek()!=std::char_traits<char>::eof()) throw std::invalid_argument("cannot read exact VIF replay artifact");
    return b;
}
}
namespace
{
VifReplayResult replay(const std::filesystem::path &directory,std::span<const VuNativeProgram> banks,bool native,bool profile)
{
    if (native && banks.empty()) throw std::invalid_argument("native VIF case requires compiled banks");
    if (detail::readVifFile(directory/".complete",1,1)!=std::vector<uint8_t>{1})
        throw std::invalid_argument("VIF case has no completion marker");
    const auto state=detail::readVifFile(directory/"input-state.nexo",24,detail::vifStateBound);
    const auto input=detail::readVifFile(directory/"vif-input.bin",1,detail::vifInputBound);
    const auto s=detail::decodeVifBoundary(state);
    auto r=std::make_unique<PS2Runtime>();
    if (!r->memory().initialize() || !r->syncCoreSubsystems()) throw std::runtime_error("cannot initialize headless VIF replay");
    if (native) bindNativeVu1(*r,banks);
    // A failed decode destroys this private machine; no caller-owned partial
    // restore is published. Device callbacks retain their initialized routing.
    auto &m=r->memory(); GsSnapshotCodec::restore(r->gs(),s.gs);
    GifSnapshotCodec::restore(r->gifArbiter(),s.gif); Vif1SnapshotCodec::restore(m,s.vif);
    VuSnapshotCodec::restore(r->vu1(),s.vu);
    if (r->vu1().state().cycles>std::numeric_limits<uint64_t>::max()-256ull*65536-64)
        throw std::invalid_argument("VIF replay clock cannot accommodate the callback budget");
    std::memcpy(m.getVU1Code(),s.code.data(),s.code.size()); std::memcpy(m.getVU1Data(),s.data.data(),s.data.size());
    m.m_vu1CodeGeneration.store(s.codeGeneration,std::memory_order_relaxed);
    r->cpu().vu0_fbrst=s.fbrst; r->cpu().vu0_vpu_stat=s.vpuStat;
    VifObservation observation(*r,true,profile);
    struct RoundingScope
    {
        int previous=std::fegetround();
        ~RoundingScope() { if (previous!=-1) std::fesetround(previous); }
    } rounding;
    const auto begin=std::chrono::steady_clock::now(); m.processVIF1Data(input.data(),input.size());
    const auto elapsed=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-begin).count();
    if (!observation.vuCalls()) throw std::invalid_argument("VIF case did not execute a VU callback");
    return {encodeVifBoundary(*r),observation.events(),observation.codeBanks(),uint64_t(elapsed),observation.timing()};
}
}
VifReplayResult replayVifCase(const std::filesystem::path &directory,bool profile) { return replay(directory,{},false,profile); }
VifReplayResult replayVifCaseNative(const std::filesystem::path &directory,std::span<const VuNativeProgram> banks,bool profile)
{ return replay(directory,banks,true,profile); }
}

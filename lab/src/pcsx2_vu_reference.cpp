#include "nexo/pcsx2_vu_reference.h"
#include "nexo/canonical_binary.h"
#include "VUmicro.h"
#include "Gif_Unit.h"
#include <limits>
#include <atomic>
#include <thread>
#include <type_traits>

VURegs vuRegs[2];
ReferenceCpuRegs cpuRegs;
VIFregisters vif0Regs,vif1Regs;
ReferenceGifUnit gifUnit;
namespace
{
std::atomic_flag upstreamOwned=ATOMIC_FLAG_INIT;
struct UpstreamOwnership
{
    UpstreamOwnership()
    {
        if (upstreamOwned.test_and_set(std::memory_order_acquire))
            throw std::runtime_error("PCSX2 reference engine already has an owner");
    }
    ~UpstreamOwnership() { upstreamOwned.clear(std::memory_order_release); }
};
std::function<void(std::span<const uint8_t>,bool,uint64_t)> receiveChunk;
std::function<void()> receiveIssue;
std::vector<uint8_t> identifiedResetSeed()
{
    // Fixed schema-v1 domain, independent of the current model constructor.
    // A future change to its reset state or hidden-state encoding must require
    // a new explicit mapping instead of silently changing this import domain.
    std::vector<uint8_t> seed(70249,0);
    const std::array<uint8_t,8> magic{'N','E','X','O','V','U',0,0};
    std::copy(magic.begin(),magic.end(),seed.begin());
    using ps2native::nexo::binary::put32;
    put32(seed,8,1); put32(seed,12,1); put32(seed,16,uint32_t(seed.size()-24));
    put32(seed,36,0x3f800000); // VF0.w
    put32(seed,616,0x3f800000); // Q
    put32(seed,628,0x3f800000); // R bits
    put32(seed,20,ps2native::nexo::binary::checksum(seed));
    return seed;
}
}
void referenceVuDiagnostic(const char* function)
{
    if (receiveIssue && std::strcmp(function,"_vu1ExecUpper")==0) receiveIssue();
}
u32 ReferenceGifUnit::TransferGSPacketData(GIF_TRANSFER_TYPE type,u8* memory,u32 size,bool aligned)
{
    if (type!=GIF_TRANS_XGKICK || !aligned || !receiveChunk || !size || (size&15u) ||
        size>VU1.xgkicksizeremaining || size>0x4000u-VU1.xgkickaddr ||
        memory!=VU1.Mem+VU1.xgkickaddr)
        throw std::runtime_error("unsupported reference GIF transfer boundary");
    receiveChunk({memory,size},size==VU1.xgkicksizeremaining && VU1.xgkickendpacket,VU1.cycle);
    return size;
}
namespace ps2native::nexo
{
struct Pcsx2Vu1Reference::Impl
{
    UpstreamOwnership ownership;
    std::thread::id thread=std::this_thread::get_id();
    PacketReceiver receiver;
    std::vector<uint8_t> pending;
    std::vector<VuReferenceChunk> trace;
    std::vector<VuReferenceIssue> issueTrace;
    bool issueTraceEnabled=false,started=false;
    size_t observedBytes=28; // Canonical chunk envelope + count.
    bool failed=false;
    Impl(std::span<const uint8_t> seed,std::span<uint8_t> micro,std::span<uint8_t> data,PacketReceiver target):receiver(std::move(target))
    {
        const uintptr_t c=reinterpret_cast<uintptr_t>(micro.data()),d=reinterpret_cast<uintptr_t>(data.data());
        if (micro.size()!=16384 || data.size()!=16384 || !c || !d || ((c|d)&15u) ||
            c>UINTPTR_MAX-16384 || d>UINTPTR_MAX-16384 || (c<d+16384 && d<c+16384) || !receiver)
            throw std::invalid_argument("reference requires aligned disjoint VU1 memories and a packet receiver");
        const auto reset=identifiedResetSeed();
        if (seed.size()!=reset.size() || !std::equal(seed.begin(),seed.end(),reset.begin()))
            throw std::invalid_argument("UNMAPPED_REFERENCE_STATE: only the exact canonical reset seed is qualified for import");
        // VURegs has a user-provided constructor; its scalar members need an
        // explicit zero reset. Its constructor only initializes two pointers.
        static_assert(std::is_trivially_copyable_v<VURegs>);
        std::memset(vuRegs,0,sizeof(vuRegs));
        cpuRegs={}; vif0Regs={}; vif1Regs={};
        VU1.idx=1; VU1.Micro=micro.data(); VU1.Mem=data.data();
        VU1.VF[0].i.w=0x3f800000; VU1.q.UL=0x3f800000; VU1.VI[REG_Q].UL=0x3f800000;
        VU1.VI[REG_R].UL=0x3f800000;
        receiveChunk=[this](auto bytes,bool end,uint64_t cycle)
        {
            if (trace.size()>=262144 || bytes.size()>65536-pending.size() ||
                bytes.size()+13>64u*1024u*1024u-observedBytes)
                throw std::runtime_error("reference XGKICK observation budget exceeded");
            observedBytes+=bytes.size()+13;
            trace.push_back({cycle,end,{bytes.begin(),bytes.end()}});
            pending.insert(pending.end(),bytes.begin(),bytes.end());
            if (end) { receiver(pending); pending.clear(); }
        };
    }
    ~Impl() { receiveChunk={}; receiveIssue={}; VU1.Mem=nullptr; VU1.Micro=nullptr; }
    void check() const
    {
        if (thread!=std::this_thread::get_id()) throw std::runtime_error("reference owner cannot move across threads");
        if (failed) throw std::runtime_error("reference execution failed; discard this machine");
    }
};
Pcsx2Vu1Reference::Pcsx2Vu1Reference(std::span<const uint8_t> seed,std::span<uint8_t> micro,
    std::span<uint8_t> data,PacketReceiver receiver):impl(std::make_unique<Impl>(seed,micro,data,std::move(receiver))) {}
Pcsx2Vu1Reference::~Pcsx2Vu1Reference()=default;
void Pcsx2Vu1Reference::execute(bool fresh,uint32_t pc,uint32_t top,uint32_t itop,uint32_t budget)
{
    impl->check();
    if ((pc&7u) || pc>=16384 || top>1023 || itop>1023 || !budget || budget>65536)
        throw std::invalid_argument("reference callback arguments exceed the identified domain");
    if (VU1.cycle>std::numeric_limits<uint64_t>::max()-budget-65536)
        throw std::invalid_argument("reference cycle horizon overflows");
    try
    {
        if (fresh)
        {
            if (VU0.VI[REG_VPU_STAT].UL&0x100) throw std::runtime_error("reference excludes overlapping fresh microcalls");
            VU1.VI[REG_TPC].UL=pc>>3; CpuIntVU1.SetStartPC(pc);
        }
        vif1Regs.top=top; vif1Regs.itop=itop; vif1Regs.stat.VEW=true;
        VU0.VI[REG_VPU_STAT].UL|=0x100;
        // Isolated monotonic VU clock; no EE scheduler is being simulated.
        cpuRegs.cycle=VU1.cycle;
        impl->started=true;
        CpuIntVU1.Execute(budget);
        if (VU1.cycle-cpuRegs.cycle>budget || (VU0.VI[REG_VPU_STAT].UL&0x100) ||
            VU1.xgkickenable || !impl->pending.empty())
            throw std::runtime_error("reference microcall did not close within its callback horizon");
    }
    catch (...) { impl->failed=true; throw; }
}
VuReferenceProjection Pcsx2Vu1Reference::projection() const
{
    impl->check(); VuReferenceProjection p;
    for (size_t r=0;r<32;++r) for (size_t lane=0;lane<4;++lane) p.vf[r*4+lane]=VU1.VF[r].UL[lane];
    for (size_t r=0;r<16;++r) p.vi[r]=VU1.VI[r].US[0];
    std::copy(std::begin(VU1.ACC.UL),std::end(VU1.ACC.UL),p.acc.begin());
    p.q=VU1.VI[REG_Q].UL; p.p=VU1.VI[REG_P].UL; p.i=VU1.VI[REG_I].UL; p.r=VU1.VI[REG_R].UL;
    p.mac=VU1.VI[REG_MAC_FLAG].UL; p.status=VU1.VI[REG_STATUS_FLAG].UL; p.clip=VU1.VI[REG_CLIP_FLAG].UL;
    p.pc=VU1.VI[REG_TPC].UL<<3; p.cycles=VU1.cycle; return p;
}
const std::vector<VuReferenceChunk>& Pcsx2Vu1Reference::chunks() const { impl->check(); return impl->trace; }
uint64_t Pcsx2Vu1Reference::cycles() const { impl->check(); return VU1.cycle; }
void Pcsx2Vu1Reference::enableIssueTrace()
{
    impl->check();
    if (impl->started || impl->issueTraceEnabled) throw std::invalid_argument("enable reference issue tracing once before execution");
    impl->issueTraceEnabled=true;
    receiveIssue=[this]
    {
        if (impl->issueTrace.size()>=262144) throw std::runtime_error("reference issue trace exceeded its bound");
        const uint32_t pc=(VU1.VI[REG_TPC].UL-8u)&0x3fffu;
        uint32_t lower=0,upper=0;
        std::memcpy(&lower,VU1.Micro+pc,4); std::memcpy(&upper,VU1.Micro+pc+4,4);
        impl->issueTrace.push_back({VU1.cycle,pc,lower,upper});
    };
}
const std::vector<VuReferenceIssue>& Pcsx2Vu1Reference::issues() const { impl->check(); return impl->issueTrace; }
}

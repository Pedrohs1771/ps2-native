#include "nexo/vif_capture.h"
#include "nexo/device_snapshot.h"
#include "nexo/vu_snapshot.h"
#include "vif_case_internal.h"
#include "ps2_runtime.h"
#include "runtime/ee_scheduler.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <mutex>
#include <unordered_map>

namespace ps2native::nexo
{
namespace
{
struct Binding
{
    PS2Runtime *runtime;
    std::recursive_mutex presentation;
    explicit Binding(PS2Runtime &r):runtime(&r) {}
};
std::mutex bindingsMutex;
std::unordered_map<PS2Memory *,std::shared_ptr<Binding>> bindings;
std::shared_ptr<Binding> binding(PS2Memory &memory)
{
    std::lock_guard lock(bindingsMutex); auto it=bindings.find(&memory);
    return it==bindings.end()?nullptr:it->second;
}
R5900Context &context(PS2Runtime &runtime)
{
    auto *current=runtime.eeScheduler().currentContext(); return current?*current:runtime.cpu();
}
void writeFile(const std::filesystem::path &path,std::span<const uint8_t> bytes)
{
    std::ofstream out(path,std::ios::binary|std::ios::trunc);
    out.write(reinterpret_cast<const char *>(bytes.data()),std::streamsize(bytes.size())); out.close();
    if (!out) throw std::runtime_error("cannot write VIF capture artifact");
}
struct Event
{
    uint8_t kind=0;
    uint64_t cycle=0;
    std::array<uint32_t,5> args{};
    std::vector<uint8_t> data;
};
}
void bindVifCapture(PS2Runtime &runtime)
{
    std::lock_guard lock(bindingsMutex); auto &b=bindings[&runtime.memory()];
    if (!b || b->runtime!=&runtime) b=std::make_shared<Binding>(runtime);
}
void unbindVifCapture(PS2Memory &memory) noexcept
{
    std::lock_guard lock(bindingsMutex); bindings.erase(&memory);
}
struct VifPresentationScope::Impl
{
    std::shared_ptr<Binding> owner;
    std::unique_lock<std::recursive_mutex> lock;
    explicit Impl(std::shared_ptr<Binding> b):owner(std::move(b)),lock(owner->presentation) {}
};
VifPresentationScope::VifPresentationScope(PS2Runtime &r)
{ if (auto b=binding(r.memory())) impl=std::make_unique<Impl>(std::move(b)); }
VifPresentationScope::~VifPresentationScope()=default;

struct VifObservation::Impl
{
    PS2Runtime &runtime;
    Impl *previous=nullptr;
    bool strict,failed=false,captureOwned=false;
    uint32_t calls=0;
    size_t eventBytes=28;
    std::vector<Event> trace;
    std::vector<std::vector<uint8_t>> banks;
    static thread_local Impl *active;
    Impl(PS2Runtime &r,bool s):runtime(r),previous(active),strict(s) { active=this; }
    ~Impl() { active=previous; }
    template <typename F> void record(F &&f)
    {
        if (failed) { if (strict) throw std::invalid_argument("incomplete VIF observation"); return; }
        try { f(); }
        catch (...) { failed=true; if (strict) throw; }
    }
    void append(Event event)
    {
        constexpr size_t overhead=33;
        if (event.data.size()>detail::vifEventBound-overhead ||
            eventBytes>detail::vifEventBound-overhead-event.data.size() || trace.size()>=262144)
            throw std::invalid_argument("VIF observation exceeds its event budget");
        eventBytes+=overhead+event.data.size(); trace.push_back(std::move(event));
    }
};
thread_local VifObservation::Impl *VifObservation::Impl::active=nullptr;
VifObservation::VifObservation(PS2Runtime &r,bool strict):impl(std::make_unique<Impl>(r,strict)) {}
VifObservation::~VifObservation()=default;
uint32_t VifObservation::vuCalls() const { return impl->calls; }
bool VifObservation::failed() const { return impl->failed; }
const std::vector<std::vector<uint8_t>> &VifObservation::codeBanks() const { return impl->banks; }
std::vector<uint8_t> VifObservation::events() const
{
    if (impl->failed) throw std::invalid_argument("incomplete VIF observation");
    constexpr std::array<uint8_t,8> magic{'N','E','X','O','V','T','R',0};
    binary::Writer a(magic,1,detail::vifEventBound); a(uint32_t(impl->trace.size()));
    for (const auto &e:impl->trace) { a(e.kind); a(e.cycle); a(e.args); a(e.data); }
    return a.finish();
}
void observeVifVuCall(PS2Memory &memory,uint8_t opcode,uint32_t pc,uint32_t top,uint32_t itop)
{
    auto *s=VifObservation::Impl::active; if (!s || &s->runtime.memory()!=&memory) return;
    s->record([&]
    {
        if (s->calls>=256 || !memory.getVU1Code()) throw std::invalid_argument("VIF VU callback budget exceeded");
        const auto *code=memory.getVU1Code(); size_t bank=0;
        for (;bank<s->banks.size();++bank) if (std::equal(s->banks[bank].begin(),s->banks[bank].end(),code)) break;
        if (bank==s->banks.size()) s->banks.emplace_back(code,code+PS2_VU1_CODE_SIZE);
        s->append({1,s->runtime.vu1().state().cycles,{opcode,pc,top,itop,uint32_t(bank)},{}}); ++s->calls;
    });
}
void observeVifGifSubmission(PS2Memory &memory,GifPathId path,const uint8_t *data,uint32_t bytes,bool drain,bool hl)
{
    auto *s=VifObservation::Impl::active; if (!s || &s->runtime.memory()!=&memory) return;
    s->record([&]
    {
        if (!data || bytes<16 || bytes>detail::vifEventBound-33) throw std::invalid_argument("invalid VIF GIF submission extent");
        s->append({2,s->runtime.vu1().state().cycles,{uint32_t(path),uint32_t(drain),uint32_t(hl),0,0},{data,data+bytes}});
    });
}
void observeVifGifDelivery(PS2Memory &memory,const uint8_t *data,uint32_t bytes)
{
    auto *s=VifObservation::Impl::active; if (!s || &s->runtime.memory()!=&memory) return;
    s->record([&]
    {
        if (!data || bytes<16 || bytes>detail::vifEventBound-33) throw std::invalid_argument("invalid VIF GIF delivery extent");
        s->append({3,s->runtime.vu1().state().cycles,{},{data,data+bytes}});
    });
}
std::vector<uint8_t> encodeVifBoundary(PS2Runtime &r)
{
    auto &m=r.memory();
    if (!m.getVU1Code() || !m.getVU1Data() || m.m_gifArbiter!=&r.gifArbiter())
        throw std::invalid_argument("VIF boundary requires initialized runtime-owned routing");
    detail::VifBoundary s; auto &cpu=context(r);
    s.fbrst=cpu.vu0_fbrst; s.vpuStat=cpu.vu0_vpu_stat; s.codeGeneration=m.getVU1CodeGeneration();
    s.vif=Vif1SnapshotCodec::encode(m); s.gif=GifSnapshotCodec::encode(r.gifArbiter());
    s.gs=GsSnapshotCodec::encode(r.gs()); s.vu=VuSnapshotCodec::encode(r.vu1());
    s.code.assign(m.getVU1Code(),m.getVU1Code()+PS2_VU1_CODE_SIZE);
    s.data.assign(m.getVU1Data(),m.getVU1Data()+PS2_VU1_DATA_SIZE);
    binary::Writer a(detail::vifStateMagic,1,detail::vifStateBound); detail::boundaryFields(a,s); return a.finish();
}
struct VifCaptureScope::Impl
{
    std::shared_ptr<Binding> owner;
    std::unique_lock<std::recursive_mutex> lease;
    std::filesystem::path base;
    std::vector<uint8_t> input,state;
    std::unique_ptr<VifObservation> observation;
    int exceptions=std::uncaught_exceptions();
    std::chrono::steady_clock::time_point started;
    Impl(std::shared_ptr<Binding> b,std::filesystem::path p,std::span<const uint8_t> bytes):
        owner(std::move(b)),lease(owner->presentation),base(std::move(p)),input(bytes.begin(),bytes.end())
    {
        state=encodeVifBoundary(*owner->runtime);
        observation=std::make_unique<VifObservation>(*owner->runtime,false);
        observation->impl->captureOwned=true; started=std::chrono::steady_clock::now();
    }
};
VifCaptureScope::VifCaptureScope(PS2Memory &m,std::span<const uint8_t> input) noexcept
{
    try
    {
        if (auto *active=VifObservation::Impl::active)
        { if (active->captureOwned && &active->runtime.memory()==&m) active->failed=true; return; }
        const char *base=std::getenv("PS2X_CAPTURE_SCENE"); if (!base || !*base) return;
        std::error_code ec; if (!std::filesystem::is_regular_file(std::filesystem::path(base)/".vif-request",ec)) return;
        if (input.empty() || input.size()>detail::vifInputBound) throw std::invalid_argument("VIF argument outside capture budget");
        const uintptr_t first=reinterpret_cast<uintptr_t>(input.data());
        if (first>UINTPTR_MAX-input.size()) throw std::invalid_argument("VIF argument address overflow");
        const auto overlaps=[&](const uint8_t *p,size_t size)
        { const uintptr_t lo=reinterpret_cast<uintptr_t>(p); return p && first<lo+size && lo<first+input.size(); };
        if (overlaps(m.getVU1Code(),PS2_VU1_CODE_SIZE) || overlaps(m.getVU1Data(),PS2_VU1_DATA_SIZE) ||
            overlaps(m.getGSVRAM(),PS2_GS_VRAM_SIZE)) throw std::invalid_argument("VIF input aliases its mutable outputs");
        if (auto b=binding(m)) impl=std::make_unique<Impl>(std::move(b),base,input);
    }
    catch (const std::exception &e) { std::fprintf(stderr,"[vif-capture:error] %s\n",e.what()); }
}
VifCaptureScope::~VifCaptureScope()
{
    if (!impl || !impl->observation->vuCalls()) return;
    try
    {
        if (std::uncaught_exceptions()!=impl->exceptions || impl->observation->failed())
            throw std::runtime_error("VIF call did not yield a complete observation");
        const auto elapsed=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-impl->started).count();
        const auto output=encodeVifBoundary(*impl->owner->runtime); const auto events=impl->observation->events();
        const auto directory=impl->base/("vif-input-"+std::to_string(std::chrono::system_clock::now().time_since_epoch().count()));
        if (!std::filesystem::create_directory(directory)) throw std::runtime_error("VIF capture directory collision");
        writeFile(directory/"vif-input.bin",impl->input); writeFile(directory/"input-state.nexo",impl->state);
        writeFile(directory/"output-state.nexo",output); writeFile(directory/"events.nexo",events);
        const auto &banks=impl->observation->codeBanks();
        for (size_t i=0;i<banks.size();++i) writeFile(directory/("bank-"+std::to_string(i)+".bin"),banks[i]);
        std::ofstream meta(directory/"capture.json");
        meta << "{\"schema\":\"nexo.observed.vif.call.v1\",\"assurance\":\"unknown\","
             << "\"input\":\"observed_function_argument\",\"execution_provider\":\"runtime_callbacks\","
             << "\"callback_horizon\":65536,\"vu_calls\":" << impl->observation->vuCalls()
             << ",\"executed_banks\":" << banks.size() << ",\"execution_ns\":" << elapsed
             << ",\"host_presentation_serialized\":true,\"independent_reference\":false}\n";
        meta.close(); if (!meta) throw std::runtime_error("cannot write VIF metadata");
        const std::array<uint8_t,1> completed{1}; writeFile(directory/".complete",completed);
        std::error_code ec; std::filesystem::remove(impl->base/".vif-request",ec);
        std::fprintf(stderr,"[vif-capture] directory=%s calls=%u banks=%zu\n",directory.c_str(),impl->observation->vuCalls(),banks.size());
    }
    catch (const std::exception &e) { std::fprintf(stderr,"[vif-capture:error] %s\n",e.what()); }
}
}

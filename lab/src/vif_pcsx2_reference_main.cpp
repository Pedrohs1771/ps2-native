#include "nexo/pcsx2_vu_reference.h"
#include "nexo/device_snapshot.h"
#include "nexo/vif_capture.h"
#include "nexo/vu_snapshot.h"
#include "vif_case_internal.h"
#include "ps2_runtime.h"
#include <bit>
#include <chrono>
#include <charconv>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>

using namespace ps2native::nexo;
namespace
{
struct Diagnostics
{
    std::streambuf* previous=std::cout.rdbuf(std::cerr.rdbuf());
    ~Diagnostics() { std::cout.rdbuf(previous); }
};
void write(const std::filesystem::path& path,std::span<const uint8_t> bytes)
{
    std::ofstream f(path,std::ios::binary|std::ios::trunc);
    f.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size())); f.close();
    if (!f) throw std::runtime_error("cannot publish reference replay artifact");
}
void write(const std::filesystem::path& path,const std::string& text)
{ write(path,{reinterpret_cast<const uint8_t*>(text.data()),text.size()}); }
struct Event
{
    uint8_t kind=0; uint64_t cycle=0; std::array<uint32_t,5> args{}; std::vector<uint8_t> bytes;
};
std::vector<Event> decodeEvents(std::span<const uint8_t> bytes)
{
    binary::Reader a(bytes,{'N','E','X','O','V','T','R',0},1,detail::vifEventBound);
    uint32_t count=0; a(count); if (count>262144 || count>a.remaining()/33)
        throw std::invalid_argument("invalid reference comparison event count");
    std::vector<Event> events(count);
    uint64_t previousCycle=0;
    uint32_t callbacks=0;
    for (auto& e:events)
    {
        a(e.kind); a(e.cycle); a(e.args); a(e.bytes);
        if (e.kind<1 || e.kind>3) throw std::invalid_argument("unknown recorded VIF event kind");
        if (e.cycle<previousCycle) throw std::invalid_argument("recorded VIF event clock moved backwards");
        previousCycle=e.cycle;
        if (e.kind==1)
        {
            if (++callbacks>256 || !e.bytes.empty() ||
                (e.args[0]!=0x14 && e.args[0]!=0x15 && e.args[0]!=0x17) ||
                (e.args[1]&7u) || e.args[1]>=16384 || e.args[2]>1023 || e.args[3]>1023 || e.args[4]>=256)
                throw std::invalid_argument("invalid recorded VU callback event");
        }
        else if (e.bytes.size()<16 || (e.bytes.size()&15u))
            throw std::invalid_argument("invalid recorded GIF event extent");
        else if (e.kind==2 && (e.args[0]<1 || e.args[0]>3 || e.args[1]>1 || e.args[2]>1 || e.args[3] || e.args[4]))
            throw std::invalid_argument("invalid recorded GIF submission arguments");
        else if (e.kind==3 && e.args!=std::array<uint32_t,5>{})
            throw std::invalid_argument("invalid recorded GIF delivery arguments");
    }
    a.finish(); return events;
}
std::vector<std::vector<uint8_t>> recordedBanks(const std::filesystem::path& directory)
{
    std::vector<std::vector<uint8_t>> banks;
    for (size_t i=0;i<256;++i)
    {
        const auto p=directory/("bank-"+std::to_string(i)+".bin");
        if (!std::filesystem::exists(p)) break;
        banks.push_back(detail::readVifFile(p,16384,16384));
    }
    if (banks.empty()) throw std::invalid_argument("reference case has no executed code bank");
    for (const auto& entry:std::filesystem::directory_iterator(directory))
    {
        const auto name=entry.path().filename().string();
        if (!name.starts_with("bank-") || !name.ends_with(".bin")) continue;
        const auto digits=std::string_view(name).substr(5,name.size()-9);
        size_t index=0; const auto parse=std::from_chars(digits.data(),digits.data()+digits.size(),index);
        if (parse.ec!=std::errc{} || parse.ptr!=digits.data()+digits.size() || index>=banks.size() ||
            name!="bank-"+std::to_string(index)+".bin")
            throw std::invalid_argument("reference case bank inventory is noncanonical or noncontiguous");
    }
    return banks;
}
std::vector<uint8_t> encodeProjection(const VuReferenceProjection& p)
{
    binary::Writer a({'N','E','X','O','V','P','R',0},1,2048);
    a(p.vf); a(p.vi); a(p.acc); a(p.q); a(p.p); a(p.i); a(p.r);
    a(p.mac); a(p.status); a(p.clip); a(p.pc); a(p.cycles); return a.finish();
}
VuReferenceProjection modelProjection(const VU1State& s)
{
    VuReferenceProjection p;
    for (size_t r=0;r<32;++r) for (size_t lane=0;lane<4;++lane) p.vf[r*4+lane]=std::bit_cast<uint32_t>(s.vf[r][lane]);
    for (size_t r=0;r<16;++r) p.vi[r]=uint16_t(s.vi[r]);
    for (size_t lane=0;lane<4;++lane) p.acc[lane]=std::bit_cast<uint32_t>(s.acc[lane]);
    p.q=std::bit_cast<uint32_t>(s.q); p.p=std::bit_cast<uint32_t>(s.p); p.i=std::bit_cast<uint32_t>(s.i); p.r=s.r;
    p.mac=s.mac; p.status=s.status; p.clip=s.clip; p.pc=s.pc; p.cycles=s.cycles; return p;
}
struct Difference { std::string field; uint64_t actual,expected; };
std::vector<Difference> differences(const VuReferenceProjection& a,const VuReferenceProjection& b)
{
    std::vector<Difference> result;
    const auto field=[&](std::string name,uint64_t x,uint64_t y)
    { if (x!=y) result.push_back({std::move(name),x,y}); };
    for (size_t i=0;i<a.vf.size();++i) field("vf["+std::to_string(i/4)+"]["+std::to_string(i%4)+"]",a.vf[i],b.vf[i]);
    for (size_t i=0;i<a.vi.size();++i) field("vi["+std::to_string(i)+"]",a.vi[i],b.vi[i]);
    for (size_t i=0;i<4;++i) field("acc["+std::to_string(i)+"]",a.acc[i],b.acc[i]);
    field("q",a.q,b.q); field("p",a.p,b.p); field("i",a.i,b.i); field("r",a.r,b.r);
    field("mac",a.mac,b.mac); field("status",a.status,b.status); field("clip",a.clip,b.clip);
    field("pc",a.pc,b.pc); field("cycles",a.cycles,b.cycles); return result;
}
void comparison(std::ostream& out,const char* name,const std::vector<uint8_t>& actual,const std::vector<uint8_t>& expected)
{
    size_t first=0,common=std::min(actual.size(),expected.size()),count=0;
    for (size_t i=0;i<common;++i) if (actual[i]!=expected[i]) ++count;
    count+=std::max(actual.size(),expected.size())-common;
    while (first<common && actual[first]==expected[first]) ++first;
    const bool equal=actual==expected;
    if (!equal && first>=20 && first<24 && actual.size()==expected.size())
    { first=24; while (first<common && actual[first]==expected[first]) ++first; }
    out << '"' << name << "\":{\"equal\":" << (equal?"true":"false")
        << ",\"differing_bytes\":" << count << ",\"first_semantic_byte\":";
    if (equal) out << "null"; else out << first;
    out << '}';
}
}
int main(int argc,char** argv)
{
    try
    {
        if (argc!=3) throw std::invalid_argument("usage: nexo_vif_pcsx2_reference <observed-case> <new-output-directory>");
        unsetenv("PS2X_CAPTURE_SCENE"); setenv("PS2X_FUNCTION_TRACE","0",1);
        const std::filesystem::path directory(argv[1]),output(argv[2]);
        if (std::filesystem::exists(output)) throw std::invalid_argument("reference output must be a new directory");
        if (detail::readVifFile(directory/".complete",1,1)!=std::vector<uint8_t>{1})
            throw std::invalid_argument("reference input case is incomplete");
        const auto inputState=detail::readVifFile(directory/"input-state.nexo",24,detail::vifStateBound);
        const auto input=detail::readVifFile(directory/"vif-input.bin",1,detail::vifInputBound);
        const auto seed=detail::decodeVifBoundary(inputState);
        const auto expected=detail::decodeVifBoundary(detail::readVifFile(directory/"output-state.nexo",24,detail::vifStateBound));
        const auto recordedEvents=detail::readVifFile(directory/"events.nexo",24,detail::vifEventBound);
        const auto recorded=decodeEvents(recordedEvents);
        const auto expectedBanks=recordedBanks(directory);
        for (const auto& e:recorded)
            if (e.kind==1 && e.args[4]>=expectedBanks.size())
                throw std::invalid_argument("recorded callback refers to an absent code bank");
        if (seed.fbrst || seed.vpuStat) throw std::invalid_argument("reference initial domain excludes VU0 activity and interrupt enables");
        std::vector<uint8_t> vif,gif,gs,code,data,events,projection,chunks;
        std::vector<VuReferenceProjection> callbackOutputs;
        std::vector<std::vector<uint8_t>> banks;
        uint64_t ns=0; VuReferenceProjection final;
        uint32_t calls=0; uint64_t generation=0;
        {
            Diagnostics diagnostic;
            auto runtime=std::make_unique<PS2Runtime>();
            if (!runtime->memory().initialize() || !runtime->syncCoreSubsystems())
                throw std::runtime_error("cannot initialize private headless reference replay");
            auto& m=runtime->memory();
            GsSnapshotCodec::restore(runtime->gs(),seed.gs); GifSnapshotCodec::restore(runtime->gifArbiter(),seed.gif);
            Vif1SnapshotCodec::restore(m,seed.vif);
            if (!runtime->gifArbiter().empty()) throw std::invalid_argument("reference domain requires initially idle GIF queues");
            std::memcpy(m.getVU1Code(),seed.code.data(),seed.code.size());
            std::memcpy(m.getVU1Data(),seed.data.data(),seed.data.size());
            m.m_vu1CodeGeneration.store(seed.codeGeneration,std::memory_order_relaxed);
            Pcsx2Vu1Reference reference(seed.vu,{m.getVU1Code(),16384},{m.getVU1Data(),16384},[&](auto packet)
            {
                if (!runtime->gifArbiter().empty()) throw std::runtime_error("reference GIF queue became blocked");
                m.submitGifPacket(GifPathId::Path1,packet.data(),uint32_t(packet.size()));
                if (!runtime->gifArbiter().empty()) throw std::runtime_error("reference domain excludes suspended GIF delivery");
            });
            const auto execute=[&](bool fresh,uint32_t pc,uint32_t top,uint32_t itop)
            {
                if (callbackOutputs.size()>=256) throw std::runtime_error("reference callback budget exceeded");
                reference.execute(fresh,pc,top,itop); callbackOutputs.push_back(reference.projection());
            };
            m.setVu1MscalCallback([&](uint32_t pc,uint32_t top,uint32_t itop) { execute(true,pc,top,itop); });
            m.setVu1MscntCallback([&](uint32_t top,uint32_t itop) { execute(false,0,top,itop); });
            VifObservation observer(*runtime,true,false,[&] { return reference.cycles(); });
            const auto begin=std::chrono::steady_clock::now(); m.processVIF1Data(input.data(),uint32_t(input.size()));
            ns=uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-begin).count());
            calls=observer.vuCalls(); if (!calls) throw std::invalid_argument("reference input executed no VU callbacks");
            final=reference.projection(); projection=encodeProjection(final);
            events=observer.events(); banks=observer.codeBanks(); generation=m.getVU1CodeGeneration();
            vif=Vif1SnapshotCodec::encode(m); gif=GifSnapshotCodec::encode(runtime->gifArbiter()); gs=GsSnapshotCodec::encode(runtime->gs());
            code.assign(m.getVU1Code(),m.getVU1Code()+16384); data.assign(m.getVU1Data(),m.getVU1Data()+16384);
            binary::Writer a({'N','E','X','O','P','X','R',0},1,detail::vifEventBound);
            a(uint32_t(reference.chunks().size()));
            for (const auto& c:reference.chunks()) { a(c.cycle); a(c.packetEnd); a(c.bytes); }
            chunks=a.finish();
        }
        VU1Interpreter model; VuSnapshotCodec::restore(model,expected.vu);
        if (!recorded.empty() && recorded.back().cycle>model.state().cycles)
            throw std::invalid_argument("recorded event clock exceeds its final VU state");
        const auto diff=differences(final,modelProjection(model.state()));
        const auto actualEvents=decodeEvents(events);
        std::vector<std::vector<uint8_t>> actualPackets,expectedPackets;
        std::vector<uint32_t> actualPaths,expectedPaths;
        std::vector<std::array<uint32_t,5>> actualCalls,expectedCalls;
        for (const auto& e:actualEvents)
        {
            if (e.kind==2) { actualPackets.push_back(e.bytes); actualPaths.push_back(e.args[0]); }
            if (e.kind==1) actualCalls.push_back(e.args);
        }
        for (const auto& e:recorded)
        {
            if (e.kind==2) { expectedPackets.push_back(e.bytes); expectedPaths.push_back(e.args[0]); }
            if (e.kind==1) expectedCalls.push_back(e.args);
        }
        const bool bankMatch=banks==expectedBanks;
        const bool packetMatch=actualPackets==expectedPackets && actualPaths==expectedPaths;
        const bool equal=vif==expected.vif && gif==expected.gif && gs==expected.gs && code==expected.code &&
            data==expected.data && diff.empty() && events==recordedEvents && bankMatch && generation==expected.codeGeneration;
        std::ostringstream report;
        report << "{\"schema\":\"nexo.vu.independent.reference.v1\",\"assurance\":\"tested_only\","
            << "\"reference\":\"PCSX2 VU interpreter\",\"commit\":\"94d86c891b1621c0b252e4fc2e155bf90274dcc0\","
            << "\"scope\":\"original_vif_with_independent_vu_core_and_shared_vif_gif_cpu_gs\","
            << "\"independent_vu_implementation\":true,\"full_reference_qualified\":false,"
            << "\"vu_hidden_state_relation_qualified\":false,\"gs_reference_independent\":false,"
            << "\"final_package_qualified\":false,\"whole_gameplay_qualified\":false,"
            << "\"fp_policy\":{\"round\":\"toward_zero\",\"ftz\":true,\"daz\":true,\"overflow_clamp\":true,\"addsub_hack\":false},"
            << "\"gif_boundary\":\"unmodified_upstream_chunks_aggregated_at_EOP_before_shared_model_delivery\","
            << "\"matches_all_compared_fields\":" << (equal?"true":"false")
            << ",\"vu_calls\":" << calls << ",\"executed_banks\":" << banks.size()
            << ",\"execution_us\":" << double(ns)/1000.0 << ",\"callback_arguments_equal\":" << (actualCalls==expectedCalls?"true":"false")
            << ",\"executed_bank_identities_equal\":" << (bankMatch?"true":"false")
            << ",\"code_generation_equal\":" << (generation==expected.codeGeneration?"true":"false")
            << ",\"packet_payloads_and_paths_equal\":" << (packetMatch?"true":"false")
            << ",\"packets\":{\"actual\":" << actualPackets.size() << ",\"expected\":" << expectedPackets.size() << "},\"comparisons\":{";
        comparison(report,"vif",vif,expected.vif); report << ','; comparison(report,"gif",gif,expected.gif); report << ',';
        comparison(report,"gs",gs,expected.gs); report << ','; comparison(report,"code",code,expected.code); report << ',';
        comparison(report,"data",data,expected.data); report << ','; comparison(report,"events",events,recordedEvents);
        report << "},\"architectural_differences\":[";
        for (size_t i=0;i<diff.size();++i)
        { if (i) report << ','; report << "{\"field\":\"" << diff[i].field << "\",\"reference\":" << diff[i].actual << ",\"recorded_model\":" << diff[i].expected << '}'; }
        report << "],\"callback_outputs\":[";
        for (size_t i=0;i<callbackOutputs.size();++i)
        { if (i) report << ','; report << "{\"index\":" << i << ",\"pc\":" << callbackOutputs[i].pc << ",\"cycles\":" << callbackOutputs[i].cycles << '}'; }
        report << "],\"callback_clock_comparison\":[";
        if (actualCalls==expectedCalls && callbackOutputs.size()==expectedCalls.size())
        {
            std::vector<uint64_t> expectedStarts;
            for (const auto& e:recorded) if (e.kind==1) expectedStarts.push_back(e.cycle);
            for (size_t i=0;i<callbackOutputs.size();++i)
            {
                if (i) report << ',';
                const uint64_t end=i+1<expectedStarts.size()?expectedStarts[i+1]:model.state().cycles;
                const uint64_t referenceStart=i?callbackOutputs[i-1].cycles:0;
                report << "{\"index\":" << i << ",\"reference_elapsed\":" << callbackOutputs[i].cycles-referenceStart
                    << ",\"recorded_elapsed\":" << end-expectedStarts[i] << '}';
            }
        }
        report << "]}\n";
        if (!std::filesystem::create_directory(output)) throw std::runtime_error("cannot exclusively create reference output directory");
        write(output/"reference-events.nexo",events); write(output/"reference-chunks.nexo",chunks);
        write(output/"reference-vu-projection.nexo",projection); write(output/"reference-vif.nexo",vif);
        write(output/"reference-gif.nexo",gif); write(output/"reference-gs.nexo",gs);
        write(output/"reference-code.bin",code); write(output/"reference-data.bin",data);
        binary::Writer callbackWriter({'N','E','X','O','P','X','C',0},1,256u*2048u+28u);
        callbackWriter(uint32_t(callbackOutputs.size()));
        for (const auto& state:callbackOutputs) callbackWriter(encodeProjection(state));
        write(output/"reference-callbacks.nexo",callbackWriter.finish());
        write(output/"report.json",report.str()); write(output/".complete",std::array<uint8_t,1>{1});
        std::cout << report.str(); return equal?0:2;
    }
    catch (const std::exception& e) { std::cerr << "[nexo-pcsx2-reference:error] " << e.what() << '\n'; return 1; }
}

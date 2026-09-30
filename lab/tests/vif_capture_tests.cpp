#include "MiniTest.h"
#include "nexo/vif_capture.h"
#include "nexo/vu_native.h"
#include "nexo/vu_native_semantics.h"
#include "nexo/canonical_binary.h"
#include "ps2_runtime.h"
#include <array>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <future>
#include <optional>
#include <stdexcept>

using namespace ps2native::nexo;
namespace
{
void word(std::vector<uint8_t> &b, uint32_t value)
{ for (unsigned i=0; i<4; ++i) b.push_back(uint8_t(value >> (i*8))); }
void command(std::vector<uint8_t> &b, uint8_t opcode, uint16_t imm=0, uint8_t num=0)
{ word(b,uint32_t(opcode)<<24 | uint32_t(num)<<16 | imm); }
std::vector<uint8_t> stream()
{
    std::vector<uint8_t> b;
    command(b,0x4A,0,2); word(b,0x800006FC); word(b,0x400002FF); word(b,0x8000033C); word(b,0x000002FF);
    command(b,0x6C,0,2);
    // IMAGE tag (one payload qword, EOP) and four CT32 pixels.
    word(b,0x8001); word(b,2u<<26); word(b,0); word(b,0);
    for (uint32_t i=0;i<4;++i) word(b,0x80000011u+i);
    command(b,0x14); command(b,0x07,0x1234);
    return b;
}
std::unique_ptr<PS2Runtime> runtime()
{
    auto r=std::make_unique<PS2Runtime>();
    if (!r->memory().initialize() || !r->syncCoreSubsystems()) throw std::runtime_error("cannot initialize VIF fixture");
    r->gs().writeRegister(GS_REG_BITBLTBUF,1ull<<48);
    r->gs().writeRegister(GS_REG_TRXREG,4ull | (1ull<<32)); r->gs().writeRegister(GS_REG_TRXDIR,0);
    return r;
}
struct Directory
{
    std::filesystem::path path=std::filesystem::temp_directory_path()/
        ("nexo-vif-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::optional<std::string> old;
    Directory()
    {
        if (const char *s=std::getenv("PS2X_CAPTURE_SCENE")) old=s;
        std::filesystem::create_directory(path); setenv("PS2X_CAPTURE_SCENE",path.c_str(),1);
        std::ofstream(path/".vif-request") << "capture next observed VU-bearing VIF call\n";
    }
    ~Directory()
    {
        if (old) setenv("PS2X_CAPTURE_SCENE",old->c_str(),1); else unsetenv("PS2X_CAPTURE_SCENE");
        std::error_code ec; std::filesystem::remove_all(path,ec);
    }
    std::filesystem::path captured() const
    {
        for (const auto &p:std::filesystem::directory_iterator(path))
            if (p.is_directory() && std::filesystem::is_regular_file(p.path()/".complete")) return p.path();
        throw std::runtime_error("no complete VIF capture was produced");
    }
};
std::vector<uint8_t> read(const std::filesystem::path &p)
{
    std::ifstream f(p,std::ios::binary); if (!f) throw std::runtime_error("cannot read fixture artifact");
    return {std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};
}
void write(const std::filesystem::path &p,const std::vector<uint8_t> &b)
{
    std::ofstream f(p,std::ios::binary | std::ios::trunc);
    f.write(reinterpret_cast<const char *>(b.data()),b.size()); if (!f) throw std::runtime_error("cannot write fixture artifact");
}
struct Bank
{
    std::vector<uint8_t> code;
    std::vector<VuNativeAccess::DecodedPair> pairs;
    std::vector<VuNativeAccess::Entry> entries;
    explicit Bank(std::vector<uint8_t> bytes):code(std::move(bytes))
    {
        pairs=VuNativeAccess::inspectMicrocode(code,VU1Interpreter::Unit::VU1);
        entries.resize(pairs.size());
        for (size_t i=0;i<2;++i)
        {
            auto &e=entries[i]; e.decoded=&pairs[i];
            if (pairs[i].upper==0x400002FF) e.upper=&VuNativeAccess::upper<0x400002FF>;
            else if (pairs[i].upper==0x000002FF) e.upper=&VuNativeAccess::upper<0x000002FF>;
            else throw std::runtime_error("uncompiled fixture upper operation");
            if (pairs[i].lower==0x800006FC) e.lower=&VuNativeAccess::lower<0x800006FC>;
            else if (pairs[i].lower==0x8000033C) e.lower=&VuNativeAccess::lower<0x8000033C>;
            else throw std::runtime_error("uncompiled fixture lower operation");
        }
    }
    VuNativeProgram program() const { return {VU1Interpreter::Unit::VU1,entries,code}; }
};
}
int main()
{
    MiniTest::Case("NEXO original VIF call capture",[](TestCase &tc)
    {
        tc.Run("request survives non-VU input and captures the literal original argument",[](TestCase &t)
        {
            auto r=runtime(); Directory d; std::vector<uint8_t> mark; command(mark,0x07,0xFFFF);
            r->memory().processVIF1Data(mark.data(),mark.size());
            t.IsTrue(std::filesystem::exists(d.path/".vif-request"),"An unrelated command cannot consume the VU-bearing request");
            const auto b=stream(); r->memory().processVIF1Data(b.data(),b.size()); const auto c=d.captured();
            t.Equals(read(c/"vif-input.bin"),b,"The captured bytes are the original VIF argument, including MPG and UNPACK");
            t.IsFalse(std::filesystem::exists(d.path/".vif-request"),"A completed case consumes the marker once");
            t.Equals(read(c/"output-state.nexo"),encodeVifBoundary(*r),"The output includes the same post-call VIF VU GIF GS and CPU boundary");
        });
        tc.Run("original VIF replay preserves real GIF routing and final GS VRAM",[](TestCase &t)
        {
            auto r=runtime(); Directory d; const auto b=stream(); r->memory().processVIF1Data(b.data(),b.size());
            const auto c=d.captured(); const auto result=replayVifCase(c);
            t.Equals(result.state,read(c/"output-state.nexo"),"Full composite state exactly matches the recording");
            t.Equals(result.events,read(c/"events.nexo"),"Callback boundaries and GIF submissions retain order and VU cycles");
            t.Equals(result.codeBanks.size(),size_t(1),"One uploaded bank was actually executed");
            t.Equals(r->gs().ReadVram(GS_PSM_CT32,0,1,3,0),0x80000014u,"The real arbiter delivered the IMAGE pixels to GS");
        });
        tc.Run("fragmented command capture keeps literal tail and pending parser state",[](TestCase &t)
        {
            auto r=runtime(); Directory d; const auto b=stream();
            const size_t cut=b.size()-5; r->memory().processVIF1Data(b.data(),cut);
            const std::vector<uint8_t> tail(b.begin()+cut,b.end()); r->memory().processVIF1Data(tail.data(),tail.size());
            const auto c=d.captured(); t.Equals(read(c/"vif-input.bin"),tail,"Pending header bytes are state, not synthesized input");
            const auto result=replayVifCase(c); t.Equals(result.state,read(c/"output-state.nexo"),"Restored parser residue joins the original tail correctly");
            t.Equals(result.events,read(c/"events.nexo"),"The resumed call emits identical observations");
        });
        tc.Run("observer records only its own memory and all uploaded code versions",[](TestCase &t)
        {
            auto r=runtime(); auto other=runtime(); VifObservation observation(*r);
            const auto b=stream(); other->memory().processVIF1Data(b.data(),b.size());
            t.Equals(observation.vuCalls(),0u,"Another memory instance cannot pollute this trace");
            r->memory().processVIF1Data(b.data(),b.size()); auto second=b;
            // Change the second pair's upper operation; the first one still ends.
            second[16]=0xFF; second[17]=0x02; second[18]=0; second[19]=0x40;
            r->memory().processVIF1Data(second.data(),second.size());
            t.Equals(observation.vuCalls(),2u,"Both real callbacks were observed");
            t.Equals(observation.codeBanks().size(),size_t(2),"Each executed microcode version is retained before its callback");
        });
        tc.Run("capture serializes concurrent host presentation at the boundary",[](TestCase &t)
        {
            auto r=runtime(); Directory d; std::future<void> f; bool blocked=false;
            { const auto b=stream(); VifCaptureScope scope(r->memory(),b);
              f=std::async(std::launch::async,[&] { VifPresentationScope presentation(*r); });
              blocked=f.wait_for(std::chrono::milliseconds(20))==std::future_status::timeout; }
            t.IsTrue(blocked,"Presentation waits while component states and the call are captured");
            t.IsTrue(f.wait_for(std::chrono::seconds(1))==std::future_status::ready,"The lease is released after capture scope"); f.get();
        });
        tc.Run("malformed composite input fails before any replay result",[](TestCase &t)
        {
            auto r=runtime(); Directory d; const auto b=stream(); r->memory().processVIF1Data(b.data(),b.size()); const auto c=d.captured();
            auto bad=read(c/"input-state.nexo"); bad.back()^=1; write(c/"input-state.nexo",bad);
            bool rejected=false; try { replayVifCase(c); } catch (const std::invalid_argument &) { rejected=true; }
            t.IsTrue(rejected,"Corrupt component input cannot be replayed as a success");
        });
        tc.Run("native case requires compiled banks and never silently selects reference",[](TestCase &t)
        {
            bool rejected=false; try { replayVifCaseNative("does-not-exist",{}); }
            catch (const std::invalid_argument &) { rejected=true; }
            t.IsTrue(rejected,"An empty native collection is rejected before opening a case");
        });
        tc.Run("native banks replay the original MPG UNPACK MSCAL and GS effects",[](TestCase &t)
        {
            auto r=runtime(); Directory d; const auto b=stream(); r->memory().processVIF1Data(b.data(),b.size());
            const auto c=d.captured(); Bank bank(read(c/"bank-0.bin")); const std::array programs{bank.program()};
            const auto result=replayVifCaseNative(c,programs);
            t.Equals(result.state,read(c/"output-state.nexo"),"Native VU execution retains the complete surrounding VIF GIF GS state");
            t.Equals(result.events,read(c/"events.nexo"),"Native submissions and real GS deliveries match all observed cycles");
            t.Equals(result.codeBanks[0],bank.code,"The bank was selected after the original MPG upload");
        });
        tc.Run("native original VIF case selects both uploaded banks in one call",[](TestCase &t)
        {
            auto r=runtime(); Directory d; auto b=stream(); auto second=b; second[19]=0x40;
            b.insert(b.end(),second.begin(),second.end()); r->memory().processVIF1Data(b.data(),b.size());
            const auto c=d.captured(); Bank a(read(c/"bank-0.bin")), alt(read(c/"bank-1.bin"));
            const std::array programs{a.program(),alt.program()}; const auto result=replayVifCaseNative(c,programs);
            t.Equals(result.codeBanks.size(),size_t(2),"Both callback identities were observed");
            t.Equals(result.state,read(c/"output-state.nexo"),"Switching banks preserves scheduler and transport state");
            t.Equals(result.events,read(c/"events.nexo"),"All upload-dependent callbacks and deliveries match");
        });
        tc.Run("unknown uploaded bank fails without reference execution",[](TestCase &t)
        {
            auto r=runtime(); Directory d; const auto b=stream(); r->memory().processVIF1Data(b.data(),b.size());
            const auto c=d.captured(); Bank wrong(read(c/"bank-0.bin")); wrong.code.back()^=1;
            const std::array programs{wrong.program()}; bool rejected=false;
            try { replayVifCaseNative(c,programs); } catch (const std::runtime_error &e)
            { rejected=std::string(e.what()).find("UNSEEN_CODE")==0; }
            t.IsTrue(rejected,"A byte identity mismatch is a native closure failure, not a successful fallback");
        });
        tc.Run("an aborted runtime callback cannot publish a completed capture",[](TestCase &t)
        {
            auto r=runtime(); Directory d;
            r->memory().setVu1MscalCallback([](uint32_t,uint32_t,uint32_t) { throw std::runtime_error("fixture callback aborted"); });
            const auto b=stream(); bool failed=false; try { r->memory().processVIF1Data(b.data(),b.size()); }
            catch (const std::runtime_error &) { failed=true; }
            t.IsTrue(failed,"The guest execution failure is preserved");
            bool complete=false; for (const auto &p:std::filesystem::directory_iterator(d.path))
                if (p.is_directory() && std::filesystem::exists(p.path()/".complete")) complete=true;
            t.IsFalse(complete,"No successful case is published after unwinding");
            t.IsTrue(std::filesystem::exists(d.path/".vif-request"),"The failed capture does not consume the request");
        });
    });
    return MiniTest::Run();
}

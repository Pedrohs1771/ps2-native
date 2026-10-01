#include "MiniTest.h"
#include "nexo/pcsx2_vu_reference.h"
#include "nexo/vu_snapshot.h"
#include "runtime/ps2_vu1.h"
#include "runtime/ps2_memory.h"
#include "runtime/gs/gs_frontend.h"
#include <cstring>
#include <stdexcept>
#include <future>
#include <algorithm>

using namespace ps2native::nexo;
namespace
{
struct Fixture
{
    alignas(16) std::array<uint8_t,16384> code{},data{};
    std::vector<std::vector<uint8_t>> packets;
    std::function<void()> observePacket;
    std::vector<uint8_t> seed=VuSnapshotCodec::encode(VU1Interpreter{});
    void pair(size_t pc,uint32_t lower,uint32_t upper)
    { std::memcpy(code.data()+pc,&lower,4); std::memcpy(code.data()+pc+4,&upper,4); }
    std::unique_ptr<Pcsx2Vu1Reference> reference()
    { return std::make_unique<Pcsx2Vu1Reference>(seed,code,data,[&](auto bytes)
        { packets.emplace_back(bytes.begin(),bytes.end()); if (observePacket) observePacket(); }); }
    VU1State model()
    {
        auto memory=std::make_unique<PS2Memory>();
        if (!memory->initialize()) throw std::runtime_error("cannot initialize model timing fixture");
        GS gs; gs.init(memory->getGSVRAM(),PS2_GS_VRAM_SIZE,&memory->gs());
        std::memcpy(memory->getVU1Code(),code.data(),code.size());
        std::memcpy(memory->getVU1Data(),data.data(),data.size());
        VU1Interpreter vu;
        vu.execute(memory->getVU1Code(),PS2_VU1_CODE_SIZE,memory->getVU1Data(),
            PS2_VU1_DATA_SIZE,gs,memory.get(),0,0,0,65536);
        return vu.state();
    }
};
template <typename F> bool rejects(F f)
{ try { f(); return false; } catch (const std::exception&) { return true; } }
}
int main()
{
    MiniTest::Case("PCSX2 independent VU1 reference adapter",[](TestCase& tc)
    {
        tc.Run("executes upstream E flag and its delay pair",[](TestCase& t)
        {
            Fixture f; f.pair(0,0x10010007,0x400002FF); f.pair(8,0x10020009,0x000002FF);
            auto ref=f.reference(); ref->execute(true,0,0,0); const auto p=ref->projection();
            t.Equals(p.vi[1],uint16_t(7),"The first pair executes in the upstream interpreter");
            t.Equals(p.vi[2],uint16_t(9),"The E delay pair also executes");
            t.Equals(p.pc,16u,"The upstream byte PC follows both pairs");
            t.IsTrue(p.cycles>=2,"Upstream cycle accounting is retained");
        });
        tc.Run("retains registers across fresh microcalls and supplies TOP",[](TestCase& t)
        {
            Fixture f; f.pair(0,0x800106BC,0x400002FF); f.pair(8,0x8000033C,0x000002FF);
            auto ref=f.reference(); ref->execute(true,0,235,0);
            t.Equals(ref->projection().vi[1],uint16_t(235),"XTOP consumes the callback TOP");
            ref->execute(true,0,269,0);
            t.Equals(ref->projection().vi[1],uint16_t(269),"The second call uses the new TOP");
        });
        tc.Run("collects original XGKICK chunks and an EOP packet",[](TestCase& t)
        {
            Fixture f; f.pair(0,0x800006FC,0x400002FF); f.pair(8,0x8000033C,0x000002FF);
            uint64_t tag=0x8001ull | (2ull<<58); std::memcpy(f.data.data(),&tag,8);
            for (size_t i=16;i<32;++i) f.data[i]=uint8_t(i);
            auto ref=f.reference(); ref->execute(true,0,0,0);
            t.Equals(f.packets.size(),size_t(1),"One completed upstream packet is delivered");
            if (!f.packets.empty()) t.Equals(f.packets[0],std::vector<uint8_t>(f.data.begin(),f.data.begin()+32),
                "Packet bytes come from the original VU data memory");
            t.IsTrue(!ref->chunks().empty() && ref->chunks().back().packetEnd,"Raw chunk boundaries remain available");
        });
        tc.Run("rejects unmapped hidden-state seeds",[](TestCase& t)
        {
            Fixture f; VU1Interpreter dirty; dirty.state().cycles=1;
            t.IsTrue(rejects([&] { f.seed=VuSnapshotCodec::encode(dirty); }),"Malformed internal clock is rejected by the codec");
            dirty.reset(); dirty.state().vi[1]=1; f.seed=VuSnapshotCodec::encode(dirty);
            t.IsTrue(rejects([&] { auto ref=f.reference(); }),"A non-reset seed requires a separately qualified mapping");
        });
        tc.Run("rejects memory aliasing and simultaneous upstream owners",[](TestCase& t)
        {
            Fixture f;
            t.IsTrue(rejects([&] { Pcsx2Vu1Reference ref(f.seed,f.code,f.code,[](auto){}); }),"Micro and data cannot alias");
            auto ref=f.reference(); Fixture second;
            t.IsTrue(rejects([&] { auto other=second.reference(); }),"Global upstream registers cannot be shared by two adapters");
        });
        tc.Run("fails closed for unknown opcodes and exhausted horizons",[](TestCase& t)
        {
            Fixture f; f.pair(0,0xFE000000,0x000002FF); auto ref=f.reference();
            t.IsTrue(rejects([&] { ref->execute(true,0,0,0); }),"Unknown upstream opcode cannot succeed");
            ref.reset(); Fixture loop; loop.pair(0,0x8000033C,0x000002FF); auto endless=loop.reference();
            t.IsTrue(rejects([&] { endless->execute(true,0,0,0,1); }),"An unfinished microprogram fails at its explicit budget");
        });
        tc.Run("rejects foreign-thread execution and permits safe destruction",[](TestCase& t)
        {
            Fixture f; auto ref=f.reference();
            auto foreign=std::async(std::launch::async,[&] { return rejects([&] { ref->execute(true,0,0,0); }); });
            t.IsTrue(foreign.get(),"The upstream global machine executes only on its owner thread");
            auto destroy=std::async(std::launch::async,[owned=std::move(ref)]() mutable { owned.reset(); });
            destroy.get(); auto replacement=f.reference();
            t.IsNotNull(replacement.get(),"Ownership cleanup does not depend on unlocking a foreign-thread mutex");
        });
        tc.Run("rejects misaligned memory and restores ownership after import failure",[](TestCase& t)
        {
            Fixture f; alignas(16) std::array<uint8_t,16385> unaligned{};
            t.IsTrue(rejects([&] { Pcsx2Vu1Reference ref(f.seed,{unaligned.data()+1,16384},f.data,[](auto){}); }),
                "Upstream typed loads require the identified memory alignment");
            auto replacement=f.reference(); t.IsNotNull(replacement.get(),"A failed import releases the upstream owner");
        });
        tc.Run("issue trace observes the delay pair without changing execution",[](TestCase& t)
        {
            Fixture f; f.pair(0,0x10010007,0x400002FF); f.pair(8,0x10020009,0x000002FF);
            auto plain=f.reference(); plain->execute(true,0,0,0); const auto expected=plain->projection();
            t.IsTrue(plain->issues().empty(),"Issue traces are disabled by default"); plain.reset();
            auto ref=f.reference(); ref->enableIssueTrace(); ref->execute(true,0,0,0);
            t.Equals(ref->projection().vi,expected.vi,"The logger does not alter architectural values");
            t.Equals(ref->cycles(),expected.cycles,"The logger does not alter guest time");
            t.Equals(ref->issues().size(),size_t(2),"Each upstream upper dispatch is observed once");
            if (ref->issues().size()==2)
            {
                t.Equals(ref->issues()[0].pc,0u,"The observation relates to the dispatched byte PC");
                t.Equals(ref->issues()[0].cycle,uint64_t(1),"The upstream leading tick is explicitly retained");
                t.Equals(ref->issues()[1].pc,8u,"The E delay pair is also observed");
                t.Equals(ref->issues()[1].lower,0x10020009u,"The original lower word is recorded");
            }
            t.IsTrue(rejects([&] { ref->enableIssueTrace(); }),"A partial trace cannot be enabled retrospectively");
        });
        tc.Run("isolates load to integer add timing without electing a hardware oracle",[](TestCase& t)
        {
            Fixture f;
            // ILW.x VI1, 0(VI0); IADD VI2, VI1, VI0 [E]; NOP delay.
            f.pair(0,0x09010000,0x000002FF);
            f.pair(8,0x800008B0,0x400002FF); f.pair(16,0x8000033C,0x000002FF);
            const uint32_t value=7; std::memcpy(f.data.data(),&value,4);
            const auto model=f.model();
            auto ref=f.reference(); ref->enableIssueTrace(); ref->execute(true,0,0,0);
            t.Equals(uint16_t(model.vi[2]),uint16_t(7),"The model waits for and consumes the loaded value");
            t.Equals(ref->projection().vi[2],uint16_t(7),"The upstream implementation obtains the same value");
            t.Equals(ref->issues().size(),size_t(3),"The reduced case executes exactly three pairs");
            if (ref->issues().size()==3)
                t.Equals(ref->issues()[1].cycle,uint64_t(2),"Upstream IADD issues immediately after ILW");
            t.IsTrue(model.cycles>ref->cycles(),"The load-consumer scheduling disagreement is retained");
        });
        tc.Run("independent dispatch observes the second XGKICK upper before first packet delivery",[](TestCase& t)
        {
            Fixture f;
            f.pair(0,0x10020009,0x000002FF); // VI2 = second packet's qword address.
            f.pair(8,0x0101000B,0x000002FF); // LQ.x VF1, 11(VI0).
            f.pair(16,0x0102000C,0x000002FF); // LQ.x VF2, 12(VI0).
            f.pair(24,0x800006FC,0x000002FF);
            f.pair(32,0x800016FC,0x010208E8); // XGKICK VI2 / ADD.x VF3,VF1,VF2.
            f.pair(40,0x8000033C,0x400002FF); f.pair(48,0x8000033C,0x000002FF);
            const uint64_t first=0x8008ull|(2ull<<58),second=0x8001ull|(2ull<<58);
            std::memcpy(f.data.data(),&first,8); std::memset(f.data.data()+16,0x11,128);
            std::memcpy(f.data.data()+144,&second,8); std::memset(f.data.data()+160,0x22,16);
            const float a=2,b=7; std::memcpy(f.data.data()+176,&a,4); std::memcpy(f.data.data()+192,&b,4);
            auto ref=f.reference(); std::vector<size_t> dispatchedAtDelivery;
            f.observePacket=[&] { dispatchedAtDelivery.push_back(ref->issues().size()); };
            ref->enableIssueTrace(); ref->execute(true,0,0,0);
            t.Equals(ref->issues().size(),size_t(7),"The independent case has seven instruction pairs");
            // The upstream bulk flush records its chunk before adding its
            // drain cycles. Observation order is stronger than comparing two
            // equal raw timestamps from different diagnostic points.
            if (!dispatchedAtDelivery.empty())
                t.Equals(dispatchedAtDelivery[0],size_t(5),"The second upper dispatch precedes delivery of the first packet");
            if (ref->issues().size()==7)
                t.IsTrue(ref->issues()[5].cycle>ref->issues()[4].cycle+1,"The following pair is delayed by the pending transfer");
            t.Equals(ref->projection().vf[12],0x41100000u,"The independent upper ADD produces nine");
            t.Equals(f.packets.size(),size_t(2),"Both source packets are delivered in order");
            if (f.packets.size()==2)
                t.IsTrue(f.packets[0].size()==144 && f.packets[0][16]==0x11 && f.packets[1].size()==32 && f.packets[1][16]==0x22,
                    "The reference preserves both packet payloads");
        });

        tc.Run("isolates end of program XGKICK drain from instruction issue",[](TestCase& t)
        {
            Fixture plain; plain.pair(0,0x8000033C,0x400002FF); plain.pair(8,0x8000033C,0x000002FF);
            const auto modelPlain=plain.model();
            auto refPlain=plain.reference(); refPlain->execute(true,0,0,0);
            const auto referencePlain=refPlain->cycles(); refPlain.reset();
            Fixture kick; kick.pair(0,0x800006FC,0x400002FF); kick.pair(8,0x8000033C,0x000002FF);
            // One EOP IMAGE tag and eight qwords of payload; no GS commands.
            const uint64_t tag=0x8008ull|(2ull<<58); std::memcpy(kick.data.data(),&tag,8);
            const auto modelKick=kick.model();
            auto ref=kick.reference(); ref->enableIssueTrace(); ref->execute(true,0,0,0);
            t.Equals(ref->cycles(),referencePlain,"Upstream end flush delivers the pending packet without advancing VU time");
            t.IsTrue(modelKick.cycles>modelPlain.cycles,"The model accounts for pending XGKICK drain cycles");
            t.Equals(ref->issues().size(),size_t(2),"XGKICK does not change the two-pair instruction sequence");
            t.Equals(kick.packets.size(),size_t(1),"The end flush still emits its complete packet");
            if (!kick.packets.empty()) t.Equals(kick.packets[0].size(),size_t(144),"The reduced packet is not shortened");
        });
    });
    return MiniTest::Run();
}

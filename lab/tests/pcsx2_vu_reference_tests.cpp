#include "MiniTest.h"
#include "nexo/pcsx2_vu_reference.h"
#include "nexo/vu_snapshot.h"
#include "runtime/ps2_vu1.h"
#include <cstring>
#include <stdexcept>
#include <future>

using namespace ps2native::nexo;
namespace
{
struct Fixture
{
    alignas(16) std::array<uint8_t,16384> code{},data{};
    std::vector<std::vector<uint8_t>> packets;
    std::vector<uint8_t> seed=VuSnapshotCodec::encode(VU1Interpreter{});
    void pair(size_t pc,uint32_t lower,uint32_t upper)
    { std::memcpy(code.data()+pc,&lower,4); std::memcpy(code.data()+pc+4,&upper,4); }
    std::unique_ptr<Pcsx2Vu1Reference> reference()
    { return std::make_unique<Pcsx2Vu1Reference>(seed,code,data,[&](auto bytes)
        { packets.emplace_back(bytes.begin(),bytes.end()); }); }
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
    });
    return MiniTest::Run();
}

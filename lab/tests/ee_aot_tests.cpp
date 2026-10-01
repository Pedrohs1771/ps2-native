#include "MiniTest.h"
#include "ps2_ee_aot.h"
#include "ps2_native_overlay.h"

#include <cstring>
#include <stdexcept>

using namespace ps2native::ee_aot;
namespace
{
    void first(uint8_t *, R5900Context *, PS2Runtime *) {}
    void second(uint8_t *, R5900Context *, PS2Runtime *) {}
    bool rejects(const Program &program)
    {
        try { Dispatcher dispatcher(program); return false; }
        catch (const std::invalid_argument &) { return true; }
    }
}

int main()
{
    MiniTest::Case("Finite EE AOT directory", [](TestCase &suite)
    {
        suite.Run("versions_identity_and_interior_entries", [](TestCase &test)
        {
            std::vector<uint8_t> ram(PS2_RAM_SIZE);
            std::vector<uint8_t> one{1,0,0,0,2,0,0,0}, two{3,0,0,0,4,0,0,0};
            const PS2NativeOverlayBinding firstBindings[] = {{0x10000,first,0x10000,8,one.data()},
                                                            {0x10004,first,0x10000,8,one.data()}};
            const PS2NativeOverlayBinding secondBindings[] = {{0x10000,second,0x10000,8,two.data()},
                                                             {0x10004,second,0x10000,8,two.data()}};
            const Bank banks[] = {{0x10000,one,firstBindings},{0x10000,two,secondBindings}};
            Dispatcher dispatcher(Program{banks});
            test.Equals(dispatcher.pages(),size_t{1},"Sparse pages share one directory");
            test.Equals(dispatcher.bindings(),size_t{4},"Both versions remain available");
            std::memcpy(ram.data()+0x10000,one.data(),8);
            one[0]=99; // Registry owns identity bytes rather than borrowing mutable descriptors.
            test.IsTrue(dispatcher.lookup(ram.data(),0x10000).function==first,"Older matching version selected");
            test.IsTrue(dispatcher.lookup(ram.data(),0x10004).function==first,"Interior entry admitted");
            std::memcpy(ram.data()+0x10000,two.data(),8);
            test.IsTrue(dispatcher.lookup(ram.data(),0x10000).function==second,"Replacement selects existing native version");
            ram[0x10007]^=1;
            test.IsTrue(dispatcher.lookup(ram.data(),0x10000).status==Status::CodeChanged,"Tail changes reject stale code");
            const auto diagnosis = dispatcher.diagnose(ram.data(),0x10000);
            test.IsTrue(diagnosis.lookup.status==Status::CodeChanged,"Diagnosis retains the lookup result");
            test.Equals(diagnosis.candidates.size(),size_t{2},"Both precompiled candidates are diagnosed");
            if (diagnosis.candidates.empty()) return;
            test.Equals(diagnosis.candidates.front().sourceBegin,uint32_t{0x10000},"Dependency base is explicit");
            test.Equals(diagnosis.candidates.front().mismatchBytes,uint32_t{1},"Byte differences are counted exactly");
            test.Equals(diagnosis.candidates.front().firstMismatchOffset,uint32_t{7},"Tail mismatch is located");
            test.Equals(diagnosis.candidates.front().expected.size(),size_t{8},"Expected bytes are preserved");
            test.Equals(diagnosis.candidates.front().expected[0],uint8_t{3},"Directory retains owned immutable identity");
            test.IsTrue(dispatcher.diagnose(nullptr,0x10000).candidates.empty(),"Null RAM does not expose candidate bytes");
            test.IsTrue(dispatcher.lookup(ram.data(),0x10008).status==Status::MissingEntry,"Unregistered PC rejected");
            test.IsTrue(dispatcher.lookup(ram.data(),0x10002).status==Status::MisalignedPc,"Unaligned PC rejected");
            test.IsTrue(dispatcher.lookup(ram.data(),PS2_RAM_SIZE).status==Status::OutsideRam,"Aliases outside the declared physical domain rejected");
            test.IsTrue(dispatcher.lookup(nullptr,0x10000).status==Status::NoRam,"Null RAM rejected");
        });
        suite.Run("malformed_metadata_never_publishes_callbacks", [](TestCase &test)
        {
            std::vector<uint8_t> bytes(12,0);
            PS2NativeOverlayBinding binding{0x10000,first,0x10000,8,bytes.data()};
            Bank bank{0x10000,bytes,std::span(&binding,1)};
            const auto check=[&] { return rejects(Program{std::span(&bank,1)}); };
            binding.function=nullptr;test.IsTrue(check(),"Null callback rejected");binding.function=first;
            binding.sourceSize=16;test.IsTrue(check(),"Footprint outside image rejected");binding.sourceSize=8;
            binding.sourceBegin=0x10004;test.IsTrue(check(),"Misidentified source rejected");binding.sourceBegin=0x10000;
            binding.address=0x10008;test.IsTrue(check(),"PC outside source footprint rejected");binding.address=0x10000;
            binding.sourceBytes=bytes.data()+1;binding.sourceBegin=0x10001;binding.address=0x10004;binding.sourceSize=4;
            test.IsTrue(check(),"Unaligned dependency footprint rejected");
        });
    });
    if (ps2xUsesAotEeOverlays())
        MiniTest::Case("AOT EE runtime admission", [](TestCase &suite)
        {
            suite.Run("uncovered_calls_stop_even_under_diagnostic_policies", [](TestCase &test)
            {
                for (const auto policy : {PS2Runtime::MissingFunctionPolicy::ContinueToTarget,
                                          PS2Runtime::MissingFunctionPolicy::SkipCallDebug})
                {
                    PS2Runtime runtime;
                    test.IsTrue(runtime.memory().initialize(),"RAM initialization");
                    runtime.setMissingFunctionPolicy(policy);
                    R5900Context ctx{};
                    const bool resumed = runtime.dispatchGuestBranch(runtime.memory().getRDRAM(), &ctx,
                        0x20000, 0x10000, 0x10008, PS2Runtime::GuestBranchKind::IndirectCall,"AOT test");
                    test.IsTrue(runtime.isStopRequested(),"Uncovered call stops runtime");
                    test.IsTrue(!resumed,"Uncovered call never succeeds or skips");
                    test.Equals(ctx.pc,uint32_t{0x20000},"Missing PC preserved");
                }
            });
        });
    return MiniTest::Run();
}

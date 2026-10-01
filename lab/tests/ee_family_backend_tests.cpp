#include "MiniTest.h"
#include "ps2_ee_aot.h"
#include "ps2_ee_data_family.h"
#include "ps2_ee_overlay_backend.h"
#include "ps2_native_overlay.h"
#include <array>
#include <cstring>
#include <stdexcept>

namespace
{
    uint32_t calls=0,lastBase=0,lastWord=0;
    void record(uint8_t *ram,R5900Context *,PS2Runtime *,uint32_t base)
    {
        ++calls;lastBase=base;std::memcpy(&lastWord,ram+base,4);
    }
    const std::array words{0x3c010000u,0x03e00008u,0u};
    const std::array masks{0xffff0000u,0xffffffffu,0xffffffffu};
    const std::array entries{0u,4u,8u};
    const std::array nop{0u},nopMask{0xffffffffu},nopEntry{0u};
    bool declines(PS2Runtime::RecompiledFunction callback,uint8_t *ram,R5900Context *context)
    {
        try { callback(ram,context,nullptr);return false; }
        catch(const std::invalid_argument &) { return true; }
    }
}

// Test descriptors record admission only; they do not implement guest semantics.
const ps2native::ee_aot::Program &compiledEeProgram()
{
    static const ps2native::ee_aot::Program empty{};return empty;
}
const ps2native::ee_family::Program &compiledEeFamilyProgram()
{
    static const std::array families{
        ps2native::ee_family::Family{record,words,masks,entries},
        ps2native::ee_family::Family{record,nop,nopMask,nopEntry}};
    static const ps2native::ee_family::Program program{families};return program;
}

int main()
{
    MiniTest::Case("Compiled EE family backend recheck",[](TestCase &suite)
    {
        suite.Run("physical_base_and_actual_context_are_requeried",[](TestCase &test)
        {
            std::vector<uint8_t> ram(PS2_RAM_SIZE);calls=0;
            const std::array live{0x3c01f001u,0x03e00008u,0u};
            for(const uint32_t base:{0x10000u,0x1700000u})
            {
                std::memcpy(ram.data()+base,live.data(),12);
                for(const uint32_t offset:{0u,4u})
                {
                    R5900Context context{};context.pc=base+offset;
                    const auto callback=ps2xResolveNativeOverlay(nullptr,ram.data(),context.pc);
                    test.IsTrue(callback!=nullptr,"Precompiled structure is available at an unseen base");
                    if(!callback) continue;
                    // Change a data field after resolution; invocation observes current RAM.
                    const uint32_t changed=0x3c018002u;std::memcpy(ram.data()+base,&changed,4);
                    callback(ram.data(),&context,nullptr);
                    test.Equals(lastBase,base,"Invocation reconstructs its physical base");
                    test.Equals(lastWord,changed,"Invocation observes the live parameter");
                }
            }
            test.Equals(calls,4u,"All normal entries invoked the recorded native callback");
        });
        suite.Run("stale_bytes_pc_and_pending_slot_never_invoke",[](TestCase &test)
        {
            std::vector<uint8_t> ram(PS2_RAM_SIZE);calls=0;
            std::memcpy(ram.data()+0x10000,words.data(),12);
            R5900Context context{};context.pc=0x10000;
            const auto callback=ps2xResolveNativeOverlay(nullptr,ram.data(),context.pc);
            test.IsTrue(callback!=nullptr,"Initial query admitted");if(!callback)return;
            ram[0x10007]^=0x40;
            test.IsTrue(declines(callback,ram.data(),&context),"Fixed bits changed after query are rejected");
            ram[0x10007]^=0x40;context.in_delay_slot=true;
            test.IsTrue(declines(callback,ram.data(),&context),"Pending architectural delay context rejected");
            context.in_delay_slot=false;context.pc=0x10002;
            test.IsTrue(declines(callback,ram.data(),&context),"Actual invocation PC is checked");
            test.IsTrue(declines(callback,ram.data(),nullptr),"Null invocation context rejected");
            test.Equals(calls,0u,"Rejected invocations have no guest callback effects");
        });
        suite.Run("ambiguous_entry_never_resolves",[](TestCase &test)
        {
            std::vector<uint8_t> ram(PS2_RAM_SIZE);std::memcpy(ram.data()+0x10000,words.data(),12);
            test.IsTrue(ps2xResolveNativeOverlay(nullptr,ram.data(),0x10008)==nullptr,
                        "Overlapping complete guards cannot select a callback");
        });
    });
    return MiniTest::Run();
}

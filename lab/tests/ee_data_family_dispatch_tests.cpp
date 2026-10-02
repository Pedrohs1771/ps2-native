#include "MiniTest.h"
#include "ps2_ee_data_family.h"
#include <array>
#include <cstring>
#include <stdexcept>

using namespace ps2native::ee_family;
namespace
{
    void function(uint8_t *,R5900Context *,PS2Runtime *,uint32_t) {}
    bool rejects(const Family &family)
    {
        try{Dispatcher dispatcher(Program{std::span(&family,1)});return false;}
        catch(const std::invalid_argument &){return true;}
    }
}
int main()
{
    MiniTest::Case("Precompiled EE family admission",[](TestCase &suite)
    {
        suite.Run("relocation_live_data_entries_and_owned_guards",[](TestCase &test)
        {
            std::array words{0x3c010000u,0xac220000u,0x03e00008u,0u};
            std::array masks{0xffff0000u,0xffff0000u,0xffffffffu,0xffffffffu};
            const std::array offsets{0u,4u,8u,12u};
            const Family family{function,words,masks,offsets};Dispatcher dispatcher(Program{std::span(&family,1)});
            words[2]=0; masks[0]=0; // Dispatcher owns its identity.
            std::vector<uint8_t> ram(PS2_RAM_SIZE);
            const std::array live{0x3c010185u,0xac22f8c4u,0x03e00008u,0u};
            for(const uint32_t base:{0x10000u,0x184f474u,PS2_RAM_SIZE-16u})
            {
                std::memcpy(ram.data()+base,live.data(),16);
                for(const auto offset:offsets)
                {
                    const auto admission=dispatcher.lookup(ram.data(),base+offset);
                    test.IsTrue(admission.function==function,"Same precompiled callback selected");
                    test.Equals(admission.base,base,"Physical family base reconstructed");
                }
                ram[base+11]^=0x40;
                test.IsTrue(dispatcher.lookup(ram.data(),base).status==Status::MissingEntry,"Changed fixed control bits rejected");
                ram[base+11]^=0x40;
                R5900Context context{};context.pc=base;context.in_delay_slot=true;
                test.IsTrue(dispatcher.admitNormalEntry(ram.data(),&context).status==Status::UnsupportedEntryContext,"Architectural slot context rejected");
            }
            test.IsTrue(dispatcher.lookup(nullptr,0).status==Status::NoRam,"Null RAM rejected");
            test.IsTrue(dispatcher.lookup(ram.data(),2).status==Status::MisalignedPc,"Misaligned PC rejected");
            test.IsTrue(dispatcher.lookup(ram.data(),PS2_RAM_SIZE).status==Status::OutsideRam,"Physical aliases outside profile rejected");
            test.IsTrue(dispatcher.admitNormalEntry(ram.data(),nullptr).status==Status::UnsupportedEntryContext,"Null context rejected");
        });
        suite.Run("ambiguous_structures_never_choose_a_callback",[](TestCase &test)
        {
            const std::array words{0x03e00008u,0u},masks{0xffffffffu,0xffffffffu},offsets{0u,4u};
            const std::array nop{0u},nopMask{0xffffffffu},nopEntry{0u};
            const std::array families{Family{function,words,masks,offsets},Family{function,nop,nopMask,nopEntry}};
            Dispatcher dispatcher(Program{families});std::vector<uint8_t> ram(PS2_RAM_SIZE);
            std::memcpy(ram.data()+0x10000,words.data(),8);
            const auto result=dispatcher.lookup(ram.data(),0x10004);
            test.IsTrue(result.status==Status::Ambiguous && !result.function,"Multiple valid interpretations are explicit failure");
        });
        suite.Run("untrusted_descriptor_dimensions_and_masks_are_rejected",[](TestCase &test)
        {
            const std::array words{0x3c010000u,0x03e00008u,0u};
            const std::array masks{0xffff0000u,0xffffffffu,0xffffffffu};
            const std::array offsets{0u};
            Family family{nullptr,words,masks,offsets};test.IsTrue(rejects(family),"Null callback rejected");family.function=function;
            const std::array badMask{0xfc000000u,0xffffffffu,0xffffffffu};family.masks=badMask;test.IsTrue(rejects(family),"Opcode/register masks rejected");family.masks=masks;
            const std::array badEntry{2u};family.normalOffsets=badEntry;test.IsTrue(rejects(family),"Misaligned entry rejected");
            const std::array outside{12u};family.normalOffsets=outside;test.IsTrue(rejects(family),"Entry outside guard rejected");
            family.normalOffsets=offsets;const std::array duplicate{family,family};
            bool rejected=false;try{Dispatcher dispatcher(Program{duplicate});}catch(const std::invalid_argument &){rejected=true;}
            test.IsTrue(rejected,"Duplicate structures cannot hide ambiguous callbacks");
        });
        suite.Run("typed_data_classifier_is_shared_with_native_synthesis",[](TestCase &test)
        {
            const std::array masks{0xffff0000u,0xffffffffu,0xffffffffu};
            const std::array entries{0u};
            for(const uint32_t opcode:{0x09u,0x0au,0x0bu,0x0cu,0x0du,0x0eu,0x0fu,
                                      0x20u,0x21u,0x23u,0x24u,0x25u,0x27u,0x37u,0x1eu,
                                      0x28u,0x29u,0x2bu,0x3fu,0x1fu})
            {
                const std::array words{(opcode<<26)|((opcode==0x0fu?0u:5u)<<21)|(2u<<16),0x03e00008u,0u};
                const Family family{function,words,masks,entries};
                test.IsTrue(!rejects(family),"Supported data mask admitted");
            }
            for(const uint32_t opcode:{1u,2u,3u,4u,5u,6u,7u,0x14u,0x15u,0x16u,0x17u,0x10u,0x11u,0x12u})
            {
                const std::array words{opcode<<26,0x03e00008u,0u};
                const Family family{function,words,masks,entries};
                test.IsTrue(rejects(family),"Control/device operands cannot be made variable by this profile");
            }
        });
        suite.Run("batched_catalog_budget_is_explicit",[](TestCase &test)
        {
            const std::array masks{0xffffffffu,0xffffffffu,0xffffffffu};
            const std::array entries{0u};
            std::vector<std::array<uint32_t,3>> words(4097);
            std::vector<Family> families;
            families.reserve(words.size());
            for(size_t i=0;i<words.size();++i)
            {
                words[i]={0x3c020000u+static_cast<uint32_t>(i),0x03e00008u,0u};
                families.push_back({function,words[i],masks,entries});
            }
            Dispatcher dispatcher(Program{families});
            test.Equals(dispatcher.families(),words.size(),"Finite corpus above the old budget is represented");
            std::vector<Family> tooMany(32769);
            bool rejected=false;
            try { Dispatcher oversized(Program{tooMany}); }
            catch(const std::invalid_argument &) { rejected=true; }
            test.IsTrue(rejected,"Oversized catalogs fail before descriptor traversal");
        });
    });
    return MiniTest::Run();
}

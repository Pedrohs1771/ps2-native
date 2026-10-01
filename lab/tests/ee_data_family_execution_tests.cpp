#include "MiniTest.h"
#include "ee_data_family_fixture.h"
#include "nexo/ee_snapshot.h"
#include "ps2_runtime_macros.h"

#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace
{
    void callee(uint8_t *,R5900Context *ctx,PS2Runtime *)
    {
        SET_GPR_U32(ctx,2,GPR_U32(ctx,2)+1u);
        ctx->pc=GPR_U32(ctx,31);
    }
}
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x40000;
extern const uint32_t g_ps2RecompiledFunctionTableEnd=0x40004;
extern const uint32_t g_ps2RecompiledFunctionTableSlotCount=1;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[1]={callee};

int main()
{
    MiniTest::Case("Generated native EE data families",[](TestCase &suite)
    {
        suite.Run("all_relative_entries_match_fixed_native_translation",[](TestCase &test)
        {
            PS2Runtime referenceRuntime,familyRuntime;
            std::vector<uint8_t> original(PS2_RAM_SIZE,0x5a),reference(PS2_RAM_SIZE),family(PS2_RAM_SIZE);
            size_t checks=0;
            const auto fixtures=eeDataFamilyFixtures();
            size_t expectedChecks=0;
            for(size_t i=0;i<fixtures.size();++i)
            {
                const auto &fixture=fixtures[i];
                expectedChecks+=fixture.words.size();
                std::memcpy(original.data()+fixture.base,fixture.words.data(),fixture.words.size_bytes());
                const uint64_t savedFp=0x620000,savedRa=0x70000;
                std::memcpy(original.data()+0x600010,&savedFp,8);
                std::memcpy(original.data()+0x600020,&savedRa,8);
                for(uint32_t offset=0;offset<fixture.words.size_bytes();offset+=4)
                {
                    reference=original;family=original;
                    R5900Context input{};
                    for(unsigned reg=1;reg<32;++reg) input.r[reg]=_mm_set_epi64x(0x1234000000000000ull+reg,0x22000000ull+reg);
                    SET_GPR_U32(&input,1,0x100000);SET_GPR_U32(&input,2,0x12345678);
                    SET_GPR_U32(&input,4,0x40000);SET_GPR_U32(&input,5,0x180000);
                    SET_GPR_U32(&input,29,0x5fffd0);SET_GPR_U32(&input,30,0x600000);SET_GPR_U32(&input,31,0x70000);
                    input.pc=fixture.base+offset;input.branch_pc=0x76540;
                    auto expected=input,actual=input;
                    runEeDataReference(i,reference.data(),&expected,&referenceRuntime);
                    runEeDataFamily(fixture.shape,family.data(),&actual,&familyRuntime,fixture.base);
                    if(ps2native::nexo::EeSnapshotCodec::encode(expected)!=ps2native::nexo::EeSnapshotCodec::encode(actual) || reference!=family)
                    {
                        test.Fail("Native family differs at fixture "+std::to_string(i)+" offset "+std::to_string(offset));
                        return;
                    }
                    ++checks;
                }
            }
            test.Equals(checks,expectedChecks,"Every normal instruction and standalone terminal slot entry compared");
            std::cout << "Compared normal entries: " << checks << " across " << fixtures.size() << " fixtures\n";
        });
        suite.Run("guards_reject_before_context_or_ram_effects",[](TestCase &test)
        {
            PS2Runtime runtime;
            const auto fixture=eeDataFamilyFixtures().front();
            std::vector<uint8_t> ram(PS2_RAM_SIZE);
            std::memcpy(ram.data()+fixture.base,fixture.words.data(),fixture.words.size_bytes());
            R5900Context context{};context.pc=fixture.base;
            const auto rejects=[&](R5900Context candidate,uint32_t base)
            {
                const auto before=ps2native::nexo::EeSnapshotCodec::encode(candidate);
                const auto bytes=ram;
                bool rejected=false;
                try{runEeDataFamily(fixture.shape,ram.data(),&candidate,&runtime,base);}
                catch(const std::invalid_argument &){rejected=true;}
                test.IsTrue(rejected,"Unqualified entry rejected");
                test.IsTrue(before==ps2native::nexo::EeSnapshotCodec::encode(candidate),"Rejected context preserved");
                test.IsTrue(bytes==ram,"Rejected memory preserved");
            };
            auto pending=context;pending.in_delay_slot=true;rejects(pending,fixture.base);
            auto unaligned=context;unaligned.pc+=2;rejects(unaligned,fixture.base);
            auto outside=context;outside.pc+=fixture.words.size_bytes();rejects(outside,fixture.base);
            rejects(context,PS2_RAM_SIZE);rejects(context,fixture.base+2);
            for(unsigned missing=0;missing<3;++missing)
            {
                auto candidate=context;bool rejected=false;
                const auto before=ps2native::nexo::EeSnapshotCodec::encode(candidate);
                try{runEeDataFamily(fixture.shape,missing==0 ? nullptr : ram.data(),missing==1 ? nullptr : &candidate,
                                   missing==2 ? nullptr : &runtime,fixture.base);}
                catch(const std::invalid_argument &){rejected=true;}
                test.IsTrue(rejected,"Missing invocation resources rejected");
                test.IsTrue(before==ps2native::nexo::EeSnapshotCodec::encode(candidate),"Rejected invocation preserves context");
            }
            for(size_t word=0;word<fixture.words.size();++word)
            {
                ram[fixture.base+word*4+3]^=0x40;rejects(context,fixture.base);ram[fixture.base+word*4+3]^=0x40;
            }
        });
    });
    return MiniTest::Run();
}

#include "MiniTest.h"
#include "ps2recomp/native_data_family.h"
#include "ps2recomp/native_overlay.h"

#include <array>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace
{
    bool rejects(std::span<const uint32_t> words, std::span<const uint32_t> masks)
    {
        try { ps2recomp::generateNativeDataFamily(words,masks); return false; }
        catch(const std::invalid_argument &) { return true; }
    }
}

int main()
{
    MiniTest::Case("Restricted EE data-family code generation",[](TestCase &suite)
    {
        suite.Run("typed_data_and_relative_pc",[](TestCase &test)
        {
            const std::array words{0x3c010172u,0xac22526cu,0x03e00008u,0u};
            const std::array masks{0xffff0000u,0xffff0000u,0xffffffffu,0xffffffffu};
            const auto code=ps2recomp::generateNativeDataFamily(words,masks);
            test.IsTrue(code.find("family_parameters[0]")!=std::string::npos,"LUI uses typed data");
            test.IsTrue(code.find("(int16_t)family_parameters[1]")!=std::string::npos,"SW sign extends its offset");
            test.IsTrue(code.find("switch (ctx->pc - family_base)")!=std::string::npos,"Resume selection uses relative PC");
            test.IsTrue(code.find("ADD32(family_base,")!=std::string::npos,"Precise PCs relocate");
            test.IsTrue(code.find("label_c:")!=std::string::npos,"Standalone terminal slot is emitted");
            test.IsTrue(code.find("ctx->in_delay_slot")!=std::string::npos,"Wrapper rejects pending slot context");
            test.IsTrue(code.find("family guard mismatch")!=std::string::npos,"Every invocation checks structural bytes");
        });
        suite.Run("masks_never_select_isa_operations",[](TestCase &test)
        {
            const std::array words{0x3c010172u,0xac22526cu,0x03e00008u,0u};
            for(const auto mask:{0u,0xffe00000u,0xfc000000u,0xfffffffeu})
            {
                const std::array masks{mask,0xffffffffu,0xffffffffu,0xffffffffu};
                test.IsTrue(rejects(words,masks),"Only complete low-16 data fields can vary");
            }
            const std::array badWords{0x3c210172u,0xac22526cu,0x03e00008u,0u};
            const std::array masks{0xffff0000u,0xffff0000u,0xffffffffu,0xffffffffu};
            test.IsTrue(rejects(badWords,masks),"Reserved LUI source field cannot be wildcarded");
        });
        suite.Run("relative_conditional_control_and_typed_operands",[](TestCase &test)
        {
            const std::array branch{0x144000d8u,0x2442ffffu};
            const std::array full{0xffffffffu,0xffffffffu};
            const auto conditional=ps2recomp::generateNativeDataFamily(branch,full);
            test.IsTrue(conditional.find("ctx->pc = ADD32(family_base, 0x364u);")!=std::string::npos,
                        "Taken external branch publishes a relocated PC");
            for(const uint32_t opcode:{0x09u,0x0au,0x0bu,0x0cu,0x0du,0x0eu,0x0fu,
                                      0x20u,0x21u,0x23u,0x24u,0x25u,0x27u,0x37u,0x1eu,
                                      0x28u,0x29u,0x2bu,0x3fu,0x1fu})
            {
                const uint32_t word=(opcode<<26)|((opcode==0x0fu?0u:5u)<<21)|(2u<<16)|0x8000u;
                const std::array words{word,0x03e00008u,0u};
                const std::array masks{0xffff0000u,0xffffffffu,0xffffffffu};
                const auto code=ps2recomp::generateNativeDataFamily(words,masks);
                test.IsTrue(code.find("family_parameters[0]")!=std::string::npos,
                            "Supported ordinary data field uses its live typed operand");
            }
        });
        suite.Run("unimplemented_control_is_explicitly_rejected",[](TestCase &test)
        {
            const std::array masks{0xffffffffu,0xffffffffu};
            for(const auto word:{0x0000000cu,0x45010001u,0x49010001u,0x18010001u})
            {
                const std::array words{word,0u};
                test.IsTrue(rejects(words,masks),"Unsupported PC-dependent control is rejected");
            }
            for(const auto word:{0x03e00048u,0x03e00808u,0x0090f809u,0x3c210001u})
            {
                const std::array words{word,0u};
                test.IsTrue(rejects(words,masks),"Unsupported reserved fields are not silently executed");
            }
            const std::array truncated{0x03e00008u};
            const std::array oneMask{0xffffffffu};
            test.IsTrue(rejects(truncated,oneMask),"Truncated architectural slot rejected");
            const std::array early{0x03e00008u,0u,0u};
            const std::array threeMasks{0xffffffffu,0xffffffffu,0xffffffffu};
            test.IsTrue(rejects(early,threeMasks),"Regions must end at their first register transfer");
            test.IsTrue(rejects({},{}),"Empty shape rejected");
            test.IsTrue(rejects(truncated,{}),"Word/mask count mismatch rejected");
        });
        suite.Run("direct_control_keeps_absolute_targets_and_relocated_links",[](TestCase &test)
        {
            const std::array masks{0xffffffffu,0xffffffffu};
            for(const uint32_t opcode:{2u,3u})
            for(const uint32_t encodedTarget:{0u,0x10000u})
            {
                const std::array words{(opcode<<26)|encodedTarget,0u};
                const auto code=ps2recomp::generateNativeDataFamily(words,masks);
                test.IsTrue(code.find("& 0xF0000000u")!=std::string::npos,
                            "J destination uses the architectural PC high nibble");
                test.IsTrue(code.find("dispatchGuestBranch")!=std::string::npos,
                            "Absolute destinations cannot become canonical local labels");
                test.IsTrue(code.find("- family_base) {")!=std::string::npos,
                            "Actual local destinations compare against the relocated base");
                if(opcode==3u)
                    test.IsTrue(code.find("SET_GPR_U32(ctx, 31, ADD32(family_base, 0x8u));")!=std::string::npos,
                                "JAL link relocates before its slot");
                const std::array variableControl{0xffff0000u,0xffffffffu};
                test.IsTrue(rejects(words,variableControl),"Encoded direct target remains fixed");
            }
        });
        suite.Run("legacy_generation_is_unchanged",[](TestCase &test)
        {
            const std::array words{0x3c010172u,0xac22526cu,0x03e00008u,0u};
            std::array<uint8_t,sizeof(words)> bytes{};std::memcpy(bytes.data(),words.data(),bytes.size());
            const auto before=ps2recomp::generateNativeOverlay(bytes,0x10000,0x10000);
            const std::array masks{0xffff0000u,0xffff0000u,0xffffffffu,0xffffffffu};
            (void)ps2recomp::generateNativeDataFamily(words,masks);
            const auto after=ps2recomp::generateNativeOverlay(bytes,0x10000,0x10000);
            test.Equals(after,before,"Family settings do not leak into fixed banks");
        });
    });
    return MiniTest::Run();
}

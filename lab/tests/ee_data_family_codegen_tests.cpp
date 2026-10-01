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
        suite.Run("unimplemented_control_is_explicitly_rejected",[](TestCase &test)
        {
            const std::array masks{0xffffffffu,0xffffffffu};
            for(const auto word:{0x1000ffffu,0x08010000u,0x0000000cu})
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

#include "ps2recomp/native_data_family.h"
#include "ps2recomp/code_generator.h"
#include "ps2recomp/instructions.h"
#include "ps2recomp/r5900_decoder.h"
#include "ps2recomp/types.h"
#include "ps2_native_data_operands.h"

#include <sstream>
#include <stdexcept>

namespace ps2recomp
{
    std::string generateNativeDataFamily(std::span<const uint32_t> words,
                                         std::span<const uint32_t> masks)
    {
        if (words.empty() || words.size() > 128 || words.size() != masks.size())
            throw std::invalid_argument("invalid bounded native data-family shape");
        const std::vector<Section> sections;
        CodeGenerator generator({},sections);
        generator.m_nativeDataFamily = true;
        generator.setEmitInstructionComments(false);
        R5900Decoder decoder;
        std::vector<Instruction> instructions;
        std::vector<uint32_t> resumes;
        bool transfer = false;
        for (size_t i = 0; i < words.size(); ++i)
        {
            const uint32_t pc = static_cast<uint32_t>(i * 4);
            const auto instruction = decoder.decodeInstruction(pc,words[i],false);
            if (instruction.opcode == OPCODE_LUI && instruction.rs != 0u)
                throw std::invalid_argument("unsupported LUI source field");
            if (masks[i] != 0xffffffffu)
            {
                if (masks[i] != 0xffff0000u ||
                    ps2native::nativeDataOperand(words[i]) == ps2native::DataOperand::None)
                    throw std::invalid_argument("family masks may vary only supported typed data fields");
                generator.m_nativeDataSlots.emplace(pc,generator.m_nativeDataSlots.size());
            }
            if (instruction.hasDelaySlot)
            {
                const auto opcode=instruction.opcode;
                const bool registerTransfer=opcode==OPCODE_SPECIAL &&
                    (instruction.function==SPECIAL_JR || instruction.function==SPECIAL_JALR);
                const bool directTransfer=opcode==OPCODE_J || opcode==OPCODE_JAL;
                const bool conditional=opcode==OPCODE_BEQ || opcode==OPCODE_BNE || opcode==OPCODE_BEQL ||
                    opcode==OPCODE_BNEL || opcode==OPCODE_BLEZ || opcode==OPCODE_BGTZ ||
                    opcode==OPCODE_BLEZL || opcode==OPCODE_BGTZL ||
                    (opcode==OPCODE_REGIMM && (instruction.rt==REGIMM_BLTZ || instruction.rt==REGIMM_BGEZ ||
                        instruction.rt==REGIMM_BLTZL || instruction.rt==REGIMM_BGEZL ||
                        instruction.rt==REGIMM_BLTZAL || instruction.rt==REGIMM_BGEZAL ||
                        instruction.rt==REGIMM_BLTZALL || instruction.rt==REGIMM_BGEZALL));
                if (transfer || i + 2 != words.size() || !(registerTransfer || directTransfer || conditional))
                    throw std::invalid_argument("unsupported native family control structure");
                const uint32_t reserved = instruction.function == SPECIAL_JR ? 0x001fffc0u : 0x001f07c0u;
                if (registerTransfer && (words[i] & reserved))
                    throw std::invalid_argument("unsupported register-transfer reserved fields");
                if ((opcode==OPCODE_BLEZ || opcode==OPCODE_BGTZ || opcode==OPCODE_BLEZL || opcode==OPCODE_BGTZL) && instruction.rt)
                    throw std::invalid_argument("unsupported conditional reserved fields");
                transfer = true;
            }
            else
            {
                const auto opcode = instruction.opcode;
                const bool dataOpcode = opcode == OPCODE_SPECIAL || opcode == OPCODE_ADDIU ||
                    opcode == OPCODE_DADDIU || opcode == OPCODE_LUI || opcode == OPCODE_ORI ||
                    opcode == OPCODE_ANDI || opcode == OPCODE_XORI || opcode == OPCODE_SLTI || opcode == OPCODE_SLTIU || opcode == OPCODE_LB ||
                    opcode == OPCODE_LBU || opcode == OPCODE_LH || opcode == OPCODE_LHU ||
                    opcode == OPCODE_LW || opcode == OPCODE_LWU || opcode == OPCODE_LD ||
                    opcode == OPCODE_LQ || opcode == OPCODE_SB || opcode == OPCODE_SH ||
                    opcode == OPCODE_SW || opcode == OPCODE_SD || opcode == OPCODE_SQ;
                if (!dataOpcode || (opcode == OPCODE_SPECIAL &&
                    (instruction.function == SPECIAL_SYSCALL || instruction.function == SPECIAL_BREAK)) ||
                    generator.translateInstruction(instruction).find("EXCEPTION_RESERVED_INSTRUCTION") != std::string::npos)
                    throw std::invalid_argument("unsupported native family data instruction");
            }
            instructions.push_back(instruction);
            resumes.push_back(pc);
        }
        Function function{};
        function.name = "ps2native_data_family_body";
        function.start = 0;
        function.end = static_cast<uint32_t>(words.size() * 4);
        generator.setRenamedFunctions({{0,function.name}});
        generator.setResumeEntryTargets({{0,resumes}});
        const auto analysis = generator.collectInternalBranchTargets(function,instructions);
        if (!analysis.jumpTableTargets.empty())
            throw std::invalid_argument("local indirect-target analysis needs a separate family contract");
        const auto body = generator.generateFunction(function,instructions,false);
        if (body.find("EXCEPTION_RESERVED_INSTRUCTION") != std::string::npos)
            throw std::invalid_argument("native family translation contains an unsupported operation");
        std::ostringstream output;
        output << "#include <array>\n#include <cstring>\n#include <stdexcept>\n"
                  "#include \"ps2_runtime_macros.h\"\n#include \"ps2_native_overlay_abi.h\"\n"
                  "#include \"ps2_syscalls.h\"\n#include \"ps2_stubs.h\"\n#undef PS2_FUNCTION_LOG_TRACKER\n";
        output << body;
        output << "void ps2native_data_family(uint8_t *rdram,R5900Context *ctx,PS2Runtime *runtime,uint32_t family_base) {\n"
               << "if(!rdram || !ctx || !runtime || (family_base&3u) || family_base>PS2_RAM_SIZE-"
               << function.end << "u || ctx->in_delay_slot || (ctx->pc&3u) || ctx->pc<family_base || ctx->pc-family_base>="
               << function.end << "u) throw std::invalid_argument(\"unsupported family entry context\");\n";
        output << "static constexpr std::array<uint32_t," << words.size() << "> expected{{";
        for (size_t i=0;i<words.size();++i) output << (words[i]&masks[i]) << "u,";
        output << "}};\nstatic constexpr std::array<uint32_t," << words.size() << "> masks{{";
        for (const auto mask:masks) output << mask << "u,";
        output << "}};\nstd::array<uint16_t," << generator.m_nativeDataSlots.size() << "> family_parameters{};\n"
                  "size_t parameter=0;\nfor(size_t i=0;i<expected.size();++i) {\n"
                  "uint32_t live;std::memcpy(&live,rdram+family_base+i*4,4);\n"
                  "if((live&masks[i])!=expected[i]) throw std::invalid_argument(\"family guard mismatch\");\n"
                  "if(masks[i]==0xffff0000u) family_parameters[parameter++]=static_cast<uint16_t>(live);\n}\n"
                  "ps2native_data_family_body(rdram,ctx,runtime,family_base,family_parameters.data());\n}\n";
        return output.str();
    }
}

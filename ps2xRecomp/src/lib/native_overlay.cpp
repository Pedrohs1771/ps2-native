#include "ps2recomp/native_overlay.h"
#include "ps2recomp/code_generator.h"
#include "ps2recomp/r5900_decoder.h"
#include "ps2recomp/instructions.h"
#include <cstring>
#include <deque>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

namespace ps2recomp
{
    std::string generateNativeOverlay(std::span<const uint8_t> bytes, uint32_t base, uint32_t entry)
    {
        constexpr uint32_t ramSize = 32u * 1024u * 1024u;
        if ((base & 3u) || (entry & 3u) || (bytes.size() & 3u) || bytes.empty() ||
            base >= ramSize || bytes.size() > ramSize - base || entry < base || entry - base >= bytes.size())
            throw std::invalid_argument("invalid EE overlay snapshot or entry");
        const uint32_t end = base + static_cast<uint32_t>(bytes.size());
        const auto inside = [=](uint32_t pc) { return !(pc & 3u) && pc >= base && pc < end; };
        const auto word = [&](uint32_t pc)
        {
            uint32_t value;
            std::memcpy(&value, bytes.data() + pc - base, 4);
            return value;
        };
        R5900Decoder decoder;
        const std::vector<Section> sections;
        CodeGenerator generator({}, sections);
        generator.setEmitInstructionComments(false);
        std::map<uint32_t, Function> blocks;
        std::map<uint32_t, std::string> emitted;
        std::set<uint32_t> visited;
        std::set<uint32_t> covered;
        std::deque<uint32_t> pending{entry};
        std::deque<uint32_t> prefetch;
        // Cover the complete window, including leaf functions reached only
        // through function pointers. Reachable blocks have priority; speculative
        // failures never suppress the requested root.
        for (uint32_t pc = base; pc < end; pc += 4)
            prefetch.push_back(pc);
        size_t instructionCount = 0;
        while ((!pending.empty() || !prefetch.empty()) && blocks.size() < 4096 && instructionCount < 16384)
        {
            if (pending.empty()) { pending.push_back(prefetch.front()); prefetch.pop_front(); }
            const uint32_t start = pending.front();
            pending.pop_front();
            if (!inside(start) || covered.contains(start) || !visited.insert(start).second) continue;
            Function function{};
            std::ostringstream name;
            name << "ps2native_block_" << std::hex << start;
            function.name = name.str();
            function.start = start;
            std::vector<Instruction> instructions;
            std::vector<uint32_t> successors;
            try
            {
                for (uint32_t pc = start; inside(pc) && instructions.size() < 128; pc += 4)
                {
                    if (pc != start && covered.contains(pc)) break;
                    Instruction inst = decoder.decodeInstruction(pc, word(pc), false);
                    if (!inst.hasDelaySlot && generator.translateInstruction(inst).find("EXCEPTION_RESERVED_INSTRUCTION") != std::string::npos)
                    {
                        if (instructions.empty()) throw std::runtime_error("unsupported overlay instruction");
                        break;
                    }
                    instructions.push_back(inst);
                    if (!inst.hasDelaySlot) continue;
                    if (!inside(pc + 4)) throw std::runtime_error("truncated overlay delay slot");
                    const Instruction slot = decoder.decodeInstruction(pc + 4, word(pc + 4), false);
                    if (slot.hasDelaySlot) throw std::runtime_error("branch in overlay delay slot");
                    instructions.push_back(slot);
                    if (inst.isBranch) successors.push_back(decoder.getBranchTarget(inst));
                    if (inst.opcode == OPCODE_J || inst.opcode == OPCODE_JAL)
                        successors.push_back(decoder.getJumpTarget(inst));
                    if (inst.isCall || inst.isBranch) successors.push_back(pc + 8);
                    break;
                }
                function.end = instructions.back().address + 4;
                if (!instructions.back().hasDelaySlot &&
                    (instructions.size() < 2 || !instructions[instructions.size() - 2].hasDelaySlot))
                    successors.push_back(function.end);
                std::vector<uint32_t> resumes;
                for (const Instruction &inst : instructions) resumes.push_back(inst.address);
                generator.setRenamedFunctions({{start, function.name}});
                generator.setResumeEntryTargets({{start, resumes}});
                const auto code = generator.generateFunction(function, instructions, false);
                // Unsupported instructions are fatal when actually reached;
                // don't publish speculative native stubs for them.
                if (code.find("EXCEPTION_RESERVED_INSTRUCTION") != std::string::npos)
                    throw std::runtime_error("unsupported overlay instruction");
                instructionCount += instructions.size();
                for (const auto &inst : instructions) covered.insert(inst.address);
                emitted.emplace(start, code);
                blocks.emplace(start, std::move(function));
                for (uint32_t next : successors) if (inside(next)) pending.push_back(next);
            }
            catch (const std::exception &)
            {
                if (start == entry) throw;
            }
        }
        if (!blocks.contains(entry)) throw std::runtime_error("overlay root was not compiled");
        // Keep an owner for every decoded instruction, then prefer exact block
        // starts on overlap. This also registers standalone delay-slot entries.
        std::map<uint32_t, uint32_t> owners;
        for (const auto &[start, block] : blocks)
            for (uint32_t pc = start; pc < block.end; pc += 4) owners.emplace(pc, start);
        for (const auto &[start, block] : blocks) owners[start] = start;
        std::ostringstream out;
        out << "#include <stdexcept>\n#include \"ps2_runtime_macros.h\"\n"
               "#include \"ps2_native_overlay_abi.h\"\n#include \"ps2_syscalls.h\"\n"
               "#include \"ps2_stubs.h\"\n#undef PS2_FUNCTION_LOG_TRACKER\n";
        for (const auto &[start, code] : emitted) out << code << '\n';
        out << "static const uint8_t snapshot[] = {";
        for (uint8_t byte : bytes) out << static_cast<unsigned>(byte) << ',';
        out << "};\nstatic const PS2NativeOverlayBinding bindings[] = {\n";
        for (const auto &[pc, owner] : owners)
        {
            const auto &block = blocks.at(owner);
            out << "{0x" << std::hex << pc << "u," << block.name << ",0x" << owner
                << "u,0x" << block.end - owner << "u,snapshot+0x" << owner - base << "u},\n";
        }
        out << std::dec << "};\nextern \"C\" const PS2NativeOverlayBinding *ps2xOverlayGetBindings(size_t *count,uint32_t *abi) {"
               "*count=sizeof(bindings)/sizeof(bindings[0]); *abi=PS2_NATIVE_OVERLAY_ABI; return bindings;}\n";
        return out.str();
    }
}

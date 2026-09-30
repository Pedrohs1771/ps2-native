#include "ps2recomp/Emitters/function_table_emitter.h"
#include "ps2recomp/code_generator.h"
#include "ps2recomp/ps2_recompiler.h"
#include "ps2recomp/recompiler_reporter.h"
#include "ps2recomp/types.h"

#include <algorithm>
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ps2recomp
{
    namespace
    {
        std::string escapeCppStringLiteral(const std::string &value)
        {
            std::string escaped;
            escaped.reserve(value.size());
            for (unsigned char ch : value)
            {
                switch (ch)
                {
                case '\\': escaped += "\\\\"; break;
                case '"': escaped += "\\\""; break;
                case '\n': escaped += "\\n"; break;
                case '\r': escaped += "\\r"; break;
                case '\t': escaped += "\\t"; break;
                default:
                    if (ch < 0x20u)
                    {
                        escaped += "\\";
                        escaped += static_cast<char>('0' + ((ch >> 6u) & 0x07u));
                        escaped += static_cast<char>('0' + ((ch >> 3u) & 0x07u));
                        escaped += static_cast<char>('0' + (ch & 0x07u));
                    }
                    else
                    {
                        escaped += static_cast<char>(ch);
                    }
                    break;
                }
            }
            return escaped;
        }
    }

    FunctionTableEmitter::FunctionTableEmitter(CodeGenerator &codeGenerator)
        : m_codeGenerator(codeGenerator)
    {
    }

    std::string FunctionTableEmitter::emit(const std::vector<Function> &functions, const std::map<uint32_t, std::string> &stubs)
    {
        (void)stubs;

        CodeGenerator &cg = m_codeGenerator;
        std::vector<std::pair<uint32_t, std::string>> entries;
        std::unordered_set<uint32_t> registeredAddresses;

        auto addEntry = [&](uint32_t address, const std::string &name)
        {
            if (name.empty())
            {
                return;
            }
            if ((address & 3u) != 0u)
            {
                std::ostringstream oss;
                oss << "Unaligned function table entry for " << name << " at 0x" << std::hex << address;

                if (cg.m_reporter)
                {
                    cg.m_reporter->errorAt("function-table", name, address, oss.str());
                }

                throw std::runtime_error(oss.str());
            }
            if (!registeredAddresses.insert(address).second)
            {
                return;
            }
            entries.emplace_back(address, name);
        };

        std::vector<std::pair<uint32_t, std::string>> normalFunctions;
        std::vector<std::pair<uint32_t, std::string>> stubFunctions;
        std::vector<std::pair<uint32_t, std::string>> systemCallFunctions;
        std::vector<std::pair<uint32_t, std::string>> libraryFunctions;

        for (const auto &function : functions)
        {
            if (!function.isRecompiled && !function.isStub && !function.isSkipped)
                continue;

            std::string generatedName = cg.getFunctionName(function.start);

            if (function.isSkipped)
            {
                libraryFunctions.emplace_back(function.start, generatedName);
            }
            else if (function.isStub)
            {
                const auto target = PS2Recompiler::resolveStubTarget(function.name);
                if (target == StubTarget::Syscall)
                {
                    systemCallFunctions.emplace_back(function.start, generatedName);
                }
                else
                {
                    stubFunctions.emplace_back(function.start, generatedName);
                }
            }
            else
            {
                normalFunctions.emplace_back(function.start, generatedName);
            }
        }

        if (cg.m_bootstrapInfo.valid)
        {
            std::string entryTarget = cg.getFunctionName(cg.m_bootstrapInfo.entry);
            if (entryTarget.empty() && cg.m_moduleSymbolPrefix.empty())
            {
                entryTarget = cg.m_bootstrapInfo.entryName;
            }
            if (entryTarget.empty())
            {
                throw std::runtime_error("No entry function name available for registration.");
            }
            addEntry(cg.m_bootstrapInfo.entry, entryTarget);
        }

        for (const auto &[address, name] : normalFunctions)
        {
            addEntry(address, name);
        }

        for (const auto &[ownerStart, targets] : cg.m_resumeEntryTargetsByOwner)
        {
            const std::string ownerName = cg.getFunctionName(ownerStart);
            if (ownerName.empty())
            {
                continue;
            }

            for (uint32_t target : targets)
            {
                addEntry(target, ownerName);
            }
        }

        for (const auto &[address, name] : stubFunctions)
        {
            addEntry(address, name);
        }
        for (const auto &[address, name] : systemCallFunctions)
        {
            addEntry(address, name);
        }
        for (const auto &[address, name] : libraryFunctions)
        {
            addEntry(address, name);
        }

        std::sort(entries.begin(), entries.end(), [](const auto &a, const auto &b)
                  { return a.first < b.first; });

        // Consecutive instruction entries share an owner. Emit loops for long
        // runs rather than hundreds of thousands of C++ initializer statements.
        struct EntryRun { size_t begin; size_t count; };
        std::vector<EntryRun> runs;
        for (size_t begin = 0u; begin < entries.size();)
        {
            size_t end = begin + 1u;
            while (end < entries.size() && entries[end].second == entries[begin].second &&
                   static_cast<uint64_t>(entries[end - 1u].first) + 4u == entries[end].first)
                ++end;
            runs.push_back({begin, end - begin});
            begin = end;
        }
        constexpr size_t rangeThreshold = 8u;

        std::stringstream moduleSource;
        if (!cg.m_moduleSymbolPrefix.empty())
        {
            if (cg.m_moduleKeys.empty())
            {
                throw std::runtime_error("Module-scoped output requires at least one module path key.");
            }

            moduleSource << "#include \"ps2_runtime.h\"\n";
            moduleSource << "#include \"ps2_recompiled_functions.h\"\n";
            moduleSource << "#include \"ps2_recompiled_stubs.h\"\n";
            moduleSource << "#include \"ps2_stubs.h\"\n";
            moduleSource << "#include \"ps2_syscalls.h\"\n";
            moduleSource << "#include <stdexcept>\n";
            moduleSource << "#include <vector>\n\n";
            moduleSource << "void " << cg.m_moduleSymbolPrefix << "register_compiled_module(PS2Runtime &runtime)\n{\n";
            moduleSource << "    std::vector<PS2Runtime::CompiledFunctionBinding> bindings;\n";
            moduleSource << "    bindings.reserve(" << std::dec << entries.size() << "u);\n";
            moduleSource << "    const auto bindRange = [&](uint32_t start, uint32_t count, PS2Runtime::RecompiledFunction function) {\n";
            moduleSource << "        for (uint32_t i = 0u; i < count; ++i) bindings.push_back({start + i * 4u, function});\n";
            moduleSource << "    };\n";
            for (const EntryRun &run : runs)
            {
                if (run.count >= rangeThreshold)
                {
                    const auto &[address, name] = entries[run.begin];
                    moduleSource << "    bindRange(0x" << std::hex << address << "u, "
                                 << std::dec << run.count << "u, &" << name << ");\n";
                }
                else for (size_t i = run.begin; i < run.begin + run.count; ++i)
                {
                    const auto &[address, name] = entries[i];
                    moduleSource << "    bindings.push_back({0x" << std::hex << address << "u, &" << name << "});\n";
                }
            }
            moduleSource << std::dec;
            for (const std::string &moduleKey : cg.m_moduleKeys)
            {
                moduleSource << "    if (!runtime.registerCompiledModuleFunctions(\""
                             << escapeCppStringLiteral(moduleKey)
                             << "\", bindings))\n    {\n"
                             << "        throw std::runtime_error(\"invalid generated module function table\");\n"
                             << "    }\n";
            }
            moduleSource << "}\n\n";
            if (!cg.m_moduleEmitDenseFunctionTable)
            {
                return moduleSource.str();
            }
        }

        uint32_t tableBase = 0u;
        uint32_t tableEnd = 0u;
        uint32_t slotCount = 0u;
        if (!entries.empty())
        {
            tableBase = entries.front().first & ~3u;
            tableEnd = (entries.back().first + 4u + 3u) & ~3u;
            slotCount = (tableEnd - tableBase) >> 2;
        }

        std::stringstream ss;
        ss << "#include \"ps2_runtime.h\"\n";
        ss << "#include <ps2_recompiled_functions.h>\n";
        ss << "#include \"ps2_stubs.h\"\n";
        ss << "#include <ps2_recompiled_stubs.h>\n";
        ss << "#include \"ps2_syscalls.h\"\n";
        ss << "#include <algorithm>\n\n";

        ss << "extern const uint32_t g_ps2RecompiledFunctionTableBase = 0x" << std::hex << tableBase << "u;\n";
        ss << "extern const uint32_t g_ps2RecompiledFunctionTableEnd = 0x" << std::hex << tableEnd << "u;\n";
        ss << "extern const uint32_t g_ps2RecompiledFunctionTableSlotCount = " << std::dec << slotCount << "u;\n";
        ss << "PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[" << std::dec << (slotCount == 0u ? 1u : slotCount) << "u] = {};\n\n";

        ss << "namespace {\n";
        ss << "struct GeneratedFunctionTableInitializer {\n";
        ss << "    GeneratedFunctionTableInitializer() {\n";
        for (const EntryRun &run : runs)
        {
            if (run.count >= rangeThreshold)
            {
                const auto &[address, name] = entries[run.begin];
                const uint32_t slot = (address - tableBase) >> 2;
                ss << "        std::fill_n(g_ps2RecompiledFunctionTable + " << std::dec << slot
                   << "u, " << run.count << "u, " << name << ");\n";
            }
            else for (size_t i = run.begin; i < run.begin + run.count; ++i)
            {
                const auto &[address, name] = entries[i];
                const uint32_t slot = (address - tableBase) >> 2;
                ss << "        g_ps2RecompiledFunctionTable[" << std::dec << slot << "] = " << name
                   << "; // 0x" << std::hex << address << std::dec << "\n";
            }
        }
        ss << "    }\n";
        ss << "};\n";
        ss << "static const GeneratedFunctionTableInitializer g_generatedFunctionTableInitializer;\n";
        ss << "}\n";

        return moduleSource.str() + ss.str();
    }
}

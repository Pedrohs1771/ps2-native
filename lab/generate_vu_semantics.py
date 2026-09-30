"""Generate instruction-specialized V0 operations from the identified model.

This is a laboratory bridge, not a proof-producing semantic frontend. Source
shape changes fail closed; runtime code never invokes this converter.
"""
import argparse
import hashlib
from pathlib import Path
import re


NON_CODE = re.compile(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'', re.S)
SPECIALIZED = {"calculateFmacExactResult", "calculateFmacProductSticky", "normalizeFmacResult",
               "applyFmacDest", "applyFmacDestAcc"}


def _balanced(source: str, opening: int) -> tuple[str, int]:
    depth = 0
    position = opening
    while position < len(source):
        non_code = NON_CODE.match(source, position)
        if non_code:
            position = non_code.end()
            continue
        if source[position] == "{":
            depth += 1
        elif source[position] == "}":
            depth -= 1
            if depth == 0:
                return source[opening + 1:position], position + 1
        position += 1
    raise ValueError("unterminated semantic source body")


def body_of(source: str, name: str) -> str:
    matches = list(re.finditer(r"\bVU1Interpreter::" + re.escape(name) + r"\s*\(", source))
    if len(matches) != 1:
        raise ValueError(f"expected one definition of {name}")
    opening = source.find("{", matches[0].end())
    if opening < 0:
        raise ValueError(f"missing body of {name}")
    return _balanced(source, opening)[0]


def _sources(root: Path):
    directory = root / "ps2xRuntime/src/lib/vu"
    sources = {name: (directory / f"ps2_vu1_{name}.cpp").read_text() for name in ("core", "upper", "lower")}
    methods = set(re.findall(r"\bVU1Interpreter::(\w+)\s*\(", "\n".join(sources.values())))
    fingerprint = hashlib.sha256("\n".join(sources[name] for name in sorted(sources)).encode()).hexdigest()
    return sources, methods, fingerprint


def _code_only(source: str, operation):
    result = []
    offset = 0
    for match in NON_CODE.finditer(source):
        result.append(operation(source[offset:match.start()]))
        result.append(match.group())
        offset = match.end()
    result.append(operation(source[offset:]))
    return "".join(result)


def _members(body: str, methods: set[str], specialize: bool) -> str:
    def convert(code):
        code = code.replace("[this]", "[&vu]")
        code = re.sub(r"\b(m_\w+)\b", r"vu.\1", code)
        if specialize:
            for name in sorted(SPECIALIZED):
                code = re.sub(r"(?<![\w.>])\b" + name + r"\s*\(", name + "<Instruction>(vu, ", code)
        ordinary = methods - SPECIALIZED - {"run", "execUpper", "execLower", "VU1Interpreter"}
        pattern = r"(?<![\w.>])\b(" + "|".join(sorted(ordinary)) + r")\s*\("
        return re.sub(pattern, r"vu.\1(", code)
    return _code_only(body, convert)


def _anonymous_helpers(source: str) -> str:
    match = re.search(r"\bnamespace\s*\n?\s*\{", source)
    if not match:
        raise ValueError("missing numeric helper namespace")
    body = _balanced(source, source.index("{", match.start()))[0]
    return re.sub(r"(?m)^(\s*)(float|int32_t|constexpr uint8_t)\s+(\w+)\s*\(", r"\1inline \2 \3(", body)


def generate_semantics(root: Path) -> str:
    sources, methods, fingerprint = _sources(root)
    detail = (root / "ps2xRuntime/src/lib/vu/ps2_vu1_detail.h").read_text()
    detail = detail[detail.index("// Instruction field"):detail.rindex("#endif")]
    detail = detail.replace("static inline", "static constexpr")
    helpers = "\n".join(_anonymous_helpers(sources[name]) for name in ("core", "upper", "lower"))
    output = [f"// Current-model source SHA-256: {fingerprint}\n#pragma once\n",
              '#include "nexo/vu_native.h"\n#include "runtime/gs/gs_frontend.h"\n'
              '#include "runtime/ps2_memory.h"\n#include <bit>\n#include <cmath>\n'
              '#include <cstring>\n#include <limits>\n',
              "namespace ps2native::nexo {\nnamespace native_detail {\n", detail, helpers,
              "\n}\n"]
    signatures = {
        "calculateFmacExactResult": ("bool", "uint32_t component, long double &result"),
        "calculateFmacProductSticky": ("uint32_t", "uint8_t dest"),
        "normalizeFmacResult": ("void", "float *result, uint8_t dest, uint8_t laneFlags[4]"),
        "applyFmacDest": ("void", "float *dst, float *result, uint8_t dest"),
        "applyFmacDestAcc": ("void", "float *result, uint8_t dest"),
    }
    for name, (result, arguments) in signatures.items():
        body = body_of(sources["core"], name)
        body = body.replace("const uint32_t upper = m_currentUpperInstruction;",
                            "constexpr uint32_t upper = Instruction;")
        body = _members(body, methods, True)
        output.append(f"template <uint32_t Instruction> {result} VuNativeAccess::{name}(VU1Interpreter &vu, {arguments})\n"
                      "{\nusing namespace native_detail;\n" + body + "\n}\n")
    upper = _members(body_of(sources["upper"], "execUpper"), methods, True)
    lower = _members(body_of(sources["lower"], "execLower").replace("(void)upperInstr;", ""), methods, True)
    output.append("template <uint32_t Instruction> void VuNativeAccess::upper(VU1Interpreter &vu)\n"
                  "{\nusing namespace native_detail;\nconstexpr uint32_t instr = Instruction;\n" + upper + "\n}\n")
    output.append("template <uint32_t Instruction> void VuNativeAccess::lower(VU1Interpreter &vu, "
                  "uint8_t *vuData, uint32_t dataSize, GS &gs, PS2Memory *memory)\n"
                  "{\nusing namespace native_detail;\nconstexpr uint32_t instr = Instruction;\n" + lower + "\n}\n}\n")
    text = "\n".join(output)
    for forbidden in ("vu.execUpper(", "vu.execLower(", "vu.calculateFmacExactResult(", "vu.calculateFmacProductSticky(",
                      "vu.normalizeFmacResult(", "vu.applyFmacDest(", "vu.applyFmacDestAcc("):
        if forbidden in text:
            raise ValueError("generic guest-operation helper remained: " + forbidden)
    return text


def generate_machine(root: Path) -> str:
    sources, methods, fingerprint = _sources(root)
    body = body_of(sources["core"], "run")
    old_lookup = "const DecodedInstructionPair decoded = getDecodedInstructionPairForPc(vuCode, codeSize, memory, m_state.pc);"
    if body.count(old_lookup) != 1:
        raise ValueError("native scheduler source lookup shape changed")
    body = body.replace(old_lookup,
        'const Entry &entry = program.entries[m_state.pc / 8u];\n'
        '        if (!entry.decoded || !entry.upper || !entry.lower) throw std::runtime_error("UNSEEN_CODE VU entry");\n'
        '        const DecodedPair &decoded = *entry.decoded;')
    body = body.replace("if (m_state.pc + 8u > codeSize)\n            break;",
        'if ((m_state.pc & 7u) != 0 || m_state.pc > codeSize - 8u)\n'
        '            throw std::runtime_error("UNSEEN_CODE VU address");')
    reserved = "reportReservedInstruction(decoded.upperUsage.reserved, decoded.upperUsage.reserved ? decoded.upper : decoded.lower);\n            break;"
    if reserved not in body:
        raise ValueError("native scheduler reserved-instruction guard shape changed")
    body = body.replace(reserved, 'throw std::runtime_error("UNSEEN_CODE unsupported VU instruction");')
    body = body.replace("execUpper(decoded.upper);", "entry.upper(vu);")
    body = body.replace("execLower(decoded.lower, vuData, dataSize, gs, memory, decoded.upper);",
                        "entry.lower(vu, vuData, dataSize, gs, memory);")
    body = re.sub(r"^    auto \*capture = .*?;\n", "", body, flags=re.M)
    body = re.sub(r"^.*if \(capture\) capture->(?:record|finish)\(.*?;\n", "", body, flags=re.M)
    rounding = "const int previousRoundingMode = std::fegetround();\n    const bool useVuRounding = std::fesetround(FE_TOWARDZERO) == 0;"
    body = body.replace(rounding,
        "struct RoundingGuard {\n"
        "        int previous = std::fegetround();\n"
        "        bool changed = std::fesetround(FE_TOWARDZERO) == 0;\n"
        "        ~RoundingGuard() { if (changed && previous != -1) std::fesetround(previous); }\n"
        "    } rounding;")
    body = body.replace("    if (useVuRounding && previousRoundingMode != -1)\n        std::fesetround(previousRoundingMode);", "")
    body = _members(body, methods, False)
    for forbidden in ("vuCode", "execUpper(", "execLower(", "getDecodedInstructionPair", "capture", "useVuRounding"):
        if forbidden in body:
            raise ValueError("non-native scheduler dependency remained: " + forbidden)
    prefix = f"// Current-model source SHA-256: {fingerprint}\n"
    prefix += '#include "nexo/vu_native.h"\n#include <bit>\n#include <cfenv>\n#include <cstring>\n#include <limits>\n#include <stdexcept>\n'
    prefix += "namespace ps2native::nexo {\nvoid VuNativeAccess::validateBinding(const VU1Interpreter &vu, "
    prefix += "const VuNativeProgram &program, const uint8_t *data, uint32_t dataSize, uint32_t maxCycles)\n{\n"
    prefix += '    const uint32_t expectedSize = program.unit == Unit::VU1 ? 16384u : 4096u;\n'
    prefix += '    if (vu.m_unit != program.unit || !data || dataSize != expectedSize ||\n'
    prefix += '        program.entries.size() != expectedSize / 8u || program.codeIdentity.size() != expectedSize)\n'
    prefix += '        throw std::invalid_argument("invalid native VU bank or memory binding");\n'
    prefix += '    if (maxCycles == 0 || maxCycles > 1048576 || vu.m_cycle > std::numeric_limits<uint64_t>::max() - maxCycles - 64u)\n'
    prefix += '        throw std::invalid_argument("invalid native VU execution budget");\n'
    prefix += '}\nvoid VuNativeAccess::resume(VU1Interpreter &vu, const VuNativeProgram &program, '
    prefix += 'uint8_t *vuData, uint32_t dataSize, GS &gs, PS2Memory *memory, uint32_t top, uint32_t itop, uint32_t maxCycles)\n{\n'
    prefix += '    validateBinding(vu, program, vuData, dataSize, maxCycles);\n'
    prefix += '    const uint32_t codeSize = program.unit == Unit::VU1 ? 16384u : 4096u;\n'
    prefix += '    vu.m_state.top = top; vu.m_state.itop = itop;\n'
    prefix += '    vu.m_state.stoppedByD = false; vu.m_state.stoppedByT = false;\n'
    fresh = body_of(sources["core"], "execute")
    boundary = "auto capture = ps2_vu_diagnostics::Capture::request"
    if fresh.count(boundary) != 1:
        raise ValueError("native fresh-call normalization source shape changed")
    fresh = _members(fresh.split(boundary)[0], methods, False)
    for forbidden in ("vuCode", "execUpper(", "execLower(", "vu.execute(", "vu.resume("):
        if forbidden in fresh:
            raise ValueError("non-native fresh-call dependency remained: " + forbidden)
    execute = '\nvoid VuNativeAccess::execute(VU1Interpreter &vu, const VuNativeProgram &program, '
    execute += 'uint8_t *vuData, uint32_t dataSize, GS &gs, PS2Memory *memory, uint32_t startPC, uint32_t top, uint32_t itop, uint32_t maxCycles)\n{\n'
    execute += '    validateBinding(vu, program, vuData, dataSize, maxCycles);\n' + fresh
    execute += '    resume(vu, program, vuData, dataSize, gs, memory, top, itop, maxCycles);\n}\n'
    return prefix + body + "\n}\n" + execute + "}\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--semantics", type=Path, required=True)
    parser.add_argument("--machine", type=Path, required=True)
    args = parser.parse_args()
    for path, content in [(args.semantics, generate_semantics(args.root)), (args.machine, generate_machine(args.root))]:
        path.parent.mkdir(parents=True, exist_ok=True)
        if not path.is_file() or path.read_text() != content:
            path.write_text(content)
        else:
            # Both generated outputs must become newer than changed inputs;
            # otherwise Make repeats this command for each dependent target.
            path.touch()


if __name__ == "__main__":
    main()

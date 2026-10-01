"""Offline instruction-specialized IOP V0 bridge from the identified model.

Absolute inputs are already relocated RAM. Family inputs include loader operand
masks and the complete IRX identity. This tool is never called by the runtime.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct


RAM_SIZE = 2 * 1024 * 1024
CPP_KEYWORDS = set('alignas alignof and and_eq asm auto bitand bitor bool break case catch char '
                   'char8_t char16_t char32_t class compl concept const consteval constexpr constinit '
                   'const_cast continue co_await co_return co_yield decltype default delete do double '
                   'dynamic_cast else enum explicit export extern false float for friend goto if '
                   'inline int long mutable namespace new noexcept not not_eq nullptr operator or '
                   'or_eq private protected public register reinterpret_cast requires return short '
                   'signed sizeof static static_assert static_cast struct switch template this '
                   'thread_local throw true try typedef typeid typename union unsigned using virtual '
                   'void volatile wchar_t while xor xor_eq'.split())


def generate_semantics(root: Path) -> str:
    source = (root / "ps2xIOP/src/emulator/core/iop_cpu_interpreter.cpp").read_text()
    signature = "bool IopCpuCore::executeInstruction(IopCpuState &cpu)"
    if source.count(signature) != 1:
        raise ValueError("unexpected identified IOP execute signature")
    prefix, body = source.split(signature)
    if not prefix.rstrip().endswith("{") or not body.rstrip().endswith("}\n}"):
        raise ValueError("unexpected IOP source envelope")
    body = body.rstrip()[:-1].rstrip()
    fetch = "const uint32_t instruction = m_memory.read32(pc);"
    if body.count(fetch) != 1:
        raise ValueError("unexpected IOP instruction fetch")
    body = body.replace(fetch, "constexpr uint32_t instruction = Instruction;")
    for field in ("opcode", "rs", "rt", "rd", "sa", "funct", "imm"):
        declaration = "const uint32_t " + field + " ="
        if body.count(declaration) != 1:
            raise ValueError("unexpected IOP instruction field: " + field)
        body = body.replace(declaration, "constexpr uint32_t " + field + " =")
    body = body.replace("const int32_t simm =", "constexpr int32_t simm =")
    body = body.replace("m_memory.", "ramMemory.")
    for helper in ("writeRegister", "scheduleLoad", "raiseException"):
        body = re.sub(r"\b" + helper + r"\(", "core." + helper + "(", body)
    for forbidden in ("executeInstruction(", "m_memory", "read32(pc)"):
        if forbidden in body:
            raise ValueError("generic guest execution remained: " + forbidden)
    sha = hashlib.sha256(source.encode()).hexdigest()
    immediate = 'constexpr uint32_t imm = instruction & 0xFFFFu;'
    jump = 'instruction & 0x03FFFFFFu'
    if body.count(immediate) != 1 or body.count(jump) != 2:
        raise ValueError('unexpected identified IOP relocation operand expressions')
    relocated_body = body.replace(immediate,
                                  'const uint32_t imm = Mask == 0xffffu ? boundInstruction & 0xffffu : Instruction & 0xffffu;')
    relocated_body = relocated_body.replace('constexpr int32_t simm =', 'const int32_t simm =')
    relocated_body = relocated_body.replace(jump, 'boundInstruction & 0x03FFFFFFu')
    relocated_body = relocated_body.replace('{', '{\n        static_assert(validRelocationOperand(Instruction, Mask));', 1)
    return ('#pragma once\n#include "emulator/core/iop_native.h"\n'
            '#include "emulator/core/iop_cpu.h"\n#include "emulator/core/iop_memory.h"\n'
            '#include <limits>\n// Identified model SHA256: ' + sha + '\n'
            'namespace ps2x::iop::detail\n{\n'
            'template <uint32_t Instruction>\n'
            'bool IopNativeAccess::instruction(IopCpuState &cpu, IopMemory &ramMemory, IopCpuCore &core)\n'
            + body + '\n'
            'template <uint32_t Instruction, uint32_t Mask>\n'
            'bool IopNativeAccess::instructionRelocated(IopCpuState &cpu, IopMemory &ramMemory, IopCpuCore &core, uint32_t boundInstruction)\n'
            + relocated_body + '\n}\n')


def validate(base: int, words: list[int]):
    if not isinstance(base, int) or isinstance(base, bool):
        raise ValueError("IOP bank base must be an integer")
    if base < 0 or base % 4 or not words or base >= RAM_SIZE or len(words) > (RAM_SIZE - base) // 4:
        raise ValueError("IOP bank must be a nonempty aligned RAM range")
    if any(not isinstance(word, int) or isinstance(word, bool) or word < 0 or word > 0xffffffff for word in words):
        raise ValueError("IOP instruction must be a uint32")


def validate_symbol(symbol: str):
    if (not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", symbol) or symbol.startswith("_") or
            symbol in CPP_KEYWORDS or symbol in {'entries', 'program', 'image', 'modules', 'IopNativeProgram',
                                               'IopNativeEntry', 'IopNativeAccess', 'IopNativeModule', 'IopNativeModuleEntry'}):
        raise ValueError("invalid native bank symbol")


def generate_bank(base: int, words: list[int], symbol: str) -> str:
    validate(base, words)
    validate_symbol(symbol)
    entries = [f'    {{{base + index * 4:#010x}u, {word:#010x}u, &IopNativeAccess::instruction<{word:#010x}u>}},'
               for index, word in enumerate(words)]
    identity = hashlib.sha256(b''.join(struct.pack('<I', word) for word in words)).hexdigest()
    return ('#include "iop_native_semantics.h"\n#include <array>\n'
            '// RAM bank SHA256: ' + identity + '\n'
            'using namespace ps2x::iop::detail;\nnamespace\n{\n'
            f'const std::array<IopNativeEntry, {len(words)}> entries = {{{{\n'
            + '\n'.join(entries) + '\n}};\nconst IopNativeProgram program{entries};\n}\n'
            f'const IopNativeProgram &{symbol}() {{ return program; }}\n')


def valid_operand(word: int, mask: int) -> bool:
    opcode = word >> 26
    if mask == 0x03ffffff:
        return opcode in (2, 3)
    if mask != 0xffff:
        return False
    return opcode == 1 or 4 <= opcode <= 0xf or 0x20 <= opcode <= 0x26 or 0x28 <= opcode <= 0x2b or opcode == 0x2e


def generate_family_bank(base: int, words: list[int], masks: list[int], image: bytes, symbol: str) -> str:
    validate(base, words)
    validate_symbol(symbol)
    if not image or len(image) > 64 * 1024 * 1024:
        raise ValueError('IOP family image outside 1..64 MiB')
    if len(masks) != len(words) or any(type(mask) is not int or mask not in (0, 0xffff, 0x03ffffff, 0xffffffff) for mask in masks):
        raise ValueError('IOP family relocation masks invalid')
    entries = []
    for index, (word, mask) in enumerate(zip(words, masks)):
        fixed = f'&IopNativeAccess::instruction<{word:#010x}u>' if mask == 0 else 'nullptr'
        operand = f'&IopNativeAccess::instructionRelocated<{word:#010x}u, {mask:#010x}u>' if valid_operand(word, mask) else 'nullptr'
        entries.append(f'    {{{index * 4:#010x}u, {word:#010x}u, {mask:#010x}u, {fixed}, {operand}}},')
    image_lines = [','.join(f'{byte:#04x}' for byte in image[offset:offset + 16]) + ',' for offset in range(0, len(image), 16)]
    identity = hashlib.sha256(image).hexdigest()
    return ('#include "iop_native_semantics.h"\n#include <array>\n'
            '// Complete IRX identity SHA256: ' + identity + '\n'
            'using namespace ps2x::iop::detail;\nnamespace\n{\n'
            f'const std::array<uint8_t, {len(image)}> image = {{{{\n' + '\n'.join(image_lines) + '\n}};\n'
            f'const std::array<IopNativeModuleEntry, {len(words)}> entries = {{{{\n'
            + '\n'.join(entries) + '\n}};\n'
            f'const std::array<IopNativeModule, 1> modules = {{{{{{image, {len(words) * 4}u, entries}}}}}};\n'
            'const IopNativeProgram program{{}, modules};\n}\n'
            f'const IopNativeProgram &{symbol}() {{ return program; }}\n')


def write_if_changed(path: Path, content: str):
    if not path.exists() or path.read_text() != content:
        path.write_text(content)


def load_module_case(path: Path, metadata=None) -> tuple[int, list[int]]:
    if metadata is None:
        metadata = json.loads((path / 'module.json').read_text())
    if (not isinstance(metadata, dict) or type(metadata.get('schema_version')) is not int or
            metadata['schema_version'] != 1 or metadata.get('relocations_complete') is not True):
        raise ValueError('loaded IOP module schema/relocations unsupported')
    size = metadata.get('size')
    entry = metadata.get('entry')
    base = metadata.get('base')
    if type(size) is not int or type(entry) is not int or type(base) is not int:
        raise ValueError('loaded IOP module dimensions must be integers')
    if size <= 0 or size > RAM_SIZE or size % 4 or entry % 4 or not base <= entry < base + size:
        raise ValueError('loaded IOP module dimensions/entry invalid')
    memory_path = path / 'relocated-ram.bin'
    if memory_path.stat().st_size != size:
        raise ValueError('loaded IOP RAM size differs from metadata')
    data = memory_path.read_bytes()
    if len(data) != size:
        raise ValueError('loaded IOP RAM changed while reading')
    words = list(struct.unpack('<' + 'I' * (size // 4), data))
    validate(base, words)
    return base, words


def load_family_case(path: Path) -> tuple[int, list[int], list[int], bytes]:
    metadata = json.loads((path / 'module.json').read_text())
    base, words = load_module_case(path, metadata)
    records = metadata.get('relocation_masks')
    if not isinstance(records, list):
        raise ValueError('IOP family requires relocation masks from inspector')
    masks = [0] * len(words)
    for record in records:
        if not isinstance(record, dict):
            raise ValueError('IOP relocation mask record invalid')
        offset, mask = record.get('offset'), record.get('mask')
        if type(offset) is not int or type(mask) is not int or offset < 0 or offset % 4 or offset // 4 >= len(words) or mask not in (0xffff, 0x03ffffff, 0xffffffff):
            raise ValueError('IOP relocation mask record invalid')
        masks[offset // 4] |= mask
    image_path = path / 'source-image.bin'
    size = image_path.stat().st_size
    if size <= 0 or size > 64 * 1024 * 1024 or type(metadata.get('image_bytes')) is not int or size != metadata['image_bytes']:
        raise ValueError('IOP family source image size invalid')
    image = image_path.read_bytes()
    if len(image) != size:
        raise ValueError('IOP family source image changed while reading')
    return base, words, masks, image


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument('--memory', type=Path, help='already relocated little-endian instruction bytes')
    source.add_argument('--words-json', type=Path, help='laboratory fixture with base and words')
    source.add_argument('--loaded-module', type=Path, help='offline inspector output; supplies its own base')
    source.add_argument('--family-module', type=Path, help='inspector output with source identity and relocation masks')
    parser.add_argument('--base', type=lambda value: int(value, 0), default=None)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--symbol', required=True)
    args = parser.parse_args()
    if args.memory:
        if args.base is None:
            parser.error('--memory requires --base')
        if args.memory.stat().st_size > RAM_SIZE:
            parser.error('IOP bank exceeds RAM size')
        data = args.memory.read_bytes()
        if len(data) % 4:
            parser.error('IOP bank bytes must be aligned to four bytes')
        base, words = args.base, list(struct.unpack('<' + 'I' * (len(data) // 4), data))
    elif args.loaded_module or args.family_module:
        if args.base is not None:
            parser.error('--base is provided by the module metadata')
        try:
            if args.family_module:
                base, words, masks, image = load_family_case(args.family_module)
            else:
                base, words = load_module_case(args.loaded_module)
        except (ValueError, OSError) as error:
            parser.error(str(error))
    else:
        if args.base is not None:
            parser.error('--base is provided by --words-json')
        fixture = json.loads(args.words_json.read_text())
        base = fixture['base']
        if not isinstance(base, int) or isinstance(base, bool):
            parser.error('fixture base must be an integer')
        words = [int(word, 0) if isinstance(word, str) else word for word in fixture['words']]
    try:
        bank = (generate_family_bank(base, words, masks, image, args.symbol) if args.family_module
                else generate_bank(base, words, args.symbol))
        semantics = generate_semantics(args.root)
    except ValueError as error:
        parser.error(str(error))
    args.output.mkdir(parents=True, exist_ok=True)
    write_if_changed(args.output / 'iop_native_semantics.h', semantics)
    write_if_changed(args.output / 'iop_native_bank.cpp', bank)


if __name__ == '__main__':
    main()

import importlib.util
from pathlib import Path
import tempfile
import json
import struct
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location('iop_codegen', ROOT / 'lab/generate_iop_bank.py')
CODEGEN = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CODEGEN)


class IopBankCodegenTests(unittest.TestCase):
    def test_specializes_every_word_and_keeps_interior_entries(self):
        text = CODEGEN.generate_bank(0x10000, [0x24020007, 0, 0], 'testBank')
        self.assertEqual(text.count('&IopNativeAccess::instruction<'), 3)
        self.assertIn('0x00010004u, 0x00000000u', text)
        self.assertIn('0x00010008u, 0x00000000u', text)
        self.assertIn('instruction<0x24020007u>', text)
        self.assertNotIn('executeInstruction(', text)

    def test_instruction_fields_are_compile_time_and_no_guest_fetch_remains(self):
        text = CODEGEN.generate_semantics(ROOT)
        for field in ('instruction', 'opcode', 'rs', 'rt', 'rd', 'sa', 'funct', 'imm'):
            self.assertIn('constexpr uint32_t ' + field + ' =', text)
        self.assertNotIn('read32(pc)', text)
        self.assertNotIn('executeInstruction(', text)
        self.assertNotIn('const uint32_t memory = memory.read32', text)

    def test_invalid_ranges_and_words_fail(self):
        for base, words in ((-4, [0]), (1, [0]), (0x200000, [0]), (0x1ffffc, [0, 0]),
                            (0, []), (0, [-1]), (0, [0x100000000]), (0, [True]), (False, [0])):
            with self.subTest(base=base, words=words):
                with self.assertRaises(ValueError):
                    CODEGEN.generate_bank(base, words, 'testBank')

    def test_invalid_symbol_fails_before_source_is_written(self):
        for symbol in ('bad();', 'foo::bar', '9bank', '_reserved', 'class', 'image', 'modules'):
            with self.assertRaises(ValueError):
                CODEGEN.generate_bank(0, [0], symbol)

    def test_changed_model_source_fails_closed(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            target = root / 'ps2xIOP/src/emulator/core/iop_cpu_interpreter.cpp'
            target.parent.mkdir(parents=True)
            original = (ROOT / target.relative_to(root)).read_text()
            target.write_text(original.replace('const uint32_t instruction = m_memory.read32(pc);',
                                               'const uint32_t instruction = 0;'))
            with self.assertRaises(ValueError): CODEGEN.generate_semantics(root)

    def test_unchanged_generation_preserves_mtime(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'bank.cpp'
            CODEGEN.write_if_changed(path, 'source\n')
            before = path.stat().st_mtime_ns
            CODEGEN.write_if_changed(path, 'source\n')
            self.assertEqual(before, path.stat().st_mtime_ns)

    def test_changed_operand_expressions_fail_closed(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            target = root / 'ps2xIOP/src/emulator/core/iop_cpu_interpreter.cpp'
            target.parent.mkdir(parents=True)
            original = (ROOT / target.relative_to(root)).read_text()
            for changed in (original.replace('const uint32_t imm = instruction & 0xFFFFu;',
                                             'const uint32_t imm = (instruction & 0xFFFFu);'),
                            original.replace('instruction & 0x03FFFFFFu', 'instruction & 0x03ffffffu')):
                target.write_text(changed)
                with self.assertRaises(ValueError): CODEGEN.generate_semantics(root)

    def test_loaded_module_supplies_base_without_manual_addresses(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            metadata = {'schema_version': 1, 'base': 0x10000, 'size': 8, 'entry': 0x10000,
                        'relocations_complete': True}
            (path / 'module.json').write_text(json.dumps(metadata))
            (path / 'relocated-ram.bin').write_bytes(struct.pack('<II', 0x24020000, 0))
            base, words = CODEGEN.load_module_case(path)
            self.assertEqual((base, words), (0x10000, [0x24020000, 0]))
            for field, value in (('size', 12), ('entry', 0x10009), ('entry', 0x10001),
                                 ('relocations_complete', False), ('schema_version', 2)):
                changed = dict(metadata); changed[field] = value
                (path / 'module.json').write_text(json.dumps(changed))
                with self.assertRaises(ValueError): CODEGEN.load_module_case(path)

    def test_family_keeps_operation_static_and_excludes_relocated_data(self):
        text = CODEGEN.generate_family_bank(0x10000,
            [0x3c020001, 0x2442001c, 0x08004005, 0x0001000c],
            [0xffff, 0xffff, 0x03ffffff, 0xffffffff], b'identified IRX', 'familyBank')
        self.assertIn('IopNativeModule', text)
        self.assertIn('instructionRelocated<0x3c020001u, 0x0000ffffu>', text)
        self.assertIn('instructionRelocated<0x08004005u, 0x03ffffffu>', text)
        self.assertNotIn('instruction<0x0001000cu>', text)
        self.assertNotIn('instructionRelocated<0x0001000cu', text)

    def test_family_rejects_masks_that_select_operations(self):
        for masks in ([0xfc000000], [1], [0x10000], [True], []):
            with self.assertRaises(ValueError):
                CODEGEN.generate_family_bank(0x10000, [0x24020001], masks, b'IRX', 'familyBank')

    def test_relocated_semantics_has_no_dynamic_operation_selection(self):
        text = CODEGEN.generate_semantics(ROOT)
        body = text.split('bool IopNativeAccess::instructionRelocated', 1)[1]
        self.assertIn('static_assert(validRelocationOperand(Instruction, Mask))', body)
        for field in ('instruction', 'opcode', 'rs', 'rt', 'rd', 'sa', 'funct'):
            self.assertIn('constexpr uint32_t ' + field + ' =', body)
        self.assertIn('const uint32_t imm =', body)
        self.assertNotIn('read32(pc)', body)

    def test_family_metadata_requires_bounded_source_and_masks(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            metadata = {'schema_version': 1, 'base': 0x10000, 'size': 8, 'entry': 0x10000,
                        'relocations_complete': True, 'image_bytes': 3,
                        'relocation_masks': [{'offset': 0, 'mask': 0xffff}]}
            (path / 'module.json').write_text(json.dumps(metadata))
            (path / 'relocated-ram.bin').write_bytes(struct.pack('<II', 0x3c020001, 0))
            (path / 'source-image.bin').write_bytes(b'IRX')
            self.assertEqual(CODEGEN.load_family_case(path), (0x10000, [0x3c020001, 0], [0xffff, 0], b'IRX'))
            for field, value in (('image_bytes', 4), ('relocation_masks', None),
                                 ('relocation_masks', [{'offset': 8, 'mask': 0xffff}]),
                                 ('relocation_masks', [{'offset': -4, 'mask': 0xffff}]),
                                 ('relocation_masks', [{'offset': 0, 'mask': 0xfc000000}]),
                                 ('relocation_masks', [{'offset': 0, 'mask': True}])):
                changed = dict(metadata); changed[field] = value
                (path / 'module.json').write_text(json.dumps(changed))
                with self.assertRaises(ValueError): CODEGEN.load_family_case(path)


if __name__ == '__main__': unittest.main()

import importlib.util
from pathlib import Path
import tempfile
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
        for symbol in ('bad();', 'foo::bar', '9bank', '_reserved', 'class'):
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


if __name__ == '__main__': unittest.main()

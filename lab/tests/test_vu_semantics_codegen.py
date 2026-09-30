import importlib.util
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("vu_semantics_codegen", ROOT / "lab/generate_vu_semantics.py")
CODEGEN = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CODEGEN)


class VuSemanticsCodegenTests(unittest.TestCase):
    def test_body_extraction_ignores_comment_and_string_braces(self):
        source = 'void VU1Interpreter::sample() { /* } */ auto s = "{"; if (s) { f(); } }'
        self.assertEqual(CODEGEN.body_of(source, "sample"), ' /* } */ auto s = "{"; if (s) { f(); } ')

    def test_missing_or_unterminated_method_fails_closed(self):
        for source in ("", "void VU1Interpreter::sample() { f();"):
            with self.assertRaises(ValueError):
                CODEGEN.body_of(source, "sample")

    def test_operations_are_specialized_by_compile_time_words(self):
        text = CODEGEN.generate_semantics(ROOT)
        self.assertIn("VuNativeAccess::upper(VU1Interpreter &vu)", text)
        self.assertIn("VuNativeAccess::lower(VU1Interpreter &vu", text)
        self.assertIn("constexpr uint32_t instr = Instruction;", text)
        for forbidden in ("vu.execUpper(", "vu.execLower(", "vu.applyFmacDest(", "vu.applyFmacDestAcc("):
            self.assertNotIn(forbidden, text)

    def test_numeric_flags_are_specialized_too(self):
        text = CODEGEN.generate_semantics(ROOT)
        self.assertEqual(text.count("constexpr uint32_t upper = Instruction;"), 2)
        self.assertIn("calculateFmacExactResult<Instruction>(vu,", text)
        self.assertIn("calculateFmacProductSticky<Instruction>(vu,", text)
        self.assertNotIn("const uint32_t upper = vu.m_currentUpperInstruction;", text)
        self.assertNotIn("vu.calculateFmacExactResult(", text)

    def test_scheduler_uses_native_entries_without_a_guest_code_buffer(self):
        text = CODEGEN.generate_machine(ROOT)
        self.assertIn("const DecodedPair &decoded = *entry.decoded;", text)
        self.assertIn("entry.upper(vu)", text)
        self.assertIn("entry.lower(vu, vuData, dataSize, gs, memory)", text)
        for forbidden in ("getDecodedInstructionPair", "vuCode", "execUpper(", "execLower("):
            self.assertNotIn(forbidden, text)

    def test_unknown_entry_has_no_generic_fallback(self):
        text = CODEGEN.generate_machine(ROOT)
        self.assertIn("UNSEEN_CODE", text)
        self.assertIn("vu.m_state.pc > codeSize - 8u", text)
        self.assertNotIn("vu.m_state.pc + 8u > codeSize", text)
        self.assertNotIn("vu.resume(", text)
        self.assertNotIn("vu.execute(", text)


if __name__ == "__main__":
    unittest.main()

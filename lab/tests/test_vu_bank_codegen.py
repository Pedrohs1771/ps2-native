import copy
import importlib.util
from pathlib import Path
import struct
import unittest


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("vu_bank_codegen", ROOT / "lab/generate_vu_bank.py")
CODEGEN = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CODEGEN)


def fixture():
    usage = {"vfRead": [[0, 0], [0, 0]], "vfWrite": [0, 0], "vfReadCount": 0,
             "viRead": 0, "viWrite": 0, "accRead": 0, "accWrite": 0, "latency": 0,
             "vfLatency": 0, "viLatency": 0, "pipeline": 0, "waitQ": 0, "waitP": 0,
             "readsClip": 0, "writesClip": 0, "delaysNextBranchRead": 0, "reserved": 0}
    pair = {"lower": 0x8000033C, "upper": 0x2FF, "lowerUsage": usage, "upperUsage": usage,
            "iBit": 0, "eBit": 0, "mBit": 0, "dBit": 0, "tBit": 0,
            "upperVfShadowReg": 0, "suppressedLowerVf": 0}
    code = struct.pack("<II", pair["lower"], pair["upper"]) * 2048
    metadata = {"version": 1, "unit": 1, "code_size": len(code), "pairs": [copy.deepcopy(pair) for _ in range(2048)]}
    return code, metadata


class VuBankCodegenTests(unittest.TestCase):
    def test_emits_static_native_operations_and_complete_code_identity(self):
        code, metadata = fixture()
        text = CODEGEN.emit_bank(code, metadata)
        self.assertIn("VuNativeAccess::upper<0x000002ffu>", text)
        self.assertIn("VuNativeAccess::lower<0x8000033cu>", text)
        self.assertIn("const VuNativeProgram &compiledVuProgram()", text)
        self.assertIn("codeIdentity", text)
        self.assertNotIn("inspectMicrocode", text)
        self.assertNotIn("execUpper(", text)
        self.assertNotIn("execLower(", text)

    def test_deduplicates_identical_descriptors_but_keeps_all_entries(self):
        code, metadata = fixture()
        text = CODEGEN.emit_bank(code, metadata)
        self.assertEqual(text.count("DecodedPair descriptor"), 1)
        self.assertEqual(text.count("{&descriptor0,"), 2048)

    def test_code_and_metadata_must_have_the_same_words(self):
        code, metadata = fixture()
        metadata["pairs"][0]["upper"] = 0
        with self.assertRaises(ValueError): CODEGEN.emit_bank(code, metadata)

    def test_invalid_sizes_widths_fields_and_symbol_fail_before_compilation(self):
        code, metadata = fixture()
        bad = []
        item = copy.deepcopy(metadata); item["pairs"].pop(); bad.append(item)
        item = copy.deepcopy(metadata); item["pairs"][0]["upperUsage"]["vfRead"][0][0] = 32; bad.append(item)
        item = copy.deepcopy(metadata); item["pairs"][0]["upperUsage"]["latency"] = "injected"; bad.append(item)
        item = copy.deepcopy(metadata); item["pairs"][0]["surprise"] = 1; bad.append(item)
        for value in bad:
            with self.assertRaises(ValueError): CODEGEN.emit_bank(code, value)
        with self.assertRaises(ValueError): CODEGEN.emit_bank(code[:-1], metadata)
        with self.assertRaises(ValueError): CODEGEN.emit_bank(code, metadata, "bad(); injection")

    def test_uncompiled_reserved_entries_have_no_fallback(self):
        code, metadata = fixture()
        metadata["pairs"][0]["upperUsage"]["reserved"] = 1
        text = CODEGEN.emit_bank(code, metadata)
        self.assertIn("{nullptr, nullptr, nullptr}", text)

    def test_loi_literal_does_not_instantiate_a_lower_guest_operation(self):
        code, metadata = fixture()
        metadata["pairs"][0]["iBit"] = 1
        metadata["pairs"][0]["upper"] |= 0x80000000
        metadata["pairs"][0]["lower"] = 0x7FC12345
        code = struct.pack("<II", 0x7FC12345, metadata["pairs"][0]["upper"]) + code[8:]
        text = CODEGEN.emit_bank(code, metadata)
        self.assertNotIn("lower<0x7fc12345u>", text)


if __name__ == "__main__": unittest.main()

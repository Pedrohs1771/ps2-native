import importlib.util
import json
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock
from test_vu_bank_codegen import fixture

ROOT=Path(__file__).resolve().parents[2]
SPEC=importlib.util.spec_from_file_location("vif_codegen",ROOT/"lab/generate_vif_banks.py")
GEN=importlib.util.module_from_spec(SPEC); SPEC.loader.exec_module(GEN)

class VifBankCollectionTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory(); self.root=Path(self.tmp.name)
        self.case=self.root/"case"; self.case.mkdir(); self.output=self.root/"out"; self.cache=self.root/"cache"
        self.inspector=self.root/"inspector"; self.inspector.write_bytes(b"identified synthetic inspector")
        self.code,self.meta=fixture(); (self.case/"bank-0.bin").write_bytes(self.code)
        (self.case/".complete").write_bytes(b"\x01")
        self.manifest={"schema":"nexo.observed.vif.call.v1","executed_banks":1,"vu_calls":1,"callback_horizon":65536}
        self.write_manifest()
    def tearDown(self): self.tmp.cleanup()
    def write_manifest(self): (self.case/"capture.json").write_text(json.dumps(self.manifest))
    def generate(self): return GEN.generate(self.case,self.inspector,self.output,self.cache)
    def response(self): return SimpleNamespace(stdout=json.dumps(self.meta))
    def test_collection_emits_static_registry_and_no_runtime_decoder(self):
        with mock.patch("subprocess.run",return_value=self.response()) as run:
            result=self.generate(); self.assertEqual(run.call_count,1)
        sources=[Path(p).read_text() for p in result["sources"]]
        self.assertIn("compiledVifBanks",sources[-1]); self.assertIn("VuNativeAccess::upper<",sources[0])
        self.assertNotIn("inspectMicrocode",''.join(sources)); self.assertFalse(result["closure_beyond_observed_case"])
    def test_unchanged_generation_uses_cache_and_preserves_source_timestamps(self):
        with mock.patch("subprocess.run",return_value=self.response()) as run:
            first=self.generate(); stamps=[Path(p).stat().st_mtime_ns for p in first["sources"]]
            second=self.generate(); self.assertEqual(run.call_count,1)
        self.assertEqual(first["sources"],second["sources"])
        self.assertEqual(stamps,[Path(p).stat().st_mtime_ns for p in second["sources"]])
        self.assertEqual(second["inspector_runs"],0)
    def test_inspector_identity_change_invalidates_metadata_cache(self):
        with mock.patch("subprocess.run",return_value=self.response()) as run:
            self.generate(); self.inspector.write_bytes(b"changed synthetic inspector"); self.generate()
            self.assertEqual(run.call_count,2)
    def test_incomplete_cases_and_invalid_counts_fail_before_inspection(self):
        with mock.patch("subprocess.run") as run:
            (self.case/".complete").unlink()
            with self.assertRaises(ValueError): self.generate()
            (self.case/".complete").write_bytes(b"\x01"); self.manifest["executed_banks"]=257; self.write_manifest()
            with self.assertRaises(ValueError): self.generate()
            self.assertEqual(run.call_count,0)
    def test_duplicate_bank_identities_are_rejected(self):
        self.manifest.update(executed_banks=2,vu_calls=2); self.write_manifest()
        (self.case/"bank-1.bin").write_bytes(self.code)
        with mock.patch("subprocess.run") as run:
            with self.assertRaises(ValueError): self.generate()
            self.assertEqual(run.call_count,0)
    def test_corrupt_metadata_cache_is_rebuilt_automatically(self):
        with mock.patch("subprocess.run",return_value=self.response()) as run:
            self.generate()
            for p in self.cache.glob("*.metadata.json"): p.write_text('{"corrupt":true}')
            result=self.generate(); self.assertEqual(run.call_count,2)
            self.assertEqual(result["inspector_runs"],1)

if __name__=="__main__": unittest.main()

import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
GENERATOR = Path(sys.argv.pop(1)).resolve() if len(sys.argv) > 1 else ROOT / 'build/ps2xRecomp/ps2_native_overlay'


class EeMissBatchTests(unittest.TestCase):
    def setUp(self):
        spec = importlib.util.spec_from_file_location('ee_batch', ROOT / 'lab/admit_ee_misses.py')
        self.batch = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.batch)
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)

    def tearDown(self):
        self.temporary.cleanup()

    def case(self, name, immediate):
        case = self.root / name; case.mkdir()
        image = struct.pack('<III', 0x24020000 | immediate, 0x03E00008, 0x24420001)
        (case / 'snapshot.bin').write_bytes(image)
        (case / 'bank.json').write_text(json.dumps({
            'schema_version': 1, 'base': 0x10000, 'entry': 0x10000, 'image_bytes': len(image),
            'image_sha256': hashlib.sha256(image).hexdigest(),
            'bindings': [{'address': pc, 'source_begin': 0x10000, 'source_bytes': 12}
                         for pc in range(0x10000, 0x1000C, 4)]}))
        return case

    def capture(self, name, immediate):
        case = self.case(name, immediate)
        image = (case / 'snapshot.bin').read_bytes()
        ram = bytearray(32 * 1024 * 1024); ram[0x10000:0x10000 + len(image)] = image
        (case / 'ee-ram.bin').write_bytes(ram)
        (case / 'request.json').write_text(json.dumps({
            'schema_version': 1, 'processor': 'EE', 'ee_model_profile': 1,
            'window_base': 0x10000, 'target_pc': 0x10000, 'window_bytes': len(image),
            'runtime_admission': 'missing', 'complete': True, 'ram_captured': True,
            'context_captured': False, 'module_owns_address': False,
            'complete_machine_checkpoint': False, 'quiescence_qualified': False,
            'overlay_lookup_status': 'MissingEntry'}))
        return case

    def catalog(self):
        case = self.case('original', 42)
        output = self.root / 'catalog'
        self.batch.catalog_tool.generate([case], GENERATOR, output)
        return output

    def test_batch_prepares_distinct_misses_and_reuses_owned_cases(self):
        catalog = self.catalog()
        first = json.loads((catalog / 'catalog.json').read_text())
        source = catalog / first['sources'][0]
        before = (source.read_bytes(), source.stat().st_mtime_ns)
        report = self.batch.admit([self.capture('seven', 7), self.capture('nine', 9)],
                                  GENERATOR, catalog, self.root / 'job')
        self.assertEqual(report['status'], 'EXTENDED_LABORATORY')
        self.assertEqual(report['new_banks'], 2)
        self.assertEqual(report['total_banks'], 3)
        self.assertFalse(report['strict_approval'])
        self.assertFalse(report['closure_proved'])
        self.assertEqual((source.read_bytes(), source.stat().st_mtime_ns), before)
        self.assertEqual(len(self.batch.catalog_tool.owned_cases(catalog)), 3)

    def test_duplicate_records_do_not_create_duplicate_banks(self):
        catalog = self.catalog()
        capture = self.capture('same', 42)
        report = self.batch.admit([capture, capture], GENERATOR, catalog, self.root / 'job')
        self.assertEqual(report['status'], 'NO_NEW_BANKS_LABORATORY')
        self.assertEqual(report['new_banks'], 0)
        self.assertEqual(report['duplicate_records'], 2)
        self.assertEqual(report['total_banks'], 1)

    def test_any_invalid_capture_rejects_batch_without_catalog_mutation(self):
        catalog = self.catalog()
        before = {str(f.relative_to(catalog)): (f.read_bytes(), f.stat().st_mtime_ns)
                  for f in catalog.rglob('*') if f.is_file()}
        valid, invalid = self.capture('valid', 7), self.capture('invalid', 9)
        (invalid / 'snapshot.bin').write_bytes(b'wrong')
        with self.assertRaises(ValueError):
            self.batch.admit([valid, invalid], GENERATOR, catalog, self.root / 'job')
        self.assertEqual({str(f.relative_to(catalog)): (f.read_bytes(), f.stat().st_mtime_ns)
                          for f in catalog.rglob('*') if f.is_file()}, before)
        report = json.loads((self.root / 'job/report.json').read_text())
        self.assertEqual(report['status'], 'FAILED')
        self.assertFalse(report['strict_approval'])
        self.assertEqual(report['new_banks'], 0)
        self.assertEqual(report['prepared_new_banks'], 1)
        self.assertFalse(report['manifest_published'])

    def test_bootstrap_cannot_smuggle_additional_unobserved_cases(self):
        catalog = self.catalog()
        old = self.root / 'original'
        extra = self.case('unobserved', 99)
        before = (catalog / 'catalog.json').read_bytes()
        with self.assertRaises(ValueError):
            self.batch.admit([self.capture('seven', 7)], GENERATOR, catalog,
                              self.root / 'job', initial_cases=[old, extra])
        self.assertEqual((catalog / 'catalog.json').read_bytes(), before)

    def test_legacy_catalog_can_bootstrap_inputs_once(self):
        case = self.case('legacy', 42);catalog = self.root / 'catalog'
        self.batch.catalog_tool.generate([case], GENERATOR, catalog)
        manifest = json.loads((catalog / 'catalog.json').read_text())
        manifest.pop('case_inputs')
        (catalog / 'catalog.json').write_text(json.dumps(manifest))
        # A genuine older catalog never had the owned copies.
        import shutil
        shutil.rmtree(catalog / 'ee_cases')
        report = self.batch.admit([self.capture('seven', 7)], GENERATOR, catalog,
                                  self.root / 'job', initial_cases=[case])
        self.assertEqual(report['new_banks'], 1)
        self.assertEqual(len(self.batch.catalog_tool.owned_cases(catalog)), 2)

    def test_budget_output_and_nested_catalog_conflicts_fail(self):
        catalog = self.catalog()
        capture = self.capture('seven', 7)
        for captures, output in [([], self.root / 'empty'), ([capture] * 17, self.root / 'excess'),
                                  ([capture], catalog / 'nested'), ([capture], catalog)]:
            with self.assertRaises(ValueError): self.batch.admit(captures, GENERATOR, catalog, output)
        output = self.root / 'existing'; output.mkdir()
        with self.assertRaises(ValueError): self.batch.admit([capture], GENERATOR, catalog, output)
        link = self.root / 'dangling-output'; target = self.root / 'unrequested-target'
        link.symlink_to(target)
        with self.assertRaises(ValueError): self.batch.admit([capture], GENERATOR, catalog, link)
        self.assertFalse(target.exists())

    def test_cli_emits_structured_terminal_result(self):
        catalog = self.catalog()
        capture = self.capture('same', 42)
        result = subprocess.run([sys.executable, str(ROOT / 'lab/admit_ee_misses.py'),
                                 '--catalog', str(catalog), '--capture', str(capture),
                                 '--generator', str(GENERATOR), '--output', str(self.root / 'job')],
                                capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stderr)
        report = json.loads(result.stdout)
        self.assertEqual(report['status'], 'NO_NEW_BANKS_LABORATORY')
        self.assertEqual(report['new_banks'], 0)
        self.assertEqual(report['total_banks'], 1)


if __name__ == '__main__':
    unittest.main()

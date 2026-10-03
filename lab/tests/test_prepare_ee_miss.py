import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
import zlib

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location('prepare_ee_miss', ROOT / 'lab/prepare_ee_miss.py')
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)
CATALOG_SPEC = importlib.util.spec_from_file_location('ee_catalog', ROOT / 'lab/generate_ee_bank_catalog.py')
CATALOG = importlib.util.module_from_spec(CATALOG_SPEC)
CATALOG_SPEC.loader.exec_module(CATALOG)
GENERATOR = Path(sys.argv.pop(1)).resolve() if len(sys.argv) > 1 else ROOT / 'build/ps2xRecomp/ps2_native_overlay'


class PrepareEeMissTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.capture = self.root / 'capture'
        self.capture.mkdir()
        self.image = struct.pack('<III', 0x2402002A, 0x03E00008, 0x24420001)
        ram = bytearray(32 * 1024 * 1024)
        ram[0x10000:0x1000C] = self.image
        (self.capture / 'ee-ram.bin').write_bytes(ram)
        (self.capture / 'snapshot.bin').write_bytes(self.image)
        self.request = {'schema_version': 1, 'processor': 'EE', 'runtime_admission': 'missing',
                        'target_pc': 0x10000, 'window_base': 0x10000, 'window_bytes': 12,
                        'overlay_lookup_status': 'CodeChanged', 'module_owns_address': False,
                        'ram_captured': True, 'context_captured': True, 'ee_model_profile': 1,
                        'complete': True, 'quiescence_qualified': False,
                        'complete_machine_checkpoint': False, 'candidates': []}
        self.write_request()
        cpu = bytearray(b'NEXOEE\0\0' + struct.pack('<IIII', 1, 1, 1619, 0) + bytes(1619))
        struct.pack_into('<I', cpu, 24 + 512, 0x10000)
        struct.pack_into('<I', cpu, 20, zlib.crc32(cpu[:20] + cpu[24:]))
        (self.capture / 'ee-context.bin').write_bytes(cpu)

    def tearDown(self):
        self.temporary.cleanup()

    def write_request(self):
        (self.capture / 'request.json').write_text(json.dumps(self.request))

    def prepare(self):
        return MODULE.prepare(self.capture, GENERATOR, self.root / 'case')

    def test_exact_snapshot_becomes_a_case_without_loading_a_dso(self):
        metadata = self.prepare()
        self.assertEqual(metadata['base'], 0x10000)
        self.assertEqual(metadata['entry'], 0x10000)
        self.assertEqual(metadata['image_sha256'], hashlib.sha256(self.image).hexdigest())
        self.assertEqual(len(metadata['bindings']), 3)
        self.assertEqual((self.root / 'case/snapshot.bin').read_bytes(), self.image)
        self.assertFalse(metadata['complete_machine_checkpoint'])
        self.assertEqual(metadata['origin'], 'observed-ee-miss')
        manifest = CATALOG.generate([self.root / 'case'], GENERATOR, self.root / 'catalog')
        self.assertEqual(manifest['generator_sha256'], metadata['generator_sha256'])
        self.assertEqual(manifest['banks'][0]['image_sha256'], metadata['image_sha256'])

    def test_kernel_alias_case_preserves_virtual_pc_and_checks_physical_snapshot(self):
        alias = 0x80010000
        self.request.update(target_pc=alias, window_base=alias, overlay_lookup_status='MissingEntry')
        self.write_request()
        cpu = bytearray((self.capture / 'ee-context.bin').read_bytes())
        struct.pack_into('<I', cpu, 24 + 512, alias)
        struct.pack_into('<I', cpu, 20, zlib.crc32(cpu[:20] + cpu[24:]))
        (self.capture / 'ee-context.bin').write_bytes(cpu)
        metadata = self.prepare()
        self.assertEqual(metadata['entry'], alias)
        self.assertEqual(metadata['base'], alias)
        self.assertEqual((self.root / 'case/snapshot.bin').read_bytes(), self.image)
        CATALOG.generate([self.root / 'case'], GENERATOR, self.root / 'catalog')

    def test_non_ram_aliases_and_alias_windows_crossing_ram_are_refused(self):
        for target, base, size in ((0xC0010000,0xC0010000,12),
                                   (0x82010000,0x82010000,12),
                                   (0x9FC00000,0x9FC00000,12),
                                   (0xA1FFFFFC,0xA1FFFFFC,12)):
            self.request.update(target_pc=target, window_base=base, window_bytes=size)
            self.write_request()
            with self.subTest(target=target), self.assertRaises(ValueError): self.prepare()

    def test_pc_delay_state_and_noncanonical_boolean_are_rejected(self):
        original = (self.capture / 'ee-context.bin').read_bytes()
        for offset, value in ((24 + 512, 0x10004), (24 + 1350, 1), (24 + 1350, 2)):
            cpu = bytearray(original)
            if offset == 24 + 512:
                struct.pack_into('<I', cpu, offset, value)
            else:
                cpu[offset] = value
            struct.pack_into('<I', cpu, 20, zlib.crc32(cpu[:20] + cpu[24:]))
            (self.capture / 'ee-context.bin').write_bytes(cpu)
            with self.subTest(offset=offset, value=value), self.assertRaises(ValueError): self.prepare()
            self.assertFalse((self.root / 'case').exists())

    def test_changed_ram_snapshot_or_corrupt_context_is_rejected(self):
        (self.capture / 'snapshot.bin').write_bytes(b'\0' * 12)
        with self.assertRaises(ValueError): self.prepare()
        self.assertFalse((self.root / 'case').exists())
        (self.capture / 'snapshot.bin').write_bytes(self.image)
        cpu = bytearray((self.capture / 'ee-context.bin').read_bytes());cpu[100] ^= 1
        (self.capture / 'ee-context.bin').write_bytes(cpu)
        with self.assertRaises(ValueError): self.prepare()
        self.assertFalse((self.root / 'case').exists())

    def test_module_shadowing_and_already_matching_code_do_not_trigger_generation(self):
        for patch in ({'overlay_lookup_status': 'Ready'}, {'module_owns_address': True},
                      {'ram_captured': False}, {'complete': False}, {'target_pc': 0x10002},
                      {'schema_version': '1'}):
            original = self.request.copy();self.request.update(patch);self.write_request()
            with self.subTest(patch=patch), self.assertRaises(ValueError): self.prepare()
            self.assertFalse((self.root / 'case').exists())
            self.request = original

    def test_unbound_entry_in_a_loaded_module_has_explicit_recovery_evidence(self):
        self.request.update(module_owns_address=True, entry_binding_missing=True,
                            module_key='cdrom0:\\unseen.elf;1', overlay_lookup_status='MissingEntry')
        self.write_request()
        metadata = self.prepare()
        self.assertEqual(metadata['entry'], 0x10000)
        self.assertEqual(metadata['module_key'], 'cdrom0:\\unseen.elf;1')
        self.assertTrue(metadata['module_owns_address'])

    def test_incomplete_context_remains_explicit_and_existing_case_is_preserved(self):
        self.request['context_captured'] = False;self.write_request()
        (self.capture / 'ee-context.bin').unlink()
        metadata = self.prepare()
        self.assertFalse(metadata['context_captured'])
        self.assertIsNone(metadata['cpu_snapshot_sha256'])
        before = (self.root / 'case/bank.json').read_bytes()
        with self.assertRaises(ValueError): self.prepare()
        self.assertEqual((self.root / 'case/bank.json').read_bytes(), before)


if __name__ == '__main__':
    unittest.main()

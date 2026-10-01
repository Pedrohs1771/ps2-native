import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
import zlib

FIXTURE = Path(sys.argv.pop(1)).resolve()


class EeMissCaptureTests(unittest.TestCase):
    def run_fixture(self, mode, root):
        run = subprocess.run([str(FIXTURE), mode, str(root)], capture_output=True, text=True, timeout=10)
        self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
        self.assertTrue(json.loads(run.stdout)['guest_bytes_unchanged'])
        self.assertTrue(json.loads(run.stdout)['guest_context_unchanged'])
        return run

    def test_changed_dependency_preserves_exact_ram_cpu_and_expected_bytes(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary) / 'capture'
            self.run_fixture('changed', root)
            event = next(root.glob('ee-miss-*'))
            request = json.loads((event / 'request.json').read_text())
            self.assertEqual(request['overlay_lookup_status'], 'CodeChanged')
            self.assertTrue(request['context_captured'])
            self.assertFalse(request['quiescence_qualified'])
            self.assertEqual(request['operation'], 'fixture"\\\n')
            ram = (event / 'ee-ram.bin').read_bytes()
            self.assertEqual(len(ram), 32 * 1024 * 1024)
            snapshot = (event / 'snapshot.bin').read_bytes()
            self.assertEqual(snapshot, ram[request['window_base']:request['window_base'] + request['window_bytes']])
            cpu = (event / 'ee-context.bin').read_bytes()
            self.assertEqual(cpu[:8], b'NEXOEE\0\0')
            self.assertEqual(len(cpu), 1643)
            self.assertEqual(struct.unpack_from('<IIII', cpu, 24 + 4 * 16), (1, 2, 3, 4))
            self.assertEqual(struct.unpack_from('<I', cpu, 20)[0], zlib.crc32(cpu[:20] + cpu[24:]))
            candidate = request['candidates'][0]
            self.assertEqual(candidate['source_begin'], 0x10000)
            self.assertEqual(candidate['mismatch_bytes'], 1)
            self.assertEqual(candidate['first_mismatch_offset'], 0)
            self.assertEqual((event / candidate['expected_file']).read_bytes(),
                             bytes([42,0,2,36,8,0,224,3,1,0,66,36]))

    def test_missing_entry_and_module_shadowing_have_distinct_evidence(self):
        with tempfile.TemporaryDirectory() as temporary:
            for mode, status in [('missing', 'MissingEntry'), ('shadowed', 'Ready')]:
                root = Path(temporary) / mode
                self.run_fixture(mode, root)
                request = json.loads(next(root.glob('ee-miss-*/request.json')).read_text())
                self.assertEqual(request['overlay_lookup_status'], status)
                self.assertEqual(request['module_owns_address'], mode == 'shadowed')
                self.assertEqual(request['module_key'], 'cdrom0:\\module.elf;1')

    def test_disabled_capture_null_ram_and_missing_context_are_explicit(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.run_fixture('disabled', root / 'disabled')
            self.assertFalse((root / 'disabled').exists())
            self.run_fixture('no-ram', root / 'no-ram')
            request = json.loads(next((root / 'no-ram').glob('ee-miss-*/request.json')).read_text())
            self.assertFalse(request['ram_captured'])
            self.assertEqual(request['overlay_lookup_status'], 'NoRam')
            self.run_fixture('no-context', root / 'no-context')
            event = next((root / 'no-context').glob('ee-miss-*'))
            request = json.loads((event / 'request.json').read_text())
            self.assertFalse(request['context_captured'])
            self.assertFalse((event / 'ee-context.bin').exists())

    def test_event_budget_does_not_overwrite_existing_records(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary) / 'capture'
            run = self.run_fixture('saturate', root)
            self.assertEqual(len(list(root.glob('ee-miss-*/request.json'))), 16)
            self.assertIn('limit', run.stderr)
            event = root / 'ee-miss-000001'
            before = hashlib.sha256((event / 'request.json').read_bytes()).hexdigest()
            run = self.run_fixture('changed', root)
            self.assertEqual(hashlib.sha256((event / 'request.json').read_bytes()).hexdigest(), before)
            self.assertIn('exists', run.stderr)

    def test_write_failure_does_not_escape_into_guest_execution(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary) / 'file'
            root.write_bytes(b'preserve')
            run = self.run_fixture('changed', root)
            self.assertTrue(run.stderr)
            self.assertEqual(root.read_bytes(), b'preserve')


if __name__ == '__main__':
    unittest.main()

import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

PROBE = Path(sys.argv.pop(1)).resolve()
FIXTURE = Path(sys.argv.pop(1)).resolve()


def image():
    data = bytearray(0x108)
    data[:7] = b'\x7fELF\x01\x01\x01'
    struct.pack_into('<HHIIIIIHHHHHH', data, 16, 2, 8, 1, 0x10000,
                     52, 0, 0, 52, 32, 1, 0, 0, 0)
    struct.pack_into('<IIIIIIII', data, 52, 1, 0x100, 0x10000,
                     0x10000, 8, 8, 7, 4)
    struct.pack_into('<II', data, 0x100, 0x03e00008, 0)
    return bytes(data)


class IopCaptureCliTests(unittest.TestCase):
    def test_existing_event_is_not_overwritten(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary) / 'collision'
            run = subprocess.run([str(FIXTURE), str(root), 'collision'],
                                 capture_output=True, text=True, timeout=15)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            self.assertEqual((root / '00000001-module/image.irx').read_bytes(), b'preserve')

    def test_rpc_limit_preserves_module_and_fault_capture(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary) / 'saturated'
            run = subprocess.run([str(FIXTURE), str(root), 'saturate'],
                                 capture_output=True, text=True, timeout=30)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            self.assertEqual(len(list(root.glob('*-rpc'))), 4096)
            self.assertEqual(len(list(root.glob('*-module'))), 1)
            self.assertEqual(len(list(root.glob('*-fault'))), 1)

    def test_payloads_policies_bounds_and_hle_route_are_observed(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary) / 'fixture'
            run = subprocess.run([str(FIXTURE), str(root)], capture_output=True,
                                 text=True, timeout=15)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            events = sorted((root / 'events').iterdir())
            self.assertEqual(len(events), 10)
            module = events[0]
            self.assertEqual((module / 'image.irx').read_bytes(), bytes([0, 1, 128, 255]))
            self.assertEqual((module / 'arguments.bin').read_bytes(), b'a\0b\xff\0')
            self.assertEqual(json.loads((module / 'request.json').read_text())['path'],
                             'buffer"\\\n\xff')
            request = json.loads((events[1] / 'request.json').read_text())
            self.assertEqual(request['call_token'], 0x100000002)
            for field, value in {'client_address': 0x10, 'server_address': 0x20,
                                 'server_function': 0x30, 'server_buffer': 0x40,
                                 'end_function': 0x50, 'end_parameter': 0x60}.items():
                self.assertEqual(request[field], value)
            self.assertEqual((events[1] / 'send.bin').read_bytes(), b'\xcc' * 8)
            self.assertEqual((events[1] / 'receive.bin').read_bytes(), b'\x5a' * 16)
            result = json.loads((events[1] / 'result.json').read_text())
            self.assertEqual(result['guest_arguments'], [1, 2, 3, 4])
            self.assertEqual(result['native_instructions'], 7)
            for field in ('handled', 'receive_captured', 'signal_completion', 'signal_nowait_completion'):
                self.assertTrue(result[field])
            for field in ('callback_policy', 'server_dispatch_policy'):
                self.assertEqual(result[field], 1)
            for event in events[2:4]:
                self.assertFalse(json.loads((event / 'request.json').read_text())['send_captured'])
                self.assertFalse(json.loads((event / 'result.json').read_text())['receive_captured'])
                self.assertFalse((event / 'send.bin').exists())
                self.assertFalse((event / 'receive.bin').exists())
            self.assertEqual((events[4] / 'send.bin').read_bytes(), b'')
            self.assertEqual((events[4] / 'receive.bin').read_bytes(), b'')
            self.assertEqual((events[5] / 'iop-ram.bin').read_bytes(), bytes([0, 1, 128, 255]))
            self.assertTrue(json.loads((events[6] / 'result.json').read_text())['handled'])
            self.assertEqual((events[6] / 'receive.bin').read_bytes(),
                             struct.pack('<III', 0, 0x0205, 0x0206) + b'\x5a' * 4)
            self.assertFalse(json.loads((events[7] / 'result.json').read_text())['handled'])
            self.assertEqual(json.loads((events[8] / 'request.json').read_text())['path'], 'buffer@0x1000')
            self.assertEqual(len((events[9] / 'iop-ram.bin').read_bytes()), 2 * 1024 * 1024)
            self.assertIn('[IOP:UNSEEN_CODE]', json.loads((events[9] / 'request.json').read_text())['diagnostic'])
            self.assertEqual((root / 'not-a-directory').read_text(), 'preserve')

    def test_actual_runtime_records_loader_inputs_without_changing_results(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / 'module.irx'
            source.write_bytes(image())
            env = os.environ.copy()
            env.pop('DISPLAY', None)
            env.pop('WAYLAND_DISPLAY', None)
            env.pop('PS2X_IOP_CAPTURE_DIR', None)
            plain = subprocess.run([str(PROBE), str(source), str(root / 'plain')],
                                   env=env, capture_output=True, timeout=15)
            env['PS2X_IOP_CAPTURE_DIR'] = str(root / 'capture')
            recorded = subprocess.run([str(PROBE), str(source), str(root / 'recorded')],
                                      env=env, capture_output=True, timeout=15)
            self.assertEqual(recorded.returncode, plain.returncode)
            self.assertEqual((root / 'plain/report.json').read_bytes(),
                             (root / 'recorded/report.json').read_bytes())
            for ram in ('iop-ram.bin', 'ee-ram.bin'):
                self.assertEqual((root / 'plain' / ram).read_bytes(),
                                 (root / 'recorded' / ram).read_bytes())
            modules = sorted((root / 'capture').glob('*-module'))
            self.assertGreaterEqual(len(modules), 1)
            for module in modules:
                request = json.loads((module / 'request.json').read_text())
                self.assertEqual(request['path'], 'host:module.irx')
                self.assertEqual(request['image_bytes'], len(image()))
                self.assertEqual(request['argument_bytes'], 0)
                self.assertEqual((module / 'image.irx').read_bytes(), image())
                self.assertEqual((module / 'arguments.bin').read_bytes(), b'')
            report = json.loads((root / 'recorded/report.json').read_text())
            faults = list((root / 'capture').glob('*-fault'))
            self.assertEqual(len(faults), int(report['native_faults'] > 0))
            for fault in faults:
                self.assertEqual(len((fault / 'iop-ram.bin').read_bytes()), 2 * 1024 * 1024)
                self.assertIn('[IOP:UNSEEN_CODE]', json.loads(
                    (fault / 'request.json').read_text())['diagnostic'])


if __name__ == '__main__':
    unittest.main()

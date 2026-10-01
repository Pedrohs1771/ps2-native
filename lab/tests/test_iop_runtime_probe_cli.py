import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

PROBE = Path(sys.argv.pop(1)).resolve()


class IopRuntimeProbeCliTests(unittest.TestCase):
    def run_probe(self, *arguments):
        env = os.environ.copy()
        env.pop('DISPLAY', None)
        env.pop('WAYLAND_DISPLAY', None)
        env.update(SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
        return subprocess.run([str(PROBE), *map(str, arguments)], env=env,
                              capture_output=True, text=True, timeout=10)

    def test_usage_and_module_count_are_bounded(self):
        for arguments in ([], ['--sequence'], ['--sequence', 'out', '0'],
                          ['--sequence', 'out', '0', *(['input'] * 33)]):
            with self.subTest(arguments=arguments):
                result = self.run_probe(*arguments)
                self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                self.assertIn('Usage:', result.stderr)

    def test_invalid_cycle_budgets_do_not_create_outputs(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / 'output'
            for budget in ('-1', '16000001', '3x', '', '18446744073709551616'):
                with self.subTest(budget=budget):
                    result = self.run_probe('--sequence', output, budget, 'missing-input')
                    self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
                    self.assertFalse(output.exists())

    def test_sequence_requires_one_host_directory(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            sources = []
            for name in ('a', 'b'):
                folder = root / name
                folder.mkdir()
                source = folder / 'module.irx'
                source.write_bytes(b'bounded dummy input')
                sources.append(source)
            output = root / 'output'
            result = self.run_probe('--sequence', output, '0', *sources)
            self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
            self.assertIn('must share a host directory', result.stderr)
            self.assertFalse(output.exists())

    def test_existing_output_is_preserved(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            sentinel = output / 'sentinel'
            sentinel.write_bytes(b'preserve me')
            result = self.run_probe('--sequence', output, '0', 'missing-input')
            self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
            self.assertIn('already exists', result.stderr)
            self.assertEqual(sentinel.read_bytes(), b'preserve me')


if __name__ == '__main__':
    unittest.main()

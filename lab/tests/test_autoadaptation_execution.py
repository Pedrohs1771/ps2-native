"""Actual EE capture, generation, native compile, offline relink and replay."""
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
from types import SimpleNamespace
import unittest

from tools.ps2native.autoadapt import DesktopAdaptation
from tools.ps2native.pipeline import PipelineError, _run, sha256_file

ROOT = Path(__file__).resolve().parents[2]
BUILD = ROOT/'build'
FIXTURE = BUILD/'lab/nexo_ee_autoadapt_fixture'
GENERATOR = BUILD/'ps2xRecomp/ps2_native_overlay'


class NativeFixtureAdaptation(DesktopAdaptation):
    def probe(self, directory):
        env = os.environ.copy()
        env.pop('PS2X_NATIVE_OVERLAY_DRIVER', None)
        command = [str(self.manifest['desktop']['runner']), str(directory/'captures')]
        if getattr(self, 'entry_mode', None):
            command.append(self.entry_mode)
        process = subprocess.run(command,
                                 env=env, capture_output=True, text=True, timeout=15)
        (directory/'runner.log').write_text(process.stdout+process.stderr)
        captures = list((directory/'captures').glob('ee-miss-*/request.json'))
        if process.returncode == 73 and len(captures) == 1:
            return {'kind': 'code_miss', 'capture': str(captures[0].parent)}
        if process.returncode != 0 or captures:
            raise PipelineError('synthetic runner failed: '+process.stdout+process.stderr)
        self.native_result = json.loads(next(line for line in process.stdout.splitlines()
                                            if line.startswith('{')))['native_result']
        return {'kind': 'no_code_miss', 'native_result': self.native_result}

    def rebuild(self, recovered, directory):
        catalog = Path(recovered['catalog_manifest']).parent
        metadata = json.loads((catalog/'catalog.json').read_text())
        objects = []
        for name in metadata['sources']:
            output = directory/(name+'.o')
            _run(['/usr/bin/c++', '-std=c++20', '-O0', '-fno-lto', '-msse4.1',
                  '-I'+str(ROOT/'ps2xRuntime/include'), '-I'+str(ROOT/'ps2xIOP/include'),
                  '-I'+str(ROOT/'ps2xRuntime/src/lib'), '-I'+str(ROOT/'ps2xRuntime/src/lib/Kernel'),
                  '-c', str(catalog/name), '-o', str(output)],
                 directory/(name+'.log'), ROOT, 90)
            objects.append(str(output))
        cwd = BUILD/'lab'
        argv = shlex.split((cwd/'CMakeFiles/nexo_ee_autoadapt_fixture.dir/link.txt').read_text())
        bootstrap = [i for i, value in enumerate(argv) if value.endswith('ps2_empty_ee_catalog.cpp.o')]
        if len(bootstrap) != 1:
            raise PipelineError('fixture baseline must use the empty AOT bootstrap')
        argv[bootstrap[0]:bootstrap[0]+1] = objects
        runner = directory/'native-fixture'
        argv[argv.index('-o')+1] = str(runner)
        argv = ['-Wl,--dependency-file='+str(directory/'link.d')
                if value.startswith('-Wl,--dependency-file=') else value for value in argv]
        _run(argv, directory/'link.log', cwd, 90)
        self.manifest['desktop']['runner'] = str(runner)
        self.remember()
        return {'runner_sha256': sha256_file(runner), 'compiled_banks': len(metadata['banks'])}

    def initial_relink(self):
        if self.cases:
            directory = self.workspace/'memory-relink'
            directory.mkdir()
            self.rebuild({'catalog_manifest': str(self.catalog/'catalog.json')}, directory)


@unittest.skipUnless(FIXTURE.is_file() and GENERATOR.is_file(), 'build the native automation fixture first')
class AutomaticNativeExecutionTests(unittest.TestCase):
    def test_unbound_thread_entries_are_recovered_offline_and_reused(self):
        self._exercise_two_conversions('thread')

    def test_direct_mapped_kernel_thread_entries_keep_virtual_pc_and_physical_code(self):
        for mode in ('thread-kseg0', 'thread-kseg1'):
            with self.subTest(mode=mode):
                self._exercise_two_conversions(mode)

    def test_lookup_miss_emits_one_recoverable_capture_with_guest_context(self):
        with tempfile.TemporaryDirectory() as temporary:
            capture_root = Path(temporary)
            process = subprocess.run([str(FIXTURE), str(capture_root), 'lookup'],
                                     capture_output=True, text=True, timeout=15)
            self.assertEqual(process.returncode, 73, process.stderr)
            captures = list(capture_root.glob('ee-miss-*/request.json'))
            self.assertEqual(len(captures), 1, process.stderr)
            request = json.loads(captures[0].read_text())
            self.assertTrue(request['context_captured'])
            self.assertTrue(request['entry_binding_missing'])

    def test_two_unseen_mips_functions_are_recovered_and_second_conversion_reuses_memory(self):
        self._exercise_two_conversions(None)

    def _exercise_two_conversions(self, entry_mode):
        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            iso = base/'unseen.iso'; iso.write_bytes(b'owned synthetic fixture identity')
            args = SimpleNamespace(memory_root=base/'memory', overlay_generator=str(GENERATOR),
                                   cmake=None, adapt_rounds=4, probe_timeout=5, timeout=120, build_jobs=4)
            receipts = []
            for number in range(2):
                workspace = base/f'conversion-{number}';workspace.mkdir()
                manifest = {'source': {'iso_sha256': sha256_file(iso)},
                            'build_inputs': {'source_tree': {'sha256': 'synthetic-fixture-v1'}},
                            'desktop': {'runner': str(FIXTURE)}}
                with NativeFixtureAdaptation(ROOT, workspace, workspace/'package', iso, args, manifest) as adapter:
                    adapter.entry_mode = entry_mode
                    adapter.initial_relink()
                    receipt = adapter.run()
                    self.assertEqual(adapter.native_result, 123)
                    self.assertEqual(receipt['status'], 'replayed_unverified')
                    self.assertFalse(receipt['menu_approved'])
                    self.assertFalse(receipt['native_execution_qualified'])
                    self.assertFalse(receipt['live_overlay_compiler_enabled'])
                    receipts.append(receipt)
            self.assertEqual(receipts[0]['recovery_rounds'], 2)
            self.assertEqual(receipts[1]['recovery_rounds'], 0)
            self.assertEqual(receipts[1]['reused_cases'], 2)


if __name__ == '__main__':
    unittest.main()

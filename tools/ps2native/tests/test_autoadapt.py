import hashlib
import json
from pathlib import Path
import tempfile
import unittest

from tools.ps2native.pipeline import PipelineError


class AdaptationLoopTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.calls = []

    def probe(self, directory):
        self.calls.append('probe')
        if self.calls.count('probe') == 3:
            return {'kind': 'no_code_miss', 'menu_approved': False}
        capture = directory / 'ee-miss-000001'
        capture.mkdir()
        (capture / 'request.json').write_text(json.dumps({
            'target_pc': self.calls.count('probe') * 0x10000, 'window_base': 0x10000}))
        (capture / 'snapshot.bin').write_bytes(b'identified observed bytes')
        return {'kind': 'code_miss', 'capture': str(capture)}

    def recover(self, capture, directory):
        self.calls.append('recover')
        return {'catalog_manifest': str(directory / 'catalog.json'),
                'catalog_sha256': hashlib.sha256(b'catalog').hexdigest()}

    def rebuild(self, recovered, directory):
        self.calls.append('relink')
        return {'runner_sha256': hashlib.sha256(b'native runner').hexdigest()}

    def run_loop(self, **kwargs):
        from tools.ps2native.autoadapt import run_adaptation
        return run_adaptation(self.root / 'adaptation', probe=self.probe,
                              recover=self.recover, rebuild=self.rebuild, max_rounds=2, **kwargs)

    def test_two_misses_are_recovered_compiled_relinked_and_replayed_without_pc_arguments(self):
        receipt = self.run_loop()
        self.assertEqual(self.calls, ['probe', 'recover', 'relink', 'probe', 'recover', 'relink', 'probe'])
        self.assertEqual(receipt['status'], 'replayed_unverified')
        self.assertEqual(receipt['recovery_rounds'], 2)
        self.assertFalse(receipt['menu_approved'])
        self.assertFalse(receipt['native_execution_qualified'])
        self.assertFalse(receipt['closure_proved'])

    def test_budget_exhaustion_keeps_a_resumable_failure_and_never_claims_a_menu(self):
        from tools.ps2native.autoadapt import run_adaptation
        with self.assertRaisesRegex(PipelineError, 'budget'):
            run_adaptation(self.root / 'adaptation', probe=self.probe, recover=self.recover,
                           rebuild=self.rebuild, max_rounds=1)
        receipt = json.loads((self.root/'adaptation/adaptation.json').read_text())
        self.assertEqual(receipt['status'], 'budget_exhausted')
        self.assertEqual(receipt['recovery_rounds'], 1)
        self.assertEqual(self.calls, ['probe', 'recover', 'relink', 'probe'])
        self.assertFalse(receipt['menu_approved'])

    def test_repeated_bytes_and_target_stop_instead_of_compiling_forever(self):
        original_probe = self.probe
        first = None
        def same_capture(directory):
            nonlocal first
            result = original_probe(directory)
            if first is None:
                first = result
            return first
        self.probe = same_capture
        with self.assertRaisesRegex(PipelineError, 'progress'):
            self.run_loop()
        self.assertEqual(self.calls.count('recover'), 1)
        receipt = json.loads((self.root/'adaptation/adaptation.json').read_text())
        self.assertEqual(receipt['status'], 'no_progress')

    def test_device_or_probe_failure_never_triggers_code_generation(self):
        self.probe = lambda _: {'kind': 'failure', 'reason': 'unhandled IOP service'}
        with self.assertRaisesRegex(PipelineError, 'IOP'):
            self.run_loop()
        self.assertEqual(self.calls, [])
        receipt = json.loads((self.root/'adaptation/adaptation.json').read_text())
        self.assertEqual(receipt['status'], 'failed')

    def test_failed_compilation_prevents_replay_and_preserves_previous_records(self):
        def fail(*_):
            raise PipelineError('compiler failed')
        self.rebuild = fail
        with self.assertRaisesRegex(PipelineError, 'compiler'):
            self.run_loop()
        self.assertEqual(self.calls, ['probe', 'recover'])
        receipt = json.loads((self.root/'adaptation/adaptation.json').read_text())
        self.assertEqual(receipt['status'], 'failed')
        self.assertEqual(receipt['steps'][-1]['stage'], 'relink')

    def test_invalid_budget_does_not_start_a_probe(self):
        from tools.ps2native.autoadapt import run_adaptation
        for budget in [True, 0, 65]:
            with self.subTest(budget=budget), self.assertRaises(ValueError):
                run_adaptation(self.root/'adaptation', probe=self.probe, recover=self.recover,
                               rebuild=self.rebuild, max_rounds=budget)
        self.assertEqual(self.calls, [])

    def test_observed_startup_requirement_is_configured_and_replayed_without_compiling(self):
        from tools.ps2native.autoadapt import run_adaptation
        configured = []
        def probe(_):
            return ({'kind': 'startup_requirement', 'purpose': 'working_directory', 'option': '-workspace'}
                    if not configured else {'kind': 'no_code_miss'})
        def configure(requirement, _):
            configured.append(requirement['option'])
            return {'guest_arguments': ['-workspace', 'cdrom0:\\']}
        receipt = run_adaptation(self.root/'adaptation', probe=probe, recover=self.recover,
                                 rebuild=self.rebuild, configure_startup=configure, max_rounds=2)
        self.assertEqual(configured, ['-workspace'])
        self.assertEqual(receipt['configuration_rounds'], 1)
        self.assertEqual(receipt['recovery_rounds'], 0)
        self.assertFalse(receipt['menu_approved'])
        self.assertEqual(self.calls, [])


if __name__ == '__main__':
    unittest.main()

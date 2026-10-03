from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch


class StartupDiagnosticTests(unittest.TestCase):
    def test_working_directory_contract_is_read_from_guest_diagnostic(self):
        from tools.ps2native.autoadapt import read_startup_requirement
        with tempfile.TemporaryDirectory() as directory:
            log = Path(directory)/'runner.log'
            for option in ['-w', '--working-dir', '-workspace']:
                log.write_text(f'other log\nPS2 printf: Missing Command Line Option: {option} (working directory)\n')
                self.assertEqual(read_startup_requirement(log), {
                    'kind': 'startup_requirement', 'purpose': 'working_directory', 'option': option})

    def test_unrecognized_or_ambiguous_arguments_are_not_invented(self):
        from tools.ps2native.autoadapt import read_startup_requirement
        with tempfile.TemporaryDirectory() as directory:
            log = Path(directory)/'runner.log'
            for text in ['Missing Command Line Option: -secret (password)',
                         'Missing Command Line Option: ../../host (working directory)',
                         'Missing Command Line Option: -w (working directory)\n'
                         'Missing Command Line Option: -other (working directory)',
                         'Missing Command Line Option: -w (working directory) trailing data']:
                log.write_text(text)
                self.assertIsNone(read_startup_requirement(log))

    def test_configuration_is_private_persistent_and_reapplied_to_a_new_package(self):
        from tools.ps2native.autoadapt import DesktopAdaptation
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            generator = base/'generator'; generator.write_bytes(b'generator identity')
            args = SimpleNamespace(memory_root=base/'memory', overlay_generator=None, cmake=None,
                                   adapt_rounds=2, probe_timeout=5)
            for number in range(2):
                workspace = base/f'workspace-{number}'; workspace.mkdir()
                package = workspace/'package'
                manifest = {'source': {'iso_sha256': 'owned-iso'},
                            'build_inputs': {'source_tree': {'sha256': 'source-v1'}}}
                with patch('tools.ps2native.autoadapt.shutil.which', return_value='/tool'), \
                     patch('tools.ps2native.autoadapt._find_tool', return_value=generator), \
                     DesktopAdaptation(Path.cwd(), workspace, package, base/'game.iso', args, manifest) as adapter:
                    if number == 0:
                        adapter.configure_startup({'purpose': 'working_directory', 'option': '-workspace'}, workspace)
                        self.assertFalse((adapter.memory/'startup.json').exists())
                        adapter.probe = lambda _: {'kind': 'no_code_miss'}
                        adapter.run()
                    else:
                        self.assertEqual(manifest['automatic_adaptation']['reused_startup_arguments'], 3)
                        adapter.probe = lambda _: {'kind': 'no_code_miss'}
                        receipt = adapter.run()
                        self.assertEqual(receipt['configuration_rounds'], 0)
                    self.assertEqual((package/'game/boot-args.txt').read_text(), 'ps2native\n-workspace\ncdrom0:\\\n')

    def test_repeated_diagnostic_does_not_publish_the_candidate_configuration(self):
        from tools.ps2native.autoadapt import DesktopAdaptation
        from tools.ps2native.pipeline import PipelineError
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            generator = base/'generator'; generator.write_bytes(b'generator identity')
            args = SimpleNamespace(memory_root=base/'memory', overlay_generator=None, cmake=None,
                                   adapt_rounds=2, probe_timeout=5)
            for number in range(2):
                workspace = base/f'workspace-{number}'; workspace.mkdir()
                manifest = {'source': {'iso_sha256': 'owned-iso'},
                            'build_inputs': {'source_tree': {'sha256': 'source-v1'}}}
                with patch('tools.ps2native.autoadapt.shutil.which', return_value='/tool'), \
                     patch('tools.ps2native.autoadapt._find_tool', return_value=generator), \
                     DesktopAdaptation(Path.cwd(), workspace, workspace/'package', base/'game.iso', args, manifest) as adapter:
                    self.assertEqual(manifest['automatic_adaptation']['reused_startup_arguments'], 0)
                    if number == 0:
                        adapter.probe = lambda _: {'kind': 'startup_requirement',
                                                  'purpose': 'working_directory', 'option': '-w'}
                        with self.assertRaisesRegex(PipelineError, 'identical requirement'):
                            adapter.run()
                    self.assertFalse((adapter.memory/'startup.json').exists())

    def test_candidate_from_an_interrupted_conversion_is_not_reused(self):
        from tools.ps2native.autoadapt import DesktopAdaptation
        from tools.ps2native.pipeline import PipelineError, _write_json
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            generator = base/'generator'; generator.write_bytes(b'generator identity')
            args = SimpleNamespace(memory_root=base/'memory', overlay_generator=None, cmake=None,
                                   adapt_rounds=2, probe_timeout=5)
            manifest = {'source': {'iso_sha256': 'owned-iso'},
                        'build_inputs': {'source_tree': {'sha256': 'source-v1'}}}
            with patch('tools.ps2native.autoadapt.shutil.which', return_value='/tool'), \
                 patch('tools.ps2native.autoadapt._find_tool', return_value=generator):
                workspace = base/'workspace-0'; workspace.mkdir()
                with DesktopAdaptation(Path.cwd(), workspace, workspace/'package', base/'game.iso', args, manifest) as adapter:
                    candidate = adapter.configure_startup({'purpose': 'working_directory', 'option': '-w'}, workspace)
                    _write_json(adapter.memory/'startup.json', candidate)
                workspace = base/'workspace-1'; workspace.mkdir()
                with self.assertRaisesRegex(PipelineError, 'startup configuration'):
                    with DesktopAdaptation(Path.cwd(), workspace, workspace/'package', base/'game.iso', args, manifest):
                        pass


class AdaptationBuildIdentityTests(unittest.TestCase):
    def test_lab_backend_changes_invalidate_the_compiled_catalog_identity(self):
        from tools.ps2native.pipeline import source_tree_snapshot
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root/'lab/src/ee_bank_backend.cpp'
            source.parent.mkdir(parents=True)
            source.write_text('compiled backend v1')
            before = source_tree_snapshot(root)
            source.write_text('compiled backend v2')
            after = source_tree_snapshot(root)
            self.assertNotEqual(before['sha256'], after['sha256'])
            private = root/'lab/build/private.cpp'
            private.parent.mkdir(); private.write_text('private generated guest bytes')
            self.assertEqual(after['sha256'], source_tree_snapshot(root)['sha256'])

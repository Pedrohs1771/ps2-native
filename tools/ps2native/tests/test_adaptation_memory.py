import hashlib
import json
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

from tools.ps2native.autoadapt import DesktopAdaptation
from tools.ps2native.pipeline import PipelineError, sha256_file


class MemoryPathTests(unittest.TestCase):
    def test_memory_key_symlink_is_rejected_without_creating_an_external_lock(self):
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            generator = base/'generator'; generator.write_bytes(b'generator identity')
            memory = base/'memory'; memory.mkdir()
            external = base/'external'; external.mkdir()
            workspace = base/'workspace'; workspace.mkdir()
            identity = {'iso_sha256': 'owned-iso', 'generator_sha256': sha256_file(generator),
                        'source_tree_sha256': 'source-v1'}
            key = hashlib.sha256(json.dumps(identity, sort_keys=True).encode()).hexdigest()
            (memory/key).symlink_to(external, target_is_directory=True)
            manifest = {'source': {'iso_sha256': 'owned-iso'},
                        'build_inputs': {'source_tree': {'sha256': 'source-v1'}}}
            args = SimpleNamespace(memory_root=memory, overlay_generator=None, cmake=None,
                                   adapt_rounds=2, probe_timeout=5)
            adapter = DesktopAdaptation(Path.cwd(), workspace, workspace/'package',
                                        base/'game.iso', args, manifest)
            with patch('tools.ps2native.autoadapt.shutil.which', return_value='/tool'), \
                 patch('tools.ps2native.autoadapt._find_tool', return_value=generator):
                with self.assertRaisesRegex(PipelineError, 'symlink'):
                    with adapter:
                        pass
            self.assertFalse((external/'conversion.lock').exists())


if __name__ == '__main__':
    unittest.main()

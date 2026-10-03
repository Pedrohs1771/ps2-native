import json
from pathlib import Path
import tempfile
from types import SimpleNamespace
import subprocess
import unittest

from tools.ps2native.pipeline import PipelineError


class CorpusLinkTests(unittest.TestCase):
    def test_native_object_reuse_tracks_source_bytes_and_rejects_object_changes(self):
        from lab.run_native_corpus_adaptation import CorpusAdaptation
        from tools.ps2native.pipeline import repo_root
        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            source = base/'bank.cpp'
            main = base/'main.cpp'
            main.write_text('#include <cstdio>\nextern "C" int value();\nint main(){std::printf("%d",value());}\n')
            adapter = object.__new__(CorpusAdaptation)
            adapter.root = repo_root()
            adapter.args = SimpleNamespace(timeout=30)
            adapter.bank_objects = {}
            first = base/'first'; first.mkdir()
            second = base/'second'; second.mkdir()
            source.write_text('extern "C" int value(){return 43;}\n')
            original, reused = adapter.compile_bank(source, first)
            self.assertFalse(reused)
            cached, reused = adapter.compile_bank(source, second)
            self.assertTrue(reused)
            self.assertEqual(cached, original)
            source.write_text('extern "C" int value(){return 45;}\n')
            changed, reused = adapter.compile_bank(source, second)
            self.assertFalse(reused)
            for obj, expected in [(original, '43'), (changed, '45')]:
                runner = base/expected
                subprocess.run(['/usr/bin/c++', str(main), str(obj), '-o', str(runner)], check=True, timeout=30)
                self.assertEqual(subprocess.check_output([str(runner)], text=True, timeout=10), expected)
            original.write_bytes(b'altered object')
            source.write_text('extern "C" int value(){return 43;}\n')
            with self.assertRaisesRegex(PipelineError, 'object changed'):
                adapter.compile_bank(source, second)

    def test_cmake_responses_keep_guest_objects_and_replace_only_host_entry(self):
        from lab.run_native_corpus_adaptation import link_inputs
        with tempfile.TemporaryDirectory() as temporary:
            cwd = Path(temporary)
            main = cwd/'src/main.cpp.o'; main.parent.mkdir(); main.write_bytes(b'host')
            guest = cwd/'guest quoted name.cpp.o'; guest.write_bytes(b'guest')
            response = cwd/'objects.rsp'
            response.write_text(json.dumps(str(main))+'\n'+json.dumps(str(guest))+'\n')
            archive = cwd/'libps2_runtime.a'; archive.write_bytes(b'old runtime')
            argv, objects = link_inputs(['c++', '@objects.rsp', '-o', 'old-runner',
                                         'libps2_runtime.a', '-lpthread'], cwd)
            self.assertEqual(objects, [guest])
            self.assertEqual(argv, ['c++', '-o', 'old-runner', '-lpthread'])
            self.assertEqual(guest.read_bytes(), b'guest')

    def test_missing_or_ambiguous_host_input_is_rejected_before_link(self):
        from lab.run_native_corpus_adaptation import link_inputs
        with tempfile.TemporaryDirectory() as temporary:
            cwd = Path(temporary)
            for argv in [['c++', '@absent.rsp', '-o', 'runner', 'libps2_runtime.a'],
                         ['c++', '-o', 'runner', 'libps2_runtime.a'],
                         ['c++', '-o', 'runner', 'libps2_runtime.a', 'libps2_runtime.a']]:
                with self.subTest(argv=argv), self.assertRaises(PipelineError):
                    link_inputs(argv, cwd)

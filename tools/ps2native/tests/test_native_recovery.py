import json
import hashlib
import struct
import subprocess
import sys
from pathlib import Path
from types import SimpleNamespace
import tempfile
import unittest
from unittest import mock

from tools.ps2native.cli import parse_args
from tools.ps2native import native_recovery
from tools.ps2native.pipeline import PipelineError


class RecoveryTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.root=Path(self.temp.name)
        self.captures=self.root/'misses';self.captures.mkdir()
        self.tool=self.root/'generator';self.tool.write_bytes(b'identified executable')
        self.previous=self.root/'previous';self.previous.mkdir()
        self.project=self.root/'project';self.project.mkdir()
        (self.project/'CMakeLists.txt').write_text('project(fixture)')
        self.build=self.root/'native-build';self.build.mkdir()
        (self.build/'CMakeCache.txt').write_text('CMAKE_HOME_DIRECTORY:INTERNAL='+str(self.project)+'\n')
        self.args=SimpleNamespace(capture_root=self.captures,previous_batch=self.previous,
            previous_family_catalog=None,family_generator=str(self.tool),
            overlay_generator=str(self.tool),out=self.root/'recovered',workers=4,
            timeout=60,native_build=self.build,cmake=str(self.tool))

    def tearDown(self):self.temp.cleanup()

    def fake_run(self,command,log_path,cwd,timeout):
        log_path.write_text('bounded fixture output')
        if '--capture-root' in command:
            batch=self.args.out/'batch';(batch/'catalog').mkdir(parents=True)
            (batch/'catalog/catalog.json').write_text('{}')
            (batch/'report.json').write_text(json.dumps({'status':'PUBLISHED_LABORATORY',
                'manifest_published':True,'strict_approval':False,'closure_proved':False,
                'owned_cases':2,'family_count':1,'reused_body_sources':1,
                'family_catalog_sha256':hashlib.sha256(b'{}').hexdigest()}))
        return ''

    def test_cli_has_recovery_without_any_game_name_or_pc_parameter(self):
        args=parse_args(['recover-ee','--capture-root','misses','--previous-batch','batch',
                         '--family-generator','family','--overlay-generator','overlay','--out','recovered'])
        self.assertEqual(args.command,'recover-ee')
        self.assertEqual(args.capture_root,Path('misses'))
        self.assertIsNone(args.native_build)

    def test_recovery_collects_batch_then_configures_and_builds_the_delta(self):
        with mock.patch.object(native_recovery,'_run',side_effect=self.fake_run) as run:
            result=native_recovery.run_recovery(self.args)
        commands=[call.args[0] for call in run.call_args_list]
        self.assertEqual(len(commands),3)
        self.assertIn('--capture-root',commands[0])
        self.assertIn('--previous-batch',commands[0])
        self.assertIn('-DNEXO_EE_FAMILY_MANIFEST='+str(self.args.out/'batch/catalog/catalog.json'),commands[1])
        self.assertEqual(commands[2][-4:],['--target','ps2_ee_compiled_families','--parallel','4'])
        self.assertEqual(result['status'],'built')
        self.assertFalse(result['strict_approval'])
        self.assertFalse(result['closure_proved'])
        receipt=json.loads((self.args.out/'recovery.json').read_text())
        self.assertEqual(receipt['status'],'built')
        self.assertEqual(len(receipt['steps']),3)

    def test_failed_batch_never_starts_a_build_and_preserves_failed_receipt(self):
        with mock.patch.object(native_recovery,'_run',side_effect=PipelineError('frontend failed')) as run:
            with self.assertRaises(PipelineError):native_recovery.run_recovery(self.args)
        self.assertEqual(run.call_count,1)
        receipt=json.loads((self.args.out/'recovery.json').read_text())
        self.assertEqual(receipt['status'],'failed')
        self.assertFalse(receipt['strict_approval'])

    def test_invalid_budget_existing_destination_or_missing_cache_fail_before_tools(self):
        for updates in [{'workers':True},{'workers':17},{'timeout':0},
                        {'native_build':self.root/'missing'}]:
            args=SimpleNamespace(**{**vars(self.args),**updates})
            with self.subTest(updates=updates),mock.patch.object(native_recovery,'_run') as run:
                with self.assertRaises((ValueError,PipelineError)):native_recovery.run_recovery(args)
                run.assert_not_called()
        self.args.out.mkdir()
        with mock.patch.object(native_recovery,'_run') as run:
            with self.assertRaises((ValueError,PipelineError)):native_recovery.run_recovery(self.args)
            run.assert_not_called()

    def test_cli_recovers_a_real_synthetic_capture_with_existing_offline_tools(self):
        root=Path(__file__).resolve().parents[3]
        self.tool.write_text('#!/usr/bin/env python3\nimport pathlib,sys\n'
            'pathlib.Path(sys.argv[3]).write_text("void ps2native_data_family(uint8_t*,R5900Context*,PS2Runtime*,uint32_t){}\\n")\n')
        self.tool.chmod(0o700)
        overlay=self.root/'overlay'
        overlay.write_text('#!/usr/bin/env python3\nimport pathlib,sys\n'
            'base,entry=int(sys.argv[2]),int(sys.argv[3])\n'
            'pathlib.Path(sys.argv[4]).write_text(f"{{0x{entry:x}u,ps2native_block_{entry:x},0x{base:x}u,0xcu,snapshot+0x0u}},\\n")\n')
        overlay.chmod(0o700)
        case=self.root/'case';case.mkdir();image=struct.pack('<3I',0x3c020001,0x03e00008,0)
        (case/'snapshot.bin').write_bytes(image)
        (case/'bank.json').write_text(json.dumps({'schema_version':1,'base':0x10000,
            'entry':0x10000,'image_bytes':12,'image_sha256':hashlib.sha256(image).hexdigest(),
            'bindings':[{'address':0x10000,'source_begin':0x10000,'source_bytes':12}]}))
        self.previous.rmdir()
        previous=subprocess.run([sys.executable,str(root/'lab/prepare_ee_family_batch.py'),
            '--case',str(case),'--family-generator',str(self.tool),'--output',str(self.previous)],
            capture_output=True,text=True,cwd=root)
        self.assertEqual(previous.returncode,0,previous.stderr)
        capture=self.captures/'ee-miss-000001';capture.mkdir();ram=bytearray(32*1024*1024)
        ram[0x10000:0x1000c]=image
        (capture/'ee-ram.bin').write_bytes(ram);(capture/'snapshot.bin').write_bytes(image)
        (capture/'ee-context.bin').write_bytes(b'')
        (capture/'request.json').write_text(json.dumps({'schema_version':1,'processor':'EE',
            'window_base':0x10000,'window_bytes':12,'target_pc':0x10000,'ee_model_profile':1,
            'runtime_admission':'missing','complete':True,'ram_captured':True,
            'context_captured':False,'module_owns_address':False,'complete_machine_checkpoint':False,
            'quiescence_qualified':False,'overlay_lookup_status':'MissingEntry'}))
        result=subprocess.run([sys.executable,'-m','tools.ps2native','recover-ee',
            '--capture-root',str(self.captures),'--previous-batch',str(self.previous),
            '--family-generator',str(self.tool),'--overlay-generator',str(overlay),
            '--out',str(self.args.out)],capture_output=True,text=True,cwd=root)
        self.assertEqual(result.returncode,0,result.stderr)
        report=json.loads((self.args.out/'recovery.json').read_text())
        self.assertEqual(report['status'],'generated')
        self.assertEqual(report['family_count'],1)
        self.assertEqual(report['reused_body_sources'],1)
        self.assertFalse(report['gameplay_approved'])


if __name__=='__main__':unittest.main()

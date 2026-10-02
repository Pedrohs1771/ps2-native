"""Offline batch ownership/orchestration tests; mock frontends never execute guests."""
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'lab'))
import prepare_ee_family_batch as batch


class FamilyBatchTests(unittest.TestCase):
    def setUp(self):
        self.temporary=tempfile.TemporaryDirectory()
        self.root=Path(self.temporary.name)
        self.family=self.root/'family-generator'
        self.family.write_text('#!/usr/bin/env python3\nimport pathlib,sys\n'
            'pathlib.Path(sys.argv[3]).write_text("void ps2native_data_family(uint8_t*,R5900Context*,PS2Runtime*,uint32_t){}\\n")\n')
        self.family.chmod(0o700)
        self.overlay=self.root/'overlay-generator'
        self.overlay.write_text('#!/usr/bin/env python3\nimport pathlib,sys\n'
            'base,entry=int(sys.argv[2]),int(sys.argv[3])\n'
            'pathlib.Path(sys.argv[4]).write_text(f"{{0x{entry:x}u,ps2native_block_{entry:x},0x{base:x}u,0xcu,snapshot+0x0u}},\\n")\n')
        self.overlay.chmod(0o700)

    def tearDown(self):
        self.temporary.cleanup()

    def case(self,name,value=1):
        case=self.root/name;case.mkdir()
        image=struct.pack('<III',0x3c020000|value,0x03e00008,0)
        (case/'snapshot.bin').write_bytes(image)
        (case/'bank.json').write_text(json.dumps({'schema_version':1,'base':0x10000,'entry':0x10000,
            'image_bytes':len(image),'image_sha256':hashlib.sha256(image).hexdigest(),
            'bindings':[{'address':pc,'source_begin':0x10000,'source_bytes':12}
                        for pc in range(0x10000,0x1000c,4)]}))
        return case

    def run_batch(self,**kwargs):
        return batch.prepare_batch(self.root/'output',self.family,
                                   overlay_generator=self.overlay,**kwargs)

    def test_batch_owns_and_deduplicates_cases_without_source_changes(self):
        a=self.case('a');b=self.case('b',2)
        before={p:p.read_bytes() for c in [a,b] for p in c.iterdir()}
        result=self.run_batch(cases=[a,a,b],workers=2)
        self.assertEqual(result['status'],'PUBLISHED_LABORATORY')
        self.assertEqual(result['owned_cases'],2)
        self.assertEqual(result['duplicate_cases'],1)
        self.assertEqual(result['family_count'],1)
        self.assertFalse(result['strict_approval'])
        report=json.loads((self.root/'output/candidates.json').read_text())
        self.assertEqual(report['discovery_policy']['minimum_variants'],1)
        self.assertEqual(report['families'][0]['guard_masks'],[0xffff0000,0xffffffff,0xffffffff])
        self.assertEqual({p:p.read_bytes() for p in before},before)
        self.assertEqual(len(list((self.root/'output/cases').iterdir())),2)

    def test_capture_preparation_needs_no_manual_root_descriptor(self):
        case=self.case('source');capture=self.root/'capture';capture.mkdir()
        image=(case/'snapshot.bin').read_bytes();ram=bytearray(32*1024*1024)
        ram[0x10000:0x1000c]=image
        (capture/'ee-ram.bin').write_bytes(ram);(capture/'snapshot.bin').write_bytes(image)
        (capture/'request.json').write_text(json.dumps({'schema_version':1,'processor':'EE',
            'window_base':0x10000,'window_bytes':12,'target_pc':0x10000,'ee_model_profile':1,
            'runtime_admission':'missing','complete':True,'ram_captured':True,
            'context_captured':False,'module_owns_address':False,'complete_machine_checkpoint':False,
            'quiescence_qualified':False,'overlay_lookup_status':'MissingEntry'}))
        result=self.run_batch(captures=[capture])
        self.assertEqual(result['prepared_captures'],1)
        self.assertEqual(result['family_count'],1)
        self.assertTrue(result['manifest_published'])
        self.assertFalse(result['closure_proved'])

    def test_failed_frontend_leaves_receipt_and_no_catalog_manifest(self):
        case=self.case('a');self.family.write_text('#!/usr/bin/env python3\nraise SystemExit(2)\n')
        with self.assertRaises(ValueError):self.run_batch(cases=[case])
        result=json.loads((self.root/'output/report.json').read_text())
        self.assertEqual(result['status'],'FAILED')
        self.assertEqual(result['failed_step'],'catalog-generation')
        self.assertFalse(result['manifest_published'])
        self.assertFalse((self.root/'output/catalog/catalog.json').exists())

    def test_overlaps_links_and_invalid_budgets_fail_before_mutation(self):
        case=self.case('a')
        with self.assertRaises(ValueError):
            batch.prepare_batch(case/'output',self.family,cases=[case])
        linked=self.root/'linked';linked.symlink_to(case,target_is_directory=True)
        with self.assertRaises(ValueError):self.run_batch(cases=[linked])
        for kwargs in [{'workers':False},{'workers':17},{'captures':[case]*17}]:
            with self.subTest(kwargs=kwargs),self.assertRaises(ValueError):
                self.run_batch(cases=[case],**kwargs)
        self.assertFalse((self.root/'output').exists())

    def test_cli_and_existing_receipt_are_not_overwritten(self):
        case=self.case('a');output=self.root/'output'
        command=[sys.executable,str(ROOT/'lab/prepare_ee_family_batch.py'),'--case',str(case),
                 '--family-generator',str(self.family),'--output',str(output)]
        result=subprocess.run(command,capture_output=True,text=True)
        self.assertEqual(result.returncode,0,result.stderr)
        before=(output/'report.json').read_bytes()
        repeat=subprocess.run(command,capture_output=True,text=True)
        self.assertNotEqual(repeat.returncode,0)
        self.assertNotIn('Traceback',repeat.stderr)
        self.assertEqual((output/'report.json').read_bytes(),before)

    def test_previous_batch_preserves_sources_and_rejects_tampering(self):
        self.run_batch(cases=[self.case('a')])
        previous=self.root/'output'
        before=json.loads((previous/'catalog/catalog.json').read_text())
        result=batch.prepare_batch(self.root/'next',self.family,previous_batch=previous,workers=2)
        after=json.loads((self.root/'next/catalog/catalog.json').read_text())
        self.assertEqual(result['owned_cases'],1)
        self.assertEqual(before['sha256'],after['sha256'])
        receipt=json.loads((previous/'report.json').read_text())
        original=(previous/'report.json').read_bytes()
        for updates in [{'owned_cases':True},{'strict_approval':True},{'closure_proved':True},
                        {'manifest_published':False},{'status':'FAILED'},
                        {'family_catalog_sha256':'0'*64},{'case_records':[]}]:
            (previous/'report.json').write_text(json.dumps({**receipt,**updates}))
            with self.subTest(updates=updates),self.assertRaises(ValueError):
                batch.owned_batch_cases(previous)
        (previous/'report.json').write_bytes(original)
        case=previous/'cases'/receipt['case_records'][0]['key']
        (case/'snapshot.bin').write_bytes(b'changed')
        with self.assertRaises(ValueError):batch.owned_batch_cases(previous)

    def test_case_identity_frames_metadata_and_image_lengths(self):
        self.assertNotEqual(batch.case_identity(b'ab',b'c'),batch.case_identity(b'a',b'bc'))

    def test_expansion_reuses_previous_catalog_units_automatically(self):
        self.run_batch(cases=[self.case('first')])
        previous=self.root/'output'
        case=self.case('second')
        image=struct.pack('<III',0x3c030001,0x03e00008,0)
        (case/'snapshot.bin').write_bytes(image)
        path=case/'bank.json';metadata=json.loads(path.read_text())
        metadata['image_sha256']=hashlib.sha256(image).hexdigest();path.write_text(json.dumps(metadata))
        result=batch.prepare_batch(self.root/'next',self.family,previous_batch=previous,cases=[case])
        self.assertEqual(result['owned_cases'],2)
        catalog=json.loads((self.root/'next/catalog/catalog.json').read_text())
        self.assertEqual(catalog['reused_families'],1)
        self.assertEqual(catalog['reused_body_sources'],1)

    def test_default_batch_includes_linear_continuations_without_manual_roots(self):
        case=self.case('split');words=[0x3c020001,0x24420002,0xac820000,0x24420003,0x03e00008,0]
        image=struct.pack('<6I',*words);(case/'snapshot.bin').write_bytes(image)
        metadata=json.loads((case/'bank.json').read_text());base=metadata['base']
        metadata.update(image_bytes=len(image),image_sha256=hashlib.sha256(image).hexdigest(),
            bindings=[{'address':base,'source_begin':base,'source_bytes':12},
                      {'address':base+12,'source_begin':base+12,'source_bytes':12}])
        (case/'bank.json').write_text(json.dumps(metadata))
        result=self.run_batch(cases=[case])
        catalog=json.loads((self.root/'output/catalog/catalog.json').read_text())
        self.assertEqual(sorted(len(row['words']) for row in catalog['families']),[3,6])
        self.assertEqual(result['discovery_policy']['region_policy'],'canonical-v1')
        self.assertFalse(catalog['terminal_only'])


if __name__=='__main__':unittest.main()

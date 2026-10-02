"""Publication/admission metadata tests; these do not qualify PS2 semantics."""
import copy
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'lab'))
import generate_ee_family_catalog as publisher


class FamilyCatalogTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.generator = self.root / 'generator'
        self.generator.write_text('#!/usr/bin/env python3\nimport pathlib,sys\n'
                                  'pathlib.Path(sys.argv[3]).write_text("#include <cstdint>\\n'
                                  'void ps2native_data_family(uint8_t*,R5900Context*,PS2Runtime*,uint32_t){}\\n")\n')
        self.generator.chmod(0o700)
        self.family = {'shape_sha256': '1' * 64, 'word_count': 3,
                       'guard_words': [0x3c010000, 0x03e00008, 0],
                       'guard_masks': [0xffff0000, 0xffffffff, 0xffffffff]}
        self.report = {'schema_version': 1, 'status': 'CANDIDATES_LABORATORY',
                       'strict_approval': False, 'families': [self.family]}
        self.report_path = self.root / 'candidates.json'
        self.write_report()

    def tearDown(self):
        self.temporary.cleanup()

    def write_report(self):
        self.report_path.write_text(json.dumps(self.report))

    def generate(self, name='catalog', **kwargs):
        return publisher.generate(self.report_path, self.generator, self.root / name, **kwargs)

    def case(self):
        case = self.root / 'case'
        case.mkdir()
        image = struct.pack('<III', 0x2402002a, 0x03e00008, 0)
        (case / 'snapshot.bin').write_bytes(image)
        metadata = {'schema_version': 1, 'base': 0x10000, 'entry': 0x10004,
                    'image_bytes': 12, 'image_sha256': hashlib.sha256(image).hexdigest(),
                    'bindings': [{'address': 0x10004, 'source_begin': 0x10000, 'source_bytes': 12}]}
        (case / 'bank.json').write_text(json.dumps(metadata))
        return case, metadata

    def configure(self, directory, *, succeeds):
        project = self.root / 'project'
        project.mkdir(exist_ok=True)
        (project / 'CMakeLists.txt').write_text(
            'cmake_minimum_required(VERSION 3.20)\nproject(FamilyManifest NONE)\n'
            f'include("{ROOT / "ps2xRuntime/cmake/ee_family_catalog.cmake"}")\n'
            f'ee_family_catalog_sources("{directory / "catalog.json"}" sources)\n')
        result = subprocess.run(['cmake', '-S', str(project), '-B', str(self.root / 'cmake-build')],
                                capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode == 0, succeeds, result.stdout + result.stderr)

    def test_published_sources_are_hashed_and_duplicates_merge(self):
        self.report['families'].append(copy.deepcopy(self.family))
        self.write_report()
        result = self.generate()
        self.assertEqual(result['family_count'], 1)
        self.assertIs(result['strict_approval'], False)
        self.assertIs(result['closure_proved'], False)
        for name in result['sources']:
            self.assertEqual(result['sha256'][name], hashlib.sha256((self.root/'catalog'/name).read_bytes()).hexdigest())
        self.configure(self.root/'catalog', succeeds=True)

    def test_changed_report_is_rejected_before_catalog_publication(self):
        reader = publisher.read_report

        def replace_before_parse(path, **kwargs):
            changed = copy.deepcopy(self.report)
            changed['families'][0]['guard_words'][0] = 0x3c020000
            path.write_text(json.dumps(changed))
            return reader(path, **kwargs)

        with patch.object(publisher, 'read_report', side_effect=replace_before_parse):
            with self.assertRaisesRegex(ValueError, 'candidate report identity differs'):
                self.generate()
        self.assertFalse((self.root/'catalog').exists())

    def test_root_provenance_and_requested_entry_are_preserved(self):
        case, metadata = self.case()
        result = self.generate(cases=[case])
        self.assertEqual(result['family_count'], 2)
        ledger = result['root_cases'][0]
        self.assertEqual(ledger['metadata_sha256'], hashlib.sha256((case/'bank.json').read_bytes()).hexdigest())
        self.assertEqual(ledger['image_sha256'], metadata['image_sha256'])
        self.assertEqual((ledger['entry'], ledger['source_begin'], ledger['source_bytes']), (0x10004, 0x10000, 12))
        self.assertIn('static const uint32_t entries[]={4u};', '\n'.join(
            (self.root/'catalog'/name).read_text() for name in result['sources']))

    def test_selection_and_output_must_be_valid_and_fresh(self):
        for key in ['../escape', '2'*64]:
            with self.subTest(key=key), self.assertRaises(ValueError):
                self.generate(only_shapes=[key])
        self.generate(only_shapes=['1'*64])
        with self.assertRaises(ValueError):
            self.generate()

    def test_declared_entries_and_batching_are_explicit(self):
        self.family['normal_entry_offsets']=[0,8]
        self.write_report()
        one=self.generate('observed',families_per_source=1)
        roots=self.generate('roots',entry_policy='root-only')
        self.assertEqual(one['schema_version'],2)
        self.assertEqual(one['families'][0]['normal_entry_offsets'],[0,8])
        self.assertEqual(roots['families'][0]['normal_entry_offsets'],[0])
        self.assertEqual(roots['source_count'],len(roots['sources']))
        case,_=self.case()
        merged=self.generate('merged',cases=[case],source_buckets=1)
        separate=self.generate('separate',cases=[case],families_per_source=1,source_buckets=1)
        self.assertEqual(merged['family_count'],separate['family_count'])
        self.assertEqual(merged['families'],separate['families'])
        self.assertEqual(merged['source_count'],2)
        self.assertEqual(separate['source_count'],3)
        self.configure(self.root/'merged',succeeds=True)
        for offsets in [[],[False],[2],[0,0],[{}]]:
            self.family['normal_entry_offsets']=offsets;self.write_report()
            with self.subTest(offsets=offsets),self.assertRaises(ValueError):
                self.generate('invalid',entry_policy='root-only')

    def test_root_data_parameters_keep_opcodes_registers_and_control_fixed(self):
        case,metadata=self.case()
        result=self.generate(cases=[case],root_data_parameters=True)
        root=[row for row in result['families'] if row['words'][0]>>26==9][0]
        self.assertEqual(root['masks'],[0xffff0000,0xffffffff,0xffffffff])
        self.assertEqual(root['words'],[0x24020000,0x03e00008,0])
        self.assertEqual(root['normal_entry_offsets'],[4])
        self.assertEqual(result['root_cases'][0]['operand_policy'],'typed-data')
        self.assertFalse(result['closure_proved'])

    def test_hash_buckets_keep_unrelated_source_units_unchanged(self):
        first=self.generate('first',families_per_source=1)
        case,_=self.case()
        second=self.generate('second',cases=[case],families_per_source=1)
        shared=set(first['sha256'])&set(second['sha256'])-{'ee_family_catalog.cpp'}
        self.assertEqual(len(shared),1)
        self.assertTrue(all(first['sha256'][name]==second['sha256'][name] for name in shared))

    def test_inserting_one_family_replaces_at_most_one_existing_body_unit(self):
        families=[]
        for reg in range(1,13):
            family={**self.family,'shape_sha256':format(reg,'064x'),
                    'guard_words':[0x3c000000|(reg<<16),0x03e00008,0]}
            key=hashlib.sha256(b'ee-native-family-catalog-v0\0'+
                publisher.encode(family['guard_words'])+
                publisher.encode(family['guard_masks'])).hexdigest()
            families.append((key,family))
        families.sort(key=lambda row:row[0])
        self.report['families']=[row[1] for row in families[1:9]]
        self.write_report()
        first=self.generate('first',families_per_source=2,source_buckets=1)
        self.report['families'].append(families[0][1]);self.write_report()
        second=self.generate('second',families_per_source=2,source_buckets=1)
        before=set(first['sources'])-{'ee_family_catalog.cpp'}
        after=set(second['sources'])-{'ee_family_catalog.cpp'}
        self.assertLessEqual(len(before-after),1,
                             'An insertion may split its leaf, but cannot shift unrelated units')
        for name in before&after:self.assertEqual(first['sha256'][name],second['sha256'][name])
        self.assertEqual(second['family_count'],first['family_count']+1)

    def test_parallel_generation_preserves_deterministic_source_bytes(self):
        case,_=self.case()
        serial=self.generate('serial',cases=[case],workers=1)
        parallel=self.generate('parallel',cases=[case],workers=2)
        self.assertEqual(serial['families'],parallel['families'])
        self.assertEqual(serial['sha256'],parallel['sha256'])
        self.assertEqual(serial['rejected'],parallel['rejected'])
        self.assertEqual(parallel['generation_workers'],2)
        for workers in [False,0,17]:
            with self.subTest(workers=workers),self.assertRaises(ValueError):
                self.generate('invalid',workers=workers)

    def test_previous_catalog_keeps_existing_body_units_when_families_are_added(self):
        self.report['families']=[{**self.family,'shape_sha256':format(reg,'064x'),
            'guard_words':[0x3c000000|(reg<<16),0x03e00008,0]} for reg in range(1,9)]
        self.write_report()
        first=self.generate('first',families_per_source=2,source_buckets=1)
        before={name for name in first['sources'] if name!='ee_family_catalog.cpp'}
        self.report['families'].append({**self.family,'shape_sha256':'9'*64,
            'guard_words':[0x3c090000,0x03e00008,0]})
        self.write_report()
        second=self.generate('second',families_per_source=2,source_buckets=1,
            previous_catalog=self.root/'first/catalog.json')
        self.assertTrue(before.issubset(second['sources']))
        self.assertEqual(second['reused_body_sources'],len(before))
        self.assertEqual(second['reused_families'],8)
        self.assertEqual(second['family_count'],9)
        for name in before:self.assertEqual(first['sha256'][name],second['sha256'][name])
        third=self.generate('third',families_per_source=2,source_buckets=1,
            previous_catalog=self.root/'second/catalog.json')
        self.assertEqual(third['sha256'],second['sha256'])
        self.configure(self.root/'third',succeeds=True)

    def test_previous_catalog_rejects_changed_or_unsafe_sources(self):
        first=self.generate('first')
        path=self.root/'first/catalog.json';original=path.read_bytes()
        for updates in [{'strict_approval':True},{'source_count':True},
                        {'sources':['../outside.cpp','ee_family_catalog.cpp']},
                        {'sha256':{name:'0'*64 for name in first['sources']}}]:
            path.write_text(json.dumps({**first,**updates}))
            with self.subTest(updates=updates),self.assertRaises(ValueError):
                self.generate('invalid',previous_catalog=path)
            self.assertFalse((self.root/'invalid').exists())
        path.write_bytes(original)
        body=next(name for name in first['sources'] if name!='ee_family_catalog.cpp')
        (self.root/'first'/body).write_text('changed')
        with self.assertRaises(ValueError):self.generate('invalid',previous_catalog=path)

    def test_previous_body_is_not_reused_when_current_entry_contract_changes(self):
        self.family['normal_entry_offsets']=[0,8];self.write_report()
        first=self.generate('first')
        self.family['normal_entry_offsets']=[0];self.write_report()
        second=self.generate('second',previous_catalog=self.root/'first/catalog.json')
        self.assertEqual(second['reused_body_sources'],0)
        self.assertEqual(second['reused_families'],0)
        self.assertFalse(set(first['sources'])&set(second['sources'])-{'ee_family_catalog.cpp'})

    def test_proposals_and_admitted_families_have_separate_budgets(self):
        other={**self.family,'shape_sha256':'2'*64,'guard_words':[0x3c020000,0x03e00008,0]}
        self.report['families'].append(other);self.write_report()
        with patch.object(publisher,'MAX_FAMILIES',1):
            with self.assertRaises(ValueError):self.generate()
        self.assertFalse((self.root/'catalog').exists())

    def test_canonical_policy_cannot_mix_short_prefixes_or_extra_entries(self):
        self.report['discovery_policy']={'region_policy':'canonical-v1','operand_policy':'typed',
            'root_only':True,'terminal_only':False}
        self.family['normal_entry_offsets']=[0];self.write_report()
        valid=self.generate('valid',entry_policy='root-only')
        self.assertEqual(valid['region_policy'],'canonical-v1')
        for updates in [{'guard_words':[0x24420000]*3},
                        {'normal_entry_offsets':[0,4]}, {'guard_masks':[0xffffffff]*3}]:
            original=copy.deepcopy(self.report)
            self.family.update(updates);self.write_report()
            with self.subTest(updates=updates),self.assertRaises(ValueError):
                self.generate('invalid',entry_policy='root-only')
            self.report=original;self.family=self.report['families'][0]
        self.write_report()
        for kwargs in [{'terminal_only':True},{'entry_policy':'observed'},
                       {'cases':[self.case()[0]]}]:
            with self.subTest(kwargs=list(kwargs)),self.assertRaises(ValueError):
                self.generate('invalid',**{'entry_policy':'root-only',**kwargs})

    def test_batched_index_and_body_have_separate_size_bounds(self):
        manifest=self.generate()
        directory=self.root/'catalog';index=directory/'ee_family_catalog.cpp'
        index.write_text(index.read_text()+'\n//'+('x'*(1024*1024)))
        manifest['sha256'][index.name]=hashlib.sha256(index.read_bytes()).hexdigest()
        (directory/'catalog.json').write_text(json.dumps(manifest))
        self.configure(directory,succeeds=True)
        body=directory/next(name for name in manifest['sources'] if name!=index.name)
        body.write_text(body.read_text()+'\n//'+('x'*(1024*1024)))
        manifest['sha256'][body.name]=hashlib.sha256(body.read_bytes()).hexdigest()
        (directory/'catalog.json').write_text(json.dumps(manifest))
        self.configure(directory,succeeds=False)

    def test_malformed_candidate_types_and_budgets_fail(self):
        valid = copy.deepcopy(self.family)
        for updates in [{'word_count': True}, {'word_count': 3.0}, {'shape_sha256': []},
                        {'guard_words': [True, 0x03e00008, 0]}, {'guard_masks': [0xfc000000]*3},
                        {'guard_words': [0]*129, 'guard_masks': [0xffffffff]*129, 'word_count':129}]:
            self.report['families'] = [{**valid, **updates}]
            self.write_report()
            with self.subTest(updates=updates), self.assertRaises(ValueError):
                self.generate()
            self.assertFalse((self.root/'catalog').exists())

    def test_root_metadata_and_tampered_bytes_fail(self):
        case, valid = self.case()
        for metadata in [[], {**valid, 'base': True}, {**valid, 'image_bytes': 12.0},
                         {**valid, 'bindings': [{**valid['bindings'][0], 'source_bytes': 516}]},
                         {**valid, 'image_sha256': '0'*64}]:
            (case/'bank.json').write_text(json.dumps(metadata))
            with self.subTest(metadata=metadata), self.assertRaises(ValueError):
                self.generate(cases=[case])
            self.assertFalse((self.root/'catalog').exists())

    def test_symlink_inputs_and_all_declined_generator_fail(self):
        linked = self.root/'linked.json'
        linked.symlink_to(self.report_path)
        with self.assertRaises(ValueError):
            publisher.generate(linked, self.generator, self.root/'catalog')
        self.generator.write_text('#!/usr/bin/env python3\nraise SystemExit(2)\n')
        with self.assertRaises(ValueError):
            self.generate()
        self.assertFalse((self.root/'catalog').exists())

    def test_cmake_rejects_wrong_types_approval_counts_names_and_hashes(self):
        valid = self.generate()
        path = self.root/'catalog/catalog.json'
        variants = [{'schema_version':'1'}, {'family_count':'1'}, {'family_count':1.5},
                    {'strict_approval':0}, {'strict_approval':'false'}, {'strict_approval':True},
                    {'closure_proved':True}, {'closure_proved':0}, {'sources':{}},
                    {'sources':[42, valid['sources'][1]]}, {'family_count':2}, {'source_count':True},
                    {'source_count':'2'}, {'source_count':3}, {'families':{}},
                    {'sources':['../escape.cpp', 'ee_family_catalog.cpp']},
                    {'sha256':{name:'0'*64 for name in valid['sources']}}]
        for change in variants:
            path.write_text(json.dumps({**valid, **change}))
            with self.subTest(change=change):
                self.configure(self.root/'catalog', succeeds=False)
        path.write_text(json.dumps(valid))
        (self.root/'catalog'/valid['sources'][0]).write_text('tampered')
        self.configure(self.root/'catalog', succeeds=False)

    def test_cmake_rejects_symlink_ancestry(self):
        self.generate()
        linked = self.root/'linked'
        linked.symlink_to(self.root/'catalog', target_is_directory=True)
        self.configure(linked, succeeds=False)

    def test_cmake_cache_reuses_bytes_and_rejects_corruption(self):
        manifest=self.generate()
        self.configure(self.root/'catalog',succeeds=True)
        cache=self.root/'cmake-build/ee-family-source-cache'
        cached=cache/('ee_family_'+manifest['sha256']['ee_family_catalog.cpp']+'.cpp')
        before=cached.stat().st_mtime_ns
        self.configure(self.root/'catalog',succeeds=True)
        self.assertEqual(cached.stat().st_mtime_ns,before)
        cached.write_text('corrupted')
        self.configure(self.root/'catalog',succeeds=False)


if __name__ == '__main__':
    unittest.main()

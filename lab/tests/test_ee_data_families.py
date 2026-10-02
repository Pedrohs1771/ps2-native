import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'lab'))


class EeDataFamilyTests(unittest.TestCase):
    def setUp(self):
        spec = importlib.util.spec_from_file_location('ee_data_families', ROOT / 'lab/discover_ee_data_families.py')
        self.tool = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.tool)
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)

    def tearDown(self):
        self.temp.cleanup()

    def case(self, name, words, base=0x10000):
        case = self.root / name
        case.mkdir()
        image = struct.pack('<' + 'I' * len(words), *words)
        (case / 'snapshot.bin').write_bytes(image)
        (case / 'bank.json').write_text(json.dumps({
            'schema_version': 1, 'base': base, 'entry': base, 'image_bytes': len(image),
            'image_sha256': hashlib.sha256(image).hexdigest(),
            'bindings': [{'address': pc, 'source_begin': base, 'source_bytes': len(image)}
                         for pc in range(base, base + len(image), 4)]}))
        return case

    def variants(self):
        return [self.case('a', [0x3C010172, 0xAC22526C, 0x03E00008, 0]),
                self.case('b', [0x3C010185, 0xAC22F8C4, 0x03E00008, 0], 0x20000)]

    def test_typed_fields_preserve_structure_and_roundtrip_observed_bytes(self):
        cases = self.variants()
        report = self.tool.discover(cases)
        self.assertEqual(report['analyzer_sha256'], hashlib.sha256(
            (ROOT / 'lab/discover_ee_data_families.py').read_bytes()).hexdigest())
        self.assertEqual(len(report['families']), 1)
        family = report['families'][0]
        self.assertEqual(family['guard_masks'], [0xFFFF0000, 0xFFFF0000, 0xFFFFFFFF, 0xFFFFFFFF])
        self.assertEqual([p['kind'] for p in family['parameters']], ['lui-u16', 'sw-s16'])
        self.assertEqual([o['parameters'] for o in family['observations']], [[0x172, 0x526C], [0x185, -1852]])
        for observation, case in zip(family['observations'], cases):
            reconstructed = list(family['guard_words'])
            for parameter, value in zip(family['parameters'], observation['parameters']):
                reconstructed[parameter['word_index']] |= value & 0xFFFF
            self.assertEqual(struct.pack('<4I', *reconstructed), (case / 'snapshot.bin').read_bytes())
        for gate in ['strict_approval', 'closure_proved', 'producer_invariant_proved', 'native_execution_validated']:
            self.assertFalse(report[gate])

    def test_opcode_register_and_flow_variations_do_not_merge(self):
        reference = [0x3C010172, 0xAC22526C, 0x1000FFFC, 0]
        first = self.case('first', reference)
        for index, replacement in [(0, 0x34010185), (0, 0x3C020185),
                                   (1, 0xAC23F8C4), (1, 0xAC42F8C4),
                                   (2, 0x1000FFFD), (2, 0x08010000)]:
            changed = reference.copy()
            changed[index] = replacement
            case = self.case('variant-' + str(index) + '-' + str(replacement), changed, 0x20000)
            self.assertEqual(self.tool.discover([first, case])['families'], [])

    def test_reserved_lui_source_register_is_not_a_parameter(self):
        a = self.case('a', [0x3C210001, 0x03E00008, 0])
        b = self.case('b', [0x3C210002, 0x03E00008, 0], 0x20000)
        self.assertEqual(self.tool.discover([a, b])['families'], [])

    def test_fixed_fields_remain_exact_and_duplicate_snapshots_are_not_new_variants(self):
        a = self.case('a', [0x3C010001, 0xAC220010, 0x03E00008, 0])
        b = self.case('b', [0x3C010002, 0xAC220010, 0x03E00008, 0], 0x20000)
        copy = self.case('copy', [0x3C010001, 0xAC220010, 0x03E00008, 0])
        family = self.tool.discover([a, b, copy])['families'][0]
        self.assertEqual(family['guard_masks'][1], 0xFFFFFFFF)
        self.assertEqual(len(family['parameters']), 1)
        self.assertEqual(len(family['observations']), 2)
        self.assertEqual(self.tool.discover([a, copy])['families'], [])

    def test_unsigned_and_signed_boundaries(self):
        a = self.case('a', [0x3C010000, 0xAC220000, 0])
        b = self.case('b', [0x3C01FFFF, 0xAC228000, 0], 0x20000)
        c = self.case('c', [0x3C017FFF, 0xAC227FFF, 0], 0x30000)
        family = self.tool.discover([a, b, c])['families'][0]
        self.assertEqual([o['parameters'] for o in family['observations']],
                         [[0, 0], [65535, -32768], [32767, 32767]])

    def test_all_supported_data_fields_preserve_instruction_and_register_bits(self):
        opcodes=[0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,0x20,0x21,0x23,0x24,0x25,
                 0x27,0x37,0x1e,0x28,0x29,0x2b,0x3f,0x1f]
        for opcode in opcodes:
            word=(opcode<<26)|((0 if opcode==0x0f else 5)<<21)|(2<<16)
            cases=[self.case(f'{opcode}-a',[word,0x03e00008,0]),
                   self.case(f'{opcode}-b',[word|0x8000,0x03e00008,0],0x20000)]
            family=self.tool.discover(cases)['families'][0]
            with self.subTest(opcode=opcode):
                self.assertEqual(family['guard_masks'],[0xffff0000,0xffffffff,0xffffffff])
                self.assertEqual(family['normal_entry_offsets'],[0,4,8])
                self.assertEqual(family['observations'][1]['parameters'],
                                 [0x8000 if opcode in [0x0c,0x0d,0x0e,0x0f] else -32768])
        for opcode in [1,4,5,6,7,0x14,0x15,0x16,0x17,0x10,0x11,0x12,0x18,0x19]:
            self.assertIsNone(self.tool.parameter_kind(opcode<<26))

    def test_entry_ownership_comes_from_binding_metadata(self):
        cases=self.variants()
        for case in cases:
            path=case/'bank.json';metadata=json.loads(path.read_text());metadata['bindings']=metadata['bindings'][:1]
            path.write_text(json.dumps(metadata))
        family=self.tool.discover(cases)['families'][0]
        self.assertEqual(family['normal_entry_offsets'],[0])
        self.assertEqual([o['normal_entry_offsets'] for o in family['observations']],[[0],[0]])

    def test_single_observation_batch_keeps_typed_constants_and_fixed_control(self):
        case=self.case('single',[0x27bdffc0,0xffbf0030,0x14430030,0])
        self.assertEqual(self.tool.discover([case])['families'],[])
        report=self.tool.discover([case],minimum_variants=1,operand_policy='typed',
                                  root_only=True,terminal_only=True)
        self.assertEqual(report['discovery_policy'],{'minimum_variants':1,'operand_policy':'typed',
                                                     'root_only':True,'terminal_only':True,'region_policy':'metadata'})
        family=report['families'][0]
        self.assertEqual(family['guard_words'],[0x27bd0000,0xffbf0000,0x14430030,0])
        self.assertEqual(family['guard_masks'],[0xffff0000,0xffff0000,0xffffffff,0xffffffff])
        self.assertEqual(family['normal_entry_offsets'],[0])
        self.assertEqual(family['observations'][0]['parameters'],[-64,48])
        self.assertFalse(report['producer_invariant_proved'])

    def test_batch_filtering_records_nonterminal_and_interior_omissions(self):
        terminal=self.case('terminal',[0x03e00008,0])
        linear=self.case('linear',[0x24020001,0])
        report=self.tool.discover([terminal,linear],minimum_variants=1,
                                  root_only=True,terminal_only=True)
        self.assertEqual(len(report['families']),1)
        self.assertEqual(report['counts']['nonterminal_regions_skipped'],1)
        self.assertEqual(report['counts']['nonroot_bindings_skipped'],2)

    def test_single_observation_exact_policy_does_not_generalize_operands(self):
        case=self.case('exact',[0x3c020040,0x03e00008,0])
        family=self.tool.discover([case],minimum_variants=1)['families'][0]
        self.assertEqual(family['guard_masks'],[0xffffffff]*3)
        self.assertEqual(family['parameters'],[])

    def test_invalid_discovery_policy_and_family_budget_fail_explicitly(self):
        cases=self.variants()
        for settings in [{'minimum_variants':True},{'minimum_variants':0},
                         {'operand_policy':'anything'},{'root_only':1},{'terminal_only':'yes'}]:
            with self.subTest(settings=settings),self.assertRaises(ValueError):
                self.tool.discover(cases,**settings)
        self.tool.MAX_CANDIDATES=0
        with self.assertRaises(ValueError):
            self.tool.discover(cases,minimum_variants=1)

    def canonical(self,cases):
        return self.tool.discover(cases,minimum_variants=1,operand_policy='typed',
                                  root_only=True,region_policy='canonical-v1')

    def test_canonical_regions_cross_metadata_linear_boundaries(self):
        case=self.case('split',[0x3c020001,0x24420002,0xac820000,
                                0x24420003,0x03e00008,0])
        path=case/'bank.json';metadata=json.loads(path.read_text());base=metadata['base']
        metadata['bindings']=[{'address':base,'source_begin':base,'source_bytes':12},
                              {'address':base+12,'source_begin':base+12,'source_bytes':12}]
        path.write_text(json.dumps(metadata))
        report=self.canonical([case])
        self.assertEqual(sorted(f['word_count'] for f in report['families']),[3,6])
        self.assertEqual(report['discovery_policy']['region_policy'],'canonical-v1')
        self.assertEqual(report['counts']['canonical_terminal_regions'],2)
        self.assertTrue(all(f['normal_entry_offsets']==[0] for f in report['families']))

    def test_canonical_discovers_return_continuation_after_indirect_call(self):
        case=self.case('call',[0x24420001,0x0040f809,0,0x24630002,0x03e00008,0])
        report=self.canonical([case])
        self.assertEqual({o['pc'] for f in report['families'] for o in f['observations']},
                         {0x10000,0x1000c})
        self.assertEqual(report['counts']['canonical_successor_roots'],1)

    def test_canonical_discovers_both_conditional_successors(self):
        case=self.case('conditional',[0x10430003,0,0x03e00008,0,
                                      0x24420001,0x03e00008,0])
        report=self.canonical([case])
        self.assertEqual({o['pc'] for f in report['families'] for o in f['observations']},
                         {0x10000,0x10008,0x10010})
        self.assertEqual(report['counts']['canonical_successor_roots'],2)

    def test_canonical_discovers_linear_successor_without_an_extra_binding(self):
        case=self.case('linear',[0x24420001]*127+[0x03e00008,0])
        report=self.canonical([case])
        self.assertEqual(sorted(f['word_count'] for f in report['families']),[2,127])
        self.assertEqual(report['counts']['canonical_successor_roots'],1)

    def test_canonical_direct_jump_queues_target_without_false_fallthrough(self):
        case=self.case('jump',[0x08004004,0,0x24420001,0,0x03e00008,0])
        report=self.canonical([case])
        self.assertEqual({o['pc'] for f in report['families'] for o in f['observations']},
                         {0x10000,0x10010})

    def test_canonical_call_records_external_target_and_internal_return(self):
        case=self.case('external-call',[0x0c008000,0,0x03e00008,0])
        report=self.canonical([case])
        self.assertEqual({o['pc'] for f in report['families'] for o in f['observations']},
                         {0x10000,0x10008})
        self.assertEqual(report['counts']['canonical_external_successors'],1)

    def test_canonical_requested_interior_entry_is_an_independent_root(self):
        case=self.case('resume',[0x24420001,0x0040f809,0,0x24630002,0x03e00008,0])
        path=case/'bank.json';metadata=json.loads(path.read_text());metadata['entry']=0x1000c
        path.write_text(json.dumps(metadata))
        report=self.canonical([case])
        self.assertEqual({o['pc'] for f in report['families'] for o in f['observations']},
                         {0x10000,0x1000c})

    def test_canonical_backedge_is_deduplicated_and_keeps_fallthrough(self):
        case=self.case('loop',[0x1043ffff,0,0x03e00008,0])
        report=self.canonical([case])
        self.assertEqual(report['counts']['regions'],2)
        self.assertEqual(report['counts']['canonical_successor_roots'],1)
        self.assertEqual({o['pc'] for f in report['families'] for o in f['observations']},
                         {0x10000,0x10008})

    def test_canonical_jalr_without_link_does_not_invent_a_return(self):
        case=self.case('no-link',[0x00400009,0,0x03e00008,0])
        report=self.canonical([case])
        self.assertEqual({o['pc'] for f in report['families'] for o in f['observations']},
                         {0x10000})
        self.assertEqual(report['counts']['canonical_successor_roots'],0)

    def test_canonical_linear_and_transfer_boundary_are_prefix_disjoint(self):
        cases=[self.case('linear',[0x24420001]*130),
               self.case('branch-at-126',[0x24420002]*126+[0x03e00008,0]),
               self.case('branch-at-127',[0x24420003]*127+[0x03e00008,0])]
        report=self.canonical(cases)
        self.assertEqual(sorted(f['word_count'] for f in report['families']),[2,127,128])
        self.assertEqual(report['counts']['canonical_linear_regions'],2)
        self.assertEqual(report['counts']['canonical_terminal_regions'],2)
        for index,a in enumerate(report['families']):
            for b in report['families'][index+1:]:
                self.assertTrue(any(((x^y)&mx&my)!=0 for x,y,mx,my in
                    zip(a['guard_words'],b['guard_words'],a['guard_masks'],b['guard_masks'])))

    def test_canonical_truncated_windows_are_explicit_obligations(self):
        report=self.canonical([self.case('short',[0x24420001]*4),
                               self.case('no-slot',[0x24420001,0x03e00008])])
        self.assertEqual(report['families'],[])
        self.assertEqual(report['counts']['truncated_linear_regions_skipped'],1)
        self.assertEqual(report['counts']['truncated_terminal_regions_skipped'],1)

    def test_canonical_reserved_and_coprocessor_control_remain_fixed(self):
        report=self.canonical([self.case('cop',[0x24420001,0x45010003,0,0])])
        family=report['families'][0]
        self.assertEqual(family['word_count'],3)
        self.assertEqual(family['guard_masks'],[0xffff0000,0xffffffff,0xffffffff])
        self.assertFalse(report['native_execution_validated'])

    def test_canonical_policy_rejects_incompatible_entry_and_operand_domains(self):
        case=self.case('a',[0x03e00008,0])
        for updates in [{'root_only':False},{'terminal_only':True},
                        {'operand_policy':'observed'},{'region_policy':'unknown'}]:
            policy={'minimum_variants':1,'operand_policy':'typed','root_only':True,
                    'region_policy':'canonical-v1',**updates}
            with self.subTest(updates=updates),self.assertRaises(ValueError):
                self.tool.discover([case],**policy)

    def test_compact_canonical_publication_roundtrips_owned_provenance(self):
        cases=self.variants();output=self.root/'compact.json'
        original=self.canonical(cases)
        result=self.tool.write_report(cases,output,minimum_variants=1,operand_policy='typed',
                                      root_only=True,region_policy='canonical-v1')
        self.assertEqual(result['schema_version'],2)
        self.assertNotIn('observations',result['families'][0])
        self.assertEqual(result['families'][0]['observation_count'],2)
        loaded=self.tool.read_report(output,observations=True)
        self.assertEqual(loaded['families'],original['families'])
        self.assertEqual(len(result['provenance']['origins']),2)

    def test_compact_word_count_is_derived_from_guard_dimensions(self):
        output=self.root/'derived.json'
        original=self.canonical(self.variants())
        result=self.tool.write_report([self.root/'a',self.root/'b'],output,
            minimum_variants=1,operand_policy='typed',root_only=True,region_policy='canonical-v1')
        self.assertNotIn('word_count',result['families'][0])
        loaded=self.tool.read_report(output,observations=True)
        self.assertEqual(loaded['families'],original['families'])
        result['families'][0]['word_count']=3
        output.write_text(json.dumps(result))
        with self.assertRaisesRegex(ValueError,'invalid compact family dimensions'):
            self.tool.read_report(output)
        self.assertGreater(len(result['provenance']['shards']),0)
        shard=self.root/result['provenance']['shards'][0]['name']
        shard.write_bytes(shard.read_bytes()+b' ')
        with self.assertRaises(ValueError):self.tool.read_report(output)

    def test_compact_publication_splits_provenance_and_fails_without_manifest(self):
        cases=[self.case('many-'+str(i),[0x3c010001+i,0x03e00008,0],0x10000+i*0x1000)
               for i in range(12)]
        self.tool.MAX_PROVENANCE_SHARD_BYTES=1024
        result=self.tool.write_report(cases,self.root/'compact.json',minimum_variants=1,
            operand_policy='typed',root_only=True,region_policy='canonical-v1')
        self.assertGreater(len(result['provenance']['shards']),1)
        self.assertTrue(all(row['bytes']<=1024 for row in result['provenance']['shards']))
        loaded=self.tool.read_report(self.root/'compact.json',observations=True)
        self.assertEqual(len(loaded['families'][0]['observations']),12)
        self.tool.MAX_PROVENANCE_BYTES=1
        with self.assertRaises(ValueError):
            self.tool.write_report(cases,self.root/'failed.json',minimum_variants=1,
                operand_policy='typed',root_only=True,region_policy='canonical-v1')
        self.assertFalse((self.root/'failed.json').exists())

    def test_deterministic_input_order(self):
        cases = self.variants()
        self.assertEqual(self.tool.discover(cases), self.tool.discover(list(reversed(cases))))

    def test_bad_hash_types_or_binding_ranges_are_rejected(self):
        for patch in [{'base': True}, {'schema_version': '1'}, {'entry': 0x10002},
                      {'image_sha256': '0' * 64}, {'image_bytes': 4},
                      {'bindings': [{'address': 0x10000, 'source_begin': 0x10000, 'source_bytes': 100}]}]:
            case = self.case('case-' + str(len(list(self.root.iterdir()))), [0x3C010001, 0])
            metadata = json.loads((case / 'bank.json').read_text())
            metadata.update(patch)
            (case / 'bank.json').write_text(json.dumps(metadata))
            with self.subTest(patch=patch), self.assertRaises(ValueError):
                self.tool.discover([case])

    def test_empty_case_list_and_scan_budget_are_rejected(self):
        with self.assertRaises(ValueError):
            self.tool.discover([])
        self.tool.MAX_SCANNED_WORDS = 1
        with self.assertRaises(ValueError):
            self.tool.discover(self.variants())

    def test_oversized_regions_are_explicitly_skipped(self):
        cases = [self.case('a', [0x3C010001] * 129),
                 self.case('b', [0x3C010002] * 129, 0x20000)]
        report = self.tool.discover(cases)
        self.assertEqual(report['families'], [])
        self.assertEqual(report['counts']['oversized_regions_skipped'], 2)

    def test_unsorted_duplicate_or_missing_entry_bindings_are_rejected(self):
        case = self.case('a', [0x3C010001, 0, 0])
        original = json.loads((case / 'bank.json').read_text())
        rows = original['bindings']
        for bad in [list(reversed(rows)), [rows[0], rows[0]], rows[1:]]:
            metadata = original.copy()
            metadata['bindings'] = bad
            (case / 'bank.json').write_text(json.dumps(metadata))
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                self.tool.discover([case])

    def test_aggregate_metadata_budget_is_enforced(self):
        cases = self.variants()
        self.tool.MAX_TOTAL_METADATA_BYTES = (cases[0] / 'bank.json').stat().st_size
        with self.assertRaises(ValueError):
            self.tool.discover(cases)

    def test_cli_publishes_only_a_fresh_candidate_report(self):
        cases = self.variants()
        output = self.root / 'cli.json'
        command = [sys.executable, str(ROOT / 'lab/discover_ee_data_families.py'),
                   '--case', str(cases[0]), '--case', str(cases[1]), '--output', str(output)]
        result = subprocess.run(command, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout)['family_candidates'], 1)
        self.assertEqual(json.loads(output.read_text())['status'], 'CANDIDATES_LABORATORY')
        before = output.read_bytes()
        repeat = subprocess.run(command, capture_output=True, text=True)
        self.assertNotEqual(repeat.returncode, 0)
        self.assertEqual(output.read_bytes(), before)

    def test_report_budget_fails_before_publication(self):
        output=self.root/'too-large.json'
        self.tool.MAX_REPORT_BYTES=1
        with self.assertRaises(ValueError):self.tool.write_report(self.variants(),output)
        self.assertFalse(output.exists())

    def test_empty_canonical_publication_is_rejected_before_writing(self):
        case=self.case('single',[0x24420001,0x03e00008,0])
        output=self.root/'empty.json'
        with self.assertRaisesRegex(ValueError,'canonical publication has no candidate families'):
            self.tool.write_report([case],output,minimum_variants=2,operand_policy='typed',
                                   root_only=True,region_policy='canonical-v1')
        self.assertFalse(output.exists())
        self.assertEqual(list(self.root.glob('ee-candidate-provenance-*.json')),[])

    def test_invalid_input_does_not_publish_and_cli_has_no_traceback(self):
        cases = self.variants()
        (cases[1] / 'snapshot.bin').write_bytes(bytes(16))
        output = self.root / 'invalid.json'
        command = [sys.executable, str(ROOT / 'lab/discover_ee_data_families.py'),
                   '--case', str(cases[0]), '--case', str(cases[1]), '--output', str(output)]
        result = subprocess.run(command, capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertNotIn('Traceback', result.stderr)
        self.assertFalse(output.exists())

    def test_symlinked_parent_is_rejected(self):
        case = self.variants()[0]
        parent = self.root / 'parent'
        parent.symlink_to(self.root, target_is_directory=True)
        with self.assertRaises(ValueError):
            self.tool.discover([parent / case.name])

    def test_linked_inputs_and_reused_output_are_rejected(self):
        cases = self.variants()
        linked = self.root / 'linked'
        linked.symlink_to(cases[0], target_is_directory=True)
        with self.assertRaises(ValueError):
            self.tool.discover([linked])
        output = self.root / 'report.json'
        self.tool.write_report(cases, output)
        original = output.read_bytes()
        with self.assertRaises(FileExistsError):
            self.tool.write_report(cases, output)
        self.assertEqual(output.read_bytes(), original)
        dangling = self.root / 'dangling.json'
        dangling.symlink_to(self.root / 'absent.json')
        with self.assertRaises(ValueError):
            self.tool.write_report(cases, dangling)

    def test_linked_snapshot_and_catalog_manifest_are_rejected(self):
        case = self.case('a', [0x3C010001, 0])
        snapshot = case / 'snapshot.bin'
        moved = self.root / 'moved.bin'
        snapshot.rename(moved)
        snapshot.symlink_to(moved)
        with self.assertRaises(ValueError):
            self.tool.discover([case])
        catalog = self.root / 'catalog'
        catalog.mkdir()
        (catalog / 'catalog.json').symlink_to(case / 'bank.json')
        output = self.root / 'report.json'
        result = subprocess.run([sys.executable, str(ROOT / 'lab/discover_ee_data_families.py'),
                                 '--catalog', str(catalog), '--output', str(output)],
                                capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('linked paths', result.stderr)
        self.assertFalse(output.exists())


if __name__ == '__main__':
    unittest.main()

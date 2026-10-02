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

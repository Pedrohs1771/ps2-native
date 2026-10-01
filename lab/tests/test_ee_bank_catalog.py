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
SPEC = importlib.util.spec_from_file_location('ee_catalog', ROOT / 'lab/generate_ee_bank_catalog.py')
CODEGEN = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CODEGEN)
GENERATOR = Path(sys.argv.pop(1)).resolve() if len(sys.argv) > 1 else ROOT / 'build/ps2xRecomp/ps2_native_overlay'


class EeCatalogTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)

    def tearDown(self):
        self.temporary.cleanup()

    def case(self, name, immediate):
        case = self.root / name
        case.mkdir()
        image = struct.pack('<III', 0x24020000 | immediate, 0x03E00008, 0x24420001)
        (case / 'snapshot.bin').write_bytes(image)
        metadata = {'schema_version': 1, 'base': 0x10000, 'entry': 0x10000,
                    'image_bytes': len(image), 'image_sha256': hashlib.sha256(image).hexdigest(),
                    'bindings': [{'address': pc, 'source_begin': 0x10000, 'source_bytes': 12}
                                 for pc in range(0x10000, 0x1000C, 4)]}
        (case / 'bank.json').write_text(json.dumps(metadata))
        return case

    def generate(self, cases, name='catalog'):
        return CODEGEN.generate(cases, GENERATOR, self.root / name, ROOT)

    def test_tampered_bytes_duplicate_banks_and_existing_outputs_fail(self):
        case = self.case('one', 42)
        with self.assertRaises(ValueError): self.generate([case, case])
        self.generate([case])
        with self.assertRaises(ValueError): self.generate([case])
        (case / 'snapshot.bin').write_bytes(b'\0' * 12)
        with self.assertRaises(ValueError): self.generate([case], 'bad')
        self.assertFalse((self.root / 'bad').exists())

    def test_recovered_binding_identity_must_agree_with_generated_code(self):
        case = self.case('one', 42)
        metadata = json.loads((case / 'bank.json').read_text())
        metadata['bindings'][0]['source_bytes'] = 8
        (case / 'bank.json').write_text(json.dumps(metadata))
        with self.assertRaises(ValueError): self.generate([case])

    def test_captured_case_generator_identity_must_match(self):
        case = self.case('one', 42)
        metadata = json.loads((case / 'bank.json').read_text())
        metadata['generator_sha256'] = '0' * 64
        (case / 'bank.json').write_text(json.dumps(metadata))
        with self.assertRaises(ValueError): self.generate([case])
        self.assertFalse((self.root / 'catalog').exists())

    def test_malformed_metadata_and_excessive_case_counts_fail_before_generation(self):
        case = self.case('one', 42)
        valid = json.loads((case / 'bank.json').read_text())
        for changed in ([], {**valid, 'base': True}, {**valid, 'schema_version': '1'},
                        {**valid, 'bindings': [None]}):
            (case / 'bank.json').write_text(json.dumps(changed))
            with self.subTest(metadata=changed), self.assertRaises(ValueError): self.generate([case])
            self.assertFalse((self.root / 'catalog').exists())
        with self.assertRaises(ValueError): self.generate([self.root / 'absent'] * 513)

    def test_input_order_does_not_change_catalog(self):
        one, two = self.case('one', 42), self.case('two', 7)
        first = self.generate([one, two], 'first')
        second = self.generate([two, one], 'second')
        self.assertEqual(first, second)
        for name in first['sources']:
            self.assertEqual((self.root / 'first' / name).read_bytes(),
                             (self.root / 'second' / name).read_bytes())

    def test_extension_preserves_existing_bank_bytes_and_timestamps(self):
        one, two = self.case('one', 42), self.case('two', 7)
        first = self.generate([one])
        output = self.root / 'catalog'
        source = output / first['sources'][0]
        before = (source.read_bytes(), source.stat().st_mtime_ns)
        second = CODEGEN.extend_catalog([one, two], GENERATOR, output)
        self.assertEqual(len(second['banks']), 2)
        self.assertEqual((source.read_bytes(), source.stat().st_mtime_ns), before)
        manifest_before = (output / 'catalog.json').read_bytes()
        with self.assertRaises(ValueError): CODEGEN.extend_catalog([two], GENERATOR, output)
        self.assertEqual((output / 'catalog.json').read_bytes(), manifest_before)
        source.write_text('// tampered\n')
        with self.assertRaises(ValueError): CODEGEN.extend_catalog([one, two], GENERATOR, output)
        self.assertEqual((output / 'catalog.json').read_bytes(), manifest_before)

    def test_extension_rejects_malformed_manifests_and_unrecorded_artifacts(self):
        one, two = self.case('one', 42), self.case('two', 7)
        first = self.generate([one])
        output = self.root / 'catalog'
        valid = (output / 'catalog.json').read_bytes()
        for patch in ({'schema_version': True}, {'generator_sha256': '0' * 64},
                      {'sources': [None, 'ee_catalog.cpp']},
                      {'sources': first['sources'] * 2}):
            (output / 'catalog.json').write_text(json.dumps({**first, **patch}))
            with self.subTest(patch=patch), self.assertRaises(ValueError):
                CODEGEN.extend_catalog([one, two], GENERATOR, output)
        (output / 'catalog.json').write_bytes(valid)
        next_manifest = self.generate([one, two], 'other')
        new_source = next(name for name in next_manifest['sources'] if name not in first['sources'])
        (output / new_source).write_text('preserve unrecorded artifact')
        with self.assertRaises(ValueError): CODEGEN.extend_catalog([one, two], GENERATOR, output)
        self.assertEqual((output / new_source).read_text(), 'preserve unrecorded artifact')
        self.assertEqual((output / 'catalog.json').read_bytes(), valid)

    def test_generated_native_functions_execute_and_reject_stale_versions(self):
        one, two = self.case('one', 42), self.case('two', 7)
        manifest = self.generate([one, two])
        source = self.root / 'main.cpp'
        source.write_text('''#include "ps2_ee_aot.h"
#include <cstring>
const ps2native::ee_aot::Program &compiledEeProgram();
int main() {
  using namespace ps2native::ee_aot;
  const auto &program = compiledEeProgram();
  Dispatcher dispatcher(program);
  std::vector<uint8_t> ram(PS2_RAM_SIZE);
  for (const auto &bank : program.banks) {
    std::memcpy(ram.data()+bank.base,bank.image.data(),bank.image.size());
    R5900Context ctx{}; ctx.pc=0x10000;
    ctx.r[31]=_mm_set_epi32(0,0,0,0x20000);
    auto result=dispatcher.lookup(ram.data(),ctx.pc);
    if (!result.function) return 1;
    result.function(ram.data(),&ctx,nullptr);
    if (_mm_extract_epi32(ctx.r[2],0)!=bank.image[0]+1 || ctx.pc!=0x20000) return 2;
    ctx.pc=0x10004; ctx.r[2]=_mm_set_epi32(0,0,0,100);
    result=dispatcher.lookup(ram.data(),ctx.pc);
    if (!result.function) return 3;
    result.function(ram.data(),&ctx,nullptr);
    if (_mm_extract_epi32(ctx.r[2],0)!=101 || ctx.pc!=0x20000) return 4;
    ctx.pc=0x10008;
    result=dispatcher.lookup(ram.data(),ctx.pc);
    if (!result.function) return 5;
    result.function(ram.data(),&ctx,nullptr);
    if (_mm_extract_epi32(ctx.r[2],0)!=102 || ctx.pc!=0x1000C) return 6;
    ram[0x1000B]^=1;
    if (dispatcher.lookup(ram.data(),0x10000).status!=Status::CodeChanged) return 7;
  }
  return dispatcher.lookup(ram.data(),0x1000C).status==Status::MissingEntry ? 0 : 8;
}
''')
        command = ['c++', '-std=c++20', '-O0', '-fno-lto', '-msse4.1',
                   '-I' + str(ROOT / 'ps2xRuntime/include'),
                   '-I' + str(ROOT / 'ps2xRuntime/src/lib'), '-I' + str(ROOT / 'ps2xIOP/include'),
                   '-I' + str(ROOT / 'ps2xRuntime/src/lib/Kernel'),
                   str(source), str(ROOT / 'ps2xRuntime/src/lib/ps2_ee_aot.cpp')]
        command += [str(self.root / 'catalog' / name) for name in manifest['sources']]
        command += ['-o', str(self.root / 'fixture')]
        result = subprocess.run(command, capture_output=True, text=True, timeout=60)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        result = subprocess.run([str(self.root / 'fixture')], timeout=10)
        self.assertEqual(result.returncode, 0)

    def test_cmake_rejects_ambiguous_schema_and_changed_sources(self):
        manifest = self.generate([self.case('one', 42)])
        catalog = self.root / 'catalog/catalog.json'
        script = self.root / 'validate.cmake'
        script.write_text(f'include("{ROOT}/ps2xRuntime/cmake/ee_native_catalog.cmake")\n'
                          f'ee_catalog_sources("{catalog}" sources)\n')
        def check(expected):
            result = subprocess.run(['cmake', '-P', str(script)], capture_output=True, text=True, timeout=10)
            self.assertEqual(result.returncode == 0, expected, result.stdout + result.stderr)
        check(True)
        for changed in ({**manifest, 'schema_version': '1'},
                        {**manifest, 'sources': manifest['sources'][:-1]},
                        {**manifest, 'sources': [manifest['sources'][0]] * 2},
                        {**manifest, 'sources': ['../outside.cpp', 'ee_catalog.cpp']}):
            catalog.write_text(json.dumps(changed))
            check(False)
        catalog.write_text(json.dumps(manifest))
        (self.root / 'catalog' / manifest['sources'][0]).write_text('// tampered\n')
        check(False)


if __name__ == '__main__':
    unittest.main()

import hashlib
import importlib.util
import json
import os
from pathlib import Path
import struct
import subprocess
import shutil
import shlex
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

    def test_entry_dependency_runs_preserve_holes_backedges_and_slot_entries(self):
        rows = [{'address': 0x10000, 'source_begin': 0x10000, 'source_bytes': 16},
                {'address': 0x10004, 'source_begin': 0x10004, 'source_bytes': 12},
                {'address': 0x10008, 'source_begin': 0x10004, 'source_bytes': 12},
                {'address': 0x1000C, 'source_begin': 0x1000C, 'source_bytes': 4},
                {'address': 0x10014, 'source_begin': 0x10014, 'source_bytes': 4}]
        runs = CODEGEN.dependency_runs(rows)
        restored = []
        for start, end, begin, stop in runs:
            for address in range(start, end, 4):
                source = address if begin == 0xFFFFFFFF else begin
                restored.append({'address': address, 'source_begin': source, 'source_bytes': stop - source})
        self.assertEqual(restored, rows)

    def test_migration_updates_index_only_and_preserves_producer_identity(self):
        case = self.case('one', 42)
        old_generator = self.root / 'old-generator'
        shutil.copyfile(GENERATOR, old_generator)
        with old_generator.open('ab') as stream: stream.write(b'\0identified-previous-producer')
        old_generator.chmod(0o700)
        first = CODEGEN.generate([case], old_generator, self.root / 'catalog', normal_entries=False)
        source = self.root / 'catalog' / first['sources'][0]
        before = (source.read_bytes(), source.stat().st_mtime_ns)
        metadata = json.loads((case / 'bank.json').read_text())
        metadata['generator_sha256'] = first['generator_sha256']
        (case / 'bank.json').write_text(json.dumps(metadata))
        with self.assertRaises(ValueError): CODEGEN.extend_catalog([case], GENERATOR, self.root / 'catalog')
        second = CODEGEN.extend_catalog([case], GENERATOR, self.root / 'catalog', migrate_entry_guards=True)
        self.assertEqual((source.read_bytes(), source.stat().st_mtime_ns), before)
        self.assertEqual(second['dependency_contract'], 'normal-entry-v1')
        self.assertEqual(second['previous_generator_sha256'], first['generator_sha256'])
        self.assertEqual(second['generator_sha256'], hashlib.sha256(GENERATOR.read_bytes()).hexdigest())
        # The next ordinary extension must retain the captured old producer pin.
        two = self.case('two', 7)
        third = CODEGEN.extend_catalog([case, two], GENERATOR, self.root / 'catalog')
        self.assertEqual(len(third['banks']), 2)
        self.assertEqual(third['case_producers'][source.name], first['generator_sha256'])
        self.assertEqual((source.read_bytes(), source.stat().st_mtime_ns), before)
        # A second extension must not rely on a one-generation global exception.
        fourth = CODEGEN.extend_catalog([case, two], GENERATOR, self.root / 'catalog')
        self.assertEqual(fourth, third)
        foreign = self.case('foreign', 99)
        foreign_metadata = json.loads((foreign / 'bank.json').read_text())
        foreign_metadata['generator_sha256'] = first['generator_sha256']
        (foreign / 'bank.json').write_text(json.dumps(foreign_metadata))
        with self.assertRaises(ValueError):
            CODEGEN.extend_catalog([case, two, foreign], GENERATOR, self.root / 'catalog')

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

    def test_owned_case_inputs_allow_future_extensions_without_external_case_paths(self):
        one = self.case('one', 42)
        manifest = self.generate([one])
        output = self.root / 'catalog'
        shutil.rmtree(one)
        cases = CODEGEN.owned_cases(output)
        self.assertEqual(len(cases), 1)
        bank = output / manifest['sources'][0]
        before = (bank.read_bytes(), bank.stat().st_mtime_ns)
        two = self.case('two', 7)
        extended = CODEGEN.extend_catalog([*cases, two], GENERATOR, output)
        self.assertEqual(len(extended['case_inputs']), 2)
        self.assertEqual((bank.read_bytes(), bank.stat().st_mtime_ns), before)
        case = CODEGEN.owned_cases(output)[0]
        (case / 'snapshot.bin').write_bytes(b'changed')
        with self.assertRaises(ValueError): CODEGEN.owned_cases(output)

    def test_owned_case_ledger_rejects_escaping_paths_and_symlinks(self):
        manifest = self.generate([self.case('one', 42)])
        output = self.root / 'catalog'
        name = manifest['sources'][0]
        catalog = output / 'catalog.json'
        saved = catalog.read_bytes()
        for patch in ({'directory': '../outside'}, {'metadata_sha256': '0' * 64}):
            changed = json.loads(saved)
            changed['case_inputs'][name].update(patch)
            catalog.write_text(json.dumps(changed))
            with self.assertRaises(ValueError): CODEGEN.owned_cases(output)
        catalog.write_bytes(saved)
        case = CODEGEN.owned_cases(output)[0]
        data = (case / 'bank.json').read_bytes()
        (case / 'bank.json').unlink()
        outside = self.root / 'external.json'; outside.write_bytes(data)
        (case / 'bank.json').symlink_to(outside)
        with self.assertRaises(ValueError): CODEGEN.owned_cases(output)

    def test_legacy_input_parent_conflict_fails_before_any_publication(self):
        one, two = self.case('one', 42), self.case('two', 7)
        manifest = self.generate([one])
        output = self.root / 'catalog'
        manifest.pop('case_inputs')
        (output / 'catalog.json').write_text(json.dumps(manifest))
        shutil.rmtree(output / 'ee_cases')
        (output / 'ee_cases').write_text('preserve unrecorded artifact')
        before = {path.name: path.read_bytes() for path in output.iterdir()}
        with self.assertRaises(ValueError): CODEGEN.extend_catalog([one, two], GENERATOR, output)
        self.assertEqual({path.name: path.read_bytes() for path in output.iterdir()}, before)

    def test_guard_migration_refuses_any_callback_source_rewrite(self):
        case = self.case('one', 42)
        output = self.root / 'catalog'
        CODEGEN.generate([case], GENERATOR, output, normal_entries=False)
        before = {str(path.relative_to(output)): (path.read_bytes(), path.stat().st_mtime_ns)
                  for path in output.rglob('*') if path.is_file()}
        changed = self.root / 'changed-generator'
        changed.write_text('#!' + sys.executable + '\n'
                           'import subprocess,sys\nfrom pathlib import Path\n'
                           'subprocess.run([' + repr(str(GENERATOR)) + ',*sys.argv[1:]],check=True)\n'
                           'p=Path(sys.argv[4])\n'
                           'p.write_text(p.read_text()+"\\n// changed callback producer\\n")\n')
        changed.chmod(0o700)
        with self.assertRaisesRegex(ValueError, 'rewrite'):
            CODEGEN.extend_catalog([case], changed, output, migrate_entry_guards=True)
        self.assertEqual({str(path.relative_to(output)): (path.read_bytes(), path.stat().st_mtime_ns)
                          for path in output.rglob('*') if path.is_file()}, before)

    def test_extension_rejects_malformed_manifests_and_unrecorded_artifacts(self):
        one, two = self.case('one', 42), self.case('two', 7)
        first = self.generate([one])
        output = self.root / 'catalog'
        valid = (output / 'catalog.json').read_bytes()
        for patch in ({'schema_version': True}, {'generator_sha256': '0' * 64},
                      {'case_producers': {first['sources'][0]: True}},
                      {'case_producers': {'ee_catalog.cpp': '0' * 64}},
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
    ram[0x10000]^=1; // Changed instruction is skipped by this normal entry.
    result=dispatcher.lookup(ram.data(),ctx.pc);
    if (!result.function) return 3;
    result.function(ram.data(),&ctx,nullptr);
    if (_mm_extract_epi32(ctx.r[2],0)!=101 || ctx.pc!=0x20000) return 4;
    if (dispatcher.lookup(ram.data(),0x10000).status!=Status::CodeChanged) return 9;
    ram[0x10000]^=1;
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

    def test_runtime_adapter_rechecks_code_and_preserves_rejected_contexts(self):
        one = self.case('one', 42)
        loop = self.root / 'loop'
        loop.mkdir()
        image = struct.pack('<IIII', 0, 0x2508FFFF, 0x1D00FFFE, 0)
        (loop / 'snapshot.bin').write_bytes(image)
        (loop / 'bank.json').write_text(json.dumps({
            'schema_version': 1, 'base': 0x20000, 'entry': 0x20000,
            'image_bytes': len(image), 'image_sha256': hashlib.sha256(image).hexdigest(),
            'bindings': [{'address': pc, 'source_begin': 0x20000, 'source_bytes': 16}
                         for pc in range(0x20000, 0x20010, 4)]}))
        manifest = self.generate([one, loop])
        source = self.root / 'adapter.cpp'
        source.write_text('''#include "ps2_ee_aot.h"
#include "ps2_ee_overlay_backend.h"
#include "ps2_native_overlay.h"
#include <array>
#include <cstring>
const ps2native::ee_aot::Program &compiledEeProgram();
int main() {
  PS2Runtime runtime;
  if (!runtime.memory().initialize()) return 1;
  auto *ram=runtime.memory().getRDRAM();
  for (const auto &bank:compiledEeProgram().banks)
    std::memcpy(ram+bank.base,bank.image.data(),bank.image.size());
  R5900Context ctx{};ctx.pc=0x10000;ctx.r[31]=_mm_set_epi32(0,0,0,0x30000);
  auto callback=ps2xResolveNativeOverlay(&runtime,ram,ctx.pc);
  if (!callback) return 2;
  callback(ram,&ctx,&runtime);
  if (runtime.isStopRequested() || _mm_extract_epi32(ctx.r[2],0)!=43 || ctx.pc!=0x30000) return 3;
  ctx.pc=0x10004;ctx.r[2]=_mm_set_epi32(0,0,0,100);ram[0x10000]^=1;
  callback=ps2xResolveNativeOverlay(&runtime,ram,ctx.pc);
  if (!callback) return 4;
  callback(ram,&ctx,&runtime);
  if (_mm_extract_epi32(ctx.r[2],0)!=101 || ctx.pc!=0x30000) return 5;
  ctx.pc=0x20008;ctx.r[8]=_mm_set_epi32(0,0,0,3);ram[0x20000]=1;
  callback=ps2xResolveNativeOverlay(&runtime,ram,ctx.pc);
  if (!callback) return 6;
  callback(ram,&ctx,&runtime);
  if (ctx.pc!=0x20010 || _mm_extract_epi32(ctx.r[8],0)!=0) return 7;
  ram[0x20004]^=1;
  if (ps2xResolveNativeOverlay(&runtime,ram,0x20008)) return 8;
  // A standalone slot must not repeat its preceding conditional branch.
  ctx.pc=0x2000C;ctx.r[8]=_mm_set_epi32(0,0,0,3);
  callback=ps2xResolveNativeOverlay(&runtime,ram,ctx.pc);
  if (!callback) return 9;
  callback(ram,&ctx,&runtime);
  if (ctx.pc!=0x20010 || _mm_extract_epi32(ctx.r[8],0)!=3) return 10;
  // Bytes can change after resolution and before invocation.
  ctx.pc=0x10004;
  callback=ps2xResolveNativeOverlay(&runtime,ram,ctx.pc);
  if (!callback) return 11;
  ram[0x10004]^=1;
  std::array<uint8_t,sizeof(ctx)> before{};std::memcpy(before.data(),&ctx,sizeof(ctx));
  std::vector<uint8_t> memoryBefore(ram,ram+PS2_RAM_SIZE);
  callback(ram,&ctx,&runtime);
  if (!runtime.isStopRequested() || std::memcmp(before.data(),&ctx,sizeof(ctx)) ||
      std::memcmp(memoryBefore.data(),ram,PS2_RAM_SIZE)) return 12;
  ram[0x10004]^=1;
  // A pending architectural delay context is rejected without clearing it.
  PS2Runtime delayed;
  ctx.pc=0x10004;ctx.in_delay_slot=true;
  callback=ps2xResolveNativeOverlay(&delayed,ram,ctx.pc);
  if (!callback) return 13;
  std::memcpy(before.data(),&ctx,sizeof(ctx));
  memoryBefore.assign(ram,ram+PS2_RAM_SIZE);
  callback(ram,&ctx,&delayed);
  if (!delayed.isStopRequested() || !ctx.in_delay_slot ||
      std::memcmp(before.data(),&ctx,sizeof(ctx)) ||
      std::memcmp(memoryBefore.data(),ram,PS2_RAM_SIZE)) return 14;
  PS2Runtime missing;
  callback(ram,nullptr,&missing);
  return missing.isStopRequested() ? 0 : 15;
}
''')
        build = GENERATOR.parent.parent
        link = shlex.split((build / 'lab/CMakeFiles/nexo_ee_aot_tests.dir/link.txt').read_text())
        libraries = link[link.index('-o') + 2:]
        libraries = [str((build / 'lab' / item).resolve()) if not item.startswith('-') else item
                     for item in libraries]
        command = ['c++', '-std=c++20', '-O0', '-fno-lto', '-msse4.1',
                   '-DPS2X_RUNTIME_AOT_EE_OVERLAYS=1']
        command += ['-I' + str(ROOT / directory) for directory in
                    ('ps2xRuntime/include', 'ps2xRuntime/src/lib',
                     'ps2xRuntime/src/lib/Kernel', 'ps2xIOP/include')]
        command += [str(source), str(ROOT / 'lab/tests/empty_ee_table.cpp'),
                    str(ROOT / 'ps2xRuntime/src/lib/ps2_ee_overlay_backend.cpp')]
        command += [str(self.root / 'catalog' / name) for name in manifest['sources']]
        command += ['-o', str(self.root / 'adapter'), '-Wl,--start-group', *libraries, '-Wl,--end-group']
        result = subprocess.run(command, capture_output=True, text=True, timeout=60)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        env = os.environ.copy()
        for key in ('PS2X_EE_MISS_CAPTURE_DIR', 'PS2X_IOP_CAPTURE_DIR'):
            env.pop(key, None)
        result = subprocess.run([str(self.root / 'adapter')], capture_output=True, text=True, env=env, timeout=10)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_diagnostic_driver_keeps_legacy_whole_block_guards(self):
        case = self.case('one', 42)
        env = os.environ.copy()
        env['PS2X_NATIVE_OVERLAY_GENERATOR'] = str(GENERATOR)
        env['PS2X_NATIVE_OVERLAY_CACHE'] = str(self.root / 'driver-cache')
        library = self.root / 'library.txt'
        result = subprocess.run([sys.executable, str(ROOT / 'tools/ps2native/native_overlay_driver.py'),
                                 str(case / 'snapshot.bin'), '0x10000', '0x10000', str(library)],
                                capture_output=True, text=True, env=env, timeout=60)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        spec = importlib.util.spec_from_file_location('extract_driver', ROOT / 'lab/extract_ee_overlay.py')
        extractor = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(extractor)
        _, metadata = extractor.extract(Path(library.read_text().strip()).read_bytes(), 0x10000)
        self.assertEqual(metadata['bindings'], json.loads((case / 'bank.json').read_text())['bindings'])

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
                        {**manifest, 'dependency_contract': 'unknown-v9'},
                        {**manifest, 'dependency_contract': True},
                        {**manifest, 'dependency_plan_sha256': '0' * 64},
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

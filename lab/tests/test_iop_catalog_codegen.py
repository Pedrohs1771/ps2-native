import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location('iop_codegen', ROOT / 'lab/generate_iop_bank.py')
CODEGEN = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CODEGEN)


class IopCatalogCodegenTests(unittest.TestCase):
    def catalog(self, cases):
        return CODEGEN.generate_catalog(cases, 'compiledIopProgram', ROOT)

    def test_shared_operands_keep_full_image_and_word_identity(self):
        files = self.catalog([(0x10000, [0x24020007], [0], b'IRX-A'),
                              (0x10000, [0x24020009], [0], b'IRX-B')])
        manifest = json.loads(files['catalog.json'])
        self.assertEqual(len(manifest['modules']), 2)
        self.assertEqual(manifest['kernel_count'], 1)
        pools = [value for name, value in files.items() if name.startswith('kernels-')]
        self.assertEqual(len(pools), 1)
        self.assertIn('instructionRelocated<0x24020000u, 0x0000ffffu>', pools[0])
        modules = [value for name, value in files.items() if name.startswith('module-')]
        self.assertTrue(any('0x24020007u, 0x00000000u' in value for value in modules))
        self.assertTrue(any('0x24020009u, 0x00000000u' in value for value in modules))
        self.assertTrue(all('iop_native_semantics.h' not in value for value in modules))
        self.assertEqual(sorted(name for name in files if name.endswith('.cpp')), manifest['sources'])

    def test_normalized_family_is_independent_of_inspector_base(self):
        a = (0x10000, [0x3c020001, 0x08004005, 0x1000c], [0xffff, 0x03ffffff, 0xffffffff], b'IRX')
        b = (0x20000, [0x3c020002, 0x08008005, 0x2000c], [0xffff, 0x03ffffff, 0xffffffff], b'IRX')
        self.assertEqual(self.catalog([a]), self.catalog([b]))
        self.assertEqual(self.catalog([a]), self.catalog([a, b]))

    def test_adding_module_does_not_rewrite_existing_directories_or_pools(self):
        a = (0x10000, [0x24020007], [0], b'IRX-A')
        b = (0x10000, [0x24020009], [0], b'IRX-B')
        first, second = self.catalog([a]), self.catalog([b, a])
        for name, contents in first.items():
            if name.startswith(('module-', 'kernels-')) or name == 'iop_native_semantics.h':
                self.assertEqual(second[name], contents)
        self.assertEqual(second, self.catalog([a, b]))

    def test_conflicting_metadata_for_same_source_is_rejected(self):
        a = (0x10000, [0x24020007], [0], b'IRX')
        b = (0x10000, [0x24020009], [0], b'IRX')
        with self.assertRaises(ValueError): self.catalog([a, b])
        with self.assertRaises(ValueError): self.catalog([])

    def test_data_and_unqualified_operand_forms_never_select_kernels(self):
        files = self.catalog([(0x10000, [0x00010000, 0x03e00008], [0xffffffff, 0xffff], b'IRX')])
        manifest = json.loads(files['catalog.json'])
        self.assertEqual(manifest['kernel_count'], 0)
        self.assertEqual(manifest['modules'][0]['executable_words'], 0)
        self.assertFalse(any(name.startswith('kernels-') for name in files))

    def test_unchanged_catalog_generation_keeps_all_mtimes(self):
        files = self.catalog([(0x10000, [0x24020007], [0], b'IRX')])
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for name, value in files.items(): CODEGEN.write_if_changed(root / name, value)
            before = {name: (root / name).stat().st_mtime_ns for name in files}
            for name, value in files.items(): CODEGEN.write_if_changed(root / name, value)
            self.assertEqual(before, {name: (root / name).stat().st_mtime_ns for name in files})

    def test_cmake_rejects_invalid_or_changed_catalog_sources(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'registry.cpp').write_text('int catalog_dummy;\n')
            (root / 'iop_native_semantics.h').write_text('// identified header\n')
            hashes = {name: hashlib.sha256((root / name).read_bytes()).hexdigest()
                      for name in ('registry.cpp', 'iop_native_semantics.h')}
            original = {'schema_version': 1, 'symbol': 'compiledIopProgram',
                        'sources': ['registry.cpp'], 'sha256': hashes}
            variants = [('schema_version', '1'), ('sources', ['../registry.cpp']),
                        ('sources', []), ('sources', ['missing.cpp']),
                        ('sources', ['registry.cpp', 'registry.cpp']), ('symbol', 'otherProgram'),
                        ('sha256', dict(hashes, **{'registry.cpp': '0' * 64}))]
            for index, (key, value) in enumerate(variants):
                with self.subTest(key=key, value=value):
                    manifest = dict(original); manifest[key] = value
                    (root / 'catalog.json').write_text(json.dumps(manifest))
                    result = subprocess.run(['cmake', '-S', str(ROOT / 'ps2xIOP'),
                        '-B', str(root / f'build-{index}'), '-DPS2X_IOP_BUILD_TESTS=OFF',
                        '-DNEXO_IOP_BANK_MANIFEST=' + str(root / 'catalog.json')], capture_output=True, text=True)
                    self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_build_revalidates_changed_sources_without_manifest_changes(self):
        for changed in ('registry.cpp', 'iop_native_semantics.h'):
            with self.subTest(changed=changed), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                (root / 'registry.cpp').write_text('int catalog_dummy;\n')
                (root / 'iop_native_semantics.h').write_text('// identified header\n')
                hashes = {name: hashlib.sha256((root / name).read_bytes()).hexdigest()
                          for name in ('registry.cpp', 'iop_native_semantics.h')}
                (root / 'catalog.json').write_text(json.dumps({
                    'schema_version': 1, 'symbol': 'compiledIopProgram',
                    'sources': ['registry.cpp'], 'sha256': hashes}))
                helper = (ROOT / 'ps2xIOP/cmake/iop_native_catalog.cmake').as_posix()
                (root / 'CMakeLists.txt').write_text(
                    'cmake_minimum_required(VERSION 3.21)\nproject(CatalogIdentity NONE)\n'
                    f'include("{helper}")\n'
                    'iop_catalog_sources("${CMAKE_CURRENT_SOURCE_DIR}/catalog.json" sources)\n'
                    'add_custom_target(catalog_verified ALL)\n')
                configured = subprocess.run(['cmake', '-S', str(root), '-B', str(root / 'build')],
                                            capture_output=True, text=True)
                self.assertEqual(configured.returncode, 0, configured.stdout + configured.stderr)
                (root / changed).write_text('// changed after configuration\n')
                built = subprocess.run(['cmake', '--build', str(root / 'build')],
                                       capture_output=True, text=True)
                self.assertNotEqual(built.returncode, 0, built.stdout + built.stderr)
                self.assertIn('differs from its manifest', built.stdout + built.stderr)


if __name__ == '__main__': unittest.main()

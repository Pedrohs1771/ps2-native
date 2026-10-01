import importlib.util
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

MODULE = Path(__file__).resolve().parents[1] / 'extract_ee_overlay.py'
spec = importlib.util.spec_from_file_location('extract_ee_overlay', MODULE)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class ExtractOverlayTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory()
        cls.root = Path(cls.temporary.name)
        source = cls.root / 'fixture.cpp'
        source.write_text('''#include <cstdint>
#include <cstddef>
struct Context; struct Runtime;
void ps2native_block_10000(uint8_t*,Context*,Runtime*) {}
static const uint8_t snapshot[] = {42,0,2,36,8,0,224,3,1,0,66,36};
struct Binding { uint32_t address; void(*function)(uint8_t*,Context*,Runtime*);
uint32_t begin; uint32_t size; const uint8_t* bytes; };
static const Binding bindings[] = {{0x10000,ps2native_block_10000,0x10000,12,snapshot},
{0x10004,ps2native_block_10000,0x10000,12,snapshot},
{0x10008,ps2native_block_10000,0x10000,12,snapshot}};
extern "C" const Binding* ps2xOverlayGetBindings(size_t* count,uint32_t* abi)
{*count=3;*abi=1;return bindings;}
''')
        cls.library = cls.root / 'fixture.so'
        result = subprocess.run(['c++', '-std=c++20', '-O0', '-shared', '-fPIC',
                                 str(source), '-o', str(cls.library)], capture_output=True, text=True, timeout=30)
        if result.returncode:
            raise RuntimeError(result.stdout + result.stderr)
        cls.data = cls.library.read_bytes()

    @classmethod
    def tearDownClass(cls):
        cls.temporary.cleanup()

    def test_recovers_snapshot_and_interior_entries_without_dlopen(self):
        image, metadata = module.extract(self.data, 0x10004)
        self.assertEqual(image, bytes([42,0,2,36,8,0,224,3,1,0,66,36]))
        self.assertEqual(metadata['base'], 0x10000)
        self.assertEqual(len(metadata['bindings']), 3)
        self.assertFalse(metadata['live_abi_independently_verified'])

    def test_invalid_file_headers_and_uncovered_roots_fail(self):
        for data in (b'', self.data[:63], b'BAD!' + self.data[4:]):
            with self.assertRaises(ValueError): module.extract(data, 0x10000)
        for entry in (0, 0x10002, 0x1000C):
            with self.assertRaises(ValueError): module.extract(self.data, entry)

    def test_existing_output_is_preserved(self):
        output = self.root / 'existing'
        output.mkdir(exist_ok=True)
        sentinel = output / 'sentinel'
        sentinel.write_bytes(b'preserve')
        run = subprocess.run([sys.executable, str(MODULE), str(self.library), str(output),
                              '--entry', '0x10000'], capture_output=True, timeout=10)
        self.assertNotEqual(run.returncode, 0)
        self.assertEqual(sentinel.read_bytes(), b'preserve')

    def test_corrupt_object_extent_and_binding_footprint_fail(self):
        artifact = module.Artifact(self.data)
        symbol, _ = artifact.object('_ZL8bindings')
        section = artifact.sections[symbol['owner']]
        offset = section[4] + symbol['address'] - section[3]
        for size in (0, 13, 16, 0xFFFFFFFF):
            data = bytearray(self.data)
            struct.pack_into('<I', data, offset + 20, size)
            with self.subTest(size=size), self.assertRaises(ValueError):
                module.extract(bytes(data), 0x10000)
        data = bytearray(self.data)
        struct.pack_into('<Q', data, 40, len(data) - 1)
        with self.assertRaises(ValueError): module.extract(bytes(data), 0x10000)

    def test_callback_outside_its_executable_section_is_rejected(self):
        artifact = module.Artifact(self.data)
        data = bytearray(self.data)
        for index, symbols in artifact.symbol_tables.items():
            table = artifact.sections[index]
            for slot, symbol in enumerate(symbols):
                if symbol['type'] == 2 and 'ps2native_block_' in symbol['name']:
                    section = artifact.sections[symbol['owner']]
                    struct.pack_into('<Q', data, table[4] + slot * 24 + 8,
                                     section[3] + section[5] + 64)
        with self.assertRaises(ValueError): module.extract(bytes(data), 0x10000)


if __name__ == '__main__':
    unittest.main()

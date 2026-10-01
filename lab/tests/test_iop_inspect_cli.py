import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

INSPECT = Path(sys.argv[1]).resolve()
sys.argv = sys.argv[:1]


def fixture(relocation=None, entry=0):
    size = 0x300 if relocation is not None else 0x110
    image = bytearray(size)
    ident = b'\x7fELF\x01\x01\x01' + bytes(9)
    struct.pack_into('<16sHHIIIIIHHHHHH', image, 0, ident, 0xff80, 8, 1, entry,
                     52, 0x200 if relocation is not None else 0, 0, 52, 32, 1,
                     40 if relocation is not None else 0, 2 if relocation is not None else 0, 0)
    struct.pack_into('<IIIIIIII', image, 52, 1, 0x100, 0, 0, 16, 16, 7, 4)
    struct.pack_into('<IIII', image, 0x100, 0x24020000, 0x03e00008, 0, 12)
    if relocation is not None:
        struct.pack_into('<IIIIIIIIII', image, 0x200, 0, 1, 6, 0, 0x100, 16, 0, 0, 4, 0)
        struct.pack_into('<IIIIIIIIII', image, 0x228, 0, 9, 0, 0, 0x280, 8, 0, 0, 4, 8)
        struct.pack_into('<II', image, 0x280, 12, relocation)
    return image


class IopInspectCliTests(unittest.TestCase):
    def run_case(self, image, cursor='0x10000'):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        root = Path(directory.name)
        source, output = root / 'input.irx', root / 'result'
        source.write_bytes(image)
        result = subprocess.run([str(INSPECT), str(source), str(output), cursor], capture_output=True)
        self.assertEqual(source.read_bytes(), image)
        return result, output

    def test_complete_absolute_loaded_bank(self):
        result, output = self.run_case(fixture())
        self.assertEqual(result.returncode, 0, result.stderr)
        metadata = json.loads((output / 'module.json').read_text())
        self.assertEqual(metadata['base'], 0x10000)
        self.assertEqual(metadata['entry'], 0x10000)
        data = (output / 'relocated-ram.bin').read_bytes()
        self.assertEqual(len(data), metadata['size'])
        self.assertEqual(data[:16], fixture()[0x100:0x110])
        self.assertEqual((output / 'source-image.bin').read_bytes(), fixture())
        self.assertEqual(metadata['relocation_masks'], [])

    def test_sony_data_relocation_at_two_bases(self):
        for base in (0x10000, 0x20000):
            result, output = self.run_case(fixture(2), hex(base))
            self.assertEqual(result.returncode, 0, result.stderr)
            data = (output / 'relocated-ram.bin').read_bytes()
            self.assertEqual(struct.unpack_from('<I', data, 12)[0], base + 12)
            metadata = json.loads((output / 'module.json').read_text())
            self.assertEqual(metadata['relocation_masks'], [{'offset': 12, 'mask': 0xffffffff}])

    def test_unsupported_relocation_emits_no_bank(self):
        result, output = self.run_case(fixture(255))
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse(output.exists())

    def test_incomplete_relocation_tables_emit_no_bank(self):
        bad_info = fixture(2); struct.pack_into('<I', bad_info, 0x228 + 28, 2)
        bad_range = fixture(2); struct.pack_into('<I', bad_range, 0x228 + 16, 0xfffffff0)
        truncated = fixture(2); struct.pack_into('<I', truncated, 0x228 + 20, 7)
        unmatched_hi = fixture(5)
        wrapped_place = fixture(2); struct.pack_into('<I', wrapped_place, 0x280, 0xfffeffff)
        for image in (bad_info, bad_range, truncated, unmatched_hi, wrapped_place):
            result, output = self.run_case(image)
            self.assertNotEqual(result.returncode, 0)
            self.assertFalse(output.exists())

    def test_invalid_image_and_entry_emit_no_bank(self):
        for image in (b'invalid', fixture(entry=0x1000), fixture(entry=1)):
            result, output = self.run_case(image)
            self.assertNotEqual(result.returncode, 0)
            self.assertFalse(output.exists())

    def test_invalid_cursors_emit_no_bank(self):
        for cursor in ('-1', '0', '0x120000', 'garbage', '0x10000junk'):
            result, output = self.run_case(fixture(), cursor)
            self.assertNotEqual(result.returncode, 0)
            self.assertFalse(output.exists())

    def test_existing_output_is_preserved(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source, output = root / 'input.irx', root / 'output'
            source.write_bytes(fixture()); output.mkdir()
            marker = output / 'keep'; marker.write_text('existing')
            result = subprocess.run([str(INSPECT), str(source), str(output)], capture_output=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(marker.read_text(), 'existing')
            self.assertEqual(list(output.iterdir()), [marker])


if __name__ == '__main__': unittest.main()

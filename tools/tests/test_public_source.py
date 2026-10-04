"""The publication guard must catch executable content with innocuous names."""
import unittest
from tools.check_public_source import violation


class PublicSourceTests(unittest.TestCase):
    def test_payload_paths_and_disguised_content_are_rejected(self):
        for path in ('fixtures/title/SLES_000.00', 'disc.ISO', 'module.IRX',
                     'ps2xRuntime/vita/module/libGL.suprx', 'test-data/isos/input.dat'):
            self.assertIsNotNone(violation(path), path)
        self.assertIsNotNone(violation('innocent.dat', b'\x7fELF'))
        self.assertIsNotNone(violation('innocent.dat', b'1234', b'CD001'))

    def test_owned_source_and_input_readme_are_allowed(self):
        for path in ('lab/tests/iop_words.json', 'lab/tests/ee_autoadapt_fixture.cpp',
                     'README.md', 'test-data/isos/README.md'):
            self.assertIsNone(violation(path), path)


if __name__ == '__main__':
    unittest.main()

"""The reference engine must stay byte-identical to its pinned upstream source."""
import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import hashlib

SCRIPT = Path(__file__).resolve().parents[1] / "prepare_pcsx2_reference.py"
spec = importlib.util.spec_from_file_location("prepare_pcsx2_reference", SCRIPT)
reference = importlib.util.module_from_spec(spec)
spec.loader.exec_module(reference)


class ReferenceSourceTests(unittest.TestCase):
    def fixture(self, directory):
        source = directory / "cached"
        data = {"pcsx2/VUops.cpp": b"original core\n", "COPYING.GPLv3": b"license\n",
                "pcsx2/Gif_Unit.h": b"// original license\n#pragma once\nstruct Gif_Tag\n{ int x; };\n\nstruct GS_Packet\n{};\n"
                b"static __fi void incTag(int x)\n{}\n\nstruct Gif_Path_MTVU\n{};\n"
                b"\tu32 GetGSPacketSize(int x)\n{ return x; }\n\t// Specify the transfer type\n"}
        for name, content in data.items():
            p = source / name
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_bytes(content)
        return source, {n: hashlib.sha256(d).hexdigest() for n, d in data.items()}

    def test_extract_preserves_exact_bytes(self):
        source = b"prefix\nstruct X\n{\n int x;\n};\n\nsuffix\n"
        self.assertEqual(reference.extract(source, b"struct X\n", b"\n\nsuffix"),
                         b"struct X\n{\n int x;\n};")

    def test_extract_rejects_missing_or_ambiguous_boundaries(self):
        for source in (b"start", b"start end start end", b"end start"):
            with self.assertRaises(ValueError):
                reference.extract(source, b"start", b" end")

    def test_hash_mismatch_is_rejected(self):
        with self.assertRaises(ValueError):
            reference.verify(b"modified", "0" * 64, "fixture")

    def test_unchanged_staging_preserves_timestamp(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp) / "source.cpp"
            reference.write_unchanged(p, b"upstream")
            stamp = p.stat().st_mtime_ns
            reference.write_unchanged(p, b"upstream")
            self.assertEqual(p.stat().st_mtime_ns, stamp)
            reference.write_unchanged(p, b"new revision")
            self.assertEqual(p.read_bytes(), b"new revision")

    def test_full_prepare_preserves_core_license_and_cache(self):
        with tempfile.TemporaryDirectory() as tmp:
            directory = Path(tmp)
            source, hashes = self.fixture(directory)
            output = directory / "staged"
            with patch.object(reference, "FILES", hashes):
                reference.prepare(source, output)
                self.assertEqual((output / "VUops.cpp").read_bytes(), (source / "pcsx2/VUops.cpp").read_bytes())
                self.assertTrue((output / "Gif_Tag.inc").read_bytes().startswith(b"// original license\n"))
                stamp = (output / "VUops.cpp").stat().st_mtime_ns
                reference.prepare(source, output)
                self.assertEqual((output / "VUops.cpp").stat().st_mtime_ns, stamp)
                (source / "pcsx2/VUops.cpp").write_bytes(b"changed")
                with self.assertRaises(ValueError):
                    reference.prepare(source, output)

    def test_unverified_header_cannot_shadow_an_adapter(self):
        with tempfile.TemporaryDirectory() as tmp:
            directory = Path(tmp)
            source, hashes = self.fixture(directory)
            output = directory / "staged"
            output.mkdir()
            (output / "Vif.h").write_bytes(b"unverified header")
            with patch.object(reference, "FILES", hashes), self.assertRaises(ValueError):
                reference.prepare(source, output)


if __name__ == "__main__":
    unittest.main()

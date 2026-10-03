"""Corpus orchestration contracts; these fixtures do not qualify PS2 games."""
import argparse
import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock

from tools.ps2native import corpus_batch
from tools.ps2native.pipeline import PipelineError, attest_package_files


class CorpusBatchTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.discs = self.root / "discs"
        self.discs.mkdir()

    def arguments(self):
        return argparse.Namespace(iso_dir=self.discs, out=self.root / "job",
                                  build_jobs=2, timeout=30, inspector=None,
                                  analyzer=None, recompiler=None, cmake=None)

    def fixture_builder(self, args):
        package = args.out
        (package / "bin").mkdir(parents=True)
        (package / "bin/runner-fixture").write_bytes(b"synthetic host artifact")
        manifest = {"artifact_files_schema_version": 1,
                    "artifact_files": attest_package_files(package)}
        (package / "manifest.json").write_text(json.dumps(manifest))
        return {"status": "complete", "artifact": str(package),
                "gameplay_compatibility": "unverified"}

    def test_inventory_deduplicates_content_and_only_selects_iso_candidates(self):
        (self.discs / "a.ISO").write_bytes(b"one")
        (self.discs / "copy.iso").write_bytes(b"one")
        (self.discs / "b.iso").write_bytes(b"two")
        (self.discs / "ignored.chd").write_bytes(b"three")
        report = corpus_batch.inventory(self.discs)
        self.assertEqual(report["unique_images"], 2)
        self.assertEqual(report["duplicate_images"], 1)
        self.assertEqual(report["images"][0]["duplicate_paths"],
                         [str(self.discs / "copy.iso")])
        self.assertFalse(report["gameplay_approved"])
        self.assertEqual(sorted(p.name for p in self.discs.iterdir()),
                         ["a.ISO", "b.iso", "copy.iso", "ignored.chd"])

    def test_build_preserves_paths_with_unicode_quotes_and_shell_characters(self):
        image = self.discs / "Jogo (edição) '$;literal.iso"
        image.write_bytes(b"fixture")
        received = []

        def builder(args):
            received.append(args.iso)
            return self.fixture_builder(args)

        report = corpus_batch.build_corpus(self.arguments(), builder=builder)
        self.assertEqual(received, [image])
        self.assertEqual(report["status"], "BUILT_UNVERIFIED")
        self.assertEqual(report["images"][0]["status"], "BUILT_UNVERIFIED")
        self.assertEqual(image.read_bytes(), b"fixture")
        self.assertFalse(report["strict_approval"])

    def test_one_build_failure_is_recorded_and_other_images_are_processed(self):
        (self.discs / "a.iso").write_bytes(b"one")
        (self.discs / "b.iso").write_bytes(b"two")

        def builder(args):
            if args.iso.name == "a.iso":
                raise PipelineError("unsupported fixture operation")
            return self.fixture_builder(args)

        args = self.arguments()
        report = corpus_batch.build_corpus(args, builder=builder)
        self.assertEqual(report["status"], "PARTIAL_FAILURE")
        self.assertEqual([row["status"] for row in report["images"]],
                         ["FAILED", "BUILT_UNVERIFIED"])
        self.assertEqual(report["images"][0]["error_type"], "PipelineError")
        self.assertEqual(json.loads((args.out / "corpus.json").read_text())["status"],
                         "PARTIAL_FAILURE")

    def test_return_zero_cannot_approve_a_missing_or_corrupted_package(self):
        (self.discs / "a.iso").write_bytes(b"one")

        def builder(args):
            result = self.fixture_builder(args)
            (args.out / "bin/runner-fixture").write_bytes(b"corrupted")
            result["gameplay_compatibility"] = "approved"
            return result

        report = corpus_batch.build_corpus(self.arguments(), builder=builder)
        self.assertEqual(report["status"], "PARTIAL_FAILURE")
        self.assertEqual(report["images"][0]["status"], "FAILED")
        self.assertFalse(report["gameplay_approved"])

    def test_existing_job_is_not_overwritten(self):
        (self.discs / "a.iso").write_bytes(b"one")
        args = self.arguments()
        args.out.mkdir()
        marker = args.out / "corpus.json"
        marker.write_text("previous evidence")
        with self.assertRaises((PipelineError, ValueError)):
            corpus_batch.build_corpus(args, builder=self.fixture_builder)
        self.assertEqual(marker.read_text(), "previous evidence")

    def test_iso_modified_during_build_is_not_marked_complete(self):
        image = self.discs / "a.iso"
        image.write_bytes(b"one")

        def builder(args):
            result = self.fixture_builder(args)
            image.write_bytes(b"changed")
            return result

        report = corpus_batch.build_corpus(self.arguments(), builder=builder)
        self.assertEqual(report["images"][0]["status"], "FAILED")
        self.assertIn("changed", report["images"][0]["error"].lower())

    def test_symlink_image_is_rejected_before_work(self):
        target = self.root / "source"
        target.write_bytes(b"one")
        (self.discs / "a.iso").symlink_to(target)
        with self.assertRaises((PipelineError, ValueError)):
            corpus_batch.inventory(self.discs)

    def make_capture(self, root, number):
        capture = root / f"ee-miss-{number:06d}"
        capture.mkdir(parents=True)
        for name in ("request.json", "snapshot.bin", "ee-ram.bin"):
            (capture / name).write_bytes(b"fixture")
        return capture

    def test_multi_title_capture_selection_keeps_both_equal_event_numbers(self):
        a = self.make_capture(self.root / "title-a", 1)
        b = self.make_capture(self.root / "title-b", 1)
        selected = corpus_batch.capture_selection([a.parent, b.parent, a.parent])
        self.assertEqual(selected, [a, b])

    def test_capture_overflow_fails_without_silently_dropping_cases(self):
        roots = [self.root / "title-a", self.root / "title-b"]
        for index in range(17):
            self.make_capture(roots[index // 10], index % 10 + 1)
        with self.assertRaisesRegex((PipelineError, ValueError), "16"):
            corpus_batch.capture_selection(roots)

    def test_incomplete_capture_is_not_admitted(self):
        capture = self.make_capture(self.root / "title", 1)
        (capture / "ee-ram.bin").unlink()
        with self.assertRaises((PipelineError, ValueError)):
            corpus_batch.capture_selection([capture.parent])

    def test_merge_keeps_old_catalog_and_records_new_publication_identity(self):
        capture = self.make_capture(self.root / "title-a", 1)
        previous = self.root / "previous"
        previous.mkdir()
        marker = previous / "old-evidence"
        marker.write_text("retain")
        generator = self.root / "generator"
        generator.write_text("fixture converter")
        args = argparse.Namespace(capture_root=[capture.parent], previous_batch=previous,
                                  previous_family_catalog=None, family_generator=generator,
                                  overlay_generator=None, out=self.root / "merge-job",
                                  workers=2, timeout=30)

        def synthetic_publication(command, log, cwd, timeout):
            # Emulate only the external compiler boundary. Verify the on-disk
            # publication and its hash through the production receipt reader.
            self.assertEqual(command.count("--capture"), 1)
            batch = args.out / "batch"
            (batch / "catalog").mkdir(parents=True)
            manifest = batch / "catalog/catalog.json"
            manifest.write_text('{"strict_approval": false}\n')
            (batch / "report.json").write_text(json.dumps({
                "status": "PUBLISHED_LABORATORY", "strict_approval": False,
                "closure_proved": False, "manifest_published": True,
                "family_catalog_sha256": corpus_batch.sha256_file(manifest),
                "family_count": 1, "owned_cases": 1, "duplicate_cases": 0,
                "candidate_count": 1, "declined_structures": 0}))

        with mock.patch.object(corpus_batch, "_run", side_effect=synthetic_publication):
            report = corpus_batch.merge_captures(args)
        self.assertEqual(report["status"], "GENERATED_LABORATORY")
        self.assertFalse(report["compiled"])
        self.assertFalse(report["installed_in_runner"])
        self.assertEqual(marker.read_text(), "retain")
        self.assertEqual(report["family_manifest_sha256"],
                         corpus_batch.sha256_file(Path(report["family_manifest"])))


if __name__ == "__main__":
    unittest.main()

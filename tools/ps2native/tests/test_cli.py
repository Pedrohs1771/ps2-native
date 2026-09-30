import json
import os
import tempfile
import unittest
from pathlib import Path

from tools.ps2native.cli import parse_args
from tools.ps2native.pipeline import (
    PipelineError,
    attest_package_files,
    create_workspace,
    iso_path_to_extracted_path,
    normalize_generated_cpp_compatibility,
    publish_package,
    source_tree_snapshot,
    validate_inspector_report,
    verify_package,
)


class CliArgumentTests(unittest.TestCase):
    def test_build_defaults_to_desktop_target(self):
        args = parse_args(["build", "--iso", "game.iso"])
        self.assertEqual(args.command, "build")
        self.assertEqual(args.target, "desktop")

    def test_android_is_an_explicit_build_target(self):
        args = parse_args(["build", "--iso", "game.iso", "--target", "android"])
        self.assertEqual(args.target, "android")

    def test_build_rejects_unknown_target(self):
        with self.assertRaises(SystemExit):
            parse_args(["build", "--iso", "game.iso", "--target", "ios"])

    def test_verify_accepts_package_directory_argument(self):
        args = parse_args(["verify", "--package", "out/game"])
        self.assertEqual(args.command, "verify")
        self.assertEqual(args.package, Path("out/game"))


class InspectorContractTests(unittest.TestCase):
    def test_accepts_inspector_schema_v1_and_returns_boot_metadata(self):
        report = {
            "schema_version": 1,
            "image": {"path": "/tmp/game.iso", "size_bytes": 4096, "filesystem": "ISO9660",
                      "volume_id": "GAME", "logical_block_size": 2048,
                      "joliet_names": False, "sha256": "a" * 64},
            "entries": [{"path": "SLUS_000.00;1", "kind": "file", "size_bytes": 256,
                         "extent_lba": 2}],
            "boot": {"system_cnf": {"path": "SYSTEM.CNF;1",
                                     "boot2": "cdrom0:\\SLUS_000.00;1", "version": "1.00",
                                     "video_mode": "NTSC", "region_inferred": "NTSC-U"},
                     "elf": {"path": "/SLUS_000.00;1", "format": "ELF32",
                             "byte_order": "little-endian", "machine": "MIPS", "machine_id": 8,
                             "entrypoint": "0x00100000", "sha256": "b" * 64,
                             "load_segments": []}},
        }

        parsed = validate_inspector_report(report)
        self.assertEqual(parsed.boot_elf_path, "/SLUS_000.00;1")
        self.assertEqual(parsed.boot_elf_sha256, "b" * 64)
        self.assertEqual(parsed.image_sha256, "a" * 64)

    def test_rejects_unknown_inspector_schema_version(self):
        with self.assertRaisesRegex(ValueError, "schema_version"):
            validate_inspector_report({"schema_version": 2})

    def test_rejects_traversal_in_boot_path(self):
        with self.assertRaisesRegex(ValueError, "unsafe"):
            iso_path_to_extracted_path("/../outside.elf")

    def test_maps_iso_version_suffixes_to_extracted_names(self):
        path = iso_path_to_extracted_path("/DIR;1/GAME.ELF;12")
        self.assertEqual(path.as_posix(), "DIR/GAME.ELF")


class WorkspaceIsolationTests(unittest.TestCase):
    def test_source_snapshot_changes_when_tracked_source_bytes_change(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "CMakeLists.txt").write_text("project(before)\n", encoding="utf-8")
            source = root / "ps2xRuntime" / "src" / "runtime.cpp"
            source.parent.mkdir(parents=True)
            source.write_text("before\n", encoding="utf-8")
            before = source_tree_snapshot(root)

            source.write_text("after\n", encoding="utf-8")
            after = source_tree_snapshot(root)

            self.assertEqual(before["file_count"], 2)
            self.assertNotEqual(before["sha256"], after["sha256"])

    def test_workspace_is_title_scoped_and_kept_out_of_source_directories(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary) / "repo"
            (root / "ps2xRuntime" / "src").mkdir(parents=True)
            work_root = root / "build" / "ps2native"
            work_root.mkdir(parents=True)

            workspace = create_workspace(
                repo_root=root, work_root=work_root, title="../bad/title.iso",
                image_sha256="c" * 64,
            )

            self.assertTrue(workspace.is_relative_to(work_root.resolve()))
            self.assertFalse(workspace.is_relative_to((root / "ps2xRuntime").resolve()))
            self.assertIn("bad-title", workspace.as_posix())
            self.assertTrue(workspace.is_dir())

    def test_rejects_work_root_inside_runtime_source(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary) / "repo"
            source_dir = root / "ps2xRuntime" / "src"
            source_dir.mkdir(parents=True)

            with self.assertRaisesRegex(ValueError, "source tree"):
                create_workspace(
                    repo_root=root, work_root=source_dir / "generated",
                    title="game.iso", image_sha256="d" * 64,
                )


class GeneratedSourceCompatibilityTests(unittest.TestCase):
    def test_adds_isnan_import_only_to_units_using_unordered_fpu_comparisons(self):
        with tempfile.TemporaryDirectory() as temporary:
            generated = Path(temporary)
            affected = generated / "sub_1000.cpp"
            unaffected = generated / "sub_2000.cpp"
            affected.write_text(
                '#include "ps2_runtime_macros.h"\n'
                "void compare() { (void)FPU_C_UEQ_S(1.0f, 2.0f); }\n",
                encoding="utf-8",
            )
            unaffected.write_text(
                '#include "ps2_runtime_macros.h"\n'
                "void compare() { (void)FPU_C_EQ_S(1.0f, 2.0f); }\n",
                encoding="utf-8",
            )

            patched = normalize_generated_cpp_compatibility(generated)

            self.assertEqual(patched, ["sub_1000.cpp"])
            self.assertIn('#include "ps2_runtime_macros.h"\nusing std::isnan;', affected.read_text(encoding="utf-8"))
            self.assertNotIn("using std::isnan", unaffected.read_text(encoding="utf-8"))
            self.assertEqual(normalize_generated_cpp_compatibility(generated), [])


class PackagePublicationTests(unittest.TestCase):
    def test_package_attestation_covers_every_file_except_its_manifest(self):
        with tempfile.TemporaryDirectory() as temporary:
            package = Path(temporary) / "package"
            (package / "bin").mkdir(parents=True)
            (package / "bin" / "runner").write_bytes(b"native runner")
            (package / "game.iso").write_bytes(b"disc image")

            records = attest_package_files(package)

            self.assertEqual([record["path"] for record in records], ["bin/runner", "game.iso"])
            self.assertEqual(records[0]["size_bytes"], len(b"native runner"))
            self.assertEqual(len(records[0]["sha256"]), 64)

    def test_verify_package_detects_changed_missing_and_unexpected_files(self):
        with tempfile.TemporaryDirectory() as temporary:
            package = Path(temporary) / "package"
            (package / "bin").mkdir(parents=True)
            (package / "bin" / "runner").write_bytes(b"runner v1")
            (package / "game.iso").write_bytes(b"disc")
            (package / "manifest.json").write_text(
                json.dumps({"artifact_files_schema_version": 1,
                            "artifact_files": attest_package_files(package)}), encoding="utf-8"
            )

            clean = verify_package(package)
            self.assertEqual(clean["status"], "verified")
            self.assertEqual(clean["checked_files"], 2)

            (package / "bin" / "runner").write_bytes(b"runner v2")
            (package / "game.iso").unlink()
            (package / "unexpected.txt").write_text("extra", encoding="utf-8")
            changed = verify_package(package)
            self.assertEqual(changed["status"], "mismatch")
            self.assertIn("bin/runner", changed["changed"])
            self.assertIn("game.iso", changed["missing"])
            self.assertIn("unexpected.txt", changed["unexpected"])

    def test_verify_allows_declared_runtime_logs_created_after_packaging(self):
        with tempfile.TemporaryDirectory() as temporary:
            package = Path(temporary) / "package"
            (package / "bin").mkdir(parents=True)
            (package / "bin" / "runner").write_bytes(b"runner")
            manifest = {
                "artifact_files_schema_version": 1,
                "artifact_files": attest_package_files(package),
                "mutable_outputs": ["ps2_log.txt", "game/ps2_log.txt"],
            }
            (package / "manifest.json").write_text(json.dumps(manifest), encoding="utf-8")
            (package / "ps2_log.txt").write_text("runtime log", encoding="utf-8")
            (package / "game").mkdir()
            (package / "game" / "ps2_log.txt").write_text("function log", encoding="utf-8")

            report = verify_package(package)

            self.assertEqual(report["status"], "verified")
            self.assertEqual(report["checked_files"], 1)
            self.assertEqual(report["unexpected"], [])

    def test_verify_rejects_unsupported_or_unsafe_manifest_inventory(self):
        with tempfile.TemporaryDirectory() as temporary:
            package = Path(temporary) / "package"
            package.mkdir()
            manifest = package / "manifest.json"
            manifest.write_text(json.dumps({"artifact_files_schema_version": 2,
                                            "artifact_files": []}), encoding="utf-8")
            with self.assertRaisesRegex(PipelineError, "schema_version"):
                verify_package(package)

            manifest.write_text(json.dumps({
                "artifact_files_schema_version": 1,
                "artifact_files": [{"path": "../outside", "size_bytes": 1, "sha256": "0" * 64}],
            }), encoding="utf-8")
            with self.assertRaisesRegex(PipelineError, "unsafe artifact path"):
                verify_package(package)

            manifest.write_text(json.dumps({
                "artifact_files_schema_version": 1,
                "artifact_files": [{"path": "safe.bin", "size_bytes": 1, "sha256": "0" * 64}],
                "mutable_outputs": ["../../outside"],
            }), encoding="utf-8")
            with self.assertRaisesRegex(PipelineError, "unsafe mutable output path"):
                verify_package(package)

    def test_publishes_to_external_destination_without_overwrite(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            package = root / "isolated-work" / "package"
            package.mkdir(parents=True)
            (package / "runner").write_text("built", encoding="utf-8")
            destination = root / "user-output" / "game-package"

            result = publish_package(package, destination)

            self.assertEqual(result, destination.resolve())
            self.assertEqual((destination / "runner").read_text(encoding="utf-8"), "built")
            with self.assertRaisesRegex(PipelineError, "refusing to overwrite"):
                publish_package(package, destination)

    @unittest.skipIf(os.name == "nt", "symlink creation is not enabled on all Windows hosts")
    def test_rejects_dangling_destination_symlink_before_resolving_it(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            package = root / "package"
            package.mkdir()
            destination = root / "dangling-link"
            target = root / "should-not-be-created"
            destination.symlink_to(target)

            with self.assertRaisesRegex(PipelineError, "refusing to overwrite"):
                publish_package(package, destination)
            self.assertFalse(target.exists())


if __name__ == "__main__":
    unittest.main()

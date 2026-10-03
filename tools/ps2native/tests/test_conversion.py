import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest import mock

from tools.ps2native import cli
from tools.ps2native.pipeline import PipelineError, attest_package_files, sha256_file


class ConversionTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.iso = self.root / "Game (edição) '$;literal.iso"
        self.iso.write_bytes(b"synthetic disc")
        self.workspace = self.root / "workspace"
        self.workspace.mkdir()
        self.package = self.root / "package"
        (self.package / "bin").mkdir(parents=True)
        (self.package / "game").mkdir()
        (self.package / "game/boot.elf").write_bytes(b"synthetic boot")
        self.runner = self.package / "bin/ps2EntryRunner"
        self.runner.write_text(
            "#!/usr/bin/env python3\n"
            "import json,os,pathlib,sys\n"
            "pathlib.Path('ps2_log.txt').write_text(json.dumps({'argv':sys.argv[1:],"
            "'driver':os.environ.get('PS2X_NATIVE_OVERLAY_DRIVER')}))\n",
            encoding="utf-8",
        )
        self.runner.chmod(0o755)
        self.attest()

    def attest(self):
        manifest = {
            "artifact_files_schema_version": 1,
            "artifact_files": attest_package_files(self.package),
            "mutable_outputs": ["ps2_log.txt"],
            "source": {"iso_sha256": sha256_file(self.iso)},
            "desktop": {"runner": "bin/ps2EntryRunner"},
        }
        (self.package / "manifest.json").write_text(json.dumps(manifest))

    def result(self):
        return {"status": "complete", "workspace": str(self.workspace),
                "artifact": str(self.package), "gameplay_compatibility": "unverified"}

    def test_convert_accepts_positional_iso_and_shared_build_options(self):
        args = cli.parse_args(["convert", str(self.iso), "--no-run", "--build-jobs", "2"])
        self.assertEqual(args.iso, self.iso)
        self.assertEqual(args.target, "desktop")
        self.assertFalse(args.run)
        self.assertEqual(args.build_jobs, 2)

    def test_no_run_builds_and_verifies_without_starting_a_process(self):
        from tools.ps2native.conversion import run_convert
        args = cli.parse_args(["convert", str(self.iso), "--no-run"])
        with mock.patch("tools.ps2native.conversion.run_build", return_value=self.result()):
            result = run_convert(args)
        self.assertEqual(result["status"], "complete")
        self.assertEqual(result["run_status"], "not_requested")
        self.assertFalse((self.package / "ps2_log.txt").exists())
        receipt = json.loads((self.workspace / "conversion.json").read_text())
        self.assertEqual(receipt["integrity"]["status"], "verified")
        self.assertFalse(receipt["menu_approved"])

    @unittest.skipUnless(os.name == "posix", "executable synthetic runner uses a POSIX shebang")
    def test_default_launch_uses_literal_paths_and_removes_live_compiler_driver(self):
        from tools.ps2native.conversion import run_convert
        args = cli.parse_args(["convert", str(self.iso)])
        with mock.patch("tools.ps2native.conversion.run_build", return_value=self.result()), \
                mock.patch.dict(os.environ, {"PS2X_NATIVE_OVERLAY_DRIVER": "must-not-run"}):
            result = run_convert(args)
        event = json.loads((self.package / "ps2_log.txt").read_text())
        self.assertEqual(event["argv"], [str(self.package / "game/boot.elf"), str(self.iso)])
        self.assertIsNone(event["driver"])
        self.assertEqual(result["run_status"], "exited")
        self.assertEqual(result["gameplay_compatibility"], "unverified")
        receipt = json.loads((self.workspace / "conversion.json").read_text())
        self.assertFalse(receipt["menu_approved"])
        self.assertFalse(receipt["native_execution_qualified"])

    def test_changed_package_cannot_be_launched(self):
        from tools.ps2native.conversion import run_convert
        self.runner.write_bytes(b"tampered")
        args = cli.parse_args(["convert", str(self.iso)])
        with mock.patch("tools.ps2native.conversion.run_build", return_value=self.result()):
            with self.assertRaisesRegex(PipelineError, "integrity"):
                run_convert(args)
        self.assertFalse((self.package / "ps2_log.txt").exists())

    def test_changed_disc_is_rejected_before_launch(self):
        from tools.ps2native.conversion import run_convert
        self.iso.write_bytes(b"changed disc")
        args = cli.parse_args(["convert", str(self.iso)])
        with mock.patch("tools.ps2native.conversion.run_build", return_value=self.result()):
            with self.assertRaisesRegex(PipelineError, "ISO"):
                run_convert(args)
        self.assertFalse((self.package / "ps2_log.txt").exists())

    @unittest.skipUnless(os.name == "posix", "executable synthetic runner uses a POSIX shebang")
    def test_failed_runner_is_recorded_as_failure_and_never_as_menu_approval(self):
        from tools.ps2native.conversion import run_convert
        self.runner.write_text("#!/usr/bin/env python3\nraise SystemExit(23)\n")
        self.attest()
        args = cli.parse_args(["convert", str(self.iso)])
        with mock.patch("tools.ps2native.conversion.run_build", return_value=self.result()):
            with self.assertRaisesRegex(PipelineError, "status 23"):
                run_convert(args)
        receipt = json.loads((self.workspace / "conversion.json").read_text())
        self.assertEqual(receipt["run_status"], "failed")
        self.assertFalse(receipt["menu_approved"])

    def test_android_does_not_attempt_desktop_launch(self):
        from tools.ps2native.conversion import run_convert
        args = cli.parse_args(["convert", str(self.iso), "--target", "android", "--run"])
        with mock.patch("tools.ps2native.conversion.run_build") as build:
            with self.assertRaisesRegex(PipelineError, "desktop"):
                run_convert(args)
        build.assert_not_called()

    def test_nonpositive_run_budget_is_rejected_before_build(self):
        from tools.ps2native.conversion import run_convert
        args = cli.parse_args(["convert", str(self.iso), "--run-timeout", "0"])
        with mock.patch("tools.ps2native.conversion.run_build") as build:
            with self.assertRaisesRegex(PipelineError, "run-timeout"):
                run_convert(args)
        build.assert_not_called()


if __name__ == "__main__":
    unittest.main()

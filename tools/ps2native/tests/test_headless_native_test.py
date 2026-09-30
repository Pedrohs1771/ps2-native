"""Isolation regressions; these fixtures never start a window or audio server."""
import importlib.util
import json
import os
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock

SCRIPT = Path(__file__).resolve().parents[1] / "headless_native_test.py"
SPEC = importlib.util.spec_from_file_location("headless_native_test", SCRIPT)
HEADLESS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(HEADLESS)


class HeadlessIsolationTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        root = Path(self.temporary.name)
        self.package = root / "package"
        (self.package / "bin").mkdir(parents=True)
        (self.package / "game").mkdir()
        (self.package / "bin/ps2EntryRunner").touch()
        (self.package / "game/boot.elf").touch()
        self.iso = root / "fixture.iso"
        self.iso.touch()
        self.args = SimpleNamespace(package=self.package, iso=self.iso, state=root / "state.json",
                                    log=root / "game.log", runner=None, capture_scene=None)

    def tearDown(self):
        self.temporary.cleanup()

    def test_free_display_search_is_after_base_display_option(self):
        process = mock.Mock(pid=3456)
        process.poll.return_value = 0
        process.wait.return_value = 0
        loaded = SimpleNamespace(returncode=0, stdout="123\n")
        with mock.patch.object(HEADLESS.shutil, "which", return_value="available"), \
             mock.patch.object(HEADLESS.subprocess, "run", return_value=loaded), \
             mock.patch.object(HEADLESS.subprocess, "Popen", return_value=process) as popen, \
             mock.patch.object(HEADLESS.signal, "signal"), \
             mock.patch("builtins.print"):
            HEADLESS.launch(self.args)
        command = popen.call_args.args[0]
        self.assertLess(command.index("-n"), command.index("-a"))
        self.assertEqual(command[command.index("-n") + 1], "90")
        self.assertTrue(popen.call_args.kwargs["start_new_session"])

    def test_borrowed_virtual_server_is_rejected_before_runner_or_state(self):
        with mock.patch.dict(os.environ, {"DISPLAY": ":91", "XAUTHORITY": "/synthetic/auth"}), \
             mock.patch.object(Path, "read_text", return_value="2345"), \
             mock.patch.object(HEADLESS.os, "getpgid", return_value=1), \
             mock.patch.object(HEADLESS.os, "getpgrp", return_value=2), \
             mock.patch.object(HEADLESS.os, "execv") as execute:
            with self.assertRaisesRegex(RuntimeError, "another session"):
                HEADLESS.child(self.args)
        execute.assert_not_called()
        self.assertFalse(self.args.state.exists())

    def test_owned_server_is_recorded_and_capture_is_opt_in(self):
        def server_text(path, *args, **kwargs):
            if str(path) == "/tmp/.X91-lock":
                return "2345"
            if str(path) == "/proc/2345/comm":
                return "Xvfb\n"
            raise AssertionError(f"unexpected read: {path}")
        with mock.patch.dict(os.environ, {"DISPLAY": ":91", "XAUTHORITY": "/synthetic/auth",
                                         "PS2X_CAPTURE_SCENE": "/inherited/capture"}), \
             mock.patch.object(Path, "read_text", autospec=True, side_effect=server_text), \
             mock.patch.object(HEADLESS.os, "getpgid", return_value=2), \
             mock.patch.object(HEADLESS.os, "getpgrp", return_value=2), \
             mock.patch.object(HEADLESS.os, "chdir"), \
             mock.patch.object(HEADLESS.os, "execv") as execute:
            HEADLESS.child(self.args)
            self.assertNotIn("PS2X_CAPTURE_SCENE", os.environ)
        record = json.loads(self.args.state.read_text())
        self.assertEqual(record["xvfb_pid"], 2345)
        self.assertEqual(record["display"], ":91")
        self.assertNotIn("capture_scene", record)
        self.assertEqual(self.args.state.stat().st_mode & 0o777, 0o600)
        execute.assert_called_once()

    def test_capture_requires_explicit_laboratory_runner(self):
        self.args.capture_scene = self.args.state.parent / "capture"
        with mock.patch.object(HEADLESS.shutil, "which", return_value="available"), \
             mock.patch.object(HEADLESS.subprocess, "Popen") as popen:
            with self.assertRaisesRegex(RuntimeError, "explicitly selected"):
                HEADLESS.launch(self.args)
        popen.assert_not_called()


if __name__ == "__main__":
    unittest.main()

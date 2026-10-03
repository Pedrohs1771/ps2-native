"""Isolation regressions; these fixtures never start a window or audio server."""
import importlib.util
import json
import os
from pathlib import Path
import tempfile
import subprocess
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
                                    log=root / "game.log", runner=None, capture_scene=None,
                                    disable_overlay_driver=False)

    def tearDown(self):
        self.temporary.cleanup()

    def test_fixed_high_display_does_not_use_wrapper_displayfd_auto_mode(self):
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
        self.assertNotIn("-a", command, "modern wrappers ignore the base when using displayfd")
        self.assertGreaterEqual(int(command[command.index("-n") + 1]), 90)
        self.assertTrue(popen.call_args.kwargs["start_new_session"])

    def test_display_selection_skips_existing_locks_and_sockets(self):
        root = self.args.state.parent
        sockets = root / "sockets"
        sockets.mkdir()
        (root / ".X90-lock").touch()
        (sockets / "X91").touch()
        self.assertEqual(HEADLESS.free_display_number(root, sockets), 92)

    def test_exhausted_private_display_range_is_a_failure(self):
        with mock.patch.object(Path, "exists", return_value=True):
            with self.assertRaisesRegex(RuntimeError, "display"):
                HEADLESS.free_display_number()

    def test_aot_launch_forwards_driver_disable_to_owned_child(self):
        self.args.disable_overlay_driver = True
        process = mock.Mock(pid=3456)
        process.poll.return_value = 0
        process.wait.return_value = 0
        loaded = SimpleNamespace(returncode=0, stdout="123\n")
        with mock.patch.object(HEADLESS.shutil, "which", return_value="available"), \
             mock.patch.object(HEADLESS.subprocess, "run", return_value=loaded), \
             mock.patch.object(HEADLESS.subprocess, "Popen", return_value=process) as popen, \
             mock.patch.object(HEADLESS.signal, "signal"), mock.patch("builtins.print"):
            HEADLESS.launch(self.args)
        self.assertIn("--disable-overlay-driver", popen.call_args.args[0])

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
                                         "PS2X_CAPTURE_SCENE": "/inherited/capture",
                                         "PS2X_NATIVE_OVERLAY_DRIVER": "/inherited/driver",
                                         "PULSE_PROP_OVERRIDE": "module-stream-restore.id=muted_desktop",
                                         "PULSE_PROP_application.name": "desktop_app"}), \
             mock.patch.object(Path, "read_text", autospec=True, side_effect=server_text), \
             mock.patch.object(HEADLESS.os, "getpgid", return_value=2), \
             mock.patch.object(HEADLESS.os, "getpgrp", return_value=2), \
             mock.patch.object(HEADLESS.os, "chdir"), \
             mock.patch.object(HEADLESS.os, "execv") as execute:
            HEADLESS.child(self.args)
            self.assertFalse("PS2X_CAPTURE_SCENE" in os.environ,"Inherited scene capture is removed")
            self.assertTrue("PS2X_NATIVE_OVERLAY_DRIVER" in os.environ,"Diagnostic driver is supplied")
            with self.args.state.open() as state_file:
                record = json.load(state_file)
            identity = record["audio_restore_id"]
            self.assertTrue(identity.startswith("ps2native_headless_"))
            self.assertEqual(os.environ["PULSE_PROP_OVERRIDE"],
                             "module-stream-restore.id=" + identity + " application.name=" + identity)
            self.assertNotIn("PULSE_PROP_application.name", os.environ)
        record = json.loads(self.args.state.read_text())
        self.assertEqual(record["xvfb_pid"], 2345)
        self.assertEqual(record["display"], ":91")
        self.assertNotIn("capture_scene", record)
        self.assertEqual(self.args.state.stat().st_mode & 0o777, 0o600)
        execute.assert_called_once()

    def test_aot_child_removes_inherited_overlay_driver(self):
        self.args.disable_overlay_driver = True
        def server_text(path, *args, **kwargs):
            if str(path) == "/tmp/.X91-lock": return "2345"
            if str(path) == "/proc/2345/comm": return "Xvfb\n"
            raise AssertionError(f"unexpected read: {path}")
        with mock.patch.dict(os.environ, {"DISPLAY": ":91", "XAUTHORITY": "/synthetic/auth",
                                         "PS2X_NATIVE_OVERLAY_DRIVER": "/inherited/driver"}), \
             mock.patch.object(Path, "read_text", autospec=True, side_effect=server_text), \
             mock.patch.object(HEADLESS.os, "getpgid", return_value=2), \
             mock.patch.object(HEADLESS.os, "getpgrp", return_value=2), \
             mock.patch.object(HEADLESS.os, "chdir"), \
             mock.patch.object(HEADLESS.os, "execv"):
            HEADLESS.child(self.args)
            self.assertFalse("PS2X_NATIVE_OVERLAY_DRIVER" in os.environ,"AOT has no diagnostic driver")

    def test_capture_requires_explicit_laboratory_runner(self):
        self.args.capture_scene = self.args.state.parent / "capture"
        with mock.patch.object(HEADLESS.shutil, "which", return_value="available"), \
             mock.patch.object(HEADLESS.subprocess, "Popen") as popen:
            with self.assertRaisesRegex(RuntimeError, "explicitly selected"):
                HEADLESS.launch(self.args)
        popen.assert_not_called()

    def test_duration_stops_only_owned_group_and_does_not_approve_menu(self):
        self.args.duration = 5.0
        process = mock.Mock(pid=3456)
        process.poll.side_effect = [None, None, -15]
        process.wait.side_effect = [subprocess.TimeoutExpired("fixture", 5), -15]
        loaded = SimpleNamespace(returncode=0, stdout="123\n")
        with mock.patch.object(HEADLESS.shutil, "which", return_value="available"), \
             mock.patch.object(HEADLESS.subprocess, "run", return_value=loaded) as run, \
             mock.patch.object(HEADLESS.subprocess, "Popen", return_value=process), \
             mock.patch.object(HEADLESS.signal, "signal"), \
             mock.patch.object(HEADLESS.os, "killpg") as kill, \
             mock.patch.object(HEADLESS, "environment", return_value=({"display": ":91", "runner_pid": 3456}, {})), \
             mock.patch.object(HEADLESS, "window", return_value="234"), \
             mock.patch("builtins.print"):
            result = HEADLESS.launch(self.args)
        kill.assert_called_once_with(3456, HEADLESS.signal.SIGTERM)
        self.assertEqual(result["status"], "duration_reached")
        self.assertTrue(result["window_ready"])
        self.assertFalse(result["menu_approved"])
        self.assertEqual(result["exit_code"], -15)
        self.assertEqual(run.call_args.args[0], ["pactl", "unload-module", "123"])
        self.assertGreater(process.wait.call_args_list[0].kwargs["timeout"], 0)
        self.assertLessEqual(process.wait.call_args_list[0].kwargs["timeout"], 5)

    def test_invalid_duration_fails_before_creating_audio_or_process(self):
        for duration in (0, -1, float("nan"), float("inf"), 3601):
            self.args.duration = duration
            with mock.patch.object(HEADLESS.subprocess, "Popen") as popen, \
                 mock.patch.object(HEADLESS.subprocess, "run") as run:
                with self.assertRaisesRegex(RuntimeError, "duration"):
                    HEADLESS.launch(self.args)
            popen.assert_not_called()
            run.assert_not_called()

    def test_bounded_early_failure_is_not_reported_as_duration_reached(self):
        self.args.duration = 5.0
        process = mock.Mock(pid=3456)
        process.poll.return_value = 4
        process.wait.return_value = 4
        loaded = SimpleNamespace(returncode=0, stdout="123\n")
        with mock.patch.object(HEADLESS.shutil, "which", return_value="available"), \
             mock.patch.object(HEADLESS.subprocess, "run", return_value=loaded), \
             mock.patch.object(HEADLESS.subprocess, "Popen", return_value=process), \
             mock.patch.object(HEADLESS.signal, "signal"), mock.patch("builtins.print"):
            with self.assertRaisesRegex(RuntimeError, "status 4"):
                HEADLESS.launch(self.args)


if __name__ == "__main__":
    unittest.main()

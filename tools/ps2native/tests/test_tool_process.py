"""Real subprocess regressions for bounded build tools and their children."""
import json
import os
from pathlib import Path
import signal
import sys
import tempfile
import unittest
from unittest import mock

from tools.ps2native import pipeline
from tools.ps2native.pipeline import PipelineError, _run


class ToolProcessTests(unittest.TestCase):
    @unittest.skipUnless(os.name == "posix" and Path("/proc").is_dir(), "Linux process ownership fixture")
    def test_timeout_terminates_descendant_compiler_in_owned_group(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            child_record = root / "child.json"
            script = root / "parent.py"
            script.write_text(
                "import json, os, subprocess, sys, time\n"
                "from pathlib import Path\n"
                "child = subprocess.Popen([sys.executable, '-c', 'import time; time.sleep(30)'], "
                "stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)\n"
                "Path(sys.argv[1]).write_text(json.dumps({'pid': child.pid, 'group': os.getpgrp()}))\n"
                "print('compiler started', flush=True)\n"
                "time.sleep(30)\n"
            )
            try:
                with self.assertRaisesRegex(PipelineError, "timed out after 1s"):
                    _run([sys.executable, str(script), str(child_record)], root / "tool.log", root, 1)
                child = json.loads(child_record.read_text())
                status = Path(f'/proc/{child["pid"]}/status')
                active = status.exists() and "State:\tZ" not in status.read_text()
                self.assertFalse(active, "timed-out tool left its compiler running")
                self.assertIn("compiler started", (root / "tool.log").read_text())
                self.assertNotEqual(child["group"], os.getpgrp())
            finally:
                if child_record.exists():
                    child_pid = json.loads(child_record.read_text())["pid"]
                    try:
                        os.kill(child_pid, signal.SIGKILL)
                    except ProcessLookupError:
                        pass

    def test_failure_retains_stderr_and_status(self):
        with tempfile.TemporaryDirectory() as directory:
            log = Path(directory) / "failure.log"
            with self.assertRaisesRegex(PipelineError, "status 7"):
                _run([sys.executable, "-c", "import sys; print('real diagnostic', file=sys.stderr); sys.exit(7)"],
                     log, None, 5)
            self.assertIn("real diagnostic", log.read_text())

    def test_arguments_remain_literal(self):
        with tempfile.TemporaryDirectory() as directory:
            log = Path(directory) / "tool.log"
            argument = "space ' quote $() `literal`"
            output = _run([sys.executable, "-c", "import sys; print(sys.argv[1])", argument], log, None, 5)
            self.assertEqual(output, argument + "\n")
            self.assertEqual(log.read_text(), output)


class DesktopConfigurationReceiptTests(unittest.TestCase):
    def test_boot_config_attestation_includes_injected_module_identity(self):
        with tempfile.TemporaryDirectory() as directory:
            workspace = Path(directory)
            config = workspace / "analysis/ps2native.toml"
            config.parent.mkdir()
            config.write_text('[general]\ninput = "synthetic.elf"\n')
            before = pipeline.sha256_file(config)
            manifest = {"build_inputs": {}}
            pipeline._configure_boot_recompiler(config, workspace, manifest, ["fixture"], "fixture_")
            attestation = manifest["build_inputs"]["analysis_config"]
            self.assertEqual(attestation["path"], "analysis/ps2native.toml")
            self.assertEqual(attestation["sha256"], pipeline.sha256_file(config))
            self.assertNotEqual(attestation["sha256"], before)
            self.assertEqual(attestation["size_bytes"], config.stat().st_size)
            self.assertIn('module_emit_dense_function_table = true', config.read_text())

    def test_receipt_matches_pch_and_worker_options_sent_to_cmake(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            generated, disc, package = (root / name for name in ("generated", "disc", "package"))
            generated.mkdir(); disc.mkdir()
            (generated / "function.cpp").write_text("// synthetic source\n")
            (disc / "boot.elf").write_bytes(b"synthetic fixture")
            manifest = {"steps": [], "build_inputs": {"configuration": {
                "precompiled_headers": False, "unity_build": True, "build_jobs": 2}}}
            commands = []
            def tool(command, *unused):
                commands.append(command)
                if "--install" in command:
                    (package / "bin").mkdir(parents=True)
                    (package / "bin/ps2EntryRunner").write_bytes(b"synthetic runner; never executed")
                return ""
            with mock.patch.object(pipeline, "_find_tool", return_value=Path("/synthetic/cmake")), \
                 mock.patch.object(pipeline, "_run", side_effect=tool):
                pipeline._configure_and_package(root, root / "work", generated, disc, package,
                                                None, 4, 30, manifest)
            self.assertIn("-DPS2X_ENABLE_RUNNER_PCH:BOOL=ON", commands[0])
            self.assertIn("-DPS2X_ENABLE_RUNNER_UNITY_BUILD:BOOL=OFF", commands[0])
            self.assertEqual(commands[1][-2:], ["--parallel", "4"])
            configuration = manifest["build_inputs"]["configuration"]
            self.assertTrue(configuration["precompiled_headers"])
            self.assertFalse(configuration["unity_build"])
            self.assertEqual(configuration["build_jobs"], 4)


if __name__ == "__main__":
    unittest.main()

from pathlib import Path
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]


class RuntimeCaptureBuildTests(unittest.TestCase):
    def test_desktop_capture_hooks_configure_without_codegen_or_lab_executables(self):
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary)
            (source/'empty.cpp').write_text('int owned_fixture() { return 0; }\n')
            (source/'CMakeLists.txt').write_text(
                'cmake_minimum_required(VERSION 3.21)\nproject(DesktopCaptureHooks LANGUAGES CXX)\n'
                f'set(PROJECT_SOURCE_DIR "{ROOT.as_posix()}")\n'
                'set(PS2X_NEXO_CAPTURE_ONLY ON)\n'
                'set(CMAKE_EXPORT_COMPILE_COMMANDS ON)\n'
                'add_library(ps2_runtime STATIC empty.cpp)\n'
                'add_library(ps2_iop STATIC empty.cpp)\n'
                f'add_subdirectory("{(ROOT/"lab").as_posix()}" lab)\n'
                'if(TARGET ps2_native_overlay OR TARGET nexo_vu_native OR TARGET nexo_ee_autoadapt_fixture)\n'
                'message(FATAL_ERROR "The desktop capture build added offline laboratory tools")\nendif()\n')
            build = source/'build'
            configured = subprocess.run(['cmake', '-S', str(source), '-B', str(build)],
                                        capture_output=True, text=True, timeout=30)
            self.assertEqual(configured.returncode, 0, configured.stdout+configured.stderr)
            commands = (build/'compile_commands.json').read_text()
            for source_file in ('ee_miss_capture.cpp', 'ee_snapshot.cpp', 'iop_capture.cpp'):
                self.assertIn(source_file, commands)
            self.assertIn('PS2X_NEXO_LAB=1', commands)
            self.assertNotIn('ee_autoadapt_fixture.cpp', commands)


if __name__ == '__main__':
    unittest.main()

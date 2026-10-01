from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
HELPER = ROOT / 'ps2xRuntime/cmake/iop_runtime_backend.cmake'


class IopRuntimeBackendBuildTests(unittest.TestCase):
    def test_make_profile_switch_only_rebuilds_the_backend_object(self):
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary)
            build = source / 'build'
            (source / 'runtime.cpp').write_text(
                'extern int backend(); int main() { return backend(); }\n')
            (source / 'backend.cpp').write_text(
                'int backend() { return PS2X_RUNTIME_NATIVE_IOP; }\n')
            (source / 'CMakeLists.txt').write_text(
                'cmake_minimum_required(VERSION 3.21)\nproject(IopBackendProfile CXX)\n'
                'option(NATIVE "Native backend" OFF)\n'
                'add_library(ps2_iop INTERFACE)\n'
                'add_executable(runtime runtime.cpp)\n'
                f'include("{HELPER.as_posix()}")\n'
                'ps2x_add_iop_runtime_backend(runtime backend "${CMAKE_CURRENT_SOURCE_DIR}/backend.cpp" "${NATIVE}")\n')
            runtime_object = build / 'CMakeFiles/runtime.dir/runtime.cpp.o'
            backend_object = build / 'CMakeFiles/backend.dir/backend.cpp.o'
            previous_runtime = previous_backend = None
            for native in (False, True, False):
                configured = subprocess.run(['cmake', '-G', 'Unix Makefiles', '-S', str(source),
                                             '-B', str(build), f'-DNATIVE={"ON" if native else "OFF"}'],
                                            capture_output=True, text=True, timeout=30)
                self.assertEqual(configured.returncode, 0, configured.stdout + configured.stderr)
                built = subprocess.run(['cmake', '--build', str(build), '-j', '2'],
                                       capture_output=True, text=True, timeout=30)
                self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
                runtime_state = (runtime_object.stat().st_mtime_ns, runtime_object.read_bytes())
                backend_state = (backend_object.stat().st_mtime_ns, backend_object.read_bytes())
                if previous_runtime is not None:
                    self.assertEqual(runtime_state, previous_runtime,
                                     'The profile switch recompiled unrelated runtime code')
                    self.assertNotEqual(backend_state, previous_backend)
                    self.assertNotIn('Building CXX object CMakeFiles/runtime.dir/runtime.cpp.o', built.stdout)
                previous_runtime, previous_backend = runtime_state, backend_state
                result = subprocess.run([str(build / 'runtime')], timeout=10)
                self.assertEqual(result.returncode, int(native))


if __name__ == '__main__':
    unittest.main()

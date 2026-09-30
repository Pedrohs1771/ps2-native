# ELF to native runner handoff

Audit was run in an isolated tree at `/tmp/ps2-native-handoff-75d729c-20260928`; no production source or CMake file was edited for this test. The host build and a generated-function desktop link both succeeded. Android APK compilation was not run because this environment has no `gradle`, `ANDROID_HOME`, or `ANDROID_SDK_ROOT`.

## Existing host pipeline

`ps2_analyzer <input.elf> <output.toml>` discovers functions and writes a TOML next to the requested file. Its generator sets `general.output` to a sibling `output/` directory and `single_file_output = false` by default. `ps2_recomp <config.toml>` then emits one `.cpp` per function plus `register_functions.cpp`, `ps2_recompiled_functions.h`, and `ps2_recompiled_stubs.h` into that directory. Relevant sources are `ps2xAnalyzer/src/analyzer_main.cpp`, `ps2xAnalyzer/src/toml_generator.cpp`, `ps2xRecomp/src/runner/main.cpp`, and `ps2xRecomp/src/lib/ps2_recompiler.cpp`.

The default runtime target does **not** link the configured recompiler output directory. `ps2xRuntime/CMakeLists.txt` builds `ps2EntryRunner` from the runtime runner sources and links it to `ps2_runtime`; its source glob also picks up `ps2xRuntime/src/runner/register_functions.cpp`, which is an empty-table placeholder. A game build must add the generated function sources and headers and replace that placeholder’s table source. Otherwise the executable does not contain the generated game entrypoint.

## Reproduced smoke path

Host tools were built outside the checkout. The root `PS2X_BUILD_RUNTIME=OFF` option keeps raylib and runner dependencies out of the tool build; the analyzer still builds its required `ps2_recomp_lib` dependency.

```sh
REPO=/home/pedrohs/Downloads/ps2-native-recompiler
WORK=/tmp/ps2-native-handoff-75d729c-20260928

mkdir -p "$WORK/host" "$WORK/fixture"
cmake -S "$REPO" -B "$WORK/host" \
  -DCMAKE_BUILD_TYPE=Release \
  -DPS2X_BUILD_RUNTIME=OFF \
  -DPS2X_BUILD_TEST=OFF \
  -DPS2X_BUILD_STUDIO=OFF
cmake --build "$WORK/host" --target ps2_analyzer ps2_recomp -j2
```

For a toolchain smoke test, this Python snippet writes a 264-byte ELF32 little-endian MIPS executable with one executable PT_LOAD segment at `0x00100000`. Its two instructions are `jr $ra; nop`; this checks the handoff and is not a game-compatibility test.

```sh
python3 - "$WORK/fixture/mini.elf" <<'PY'
import os, struct, sys
out = sys.argv[1]
os.makedirs(os.path.dirname(out), exist_ok=True)
ident = b'\x7fELF' + bytes([1, 1, 1, 0, 0]) + bytes(7)
code = struct.pack('<II', 0x03e00008, 0)
header = struct.pack('<16sHHIIIIIHHHHHH', ident, 2, 8, 1,
                     0x00100000, 52, 0, 0, 52, 32, 1, 40, 0, 0)
ph = struct.pack('<IIIIIIII', 1, 0x100, 0x00100000, 0x00100000,
                 len(code), len(code), 5, 0x1000)
with open(out, 'wb') as f:
    f.write(header)
    f.write(ph)
    f.write(bytes(0x100 - f.tell()))
    f.write(code)
PY

"$WORK/host/ps2xAnalyzer/ps2_analyzer" \
  "$WORK/fixture/mini.elf" "$WORK/fixture/config.toml"
"$WORK/host/ps2xRecomp/ps2_recomp" "$WORK/fixture/config.toml"
```

Observed analyzer/recompiler result: one discovered and recompiled function, zero decode failures, zero unhandled instructions. Files produced in `$WORK/fixture/output/` were `sub_00100000_0x100000.cpp`, `register_functions.cpp`, `ps2_recompiled_functions.h`, and `ps2_recompiled_stubs.h`.

## Desktop link and run

This temporary CMake wrapper proves the missing handoff without changing the repository’s target definition. It adds generated `.cpp` files to `ps2EntryRunner`, adds the generated-header include directory, and filters out the checked-in empty registration table. It also disables optional debug UI and FFmpeg for this small test:

```sh
mkdir -p "$WORK/link"
cat > "$WORK/link/CMakeLists.txt" <<'EOF'
cmake_minimum_required(VERSION 3.21)
project(ps2_native_handoff LANGUAGES C CXX)

set(PS2X_BUILD_RECOMP OFF CACHE BOOL "" FORCE)
set(PS2X_BUILD_ANALYZER OFF CACHE BOOL "" FORCE)
set(PS2X_BUILD_TEST OFF CACHE BOOL "" FORCE)
set(PS2X_BUILD_STUDIO OFF CACHE BOOL "" FORCE)
set(PS2X_ENABLE_DEBUG_UI OFF CACHE BOOL "" FORCE)
set(PS2X_ENABLE_FFMPEG OFF CACHE BOOL "" FORCE)
set(PS2X_ENABLE_RUNNER_PCH OFF CACHE BOOL "" FORCE)
set(PS2X_ENABLE_SCCACHE OFF CACHE BOOL "" FORCE)

add_subdirectory("/home/pedrohs/Downloads/ps2-native-recompiler" ps2x)

get_target_property(runner_sources ps2EntryRunner SOURCES)
list(FILTER runner_sources EXCLUDE REGEX "register_functions\\.cpp$")
set_property(TARGET ps2EntryRunner PROPERTY SOURCES "${runner_sources}")
file(GLOB generated_title_sources CONFIGURE_DEPENDS
     "/tmp/ps2-native-handoff-75d729c-20260928/fixture/output/*.cpp")
target_sources(ps2EntryRunner PRIVATE ${generated_title_sources})
target_include_directories(ps2EntryRunner PRIVATE
    "/tmp/ps2-native-handoff-75d729c-20260928/fixture/output")
EOF

cmake -S "$WORK/link" -B "$WORK/desktop" -DCMAKE_BUILD_TYPE=Release
cmake --build "$WORK/desktop" --target ps2EntryRunner -j2
xvfb-run -a gdb --batch \
  -ex 'set debuginfod enabled off' \
  -ex 'set pagination off' \
  -ex 'break sub_00100000_0x100000' \
  -ex run \
  --args "$WORK/desktop/ps2x/ps2xRuntime/ps2EntryRunner" "$WORK/fixture/mini.elf"
```

This configures, compiles, and breaks in the translated guest function under a virtual X server.

Observed: the host build linked `ps2_runtime`, `ps2_iop`, raylib, generated function code, and the generated table. GDB hit `sub_00100000_0x100000` on the `GameThread`, confirming the guest ELF entry dispatched to the translated host function. The isolated desktop configure used `PS2X_ENABLE_FFMPEG=OFF`; on non-Windows desktop builds FFmpeg is on by default and its development libraries are found through pkg-config. Debug UI is on by default and can be disabled with `PS2X_ENABLE_DEBUG_UI=OFF`.

For an interactive run without GDB, run the same `ps2EntryRunner <mini.elf>` command under a desktop display. The smoke function returns to guest PC zero; the raylib window remains open, so the app waits for its window loop to close. In the headless smoke run, a five-second timeout returned 124 after initialization and entry dispatch.

## Required Android wiring

The Android APK should compile the **same generated title sources** with the NDK. Keep `ps2_analyzer` and `ps2_recomp` as desktop host tools; root CMake intentionally disables them under `ANDROID`. In `ps2xRuntime/CMakeLists.txt`, add a cache path such as `PS2X_GENERATED_CODE_DIR`, glob the generated directory’s `.cpp` files, remove the checked-in `src/runner/register_functions.cpp` from `RUNNER_SRC_FILES` when generated code is supplied, append the generated sources, and add that directory to `ps2EntryRunner`’s private include paths. The current Android runner target is a shared library named `ps2EntryRunner`; those source/include changes let Gradle’s existing external CMake target compile and link the game functions into `libps2EntryRunner.so`.

In `android/app/build.gradle`, read a `ps2xGeneratedCodeDir` Gradle property and pass it as `-DPS2X_GENERATED_CODE_DIR=...` in the existing CMake arguments. Build after host recompile, for example `gradle -p android assembleRelease -Pps2xGeneratedCodeDir=/path/to/generated`. Use a unique staging directory per title/build; do not copy generated files over the checked-in placeholder or include host executables in the APK. The Gradle target already names `ps2EntryRunner` and filters Android ABIs; `arm64-v8a` is the phone target, while Android `x86_64` is for Android devices/emulators and is distinct from a desktop x86-64 executable.

Generated code alone is insufficient to boot a game. `PS2Runtime::loadELF()` still loads the original guest ELF to initialize guest memory and data. The current Android README uses a device-side ELF path plus `adb push`; a self-contained APK additionally needs the boot ELF and game files packaged as assets, copied into app-private storage, and resolved to a device path. Do not pass the desktop build machine’s ELF path as `PS2X_DEFAULT_BOOT_ELF`.

The Android dependency path uses the root `sse2neon` FetchContent shim for ARM. The checked-in `ps2xRuntime/CMakeLists.txt` currently also has an in-progress x86 compile-option change that propagates SSE4.1 to the runtime/runner; the desktop link above used that working-tree change. Keep host SIMD requirements explicit when integrating generated sources. Android’s `PS2X_ENABLE_FFMPEG` default is off, so MPEG video uses the runtime’s stub-frame path unless an Android FFmpeg build is added.

## What “native” means in this build

The recompiler emits C++ for EE/R5900 functions; the desktop compiler or Android NDK turns those functions into x86-64 or ARM64 machine code. That is native host code for the translated EE functions. The generated code still operates on `R5900Context` and guest RAM, and calls `PS2Runtime` for branches, syscalls, MMIO, scheduling, graphics, audio, and other PS2 services. Other processors remain runtime subsystems: `ps2xIOP` contains an R3000A interpreter, and VU0 microprograms execute through the runtime VU core. This handoff is an AOT EE code recompiler linked to a PS2 compatibility runtime, not recovered game-engine source or a runtime-free native port.

The smoke test establishes only that one supported `jr $ra` ELF can be analyzed, translated, compiled into a native desktop runner, loaded, and dispatched. It says nothing about an arbitrary commercial title’s instruction coverage, dynamic overlays, libraries, disc reads, or full-game behavior.

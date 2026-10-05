# Review and reproduce PS2Native

This guide separates runnable public evidence from historical private observations. Examine automatic, bounded offline recovery of uncovered EE code, guarded reuse, and explicit rejection of unverified compatibility claims.

## First checks

Use Python 3.10+ and CMake 3.21+ on `PATH`. No commercial input is needed:

```sh
python3 -m tools.ps2native --help
python3 -m unittest discover -s tools/ps2native/tests
python3 -m unittest lab.tests.test_ee_data_families lab.tests.test_ee_family_catalog lab.tests.test_prepare_ee_family_batch lab.tests.test_ee_context_inventory
```

Publication-preparation checkpoint: **74 CLI tests and 87 laboratory tests passed**. Counts are observations, not compatibility metrics. Read current CI for later revisions.

Build IOP independently of graphics dependencies:

```sh
cmake -S ps2xIOP -B build-iop -DCMAKE_BUILD_TYPE=Release -DPS2X_IOP_BUILD_TESTS=ON -DPS2X_IOP_BUILD_LAB=ON
cmake --build build-iop --parallel 4
ctest --test-dir build-iop --output-on-failure
```

All **8 registered IOP CTest suites passed on Linux/GCC 13** during publication preparation. They cover generated native banks, RPC/import behavior, code generation, and diagnostic interpreter tests. A constructor change removes a noncopyable temporary from `IopSubsystem`; MSVC verification belongs to Windows CI.

## Native EE recovery demonstration

This Linux-only fixture uses project-owned MIPS words with the production runtime and recovery pipeline. It contains no disc data. Use the root `build` directory and **Unix Makefiles**: the test currently reads CMake `link.txt` and invokes `/usr/bin/c++`.

On Ubuntu:

```sh
sudo apt-get update
sudo apt-get install -y build-essential cmake pkg-config libgl-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libavcodec-dev libavformat-dev libavutil-dev libswresample-dev libswscale-dev xvfb xdotool imagemagick pulseaudio-utils
cmake -S . -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS=-msse4.1 -DPS2X_BUILD_STUDIO=OFF -DPS2X_BUILD_ANALYZER=OFF -DPS2X_BUILD_TEST=OFF -DPS2X_ENABLE_DEBUG_UI=OFF -DPS2X_BUILD_NEXO_LAB=ON -DPS2X_RUNTIME_AOT_EE_OVERLAYS=ON
cmake --build build --target ps2_native_overlay nexo_ee_autoadapt_fixture --parallel 4
python3 tools/check_native_demo.py
```

A fresh CMake build fetches the declared third-party dependencies. The existing adaptation context checks headless helper binaries even though this fixture opens no game window. The runner rejects missing targets and any skipped tests.

Assertions verify one contextual miss capture and exit `73`, two offline recoveries producing `123`, two reused cases and zero recoveries on the next conversion, thread/KSEG0/KSEG1 behavior, and false menu/native-qualification/live-compiler fields.

This proves the fixture's compiler/runtime contract, not commercial compatibility. During publication preparation, the [source evidence workflow](https://github.com/Pedrohs1771/ps2-native/actions/runs/37245389803) built the fixture from source and passed all four integration tests with no skips. Linux GCC and Clang also passed the full 525-test C++ suite. These results apply to the preparation revision; consult current CI for later changes.

## Follow the implementation

| Question | Start here |
|---|---|
| Miss capture and replay | `tools/ps2native/autoadapt.py`, `lab/tests/test_autoadaptation_execution.py` |
| Demonstration code | `lab/tests/ee_autoadapt_fixture.cpp` |
| Guards and catalog generation | `lab/prepare_ee_miss.py`, `lab/generate_ee_bank_catalog.py` |
| Conservative package qualification | `tools/ps2native/conversion.py`, `tools/ps2native/pipeline.py` |
| Scheduler and IOP regressions | `ps2xTest/`, `ps2xIOP/tests/`, `lab/tests/` |

## Historical observations and open gates

The [03 October adaptation checkpoint](ADAPTACAO_AUTONOMA_20261003.md) reports private replays and their limitations. Its local receipt paths are not in a public clone. Later diagnostics corrected an earlier all-zero audio measurement; dated notes remain historical observations.

The commercial corpus has **0/5 qualified menus**. Metal Slug 4 showed an animated intro and nonzero PCM, with navigation unqualified. Other titles remain blocked or incomplete. The latest scheduler delta still needs commercial replay validation.

Use the [roadmap](../ROADMAP.md) and [Beta gates](BETA_V01.md) to assess the next deliverable. No adoption count or compatibility percentage is asserted. This is an actively developed experimental toolchain seeking reproducible scrutiny and contributors.

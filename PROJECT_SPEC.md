# PS2 Native Recompiler — Project Specification

## Status

Draft baseline for the active project. The long-term objective remains: accept PS2 disc images and generate complete native builds for Android and PC with a plug-and-play workflow. This specification does not claim universal compatibility exists today; it defines the architecture and evidence required to earn that claim.

Current working-tree progress: supported ISO9660/Joliet intake, `SYSTEM.CNF` boot selection, content-deduplicated ELF inventory, boot-ELF AOT compilation, heuristic MIPS III secondary-ELF compilation, module-scoped function registration, isolated desktop packaging, and Android per-title project/asset staging are implemented. SIF loads activate matching compiled ranges with overlap ownership. Other MIPS executables and arbitrary overlays remain unresolved; Android APK generation and real-title playability remain unverified.

## Objective

Build an open-source toolchain that takes a user-supplied PS2 ISO, discovers its executable code and data, statically recompiles EE/IOP guest code to native host code, links a reusable PS2 compatibility runtime, and packages a game-specific Android APK or PC executable.

“Native” means EE/IOP instructions are translated ahead of time to ARM64 or x86-64 machine code and execute as host code. A shared runtime still implements PS2 device semantics (GS, VU, DMA, SPU2, CDVD, IOP services, timing, input, and saves). Dynamically generated or undiscovered guest code may use a selective fallback path; the normal game CPU path must not require a full-system PS2 CPU emulator.

The source project, translator, runtime, profiles, and packaging tools are open source. Game ISOs, extracted files, firmware, and generated game-derived builds remain local inputs/outputs unless separately licensed for redistribution.

## Assumptions

1. PS2 is the first and only console family in this project; `.iso` does not imply a console or executable format.
2. Initial host targets are Android ARM64-v8a, Windows x86-64, and Linux x86-64. The architecture should permit additional PC/mobile targets.
3. A desktop builder accepts a local ISO and emits an installable APK or a desktop executable. Runtime users should not need to hand-edit configuration or extract files manually for supported games.
4. User-provided game data is read locally and is not uploaded, committed, or included in public source releases.
5. The initial codebase is the public GPL-3.0 PS2Recomp project in this directory. Preserve upstream notices and keep the full derivative project GPL-compatible.
6. A title-specific compatibility profile is an allowed automated fallback when static discovery or generic hardware behavior is insufficient; the long-term product goal remains broad ISO coverage and low-touch use.

## Product Flow

```text
Local PS2 ISO
  -> inspect disc and SYSTEM.CNF
  -> locate boot ELF, IRX modules, overlays, and assets
  -> analyze code/data and discover guest entry points
  -> lower R5900/IOP/VU code into host-independent IR
  -> emit AArch64 or x86-64 native code
  -> link PS2 compatibility runtime and user assets
  -> sign/package APK or stage desktop executable
```

## Interface

Initial interface is a desktop CLI so the end-to-end pipeline can be automated and debugged. A GUI can call the same stable library/commands later.

```sh
python3 -m tools.ps2native inspect --iso "/path/game.iso" --json-output
python3 -m tools.ps2native build --iso "/path/game.iso" --target android --out out/android-package
python3 -m tools.ps2native build --iso "/path/game.iso" --target desktop --out out/desktop-package
```

The current `desktop` target builds for the host machine ABI. Separate Windows,
Linux, and macOS cross-compilation targets are future work; the current
`android` target emits an ARM64 APK only when its Gradle/SDK/NDK toolchain is
installed.

The build report must list game ID, region/revision, source hashes, discovered executables/overlays, translated instruction coverage, runtime services used, target ABI, compatibility profile, warnings, and unresolved behavior.

## Technical Stack

- C++20 and CMake for the shared runtime, translator, analysis libraries, and packager.
- Existing PS2Recomp modules are the starting point: `ps2xAnalyzer`, `ps2xRecomp`, `ps2xRuntime`, and `ps2xIOP`.
- A host-independent guest IR separates instruction semantics from ARM64 and x86-64 code generation.
- Android builds use Android NDK/Gradle and produce ARM64-v8a APKs; desktop packaging is target-specific.
- Vulkan is the preferred shared graphics backend where available, behind a GS-facing abstraction. Audio, input, files, clocks, and threading also sit behind host interfaces.

## Core Components

1. **Disc and executable intake:** ISO9660 directory reader, `SYSTEM.CNF` parser, boot ELF and IRX discovery, checksums, deterministic manifest, and clear diagnostics for malformed/unsupported images.
2. **Binary analysis:** ELF segments/relocations/symbols, function boundary discovery, CFG recovery, jump tables, overlay/code-region tracking, confidence labels, and optional user-supplied Ghidra maps.
3. **Guest semantics:** explicit R5900 MIPS, MMI, COP0/COP1, branch-delay, exception, memory, and IOP/R3000A semantics. Unsupported instructions must be diagnosed, never silently replaced with NOPs.
4. **Static recompilation:** lift to a host-independent IR, optimize without changing guest-visible behavior, emit AArch64 and x86-64, and register code for overlays/dynamic entry points.
5. **Selective dynamic fallback:** detect code writes, invalidate stale translated blocks, and route unknown or runtime-generated blocks through an interpreter/JIT fallback. Report every fallback path.
6. **PS2 compatibility runtime:** EE/IOP memory and scheduling; VU0/VU1; DMAC/VIF/GIF/SIF; GS; SPU2; IPU/CDVD; BIOS/HLE services; controller; memory card; timing; and per-game override/profile hooks.
7. **Host adapters:** Android APK lifecycle, rendering surface, audio, controller/touch mapping, scoped storage, and save storage; equivalent desktop window/audio/input/file adapters.
8. **Packaging:** private local asset staging, native library linking, app metadata, debug/release signing options, launch configuration, and reproducible build manifest.
9. **Compatibility database:** public metadata and tests keyed to exact game ID, region, revision, and executable hash. Profiles may contain code/configuration but not game content.

## Project Structure

```text
ps2xAnalyzer/             ISO/ELF analysis and code discovery
ps2xRecomp/               Guest IR and native code generators
ps2xRuntime/              Shared PS2 hardware compatibility services
ps2xIOP/                  IOP execution and service bridge
android/                  Android runner and APK packaging
desktop/                  Windows/Linux runner and packaging
tools/                    ISO inspection, build orchestration, manifests
profiles/                 Source-only compatibility profiles
docs/                     Architecture, support matrix, reverse-engineering notes
PROJECT_SPEC.md           Product contract and acceptance criteria
```

## Code Style

Keep guest state explicit and independent from host ABI. Centralize guest address translation and never cast guest addresses directly to host pointers.

```cpp
struct GuestAddress {
    std::uint32_t value;
};

Result<BootManifest> inspectDisc(const std::filesystem::path& isoPath);
```

Use descriptive lower-camel-case functions, PascalCase types, RAII for host resources, structured diagnostics with guest address/instruction context, and deterministic output ordering. All guest-visible arithmetic and flags must use named semantic helpers instead of host-dependent implicit behavior.

## Build, Run, and Verification Commands

Existing upstream baseline:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/ps2_analyzer --help
./build/ps2_recomp --help
```

Current prototype commands:

```sh
python3 -m tools.ps2native inspect --iso game.iso --json-output
python3 -m tools.ps2native build --iso game.iso --target desktop --out out/desktop-package
python3 -m tools.ps2native build --iso game.iso --target android --out out/android-package
```

Android packaging must have a documented NDK/Gradle invocation and produce a signed installable APK. Full-game compatibility claims require repeatable cold-boot gameplay runs and a public status row for the exact ISO/executable hash.

## Compatibility and Completion Criteria

The long-term success criterion is that a user supplies any supported PS2 ISO and receives a complete native PC build and/or Android APK without editing project files. Support means the exact game/revision reaches playable gameplay and correctly handles its menus, scenes/FMVs where present, audio, controls, save/load, and transitions. If a game requires a profile, the profile must be selected automatically from its identity and hash.

Every accepted ISO must produce one of three explicit outcomes: **buildable**, **buildable with an automatically selected profile**, or a diagnostic report identifying the unsupported binary/hardware behavior. Silent partial builds are failures.

“Any PS2 ISO” is not considered proven by a single demo. Completion requires a compatibility corpus covering the target library, automatic code discovery for retail stripped executables and overlays, complete hardware-service coverage for observed title behavior, and native runtime verification on Android ARM64 and PC x86-64. Compatibility reports must separate CPU translation coverage from full-game playability.

## Phased Delivery

1. **Baseline and intake:** preserve upstream build, document its gaps, parse ISO and `SYSTEM.CNF`, hash-deduplicate ELF candidates, distinguish the boot executable from MIPS III secondary candidates and IRX candidates, and emit a deterministic report. The boot path and heuristic MIPS III module handoff are implemented; retail classification coverage remains.
2. **Compiler foundation:** establish semantic IR, complete instruction coverage accounting, decoder/runtime differential tooling, ARM64 and x86-64 backends.
3. **End-to-end host build:** ISO-to-native desktop output including overlays, assets, basic boot, controller, audio, and saves.
4. **Android target:** cross-compile runtime/native code, generate APK, input/audio/rendering/storage integration, installable build.
5. **Dynamic code and hardware completion:** discover/recompile or selectively interpret dynamic blocks; close EE/IOP/VU/GS/DMA/SPU2/CDVD service gaps.
6. **Universalization:** build a broad PS2 compatibility corpus, auto-match profiles, fix regressions, publish per-title/per-region compatibility evidence, and remove manual setup for supported images.

## Boundaries

- **Always:** preserve upstream licenses; read user ISOs locally; keep game-derived files out of source releases; retain original hashes and exact revision metadata; expose incomplete coverage honestly.
- **Ask first:** change upstream project license, publish generated game-specific artifacts, or add network upload/telemetry of any user files.
- **Never:** bundle a commercial ISO, BIOS image, extracted assets, generated executable code, or unreviewed third-party game content in public releases.

## Current Known Risks

- The host recompiler/analyzer/packager are now separate from the Android runtime build, but no Android APK has been built in the current environment.
- Secondary MIPS `ET_EXEC` candidates are ambiguous between EE and IOP code from basic ELF headers alone. Compiling them as R5900 code without further classification would be incorrect.
- The boot path keeps its legacy global dense function table and registers sparse aliases for SIF loads. Generated MIPS III modules register sparse function maps per runtime; SIF loads activate path-matched ranges and newest loads own overlaps. Failed heuristic candidates remain visible in the manifest. Named partial `SifLoadElfPart` loads currently fail without writing memory. The e_flags classification is heuristic, and buffer-loaded modules and arbitrary overlays are not covered.
- Static control-flow analysis cannot guarantee discovery of indirect jumps, overlays, or runtime-generated code.
- VU microcode, GS behavior, DMA ordering, timing, IOP modules, and firmware services are substantial compatibility subsystems.
- The provided `JUYL Unlimited Codes.rar` is password-protected, so its ISO, region, and title remain unverified until an accessible archive/password is supplied.

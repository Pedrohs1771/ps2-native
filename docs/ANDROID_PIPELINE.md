# ISO-to-Android APK Build Pipeline

## Purpose and boundary

This document specifies a desktop-hosted build that accepts a locally supplied PS2 ISO, runs the existing analysis and static recompiler tools on the desktop, then cross-compiles the generated C++ and PS2 runtime into an ARM64 Android APK.

The recompiler and analyzer remain host programs. The APK contains the Android runtime, the generated title-specific code, and the game files selected for that package. It does not contain the analyzer, the recompiler, or a general-purpose ISO-to-APK builder.

This produces a **game-specific native build backed by the PS2 compatibility runtime**. It does not turn arbitrary PS2 engine code into source code or remove the runtime's responsibility for PS2 memory, graphics, audio, IOP, and device behavior. A title is complete only after its generated code, runtime paths, assets, and supported device behavior have been validated together.

## Implementation status (2026-09-28)

The checkout now contains `tools/iso_inspect/`, the host-side `tools/ps2native`
orchestrator, an isolated desktop CMake package template, and Android Gradle /
CMake / `Ps2PackageActivity` wiring. A synthetic adapter run has configured,
linked, and packaged the desktop runner. It does not validate a commercial ISO,
real analyzer output, or gameplay. Android packaging is staged, but this host
has no JDK 17 or Android SDK/NDK, so an APK has not been built here. The
builder records that state as `blocked` and never reports an APK when the
toolchain is absent.

## What the repository does today

| Observed in this tree | Consequence for the pipeline |
|---|---|
| The root [`CMakeLists.txt`](../CMakeLists.txt#L16-L32) defines separate `PS2X_BUILD_*` switches. When `ANDROID` is set, it disables host recompiler/analyzer/tests/Studio and leaves the runtime as the Android product. | Host analysis and NDK runtime builds remain separate toolchains. |
| [`ps2iso-inspect`](../tools/iso_inspect/README.md) reads supported ISO9660/Joliet images, reports `SYSTEM.CNF` and the boot ELF, inventories ELF contents and `PT_LOAD` ranges, and can extract the tree. | MIPS III flags are used as a secondary EE-candidate heuristic; other MIPS `ET_EXEC` and IRX files remain outside automatic EE recompilation. Raw/CUE-BIN images, multi-extent files, and some unusual layouts remain outside its verified scope. |
| [`ps2_analyzer`](../ps2xAnalyzer/src/analyzer_main.cpp#L5-L64) accepts an ELF and emits TOML. [`ps2_recomp`](../ps2xRecomp/src/runner/main.cpp#L7-L45) consumes TOML and emits C++ output; [`ps2native`](../tools/ps2native/README.md) invokes them for the boot ELF and MIPS III secondary candidates. | The boot keeps its dense table and adds sparse aliases; module paths and function symbols are namespaced. Failed secondary candidates stay in the manifest while other work continues. Partial named `SifLoadElfPart` loads currently fail before guest memory is written. |
| The analyzer's [`TomlGenerator`](../ps2xAnalyzer/src/toml_generator.cpp#L24-L61) writes an output directory beside its TOML file. The recompiler emits `register_functions.cpp`, generated function sources, and headers ([`ps2_recompiler.cpp`](../ps2xRecomp/src/lib/ps2_recompiler.cpp#L1375-L1385), [`ps2_recompiler.cpp`](../ps2xRecomp/src/lib/ps2_recompiler.cpp#L1771-L1881)). | `ps2native` runs those tools in a per-ISO workspace, preserves logs, and records the generated source list and hashes in its manifest. |
| [`ps2xRuntime/CMakeLists.txt`](../ps2xRuntime/CMakeLists.txt#L440-L463) builds `ps2EntryRunner` as a desktop executable or Android shared library. `PS2X_GENERATED_CODE_DIR` is the generated-source handoff. | Runtime CMake replaces the checked-in empty `register_functions.cpp` only for a per-game build; normal source files stay untouched. |
| The checked-in placeholder [`register_functions.cpp`](../ps2xRuntime/src/runner/register_functions.cpp) is replaced when `PS2X_GENERATED_CODE_DIR` is set. Runtime CMake adds generated sources/headers to `ps2EntryRunner` without modifying the checkout. | Per-title builds can run in isolated workspaces without overwriting the shared placeholder. |
| [`android/app/build.gradle`](../android/app/build.gradle) uses NDK 28.2, min SDK 28, ARM64 ABI by default, and `android/CMakeLists.txt` as an external wrapper. It passes `PS2X_GENERATED_CODE_DIR` to runtime CMake. | The host builder copies the Android project per title, stages `assets/game/`, and invokes Gradle only after checking prerequisites. |
| [`ps2xRuntime/src/main.cpp`](../ps2xRuntime/src/main.cpp) resolves Android boot ELF from `ANativeActivity::internalDataPath/game/boot.elf`. | Device boot selection uses app-private storage instead of a host path or public-storage location. |
| [`Ps2PackageActivity.java`](../android/app/src/main/java/com/ps2x/runner/Ps2PackageActivity.java) copies `assets/game/**` into app-private `files/game/**` before `NativeActivity` loads the native library. The manifest enables Java code and launches that activity. | ISO selection stays on the host; APK contains the complete staged disc tree for the runtime. Asset delivery may make APKs large; split/OBB delivery is not implemented. |
| [`android/README.md`](../android/README.md) describes `ps2native build --target android`; this checkout has wrapper properties but no Gradle wrapper executable/JAR. | Provide Gradle through `PATH`, `--gradle`, or the local wrapper-distribution cache. The builder does not install SDK/NDK components or accept SDK licenses. |
| [`ps2xStudio/CMakeLists.txt`](../ps2xStudio/CMakeLists.txt#L1-L3) builds a desktop SDL/ImGui tool linked to the analyzer, recompiler, and test library. | Studio may later front the builder, but the first end-to-end implementation should expose a deterministic CLI contract that can also be called by a GUI. |

The components above form an experimental host-to-package path, not a universal ISO converter. The C++ recompiler translates the boot ELF and selected secondary candidates into native host code that runs within a substantial PS2 runtime; successful compilation or APK generation does not establish full-game compatibility.

## Target architecture

```text
Desktop CLI or Studio front-end
  └─ host builder/orchestrator
      ├─ inspect ISO and resolve SYSTEM.CNF boot path
      ├─ extract boot ELF and stage the disc file tree
      ├─ invoke host ps2_analyzer → TOML/report
      ├─ invoke host ps2_recomp → generated C++/headers/table
      ├─ validate support gates and write package manifest
      └─ invoke Gradle + Android NDK in an isolated package workspace
          └─ game-specific APK
              ├─ lib/arm64-v8a/libps2EntryRunner.so
              ├─ generated title code linked into the native runner
              ├─ Android manifest and game files
              └─ runtime libraries and Android resources
```

The same generated title sources feed the desktop package path as well. The desktop artifact and Android APK are distinct products with their own host APIs and ABIs.

### Implemented pieces and remaining gates

1. **ISO intake and ELF inventory (implemented).** `ps2iso-inspect` identifies `SYSTEM.CNF` and the boot ELF, hashes the image, extracts supported ISO9660/Joliet trees, and inventories ELF files by content hash and disc paths. Secondary MIPS `ET_EXEC` files remain EE/IOP-ambiguous until runtime profile or stronger binary analysis classifies them. Raw 2352/CUE-BIN images and multi-extent files remain unsupported.
2. **Disc staging (implemented).** `ps2native` stages the complete extracted tree, verifies the extracted boot ELF hash, and adds a root `boot.elf` alias for runtime boot. Secondary EE ELFs and raw overlays are not yet analyzed or recompiled.
3. **Host analysis and recompilation (boot ELF only).** The builder invokes `ps2_analyzer` and `ps2_recomp`, retains their logs/config/generated sources, and records source hashes and counts. The tools do not yet provide a complete, machine-readable title-compatibility gate.
4. **Desktop and Android package handoff (implemented, experimental).** Isolated CMake/Gradle projects pass `PS2X_GENERATED_CODE_DIR` into runtime CMake, where the per-game registration table replaces the empty placeholder. Android stages assets under `assets/game/`, derives an application ID from the ISO hash, and records missing toolchain components as `blocked` instead of reporting a nonexistent APK.

### Generated-source handoff to CMake

The runner still globs checked-in sources, and now also accepts the generated directory through `PS2X_GENERATED_CODE_DIR`. Runtime CMake gathers generated `.cpp`/headers and includes them in `ps2EntryRunner`; when the option is set it filters out the checked-in empty registration table.

Keep the existing runtime source list and host-side tool targets separate. Do not configure the Android toolchain for `ps2_recomp` or `ps2_analyzer`; the Android Gradle build should only compile the runtime and generated title C++ with the NDK.

The generated [`register_functions.cpp`](../ps2xRecomp/src/lib/ps2_recompiler.cpp#L1771-L1781) is title-specific even though the repository has a checked-in placeholder with the same filename. Per-title runtime CMake links the staged version and filters out the placeholder, without copying over it. This avoids dirtying the checkout, cross-game contamination, and races between simultaneous builds.

### Android assets and boot selection

The boot ELF and disc data are staged for the runtime at launch:

1. Package the staged disc tree under Android assets and add a small hash marker for cache reuse.
2. On first launch, copy the tree into app-private files storage, preserving the relative paths the PS2 VFS/CDVD layer expects.
3. Resolve the boot ELF using `internalDataPath/game/boot.elf`; the build machine ISO path is not embedded in the APK.
4. `Ps2PackageActivity` stages the files before `NativeActivity` loads its native library. Raylib's Android app context exposes the activity through `GetAndroidApp()`, and `main()` resolves `internalDataPath/game/boot.elf` from there.

The Java activity verifies the packaged ELF and copies the tree transactionally to app-private `files/game/`. A package marker based on ISO/boot hashes avoids copying the same data on every launch. The native runner resolves that private path through raylib's `GetAndroidApp()` accessor; no machine-specific absolute path is compiled into the app.

Package the complete extracted disc tree for the MVP because disc reads, IOP modules, and overlays can be discovered at runtime. This increases APK size and first-launch storage use. If a title is too large for the chosen distribution channel, define an external-data or split-asset delivery mode; do not silently omit files. Keep BIOS/firmware outside the builder and package unless the runtime has a clean, redistributable implementation for the required service. A missing firmware requirement should be explicit in the manifest and support report.

## Builder interface and UI boundary

### CLI-first contract

The host builder currently exposes this command interface:

```text
python3 -m tools.ps2native build --iso <local.iso> --target android --out <new-directory>
```

It currently:

- validates and hashes the ISO, then identifies/extracts its boot ELF;
- records named stages in `manifest.json` and preserves tool logs/reports when a stage fails;
- supports process interruption through the host CLI;
- invokes processes with argument vectors rather than shell-concatenated user paths;
- builds under a temporary per-input workspace and publishes only a completed artifact to a previously absent destination;
- returns nonzero exit status for parse, analysis, recompile, Android build, or packaging failures;
- prints the final package path and marks gameplay as `unverified` / `experimental` until a real-title run exists.

Expected workspace structure:

```text
<workspace>/
  manifest.json
  source_manifest.json
  disc/                 # extracted game files
  analysis/             # ELF, generated TOML, and C++
  android-project/       # isolated Gradle/CMake project for Android
  desktop-project/       # isolated CMake project for desktop
  package/               # completed package before optional --out publication
  logs/
```

The `package/` directory contains the desktop runner package or Android APK. The final `--out` directory receives the completed package contents and `manifest.json`; the full workspace remains under the configured workspace root. Never put generated game files into tracked runtime source directories.

### GUI role

`ps2xStudio` can later call the same builder API/CLI to provide ISO selection, output selection, progress, cancellation, reports, and per-title profile selection. It should not duplicate analysis, codegen, or Gradle orchestration logic. The builder remains callable without Studio for automation and reproducibility.

The generated Android APK is a **player for one packaged title**. It should not contain an ISO analyzer/recompiler screen or ask the user for the desktop ISO again. The desktop builder owns ISO access and package generation. Android settings should be limited to runtime concerns such as display, input, and saves.

## Gradle, CMake, and artifact evolution

1. **Keep separate build trees:** `out/host` uses the desktop compiler; `out/android/<disc-hash>` uses Gradle's Android toolchain and NDK. Never reuse CMake cache directories across host and Android toolchains.
2. **Build host tools first:** the current root options allow a host tools-only configure by keeping recomp/analyzer enabled and setting runtime/tests/Studio off. The analyzer depends on the recompiler library, so the host build includes both `ps2_recomp` and `ps2_analyzer`.
3. **Generated-source input (implemented):** the runtime's `ps2EntryRunner` target remains the Android shared library. `PS2X_GENERATED_CODE_DIR` supplies staged title C++ and headers, and CMake excludes the placeholder registration source for per-title builds.
4. **Per-build Gradle properties (implemented for the current target):** the builder supplies a unique application ID derived from the ISO hash, plus the generated-source path and ARM64 ABI as separate build properties. The display label remains generic and signing uses the debug key for local builds.
5. **Target packaging:** MVP output is `arm64-v8a`. Android `x86_64` may be added for Android emulators but does not create a Windows/Linux desktop executable. Build desktop x86-64 separately using the host runtime target.
6. **Pin the toolchain:** Android pins AGP, SDK, NDK, and CMake; the repository has wrapper properties but no wrapper executable/JAR. The builder can use Gradle on `PATH`, `--gradle`, or a cached distribution, and checks for JDK 17 before invoking it.
7. **Signing:** current release config uses Gradle's debug signing config. That is suitable only for local prototypes. Any distributable release needs a signing key owned by the publisher/user and must never place that secret in the repository or generated build logs.

For desktop artifacts, build the generated source and runtime as a separate target using the desktop toolchain. Reuse the package manifest and disc tree, but do not try to put desktop x86-64 binaries in the Android APK's ABI directories.

## Open-world compatibility plan

Use versioned support records keyed by disc hash plus boot ELF hash/region. A serial number alone is not enough to distinguish revisions. A profile may describe verified boot-file selection, known overlays, compatibility patches, firmware requirements, and tested runtime features. Every patch needs an address, expected original bytes, reason, and exact identity match; never apply a title patch to a different hash by default.

Publish support as explicit tiers:

- **Automatic:** the generic pipeline extracted, analyzed, recompiled, packaged, and launched this tested revision without a game-specific patch.
- **Profiled:** a checked-in or user-supplied version-specific profile is required; it is still one-command after selection.
- **Experimental:** APK generation succeeds, but one or more runtime paths have not passed full-game acceptance.
- **Unsupported:** extraction, static analysis, generated-source coverage, firmware/device requirements, or runtime behavior blocks packaging or execution.

The project can expand its supported-title set over time, but it must not market the first working APK as proof that arbitrary ISOs are plug-and-play. This tree currently recompiles an ELF into C++ and runs it inside a substantial PS2 runtime. Dynamic overlays, unrecognized code paths, runtime-loaded IOP modules, unusual disc access, and device timing can require additional runtime support or per-title work.

## Acceptance checkpoints

These remain title-level release gates. The desktop CMake plumbing smoke test does not satisfy the real-ISO or gameplay gates.

### A. Host pipeline

- A clean desktop build produces `ps2_analyzer` and `ps2_recomp` without configuring Android.
- Given a supported test ISO, ISO intake resolves the boot ELF and emits an extraction manifest with ISO/ELF hashes and relative file paths.
- Analyzer and recompiler run in the per-ISO workspace and emit all expected generated code, headers, registration table, and reports.
- A malformed ISO, missing boot ELF, analyzer failure, or missing Android SDK produces a stage-specific error and no final APK. Unsupported-instruction analysis is retained in the recompiler log, but a complete compatibility gate is not implemented yet.
- Two builds for different ISO hashes can run concurrently without writing into or overwriting the source checkout or each other's outputs.

### B. Android package build

- Gradle builds the Android runtime and generated title code with the NDK for `arm64-v8a`; host `ps2_analyzer`/`ps2_recomp` are absent from the APK.
- APK inspection confirms a game-specific runner library, package manifest, and complete staged game-data tree; no path from the build machine is embedded as the runtime boot path.
- The generated package installs side-by-side with another generated title package using a distinct deterministic application ID.
- A physical ARM64 device launches the APK offline, stages assets into app-private storage, resolves the boot ELF, and reaches the first defined gameplay checkpoint.

### C. Title support claim

- The tested ISO revision has a published support record and build report with profile and firmware requirements.
- The title passes its defined end-to-end acceptance route: launch, menus, controls, level/session transition, audio, save, process restart, and save reload. For a “complete” claim, all known game modes and content paths must be accounted for, not only boot or one playable scene.
- Every incomplete subsystem or known crash remains visible as experimental/unsupported; APK generation alone is not a compatibility pass.

## Unsupported assumptions to resolve before implementation

- **ISO coverage:** current intake covers supported ISO9660/Joliet directory records and boot ELF paths; raw/CUE-BIN, multi-extent files, and unusual disc layouts need explicit fixtures/support.
- **Disc/VFS integration:** whether the existing runtime can map an extracted disc tree to every CDVD/IOP access pattern required by the selected title. Full ISO-tree extraction does not itself implement CDVD timing or device behavior.
- **Generated-source integration:** `PS2X_GENERATED_CODE_DIR` is implemented for desktop and Android; it still needs validation against real commercial-title output.
- **Android asset bootstrap:** `Ps2PackageActivity` copies packaged assets into app-private storage; device boot and complete disc access still need a real ARM64 acceptance run.
- **Runtime completeness:** the generated C++ and existing PS2 runtime must support the selected game's EE/VU/GS/audio/IOP paths. A successful C++ compile does not prove gameplay completeness.
- **Distribution mode:** whether packages are local sideloads or store releases; asset size handling and signing differ.
- **Build prerequisites:** the desktop machine needs a compatible JDK, Gradle, Android SDK/NDK, CMake, and access to dependencies fetched by CMake/Gradle. Offline and reproducible dependency caching are future requirements unless explicitly added to MVP.

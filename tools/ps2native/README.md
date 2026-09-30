# `ps2native` host builder

`ps2native` is a host-side orchestrator for the current PS2Recomp analyzer and
recompiler. The analyzer/recompiler and desktop CMake configure on the build
machine; the eventual package contains generated game C++, the current runtime,
and extracted disc files. It does not promise universal PS2 game compatibility.

## Commands

```sh
python3 -m tools.ps2native inspect --iso /path/to/game.iso
python3 -m tools.ps2native inspect --iso /path/to/game.iso --json-output
python3 -m tools.ps2native build --iso /path/to/game.iso --target desktop
python3 -m tools.ps2native build --iso /path/to/game.iso --out /path/to/new-package
python3 -m tools.ps2native verify --package /path/to/new-package
```

The equivalent executable is `tools/ps2native/ps2native`. `inspect` requires
the separate `ps2iso-inspect` executable and consumes its schema-version-1 JSON
contract. `build` calls the inspector to identify the boot ELF and extract the
disc, verifies the ISO and boot-ELF hashes, then runs `ps2_analyzer` followed by
`ps2_recomp`.

The inspector's ELF inventory is preserved in `manifest.json` and summarized
as `executable_inventory`. It deduplicates identical ELF contents while
retaining their disc paths and identifies secondary MIPS `ET_EXEC` and IRX
candidates. Secondary ELF32 little-endian MIPS `ET_EXEC` files whose MIPS
`e_flags` select MIPS III are treated as EE candidates, compiled into unique
module directories, and linked with sparse runtime registration tables. This
is a conservative heuristic rather than definitive EE/IOP identification;
other MIPS executables remain inventory-only. At runtime the SIF loader
activates a matching table by normalized module path, with the newest loaded
executable owning overlapping code ranges. The boot ELF keeps its legacy
dense table while registering sparse aliases for the ISO path and staged
`boot.elf`. Candidate-specific analysis/recompile failures remain in the
manifest and do not prevent other candidates from being processed. CD path
resolution is ASCII case-insensitive and strips ISO version suffixes for each
path component. Named partial `SifLoadElfPart` loads currently fail before
guest memory is changed.

Tool discovery checks the matching `PS2NATIVE_INSPECTOR`,
`PS2NATIVE_ANALYZER`, `PS2NATIVE_RECOMPILER`, and `PS2NATIVE_CMAKE` environment
variables, command search path, then `<checkout>/build` and
`<checkout>/out/build`. Explicit `--inspector`, `--analyzer`, `--recompiler`,
and `--cmake` options override discovery. `--work-root` selects the isolated
workspace parent; each invocation receives a unique `<title>-<iso-hash>/run-*`
directory. Generated work is kept out of runtime/recompiler source trees.
`--out` chooses a package destination outside that workspace. It must not
already exist; the completed package is copied to a sibling staging directory
and published with an atomic no-replace rename. Intermediate builds and logs
stay in the isolated workspace.

The complete workspace includes extraction, per-tool logs, analyzer TOML,
recompiler output, `source_manifest.json`, and `manifest.json`. The manifest
records hashes, resolved tool paths, pipeline steps, source lists, and errors.
Completed package builds also store a SHA-256 and size for every packaged file
except `manifest.json` itself. `verify` detects changed, missing, or added
files against that inventory, while allowing the declared runtime log paths
(`ps2_log.txt` and `game/ps2_log.txt`) to be created or updated after launch.
If either path was already present in the ISO, its original packaged bytes
remain in the immutable inventory. The manifest includes a compact source-tree
fingerprint and resolved analyzer/recompiler/inspector binary hashes. These
hashes detect drift against the stored inventory; they are not a signature and
do not attest gameplay compatibility.
`native_translation_assessment` summarizes reported function counts,
skipped/stubbed functions, decode failures, unhandled instructions, and
recompiler errors for the boot ELF and each compiled secondary candidate.
`known_gaps` means the counters identify a static gap, `unknown` means the
report is incomplete, and `runtime_stubs_present` means generated runtime
stubs exist. `no_reported_instruction_gaps` means only that the reported
counters are clean; every status retains `gameplay_compatibility: unverified`.
This summary does not measure undiscovered code, dynamic overlays, IOP/VU or
device behavior, or whether a game can be completed. When emitted by the
recompiler, `function_coverage.csv` lists each discovered function's estimated
address span, processing status, and decoded instruction count. Those function
records are not a byte-complete map of executable ELF segments. The pipeline
also writes `address_coverage.csv`, partitioning each executable `PT_LOAD`
file-backed span into reported function ranges, overlaps, and unattributed
bytes; executable zero-fill is listed separately. Unattributed bytes may be
data or padding, and range attribution does not prove instruction semantics.
The boot ELF is copied to the extracted disc root as `boot.elf` so the
runtime's disc-root inference points at the extracted disc tree.

## Desktop target

Desktop packaging is a native build for the host ABI. A standalone template
under `templates/desktop/CMakeLists.txt` adds the checkout with
`add_subdirectory` and passes `PS2X_GENERATED_CODE_DIR` before the runtime is
configured. Runtime CMake removes the checked-in `register_functions.cpp`
placeholder from `ps2EntryRunner` and adds the generated sources and includes.
The CMake project and build tree live in the isolated title workspace. The
package contains `bin/ps2EntryRunner`, `game/` (the extracted disc, with a root
`boot.elf` alias), and `run-ps2native.sh` or `run-ps2native.bat`.

The wrapper currently disables the debug UI to keep the first native build path
focused. FFmpeg uses the runtime CMake default, retaining FMV support when host
FFmpeg development packages are available. It uses any already populated source dependencies in the
checkout's `build/_deps`; otherwise CMake fetches the dependencies declared by
the runtime. Host builds do not yet cross-compile to a different desktop ABI.

## Android target

`--target android` stages a per-title Gradle project, puts the extracted disc
tree under `app/src/main/assets/game/`, passes `PS2X_GENERATED_CODE_DIR` to the
Android CMake wrapper, and asks Gradle for an `arm64-v8a` release APK.
`Ps2PackageActivity` copies those assets into app-private storage before
`NativeActivity` starts; the runtime resolves
`ANativeActivity::internalDataPath/game/boot.elf`. If Gradle, JDK 17, Android
SDK platform 34, NDK 28.2, Android CMake 3.22.1, or the Activity bridge is
missing, the manifest records `status: blocked`, the exact prerequisites, and
the staged project path; no APK artifact is claimed. Embedded disc files can
make an APK larger than common distribution limits; split/OBB packaging is not
implemented.

## Current functional boundary

The pipeline covers the boot ELF and MIPS III secondary ELF candidates found
in the ISO. It does not discover/recompile arbitrary overlays or prove that
all EE/IOP, VU, GS, DMA, SPU2, firmware, save-data, and game-specific system
behavior has been implemented. A successful desktop link proves the package
compiled; gameplay compatibility still needs per-title validation.
Every generated manifest and CLI summary marks the support tier `experimental`
and gameplay compatibility `unverified` until a real-title acceptance run
exists.

# NEXO laboratory: canonical device state and conservative native V0

This implements a **partial preparation** for the first increment in the root
README, section 31.1. It preserves the current VU runtime as a regression
baseline and implements a laboratory AOT V0 backend for finite microcode banks.
An original VIF-call replay now includes VU, GIF and CPU GS state. An independent
reference with a qualified full state relation, broader external-input closure,
second-architecture validation and a qualified native game package remain
unfinished. The identified independent VU engine and its timing differences are
described in `PCSX2_VU_REFERENCE.md`.

## Initial IOP AOT path

The IOP now also has an instruction-specialized V0 bridge integrated with IRX
startup and RPC callbacks. `generate_iop_bank.py` consumes an already relocated
RAM bank, creates an entry for every aligned word, and emits C++ operations with
constant instruction parameters. The native dispatcher verifies live code
identity, supports interior entries and RAM aliases, and refuses missing,
changed, or misaligned code without interpreting it.

The synthetic acceptance corpus has 20 strict native cases, 23 diagnostic cases
(including 292 absolute and 159 parameterized one-step comparisons to the
identified CPU model), and 12 converter cases. It includes native RPC, self modification, all RAM writers,
pending loads and branches, unknown imports, incomplete relocations, and
unfinished startup. `PS2X_IOP_ENABLE_INTERPRETER=OFF` removes the generic CPU
instruction-execution symbol from the native test executable.

This is preparation for M4. The game runtime still uses its diagnostic IOP
configuration; banks for its complete commercial corpus,
independent R3000A fidelity, canonical snapshots and qualified service/timing
contracts remain open. A synthetic startup/RPC result does not qualify Monster
House or a complete game. Commands and the internal bank contract are in
[`nexo-iop-aot-v0.md`](../schemas/nexo-iop-aot-v0.md).

### Original IRX startup bridge

`PS2X_IOP_BUILD_LAB=ON` adds `nexo_iop_inspect`: it runs the existing loader
offline, rejects incomplete relocation tables and misaligned/out-of-range entry
points, and writes a complete relocated RAM bank plus `module.json`.
`generate_iop_bank.py --loaded-module` reads that metadata directly; no manual
function-entry or callback address list is supplied.

```sh
cmake -S ps2xIOP -B build/iop-aot-strict -DPS2X_IOP_BUILD_LAB=ON -DPS2X_IOP_BUILD_TESTS=ON -DPS2X_IOP_ENABLE_INTERPRETER=OFF '-DCMAKE_CXX_FLAGS_RELEASE=-O1 -DNDEBUG -fno-lto' -DCMAKE_BUILD_TYPE=Release
cmake --build build/iop-aot-strict --target nexo_iop_inspect --parallel 4
build/iop-aot-strict/nexo_iop_inspect /path/to/module.irx /path/to/new-case
python lab/generate_iop_bank.py --loaded-module /path/to/new-case --output /path/to/generated --symbol compiledIopProgram
cmake -S ps2xIOP -B build/iop-aot-strict -DNEXO_IOP_BANK_CPP=/path/to/generated/iop_native_bank.cpp
cmake --build build/iop-aot-strict --target nexo_iop_native_probe --parallel 4
build/iop-aot-strict/nexo_iop_native_probe /path/to/module.irx /path/to/new-result
```

The native probe uses a bounded laboratory host and reports unsupported external
operations as failures. It captures final physical IOP RAM and EE RAM, startup
return, counters and logs. `nexo_iop_baseline_probe` is a separate target available
only with the diagnostic interpreter enabled. Neither probe is a game runner.

The original Monster House `HKSIF.IRX` startup was exercised with 85 native guest
operations and 11 service dispatches, zero interpreted operations and no generic
CPU execution symbol in the native binary. Its 2 MiB IOP RAM, 32 MiB EE RAM,
startup return, counters and logs matched the identified diagnostic model.
All 11 external IRX files passed **offline loader acceptance**, each isolated at
the default base. This does not claim execution of the other ten modules.

Relocation-family binding in the actual game, embedded `IOPRP271.IMG` modules,
canonical hidden state and independent fidelity remain open. The commercial
images, generated C++ and captures stay in ignored local build directories.

### Relocatable IRX families

The inspector also emits `source-image.bin` and per-word relocation masks.
`generate_iop_bank.py --family-module` embeds the full image identity and generates
fixed operation shapes with bound operands. It admits immediate-16 operand
forms and J/JAL targets. Full-word relocated data and unqualified operand forms
receive no executable callback; jumping to them fails explicitly.

```sh
build/iop-aot-strict/nexo_iop_inspect /path/to/module.irx /path/to/new-family-case
python lab/generate_iop_bank.py --family-module /path/to/new-family-case --output /path/to/family-generated --symbol compiledIopProgram
cmake -S ps2xIOP -B build/iop-aot-strict -DNEXO_IOP_BANK_CPP=/path/to/family-generated/iop_native_bank.cpp
cmake --build build/iop-aot-strict --target nexo_iop_native_probe --parallel 4
build/iop-aot-strict/nexo_iop_native_probe /path/to/module.irx /path/to/new-family-result 2
```

The subsystem binds the compiled family after relocation and before startup.
The dispatcher owns image/entry metadata, checks source identity, dimensions,
relocation masks and fixed instruction bits, then guards the complete bound
word before each native guest operation. Reset clears bindings while retaining
compiled families. Directory replacement invalidates a previous overlapping
binding completely; module unload also retires its directory.

The original HKSIF family passed two consecutive startups, automatically placed
at `0x10000` and `0x10500`: 170 native operations, 22 service dispatches, zero
interpreted operations and no native faults. Full RAM and all reported fields
apart from native/diagnostic counters agreed with the identified model. A
modified source image failed before executing a native operation.

This is one module's startup coverage. The loader remains an identified model;
independent fidelity, hidden state, service/version/timing contracts, code
publication epochs and full kernel lifecycle on replacement are open. Buffer
load identity variants, the complete module corpus and final game integration
are also open. Matching a family at two bases does not qualify M4 or a game.

## Build and run

```sh
cmake -S . -B build -DPS2X_BUILD_NEXO_LAB=ON -DPS2X_FAST_ITERATION=ON
cmake --build build --target nexo_vu_snapshot_tests nexo_device_snapshot_tests nexo_vu_native_tests nexo_vu_replay nexo_vu_inspect --parallel 4
ctest --test-dir build -R '^nexo_(vu_|device_)' --output-on-failure
build/lab/nexo_vu_replay /path/to/canonical-capture 65536 100
```

The replay's explicit cycle budget must match the recorded call budget. Restore
uses continuation semantics even for MSCAL: the capture already contains the
fresh-call scheduler normalization. Re-executing a fresh call would normalize
the captured pipeline a second time. MSCNT captures must retain their pending
work. Fresh-call normalization in this runtime preserves the absolute clock.

The lab targets compile with `-O1 -fno-lto`, and IPO is disabled. The existing
runtime fast-iteration profile uses `-O1 -fno-lto` with its private VU sources
at `-O2`. There is no rebuild of generated EE game functions for a replay case.
Laboratory capture hooks and the codec enter the runtime archive only when the
lab option is enabled. The default option is off.

Root development settings now apply to the recompilation, analysis and test
targets too. Their prior release helper ignored fast iteration and enabled LTO
for 48 source units. The Linux compile-command audit now finds zero LTO units
among those project targets. Release IPO requires both fast iteration off and
`PS2X_ENABLE_RELEASE_IPO=ON`; MSVC's explicit /GL and /LTCG follow that gate too.
Only the Linux configuration has been exercised for this change.

## Capture boundary

Set `PS2X_CAPTURE_SCENE` to a private local directory and create `.vu-request`
inside it. A lab-enabled runner consumes the marker at the next VU1 MSCAL or
MSCNT call. It records canonical input/output state, code/data byte memories,
and timed PATH1 submissions. Capture and code/data writers must be synchronous.
Commercial game data belongs in ignored local build workspaces.

The state codec covers pending vector/integer/ACC writes, delayed flags, Q/P
results, readiness and resource clocks, branch history and partially transferred
XGKICK packets. Host pointers and derived decode caches are rebuilt on restore.
See the versioned documents in `schemas/` for field order and bounds. States
with a second XGKICK already issued and waiting for PATH1 use the version-2
extension; empty request slots retain the exact version-1 encoding. Tests
exercise the second pair's Upper operation, its latched source, live future
payload reads, and continuation without generic VU execution in the AOT path.

Legacy `.bin` register images remain useful only with the original host ABI.
They cannot substitute for `input-state.nexo` in this tool.

The replay observes submissions using an otherwise empty GIF receiver. It does
not restore GIF arbitration, VIF execution, GS state or VRAM; it does not render
the game. Comparisons are explicitly `tested_only`, against the same runtime.
An enclosing case manifest must bind all blobs and producer/reference binaries
with SHA-256 before it can be used as evidence.

## Compile a finite native VU bank

After the inspector has been built, translate a captured bank before execution:

```sh
python lab/generate_vu_bank.py --inspect build/lab/nexo_vu_inspect \
  --code /path/to/canonical-capture/code.bin --unit vu1 \
  --output build/lab/compiled-vu-bank.cpp --manifest build/lab/compiled-vu-bank.json
cmake -S . -B build -DPS2X_BUILD_NEXO_LAB=ON -DPS2X_FAST_ITERATION=ON \
  -DNEXO_VU_BANK_CPP="$PWD/build/lab/compiled-vu-bank.cpp"
cmake --build build --target nexo_vu_native_replay nexo_vu_runtime_replay --parallel 4
build/lab/nexo_vu_native_replay /path/to/canonical-capture 65536 100
build/lab/nexo_vu_runtime_replay /path/to/canonical-capture 65536 100
```

The converter derives static dependency descriptors and emits upper/lower
operations specialized by compile-time instruction constants, including the
opcode-dependent numeric and flag helpers. The native scheduler takes the
compiled entries and data memory; it has no guest instruction buffer to fetch
or decode. Unsupported entries, invalid PCs and code identity changes produce
`UNSEEN_CODE`, without an interpreter or runtime guest compiler fallback.

Eight link wrappers reject generic interpreter execution entry points in this
replay executable. A deliberate forbidden call verifies that the trap is active.
The inspector is a separate conversion target. The prototype still shares
pipeline and device helpers with the existing runtime, and the linked runtime
contains legacy interpreter code. These checks do not establish final-package
interpreter absence or native execution of the whole game. Linux with the
GNU/LLVM C++ ABI is the currently supported laboratory configuration.

## Native runtime integration

`bindNativeVu1` installs strict native callbacks into an initialized PS2Runtime
memory session, using the existing VIF MSCAL/MSCALF and MSCNT interfaces. It
copies bank descriptors and identities into owned storage. Compiled function
pointers must remain loaded. Selection compares complete microcode bytes on
every call, including after raw mutable code writes that bypass generation
counters. Ambiguous identities and malformed collections are rejected before
replacing an installation. Unknown code does not fall back to interpretation.

Native fresh execution resets pending work according to the identified runtime
model while retaining the clock. Native MSCNT retains pending pipelines. Both
propagate D/T enables and CPU-visible stop flags through the normal runtime.
The current callback horizon is 65,536 cycles. See
`schemas/nexo-vu-runtime-binding-v0.md` for ownership, reset and scope limits.

The runtime replay executable restores a normalized VU capture and drives it
through a **synthesized MSCNT command**. It reconstructs TOP/ITOP and D/T inputs
from canonical VU fields and observes PATH1 with the same empty receiver as the
standalone replay. This exercises real VIF parsing and native runtime callbacks;
it does not reconstruct the original VIF command stream, full VIF state, GIF
arbitration or GS/VRAM. Its JSON identifies the synthesized input explicitly.
Headless fixtures contain no executable EE entries and open no user window.

Both native replay executables link the same compiled bank archive, avoiding
duplicate bank compilation. Semantic generation updates both output timestamps
so unchanged outputs do not make Make rerun generation for each dependent target.

## Transport and CPU graphics checkpoints

`Vif1SnapshotCodec`, `GifSnapshotCodec` and `GsSnapshotCodec` preserve the current
runtime's incremental VIF parser/transport, queued GIF submissions, and CPU GS
frontend/backend plus VRAM. Restore is bounded and transactional. GS includes
partially assembled primitives, loaded palettes and remembered CBPs, stale
texture page bytes, partly consumed readbacks, private register values, and
latched presentation data. Host pointers and callbacks remain owned by the
receiving instance. Execution and all writers must be paused before capture.

The headless device suite splits example VIF inputs at every byte and compares
restored continuations. It tests GS/GIF continuation and corrupt states as well.
See `schemas/nexo-device-state-v1.md` for exact scope, ordering and bounds.
The older VU replay CLIs retain their documented empty GIF receiver and
synthesized input. The separate original-call replay below restores these
device codecs with the runtime's real GIF-to-GS routing. Neither comparison
is an independent graphics reference.

## Original VIF call and all observed native banks

Create `.vif-request` in the lab runner's `PS2X_CAPTURE_SCENE` directory. The
request remains pending until a completed call containing a VU callback. It
captures the literal VIF argument before parser normalization, full VIF/VU/GIF/
CPU-GS states, 16 KiB code/data memories, each actually executed code identity,
and both GIF submissions and GS deliveries. Host presentation is serialized
across acquisition; other writers must remain quiescent. See
`schemas/nexo-observed-vif-case-v1.md` for the boundary and exclusions.

```sh
cmake --build build --target nexo_vif_replay nexo_vu_inspect --parallel 4
build/lab/nexo_vif_replay /path/to/completed-vif-case 10
python lab/generate_vif_banks.py --case /path/to/completed-vif-case \
  --inspect build/lab/nexo_vu_inspect --output build/lab/vif-banks \
  --cache build/lab/vif-bank-cache
cmake -S . -B build -DPS2X_BUILD_NEXO_LAB=ON -DPS2X_FAST_ITERATION=ON \
  -DPS2X_ENABLE_RELEASE_IPO=OFF \
  -DNEXO_VIF_BANK_SOURCES="$(cat build/lab/vif-banks/cmake-sources.txt)"
cmake --build build --target nexo_vif_native_replay --parallel 4
build/lab/nexo_vif_native_replay /path/to/completed-vif-case 10
ctest --test-dir build -R '^nexo_' --output-on-failure
```

Conversion processes the entire finite bank collection, rather than waiting for
an unknown callback and manually adding one address. Metadata reuse binds the
inspector/emitter/converter identities and full bank bytes. Corrupt cache is
reinspected; unchanged generated C++ retains its timestamp. Changing unseen
game code still requires conversion-time closure, rather than silent runtime
interpretation. The collection manifest explicitly denies closure beyond the
observed case.

The native CLI installs strict owned native callbacks and eight fatal VU
interpreter link traps. Both original-call CLIs open no user window, compare
complete canonical component states and events, and produce JSON on stdout
with diagnostics on stderr. The acquisition runner remains a laboratory
producer with legacy IOP/VU and overlay paths; it is not a final game package.

The headless launch tool supports `--runner` and explicit `--capture-scene`.
It chooses a free display after the base display option and checks that the
Xvfb lock PID belongs to its own process group before executing the game.
This avoids an installed wrapper's behavior of launching a client even when
its new server failed. An existing session's virtual server is rejected.

### Original Monster House sample, 2026-09-30

The captured original call contains nine VU callbacks and two distinct full
code banks. All ten baseline repetitions and ten conservative AOT repetitions
match the recorded VIF, VU, GIF, CPU GS/VRAM, code/data, relevant CPU status and
ordered events exactly. No interpreter wrapper fired in the native path.
These results are same-model `tested_only` evidence for one observed call.

Compiling both banks and the native CLI took 23.489 seconds under concurrent
host load, with six source compilations and zero generated EE compilations.
An earlier diagnostic game relink took 11.621 seconds using existing EE objects.
These are scoped development build measurements, not whole-ISO conversion,
whole-game gameplay or a promise of 60 FPS.

Repeated conversion of the unchanged collection used zero inspector runs,
preserved every C++ source timestamp and took 0.348 seconds. The following
unchanged native replay build took 0.307 seconds and compiled zero source
files. These measurements cover local cache reuse, under uncontrolled host
load, and exclude discovery of unobserved code.

At commit `ea497eb`, the eight laboratory suites passed 82 cases: 14 VU checkpoints, 14 device
checkpoints, 21 native VU cases, 11 original VIF cases, three Python generator
suites of six cases each, and four headless isolation cases. The rebuilt general
runtime suite passes 484/484 with DISPLAY, WAYLAND_DISPLAY and capture unset.

## Profiling the original call and CPU GS compilation

```sh
build/lab/nexo_vif_native_replay /path/to/completed-vif-case 10 --profile
cmake -S . -B build -DPS2X_FAST_ITERATION=ON \
  -DPS2X_FAST_ITERATION_OPTIMIZE_GS_CPU=ON -DPS2X_ENABLE_RELEASE_IPO=OFF
cmake --build build --target nexo_vif_native_replay --parallel 4
```

Profiling brackets actual VU callbacks, GIF submission and GS delivery using
thread-local host timers. Nested exclusive totals avoid counting GS work again
as VU work. Profiling remains outside canonical states/events, is disabled by
default, and includes observation overhead according to the documented scope.
There is no guest interpreter fallback in the native replay.

In ten instrumented repetitions of the original Monster House case, the GS
receiver used 247.990 ms out of 250.813 ms of VIF-call wall time (about 98.9%).
VU callback exclusive time was 1.296 ms. These are one-case measurements under
uncontrolled host load, not whole-game throughput.

A paired `ABBA ABBA` experiment compared CPU GS at `-O1` with `-O2` and FP
contraction disabled. Each mode replayed the original case 20 times. Median
call execution was 251.435 ms versus 212.890 ms (measured ratio 1.181), with
identical full states and events. No new graphics algorithm, hardware fidelity
proof, GPU backend, game FPS or whole-game qualification follows from this.

The kernel lives in `ps2_gs_cpu_backend`, a separate object target included in
the runtime archive. It inherits the runtime's includes, definitions and common
compile options. Its private optimization flags have their own Makefile: the
previous per-source-option placement changed `ps2_runtime/flags.make` and
recompiled 52 runtime units. This target split isolates later option changes.
The first structural migration still invalidates the old monolithic flags.
Linux GNU/Make is the configuration exercised for this change.

After that migration, switching the GS option off and on compiled exactly one
source each time, taking 2.192 and 2.742 seconds respectively. The global
runtime flags file remained byte-identical; generated EE compilation count
remained zero. Each switch replayed the original case five times with exact
state/event equality. The optimization remains an opt-in development variant.

The final CTest run passes all 55 registered groups, including the expanded 85
laboratory cases and 484 general runtime cases. Original-call reference,
native and instrumented native paths each match ten repetitions of the
recording with the split object target. The three new timing cases initially
failed, then passed after nested timing and the actual callback brackets were
implemented. They cover memory filtering, nested exclusive subtraction,
exception unwinding, disabled timing and preservation of canonical state/trace.

The external gprofng sampling attempt was rejected because the collector
reported a changed interval timer and unreliable data. Its 49 samples cannot
support whole-run CPU percentages. Direct scope measurements replaced that
attempt; matching game state alone does not approve a profiler.

For independent-reference investigation, upstream PCSX2 revision
`94d86c891b1621c0b252e4fc2e155bf90274dcc0` was acquired with source hashes and
its GPL license in ignored local reference storage. It has not executed this
canonical case. An adapter must establish its state correspondence and device
boundary before it can provide independent evidence. No Android device was
attached at the latest ADB inventory.

## Recorded checks, 2026-09-30

- Device checkpoint cycle: all six initial cases first failed against explicit
  stubs; VIF/GIF implementation passed five, then GS completed all six. The
  expanded suite passes 14/14 cases, including checksummed semantic corruption,
  transactional rejection, partial transfers/vertices, cached palettes and
  texture visibility, atomics and presentation state. Together with the other
  four lab suites this is 61 passing cases. General runtime regressions remain
  484/484 after rebuilding against the new sources, with no user display.
- A development rebuild after touching only `lab/src/gs_snapshot.cpp` took
  7.537 seconds under concurrent CPU load, compiled exactly one source unit
  and rebuilt no generated EE functions. This is a codec rebuild measurement,
  not ISO conversion time or whole-game compilation.
- Initial codec test cycle: 6 failures / 1 pass, then 7/7 passes; expanded to 10/10.
- Capture integration: the new MSCAL/MSCNT tests first failed, then 12/12 passed.
- Timed PATH1 capture: the added XGKICK test first failed, then 13/13 passed.
- Standalone replay: the three integration cases first failed against its stub,
  then all 13 tests passed with the implementation.
- Six malformed CLI invocations were rejected.
- A checksum-valid scalar deadline outside the scheduler model was first
  accepted by the decoder (the new test failed), then rejected transactionally
  after adding deadline bounds. The expanded suite passes 14/14.
- A real Monster House MSCAL capture ran for 277 VU cycles and issued 254 pairs.
  All 100 standalone repetitions matched the complete canonical output state,
  VU data memory and timed PATH1 submissions exactly.
- That sample averaged 104.589 microseconds for VU execution, including cold
  derived-code decoding and PATH1 recording. File input, machine construction,
  state restoration and final comparison were outside the measured interval.
  This is one CPU sample, not game FPS, an AOT speedup, or sustained performance.
- The diagnostic game runner was relinked in 13.408 seconds using existing EE
  objects: zero generated-source compilations. Its legacy overlay driver still
  compiles guest code at runtime; it is not eligible for final AOT acceptance.
- Initial conservative native V0: 11/11 C++ cases passed, including FMAC, suspended Q/P
  pipelines, VI branch history, stores, future XGKICK reads, interpreter traps,
  unknown entries and an overflowing PC. The latter first caused a segmentation
  fault, then was rejected before table lookup after fixing the bounds check.
- The native suite now passes 21/21 cases. Added checks cover fresh normalization,
  real VIF callbacks, ownership after source views are destroyed, changed banks
  with pending pipelines, CPU-visible D/T status, unknown identities, ambiguous
  installations, canonical runtime replay and its exact budget and D/T inputs.
  The new behavior first failed against explicit implementation stubs. The D/T
  replay check subsequently exposed overwritten enables and passed after their
  canonical inputs were reconstructed. Captured TOP/ITOP outside the VIF callback
  domain are explicitly rejected, rather than silently masked.
- A continuation test exposed a stale reference decode cache after a raw write.
  The fixture now publishes that write to the reference's generation counter;
  native selection still detects the new bytes without a generation increment.
- Semantic generation and bank generation each pass 6/6 Python cases. Together
  with the 14 codec/capture cases, the four laboratory suites cover 47 cases;
  the latest CTest run completed in 1.51 seconds.
- The existing general C++ suite was rebuilt in the fast profile and passes
  484/484 cases with DISPLAY, WAYLAND_DISPLAY and scene capture unset.
- An unchanged rebuild of all five laboratory executable/test targets completed
  in 1.534 seconds, compiled zero source units and reran the semantic generator
  zero times. This measures an unchanged incremental build, not ISO conversion.
- The real Monster House bank contains 2,048 compiled entries. Frontend analysis
  and C++ emission took 0.169 seconds; this excludes C++ compilation and linking.
- All 100 native-only repetitions of the real capture matched complete state,
  data and timed PATH1 submissions exactly, without triggering the interpreter
  traps. A changed instruction byte was rejected by the identity guard.
- Undefined-symbol audits of the generated bank and native scheduler objects
  found no generic interpreter execution/decoder imports. This is an audit of
  those two objects, not a complete executable reachability proof.
- All 100 repetitions through the real runtime's native VIF callbacks also
  matched the Monster House recording exactly. Changed-code and wrong-budget
  CLI cases were rejected. The existing reference and direct native paths each
  retained 100/100 exact matches after the shared replay refactor.
- The final integrated sample averaged 112.056 microseconds under uncontrolled CPU
  load, including synthesized VIF processing and native bank selection. It is
  not a controlled speed comparison, whole-game FPS or evidence of a faster port.

Local evidence is under `build/nexo-*.log`. The canonical game capture and its
SHA-256 manifest are in the directory named by
`build/lab/latest-monsterhouse-vu-capture.txt`. Generated captures/binaries are
ignored and must not be committed with the laboratory sources.
The native validation receipt and frozen artifacts are identified by
`build/lab/latest-monsterhouse-native-v0-evidence.txt`.
Integrated runtime replay evidence is identified by
`build/lab/latest-monsterhouse-native-vif-evidence.txt`.

## Remaining first-increment work

1. Capture/replay VIF input and hidden state at the same boundary.
2. Integrate the transport/CPU GS checkpoints into the synchronized original
   VIF case and compare GIF/GS effects with identified references. Synthetic
   checkpoint tests do not complete this integration gate.
3. Add an identified independent reference and extend V0 validation to more
   programs, both VUs, additional operations and code-upload variants.
4. Map divergences to semantic fields and causal events; current CLI reports
   only the first differing byte per output blob.
5. Validate the canonical format and execution on a second architecture.
6. Measure baseline/AOT/optimized variants on a held-out corpus with proof gates.
7. Extend runtime integration to all required banks, prove upload/code closure
   and exclude legacy guest execution from a separate final package build.

None of these outstanding gates is satisfied by matching one case against the
same runtime. Full game progression, Android and universal conversion remain
separate unfinished milestones in the root plan.

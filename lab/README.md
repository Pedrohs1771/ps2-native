# NEXO laboratory: canonical device state and conservative native V0

This implements a **partial preparation** for the first increment in the root
README, section 31.1. It preserves the current VU runtime as a regression
baseline and implements a laboratory AOT V0 backend for finite microcode banks.
An independent hardware reference, the full VIF/GIF/GS causal boundary and a
qualified native game package remain unfinished.

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
See the versioned documents in `schemas/` for field order and bounds.

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
These codecs are preparatory components: existing VU replay CLIs still use
their documented empty GIF receiver and synthesized input. No original full
Monster House VIF/GS replay or independent graphics reference is implied.

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

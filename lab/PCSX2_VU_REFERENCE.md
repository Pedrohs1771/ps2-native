# Independent VU reference for the original VIF case

This laboratory implements the separate VU implementation required by README
§31.1. It does not qualify the whole PS2 runtime, the complete Monster House
port, the hidden-state correspondence, or a final native package.

## Source identity and isolation

The reference is the official PCSX2 repository at commit
`94d86c891b1621c0b252e4fc2e155bf90274dcc0`. The instruction engine compiles
`VUops.cpp`, `VUflags.cpp`, and `VU1microInterp.cpp` byte-for-byte unchanged.
`VU.h`, `VUops.h`, `VUflags.h`, and `VUmicro.h` are unchanged too. GIF tag parsing
and packet sizing are unchanged slices of that revision's `Gif_Unit.h`, with
the original copyright/license header retained. The complete original header
and GPLv3 license are also staged. Source SHA-256 values are pinned in
`prepare_pcsx2_reference.py`; a mismatch fails configuration.

The adapter supplies a small surrounding environment instead of building the
whole emulator. Unsupported IRQs, asynchronous VIF wakeups, unknown opcodes,
unfinished microcalls, blocked GIF delivery, foreign-thread execution, and
unmapped starting states fail. These paths do not return invented success.
The optional reference archive only links to its laboratory executables.
Neither the native VIF executable nor final packaging depends on this engine.

Build flags are `-O1 -fno-lto -fno-strict-aliasing -ffp-contract=off`.
The explicitly selected FP policy is rounding toward zero, FTZ and DAZ enabled,
overflow clamping enabled, and the configurable ADD/SUB hack disabled. This is
an identified comparison configuration; it is not a hardware accuracy proof.
The upstream interpreter's own instruction arithmetic remains unchanged.

## Input domain and state mapping

The initial adapter accepts exactly the canonical reset VU1 snapshot. It checks
the complete byte encoding, including the hidden state, before accepting it.
Its schema-v1 reset encoding is fixed independently of the current model's
constructor; changes to that model cannot silently broaden this import domain.
It rejects a warm pipeline instead of dropping its pending writes. Microcode
and data each require 16 KiB of aligned, disjoint storage. VU0 activity and
interrupt enables are excluded from this replay's initial domain.

The Monster House original VIF capture meets this initial VU domain: cycle 0,
empty pipelines, no branch or transfer pending, VF0.w/Q/R reset bits, and all
remaining architectural registers reset. MPG and UNPACK still execute through
the current VIF parser. Each callback then executes the separate upstream VU
engine. Its registers, hidden pipelines, and monotonic VU clock persist across
callbacks. No register or cycle is overwritten to force comparison agreement.

VIF parsing, GIF arbitration, and the CPU GS remain shared components. The
reference's raw XGKICK chunk boundaries and cycles are retained. Chunks are
aggregated at the upstream EOP boundary before delivery to the shared GS;
this delivery translation is explicitly reported. Consequently a matching GS
output is evidence about VU output under this environment, not independent GS
accuracy or whole-machine scheduling accuracy. A physical PS2 trace and a
qualified relation between both hidden pipeline representations remain open.

## Build and run

Acquisition is explicit; ordinary CMake configuration does not download the
reference. Existing cached files are verified before staging. Unchanged files
keep their timestamps, avoiding another reference compilation on every run.

```sh
python3 lab/prepare_pcsx2_reference.py \
  --source build/lab/references/pcsx2/94d86c891b1621c0b252e4fc2e155bf90274dcc0 \
  --output build/lab/pcsx2-reference --fetch

cmake -S . -B build -DPS2X_BUILD_NEXO_LAB=ON \
  -DPS2X_FAST_ITERATION=ON -DPS2X_ENABLE_RELEASE_IPO=OFF \
  -DNEXO_PCSX2_VU_SOURCE_DIR="$PWD/build/lab/references/pcsx2/94d86c891b1621c0b252e4fc2e155bf90274dcc0"

cmake --build build --target nexo_vif_pcsx2_reference nexo_pcsx2_vu_reference_tests -j 4
env -u DISPLAY -u WAYLAND_DISPLAY -u PS2X_CAPTURE_SCENE \
  build/lab/nexo_vif_pcsx2_reference /absolute/path/to/observed-case /new/output/directory
```

The executable runs a private, windowless runtime. It requires a complete input
case and a new output directory. Exit 0 means all explicitly compared fields
agree; exit 2 means execution completed with comparison differences; exit 1
means an input, domain, execution, or publication failure. A `.complete` byte
is written last. It means a completed laboratory observation, not certification.

`report.json` records source revision, FP/device policies, comparisons, all
architectural differences, and callback clocks. Raw artifacts include GS/VIF/GIF
snapshots, actual data and microcode, engine chunk events, VIF events using the
reference's own clock, and register projections at every callback exit.
There is deliberately no fabricated canonical NEXO hidden-state output.

Binary artifacts use the existing version-1, little-endian, CRC32 envelope:

| Magic/variant | Payload |
|---|---|
| `NEXOVPR\0` / 1 | VF[32][4] u32 bits, VI[16] u16, ACC[4] u32 bits, Q/P/I/R/MAC/STATUS/CLIP/byte-PC u32, cycle u64 |
| `NEXOPXR\0` / 1 | u32 chunk count; each chunk: cycle u64, EOP-boundary bool8, byte blob |
| `NEXOPXC\0` / 1 | u32 callback count; each callback: complete `NEXOVPR` envelope as a byte blob |

## First original-case result

The first executed original case contained 1,825,472 VIF input bytes, nine VU
callbacks, and two uploaded 16 KiB banks. Under the identified environment:

- All 168 GIF submission payloads and paths agree with the recorded runtime.
- Final VIF, GIF, CPU-GS, microcode, and VU data bytes agree.
- Final projected registers, flags, and PC agree.
- VU time differs: the reference ends at 3,777 cycles, the recorded model at
  4,024 cycles. Canonical VIF events therefore differ too.

This exposes a timing obligation instead of hiding it behind matching images.
The current replay is `tested_only`; `full_reference_qualified`,
`vu_hidden_state_relation_qualified`, `gs_reference_independent`,
`final_package_qualified`, and `whole_gameplay_qualified` remain false.
The measured host time for this one call is not a gameplay FPS measurement.

## Complete instruction issue traces

Tracing is optional and disabled by default. The following commands replay the
same original VIF input with the original callback sequence. They do not open
a window or drive a running game process. All output directories must be new.

```sh
cmake --build build --target nexo_vif_issue_trace nexo_vif_pcsx2_reference -j 4
env -u DISPLAY -u WAYLAND_DISPLAY build/lab/nexo_vif_issue_trace \
  /absolute/path/to/observed-case /new/model-trace
env -u DISPLAY -u WAYLAND_DISPLAY build/lab/nexo_vif_pcsx2_reference \
  /absolute/path/to/observed-case /new/reference-trace --issue-trace
# The identified case returns 2: its timing disagreement is retained.
python3 lab/compare_vu_issue_traces.py \
  --case /absolute/path/to/observed-case --model /new/model-trace \
  --reference /new/reference-trace --output /new/timing-map.json
```

The model records every pair, including histories longer than the legacy
512-entry diagnostic ring. Each callback has its own complete receipt and
canonical input/output state. The reference uses the pinned interpreter's
upper-dispatch logging point after its leading cycle tick and issue stalls.
The unchanged upstream core is not patched to add observations. Its shim
identifies the existing diagnostic call; diagnostic arguments remain unevaluated.

`NEXOVPI\0` uses the version-1 CRC32 envelope. Variant 1 means the model issue
point and variant 2 means the upstream dispatch point. The payload is a u32
count followed by cycle u64, byte-PC u32, lower-word u32, and upper-word u32.
Each history is bounded at 262,144 pairs and 6 MiB. The adapter's identified
coordinate relation is `model issue = reference dispatch clock - 1`. Both raw
clocks are retained; this relation describes logging phases and does not remove
stalls or end-of-program drain time.

The mapper verifies receipts, checksums, callback partitions, original callback
arguments, full code-bank bytes, and clock horizons. A changed PC, instruction,
or pair count stops timing alignment. It reports the first disagreement and
every change in accumulated issue delta, then separately reports the tail from
the last issue to callback completion. Input and output digests bind this map
to its observed artifacts.

## Observed timing decomposition

The complete original-case trace contains **3,761 identical instruction pairs**
across nine callbacks. Enabling tracing preserves the model's entire recorded
output state and event stream. The independent reference also preserves its
untraced architectural outputs, packets, and cycle count.

| Callback | Pairs | Last issue delta | Model tail | Reference tail | Elapsed delta |
|---|---:|---:|---:|---:|---:|
| 0 | 254 | 8 | 14 | 1 | 21 |
| 1 | 254 | 8 | 14 | 1 | 21 |
| 2 | 255 | 10 | 1 | 1 | 10 |
| 3 | 245 | 10 | 1 | 1 | 10 |
| 4 | 412 | 11 | 14 | 1 | 24 |
| 5 | 412 | 11 | 14 | 1 | 24 |
| 6 | 1,105 | 16 | 74 | 1 | 89 |
| 7 | 412 | 11 | 14 | 1 | 24 |
| 8 | 412 | 11 | 14 | 1 | 24 |

All values are guest cycles. For each callback, elapsed delta equals last
issue delta plus model tail minus reference tail. Totals are **96 issue cycles
and 151 tail cycles**, accounting for the entire 247-cycle disagreement.

Bank 0 first differs at byte-PC `0x148`, lower word `0x800f18f0`: an integer add
consuming VI15 after ILW. Bank 1 first differs at byte-PC `0x28`, lower word
`0x80016b70`: an integer add consuming the preceding loaded VI registers.
The model waits for its four-cycle pending VI writes. In the pinned upstream
interpreter, `_vuRegsILW` declares four cycles but `_vuTestLowerStalls` only
dispatches integer dependency stall checks for the branch pipeline. ILW writes
the upstream VI value immediately; IADD can therefore issue before that load's
declared completion. This is a difference between implementations, not proof
that either implementation's timing agrees with physical hardware.

At E termination the upstream core clears its VU-running bit before flushing
pending XGKICK data. The final packet transfer then does not advance its VU
clock. The model's `flushPipelines` advances cycles while pending transfers
drain. Two reduced cross-engine regression cases preserve these observations:
ILW followed by dependent IADD, and an E-terminated XGKICK IMAGE packet compared
with the same two-pair NOP program. They deliberately preserve timing
disagreements while checking the loaded value and completed transfer.

## Remaining timing obligations

Retain exact original input, source, executable, policy, and output identities.
Use the complete trace and reduced microtests to establish the required
hardware timing and observable device boundaries before changing the model.
Do not subtract a fitted constant, reset a
clock between callbacks, or adopt every PCSX2 approximation as hardware truth.

Then expand the input-state relation beyond reset, execute on a second
architecture, acquire independent GS/device and physical timing evidence,
and proceed through the remaining IOP AOT, GPU GS, game progression, audio,
controls, saves, Android, and catalogue gates in the root README.

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

## Next diagnostic work

Retain exact original input, source, executable, policy, and output identities.
Locate the first instruction timing disagreement using pair-level traces of
both engines. Distinguish instruction issue stalls, pipeline completion, and
XGKICK drain timing. Construct independent microtests for the implicated
dependency before changing the model. Do not subtract a constant, reset a
clock between callbacks, or adopt every PCSX2 approximation as hardware truth.

Then expand the input-state relation beyond reset, execute on a second
architecture, acquire independent GS/device and physical timing evidence,
and proceed through the remaining IOP AOT, GPU GS, game progression, audio,
controls, saves, Android, and catalogue gates in the root README.

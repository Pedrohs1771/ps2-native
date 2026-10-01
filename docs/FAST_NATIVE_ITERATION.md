# Native iteration and complete static dispatch

## Development build

Use these options in the generated desktop project:

```sh
cmake -S <desktop-project> -B <desktop-project>/build \
  -DPS2X_FAST_ITERATION=ON \
  -DPS2X_ENABLE_RELEASE_IPO=OFF \
  -DPS2X_ENABLE_RUNNER_PCH=ON
cmake --build <desktop-project>/build --target ps2EntryRunner --parallel 12
```

`PS2X_FAST_ITERATION` uses `-O1 -fno-lto` on GNU/Clang Release builds.
Registration code uses `-O0 -fno-lto` and skips PCH because its compiler
options differ. Generated sources larger than 4 MiB also use `-O0` without PCH
in fast iteration, bounding optimizer cost for outliers. Other game sources
use `-O1` with PCH. PCH resolves declaration headers from the actual generated
game directory. The host pipeline supplies these options explicitly, including
when configuring an existing CMake cache.

For a final optimized package, disable fast iteration and enable release IPO.
Compilation alone does not validate game compatibility.

## Offline EE overlay banks

The experimental `PS2X_RUNTIME_AOT_EE_OVERLAYS=ON` backend consumes a hashed
`NEXO_EE_BANK_MANIFEST`. Generation and compilation happen offline; the running
game chooses existing callbacks by PC and source-byte identity and stops on
uncovered code. Its private selection flag does not affect every common runtime
object's compiler flags. The diagnostic overlay driver implementation is omitted
from the AOT backend. See [the V0 contract](../schemas/nexo-ee-aot-v0.md).

Monster House's 32 observed banks retained all 32 compiled bank objects during a
catalog-index update; that build took 10.117 seconds. The actual game relink took
12.757 seconds and reused its original EE objects. The first catalog build still
required offline compilation. These measurements do not promise whole-ISO
conversion times, complete dynamic-code coverage or gameplay compatibility.

For an isolated AOT observation, pass `--disable-overlay-driver` to
`tools/ps2native/headless_native_test.py launch` with an explicit laboratory
runner. The helper removes the inherited diagnostic compiler driver in its owned
child and records this choice. A miss must be diagnosed from full source/state
identity; registering an already registered PC cannot repair a rejected version.

## Indirect calls

The recompiler registers every decoded instruction boundary in every generated
non-stub EE function. This includes interior callback thunks in functions that
contain no indirect calls of their own. Overlaps use the most specific decoded
owner. Function starts retain their own entries, and unresolved/undecoded
addresses remain unresolved.

The generated wrapper selects the exact label from `ctx->pc`. Direct entry into
a branch delay-slot instruction uses a separate handler that executes that
instruction alone, publishes its next PC, and returns to the native dispatcher.
Normal branch execution still executes its architectural delay slot once.
A branch inside an independently entered delay slot is explicitly unsupported.

This covers decoded static code; dynamic overlays, self-modifying code, missing
instruction implementations and PS2 device compatibility require separate work.
The code remains native EE translation backed by the PS2 compatibility runtime.

## Build cost

Consecutive addresses sharing a wrapper are emitted as ranges. The generated
module registrar expands those ranges at startup; the dense dispatcher fills
exact ranges with `std::fill_n`. Holes and different owners are preserved.
The number of runtime bindings does not decrease.

Multi-file generation compares existing contents before writing each C++ source
and declaration header. Identical files retain timestamps, allowing CMake to
reuse their objects and PCH. Changed files are written and checked for I/O errors.
Combined single-file output and diagnostic CSV output are not covered by this
timestamp optimization.

## Reproduced checks (2026-09-29)

- Monster House: 698,143 decoded interior instruction boundaries registered;
  699,283 resumable entries after the existing CFG and configured targets.
- Previously missing `0x176b10`, `0x1639e0` and `0x177350` are emitted without
  their three manually added TOML entries.
- Registration C++: 136,045,196 bytes before compression, 2,253,899 after.
- Repeating generation of this Monster House workspace took 5.66 seconds under
  concurrent compilation and changed zero of 7,244 C++/header timestamps.
- `tools/ps2native/check_dispatch_fixture.py` compiles generated code from a
  synthetic ELF and executes four paths. It also verifies all 16 compressed
  dense bindings. No commercial assets or graphics window are required.
- The code-generator suite covers range holes, overlapping owners and independent
  delay-slot labels. The incremental-output regression checks both unchanged
  timestamps and replacement of modified output.

Run the relevant checks from the repository root:

```sh
cmake --build build --target ps2x_tests ps2_recomp --parallel 2
./build/ps2xTest/ps2x_tests
python3 tools/ps2native/check_dispatch_fixture.py
python3 -m unittest discover -s tools/ps2native/tests -v
```

These checks establish the dispatch/build changes. They do not certify a
playable Monster House campaign; runtime boot, rendering, audio, input and saves
must be checked on the actual native package.

## Live native EE overlays (Linux development)

Set `PS2X_NATIVE_OVERLAY_DRIVER` to the absolute path of
`tools/ps2native/native_overlay_driver.py`. A missing EE target can then be
translated from its current RAM contents into C++ and compiled as a native
shared library. The runtime registers the resulting instruction entries; there
is no per-address TOML edit or rebuild of the full game runner for this path.
Build `ps2_native_overlay` in the repository's `build` directory first.

```sh
cmake --build build --target ps2_native_overlay --parallel 4
cd <package>
PS2X_FUNCTION_TRACE=0 PS2X_TRACE_SIF_DMA=0 \
PS2X_NATIVE_OVERLAY_DRIVER=/absolute/repository/tools/ps2native/native_overlay_driver.py \
./bin/ps2EntryRunner ./game/boot.elf /absolute/path/to/game.iso
```

The driver uses a host C++ compiler with `-O0 -fno-lto`, without a shell.
`clang++` is the default. `PS2X_NATIVE_OVERLAY_CXX`,
`PS2X_NATIVE_OVERLAY_GENERATOR`, and `PS2X_NATIVE_OVERLAY_CACHE` can override
its compiler, generator and cache directory. The runner must export runtime
symbols for these libraries; the generated Linux desktop project already uses
`-Wl,--export-dynamic`.

The cache includes code bytes, entry address, tool binaries, driver source and
runtime headers. Four Monster House libraries loaded from cache in 66–67 ms
each in the SPU timing smoke. A new cross-boundary batch took 9.63 seconds.
These are batch measurements on this machine, not a full-game conversion time.
Windows, macOS and Android live compilation are not implemented by this driver.

Snapshots are normally 64 KiB. A root near the end uses an overlapping window
so that a branch and its delay slot can cross the previous boundary. Blocks are
limited to 128 instructions, with global block/instruction budgets; speculative
prefetch does not guarantee coverage of every address in a window. Each lookup
checks the owning block's bytes before reusing native code. Failed translations
are retried when the snapshot changes, including changes beyond the first
16 bytes. Unsupported instructions and branch-in-delay-slot cases remain errors.

`PS2X_FUNCTION_TRACE=0` suppresses the shared function-entry/exit file stream in
already compiled tracing builds. `PS2X_TRACE_SIF_DMA=0` suppresses verbose DMA
calls/descriptors. Other diagnostics remain available. These options avoid
rebuilding thousands of generated objects solely to turn off this tracing.
For IOP debugging, `PS2X_IOP_DEBUG_SYMBOLS=ON` adds symbols only to its emulator
and thread-kernel implementation files. It does not enable LTO.

## IOP compatibility fixes checked on 2026-09-29

- LOADFILE function 6 and the EE module-buffer helper read physical IOP RAM.
- Resident IRX entry points receive `argc/argv`, including the module name and
  NUL-separated arguments, rather than a raw byte-count/payload pair.
- Internal SDK `_sceSifSendCmd` wrappers remove their extra `mode` argument
  before dispatching to the public six-argument helper.
- IOP thread alarms retain callback GP/userdata, use a 64-bit deadline, repeat
  according to the callback return value, and support cancellation.
- Sysclib `_wmemcopy` and `_wmemset` sizes are bytes. Only complete words are
  touched; tests protect adjacent allocations and partial-word tails.
- SPU AutoDMA output uses 768 IOP clocks per 32-bit stereo PCM sample frame,
  while ordinary SPU transfers retain their existing RAM-copy timing. The game
  was observed using core 1 AutoDMA with a 2048-byte buffer. Its old 1024-cycle
  completion generated refill callbacks much faster than audio playback.
- The IOP heap reuses freed buffers and gaps between live allocations, checks
  arithmetic overflow/alignment and reports the largest free gap.
- IOMAN supports read-only host/CD files through the host VFS: open, close,
  bounded physical IOP reads, signed seek, EOF and reset cleanup. Writes return
  an explicit error. This does not implement every custom IOP device operation.

The IOP test group has four executables. The root native overlay integration
also checks cross-window delay slots and retry after a failed block changes:

```sh
cmake --build build --target ps2x_tests --parallel 4
PS2X_TEST_NATIVE_OVERLAY_DRIVER="$PWD/tools/ps2native/native_overlay_driver.py" \
./build/ps2xTest/ps2x_tests
ctest --test-dir /tmp/ps2native-iop-tests --output-on-failure
```

## SIF command tables and VIF image transfers

The EE and IOP SIF command registration helpers now mirror function/userdata
pairs into the guest command buffers. Dispatch reads those buffers, including
guest writes performed without the registration helper. This keeps the SDK's
reserved control handler visible when the RAD code allocates its own handlers.
The IOP path retains callback GP and clears handlers associated with an unloaded
module. Tests cover registration, direct writes, removal and module cleanup.

VIF DIRECT payloads are assembled before being submitted to GIF. Continuation
across DMA segments retains the declared byte count; VIF command words are not
uploaded as pixels. GIF IMAGE data can also continue across separate DIRECT
commands. Tests include TTE command words before image data, reset, independent
memory instances, and readable VIF register state. This does not establish
complete PACKED/REGLIST fragmentation or every MPG/UNPACK case.

## Packed MMI arithmetic used by movie decoding

The generated implementations of PMULTH, PMADDH and PMSUBH preserve all eight
signed halfword products in the R5900 HI/LO banks. PMFHL's five modes, PMTHL.LW,
the 128-bit HI/LO moves, PINTH/PINTEH, PADDUH/PSUBUH and PEXEH/PREVH now preserve
their lane ordering, signedness and saturation. Source values are captured
before writing aliased destinations; writing register zero still updates HI/LO
where required. These are translator fixes; they do not certify every MMI
instruction or every legacy runtime macro.

```sh
python3 tools/ps2native/check_mmi_fixture.py
```

This fixture generates a synthetic ELF, compiles the emitted C++, and executes
21 variants against independent scalar oracles, with 512 boundary/random cases
each: **10,752 native executions passed**. It checks all HI/LO banks, register
zero, aliased operands and return behavior. The previous PMULTH implementation
failed its first case. Regenerating Monster House changed 12 generated C++
files; the remaining game objects were reused.

## COP0 Count and observed native video

The scheduler advances a shared COP0 Count with its virtual EE cycle clock.
Threads and callbacks observe that shared value instead of restoring stale
copies from saved contexts. Guest MTC0 writes are retained, the 32-bit counter
wraps, and idle/VSync time also advances it. Four regressions cover wraparound,
thread switches, callback writes and idle time. Compare interrupts and
cycle-exact CPU timing are not established by this change.

Monster House's Bink clock reads Count and converts 294,912 cycles into one
millisecond. The counter previously never advanced, so the game repeatedly
displayed its first movie frame. After the clock and MMI fixes, actual native
image transfers advance through colorful Columbia intro frames, and the game
window proceeds into later publisher introductions. The capture comes from
the game's own generated code and GS transfers; no external video decoder is
substituted into the runtime. Start input is visible in `padread` diagnostics.

## Controller byte-array ABI

`scePadRead` publishes the active-low button **low byte at offset 2 and high
byte at offset 3**. This is the little-endian `unsigned short btns` field in
[PS2SDK's padButtonStatus](https://github.com/ps2dev/ps2sdk/blob/master/ee/rpc/pad/include/libpad.h).
An earlier change reversed these bytes after inspecting only a boot/menu
consumer that reconstructs them with `LBU`, `SLL 8`, `OR`. That diagnosis was
wrong: the actual gameplay consumer tests Start in byte 2 and Cross in byte 3.
The reversed implementation made R1 pause the game and Enter fail to pause.
The corrected native run confirms Enter pauses and R1 no longer pauses.
Tests check literal bytes for all relevant combinations and independently check
Start/R1 and Cross/Down separation across all 16 button bits.

The raylib host backend also previously mapped WASD to D-pad buttons while
leaving both analog axes at 128. It now maps WASD to the left stick and arrows
to the D-pad, matching the existing SDK fallback. Opposing keys neutralize that
axis; diagonals can drive both axes. Two regressions cover all 16 WASD
combinations and 18 arrow/button keys through the same private mapping used by
the backend. A live `PSPadBackend::readState` capture confirmed W changes LY
from 128 to 0 without pressing Up. That verifies packet delivery, not visible
player movement in the incomplete scene.

Latest reproduced root check: **483/483 tests**. Earlier independent checks:
**4/4 IOP test groups**,
**18/18 Python pipeline tests**, the native dispatch fixture's **4 execution
cases / 16 bindings**, and the **10,752 MMI executions**. These measurements
cover the named checks, not a complete campaign.

## Memory-card exact directory queries

`sceMcGetDir` searches the last path component, even when that component names
an existing directory. An exact save-directory query now returns that directory's
single metadata entry; `directory/*` still enumerates its children. The previous
implementation silently converted existing directories into `*` searches.
Monster House received seven entries for its save directory, reported changed
card statistics and failed a subsequent save attempt. The new regression failed
before this fix and passes for relative, absolute and wildcard directory names.
This fix follows the filename-search interface documented in
[PS2SDK libmc](https://github.com/ps2dev/ps2sdk/blob/master/ee/rpc/memorycard/include/libmc.h).
It does not establish directory-search continuation/pagination support.

## Scene diagnostics without rebuilding

Launch the native runner with `PS2X_CAPTURE_SCENE=/absolute/capture-directory`.
Create that directory and write an empty `.request` file inside it to request
one capture. At a guest executor checkpoint, the runner consumes the marker
and writes a unique `capture-*` subdirectory with EE RAM/context, the published
main EE context, GS VRAM,
VU1 code/data and textual register/graphics metadata. The opt-in session also
retains the bounded GS event ring. With the variable unset, capture is disabled.

```sh
mkdir -p /absolute/capture-directory
touch /absolute/capture-directory/.request
```

Captures contain game data and remain local. They are diagnostic files tied to
the host ABI, not portable or restorable savestates. Two regressions check binary
sizes, guest bytes, metadata, unchanged guest state and missing initialization.

## R5900 square-root operands

R5900 `SQRT.S` reads the radicand from FT. `RSQRT.S` computes FS / sqrt(FT).
The translator previously used FS for SQRT and computed 1 / sqrt(FS) for RSQRT.
The fix is shared by static game translation and the native overlay generator.
The operand definitions also match
[PCSX2's EE FPU implementation](https://github.com/PCSX2/pcsx2/blob/master/pcsx2/FPU.cpp).

Monster House instruction `0x46010044` at `0x2e4eb0` must compute F1 = sqrt(F1).
The previous generated code computed F1 = sqrt(F0), where F0 had just been set
to zero. The following reciprocal then divided by zero. A first-chapter RAM
capture showed NaNs in the collision direction and transformed support vector,
and roughly 61 million iterations in the collision loop. This establishes a
translation error. The corrected native run advances through the 3D prologue
and into Chapter One's HUD/tutorial instead of remaining in that collision
loop. Rendering remains incomplete; campaign compatibility is not established.

```sh
python3 tools/ps2native/check_fpu_fixture.py
```

The synthetic ELF fixture compiles the emitted C++ and executes seven SQRT/RSQRT
operand/alias variants over 512 cases each: **3,584 native executions passed**.
The pre-fix translator failed the first case. The root suite passed **469/469**
at that stage, and the packed MMI fixture still passed **10,752** executions. Regenerating
Monster House changed 138 generated sources; unchanged game objects were reused.
These checks cover finite operands and nonnegative square-root inputs, including
zero for SQRT. They do not certify exceptional-value behavior, FCR31 flags or
exact PS2 rounding.

## VIF1 payloads split across transfers

VIF1 now retains incomplete command headers and STMASK/STROW/STCOL/MPG/UNPACK
payloads across FIFO or DMA chunks. Later payload bytes are never interpreted
as fresh commands simply because they resemble an opcode. UNPACK's retained
length accounts for STCYCL fill-write cycles and NUM=0. Complete streams keep
their existing direct read path; only incomplete commands require concatenation.
The internal per-memory state is released by FBRST, initialization and destruction,
without changing the public memory ABI or rebuilding generated game objects.

A regression compares complete and fragmented streams for seven chunk sizes
from one to 31 bytes, including control registers, microcode, unpacked vectors
and following commands. It failed before the fix. Reset and instance-isolation
checks also pass. The root suite passed **475/475** at that stage.
VIF0 streaming and partial-transfer timing still require separate coverage.

## VIF1 V2 replication and packed color values

V2 UNPACK must write X,Y,X,Y rather than leave Z/W at their previous values.
The decoder now replicates the two decoded lanes before masking and STMOD,
for 32-, 16- and 8-bit sources, with signed/unsigned extension retained.
V4-5 expands each five-bit RGB field by three bits and the alpha bit by seven,
yielding RGB 0..248 and alpha 0/128. V4-5 continues to bypass STMOD.
These rules match [PCSX2's VIF unpack implementation](https://github.com/PCSX2/pcsx2/blob/master/pcsx2/Vif_Unpack.cpp).
The two new regressions failed before the fixes and passed afterward:
**478/478 root tests** at that stage. They cover six V2 formats and V4-5
under all four STMOD values with nonzero ROW registers. V3's source-dependent
fourth lane, STMOD=3 ROW updates and zero-valued STCYCL lengths remain separate
questions; this is not complete VIF conformance.

## VU write chains and raw MIN/MAX operands

A branch following consecutive writes to one VI must retain the value from
before the entire write chain. The interpreter previously replaced that saved
value at each write. Monster House issues consecutive SQI instructions, then
compares the output counter with its endpoint. Losing the original counter
caused the equality to be missed repeatedly. Tests cover SQI, LQI and IADDIU
chains, alongside the existing single-write, intervening-instruction and stall
cases. Actual VI writes and their pipeline commits remain separate from the
value retained for the branch.

LOI stores raw bits. VU MIN/MAX select raw operands by encoded numeric order,
preserving denormals, signed zeros and values whose IEEE encodings resemble
infinities or NaNs. Arithmetic continues to normalize operands when consumed.
Monster House constructs GIF fields 1, 6 and 14 using LOI/MAXi; flushing those
values to zero produced malformed graphics packets. Regressions cover exact
GIF tag construction and 48 MIN/MAX vector, broadcast and immediate cases,
including aliased destinations and unchanged arithmetic flags. The semantics
match [PCSX2's VU implementation](https://github.com/PCSX2/pcsx2/blob/master/pcsx2/VUops.cpp).

One actual Monster House MSCAL input was reproduced locally:

| Runtime | Result |
| --- | --- |
| Before the write-chain fix | Reached 65,536-cycle limit, no program end |
| Write-chain fix alone | Advanced in 412 cycles, then stopped on malformed GIF packet |
| Write-chain and LOI/MIN/MAX fixes | Program ended normally in 438 cycles, no stop or limit |

This checks one captured graphics program, not complete game compatibility.
The installed runner has since displayed a progressing, corrupted 3D prologue
and the first-chapter movement tutorial. This does not establish playable 3D
gameplay.

## Isolated graphics replay

With `PS2X_CAPTURE_SCENE` enabled, create `.vu-request` in its directory to
capture the next fresh VU1 MSCAL execution. Its `vu-input-*` directory contains
microcode, input/output data, input/output `VU1State` and a bounded ring of the
last 512 issued instruction pairs with VI registers and flags. `trace-state.bin`
also stores the full `VU1State` for each retained issued pair, including VF,
ACC, Q/P/I and arithmetic flags. On this host each record is 664 bytes; this
is a raw host ABI format, not a portable interchange format. The metadata
distinguishes normal termination, a stop and exhaustion of the cycle budget.
No further captures occur without another marker. MSCNT's hidden pending
pipelines are not captured by this mechanism.

`tools/ps2native/replay_vu_capture.cpp`, linked against the same host runtime and
the test function table, reproduces a captured MSCAL without a game boot or
graphics window. It accepts the capture directory, an optional cycle budget
and an optional output directory. Exit 0 requires recorded normal termination;
exit 2 reports exhaustion of the budget, and exit 1 reports an error or other
stop. A low cycle count alone is not treated as success. Diagnostic inputs
remain local and depend on the host state ABI.

## Current Monster House progression

The native runner has now displayed the Portuguese autosave notice,
the Start prompt, and the main menu. Enter sends Start; X/Space sends Cross.
Selecting New Game advances into the memory-card dialogs. Left changed the
save-creation choice to Yes, and Cross confirmed it. The game created
`mc0/BASLUS-21400/BASLUS-21400` (1,204 bytes) plus its icon files, then read back
the save payload and icon data through libmc. This establishes initial file
creation and reads, not recovery of campaign progress after a fresh launch.

The game subsequently displayed a progressing but visually corrupted 3D
prologue, the loading titles “A Casa Monstro” and “Capítulo Um / Dentro da Casa”,
and Chapter One's HUD and movement tutorial. Enter pauses the chapter; R1
does not pause it. Loading an existing save displays “Carregado com sucesso”
and reaches the first chapter without repeating the long exterior prologue.
This verifies an initial save load, not arbitrary campaign save/resume.

Before the double-libm fix below, the chapter's world was mostly black. Small floor/fire fragments appeared
during its introduction; the complete environment and player movement remain
unverified. Native code loaded into RAM is
translated automatically in batches, with no manual TOML address additions.
The main-menu smoke measured warm batches around 67–81 ms and new batches
around 5–9 seconds on this host; these are batch costs, not complete ISO
conversion times.

Playable 3D gameplay, audible sound, full save/resume and Android operation
remain unverified. The current package is experimental. Logs and captures are in the
Monster House workspace's `logs/` directory; `dispatch-validation.json` records
the latest confirmed stage. In particular, `pad-corrected-menu-smoke.log`,
`native-main-menu-window.png`, and `native-create-save-dialog.png` document the
real menu/initial-save run.

The current root suite is **484/484**, with durable red/green test logs for the
SDK pad layout, VIF formats, keyboard analog mapping and double libm ABI. The latest package
contains normal native builds of these fixes; changes to private runtime files
did not require regeneration or recompilation of game translation units.
The long-running GDB session separately used temporary VIF and pad function
redirects to preserve its chapter state. Those diagnostic redirects are not
a production hot-reload facility or a fresh-launch validation of the package.

## Black-scene investigation

Both framebuffers in a first-chapter capture contain almost only RGB 0/1.
This establishes an actual rendered-output problem rather than only a window
presentation problem. Twelve valid raw GIF packets and the GS event history
show a full-screen untextured gray quad, after HUD primitives, with blending
enabled. Its alpha changed from 128 in an earlier capture to 1 later; this is
a diagnostic lead, not an established cause. Untextured packets' unused ST
fields may contain arbitrary values and are not evidence of corruption.

Thirty-two sampled MSCNT executions completed in 1,665..5,594 cycles with a
65,536-cycle budget; none exhausted it. This rules out budget exhaustion for
those samples, not for every VU program. The software GS rasterizer and VU
interpreter remain performance work. No game-specific primitive suppression
has been added to the runtime. Diagnostic samples, packet captures and the
SDK-corrected menu/pause screenshots are retained under `logs/`.

### Zero world coordinates traced back to camera projection

A bounded sample of 1,600 `GSCpuBackend::Submit` batches contains 1,340
textured triangle-fan batches for the two main framebuffers. All their world
vertices arrive with X, Y and Z equal to zero; HUD coordinates in the same
sample remain valid. A captured fresh VU1 invocation reproduces zero XYZ in
its generated GIF packets and ends normally after 7,200 cycles. These are
measured samples, not a claim about every model or VU invocation.

The invalid projection already exists in the VU input and EE RAM. Tracing its
EE matrix producer shows a valid screen transform and an incoming camera
projection containing infinities. The perspective builder receives a horizontal
field of view of about 1.303225 and a vertical field of view of zero; its vertical
reciprocal consequently becomes infinity. This localizes a bad camera input
before VIF, VU execution and GS presentation. A fresh packaged-runner launch initially sets both camera
values correctly, including approximately 1.303225 / 0.977419. A hardware
watchpoint later catches their first transition to zero through the original
camera angle conversion: `fptodp -> atan -> dpmul -> dptofp -> tanf`. The
double `atan` stub returned float bits in V0. The following double operations
consumed those bits as a tiny double, which narrowed to zero. This is a libm
calling-convention error, reproduced in a fresh runner before the fix.

A temporary debugger-only experiment omitted the observed full-screen gray
quad. It exposed the HUD more clearly but did not restore the world. No such
omission, camera constant or game-specific projection override is included in
the runtime.

Evidence: `native-gs-batches.bin`, `native-gs-batch-layout.json`,
`native-gs-batches.json`, `native-world-any-gif-packets.jsonl`,
`native-perspective-input.json`, `native-camera-projection-fields.json`,
`native-fov-start-inputs.jsonl` and the captured `vu-input-*` files under the
workspace's `logs/` directory. Raw batches, contexts and VU state are tied to
this host ABI. The debugger samples and isolated VU replay do not prove a
complete fresh-launch playthrough.


## Double libm arguments and return values

The standard double libm stubs now read 64-bit A0 (and A1 for `atan2`/`pow`)
and return the IEEE double bits in V0. They no longer read or overwrite the
single-precision FPRs. The change covers `sqrt`, `sin`, `cos`, `tan`, `atan`,
`atan2`, `pow`, `exp`, `log`, `log10`, `ceil`, `floor` and `fabs`. Float kernel
entry points retain their float argument conventions. No public runtime header,
generated source or per-game configuration change is involved.

For Monster House, the original `atan`, `sin`, `cos`, `fabs` and `floor` machine
code consumes A0's full 64-bit value, consistent with the guest's soft double
conversion helpers. A fresh pre-fix watchpoint caught camera FOV becoming
0 / 0 inside `sub_001C60D0`, after the libm conversion chain. This reproduces
the earlier bad camera independently of the long-running session's VIF/pad
redirects.

Three regressions exercise 66 unary calls, ten binary calls and four camera
angle round trips, including signed zero, precision beyond float, NaN/domain
cases and preservation of all float registers. They failed before the fix:
**480 passed / 3 failed**. Afterward the root suite passes **483/483**. These
are host-libm ABI tests, not certification of PS2 libm's exact rounding, errno
or all compiler-specific short-double variants. Symbol recognition must also
identify the calling convention for those variants before substituting a stub.

A debugger-only continuation redirected these 13 functions to the corrected
bodies and restored the immediately preceding camera values recorded by its
getter. The native camera conversion then produced roughly 0.622356 / 0.466767;
128 further perspective samples had valid vertical FOV. In 276 sampled
textured fan batches, coordinates became finite and nonzero. Rendering still
showed serious defects, including a dark environment and corrupted HUD. Omitting
the gray quad in a separate debugger experiment exposed a small character
fragment; that primitive omission is not included in the runtime. These tests
do not establish playable gameplay or a clean complete playthrough.

The normally compiled runner/runtime have been repackaged. A separate fresh
launch now displays a progressing 3D interior with floor, walls and stairs,
without function redirects, restored camera values or primitive omissions.
Graphics remain defective; corrected chapter controls and a complete campaign
are still unverified. `native-libm-fixed-fresh-interior.png` records this clean
run. Durable evidence also includes `libm-double-{red,green}-tests.log`,
`libm-double-native-build.log`, `native-fresh-fov-zero-write-{context,ee}.bin`,
`native-fresh-fov-zero-write-stack.txt`, `native-post-math-fan-batches.bin`,
`libm-double-diagnostic-redirects.json` and `native-libm-fixed-fresh-*` logs.

## Measured VU execution improvements

A 24-interruption CPU sample found 20 stacks in VU execution, two in software
GS drawing and two in the IOP ready-thread scan. This small, nonuniform sample
identified an optimization target; it is not an exact CPU-time percentage.
`native-libm-fixed-cpu-samples.json` retains the stacks.

The VU scheduler now visits only the set bits of VI dependency masks instead
of scanning VI1..VI15 for every instruction pair. It excludes constant VI0 and
preserves lowest-register selection and writeback latency. Double-width FMAC
flag calculation normalizes Q/I only when the instruction uses them. Neither
change skips guest instructions or changes a game-specific address table.

`PS2X_FAST_ITERATION_OPTIMIZE_VU` defaults to ON. For GNU/Clang fast-iteration
Release/RelWithDebInfo builds it appends `-O2` only to the three private VU
sources. Generated game code remains at `-O1`, and LTO remains disabled.
The Makefiles dry run scheduled 48 runtime/utility compilations because their
shared flags file changed, including those three VU sources; it scheduled
zero generated game or PCH compilations. Subsequent source-only edits retain
the existing short relink path.

Alternating before/after trials used three actual MSCAL captures, three trials
and 3,000 replays per variant per trial: **54,000 replays**. Each replay checked
VU state and data against its first replay; that replay also had to terminate
at the recorded PC and reproduce the recorded data. Before and after variants
had identical state/data hashes in all three captures.

| Capture | Earlier runtime, median µs | Optimized runtime, median µs | Time reduction |
| --- | ---: | ---: | ---: |
| Interior, 1,650 cycles | 433.466 | 290.029 | 33.1% |
| Earlier scene, 4,873 cycles | 1,286.710 | 872.148 | 32.2% |
| Diagnostic continuation, 2,062 cycles | 485.227 | 334.043 | 31.2% |

These are warm VU replay timings with an isolated GS instance, not full-frame
timings, cold compilation costs, VRAM comparisons or proof of 60 FPS.
The initial mask-only change reduced median time by 7.5..9.1%; the final table
also includes deferred scalar normalization and source-local `-O2`.
Evidence: `vu-{mask,lazy,final}-alternating-trials.json`,
`vu-{mask,final}-benchmark-summary.json`, and `vu-o2-desktop-build.log`.

`tools/ps2native/benchmark_vu_capture.cpp` resets the interpreter between
independent replays. `execute()` intentionally retains its absolute scheduler
clock; omitting `reset()` initially made the benchmark report nondeterminism
because the cycle counter accumulated. Restoring state uses raw byte copies
to retain the local capture ABI padding. The benchmark does not serialize
hidden MSCNT pipelines or provide a portable save-state format.

The all-VI-bits regression passed before and after the scheduler optimization.
The root suite passes **484/484**. A separate test executable linked against
the actual desktop runtime with VU at `-O2` also passes **484/484**, launched
from the project root as required by its source-reading tests. Its initial
launch from the game workspace failed the source-file lookup test; the VU
tests passed. `vu-o2-desktop-root-tests.log` records the corrected invocation.

## Isolated headless native testing

`tools/ps2native/headless_native_test.py` starts Xvfb on a free display numbered
90 or higher. It records the runner PID and private Xauthority file path in a
local session JSON, and routes only this runner's audio to a temporary PulseAudio
null sink. It does not change the default audio output. Its screenshot and
input commands verify that the live runner owns that virtual display and refuse
desktop displays. Key releases run even when a held-input test is interrupted.
No desktop window activation, desktop screenshots or physical input is needed.

Requirements on this Linux host: Xvfb, xvfb-run, xauth, xdotool, ImageMagick
`import` and a running PulseAudio-compatible `pactl` service. Run from the game
workspace, using the user's local ISO:

```sh
python3 /home/pedrohs/Downloads/ps2-native-recompiler/tools/ps2native/headless_native_test.py launch \
  --package package \
  --iso '/home/pedrohs/Downloads/Monster House (BR-USA) (T2.0) (www.romsportugues.com).iso' \
  --state logs/headless-native-session.json \
  --log logs/native-headless-optimized.log
```

The launch command stays alive with the runner and prints `status: ready` when
its virtual window exists. In another terminal/process:

```sh
python3 /home/pedrohs/Downloads/ps2-native-recompiler/tools/ps2native/headless_native_test.py screenshot \
  --state logs/headless-native-session.json --output logs/headless-checkpoint.png
python3 /home/pedrohs/Downloads/ps2-native-recompiler/tools/ps2native/headless_native_test.py press \
  --state logs/headless-native-session.json w --hold 2
```

SIGINT/SIGTERM to the launcher closes its runner/display and removes only the
silent sink it created. The session file remains as a record; a terminated
session cannot send input. The local smoke confirmed screenshots, Cross and
Start on `:90`, and rejected a `:1` session before executing any screenshot or
input command. The observed Xvfb OpenGL renderer is **llvmpipe**. Full-game FPS
in this environment must be reported separately from hardware-GPU desktop FPS.
Muted headless playback does not validate audible sound.

The fresh optimized package has now reached that same progressing interior on
`:90`: floor, walls, stairs and three animated characters are visible, with
character rendering defects. `headless-vu-o2-current-scene.png` and
`headless-vu-o2-interior-start.png` record it. No debugger redirects or camera
restoration were used. `headless-harness-validation.json` records five CLI
checks, and `headless-isolation-verified.json` confirms the runner's audio
stream is on its silent sink. Player control and stable 60 FPS remain unverified.
Four additional malformed/stale/desktop session records were rejected before
input or capture, recorded in `headless-harness-schema-validation.json`.
A 15-second sample of the progressing interior cutscene counted 23 presentation
uploads, approximately **1.53 uploads/second**, on Xvfb/llvmpipe. This poor rate
is retained in `headless-vu-o2-interior-presentation-rate.json`; the VU replay
speedup must not be presented as a 60-FPS or hardware-GPU result.

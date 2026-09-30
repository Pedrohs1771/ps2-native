# NEXO VIF1, GIF queue and CPU GS checkpoints, version 1

These laboratory formats describe the **identified current runtime model**.
Matching a restored model against the same model is `tested_only` evidence.
It does not establish independent PS2 correctness, full-game native execution,
Android support, universal code closure, or whole-game frame rate.

The codecs operate while execution and all writers are paused. Their internal
locks protect storage ownership; they do not create a cross-device atomic
capture boundary. The enclosing case must pause CPU, DMA, VIF, VU, GIF, GS and
presentation writers and bind all component states, input bytes and producer
implementations with hashes. No host pointers or callbacks are serialized.

## Common envelope

| Offset | Bytes | Meaning |
| --- | --- | --- |
| 0 | 8 | Format magic below |
| 8 | 4 | Envelope version, `1` |
| 12 | 4 | Component variant below |
| 16 | 4 | Payload length, excluding the 24-byte envelope |
| 20 | 4 | CRC32 of envelope and payload, skipping bytes 20–23 |
| 24 | variable | Explicit component fields |

All integers are little-endian. Booleans occupy one byte and accept only 0/1.
Enums use their declared fixed-width underlying integer type. Floating point
values preserve their IEEE binary32/binary64 bits, including signed zero and
NaN payloads; no host struct padding is included. Byte blobs use a `u32` length
followed by exactly that many bytes. Arrays contain their elements in order
without another length. CRC32 uses the reflected polynomial `0xEDB88320`,
initial accumulator `0xFFFFFFFF`, and final bitwise complement. CRC detects
accidental corruption; an enclosing evidence manifest provides SHA-256 binding.

The reader verifies magic, version, variant, exact length, size budget and CRC
before decoding. It rejects truncated fields and trailing bytes. Decoding uses
owned temporary storage. A rejected restore preserves the target state; all
allocations and validation precede publication. Target callbacks, pointers and
backend ownership remain installed.

## VIF1 variant 1

Magic: `4e 45 58 4f 56 49 46 00` (`NEXOVIF` plus zero).
Maximum encoded size: 64 MiB including the envelope.

Payload order:

1. Twenty-three `u32` VIF1 register words: `stat, fbrst, err, mark, cycle, mode,
   num, mask, code, itops, base, ofst, tops, itop, top`, then `row[4], col[4]`.
2. Parser residue: `remainingBytes:u32, directHl:bool, payload:blob,
   pendingCommand:blob`.
3. Transport: `path3Masked:bool, pendingPath2ImageQwc:u32,
   pendingPath2DirectHl:bool`.
4. Masked PATH3 FIFO: packet count `u32`, then one blob per packet in queue order.

An incomplete DIRECT/DIRECTHL transfer is bounded by 65,536 qwords (1 MiB).
Consumed payload plus remaining bytes must fit that budget and total a whole
qword when a DIRECT transfer is active. Pending command residue is at most
4,099 bytes. A parser cannot have an active DIRECT transfer and pending command
residue simultaneously; completed DIRECT payload storage is empty. Pending
PATH2 IMAGE qwords are at most 32,767. Masked FIFO packets are at least 16 bytes.
Packet counts are checked against remaining encoded bytes before allocation.

This checkpoint excludes VIF0, RAM/scratchpad, VU memories/state, CPU status,
DMA registers/queued DMA work, completed DMA causes, MMIO storage, the GIF
arbiter and GS. Those are separate parts of an enclosing causal case. The
parser bridge accesses the existing sidecar keyed by the memory instance,
without changing the memory class layout. A memory reset still discards its
parser sidecar. Resetting an instance after restore discards the restored state.

## GIF arbitration queue variant 0

Magic: `4e 45 58 4f 47 41 52 00` (`NEXOGAR` plus zero).
Maximum encoded size: 64 MiB including the envelope.

Payload: queue count `u32`, then for every packet:
`pathId:u8, path2DirectHl:bool, path3Image:bool, data:blob`.

`pathId` is 1, 2 or 3. Packet data is at least 16 bytes. DIRECTHL metadata is
valid only for PATH2. The PATH3 IMAGE bit must agree with the first tag's format.
Queue order and all metadata survive restore; the receiving target's callback
is preserved. The codec does not alter or certify the runtime's arbitration
algorithm or its relationship to hardware timing. It does not serialize
callbacks, VIF's separately masked PATH3 FIFO, or delivered packet history.

## CPU GS variant 0

Magic: `4e 45 58 4f 47 53 00 00` (`NEXOGS` plus two zeros).
Maximum encoded size: 128 MiB including the envelope.

Source and target must have the identified `GSCpuBackend`, coherent frontend
and backend VRAM pointers, and exactly 4 MiB of initialized local memory.
The target must match whether privileged registers are bound. Other raster
backends and uninitialized instances are rejected.

Payload order:

1. Privileged-register presence `bool`, then twenty `u64` values in the declared
   `GSRegisters` order: `pmode, smode1, smode2, srfsh, synch1, synch2, syncv,
   dispfb1, display1, dispfb2, display2, extbuf, extdata, extwrite, bgcolor, csr,
   vsyncTick, imr, busdir, siglblid`. An unbound register set contains all zeros.
   Atomics are sampled/restored as values, rather than serialized as objects.
2. VRAM byte blob, exactly 4 MiB.
3. Frontend fields, in the `GS_FRONT_FIELDS` order in `lab/src/gs_snapshot.cpp`:
   both contexts; active, PRIM and PRMODE primitive registers; current color,
   Q/ST/UV/fog inputs; global draw registers; transfer registers; all six vertex
   slots and both signed 32-bit counters; display snapshot; preferred display
   source; latched presentation frame/metadata; native upload/packet counters.
4. Backend CLUT: 512 `u16` values and two remembered CBP values as `u32`.
5. Texture page state: page base `u32` and 8,192 cached bytes. Base is either
   `UINT32_MAX` or an aligned page entirely within VRAM.
6. Transfer command fields: BITBLTBUF, TRXPOS, TRXREG, direction `u32`.
7. Transfer progress: `x, y, totalPixels, copiedPixels, direction` as `u32`,
   then stored pending-byte count as explicit `u64` with host-size fit checking.
8. Local-to-host staging blob and read cursor as explicit `u64`.

Public GS aggregates use explicit fields in declaration order, recursively,
as listed by `GS_FIELDS` in the codec. Vertex Z is binary64; X/Y/Q/S/T are
binary32. Primitive registers contain a `u8` type plus eight booleans. Reserved
primitive type 7 is preserved because the current parser can produce it.
The runtime's vertex index accumulates across draws; it is not restricted to
the six-slot queue length. Both counters must be nonnegative.

### Hidden state that must survive

Loaded CLUT colors and remembered CBPs are not reconstructed from VRAM. A game
may reuse the palette source after loading it. Likewise the current backend's
texture page may intentionally hide subsequent VRAM writes until TEXFLUSH.
The page bytes are preserved even if they differ from VRAM; silently clearing
this cache would change the continuation. Partly assembled primitives retain
all vertices, input registers and counters. Local-to-host staged bytes retain
their subpixel byte cursor instead of being regenerated.

The codec preserves presentation buffers so it can compare subsequent host
observations of this model. Diagnostic history rings, log counters, mutexes,
VRAM handlers, host pointers, vtables and backend ownership are excluded.
No GPU state or full global-machine state is represented by this variant.
Frontend, backend and presentation writers must be quiescent before capture.

## Tests and remaining gates

The device suite exercises every byte split in UNPACK, MPG and DIRECTHL example
streams; masked PATH3 retention; queued GIF metadata and drain continuation;
in-flight host-to-local transfers; partial local-to-host reads; loaded CLUT and
half-built sprite continuation; stale texture visibility until TEXFLUSH;
privileged atomics, float payloads and latched presentation buffers. It also
checks checksummed semantic corruption and transactional target rejection.

These synthetic tests establish restoration of this model in these cases.
An original Monster House VIF case, synchronized enclosing capture, complete
external-input closure, independent reference and second-architecture replay
remain required by root README section 31.1. Existing normalized VU captures
cannot be relabeled as original VIF captures.

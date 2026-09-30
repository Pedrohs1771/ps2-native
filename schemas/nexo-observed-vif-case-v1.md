# NEXO observed VIF call, version 1

This laboratory case preserves the literal argument of one completed
`PS2Memory::processVIF1Data` call and the surrounding **current runtime model**.
It is not an independent hardware trace, complete PS2 machine checkpoint,
proof of code closure, or final native game package. Successful replay against
the recording has assurance `tested_only`.

## Acquisition and ownership

In a lab-enabled runtime, set `PS2X_CAPTURE_SCENE` to a private directory and
create `.vif-request` there. The request survives calls without an observed VU
callback. The first successfully observed VU-bearing call writes a new case
directory. `.complete` contains exactly one byte, `01`, and is written last;
only then is the request removed. Failure preserves the request and does not
publish a completed case. A partly written directory is not evidence.

Capture begins before pending VIF command bytes are prepended. Original input
is copied; preexisting pending bytes belong to the input checkpoint. No
synthesized MSCNT command or normalized VU capture substitutes for this input.
Arguments aliasing VU code, VU data or GS VRAM are rejected because replay does
not preserve that mutable alias. Recursive captured VIF calls are marked
incomplete. Non-strict acquisition errors preserve the guest's failure behavior.

The normal synchronous runtime CPU worker owns the call. A per-memory lease
blocks the runtime's host presentation function across before-state, execution
and after-state acquisition. The observer is local to its thread and filters
the memory instance; it does not replace callbacks. **All other writers must
remain quiescent.** This lease does not pause arbitrary custom callbacks,
external threads, DMA producers or an independently operating renderer. The
format has no general external-input or floating-environment closure proof.

## Case files

| File | Contents |
| --- | --- |
| `capture.json` | Schema `nexo.observed.vif.call.v1`, counts, provider, horizon and scope |
| `vif-input.bin` | Literal nonempty input argument; maximum 32 MiB |
| `input-state.nexo` | Composite boundary immediately before parsing |
| `output-state.nexo` | Composite boundary after the completed call |
| `events.nexo` | Ordered VU callback boundaries and GIF submissions/deliveries |
| `bank-N.bin` | Full 16 KiB VU1 code identity before an executed callback |
| `.complete` | Single byte `01`, published last |

Bank indices are contiguous from zero and assigned by first observation.
Identical banks are deduplicated using their complete bytes. Only executed
identities are represented; an uploaded but unexecuted bank need not appear.
At most 256 VU callbacks are recorded. Current replay callbacks have a 65,536
cycle horizon. The finite collection converter verifies file inventory,
unique identities, counts and horizon before invoking its separate inspector.

An enclosing immutable recording manifest must bind these files and the
producer's source, binaries and build configuration with SHA-256. CRC32 checks
internal corruption; it provides no provenance or correctness attestation.

## Composite state

The common 24-byte little-endian envelope and CRC are defined in
`nexo-device-state-v1.md`. Composite magic is `NEXOVCS` followed by zero,
variant 1; maximum size is 272 MiB including the envelope.

Payload order:

1. CPU-visible VU FBRST and VPU status, each `u32`.
2. VU1 code generation, `u64`.
3. VIF1, GIF arbitration, CPU GS and VU1 canonical states, each a length-prefixed
   blob, with bounds 64 MiB, 64 MiB, 128 MiB and 80 KiB respectively.
4. VU1 code and VU1 data blobs, each exactly 16 KiB.

Component decoders retain their own magic, variants and validation. GS includes
VRAM, partial vertices/transfers, loaded CLUT and stale texture-page state,
readback progress, presentation values and privileged registers. VU includes
hidden readiness, flag, branch, Q/P/EFU and pending-write state.

Replay restores a fresh, private, windowless runtime. Component failure destroys
that instance before returning a result; it does not publish a partially
restored caller-owned machine. Code generation is restored exactly. Device
routing and callbacks belong to the newly initialized runtime. The recorded
active CPU's relevant VU fields become the private runtime CPU's fields.

This boundary excludes the complete EE/IOP machines, general RAM/scratchpad,
DMA/scheduler/interrupt continuations, audio, peripherals and GPU state.
Custom callbacks with additional dependencies cannot be qualified by this
boundary merely because a completion marker exists.

## Ordered event trace

Magic is `NEXOVTR` followed by zero, variant 1, maximum 64 MiB. The payload
starts with an event count `u32`. Each event contains, in order:

`kind:u8, absoluteVuCycle:u64, arguments[5]:u32, data:blob`.

| Kind | Arguments | Data |
| --- | --- | --- |
| 1: before VU callback | VIF opcode, PC, TOP, ITOP, bank index | Empty |
| 2: GIF submission | Path ID, drain-immediately, DIRECTHL, zero, zero | Submitted packet bytes |
| 3: GS delivery | Five zeros | Packet actually delivered by the GIF arbiter |

Submission is observed before path masking/arbitration; delivery is observed
at the initialized runtime's GS receiver. Preserving both distinguishes a
queued or masked submission from one processed by GS. Event time is the
absolute VU model clock, not a certified global PS2/device clock. Maximum
event count is 262,144. No host addresses are serialized.

## Finite native collection and reuse

`generate_vif_banks.py` is a conversion tool. It inspects every observed bank
in a batch and emits one C++ translation unit per identity plus a static
registry. The game/replay runtime does not invoke the inspector or compiler.
The native callbacks require an exact full-code identity and reject unknown
identities/entries with `UNSEEN_CODE`; there is no reference fallback.

Metadata cache identity covers code bytes, inspector binary, C++ emitter and
collection converter. Cached metadata has an integrity hash and is validated
again against code words, widths, control bits and descriptor fields. Invalid
cache entries are regenerated. Identical C++ bytes retain their modification
times so existing object files remain reusable. Cache reuse has assurance
`unknown`; it neither broadens closure nor independently approves semantics.

On Linux GNU/LLVM ABI, the native replay links eight fatal wrappers for generic
VU interpreter/numeric entry points. These guards apply to this execution path;
the broader linked archive still contains legacy interpreters, and this is not
a whole-package reachability proof.

CLI stdout is one JSON result; runtime diagnostics go to stderr. Comparison
reports full-state/event equality and the first differing byte per component.
For equally sized state envelopes, derived CRC differences are skipped when
reporting a first payload divergence. Timing covers the VIF call, VU callbacks,
GIF/CPU-GS work and trace recording; it excludes file input, machine construction,
restore, final encoding and comparison. It is not game FPS.

## Optional host profiling

`nexo_vif_replay case [iterations] --profile` and the native CLI add
`host_profile` to their JSON. Default execution leaves profiling disabled and
all profiling counters zero. Profiling does not change this canonical format
or insert wall clocks into the event trace.

Three RAII scopes bracket VU callbacks, GIF submission and actual GS receiver
execution. Each reports call count and inclusive/exclusive wall nanoseconds.
Exclusive time subtracts directly nested measured scopes belonging to the
same observation; inclusive values must not be added together. Observations
and nesting are thread-local and filter the owning memory instance. Scopes
must be nested in stack order and destroyed before their observation owner.
Exception unwinding closes the scope and restores its previous parent.

The GS scope starts after delivery observation and encloses `processGIFPacket`.
The VU scope starts after bank/callback observation. Time spent copying packet
bytes, constructing timing scopes or performing other work outside a child
scope can remain in its parent's exclusive value or the VIF residual. These
are instrumented callback wall measurements, not isolated ISA throughput or
certified hardware timings. Host contention and instrumentation overhead must
be reported. State/events must still match when profiling is enabled.

## Remaining acceptance gates

Root README section 31.1 still requires an identified independent reference
and qualified optimization. M1 also requires replay on a second architecture.
Broader external-input closure, strict final EE/IOP/VU AOT, GPU compatibility,
whole-game progression/audio/controls/saves and physical Android qualification
remain separate gates. One finite observed case cannot approve a whole game
or unseen ISOs.

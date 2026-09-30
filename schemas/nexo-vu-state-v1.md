# NEXO VU state format, version 1

This is a checkpoint of **all persistent execution state in the current
`VU1Interpreter` implementation**, at an instruction-pair boundary. It is not
a claim that this implementation models every hidden state of PS2 hardware.
The producer must pause execution and concurrent writers before serialization.
The surrounding replay must identify the semantic implementation and capture
code, data, VIF, GIF, GS, and external events at the same causal boundary.

## Encoding

No pointer, padding, native `size_t`, C++ object image, or native boolean is
serialized. Integer fields use their stated width, little-endian. `f32-bits`
means a raw 32-bit pattern, including signed zeros, denormals and NaN payloads;
no floating-point conversion is performed. A `bool8` must be exactly 0 or 1.
Arrays are row-major. No alignment bytes occur between fields.

| Offset | Width | Header field |
|---:|---:|---|
| 0 | 8 | ASCII `NEXOVU` followed by two zero bytes |
| 8 | 4 | Format version, `u32`, exactly 1 |
| 12 | 4 | Unit, `u32`: 0 for VU0, 1 for VU1 |
| 16 | 4 | Payload byte length, `u32` |
| 20 | 4 | CRC-32/ISO-HDLC, `u32` |
| 24 | payload length | Fields in the order below |

The CRC covers bytes 0..19 and 24..end, excluding its own field. Polynomial
`0xEDB88320`, initial value `0xFFFFFFFF`, final XOR `0xFFFFFFFF`. CRC detects
accidental corruption; it is neither a signature nor proof of fidelity. Replay
manifests must additionally bind blobs with SHA-256. Readers reject unknown
versions, invalid unit, inconsistent length, trailing fields, invalid booleans,
invalid descriptors, checksum errors and clocks inconsistent with the state.
Valid pending deadlines and resource/readiness clocks cannot exceed the current
clock by more than 64 cycles. The current implementation's longest scalar
latency is 54; other issue/writeback latencies are shorter. This model-specific
bound prevents fabricated deadlines from driving unbounded pipeline flushes.
The clock must also retain room for that horizon without `u64` overflow.
The allocation/input bound is 80 KiB. Decode is transactional: failure cannot
change the destination execution state. A snapshot cannot change its unit.

## Payload order

1. Architectural fields:
   `VF[32][4]:f32-bits`, `VI[16]:i32`, `ACC[4]:f32-bits`,
   `Q,P,I:f32-bits`, `R,PC,MAC,CLIP,STATUS:u32`, `cycles:u64`,
   `ebit,haltAfterDelaySlot,dBitEnabled,tBitEnabled,stoppedByD,stoppedByT:bool8`,
   `TOP,ITOP:u32`, `branchPending:bool8`, `branchTarget,branchDelay:u32`.
2. Eight flag entries, each:
   `readyCycle,issueCycle:u64`, `mac,status,extraSticky,clip:u32`,
   `valid,writesMac,writesStatus,writesSticky,writesClip:bool8`.
3. FDIV entry, then two EFU entries, each:
   `readyCycle:u64`, `value:f32-bits`, `statusDi:u32`, `valid:bool8`.
4. Eight pending stores, each:
   `readyCycle:u64`, `address:u32`, `words[4]:u32`, `laneMask:u8`, `valid:bool8`.
5. Sixteen pending VF writes, each:
   `readyCycle,sequence:u64`, `value[4]:f32-bits`, `reg,laneMask:u8`, `valid:bool8`.
6. Eight pending VI writes, each:
   `readyCycle,sequence:u64`, `value:i32`, `reg:u8`, `valid:bool8`.
7. Eight pending ACC writes, each:
   `readyCycle,sequence:u64`, `value[4]:f32-bits`, `laneMask:u8`, `valid:bool8`.
8. XGKICK:
   `packet[65536]:u8`, `sourceAddress,totalBytes,copiedBytes,currentTagEnd,cycleCredit:u32`,
   `issueCycle:u64`, `active,currentTagEop:bool8`.
9. Readiness:
   `vfReady[32][4],viReady[16],accReady[4]:u64`.
10. Latest writes:
    `vfLatestWrite[32][4],viLatestWrite[16],accLatestWrite[4]:u64`.
11. Scheduler:
    `cycle,nextWriteSequence,efuResourceReady:u64`,
    `workingClip,currentUpperInstruction:u32`, `viBranchBackupValue:i32`,
    `viBranchBackupReg:u8`, `viBranchBackupValid,stopRequested,pendingHaltD,pendingHaltT:bool8`.

Inactive pipeline slots and the whole XGKICK buffer are included. That keeps
checkpoint identity reproducible without relying on padding or implicit reset
rules. Host bindings and derived decode caches are not execution state: restore
clears them, and the next execution binds them to the receiving machine.

## Scope of validation

The current tests compare pause/restore/resume against uninterrupted continuation
of the same runtime, including deferred vector/scalar/integer writes, flags,
branch history, EFU resources and in-flight PATH1 packets. These checks prove
regression properties on those cases. They are not an independent hardware
oracle, a VU AOT implementation, a complete VIF/GIF/GS replay, or approval of
the M1 two-architecture gate. Those remain explicit work in the NEXO migration.

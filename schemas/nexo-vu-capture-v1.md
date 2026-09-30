# NEXO current-runtime VU capture, version 1

Opt-in laboratory builds (`PS2X_BUILD_NEXO_LAB=ON`) consume a local
`PS2X_CAPTURE_SCENE/.vu-request` marker at the next VU1 MSCAL or MSCNT boundary.
Execution must be synchronous with code/data writers. A capture contains:

| File | Meaning |
|---|---|
| `input-state.nexo` | Canonical VU state after call-boundary normalization, before execution |
| `code.bin` | Raw microcode memory at that boundary |
| `input-data.bin` | Raw VU data memory at that boundary |
| `output-state.nexo` | Canonical VU state after the execution budget/end/stop |
| `output-data.bin` | Raw VU data memory after execution |
| `path1-events.nexo` | VU-to-GIF submissions during this execution, in causal order |
| `trace.txt` | Diagnostic call kind, budget, stop reason, and bounded issue history |

`.nexo` VU state files follow `nexo-vu-state-v1.md`. Microcode and data are byte
memories, not native C++ object images. Legacy `input-state.bin`,
`output-state.bin` and `trace-state.bin` remain local-ABI diagnostics and are
**not** portable replay inputs. A surrounding manifest must bind portable
files and the exact producer implementation with SHA-256.

Restore the canonical input, then use continuation semantics for both capture
kinds. The captured MSCAL input already has freshly reset pipelines; resetting
it again repeats normalization unnecessarily. The existing fresh-call path
retains its absolute scheduler clock. The MSCNT input preserves
in-flight writes, flags, scalar units, branch history and XGKICK bytes.

## PATH1 stream

All integer fields are little-endian. Header:

- Bytes 0..7: ASCII `NEXOGIF` followed by a zero byte.
- Bytes 8..11: `u32` version, exactly 1.
- Bytes 12..15: `u32` record count.

Each record is `cycleOffset:u64`, `packetSize:u32`, then exactly `packetSize`
raw bytes. The offset is the submission cycle minus the input snapshot clock.
Sizes must be multiples of 16, from 16 to 65,536. Offsets are nondecreasing;
several submissions can occur at the same cycle. The complete file is bounded
to 64 MiB. An empty stream still contains the 16-byte header. A recording error
prevents a complete portable capture from being published.

This observes **submission** before GIF arbitration. It does not establish
when GS consumes the packet, or snapshot pending GIF paths, VIF commands,
GS transfer state, VRAM or external concurrent writers. Those components
remain required for the complete first increment in README section 31.1.
Current replay comparisons must explicitly identify this narrower VU boundary.

# Observed EE miss and model context, version 1

This laboratory record diagnoses a finite AOT guard rejection. It preserves
the identified EE context model and a RAM copy for **guard replay**. It does
not establish writer quiescence, architectural fetch state, a full machine
checkpoint, independent R5900 fidelity, code closure or a qualified game.

## Capture contract

With the opt-in laboratory build, set `PS2X_EE_MISS_CAPTURE_DIR` on an owned
test process. A missing AOT invocation creates an exclusive `ee-miss-NNNNNN`
directory, with owner-only permissions and a maximum of 16 attempts per process.
Existing records are not overwritten. Failures are reported and contained;
capture does not change guest RAM or registers. Capture is absent unless the
environment variable is explicitly set. An AOT miss retains its stop policy.

The record contains:

- `ee-ram.bin`: 33,554,432 physical RAM bytes, if RAM is available.
- `snapshot.bin`: an aligned window of at most 65,536 bytes around the target,
  copied from that saved RAM. A target within the final 512 bytes of a 64 KiB
  region uses a 32 KiB overlap. The window is cropped at the end of physical RAM.
- `ee-context.bin`: the context model below, when the invocation supplies one.
  The contextless lookup API explicitly records absence.
- `expected-N.bin`: the immutable declared footprint for each finite directory
  candidate at the target, in diagnostic iteration order.
- `request.json`: schema 1, written last after all blob writes complete.

The request identifies processor `EE`, admission `missing`, physical target PC,
source PC, numeric `GuestBranchKind`, operation, module ownership/key, directory
availability, diagnostic lookup status, versions checked, captured components,
window dimensions and each candidate's bank index, dependency start/size,
expected filename, mismatch byte count and first mismatch offset. Zero mismatches
use a null offset. Text fields are capped at 4,096 bytes with explicit truncation.
Directory candidates are bounded by 512 banks. Bank indices are local to the
identified catalog, not permanent global identifiers.

`complete=true` means the listed record was written. Both
`quiescence_qualified=false` and `complete_machine_checkpoint=false` remain
explicit. The diagnostic comparison uses the saved RAM copy, not a second live
read; the original admission and this later copy can differ if writers run.
EE context copying is not synchronized with all other processors and devices.
No claim of an atomic world checkpoint or resumable whole-game state is made.

## Context model envelope

| Absolute offset | Bytes | Meaning |
|---:|---:|---|
| 0 | 8 | Magic `NEXOEE` followed by two zero bytes |
| 8 | 4 | Format version, u32, exactly 1 |
| 12 | 4 | Identified model profile, u32, exactly 1 |
| 16 | 4 | Payload length, u32, exactly 1,619 |
| 20 | 4 | IEEE CRC32 of bytes 0–19 concatenated with bytes 24–end |
| 24 | 1,619 | Ordered model fields below |

The envelope is exactly **1,643 bytes**. All integers use little-endian fixed
widths. Float fields preserve their binary32 bits without numeric conversion.
SIMD values contain four u32 bit lanes in lane 0 through lane 3 order. Arrays
use increasing indices. The delay boolean is one byte, exactly 0 or 1. There
are no ABI padding bytes or host pointers. Unknown headers, checksums, length,
booleans, truncation and trailing bytes are rejected by the typed codec.

The payload order is:

1. `r[32]`, four u32 lanes each.
2. `pc` u32; `insn_count`, `hi`, `lo`, `hi1`, `lo1` u64; `sa` u32.
3. `vu0_vf[32]`, four bit lanes each; `vi[16]` u16.
4. `vu0_q`, `vu0_p`, `vu0_i` binary32; `vu0_r`, `vu0_acc`, four bit lanes each.
5. `vu0_status` u16; `vu0_mac_flags`, `vu0_clip_flags`, `vu0_clip_flags2` u32.
6. `vu0_cmsar0`, `vu0_cmsar1`, `vu0_cmsar2`, `vu0_cmsar3` u32.
7. `vu0_vpu_stat`, `vu0_vpu_stat2`, `vu0_vpu_stat3`, `vu0_vpu_stat4` u32.
8. `vu0_tpc`, `vu0_tpc2`, `vu0_fbrst`, `vu0_fbrst2`, `vu0_fbrst3`,
   `vu0_fbrst4`, `vu0_itop`, `vu0_top`, `vu0_info`, `vu0_xitop`, `vu0_pc` u32.
9. `vu0_cf[4]` binary32.
10. `cop0_index`, `cop0_random`, `cop0_entrylo0`, `cop0_entrylo1`, `cop0_context`,
    `cop0_pagemask`, `cop0_wired`, `cop0_badvaddr`, `cop0_count`, `cop0_entryhi`,
    `cop0_compare`, `cop0_status`, `cop0_cause`, `cop0_epc`, `cop0_prid`,
    `cop0_config`, `cop0_badpaddr`, `cop0_debug`, `cop0_perf`, `cop0_taglo`,
    `cop0_taghi`, `cop0_errorepc` u32.
11. `llbit`, `lladdr` u32; `in_delay_slot` bool8; `branch_pc` u32.
12. `cop2_ccr[32]` u32; `f[32]`, `f_acc` binary32; `fcr31` u32.

The absolute PC offset is 536; the delay boolean is at 1,374. This model includes
the declared VU0 macro fields, not complete VU pipeline state. TLB/cache state,
kernel continuation, scheduler, IOP, DMA, devices and event queues are outside
this context envelope. Portability of these bytes is tested on the current
x86-64 host only; a second architecture remains unqualified.

## Offline preparation and extension

```sh
python lab/prepare_ee_miss.py /path/to/ee-miss-000001 \
  --generator build/ps2xRecomp/ps2_native_overlay --output fresh-case
python lab/generate_ee_bank_catalog.py old-case-1 old-case-2 fresh-case \
  --generator build/ps2xRecomp/ps2_native_overlay --output existing-catalog --extend
```

Preparation admits only complete physical RAM byte cases with `MissingEntry` or
`CodeChanged`, without loaded-module ownership. It verifies window equality
against saved RAM. A supplied model context must pass the envelope checks,
have PC equal to the target and be outside a pending delay slot. Unsupported
entry contracts are rejected, not silently reset. Missing context is allowed
for byte generation and stays explicitly unqualified.

The generator executes offline against a private copy; preparation checks
aligned dependency ranges, unique entries, the requested root and a stable
producer executable hash. Output is fresh `snapshot.bin` and `bank.json`, with
RAM, context, request, image, generator and preparer hashes. These identify the
observed producers and bytes; they do not authenticate capture origin or prove
semantics. Catalog regeneration checks a recorded generator hash when supplied.
New prepared cases explicitly record `dependency_contract=normal-entry-v1`.
Old untagged cases retain `whole-block-v0` metadata; catalog generation verifies
them against the original descriptors before applying normal refinements.

Extension requires all previous cases plus the new case, validates old source
hashes and generator identity, and refuses removal or rewriting of prior banks.
Unchanged source files retain their bytes and timestamps. Only new bank sources,
the changed index and the manifest are replaced; the manifest is published last.
Use an exclusively owned offline directory without concurrent producers/builds.
Interruption can leave an incomplete update: reuse requires fresh hash validation,
and conflicting unrecorded artifacts require a fresh catalog. This is an
incremental build aid, not a qualified semantic cache or publication protocol.

An explicit `--extend --migrate-entry-guards` can refine the index while retaining
every prior bank source exactly, including when changing the producer binary.
It verifies all old identities and refuses any old callback source rewrite.
The bounded hashed dependency sidecar and exact entry restrictions are described
in [`nexo-ee-entry-dependencies-v1.md`](nexo-ee-entry-dependencies-v1.md).

`admit_ee_misses.py` now prepares bounded batches and reuses verified input copies
owned by the catalog. Its retained producer ledger allows later ordinary
extensions after a generator migration, only for already verified bank sources.
The command, duplicate handling and failure-report scope are in
[`nexo-ee-miss-batch-v1.md`](nexo-ee-miss-batch-v1.md).

The game runtime never invokes this preparation, generator or compiler. Finite
bank admission still compares each callback's complete **declared** footprint.
Requested entry generation can yield a different block boundary; it does not
erase mismatch evidence or disable guards. Automatic family synthesis, complete
entry-context contracts and unseen-path closure remain future obligations.

## Checks

The C++ codec tests mutate all 409 declared model cells/lanes independently,
compare original/restored member bits and reject corrupt encodings. An independent
declaration inventory test rejects model changes without test inventory updates.
Capture fixtures compare all guest RAM and canonical context before/after;
they cover missing/changed/module-owned cases, absent RAM/context, disabled
capture, exclusive records, event budget and contained write failure.
Preparation tests cover RAM/window mismatch, corrupt context, PC/delay contracts,
existing outputs and consumption by the catalog generator. Catalog tests reject
producer mismatch and prove extension preserves prior source bytes/timestamps.
These checks establish the stated laboratory behavior, not hardware fidelity.

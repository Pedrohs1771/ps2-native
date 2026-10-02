# EE data-family candidates, laboratory v1

`lab/discover_ee_data_families.py` groups bounded observed dependency regions
during conversion. It writes an offline proposal, not a runtime program, compiled
bank, certificate or execution authorization. Every report retains
`strict_approval`, `closure_proved`, `producer_invariant_proved` and
`native_execution_validated` as false.

## Input and command

Use hash-checked copies already owned by a catalog:

```sh
python lab/discover_ee_data_families.py \
  --catalog /path/to/catalog \
  --output /path/to/fresh-candidates.json
```

Alternatively, repeat `--case /path/to/prepared-case`. Each case contains
`snapshot.bin` and `bank.json` as specified by the EE extraction/miss preparation
tools. Input schema, exact snapshot hash/size, aligned physical RAM bounds,
strictly sorted unique bindings and requested-entry coverage are validated.
Bindings are byte dependencies reported by those tools; this detector does not
independently prove they are complete instruction/fetch footprints or reached
code. A source region is not necessarily a whole function or a basic block.

Paths including parents must not be symlinks. Output must be fresh; repeated
submissions do not replace an existing report. The catalog and game executable
are never modified. Catalog mode validates the owned input ledger before analysis.

## Restricted discovery

The grouping key is the complete region's normalized bytes, not just a digest.
The original classifier had two operand types. Current reports declare
`data_operand_profile: 2`, which additionally supports the ordinary fields below.
Register selection and control instructions are never normalized.

| Kind | Fixed fields | Candidate field | Reported value |
|---|---|---|---|
| `lui-u16` | LUI opcode, zero reserved source register, destination register | Low 16 bits | Unsigned integer 0..65535 |
| `sw-s16` | SW opcode, source/base and value registers | Low 16 bits | Signed integer -32768..32767 |
| `addiu-s16`, `slti-s16`, `sltiu-s16` | Exact opcode and registers | Low 16 bits | Signed integer |
| `andi-u16`, `ori-u16`, `xori-u16` | Exact opcode and registers | Low 16 bits | Unsigned integer |
| `lb/lh/lw/lbu/lhu/lwu/ld/lq-s16` | Exact load opcode and registers | Low 16 bits | Signed displacement |
| `sb/sh/sd/sq-s16` | Exact store opcode and registers | Low 16 bits | Signed displacement |

All other bits, including branches and jump targets, remain exact. Changes in
opcode, register selection, region length or fixed control encoding split groups.
By default, only fields that actually differ within a group become proposed parameters.
Eligible immediates that remain constant retain a full `0xffffffff` guard mask.
An identical region at several addresses alone does not become a data-family
candidate. Repeated copies at the same address with the same bytes are deduplicated
while retaining all source identities.

The explicit batch options `--minimum-variants 1 --operand-policy typed` also
propose singleton structures and parameterize every eligible data immediate.
Opcode/register/control fields remain fixed. These broader domains are proposals
without producer or independent fidelity approval. `--root-only` retains only
bindings at their dependency start; `--terminal-only` retains regions with a
syntactically complete terminal transfer for subsequent frontend validation.
Skipped bindings and nonterminal regions are counted. The latter restriction
can omit executable linear continuations and is not a closure algorithm.

This is a syntactic partition, not an assertion that an immediate is causally
unrelated to control flow. A parameter may feed an indirect destination or alter
observable memory behavior. Those obligations remain open. Direct jump semantics,
relative-PC expressions, exception/call locations, guest timing, entry/delay
context, writes during execution and aliases also require separate validation.

## Report

Top-level `schema_version` is 1, `status` is `CANDIDATES_LABORATORY`, and `counts`
describe submitted cases, bindings, unique per-case regions, scanned words and
oversized skipped regions. `analyzer_sha256` identifies the exact tool source
used for the report, without certifying its semantics. `families` is sorted by
`shape_sha256`.
`discovery_policy` records the minimum distinct byte variants, operand policy
and root/terminal filters. Retained older reports omit that field.

Each candidate contains:

- `word_count`: length of the proposed structure, in little-endian 32-bit words.
- `guard_words`: exact words with candidate parameter bits zeroed.
- `guard_masks`: `0xffff0000` for a proposed supported immediate; otherwise
  `0xffffffff`. No arbitrary masks are accepted from input.
- `parameters`: ordered `word_index`, `kind`, and `bits: 16` records.
- `normal_entry_offsets`: the union of actual declared binding offsets for the
  observed regions; it is not inferred from every instruction. Each observation
  also retains its own offsets. These are proposals, not entry-ownership proofs.
- `observations`: physical `pc`, exact `word_sha256`, ordered extracted
  `parameters`, and sorted `origins`. Each origin gives exact image and metadata
  SHA-256 identities and the image base.
- `shape_sha256`: hash of the versioned classifier tag followed by little-endian
  guard words and guard masks. It identifies a candidate, never a semantic proof.
  Profile 2 uses `ee-data-shape-typed-v2`; retained profile-1 artifacts keep their
  older classifier tag and exact analyzer identity.

For an observed region, replacing each parameter word's low 16 bits with
`parameter & 0xffff` reconstructs its exact bytes. This round trip is a data
extraction check. It does not validate a native kernel or unobserved parameters.
Reports contain observed values, not an approved domain or generalized bound.
The analyzer has no instruction execution, guest decoder at runtime, compiler
invocation, external model request or paid service use.

## Bounds and next stage

Limits: 512 cases; 64 KiB per image; 8 MiB per metadata document; 64 MiB aggregate
metadata; 32,768 bindings per case and 2,097,152 aggregate; 131,072 dependency
regions; 4,194,304 scanned words. Regions longer than 128 words are explicitly
skipped, counted and left unqualified. Exceeding other aggregate limits aborts
publication. These are laboratory search budgets, not coverage limits that can
be silently waived for strict approval.
At most 32,768 candidate families and 64 MiB of serialized report are permitted.
Serialization is charged before creating the publication path.

Next steps are producer/write slicing, restricted native synthesis using the
existing EE semantic emitters, context/relative-PC guards, differential checks
and independent fidelity, then establishing admissible parameter domains and
reachable-caller coverage. Candidate discovery alone does not prevent a game
stop and does not satisfy M9 or any complete-game gate.

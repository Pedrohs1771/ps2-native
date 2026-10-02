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
metadata; 32,768 bindings per case and 2,097,152 aggregate; 262,144 dependency
regions; 8,388,608 scanned words. Regions longer than 128 words are explicitly
skipped, counted and left unqualified. Exceeding other aggregate limits aborts
publication. These are laboratory search budgets, not coverage limits that can
be silently waived for strict approval.
At most 65,536 candidate proposals and 64 MiB of serialized summary are permitted.
The native catalog independently admits at most 32,768 families after synthesis.
Serialization is charged before creating the publication path.

Next steps are producer/write slicing, restricted native synthesis using the
existing EE semantic emitters, context/relative-PC guards, differential checks
and independent fidelity, then establishing admissible parameter domains and
reachable-caller coverage. Candidate discovery alone does not prevent a game
stop and does not satisfy M9 or any complete-game gate.

## Compact publication v2 and canonical regions

`--region-policy canonical-v1 --root-only --operand-policy typed` selects the
current canonical proposal policy. A root extends through the first transfer
and its complete architectural slot (at most 128 words), or through 127 linear
words when no transfer occurs in that interval. Truncated windows are rejected
and counted. This can cross a prior metadata boundary within the owned snapshot.
Every candidate and observation exposes only normal offset zero. This policy
cannot combine the terminal-only filter or observed-only operand policy.

Canonical discovery also queues known normal successors inside the same owned
snapshot: both conditional destinations, direct jump targets, return
continuations of linking calls and the next 127-word linear region. The
requested entry is seeded even when its metadata dependency starts earlier.
Each root is visited once, so backedges terminate. Indirect destinations are
not invented. `canonical_successor_roots` counts added roots and
`canonical_external_successors` counts edges outside the window; external code
still requires an admitted capture. These are syntactic discovery counts,
not executed paths or proof of reachability/closure.

Canonical output uses integer `schema_version: 2`. It preserves the laboratory
status, false approvals, policy, classifier identity, guard words/masks and shape
identity. Family summaries replace repeated `parameters` and `observations`
with `observation_count`. Current compact summaries omit redundant `word_count`;
the reader derives it from the validated, equally sized guard arrays and restores
the decoded field. Retained summaries may declare it explicitly, but it must
match those dimensions. An empty canonical proposal set fails before writing
the summary or any provenance shard.

`provenance.origins` is the sorted deduplicated table of image hash, metadata hash
and base. `provenance.shards` names content-addressed
`ee-candidate-provenance-SHA256.json` files with exact digest, byte size and record
count. `total_bytes` and `record_count` bind the aggregate. Each shard carries
schema 1, status `OBSERVED_LABORATORY`, false strict approval and records retaining
PC, word hash, parameter values, offset-zero entry, family index and origin IDs.

Each shard is at most 4 MiB; there are at most 64 shards and 128 MiB aggregate
details. Counts, numeric bounds, names, hashes, sizes and references are checked
before conversion. The 64 MiB summary is published last after all referenced
shards. Detail reuse requires exact bytes. Publication does not overwrite an
existing summary and does not claim crash durability without storage sync.

`read_report` accepts v1/v2 and can reconstruct v1-style observations for fixture
generation. An optional expected SHA-256 rejects a changed summary before
parsing; the catalog publisher uses it to keep admitted input and manifest
identity consistent. None of these hashes approve native semantics, reachable
coverage, producer/fetch closure or gameplay.

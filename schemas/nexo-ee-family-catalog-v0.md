# Finite precompiled EE family admission, laboratory v0

This opt-in runtime experiment implements the catalog and admission step after
the [restricted synthesis frontend](nexo-ee-native-data-family-v0.md). A match
authorizes a laboratory callback under the stated RAM/context model. It does
not authorize a strict native package or claim a producer invariant, code
closure, instruction-cache equivalence, independent PS2 fidelity or campaign
completion. The delivered product requirements in the root README still apply.

## Offline publication

```sh
python lab/generate_ee_family_catalog.py \
  --candidates candidates.json --generator build/ps2xRecomp/ps2_native_data_family \
  --output fresh-catalog [--shape SHAPE_SHA256] [--case prepared-ee-case]
```

The publisher accepts 1..32768 bounded candidates (a report up to 64 MiB) and up to 16 prepared
root cases. The candidate report must have integer schema 1, laboratory status
and Boolean false approval. Words/masks must be numeric uint32 values, with
1..128 words per structure. Normal entries are aligned byte offsets in the
complete guard. Current proposals declare their observed entry offsets; retained
older reports keep the all-resume-label convention. Prepared
cases expose only their captured requested entry. The synthesis frontend still
rejects unsupported control, operations, masks and reserved fields.

Identical normalized word/mask structures merge their entry offsets. The
publisher derives its own filename digest from these numeric inputs; proposed
shape identifiers cannot supply source expressions or paths. Each generated
body has a separate namespace, descriptor and immutable words/masks/entry
arrays. A separate index exports `compiledEeFamilyProgram()`.

Manifest schema 2 groups bodies into source units, with at most 32 families and
1 MiB per body unit; the separate descriptor index has an 8 MiB bound. The
manifest has a 16 MiB publication bound. Stable hash buckets constrain insertion changes to their bucket.
`family_count`, `source_count` and the numerical `families` ledger describe the
result. CMake retains schema-1 support for older one-family-per-file catalogs.
Verified copies use stable content-addressed paths under the build's
`ee-family-source-cache`, so fresh job directories do not invalidate unchanged
objects. Header/compiler dependencies still apply; corrupted cache bytes fail.
Sources are checked again before publication and the cached copy is hashed
after copying, so a changed source cannot silently establish a new cache identity.
CMake extracts source-name/hash indexes once, avoiding a full family-ledger JSON
parse for every source query. `--workers 1..16` bounds concurrent converter
processes; ordered results preserve source bytes across worker counts. Buckets
split on both their family-count and byte budgets.

`--entry-policy root-only` limits proposals to offset-zero bindings, and
`--terminal-only` declines regions without a complete terminal transfer. These
are explicit laboratory coverage limits, not closure proofs. All normal labels
remain supported by the generated body.

`--root-data-parameters` proposes eligible data fields in a prepared root even
when seen only once. It keeps opcode/register/branch bits fixed and records this
policy in provenance. A captured data address is then a live typed operand.
The broader parameter domains still need producer, alias/fetch and independent
fidelity evidence before release approval.

The manifest records all generated source hashes, the converter before/after
identity, publisher identity, input report hash, selected shapes and exact
prepared-case metadata/image hashes and bounded dependency locations. Declined
structures are recorded. The output directory must be fresh. `catalog.json` is
written after the source files; interrupted publication without that manifest
is not a complete catalog. Publication assumes one owner of these paths.

`strict_approval` and `closure_proved` are Boolean false. These fields must not
be replaced with a success claim because a source file was emitted. Source
hashes bind build inputs; they are not proofs of guest behavior or a sandbox
for arbitrary native source supplied outside this trusted offline generator.

## Runtime descriptors and lookup

```cpp
using Function = void (*)(uint8_t*, R5900Context*, PS2Runtime*, uint32_t base);
struct Family {
    Function function;
    std::span<const uint32_t> words, masks, normalOffsets;
};
struct Program { std::span<const Family> families; };
```

The dispatcher validates the budgets, callback, sizes, masks and unique entry
offsets, then owns copies of the identity arrays and its entry index. Duplicate
complete structures are rejected rather than allowing competing callbacks.
Only full-word masks or low-16 data masks on the 20 supported operand classes are accepted. The
frontend independently checks support for the complete native body.

Lookup reads the word at an aligned physical PC and uses exact/upper-16 indexes
to find potential structure/entry-offset pairs. It computes `base=pc-offset`,
checks the complete bounded region, then compares every guarded word. The
callback and base are returned only for one matching pair. Two matching pairs,
including overlapping regions in the same family, produce `Ambiguous` and no
callback. There is no priority heuristic. No opcode is executed by this lookup.

`admitNormalEntry` also rejects null or pending architectural delay contexts.
The backend resolves existing concrete AOT entries first, then families. Its
returned adapter repeats admission using the **actual invocation PC, context
and current RAM**, rather than retaining a stale query result. The generated
wrapper again checks its complete guard and extracts current data parameters
before running its fixed compiled body. Rejection never requests a runtime
guest compiler or an EE interpretation fallback in this profile.

This is a serial RAM admission model. Whole-region checks do not establish
instruction-cache fetch identity, writer quiescence, DMA/concurrent safety,
self-modification inside an executing body, alias equivalence or kernel/device
timing. Masked immediates can influence memory and future code; they are not
automatically proven data-only domains. Unknown or ambiguous code still stops.

## Build boundary and evidence

`PS2X_RUNTIME_EE_DATA_FAMILIES` defaults OFF. Enabling it requires both the AOT
EE backend and the NEXO laboratory, with `NEXO_EE_FAMILY_MANIFEST` pointing to a
published catalog. CMake checks field types, counts, laboratory flags, source
names, hashes, sizes and ordinary paths. The matcher/backend and generated
family objects are separate build targets. Their debug profile uses no LTO;
changing this catalog does not regenerate the original game's sources.

Tests cover copied identities, relocations, live data, interior entries, stale
queries, changed invocation PCs, pending architectural slots, ambiguity,
descriptor errors, publication provenance and build-time rejection. Callback
mocks in admission tests record selection only. The existing generated-body
comparisons are conservative model regressions with shared semantic emitters.

The initial Monster House experiment published two structures: an observed
eight-word suffix with two typed parameters and an eleven-word fixed prefix
ending at its indirect call. Publication took 0.093 seconds; the incremental
native build took 13.861 seconds and the game link 16.428 seconds, retaining all
original game objects. The headless New Game route passed the previous
`0x184f448` miss and traced its `0x184f474` continuation, then stopped at an
uncovered callback `0x184f498`. No new address-specific TOML or bank was added.

This proves limited execution progress under the current laboratory runtime.
Inputs were supplied by the agent, not an autonomous campaign validator. VU
remains diagnostic, and game fidelity, full gameplay, Android and sustained
60 FPS are unqualified. The next discovery work must cover whole loaded code
structures and unsupported control flow in batches, with model regressions,
producer-domain evidence and independent validation before release approval.

The later batch experiment admitted 1,622 structures in 69 stable source units.
Typed root operands covered the relocated callback and its continuation; the
owned New Game route then stopped at another module entry, `0x1a51b70`.
Root-only/terminal-only policy, parameter domains, module coverage and whole-game
qualification remain open. See the dated batch evidence in `lab/README.md`.

`lab/prepare_ee_family_batch.py` owns and deduplicates previous cases, prepares
fresh captures, proposes typed singleton structures and publishes this catalog
in one offline command. `--previous-batch` verifies the previous receipt,
manifest and owned case hashes before reusing them. Failed jobs retain a false
approval diagnostic receipt. The tool does not launch games or certify closure.

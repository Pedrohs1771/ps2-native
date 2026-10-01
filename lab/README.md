# NEXO laboratory: canonical device state and conservative native V0

This implements a **partial preparation** for the first increment in the root
README, section 31.1. It preserves the current VU runtime as a regression
baseline and implements a laboratory AOT V0 backend for finite microcode banks.
An original VIF-call replay now includes VU, GIF and CPU GS state. An independent
reference with a qualified full state relation, broader external-input closure,
second-architecture validation and a qualified native game package remain
unfinished. The identified independent VU engine and its timing differences are
described in `PCSX2_VU_REFERENCE.md`.

## Finite offline EE overlay catalog

`extract_ee_overlay.py` recovers source snapshots and binding identities from
identified diagnostic ELF artifacts without loading them. `generate_ee_bank_catalog.py`
regenerates each bank offline, checks every recovered binding, isolates native
symbols by bank namespace and emits a hashed static catalog. The opt-in
`PS2X_RUNTIME_AOT_EE_OVERLAYS=ON` backend uses sparse physical-PC pages and complete
declared instruction-footprint guards. It retains multiple precompiled versions
and stops on uncovered code, including branches under diagnostic skip policies.
The diagnostic driver implementation is omitted from this backend. The complete
V0 contract and commands are in [`nexo-ee-aot-v0.md`](../schemas/nexo-ee-aot-v0.md).

The original Monster House observation yielded **32 banks, 508,176 bindings**.
Resuming validated recovery after replacing a quadratic symbol search took
1.778 seconds. Initial C++ generation took 4.458 seconds. Regenerated bindings
agreed exactly with the recovered tables; these measurements cover that observed
corpus only. All snapshots/generated commercial sources remain local and ignored.

The first native catalog/runtime/test build took 167.928 seconds with four workers
and no LTO. The EE catalog and its test built successfully; the overall command
then failed on an incorrectly named additional IOP target. The corrected
incremental build took 11.515 seconds. Updating only the catalog index and manifest
took 10.117 seconds and retained **all 32 bank objects' bytes and timestamps**.
Relinking the actual game against EE and IOP catalogs took 12.757 seconds, using
the existing original game objects with zero original EE game recompilations.

The isolated Xvfb run had the EE diagnostic driver explicitly removed from its
environment. The executable contains the AOT dispatcher/catalog and lacks the
identified diagnostic EE resolver/release and generic IOP execution symbols.
It displayed the Sony logo and autosave confirmation, then remained on a dark
loading screen with a memory-card notice. Captures recorded 14 module loads and
4,096 RPC requests. At the last captured checkpoint: 29,724,395 native IOP
instructions, zero interpreted instructions and no captured native fault.
The RPC capture limit was reached; these are subset observations, not total
execution counters or service-fidelity evidence. VU remained diagnostic.

The run automatically stopped with `EE:UNSEEN_CODE` at `0x1193880`, called from
`0x119a458`. That PC already exists in the recovered catalog. Its first four
logged instruction words agree with the old snapshot; the full declared
40-byte dependency and entry/module context were not captured at the miss.
The precise rejection reason is therefore unqualified. Adding the same address
manually would not establish coverage. The next step is automatic miss-state
capture and comparison, then version/family admission from exact observed bytes.
No gameplay, save correctness, 60 FPS, Android or full campaign gate passed.

The restored diagnostic build passed 65 CTest groups and 485 general runtime
cases. The native EE profile passed its three C++ cases and four selected CTest
groups before restoration; Python coverage includes five extractor cases,
six catalog cases and six headless isolation cases. The current diagnostic IOP
profile passed eight groups and the current native catalog profile passed four.
Restoration retained 51 of 52 common runtime objects' bytes/timestamps; the
runtime source rebuilt after its included overlay header was edited. This is
not an unrelated whole-runtime rebuild from the backend selection flag.

Evidence is under the ignored directory in `build/lab/latest-ee-aot-bank-job.txt`:
recovered banks, manifests, command/timing records, source/binary copies, symbol
audit, RPC observations, screenshots, unseen classification and regression logs.
The root README's M3/publication/context/canonical-checkpoint requirements remain
open. Finite observed coverage does not prove closure or independent fidelity.

### Automatic EE guard diagnosis and incremental admission

The laboratory now captures an AOT miss's RAM, optional canonical EE model
context, module ownership and every declared candidate footprint. The copied
RAM supports reproducible guard comparison; writer quiescence and a complete
machine checkpoint remain unqualified. `prepare_ee_miss.py` converts an admitted
byte capture into a fresh offline bank case. Catalog `--extend` validates prior
identities and preserves unchanged sources and their timestamps. The minimal
`ps2_ee_aot_bank.h` isolates bank declarations from dispatcher/diagnostic changes.
The format, exact entry restrictions and commands are in
[`nexo-ee-miss-v1.md`](../schemas/nexo-ee-miss-v1.md).

In the new isolated Monster House run, the previous `0x1193880` miss was captured
as `CodeChanged`, with no loaded-module ownership. The 40-byte candidate begins
eight bytes before the observed entry. Only those preceding words changed:
`0` became `0x857610` and `0x8578d0`; all 32 bytes from the entry agreed.
Preparation used the observed target without a TOML address edit and emitted
16,381 bindings. Its root dependency begins at `0x1193880` and spans 32 bytes.
This refines the generated boundary while retaining complete declared guards;
it does not qualify general interior-entry semantics or hardware fetch state.

The catalog grew from 32 to 33 banks. Extension generation took **4.392 seconds**,
the new-bank/index build **17.217 seconds**, and actual game relink **10.707
seconds**. All **32 previous active bank objects retained their bytes and
timestamps**, with zero original EE game compilations. The earlier header
isolation migration required a one-time 158.240-second build of the 32 banks;
the 5.429-second capture-fixture rebuild retained all cached bank objects.
These are current-host laboratory measurements, not an ISO conversion SLA.

Tests cover 409 model cell/lane mutations, fixed-width context bit preservation,
malformed encodings, model inventory drift, whole RAM/context nonmutation,
capture limits/failure paths, offline entry contracts, prepared-case consumption,
generator identity and extension preservation. The second game run reached
another miss at `0x1142ce0`, called from `0x114302c`. Its 44-byte dependency
begins four bytes before the entry; only that preceding word changed from zero
to `0x857150`. The 40 bytes from this new entry matched. Both runs exited under
the runtime's normal stop policy, without killing another process.

An isolated guard probe linked against the preserved 33-bank artifacts admits
`0x1193880` against both saved RAM copies and rejects `0x1142ce0` against the
second copy, reproducing the diagnosis without executing a guest callback.
The second capture is prepared as a regression case, **not compiled into another
address-specific workaround**. The repeated prefix-only mismatch motivates a
general offline entry/dependency-boundary correction, with tests for normal,
interior and delay-slot entries before any guard is refined. Adding addresses
individually would not solve that generator issue. Full entry-context/fetch
qualification remains separate from this correction.

The restored regression profile passed **69 CTest groups and 485 general cases**.
Restoration took 14.774 seconds and retained all 54 common runtime object bytes
and timestamps. Gameplay, save correctness, 60 FPS, Android, VU runtime migration,
complete checkpoint replay and universal closure remain open.
Evidence is in the ignored directory recorded by `build/lab/latest-ee-miss-job.txt`.

### Batch correction of normal-entry dependencies

The repeated prefix mismatch above now has a generic offline correction. Normal
entries guard their suffix plus any earlier local static target reachable within
the callback. Standalone terminal slot entries guard their own instruction; the
runtime adapter rejects pending architectural delay contexts and rechecks bytes
at invocation. Raw RAM diagnosis remains a query, not context admission. The
diagnostic DSO driver explicitly retains legacy whole-block guards.
See [`nexo-ee-entry-dependencies-v1.md`](../schemas/nexo-ee-entry-dependencies-v1.md)
for the sidecar, producer checks and precise unresolved obligations.

Migrating **33 banks / 524,557 entries** took **9.135 seconds**, producing **58,945
dependency runs** without changing any bank C++ source. The index/runtime/test
build took **9.547 seconds** and retained **all 33 bank object hashes and
timestamps**. Actual game relink took **10.969 seconds** with no LTO and zero
original game compilations. Both saved prefix-only failures passed guard replay;
the second address-specific regression bank was not added to the catalog.

The isolated actual game reached its title, main menu and new-game file menu.
Attempting to start the selected game then stopped at `0x1724d70`, called from
`0x1baae4`, with `MissingEntry` and zero candidates. RAM, model context and the
64 KiB window were captured automatically. This is a newly uncovered region,
not either previous prefix-only rejection. Existing memory-card files were backed
up before confirming the test. No complete gameplay qualification is claimed.
Synthetic adapter execution verifies local loops, stale-code rejection after
resolution, skipped prefixes, standalone slots and preservation of guest RAM and
context on rejection. Migration tests reject callback source rewrites and retain
prior timestamps. Evidence is under `build/lab/latest-ee-entry-job.txt`.
The restored diagnostic profile passed **69 CTest groups and 487 general cases**;
the catalog group includes **14 Python cases**. The native EE profile passed its
four C++ cases. Profile restoration took **13.773 seconds**, preserving all 54
common runtime objects. The five preexisting test memory-card files were unchanged.
Full entry/fetch semantics, native VU in the game, independent fidelity, campaign,
saves, Android, universal coverage and sustained 60 FPS remain open.

### Automatic offline EE miss batches and owned inputs

`admit_ee_misses.py` consumes up to 16 captured records, prepares every case before
changing the catalog, deduplicates bank identities and calls the verified offline
publisher. After one bootstrap, hashed input copies under `ee_cases/` let the next
batch reuse all old cases automatically. A per-bank producer ledger preserves
prepared-case provenance across generator migrations. It cannot authorize an
unseen foreign case or any rewrite of a previously compiled bank source.
See [`nexo-ee-miss-batch-v1.md`](../schemas/nexo-ee-miss-batch-v1.md) for the command
and the distinction between prepared candidates and published banks.

The captured Monster House region at `0x1724d70` contributed **12,811 entries**
from its 64 KiB image. Batch preparation/extension took **11.546 seconds**, growing
the catalog to **34 banks / 537,368 entries**. The new-bank/index/runtime build
took **14.885 seconds**, preserving all **33 prior bank objects' hashes and
timestamps**; game relink took **12.934 seconds**, with no LTO and no original EE
game compilations. Saved-RAM guard replay now admits that region.

A repeat submission with no external case paths took **11.308 seconds**, detected
the duplicate and retained 34 banks. This still regenerates checked source
descriptors offline; it does not compile another bank or claim semantic cache
correctness. Synthetic tests cover atomic rejection of invalid preparations,
producer lineage, input ownership/hash/path/symlink checks, immutable timestamps,
duplicate handling, bootstrap restrictions, output conflicts and terminal reports.

This implements one laboratory preparation step of autonomy. It does not repair
arbitrary semantics, prove closure or produce a zero-touch campaign route. Game
input in the current experiment is agent-controlled and recorded. Strict approval
remains false; complete game/VU/Android/fidelity/performance gates remain open.
Evidence is under `build/lab/latest-ee-batch-job.txt`.

The 34-bank actual run stopped at `0x184f448`, again called from `0x1baae4`,
with no candidate. Both RAM copies still contain the earlier `0x1724d70` routine.
In the new RAM, three 19-word routines have the same normalized structure; only
two 16-bit immediate fields of a LUI/store pair differ, encoding distinct data
addresses. This suggests a shared initializer shape across code copies/modules.
It does not identify their materializer or prove a relocation family. The earlier
bank's saved-RAM guard success does not prove that this run executed it. No second
address-specific bank was added for the new target. Producer tracing and guarded
native family synthesis are the next investigation, alongside the remaining
entry/fetch/fidelity obligations; gameplay remains unqualified.

The restored diagnostic profile passed **70 CTest groups and 487 general cases**.
Catalog tests include **17 Python cases**, and the new batch command has **7**.
The native EE profile passed its four C++ cases. Restoration took **15.939
seconds**, retaining all 54 common runtime objects. The five preexisting test
memory-card files remained unchanged; the owned test runner exited normally.

### Offline discovery of typed EE data-family candidates

`discover_ee_data_families.py` analyzes the owned captured inputs without editing
the catalog, compiling another address-specific bank or running guest code. It
groups exact dependency-byte structures, proposing only varying LUI unsigned
immediates and SW signed offsets. Opcode/register/control changes split groups;
unchanged immediates retain exact guards. Input and search budgets, deterministic
identities, signed boundaries, round trips, duplicates and output conflicts are
covered by tests. See the
[`candidate schema`](../schemas/nexo-ee-data-family-candidates-v1.md).

The 34-bank input set yielded **196 candidates** from **71,757 dependency regions /
872,138 words**, in **1.447 seconds** including owned-ledger validation. Adding
the next miss as an offline prepared observation yielded **226 candidates** from
35 cases in **1.739 seconds**, including preparation. No bank was admitted and
the game executable/catalog remained unchanged. The eight-word suffix at
`0x184f474` (`0x184f448 + 44`) matched the same guarded structure at **eight
observed locations**, with LUI/SW parameters `389` and `-1852`. This automates
the previous manual instruction-shape comparison; it does not synthesize the
native function yet.

All **1,896 source-origin round trips** across the 226 candidates reconstructed
their exact captured bytes and hashes. Final-source replay reproduced the same
candidate structures and counts; reports now pin the analyzer source hash.
The **16 detector tests** and **71 CTest groups** passed. These extraction and
regression checks do not validate execution of a native family.

All approval fields remain false. These dependencies may be speculative or
partial function regions. Producer invariants, reachable parameter domains,
relative-PC/control semantics, entry/fetch/write/alias obligations, independent
hardware fidelity and native execution of the candidate remain open. Candidate
recognition alone cannot resume the stopped game. Evidence is under
`build/lab/latest-ee-data-family-job.txt`.

### Generated native EE data-family functions

The offline `ps2_native_data_family` frontend now reuses the existing semantic
emitters with typed LUI/SW data operands and relative PC expressions. Its generated
wrapper guards the complete physical RAM structure and normal entry, extracts
live data fields, and calls a precompiled native body. All instruction resume
labels and standalone terminal slots are retained. There is no opcode execution
loop or compilation in the runtime function. Initial regions are linear, with an
optional terminal JR/JALR plus slot; unsupported forms fail explicitly. See the
[`native synthesis contract`](../schemas/nexo-ee-native-data-family-v0.md).

The synthetic compiled fixture passed **168 execution comparisons** over three
families, 36 concrete variants/bases and every normal entry. It compares the
identified complete EE context codec and all 32 MiB of RAM. The actual observed
eight-word suffix was also compiled as **one shared function**, passing **64
entry comparisons** across its eight observed variants/bases, including the new
miss at `0x184f474`. Its generated fixture compiled in **2.275 seconds**, linked
in **0.423 seconds** without LTO and ran in **2.548 seconds**. These are constructed
context tests against the shared conservative emitter, not independent hardware
or complete-machine game replay.

The initial 226-proposal sweep generated **167 candidates** and rejected **59**
unsupported shapes in **0.397 seconds**. Generated source is not an executed or
approved family. All 34 prior concrete bank sources remained byte-identical to
the archived converter's output. The game catalog, backend and executable have
not been extended with these functions yet. Family admission, materializer/domain
proofs, fetch/write/alias/device semantics and all complete-game gates remain
open. Evidence is under `build/lab/latest-ee-family-native-job.txt`.

### Experimental precompiled family admission in the game

The finite family matcher/backend now selects immutable native structures by
their complete masked RAM identity and entry offset, across physical bases.
It rejects ambiguous matches and pending architectural slots and repeats
admission at invocation. The generated native function then rechecks its guard
and reads its live typed parameters. There is no guest compiler or opcode
execution loop in this family path. The build option defaults OFF and requires
the AOT EE laboratory profile. See the
[catalog and admission contract](../schemas/nexo-ee-family-catalog-v0.md).

The offline publisher hashes generated sources and input provenance, merges
duplicate structures and records unsupported bodies. CMake checks manifest
types, counts, laboratory flags, paths and hashes before compiling. Neither
publication nor a successful match establishes producer domains or code
closure. Fetch/cache/writes/aliases and independent fidelity remain open.

The actual Monster House test used two families, generated in **0.093 seconds**:
the observed eight-word saved-frame suffix and its eleven-word fixed prefix.
The native incremental build took **13.861 seconds**, followed by a
**16.428-second game link**, with **zero original game compilations** and no
LTO. No new concrete bank or per-address TOML entry was added. The final
provenance publication took 0.115 seconds and preserved both generated bodies
byte-for-byte.

An owned Xvfb `:90` display and isolated silent audio sink reached New Game.
The log passed the previous `0x184f448` missing entry and recorded its
`0x184f474` continuation, then stopped at another uncovered callback
`0x184f498`, source `0x1ba930`. The capture contains EE model context and RAM;
it is not a complete machine checkpoint. All five memory-card files remained
unchanged, and the runner/display/audio helper exited. Agent-supplied menu
inputs are recorded; autonomous gameplay has not been validated.

The matcher and backend recheck groups each passed three C++ cases. Eight
Python cases cover publication/provenance and malformed CMake inputs. The
native concrete-directory group also passed four cases. These are laboratory
regressions, not PS2 or campaign approval. After restoring the diagnostic
development profile, **76/76 CTest groups** and **487/487 general C++ cases**
passed. Coverage of loaded code, control-flow
family synthesis, producer/fetch proofs, native VU, Android and 60 FPS remain
open. Evidence is under `build/lab/latest-ee-family-admission-job.txt`.

## Initial IOP AOT path

The IOP now also has an instruction-specialized V0 bridge integrated with IRX
startup and RPC callbacks. `generate_iop_bank.py` consumes an already relocated
RAM bank, creates an entry for every aligned word, and emits C++ operations with
constant instruction parameters. The native dispatcher verifies live code
identity, supports interior entries and RAM aliases, and refuses missing,
changed, or misaligned code without interpreting it.

The synthetic acceptance corpus has 23 strict native cases, 26 diagnostic cases
(including 292 absolute and 159 parameterized one-step comparisons to the
identified CPU model), and 12 converter cases. It includes native RPC, self modification, all RAM writers,
pending loads and branches, unknown imports, incomplete relocations, and
unfinished startup. `PS2X_IOP_ENABLE_INTERPRETER=OFF` removes the generic CPU
instruction-execution symbol from the native test executable.

This is preparation for M4. The default game runtime uses its diagnostic IOP
configuration; an explicit native IOP catalog option is described below.
Banks for its complete commercial corpus,
independent R3000A fidelity, canonical snapshots and qualified service/timing
contracts remain open. A synthetic startup/RPC result does not qualify Monster
House or a complete game. Commands and the internal bank contract are in
[`nexo-iop-aot-v0.md`](../schemas/nexo-iop-aot-v0.md).

### Original IRX startup bridge

`PS2X_IOP_BUILD_LAB=ON` adds `nexo_iop_inspect`: it runs the existing loader
offline, rejects incomplete relocation tables and misaligned/out-of-range entry
points, and writes a complete relocated RAM bank plus `module.json`.
`generate_iop_bank.py --loaded-module` reads that metadata directly; no manual
function-entry or callback address list is supplied.

```sh
cmake -S ps2xIOP -B build/iop-aot-strict -DPS2X_IOP_BUILD_LAB=ON -DPS2X_IOP_BUILD_TESTS=ON -DPS2X_IOP_ENABLE_INTERPRETER=OFF '-DCMAKE_CXX_FLAGS_RELEASE=-O1 -DNDEBUG -fno-lto' -DCMAKE_BUILD_TYPE=Release
cmake --build build/iop-aot-strict --target nexo_iop_inspect --parallel 4
build/iop-aot-strict/nexo_iop_inspect /path/to/module.irx /path/to/new-case
python lab/generate_iop_bank.py --loaded-module /path/to/new-case --output /path/to/generated --symbol compiledIopProgram
cmake -S ps2xIOP -B build/iop-aot-strict -DNEXO_IOP_BANK_CPP=/path/to/generated/iop_native_bank.cpp
cmake --build build/iop-aot-strict --target nexo_iop_native_probe --parallel 4
build/iop-aot-strict/nexo_iop_native_probe /path/to/module.irx /path/to/new-result
```

The native probe uses a bounded laboratory host and reports unsupported external
operations as failures. It captures final physical IOP RAM and EE RAM, startup
return, counters and logs. `nexo_iop_baseline_probe` is a separate target available
only with the diagnostic interpreter enabled. Neither probe is a game runner.

The original Monster House `HKSIF.IRX` startup was exercised with 85 native guest
operations and 11 service dispatches, zero interpreted operations and no generic
CPU execution symbol in the native binary. Its 2 MiB IOP RAM, 32 MiB EE RAM,
startup return, counters and logs matched the identified diagnostic model.
All 11 external IRX files passed **offline loader acceptance**, each isolated at
the default base. The later catalog experiment below also executed their startups.

Relocation-family binding in the actual game, embedded `IOPRP271.IMG` modules,
canonical hidden state and independent fidelity remain open. The commercial
images, generated C++ and captures stay in ignored local build directories.

### Relocatable IRX families

The inspector also emits `source-image.bin` and per-word relocation masks.
`generate_iop_bank.py --family-module` embeds the full image identity and generates
fixed operation shapes with bound operands. It admits immediate-16 operand
forms and J/JAL targets. Full-word relocated data and unqualified operand forms
receive no executable callback; jumping to them fails explicitly.

```sh
build/iop-aot-strict/nexo_iop_inspect /path/to/module.irx /path/to/new-family-case
python lab/generate_iop_bank.py --family-module /path/to/new-family-case --output /path/to/family-generated --symbol compiledIopProgram
cmake -S ps2xIOP -B build/iop-aot-strict -DNEXO_IOP_BANK_CPP=/path/to/family-generated/iop_native_bank.cpp
cmake --build build/iop-aot-strict --target nexo_iop_native_probe --parallel 4
build/iop-aot-strict/nexo_iop_native_probe /path/to/module.irx /path/to/new-family-result 2
```

The subsystem binds the compiled family after relocation and before startup.
The dispatcher owns image/entry metadata, checks source identity, dimensions,
relocation masks and fixed instruction bits, then guards the complete bound
word before each native guest operation. Reset clears bindings while retaining
compiled families. Directory replacement invalidates a previous overlapping
binding completely; module unload also retires its directory.

The original HKSIF family passed two consecutive startups, automatically placed
at `0x10000` and `0x10500`: 170 native operations, 22 service dispatches, zero
interpreted operations and no native faults. Full RAM and all reported fields
apart from native/diagnostic counters agreed with the identified model. A
modified source image failed before executing a native operation.

This is one module's startup coverage. The loader remains an identified model;
independent fidelity, hidden state, service/version/timing contracts, code
publication epochs and full kernel lifecycle on replacement are open. Buffer
load identity variants, the complete module corpus and final game integration
are also open. Matching a family at two bases does not qualify M4 or a game.

### Shared multi-module catalog and runtime adapter

`--family-catalog` accepts multiple inspector directories and emits one catalog.
Each full image keeps its own directory and complete bound-word guards. Immediate
and jump target operands share compiled operation shapes across modules, including
non-relocated instructions whose original words must still match exactly. The
operation pool is explicitly instantiated once, in 64 stable hash shards; module
directories contain references only. Unchanged files retain their mtimes.

```sh
python lab/generate_iop_bank.py --family-catalog /path/to/case-a /path/to/case-b --output /path/to/catalog --symbol compiledIopProgram
cmake -S ps2xIOP -B build/iop-catalog-strict -DPS2X_IOP_BUILD_TESTS=ON -DPS2X_IOP_BUILD_LAB=ON -DPS2X_IOP_ENABLE_INTERPRETER=OFF -DNEXO_IOP_BANK_CPP= -DNEXO_IOP_BANK_MANIFEST=/path/to/catalog/catalog.json -DCMAKE_BUILD_TYPE=Release '-DCMAKE_CXX_FLAGS_RELEASE=-O1 -DNDEBUG -fno-lto'
cmake --build build/iop-catalog-strict --parallel 4
ctest --test-dir build/iop-catalog-strict --output-on-failure
build/iop-catalog-strict/nexo_iop_native_probe /path/to/module.irx /path/to/new-result
```

CMake admits only generated C++ basenames and checks the hashes of every source
and the semantics header. Source/header changes trigger manifest revalidation
before building. The converter has eight catalog tests covering normalization,
deduplication, incremental stability, unsafe manifests and changed sources.

For the 11 external Monster House IRX images, generation took 0.40 s and a new
standalone library/catalog/probe/test build took 27.58 s with four workers and
`-O1 -fno-lto`. Their 156,928 directory words selected 3,933 shared kernels;
435 relocated-data or unqualified words have no executable callback. All 11
isolated startups completed: 1,915 native operations, zero interpreted operations
and zero native faults. Each reported state and full IOP/EE RAM matched the
identified diagnostic model. Created background threads were not exercised.
These measurements cover this IOP corpus, not whole-game conversion or FPS.

The experimental root integration uses the actual runtime file/memory adapter:

```sh
cmake -S . -B build/runtime-native-iop -DPS2X_BUILD_NEXO_LAB=ON -DPS2X_FAST_ITERATION=ON -DPS2X_IOP_ENABLE_INTERPRETER=OFF -DPS2X_RUNTIME_NATIVE_IOP=ON -DNEXO_IOP_BANK_MANIFEST=/path/to/catalog/catalog.json
cmake --build build/runtime-native-iop --target nexo_iop_runtime_probe --parallel 4
env -u DISPLAY -u WAYLAND_DISPLAY SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy build/runtime-native-iop/lab/nexo_iop_runtime_probe /path/to/module.irx /path/to/new-runtime-result
```

The runtime probe initializes RAM only, never window/audio or EE/VU execution,
and loads the module twice through `PS2Runtime::loadIopModule`. Original HKSIF
completed both startups with 170 native operations, zero interpreted operations
and no faults; full RAM and reported state matched the actual diagnostic runtime
adapter. The native executable lacks the generic IOP instruction-execution symbol.
The opt-in requires a compiled catalog and the IOP interpreter disabled; it does
not qualify the application's EE/VU paths. In the root fast-iteration profile,
both the IOP library and catalog use `-O1 -fno-lto` and disable IPO.
In the existing root cache, rebuilding the native adapter/library/catalog with
that profile took 27.37 s; the next unchanged build took 0.26 s. Regenerating the
same catalog took 0.39 s and preserved all 78 source/header/manifest mtimes. A
one-byte change to the original HKSIF source was rejected by the actual runtime
before any guest operation, with no interpreted fallback.

Complete dependency ordering/RPC, embedded `IOPRP271.IMG`, buffer-load identity
variants, full replacement lifecycle, independent fidelity, service/timing
contracts and complete game execution remain
open. All commercial inputs, generated sources and RAM captures remain local in
ignored build directories.

### Import guards and bounded combined boot sequence

Native service dispatch now requires admitted identities for the import stub,
its ordinal word, table metadata and all preceding words examined by the
identified decoder. A known library name cannot execute a service from an empty
bank. Guest stores that change the ordinal, library name or version fail before
service execution. Relocated metadata can have a guarded data identity without
acquiring an executable callback. Aliases, reset and malformed dependency ranges
are covered by the native tests. This checks import dependencies; it does not
qualify the underlying service implementations or fetch/publication timing.

The actual runtime probe can load multiple modules and schedule IOP work after
each load without executing EE/VU code or initializing a window:

```sh
env -u DISPLAY -u WAYLAND_DISPLAY SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy build/runtime-native-iop/lab/nexo_iop_runtime_probe --sequence /path/to/new-sequence-result 80000 /path/to/SIO2MAN.IRX /path/to/PADMAN.IRX
```

The decimal cycle budget is EE cycles per load (0..16,000,000), passed to the
existing IOP clock model. At most 32 module paths from one host directory are
accepted. Reports include each load/scheduling checkpoint and detect exhausted
startup budgets. Four CLI tests cover invalid budgets/counts, mixed directories
and preservation of existing outputs.

The ten external modules in a preserved Monster House boot log were exercised
in its observed order, with 0, 800 and 80,000 EE cycles scheduled after each
load. At 80,000 cycles, the actual native runtime adapter observed eight threads,
six registered RPC servers, 6,482 native operations, zero interpreted operations
and no native faults; 4,758 guest/service instructions occurred during scheduling.
All reported fields except execution counters, full IOP/EE RAM, and runtime logs
agreed with the identified diagnostic adapter at all three budgets.

This experiment covers that module sequence and bounded thread work. It does
not cover RPC requests, the game's EE path, load arguments, complete dependency
ordering, or game progression. The guest logs include an event-wait failure and
a SIF-initialization warning in both adapters; model equality does not resolve
those issues. Embedded modules, buffer identities, full replacement lifecycle,
canonical hidden state, service fidelity and hardware timing remain open.

### Observed module and RPC capture

The root `PS2X_BUILD_NEXO_LAB=ON` build now includes optional IOP observation
hooks. Set `PS2X_IOP_CAPTURE_DIR` to a fresh local directory when launching a
laboratory producer or the actual-runtime probe. The non-laboratory build does
not compile these hooks. The standalone `PS2X_IOP_BUILD_LAB` option continues
to select offline inspector/probe tools; it does not enable live observation.

Each chronological event has a numbered directory. Module events contain the
exact loader input (`image.irx`), raw arguments (`arguments.bin`) and metadata.
RPC events record request metadata and bounded EE send bytes, then result
policies, instruction counters and bounded receive bytes. A native fault records
its diagnostic and physical IOP RAM. Missing payloads are explicitly marked;
a missing metadata/result file is incomplete evidence. Recording errors are
reported without writing guest memory, and failure of the diagnostic sink is
contained. Output event directories are never reused.

Capture has separate process-wide limits: 4,096 RPC events, 256 module events
and 16 fault events. RPC payloads are limited to 1 MiB. Exhaustion warns once
per event kind and leaves the other kinds' budgets available. These are bounded
observations, not complete execution histories or canonical state checkpoints.
Captures add work and are unsuitable for certifying performance.

Four capture CLI tests cover raw arguments and escaping, request/result
payloads and policies, invalid and oversized buffers, empty buffers, an HLE
route and an unhandled route, recording failures, disabled observation,
independent event budgets, preserved existing events, first native-fault
observation, and unchanged actual-runtime reports/full RAM.

A headless Monster House diagnostic producer yielded ten external module
images, four buffer module loads and 4,082 handled RPC observations before
the original shared event limit was exhausted. The four buffer images were
byte-identical (7,096 bytes, SHA-256
`aae4e64bbb49d54caf2e1c9c9071dccf46ed95f06deaf072a4c79eef7b8765c1`),
so they require one additional offline family. The captured unique images plus
CUTSTRM produced a twelve-module catalog with 3,947 shared kernels. Generation
took 0.412 seconds and the cached standalone native build took 21.113 seconds
with four workers and `-O1 -fno-lto`. Relinking the diagnostic game runner reused
all existing EE objects and took 9.988 seconds.

The captured buffer family executed one/four isolated startups with 28/103
native operations, zero interpreted operations and zero native faults. Reports
and full IOP/EE RAM matched the identified diagnostic model. Its startup return
was **1**, and the isolated probe registered no threads or RPC servers; this
does not establish residency or successful operation in the game's dependency
context. Actual RPC replay, native game progression, complete replacement
lifecycle, hidden state, service/hardware fidelity and platform/game gates remain
open. The headless producer still used diagnostic IOP/VU and EE overlay paths.
Commercial images, payloads, generated catalogs and screenshots remain local.

### Native IOP in the observed game path

The twelve-module catalog was linked into the actual Monster House producer
with `PS2X_RUNTIME_NATIVE_IOP=ON` and the IOP interpreter excluded. On its
isolated virtual display it loaded ten external IRXs and all four buffer
instances. Those buffer startups returned **2** in the game context, unlike
their isolated/staged startups, which returned **1**. Captured inputs therefore
do not replace the dependency and RPC state needed to reproduce a load.

The bounded capture contains 4,096 handled RPC calls across four SIDs and
30,425,512 native operations at its last RPC checkpoint, with zero interpreted
operations and no native fault event/log. Among the diagnostic producer's
previous observations, 2,718 transactions had identical request metadata,
send bytes, result policies and receive bytes; 1,378 native observations had no
identical recorded request. No identical request had an unmatched result in
that comparison. This is observed transaction agreement, not aligned histories,
canonical state replay, hardware fidelity or a proof for unobserved requests.
The RPC capture budget was exhausted, so the complete run is not represented.

A separate staged fourteen-module sequence at zero/80,000 EE cycles per load
matched the identified diagnostic adapter's reports (apart from execution
counters) and full IOP/EE RAM, with zero native faults. It correctly failed
startup acceptance because all four buffer startups returned 1. That result
is retained as a context counterexample; RAM agreement does not turn it into
successful residency.

The game remained on its loading screen while the diagnostic EE driver
compiled additional overlays. EE closure/AOT packaging, VU integration, the
GS backend, campaign progression, fidelity, FPS and Android qualification
remain open. This producer proves observed IOP execution through the original
modules and handlers without its generic instruction interpreter; it is not a
final native game package.

### Isolated runtime IOP backend selection

The native/diagnostic constructor choice now lives in a private object target,
`ps2_runtime_iop_backend`. This keeps its flag out of the common runtime's
`flags.make`: Make dependencies previously recompiled unrelated runtime objects
even though the flag was attached to only one source. Public runtime headers
and generated EE objects do not change.

A Make-based regression switches diagnostic → native → diagnostic and checks
that the common object's bytes and timestamp stay unchanged, the backend
object changes, and the executable selects the requested implementation.
In the actual cached root build, native → diagnostic configuration took 2.850
seconds and the build took 9.056 seconds with four workers. All 52 common
runtime objects retained their bytes and timestamps. The IOP library and the
small backend still rebuild when their profile changes; the larger runtime does
not. The one-time migration of its shared flags took 34.722 seconds.

Actual-runtime HKSIF probes through both factory variants still agreed on
reports/full RAM: two native startups executed 170 native operations with zero
interpreted operations and zero faults. These measurements concern profile
iteration and bounded module startups, not complete ISO conversion latency.

## Build and run

```sh
cmake -S . -B build -DPS2X_BUILD_NEXO_LAB=ON -DPS2X_FAST_ITERATION=ON
cmake --build build --target nexo_vu_snapshot_tests nexo_device_snapshot_tests nexo_vu_native_tests nexo_vu_replay nexo_vu_inspect --parallel 4
ctest --test-dir build -R '^nexo_(vu_|device_)' --output-on-failure
build/lab/nexo_vu_replay /path/to/canonical-capture 65536 100
```

The replay's explicit cycle budget must match the recorded call budget. Restore
uses continuation semantics even for MSCAL: the capture already contains the
fresh-call scheduler normalization. Re-executing a fresh call would normalize
the captured pipeline a second time. MSCNT captures must retain their pending
work. Fresh-call normalization in this runtime preserves the absolute clock.

The lab targets compile with `-O1 -fno-lto`, and IPO is disabled. The existing
runtime fast-iteration profile uses `-O1 -fno-lto` with its private VU sources
at `-O2`. There is no rebuild of generated EE game functions for a replay case.
Laboratory capture hooks and the codec enter the runtime archive only when the
lab option is enabled. The default option is off.

Root development settings now apply to the recompilation, analysis and test
targets too. Their prior release helper ignored fast iteration and enabled LTO
for 48 source units. The Linux compile-command audit now finds zero LTO units
among those project targets. Release IPO requires both fast iteration off and
`PS2X_ENABLE_RELEASE_IPO=ON`; MSVC's explicit /GL and /LTCG follow that gate too.
Only the Linux configuration has been exercised for this change.

## Capture boundary

Set `PS2X_CAPTURE_SCENE` to a private local directory and create `.vu-request`
inside it. A lab-enabled runner consumes the marker at the next VU1 MSCAL or
MSCNT call. It records canonical input/output state, code/data byte memories,
and timed PATH1 submissions. Capture and code/data writers must be synchronous.
Commercial game data belongs in ignored local build workspaces.

The state codec covers pending vector/integer/ACC writes, delayed flags, Q/P
results, readiness and resource clocks, branch history and partially transferred
XGKICK packets. Host pointers and derived decode caches are rebuilt on restore.
See the versioned documents in `schemas/` for field order and bounds. States
with a second XGKICK already issued and waiting for PATH1 use the version-2
extension; empty request slots retain the exact version-1 encoding. Tests
exercise the second pair's Upper operation, its latched source, live future
payload reads, and continuation without generic VU execution in the AOT path.

Legacy `.bin` register images remain useful only with the original host ABI.
They cannot substitute for `input-state.nexo` in this tool.

The replay observes submissions using an otherwise empty GIF receiver. It does
not restore GIF arbitration, VIF execution, GS state or VRAM; it does not render
the game. Comparisons are explicitly `tested_only`, against the same runtime.
An enclosing case manifest must bind all blobs and producer/reference binaries
with SHA-256 before it can be used as evidence.

## Compile a finite native VU bank

After the inspector has been built, translate a captured bank before execution:

```sh
python lab/generate_vu_bank.py --inspect build/lab/nexo_vu_inspect \
  --code /path/to/canonical-capture/code.bin --unit vu1 \
  --output build/lab/compiled-vu-bank.cpp --manifest build/lab/compiled-vu-bank.json
cmake -S . -B build -DPS2X_BUILD_NEXO_LAB=ON -DPS2X_FAST_ITERATION=ON \
  -DNEXO_VU_BANK_CPP="$PWD/build/lab/compiled-vu-bank.cpp"
cmake --build build --target nexo_vu_native_replay nexo_vu_runtime_replay --parallel 4
build/lab/nexo_vu_native_replay /path/to/canonical-capture 65536 100
build/lab/nexo_vu_runtime_replay /path/to/canonical-capture 65536 100
```

The converter derives static dependency descriptors and emits upper/lower
operations specialized by compile-time instruction constants, including the
opcode-dependent numeric and flag helpers. The native scheduler takes the
compiled entries and data memory; it has no guest instruction buffer to fetch
or decode. Unsupported entries, invalid PCs and code identity changes produce
`UNSEEN_CODE`, without an interpreter or runtime guest compiler fallback.

Eight link wrappers reject generic interpreter execution entry points in this
replay executable. A deliberate forbidden call verifies that the trap is active.
The inspector is a separate conversion target. The prototype still shares
pipeline and device helpers with the existing runtime, and the linked runtime
contains legacy interpreter code. These checks do not establish final-package
interpreter absence or native execution of the whole game. Linux with the
GNU/LLVM C++ ABI is the currently supported laboratory configuration.

## Native runtime integration

`bindNativeVu1` installs strict native callbacks into an initialized PS2Runtime
memory session, using the existing VIF MSCAL/MSCALF and MSCNT interfaces. It
copies bank descriptors and identities into owned storage. Compiled function
pointers must remain loaded. Selection compares complete microcode bytes on
every call, including after raw mutable code writes that bypass generation
counters. Ambiguous identities and malformed collections are rejected before
replacing an installation. Unknown code does not fall back to interpretation.

Native fresh execution resets pending work according to the identified runtime
model while retaining the clock. Native MSCNT retains pending pipelines. Both
propagate D/T enables and CPU-visible stop flags through the normal runtime.
The current callback horizon is 65,536 cycles. See
`schemas/nexo-vu-runtime-binding-v0.md` for ownership, reset and scope limits.

The runtime replay executable restores a normalized VU capture and drives it
through a **synthesized MSCNT command**. It reconstructs TOP/ITOP and D/T inputs
from canonical VU fields and observes PATH1 with the same empty receiver as the
standalone replay. This exercises real VIF parsing and native runtime callbacks;
it does not reconstruct the original VIF command stream, full VIF state, GIF
arbitration or GS/VRAM. Its JSON identifies the synthesized input explicitly.
Headless fixtures contain no executable EE entries and open no user window.

Both native replay executables link the same compiled bank archive, avoiding
duplicate bank compilation. Semantic generation updates both output timestamps
so unchanged outputs do not make Make rerun generation for each dependent target.

## Transport and CPU graphics checkpoints

`Vif1SnapshotCodec`, `GifSnapshotCodec` and `GsSnapshotCodec` preserve the current
runtime's incremental VIF parser/transport, queued GIF submissions, and CPU GS
frontend/backend plus VRAM. Restore is bounded and transactional. GS includes
partially assembled primitives, loaded palettes and remembered CBPs, stale
texture page bytes, partly consumed readbacks, private register values, and
latched presentation data. Host pointers and callbacks remain owned by the
receiving instance. Execution and all writers must be paused before capture.

The headless device suite splits example VIF inputs at every byte and compares
restored continuations. It tests GS/GIF continuation and corrupt states as well.
See `schemas/nexo-device-state-v1.md` for exact scope, ordering and bounds.
The older VU replay CLIs retain their documented empty GIF receiver and
synthesized input. The separate original-call replay below restores these
device codecs with the runtime's real GIF-to-GS routing. Neither comparison
is an independent graphics reference.

## Original VIF call and all observed native banks

Create `.vif-request` in the lab runner's `PS2X_CAPTURE_SCENE` directory. The
request remains pending until a completed call containing a VU callback. It
captures the literal VIF argument before parser normalization, full VIF/VU/GIF/
CPU-GS states, 16 KiB code/data memories, each actually executed code identity,
and both GIF submissions and GS deliveries. Host presentation is serialized
across acquisition; other writers must remain quiescent. See
`schemas/nexo-observed-vif-case-v1.md` for the boundary and exclusions.

```sh
cmake --build build --target nexo_vif_replay nexo_vu_inspect --parallel 4
build/lab/nexo_vif_replay /path/to/completed-vif-case 10
python lab/generate_vif_banks.py --case /path/to/completed-vif-case \
  --inspect build/lab/nexo_vu_inspect --output build/lab/vif-banks \
  --cache build/lab/vif-bank-cache
cmake -S . -B build -DPS2X_BUILD_NEXO_LAB=ON -DPS2X_FAST_ITERATION=ON \
  -DPS2X_ENABLE_RELEASE_IPO=OFF \
  -DNEXO_VIF_BANK_SOURCES="$(cat build/lab/vif-banks/cmake-sources.txt)"
cmake --build build --target nexo_vif_native_replay --parallel 4
build/lab/nexo_vif_native_replay /path/to/completed-vif-case 10
ctest --test-dir build -R '^nexo_' --output-on-failure
```

Conversion processes the entire finite bank collection, rather than waiting for
an unknown callback and manually adding one address. Metadata reuse binds the
inspector/emitter/converter identities and full bank bytes. Corrupt cache is
reinspected; unchanged generated C++ retains its timestamp. Changing unseen
game code still requires conversion-time closure, rather than silent runtime
interpretation. The collection manifest explicitly denies closure beyond the
observed case.

The native CLI installs strict owned native callbacks and eight fatal VU
interpreter link traps. Both original-call CLIs open no user window, compare
complete canonical component states and events, and produce JSON on stdout
with diagnostics on stderr. The acquisition runner remains a laboratory
producer with legacy IOP/VU and overlay paths; it is not a final game package.

The headless launch tool supports `--runner` and explicit `--capture-scene`.
It chooses a free display after the base display option and checks that the
Xvfb lock PID belongs to its own process group before executing the game.
This avoids an installed wrapper's behavior of launching a client even when
its new server failed. An existing session's virtual server is rejected.

### Original Monster House sample, 2026-09-30

The captured original call contains nine VU callbacks and two distinct full
code banks. All ten baseline repetitions and ten conservative AOT repetitions
match the recorded VIF, VU, GIF, CPU GS/VRAM, code/data, relevant CPU status and
ordered events exactly. No interpreter wrapper fired in the native path.
These results are same-model `tested_only` evidence for one observed call.

Compiling both banks and the native CLI took 23.489 seconds under concurrent
host load, with six source compilations and zero generated EE compilations.
An earlier diagnostic game relink took 11.621 seconds using existing EE objects.
These are scoped development build measurements, not whole-ISO conversion,
whole-game gameplay or a promise of 60 FPS.

Repeated conversion of the unchanged collection used zero inspector runs,
preserved every C++ source timestamp and took 0.348 seconds. The following
unchanged native replay build took 0.307 seconds and compiled zero source
files. These measurements cover local cache reuse, under uncontrolled host
load, and exclude discovery of unobserved code.

At commit `ea497eb`, the eight laboratory suites passed 82 cases: 14 VU checkpoints, 14 device
checkpoints, 21 native VU cases, 11 original VIF cases, three Python generator
suites of six cases each, and four headless isolation cases. The rebuilt general
runtime suite passes 484/484 with DISPLAY, WAYLAND_DISPLAY and capture unset.

## Profiling the original call and CPU GS compilation

```sh
build/lab/nexo_vif_native_replay /path/to/completed-vif-case 10 --profile
cmake -S . -B build -DPS2X_FAST_ITERATION=ON \
  -DPS2X_FAST_ITERATION_OPTIMIZE_GS_CPU=ON -DPS2X_ENABLE_RELEASE_IPO=OFF
cmake --build build --target nexo_vif_native_replay --parallel 4
```

Profiling brackets actual VU callbacks, GIF submission and GS delivery using
thread-local host timers. Nested exclusive totals avoid counting GS work again
as VU work. Profiling remains outside canonical states/events, is disabled by
default, and includes observation overhead according to the documented scope.
There is no guest interpreter fallback in the native replay.

In ten instrumented repetitions of the original Monster House case, the GS
receiver used 247.990 ms out of 250.813 ms of VIF-call wall time (about 98.9%).
VU callback exclusive time was 1.296 ms. These are one-case measurements under
uncontrolled host load, not whole-game throughput.

A paired `ABBA ABBA` experiment compared CPU GS at `-O1` with `-O2` and FP
contraction disabled. Each mode replayed the original case 20 times. Median
call execution was 251.435 ms versus 212.890 ms (measured ratio 1.181), with
identical full states and events. No new graphics algorithm, hardware fidelity
proof, GPU backend, game FPS or whole-game qualification follows from this.

The kernel lives in `ps2_gs_cpu_backend`, a separate object target included in
the runtime archive. It inherits the runtime's includes, definitions and common
compile options. Its private optimization flags have their own Makefile: the
previous per-source-option placement changed `ps2_runtime/flags.make` and
recompiled 52 runtime units. This target split isolates later option changes.
The first structural migration still invalidates the old monolithic flags.
Linux GNU/Make is the configuration exercised for this change.

After that migration, switching the GS option off and on compiled exactly one
source each time, taking 2.192 and 2.742 seconds respectively. The global
runtime flags file remained byte-identical; generated EE compilation count
remained zero. Each switch replayed the original case five times with exact
state/event equality. The optimization remains an opt-in development variant.

The final CTest run passes all 55 registered groups, including the expanded 85
laboratory cases and 484 general runtime cases. Original-call reference,
native and instrumented native paths each match ten repetitions of the
recording with the split object target. The three new timing cases initially
failed, then passed after nested timing and the actual callback brackets were
implemented. They cover memory filtering, nested exclusive subtraction,
exception unwinding, disabled timing and preservation of canonical state/trace.

The external gprofng sampling attempt was rejected because the collector
reported a changed interval timer and unreliable data. Its 49 samples cannot
support whole-run CPU percentages. Direct scope measurements replaced that
attempt; matching game state alone does not approve a profiler.

For independent-reference investigation, upstream PCSX2 revision
`94d86c891b1621c0b252e4fc2e155bf90274dcc0` was acquired with source hashes and
its GPL license in ignored local reference storage. It has not executed this
canonical case. An adapter must establish its state correspondence and device
boundary before it can provide independent evidence. No Android device was
attached at the latest ADB inventory.

## Recorded checks, 2026-09-30

- Device checkpoint cycle: all six initial cases first failed against explicit
  stubs; VIF/GIF implementation passed five, then GS completed all six. The
  expanded suite passes 14/14 cases, including checksummed semantic corruption,
  transactional rejection, partial transfers/vertices, cached palettes and
  texture visibility, atomics and presentation state. Together with the other
  four lab suites this is 61 passing cases. General runtime regressions remain
  484/484 after rebuilding against the new sources, with no user display.
- A development rebuild after touching only `lab/src/gs_snapshot.cpp` took
  7.537 seconds under concurrent CPU load, compiled exactly one source unit
  and rebuilt no generated EE functions. This is a codec rebuild measurement,
  not ISO conversion time or whole-game compilation.
- Initial codec test cycle: 6 failures / 1 pass, then 7/7 passes; expanded to 10/10.
- Capture integration: the new MSCAL/MSCNT tests first failed, then 12/12 passed.
- Timed PATH1 capture: the added XGKICK test first failed, then 13/13 passed.
- Standalone replay: the three integration cases first failed against its stub,
  then all 13 tests passed with the implementation.
- Six malformed CLI invocations were rejected.
- A checksum-valid scalar deadline outside the scheduler model was first
  accepted by the decoder (the new test failed), then rejected transactionally
  after adding deadline bounds. The expanded suite passes 14/14.
- A real Monster House MSCAL capture ran for 277 VU cycles and issued 254 pairs.
  All 100 standalone repetitions matched the complete canonical output state,
  VU data memory and timed PATH1 submissions exactly.
- That sample averaged 104.589 microseconds for VU execution, including cold
  derived-code decoding and PATH1 recording. File input, machine construction,
  state restoration and final comparison were outside the measured interval.
  This is one CPU sample, not game FPS, an AOT speedup, or sustained performance.
- The diagnostic game runner was relinked in 13.408 seconds using existing EE
  objects: zero generated-source compilations. Its legacy overlay driver still
  compiles guest code at runtime; it is not eligible for final AOT acceptance.
- Initial conservative native V0: 11/11 C++ cases passed, including FMAC, suspended Q/P
  pipelines, VI branch history, stores, future XGKICK reads, interpreter traps,
  unknown entries and an overflowing PC. The latter first caused a segmentation
  fault, then was rejected before table lookup after fixing the bounds check.
- The native suite now passes 21/21 cases. Added checks cover fresh normalization,
  real VIF callbacks, ownership after source views are destroyed, changed banks
  with pending pipelines, CPU-visible D/T status, unknown identities, ambiguous
  installations, canonical runtime replay and its exact budget and D/T inputs.
  The new behavior first failed against explicit implementation stubs. The D/T
  replay check subsequently exposed overwritten enables and passed after their
  canonical inputs were reconstructed. Captured TOP/ITOP outside the VIF callback
  domain are explicitly rejected, rather than silently masked.
- A continuation test exposed a stale reference decode cache after a raw write.
  The fixture now publishes that write to the reference's generation counter;
  native selection still detects the new bytes without a generation increment.
- Semantic generation and bank generation each pass 6/6 Python cases. Together
  with the 14 codec/capture cases, the four laboratory suites cover 47 cases;
  the latest CTest run completed in 1.51 seconds.
- The existing general C++ suite was rebuilt in the fast profile and passes
  484/484 cases with DISPLAY, WAYLAND_DISPLAY and scene capture unset.
- An unchanged rebuild of all five laboratory executable/test targets completed
  in 1.534 seconds, compiled zero source units and reran the semantic generator
  zero times. This measures an unchanged incremental build, not ISO conversion.
- The real Monster House bank contains 2,048 compiled entries. Frontend analysis
  and C++ emission took 0.169 seconds; this excludes C++ compilation and linking.
- All 100 native-only repetitions of the real capture matched complete state,
  data and timed PATH1 submissions exactly, without triggering the interpreter
  traps. A changed instruction byte was rejected by the identity guard.
- Undefined-symbol audits of the generated bank and native scheduler objects
  found no generic interpreter execution/decoder imports. This is an audit of
  those two objects, not a complete executable reachability proof.
- All 100 repetitions through the real runtime's native VIF callbacks also
  matched the Monster House recording exactly. Changed-code and wrong-budget
  CLI cases were rejected. The existing reference and direct native paths each
  retained 100/100 exact matches after the shared replay refactor.
- The final integrated sample averaged 112.056 microseconds under uncontrolled CPU
  load, including synthesized VIF processing and native bank selection. It is
  not a controlled speed comparison, whole-game FPS or evidence of a faster port.

Local evidence is under `build/nexo-*.log`. The canonical game capture and its
SHA-256 manifest are in the directory named by
`build/lab/latest-monsterhouse-vu-capture.txt`. Generated captures/binaries are
ignored and must not be committed with the laboratory sources.
The native validation receipt and frozen artifacts are identified by
`build/lab/latest-monsterhouse-native-v0-evidence.txt`.
Integrated runtime replay evidence is identified by
`build/lab/latest-monsterhouse-native-vif-evidence.txt`.

## Remaining first-increment work

1. Capture/replay VIF input and hidden state at the same boundary.
2. Integrate the transport/CPU GS checkpoints into the synchronized original
   VIF case and compare GIF/GS effects with identified references. Synthetic
   checkpoint tests do not complete this integration gate.
3. Add an identified independent reference and extend V0 validation to more
   programs, both VUs, additional operations and code-upload variants.
4. Map divergences to semantic fields and causal events; current CLI reports
   only the first differing byte per output blob.
5. Validate the canonical format and execution on a second architecture.
6. Measure baseline/AOT/optimized variants on a held-out corpus with proof gates.
7. Extend runtime integration to all required banks, prove upload/code closure
   and exclude legacy guest execution from a separate final package build.

None of these outstanding gates is satisfied by matching one case against the
same runtime. Full game progression, Android and universal conversion remain
separate unfinished milestones in the root plan.

# EE normal-entry dependencies V1

This is a laboratory refinement of the finite EE AOT catalog, not a complete
R5900 entry, instruction-fetch, publication or semantic-cache contract. It fixes
guards that rejected ordinary interior entries because skipped prefix words had
changed. The callback bodies, immutable snapshots and original binding ABI stay
identical; only the active catalog's dependency descriptors change.

## Entry admission

The current translator emits an independent resume label for every instruction.
For an ordinary entry at `pc`, the linear prefix before that label is skipped.
Its declared dependency begins at `pc`, except when the same callback can reach
an earlier local static branch/jump target. In that case it begins at the earliest
such target or `pc`, whichever is lower. It ends at the original block end.
This remains conservative: bytes that cannot execute may still be included.

A separately entered terminal delay-slot label executes that instruction and
returns to the next PC; it does not repeat its preceding branch. Its dependency
is its own four-byte instruction. This is different from resuming a pending
architectural branch. The runtime invocation adapter rejects a null context or
`context.in_delay_slot=true`, preserving guest RAM and context. It rechecks bytes
at invocation, including changes since the earlier resolver query. It stops on
rejection; there is no guest interpreter, decoder or runtime compiler fallback.

`Dispatcher::lookup` and `diagnose` are RAM-only queries. `Ready` from them does
not promise that an execution context can be admitted. Runtime resolution returns
an adapter, whose invocation selects by the actual context PC and calls
`admitNormalEntry`. Complete hardware entry-context semantics remain unqualified.
Existing static/core and loaded-module tables retain their prior behavior.

The C++ API accepts `OverlayDependencyContract::NormalEntry` (default) or
`LegacyWholeBlock`. The CLI defaults to normal dependencies; its optional sixth
argument `--legacy-footprints` reproduces the original whole-block descriptors.
The separate diagnostic live DSO driver explicitly requests legacy footprints,
because that consumer does not use the new invocation adapter. It remains a
development baseline and is omitted from the selected AOT EE backend.

## Offline producer and immutable banks

New catalog generation emits both descriptor variants from the same snapshot.
It checks identical callback/snapshot/getter text, identical callback names and
entry sets, and dependency ranges contained in the legacy footprint. Bank C++
sources retain the legacy descriptors and their byte identity. The index owns
copies of bindings and applies the normal dependencies to those copies once.
Changes to this index therefore need not rebuild existing bank objects.

Recovered cases without `dependency_contract` mean `whole-block-v0`; their
recorded bindings are compared against the legacy emission. Prepared cases now
record `normal-entry-v1` and are compared against the normal emission. Ordinary
extension preserves the catalog's policy and requires the same generator hash.

Explicit migration:

```sh
python lab/generate_ee_bank_catalog.py all-previous-case-directories \
  --generator build/ps2xRecomp/ps2_native_overlay \
  --output existing-catalog --extend --migrate-entry-guards
```

The old manifest and every old source hash are checked first. Migration allows
a different producer binary only when every previously compiled bank source
regenerates byte-for-byte identically. A captured case pinned to the old producer
is admitted only for a bank already listed in that verified old catalog. Any
old bank removal or source rewrite rejects the update before publication.
The new manifest records the previous producer and catalog hashes. This proves
the stated source identity, not compiler/header identity or hardware fidelity.

## Dependency sidecar

`ee_entry_dependencies.json` is bounded to 64 MiB, with schema 1 and
`dependency_contract=normal-entry-v1`. Each bank names its generated C++ source
and contains compressed `runs`. Each run is `[first,last,begin,end]`:

- `first` is an included aligned entry PC; `last` is the excluded aligned bound.
- PCs in the run are consecutive four-byte entries; missing PCs start a new run.
- `begin=4294967295` means each entry's own PC; otherwise it is a fixed floor.
- `end` is the excluded dependency end. The complete range is compared to RAM.

Compression must reconstruct the exact emitted entry descriptors. The manifest
records its basename, SHA-256, total run count, index/source hashes and producer
identities. CMake rejects unknown policies, ambiguous scalar types, wrong names,
symlinks, excessive sizes/counts and changed hashes. CMake does not independently
prove a run's semantic meaning or its correspondence to the C++ index; those
are producer/test obligations, not an authentication or cache-correctness claim.

Publication requires an exclusively owned offline directory without concurrent
producers/builds. The sidecar is replaced before the index, and the manifest last.
Interruption can leave an incomplete update that fails subsequent identity
validation. This is not the root README's qualified publication protocol.

## Checks and remaining obligations

Synthetic native execution covers skipped-prefix changes, local backedges,
reachable-prefix rejection, standalone slots, mutation after resolver query,
pending-delay/null context rejection and guest RAM/context preservation. Migration
tests check old producer identity, immutable source timestamps, callback rewrite
rejection and lossless descriptor compression. The diagnostic DSO driver is
checked separately for its retained legacy descriptors.

The active 33-bank Monster House migration refined 524,557 entries into 58,945
runs. It took 9.135 seconds; rebuilding index/runtime/tests took 9.547 seconds,
preserving all 33 bank object hashes and timestamps. Relinking the existing game
objects took 10.969 seconds with `-fno-lto`, with zero original game compilations.
Both previously captured prefix-only failures passed saved-RAM guard replay.
These are measured local increments, not full ISO conversion times or fidelity
evidence. The isolated game reached its title, main menu and new-game file menu;
campaign gameplay and all final qualification gates remain open.
Attempting to start the selected game stopped at a new region, PC `0x1724d70`,
with `MissingEntry` and no candidates; the new miss was captured automatically.

Instruction-cache visibility, writes during callbacks, writer quiescence,
complete canonical machine replay, independent EE fidelity, universal coverage,
native VU in the actual game, Android, campaign/save/audio/rendering correctness
and sustained 60 FPS remain separate, unresolved obligations.

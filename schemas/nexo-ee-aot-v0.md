# Finite EE overlay catalog V0

This experimental catalog migrates **observed** diagnostic EE snapshots into
offline compiled callbacks. It is partial preparation for M3 in the root README.
It does not qualify code publication, closure, independent fidelity, a complete
game, Android, or a fully native package. The diagnostic VU backend remains.

## Artifact recovery

`lab/extract_ee_overlay.py` reads an identified ELF64 little-endian Linux x86-64
`ET_DYN` artifact. It parses ordinary symbol tables and RELA relocations without
loading the library or executing its getter. The recovered producer layout has
32-byte bindings and the identified `snapshot` and `bindings` objects. Native
callback symbols must belong to file-backed executable section ranges.

```sh
python lab/extract_ee_overlay.py observed.so new-case --entry 0x10000
```

The entry must come from the producer observation. Recovery emits
`snapshot.bin` and `bank.json`: physical base, observed root, image/artifact
SHA-256, byte length, and every recovered PC/source dependency. The file is
bounded to 128 MiB, the snapshot to 64 KiB, and bindings to 32,768. Output must
be fresh. A failed write can leave an incomplete output; it is not a case without
both files. `live_abi_independently_verified=false` is intentional: identifying
the artifact layout does not independently verify the running ABI.

## Offline catalog generation

```sh
python lab/generate_ee_bank_catalog.py new-case other-case \
  --generator build/ps2xRecomp/ps2_native_overlay --output new-catalog
```

The generator admits the image hash/dimensions before invoking the existing
overlay translator against a private copy of those bytes. Every regenerated
binding must agree with the recovered PC/source dependency. Cases are sorted by
their content key, so input order does not affect the catalog. Each bank gets its
own C++ namespace; versions at the same guest address coexist without symbol
collisions. The tool emits up to 512 banks, an index, and `catalog.json` with
source hashes, bank identities and generator/script hashes. No C++ compilation
occurs in the game runtime.

A complete manifest is written last. Failed generation can leave sources in a
fresh output directory; those sources are not an admitted catalog. There is no
automatic overwrite of previous cases. Explicit `--extend` validates an existing
catalog and preserves all unchanged source timestamps, as described in
[`nexo-ee-miss-v1.md`](nexo-ee-miss-v1.md). The manifest records producer
identity, not a qualified semantics/build fingerprint or a cache correctness
proof. Runtime headers and compiler settings still require separate provenance.

## Runtime admission

```sh
cmake -S . -B build -DPS2X_BUILD_NEXO_LAB=ON -DPS2X_FAST_ITERATION=ON \
  -DPS2X_RUNTIME_AOT_EE_OVERLAYS=ON \
  -DNEXO_EE_BANK_MANIFEST=/absolute/new-catalog/catalog.json
```

CMake requires schema 1, `compiledEeProgram`, one generated index, one source
per bank, unique generated basenames, and exact source hashes. The catalog
compiles with `-O0 -fno-lto` on GNU/Clang; the runtime selection bit belongs only
to a separate backend object. The diagnostic EE driver implementation is omitted
from that backend when AOT is selected. IOP selection remains a separate option.

The dispatcher owns source image/entry metadata and uses sparse 4 KiB pages of
aligned PC slots. Each slot chains finite precompiled candidates. Lookup compares
the candidate's full declared instruction footprint against physical EE RAM and
returns an existing callback only on equality. It does not decode guest opcodes,
interpret instructions, call a compiler, or load a generated DSO.

Limits are 512 banks, 64 KiB per image, 32,768 bindings per bank, and 2,097,152
total bindings. Invalid dimensions, misidentified pointers, unaligned footprints,
null callbacks and duplicate PCs within a bank reject the directory. Lookup
distinguishes `Ready`, `MissingEntry`, `CodeChanged`, `MisalignedPc`, `OutsideRam`
and `NoRam`. This V0 admits physical addresses only; other aliases fail admission.

Actual runtime misses emit `EE:UNSEEN_CODE` and request a stop. This overrides
diagnostic continue/skip policies, including the earlier branch-report path.
`hasFunction` remains a query. Existing static/core and loaded-module tables have
their original precedence and are not newly qualified by this change.

## Explicit unresolved obligations

- Guards observe RAM **at lookup**. Instruction-cache visibility, DMA/store
  publication epochs, alias coherence and changes during a running block are
  unqualified. Matching RAM bytes alone is not a fetch-state proof.
- Entry context, temporal device contracts, hidden kernel state and independent
  R5900 fidelity are not qualified. This uses the existing EE translator.
- Observations cannot prove that the finite catalog covers all future code.
  Unseen code stops; automatic family synthesis and full campaign exploration
  remain open.
- Opt-in miss capture now preserves copied RAM, optional canonical fields of the
  identified EE context model and candidate footprint differences. Offline byte
  preparation is implemented. A complete canonical reproducible EE/system miss
  checkpoint and full-machine replay remain unimplemented.
- Commercial snapshots and generated callbacks remain ignored local artifacts;
  the repository contains the tools and synthetic tests.
- Passing the synthetic tests or displaying game logos/menus does not establish
  gameplay, save correctness, rendering/audio fidelity, 60 FPS, Android execution
  or universal ISO conversion.

## Checks

`nexo_ee_aot_tests` exercises version replacement, owned source identity, interior
entries, malformed footprints and strict runtime call misses. The extractor tests
compile a synthetic DSO and corrupt its ranges. `nexo_ee_bank_catalog_tests`
executes two generated native arithmetic/JR/delay-slot variants, checks independent
interior entry, rejects stale code, and checks deterministic generation, recovered
binding agreement, source hashes and unsafe manifests. Headless isolation tests
cover removing an inherited diagnostic driver in the owned child process.

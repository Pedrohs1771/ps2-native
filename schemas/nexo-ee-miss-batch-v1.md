# Offline EE miss batch V1

This laboratory command consumes bounded captured EE misses and extends an
existing finite native catalog. It derives roots and bytes from each capture;
there is no address list or title-specific repair rule. It is preparation for
the autonomy work in README sections 22 and 31, not strict approval or a proof
of universal closure, semantics, publication or complete machine replay.

`lab/admit_ee_misses.py --catalog DIR --capture RECORD ... --generator TOOL
--output FRESH_JOB` prepares at most 16 complete records before modifying the
catalog. Any invalid preparation rejects the whole batch. Duplicate bank
identities retain the existing case. The existing catalog publisher verifies
source identities, preserves all prior bank C++ sources and their timestamps,
and publishes its manifest last. The game never runs this command or a compiler.

Older catalogs need one bootstrap with `--case DIR` for every previous case.
New catalog manifests own `case_inputs`: per-bank relative directories under
`ee_cases/`, with snapshot and metadata SHA-256. Directory names contain bank
and metadata hashes. Reads reject symlinks, wrong paths, dimensions, hashes and
incomplete ledgers. New directories are published before the manifest; existing
input copies are immutable. They are laboratory artifacts and remain ignored.

`case_producers` retains a prepared case's original producer identity across
later extensions. It applies only to an already verified bank source; a foreign
new case cannot reuse that exception. Every retained callback source must still
regenerate exactly. Producer hashes identify claimed provenance, not authenticity
or semantic correctness. Build/compiler/header identities need separate evidence.

The fresh job report records input/output identities, prepared/duplicate/new bank
counts and terminal status. Status is `EXTENDED_LABORATORY`,
`NO_NEW_BANKS_LABORATORY` or `FAILED`; `strict_approval`, `closure_proved` and
`complete_machine_checkpoint` remain false. An interruption leaves incomplete
artifacts that must pass identity checks before reuse; there are no concurrent
producer/build guarantees. Compilation and game exploration are separate steps.
`prepared_new_banks` counts candidates; `new_banks` is set only after successful
extension and `manifest_published` records that terminal manifest step. A failed
preparation therefore reports zero published banks. Bootstrap cases must match
exactly the existing catalog, and cannot add unobserved cases through that option.

Example after the one-time bootstrap:

```sh
python lab/admit_ee_misses.py --catalog /path/to/catalog \
  --capture /path/to/ee-miss-000001 --capture /path/to/ee-miss-000002 \
  --generator build/ps2xRecomp/ps2_native_overlay --output /path/to/fresh-job
cmake --build build --target ps2_runtime ps2_ee_compiled_catalog --parallel 4
```

The build command assumes the existing selected native catalog configuration.
CMake validates the compiled source/dependency-plan hashes; owned input validation
belongs to the offline publisher. Neither mechanism authenticates the capture or
proves semantic cache correctness. A successful batch does not approve a package.

Tests must cover invalid all-or-nothing preparation, duplicate inputs, automatic
reuse of owned cases, producer migration lineage, unchanged bank timestamps,
corrupt/escaping/symlinked owned inputs, output conflicts and reported failure.
Actual game execution and every final qualification gate remain separate.

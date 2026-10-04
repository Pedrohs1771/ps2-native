# PS2Native

**Experimental PS2 recompilation tools with automatic offline EE code recovery.**

PS2Native develops a path from a user-supplied PlayStation 2 disc image to a host application: inspect, analyze MIPS code, emit C++20, build, and replay. When execution reaches an uncovered EE entry, the conversion tool can capture the missing code, validate it, compile an additional native bank, relink, and replay without hand-written addresses or per-title patches.

Built on [PS2Recomp](https://github.com/ran-j/PS2Recomp), maintained by [Pedrohs1771](https://github.com/Pedrohs1771), licensed under [GPLv3](LICENSE).

[Review and reproduce](docs/REVIEWER_GUIDE.md) · [Roadmap](ROADMAP.md) · [Contribute](CONTRIBUTING.md) · [Português](README.pt-BR.md)

## What you can evaluate today

- **Offline recovery and reuse:** project-owned MIPS fixtures exercise capture → C++ generation → native compilation → relink → replay. Two unseen functions produce `123`, including delay slots. A second conversion reuses both cases with zero additional recovery rounds. Thread entry and KSEG0/KSEG1 cases are included.
- **Guarded execution:** native banks check instruction bytes. Cache identity includes the input image, generator, and source revision. Incomplete captures, repeated misses, and failed builds remain failures.
- **Inspectable conversion:** the Python CLI records tool identities, source/build inputs, package hashes, diagnostics, and conservative translation assessments.
- **Subsystem work:** EE scheduling and callbacks, IOP native bank generation and RPC, VU/VIF replay, GS presentation, and isolated audio diagnostics have source and regression tests.

Start with the [reviewer guide](docs/REVIEWER_GUIDE.md). Its first checks require Python and CMake, without games, BIOS, or a graphics session.

## Current boundary

| Area | Evidence | Still open |
|---|---|---|
| Offline EE recovery | Executable synthetic recovery/reuse tests | Complete coverage of arbitrary programs |
| Commercial replay | Private experiments report animated intros and nonzero PCM in one title | **0 of 5 menus qualified**; navigation, audio fidelity, controls, and gameplay |
| Native execution | EE/IOP generated-code paths and VU laboratory tests | Integrated, qualified EE + IOP + VU AOT across a real-title route |
| Graphics and timing | CPU GS and optional Vulkan backend; replay tooling | Sustained performance and verified frame cadence |
| Packaging | Experimental desktop pipeline; Android staging | An uninterrupted cold conversion and a qualified standalone game |

A build, a logo, a test count, or a replay without a new miss does not establish compatibility. The default IOP diagnostic interpreter is enabled; optional native configurations must be audited explicitly. Universal compatibility and sustained 60 FPS are not demonstrated.

Commercial observations are historical reports, not a downloadable public demonstration. Raw captures, game-derived code, packages, and disc contents remain private. Public demonstrations use project-owned code.

## Quick source checks

Requires Python 3.10+ and CMake 3.21+ on `PATH`:

```sh
python3 -m tools.ps2native --help
python3 -m unittest discover -s tools/ps2native/tests
python3 -m unittest lab.tests.test_ee_data_families lab.tests.test_ee_family_catalog lab.tests.test_prepare_ee_family_batch lab.tests.test_ee_context_inventory
```

For a C++20 build independent of the graphics runtime:

```sh
cmake -S ps2xIOP -B build-iop -DCMAKE_BUILD_TYPE=Release -DPS2X_IOP_BUILD_TESTS=ON -DPS2X_IOP_BUILD_LAB=ON
cmake --build build-iop --parallel 4
ctest --test-dir build-iop --output-on-failure
```

The full native EE recovery demonstration has additional Linux dependencies and must run without skipped tests. See [the exact commands and assertions](docs/REVIEWER_GUIDE.md#native-ee-recovery-demonstration).

## Repository map

| Directory | Purpose |
|---|---|
| `tools/ps2native/` | Conversion CLI, integrity checks, offline adaptation, private cache |
| `ps2xAnalyzer/`, `ps2xRecomp/` | Analysis and C++20 generation |
| `ps2xRuntime/`, `ps2xIOP/` | EE runtime, devices, scheduler, IOP execution |
| `lab/`, `schemas/` | Synthetic fixtures, capture/replay tools, contracts |
| `ps2xTest/` | C++ regression suite |
| `docs/` | Reviewer guide and dated engineering notes |

The [CLI reference](tools/ps2native/README.md) describes package construction. [Beta v0.1](docs/BETA_V01.md) defines menu acceptance gates. Dated Portuguese notes and `README_GERAL_COMPLETO.md` preserve development history; newer receipts take precedence over older snapshots.

## Source and inputs

Source distribution excludes games, ISOs, BIOS, extracted commercial ELFs/IRX modules, RAM/VRAM captures, memory cards, and generated game code. Optional prebuilt Vita modules and fonts without documented redistribution provenance are excluded as well. Bring only inputs you are authorized to use and keep them outside version control. See [provenance](docs/PROVENANCE.md).

[Issues](https://github.com/Pedrohs1771/ps2-native/issues) and pull requests are welcome. Small synthetic reproductions are especially useful for the next milestone: correct menu progress, causal input, audio, and timing.

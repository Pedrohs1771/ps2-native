# Source, dependencies, and inputs

PS2Native derives from [ran-j/PS2Recomp](https://github.com/ran-j/PS2Recomp). Preserve upstream history, attribution, and the existing [GPLv3](../LICENSE). No independent-invention claim for PS2 static recompilation is made.

Third-party dependencies fetched by CMake keep their own licenses. Optional backends/reference integrations are not evidence that those components were rewritten or independently verified here. Consult upstream licenses and [reference notes](REFERENCIAS_RECOMP.md) before copying code or distributing binaries.

Public tests use project-owned instruction/data fixtures. Commercial experiments are documented separately. Source-only preparation removes extracted game payloads in `fixtures/`, commercial release assets, and retained repository references before publication. Optional prebuilt Vita modules and the two bundled fonts are excluded because this checkout does not document their redistribution provenance. Studio has a default-font fallback; Vita packaging diagnoses absent local modules and remains unqualified.

Keep authorized inputs and all game-derived outputs outside version control. `tools/check_public_source.py` checks common payload types and private input directories. It is a guard, not license adjudication, and does not erase external clones or cached Git objects.

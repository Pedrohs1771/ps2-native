# Contributing

Start with the [reviewer guide](docs/REVIEWER_GUIDE.md) and [roadmap](ROADMAP.md). Report expected/observed behavior, source revision, host/tool versions, the failing command, diagnostics, and a small reproduction through GitHub issues.

For semantic changes, demonstrate a project-owned reproduction that fails before and passes after. Keep fixes scoped to a shared instruction/device contract. Do not add per-title addresses, success stubs, or relaxed guards to conceal misses.

Run tests for the changed subsystem. Python changes use the relevant `tools/ps2native/tests` or `lab/tests`; IOP changes can use the standalone CMake/CTest commands. Runtime changes require the relevant C++ tests and fixtures. In a git checkout, run `git diff --check` before submitting.

Do not attach or commit games, BIOS, commercial ELFs/IRX, assets, memory dumps/cards, generated game code, or packages with disc content. Reduce failures to instructions/data you may redistribute. Preserve GPLv3 and upstream attribution; identify the origin and license of newly copied code/assets.

Explain what the patch proves and which gates remain open. Passing tests must not be reported as qualified menus or gameplay.

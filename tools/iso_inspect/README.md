# PS2 ISO Inspector

`ps2iso-inspect` is a standalone C++20 command-line tool for inspecting a local PS2 ISO9660/Joliet image. It reads disc metadata, `SYSTEM.CNF`, and ELF records for identification and inventory. Its explicit `extract` subcommand copies the disc tree to a new host directory. The input image is read-only.

## Build and run

Requires CMake 3.16+ and a C++20 compiler. There are no third-party runtime dependencies.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/ps2iso-inspect /path/to/game.iso
```

The CLI prints the volume ID, block size, every file and directory path, the SHA-256 of the input image, and the PS2 boot metadata. It locates the executable named by `BOOT2`, reports its ELF32 little-endian MIPS entrypoint and `PT_LOAD` segments, and prints the boot ELF SHA-256.

For machine-readable output, use `ps2iso-inspect --json /path/to/game.iso`. The stable top-level contract is `schema_version: 1` with `image`, `entries`, `elf_inventory`, and `boot` objects. `boot.system_cnf` contains `path`, `boot2`, `version`, `video_mode`, and `region_inferred`. `boot.elf` contains `path`, `format`, `byte_order`, `machine`, `machine_id`, `entrypoint`, `sha256`, and `load_segments`. For PS2 boot ELFs, `byte_order` is `little-endian`, `machine` is `MIPS R5900`, and `machine_id` is the ELF `EM_MIPS` numeric value `8`. Segment offsets and addresses are fixed-width hexadecimal strings; sizes, flags, and alignment are JSON numbers. Missing optional `VER`/`VMODE` values are empty strings. Parse failures write a human-readable message to stderr and return exit code 1; usage errors return 2.

`elf_inventory` has its own schema version. It scans ELF magic in all ISO files, records class, byte order, type, machine, flags, entrypoint, loadable ranges, SHA-256, and all ISO paths for identical contents. ELF32 little-endian MIPS `ET_EXEC` files outside `BOOT2` are secondary executable candidates, not assumed EE programs; their subsystem must be identified before recompilation. MIPS `ET_REL` `.IRX` records remain IOP module candidates. Inventory is discovery metadata and does not claim that secondary code is compiled or executable in the native runner.

To copy the ISO9660 tree to a new host directory, run `ps2iso-inspect extract /path/to/game.iso /path/to/new-directory`. The destination must not already exist, and its parent directory must exist. This subcommand removes terminal `;` plus numeric ISO version suffixes from physical host names (for example, `SYSTEM.CNF;1` becomes `SYSTEM.CNF`) while the inspector JSON keeps the original ISO paths. It rejects unsafe components and ASCII case-insensitive collisions after normalization before creating the destination. Extraction writes regular files and directories only; it never creates symlinks. A failed extraction removes the newly created destination tree.

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Tests construct synthetic ISO images in the system temporary directory; no commercial disc image or game asset is stored in this project.

## Interpretation notes

- `VER` and `VMODE` are reported as written in `SYSTEM.CNF`.
- Region is a heuristic based on known PS2 serial prefixes in the `BOOT2` executable name (`SLPM`/`SLPS`/`SCPS`/`SCAJ`, `SLUS`/`SCUS`, `SLES`/`SCES`). The label explicitly says it is inferred; unknown prefixes remain unknown.
- Joliet supplementary volume descriptors are supported and preferred for path names when present. Primary ISO9660 identifiers are percent-escaped when they contain non-printable bytes.
- The ISO SHA-256 covers the entire input file, including any bytes after the volume's declared end. This fingerprints the actual image file supplied to the CLI.
- Multi-extent directory records are rejected with a clear error. Raw 2352-byte CD tracks, CUE/BIN sets, and UDF-only images are outside this component's current scope. The boot ELF must be ELF32 little-endian MIPS `ET_EXEC`; other ELF variants can appear in the inventory as unsupported candidates.
- This tool identifies and reports the boot ELF. It does not decompile/recompile the game or translate PS2 graphics, audio, input, I/O, or hardware behavior to another platform.

## Input bounds

The parser validates descriptor signatures, both-endian ISO fields, volume and extent ranges, directory record lengths, path depth, entry count, and ELF program-header/segment bounds before reading. It limits a single directory read to 64 MiB, cumulative directory data to 256 MiB, the path list to 64 MiB, the entry count to 250,000, the `SYSTEM.CNF` read to 64 KiB, and the ELF program-header table to 16 MiB. Hashing and extraction file copies are streamed in fixed-size chunks.

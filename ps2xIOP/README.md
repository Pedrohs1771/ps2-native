# ps2xIOP

`ps2xIOP` runs original IRX modules on an R3000A interpreter, with a virtual
IOP kernel providing imports without a PS2 BIOS. The C++20 static library
`ps2_iop` / `ps2x::iop` is linked into `ps2xRuntime`.

There is now an initial native AOT path with an internal laboratory bank ABI.
Constructing the subsystem with an `IopNativeProgram` requires compiled entries;
missing or changed instructions fail with `UNSEEN_CODE` and never use the
interpreter. `PS2X_IOP_ENABLE_INTERPRETER=OFF` excludes the diagnostic interpreter
from the library and makes the default constructor require a native bank too.
The runtime defaults to the diagnostic configuration. An experimental native
IOP catalog option has passed original HKSIF startup through its actual adapter;
complete commercial game execution remains unqualified.

## Execution policy

Game-specific IOP code executes from IRX modules. There is no game-profile
selection or native profile-plugin loader. A physical IRX RPC server is
authoritative for its SID.

Generic HLE services remain available when no loaded IRX provides an endpoint:

| Service | SID | Activation | Implemented operations |
| --- | --- | --- | --- |
| LOADFILE | `0x80000006` | IOP boot/reset | Version query and path based module load |
| MCSERV | `0x80000400`, `0x80000480` | Recognized module load | Memory-card RPC subset |
| LIBSD | `0x80000701` | Recognized module load | Runtime audio dispatch |
| DBCMAN | `0x80001300` | Recognized module load | Version query |

LOADFILE is part of the no-BIOS boot profile and is registered whenever the
IOP resets. Its module-load RPC delegates to the same IOP module manager used by
the direct runtime calls. The other HLE services are dormant before module
load and after reset or the final module stop. Unsupported LOADFILE operations
remain unhandled and appear in its counters; this is not a full LOADFILE
protocol implementation yet. Unknown modules fail to load; unknown RPC SIDs
remain unhandled.
Games previously using TSNDDRV, CRI DTX, CLFILE, SOUND or SDRDRV profiles now
require their IRX modules and support for the imports and hardware they use.

## Lifecycle and transport

- `reset()` clears loaded modules, HLE service state and emulator state.
- `loadModule(...)` / `loadModuleBuffer(...)` load and start an IRX.
- `stopModule(...)` releases a module and its owned state.
- `runEeCycles(...)` advances the IOP from EE cycle accounting.
- `selectRpcAbi(...)`, `handleRpc(...)` and `onSifTransfer(...)` connect SIF transport.

IOP RAM is separate from EE RAM. The transport copies data through the IOP
memory accessors; SIF notifications do not mirror bytes into equal-numbered EE
addresses. `RpcResult` describes completion and dispatch actions for the runtime.

Link with `target_link_libraries(my_runtime PRIVATE ps2x::iop)`. The public API
is [iop_subsystem.h](include/ps2x/iop/iop_subsystem.h); `PS2Runtime` owns its
subsystem and host adapter.

## Diagnostics and tests

`debugSnapshot()` exposes emulator cycle/instruction counts, loaded module,
thread and RPC-server counts, generic service metrics and load diagnostics.
It also reports native instructions, interpreted instructions, and whether a
persistent native fault is present (zero or one). Reset clears those counters
and failures while retaining the configured compiled bank.
The runtime debugger renders these in the **IOP/SIF** tab.

Build standalone tests with:

```sh
cmake -S ps2xIOP -B out/build/iop-tests -DPS2X_IOP_BUILD_TESTS=ON
cmake --build out/build/iop-tests
ctest --test-dir out/build/iop-tests --output-on-failure
```

The suites cover IRX execution, RPC, imports, version resolution and generic
HLE compatibility. `ps2x_tests` also covers runtime SIF RPC/DMA integration.

## Native AOT laboratory

```text
already relocated RAM words → offline C++ generation → host compiler
                            → compiled bank → native dispatcher
```

The converter is `lab/generate_iop_bank.py`. It covers every aligned word in
the supplied RAM range, including interior entries, with fixed instruction
parameters. The runtime reads the live word only to verify its identity.
Parameterized kernels also extract admitted immediate/jump operands after the
full bound-word guard; opcode and register fields remain compiled constants.
The dispatcher owns its entry directory; callbacks must remain loaded while
the subsystem uses the bank. Its ABI is internal and not installed for third
party use yet.

`PS2X_IOP_BUILD_LAB=ON` builds an offline IRX loader frontend. Its output can be
passed to the converter with `--loaded-module`, so the base and complete bank
range are taken from loader metadata. A native startup probe accepts generated
bank C++ through `NEXO_IOP_BANK_CPP`; the diagnostic startup probe is a separate
target. Both record their scope and reject unsupported external host operations.
See [the laboratory guide](../lab/README.md) for the complete commands.

`--family-catalog` generates a shared operation pool and per-image directories for
multiple IRX files. Set `NEXO_IOP_BANK_MANIFEST` instead of `NEXO_IOP_BANK_CPP` to
link it. CMake verifies source/header hashes before building. Stable hash shards
and unchanged-file preservation support incremental compilation. The root
`PS2X_FAST_ITERATION` profile compiles the IOP library and catalog with
`-O1 -fno-lto`, with IPO disabled.

At the root, `PS2X_RUNTIME_NATIVE_IOP=ON` requires a compiled catalog and
`PS2X_IOP_ENABLE_INTERPRETER=OFF`. The laboratory runtime probe loads modules
through the real adapter without initializing a window. This option covers IOP
only; EE/VU, complete services and final game qualification are separate gates.

The original bank binds absolute physical addresses. The optional IRX family
frontend adds full source-image identity and binding across loader-selected
bases, with static operation/register fields and bound immediate/jump operands.
Module unload/reset retire bindings; the full kernel lifecycle on module
replacement remains unqualified. The initial semantics are specialized from
the existing CPU model: differential agreement
does not certify hardware semantics or timing. Native service contracts also
retain the current qualification limits. See
[the V0 contract](../schemas/nexo-iop-aot-v0.md) for commands and remaining gates.

The standalone native tests exercise synthetic IRX startup, load and branch
checkpoints, interior entries and aliases, code writes, empty/invalid banks,
unresolved imports, incomplete relocations, and startup budget exhaustion.
Diagnostic builds additionally compare the emitted operations to the identified
CPU model. The strict build excludes its generic instruction-execution symbol.

Native import dispatch also guards the stub, ordinal and table dependency words
against admitted identities before invoking a service. Empty banks cannot admit
known imports. Bound data metadata remains non-executable. The actual runtime
laboratory probe can load a module sequence and advance the IOP scheduler between
loads; a bounded original boot sequence observed threads and registered RPC
servers with native operations and complete RAM/model agreement. Service fidelity,
guest warning causes, canonical state and complete game execution remain open.

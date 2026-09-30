# ps2xIOP

`ps2xIOP` runs original IRX modules on an R3000A interpreter, with a virtual
IOP kernel providing imports without a PS2 BIOS. The C++20 static library
`ps2_iop` / `ps2x::iop` is linked into `ps2xRuntime`.

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
The runtime debugger renders these in the **IOP/SIF** tab.

Build standalone tests with:

```sh
cmake -S ps2xIOP -B out/build/iop-tests -DPS2X_IOP_BUILD_TESTS=ON
cmake --build out/build/iop-tests
ctest --test-dir out/build/iop-tests --output-on-failure
```

The suites cover IRX execution, RPC, imports, version resolution and generic
HLE compatibility. `ps2x_tests` also covers runtime SIF RPC/DMA integration.

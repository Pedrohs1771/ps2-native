# Native EE data-family synthesis, laboratory v0

This frontend implements a restricted conversion step of README §12.6. It
produces C++ ahead of time using the existing EE instruction and delay-slot
emitters. It does not approve closure, discover a producer invariant, prove
fetch/cache/write/alias semantics or integrate a new dispatcher into the game.

## Converter interface

```cpp
std::string ps2recomp::generateNativeDataFamily(
    std::span<const uint32_t> words,
    std::span<const uint32_t> masks);
```

Each input has 1..128 numeric words and the counts must match. Word values
represent a fixed candidate structure, not a program interpreted at runtime.
Masks may be `0xffffffff` or `0xffff0000`. The latter is allowed only for a LUI
with zero source field or a SW. Opcodes, register selection and control encodings
remain exact. Parameters are extracted as uint16 values; SW emission explicitly
sign extends its value through int16/int32 before 32-bit address addition.

Initially, supported regions are linear integer/ordinary memory instructions,
optionally ending in one register JR/JALR and its complete architectural slot.
Local indirect-target specialization, conditional/direct branches, syscalls,
other unsupported data/device operations and unsupported reserved fields are
explicitly rejected. Rejection is an open synthesis obligation, not a success
stub or a request to interpret the rejected bytes.

The CLI consumes two bounded little-endian files (at most 512 bytes each):

```sh
build/ps2xRecomp/ps2_native_data_family words.bin masks.bin fresh-family.cpp
```

It needs an exclusively owned output path and rejects an existing output or
symlink. The tool belongs in the converter. It is not included in a delivered
native game's guest execution path.

## Generated function

```cpp
void ps2native_data_family(uint8_t* ram, R5900Context* context,
                          PS2Runtime* runtime, uint32_t familyBase);
```

The generated wrapper rejects null resources, misaligned/outside physical RAM
bases, PCs outside the complete region, misaligned PCs and pending architectural
delay context before executing effects. It checks every expected guarded RAM word
and extracts the exact live low-16 data fields into a fixed-size temporary array.
The original guest bytes are preserved. Guard mismatch throws `invalid_argument`.

The following body contains fixed native operations. There is no runtime guest
decoder, opcode dispatch loop, guest-to-host compiler or new executable memory.
The comparison loop in the wrapper only checks identity and loads typed data.
Canonical instruction addresses become `ADD32(family_base, offset)` expressions
at precise PC, architectural delay, branch-source and link/return locations.
Every instruction has a normal resume label; an independently entered terminal
slot executes without replaying its preceding branch or link update. Pending
architectural delay context remains unsupported rather than being reclassified.

Masks alone are not a proof that a parameter is unrelated to control or code
writes. Store destinations may alias instructions; caller targets and devices
may change the world. RAM guard success does not prove instruction-cache fetch
identity. Matching observations does not establish universal domains, reachable
caller coverage, scheduler/device timing or independent PS2 correctness.

## Evidence and remaining integration

The synthetic build-time fixture compiles three families and 36 concrete
conservative reference regions. Execution comparisons cover all **168 normal
entries**, including signed-immediate boundaries, relocated PCs, saved-frame
register restoration, JALR link-before-slot behavior and standalone slots. They
compare all fields of the identified EE context codec and every byte of 32 MiB
RAM. Guard tests verify rejected contexts/structural bytes have no effects.

The fixture generator also accepts an exact proposal from the candidate report:

```sh
python lab/tests/generate_ee_data_family_fixture.py \
  build/ps2xRecomp/ps2_native_data_family build/ps2xRecomp/ps2_native_overlay \
  observed-fixture.cpp --candidate-json candidates.json --shape SHAPE_SHA256
```

The eight-word saved-frame suffix found at eight captured locations, including
`0x184f474`, passed **64 normal-entry comparisons** using one family function and
the eight observed byte variants/bases. Contexts are constructed test inputs;
this is not a complete replay of the missing-entry game checkpoint. It is not
independent hardware validation, since both paths share EE semantic emitters.

All 34 prior concrete bank sources were regenerated with both the archived and
new converter and remained byte-identical. This guards against accidental
default-generator changes; it does not certify semantic cache correctness.

Next: create a bounded immutable family catalog and runtime admission adapter,
including revalidation at invocation and precise context/entry ownership; cover
the unchanged prefix and unsupported structures; trace the materializer and
establish admissible parameter domains; validate fetch/writer/alias/timing and
independent fidelity. Game dispatch, campaign, native VU, Android and sustained
60 FPS remain unqualified. Source generation or model equality alone cannot
authorize a strict native package.

# Native EE data-family synthesis, laboratory v0

This frontend implements the project's restricted offline family conversion. It
produces C++ ahead of time using the existing EE instruction and delay-slot
emitters. It does not approve closure, discover a producer invariant, prove
fetch/cache/write/alias semantics. Its separate experimental catalog integration
is described in [finite family admission](nexo-ee-family-catalog-v0.md).

## Converter interface

```cpp
std::string ps2recomp::generateNativeDataFamily(
    std::span<const uint32_t> words,
    std::span<const uint32_t> masks);
```

Each input has 1..128 numeric words and the counts must match. Word values
represent a fixed candidate structure, not a program interpreted at runtime.
Masks may be `0xffffffff` or `0xffff0000`. The latter is allowed only for one of
the 20 data classes in `ps2_native_data_operands.h`: LUI(rs=0), ADDIU, SLTI/SLTIU,
ANDI/ORI/XORI and ordinary integer loads/stores. Opcodes, register selection and control encodings
remain exact. Parameters are extracted as uint16 values; SW emission explicitly
sign extends its value through int16/int32 before 32-bit address addition.

Supported regions are linear integer/ordinary memory instructions and fixed
FPU instructions emitted by the existing concrete translator, optionally ending
in one J/JAL, register JR/JALR, integer conditional branch or BC1F/T/FL/TL and its
complete architectural slot. Conditional destinations, links, internal targets
and loop/checkpoint locations use relative PCs. Branch-likely and REGIMM link
forms retain the existing conservative slot/link policy.
Direct J/JAL preserve their fixed target bits and construct the destination from
the actual architectural PC high nibble. A destination is mapped to a local
label only after comparison against the actual relocated region; local
backedges retain checkpoints. External destinations use the existing runtime
directory. JAL links relocate before its slot. No jump target
field is a parameter in this profile.
FPU data encodings, including LWC1/SWC1 offsets, remain fully exact: they are
not additional parameter classes. COP1 register moves reject reserved low bits;
CFC1 admits FCR0/FCR31 and CTC1 admits FCR31 only. Unsupported formats/functions
are rejected by the existing translator's reserved-instruction check. A COP1
branch not recognized as valid control cannot pass as a no-op data instruction.

Local indirect-target specialization, other coprocessor branches, syscalls,
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

That initial fixture is now expanded to **47 structures / 387 concrete fixtures /
1,518 normal-entry comparisons**: all 20 data classes at signed/unsigned
boundaries, both branch outcomes, branch-likely annulment, REGIMM links, external
positive/negative targets, internal backedges, variable decrements and
parameterized ADDIU-to-zero slots/body operations. A new slot test first failed
because canonical zero erased the concrete emitter's delay metadata; emission
now retains its parameter-dependent decision. Every
comparison checks the identified context and 32 MiB RAM against the shared
emitter reference. This remains a model regression, not independent PS2
equivalence or whole-machine replay.
Direct J/JAL fixtures also compare links, ordinary slots and independently
entered slots at three physical bases and four signed operand boundaries.
Generation checks cover an absolute target of zero without a false local jump.
Actual local J/JAL destinations also exercise a slot entered a second time as
an ordinary instruction, including an observable JAL link update.

The fixed FPU expansion brings the synthetic corpus to **97 structures / 537
concrete fixtures / 2,832 normal-entry comparisons**. It covers loads/stores,
register/control transfers, the supported single-precision operations, CVT.S.W
and all four FPU branch conditions. Inputs include ordinary values, signed zero,
subnormal bits, infinities and a quiet NaN; condition flags exercise both branch
outcomes and likely annulment. These remain comparisons with a shared concrete
emitter and the project context/RAM model, not independent PS2 FPU fidelity or
timing qualification.

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

The bounded immutable catalog and invocation recheck now exist as an opt-in
laboratory experiment. The first game run passed its previous initializer miss
and stopped at the next uncovered callback; full code coverage is still open.

Next: cover whole loaded structures and unsupported control flow in batches;
trace the materializer and
establish admissible parameter domains; validate fetch/writer/alias/timing and
independent fidelity. Complete game dispatch, campaign, native VU, Android and sustained
60 FPS remain unqualified. Source generation or model equality alone cannot
authorize a strict native package.

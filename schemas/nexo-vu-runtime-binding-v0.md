# NEXO VU1 native runtime binding, laboratory V0

`bindNativeVu1(runtime, programs)` attaches a finite collection of previously
compiled VU1 banks to an initialized, windowless or normal memory session. It
uses the existing VIF MSCAL/MSCALF and MSCNT callbacks. The runtime and its memory
layout remain unchanged. Conversion and guest-code compilation are prohibited
in these callbacks.

## Ownership and installation

Bank identities, descriptors and entry arrays are copied into immutable owned
storage. Compiled function pointers must remain loaded until the runtime is
destroyed. Empty collections, non-VU1 units, invalid bank sizes and duplicate
byte identities are rejected before a previous installation is replaced.

The binding synchronizes initialized core subsystems before installing its
callbacks. Those callbacks own the bank collection for the memory session.
An explicit host memory reinitialization and subsequent core rebinding require
installation again. This laboratory adapter does not change the runtime's
unbound reference behavior or establish a strict final-package build.

## Selection and execution

Every invocation compares the current complete microcode bytes to a compiled
bank identity before modifying VU state. A previous candidate can be checked
first, but neither a pointer nor the code-generation counter substitutes for
the byte comparison. Raw mutable code views must not cause stale selection.
Duplicate identities are ambiguous even if function pointers happen to match.

An unknown identity throws `UNSEEN_CODE` before fresh-call normalization. There
is no reference interpreter fallback after installation. VIF may already have
latched TOP/ITOP and updated its own flags when this host contract failure is
raised; it is not a guest exception or a transactional rollback of the whole
VIF operation.

MSCAL/MSCALF use native fresh-call normalization; MSCNT preserves the current
clock and pending pipelines. Both propagate FBRST's D/T enable bits and publish
the resulting stop flags in the CPU-visible VPU status. TOP/ITOP come from the
normal VIF parser. A code change between calls selects the corresponding bank
without resetting continuation state.

`nexo_vu_runtime_replay` reconstructs a normalized VU checkpoint and synthesizes
MSCNT for integration. TOP/ITOP and FBRST D/T enable inputs come from that
checkpoint. It requires the callback's exact 65,536-cycle budget. TOP/ITOP values
outside VIF's 10-bit callback domain are rejected instead of silently masked.
Machine construction, bank ownership, file input, checkpoint restoration and
comparison are outside its measured interval. VIF processing, bank selection,
native VU execution and submission observation are inside it.

## Scope and acceptance

Validation uses the real VIF parser and PS2Runtime, canonical VU state and link
traps against generic VU execution. No EE instructions or user window are
needed by the fixtures. The EE lookup fixture contains only null entries.

The current VIF callbacks execute synchronously. Concurrent guest code writes
during a native invocation, fetch visibility beyond this model, VIF/GIF/GS
canonical replay, upload-family closure and final package interpreter absence
remain separate unfinished requirements. Bank identity checks establish which
compiled code is selected; they do not prove equivalence or code closure.

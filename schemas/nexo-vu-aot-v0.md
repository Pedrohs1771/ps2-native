# NEXO VU AOT V0 laboratory contract

V0 compiles a finite, identified microcode bank before execution. Each native
entry binds a static dependency descriptor and operation functions specialized
by compile-time instruction constants. The native scheduler consumes data and
this immutable bank; it does not fetch or decode a guest instruction stream.

The converter frontend may use the current decoder to derive descriptors. It
belongs to a separate target and is excluded from the native execution path.
The first implementation shares the current pipeline/device helpers, while
specializing opcode-dependent numeric/flag helpers as well as upper/lower
operations. Calling a generic `execUpper`, `execLower`, `execute`, `resume` or
interpreter `run` is prohibited on that path.

Preserved state includes writeback ordering, flags, Q/P and resource readiness,
ACC forwarding, upper/lower shadowing, VI branch history, delay slots, D/T/E
termination and XGKICK's future reads. Canonical checkpoints must match the
reference after every tested pause/resume boundary, not just at program end.

Unknown or uncompiled entries emit `UNSEEN_CODE`; there is no interpreter or
runtime guest compiler fallback. The replay adapter checks the captured code
against the bank's immutable byte identity before invoking native execution.
Byte comparison/hash identification is not guest instruction execution.

Address validation checks alignment and `pc <= code_size - 8` before indexing
the table. Adding eight to an untrusted 32-bit PC is not a safe bounds check:
the addition can wrap. The native API rejects this case even when invoked
directly with mutable machine state, outside the canonical checkpoint decoder.

The current laboratory executable uses eight Linux GNU/LLVM link wrappers to
reject generic execution and opcode-dependent FMAC entry points. Tests activate
the wrappers deliberately, then compare native and reference continuations.
Undefined-symbol inspection of the bank and scheduler objects is an additional
regression check. Neither mechanism proves that all interpreter code is absent
from the executable; the prototype links shared legacy runtime components.

Initial approval is `tested_only` against the existing implementation. It does
not establish independent hardware correctness, code closure across uploads,
the complete VIF/GIF/GS boundary, final package interpreter absence, universal
compatibility or sustained speed. Those remain separate gates in the root plan.

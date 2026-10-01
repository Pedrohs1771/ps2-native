# NEXO queued XGKICK state extension, version 2

This extends the current-model canonical VU checkpoint to cover an already
issued second XGKICK waiting for the first PATH1 transfer. It preserves the
second pair's pending Upper result and source operand across suspension.
It does not certify the complete hardware or device scheduling model.

## Encoding and domain

The header, field widths, endianness, CRC algorithm, unit rule, 80 KiB bound,
transactional restore, and original payload order are those of version 1.
The header version is 2. The complete version-1 payload is followed by:

| Absolute offset | Width | Field |
|---:|---:|---|
| 70,249 | 4 | Queued source byte address, u32 |
| 70,253 | 8 | Original issue cycle, u64 |
| 70,261 | 1 | Pending request, bool8, exactly 1 |

The complete envelope is 70,262 bytes. The queued source must be aligned to
16 bytes and below 16 KiB. Its issue cycle cannot exceed the checkpoint clock.
A queued request requires unit VU1 and an active preceding transfer.
It adds no host pointer, cached instruction, or eagerly copied second packet.
The second packet is read later from the supplied VU data memory.

An empty slot has zero address and issue clock, and is encoded using version 1.
Version 2 with pending=false is rejected, keeping one canonical representation
for each supported state. Version-1 restore explicitly gets the reset empty
slot from its fresh candidate; it cannot inherit a destination's pending work.
Unknown versions, trailing fields, malformed booleans, invalid descriptors,
future issue clocks, and checksum failures leave the destination unchanged.

## Scheduling interpretation

The second XGKICK latches its source at issue. Its Upper instruction issues in
the same pair, while the previous packet continues transferring. The next pair
waits for the preceding transfer's completion. On completion, the slot is
cleared and its request starts the next transfer; later pairs may issue while
that transfer runs. Packet order is preserved across arbitrary budget pauses.

The basis for this ordering is the hardware manufacturer's VU User's Manual
version 6.0, section 3.4.9 and the XGKICK instruction description, pp.52/196.
This source describes a stall on the following pair. [Manual mirror](https://studylib.net/doc/25815876/vuusersmanual.158394566).
Exact physical transfer rate, blocked GIF arbitration, E-termination timing,
and concurrent device scheduling remain separate obligations.

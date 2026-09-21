#!/usr/bin/env python3
"""Compute the datagram budget for Outpost Commander -- both directions.

The design's figures are arithmetic on the record layout and the entity counts.
This reproduces that arithmetic so a proposal is costed rather than estimated
(AGENTS.md section 6), and so the same numbers come out every time.

    python3 budget.py                          # the snapshot as it stands
    python3 budget.py --add-bytes 1            # cost of one more byte per entity
    python3 budget.py --players 4 --modules 4  # a different match shape
    python3 budget.py --audit-fields           # what each field spends vs what it draws
    python3 budget.py --upstream               # the command packet, host-bound
    python3 budget.py --all                    # every section, for a wire-format change
    python3 budget.py --reshape --index-bits 12   # lever 0, columnar and table candidates
"""
import argparse
import math
import os
import sys

# The record, field by field, as Design/TechnicalDesign.md section 4 defines it.
RECORD = [("identity", 2), ("position", 4), ("heading", 1),
          ("hull", 1), ("design identity", 1), ("flags", 1)]

# Fixed header, then one block per player. The first six bytes are the TRANSPORT header and their
# split is pinned by NeuronCore/PacketHeader.h rather than by this table -- a 2-byte sequence that
# buys the two fragment fields, where this model previously spent all four on the sequence. Same
# fourteen bytes either way, which is why nothing below moved when M0.2 landed.
HEADER_FIXED = [("version", 1), ("type", 1), ("sequence", 2),
                ("fragment index", 1), ("fragment count", 1), ("tick", 4),
                ("entity count", 2), ("player count", 1), ("removal count", 1)]
HEADER_PER_PLAYER = [("credits", 4), ("last command applied", 2),
                     ("building design", 1), ("build progress", 1)]

# After the removal list, not in the header: ADR-004 puts the fire events behind a count byte at
# the very end, so the header's shape does not change on the day weapons arrive. THIS BYTE WAS
# MISSING FROM THIS MODEL until M0.9's encoder measured a snapshot one byte larger than the design
# said -- TechnicalDesign.md section 4 specified it one line below the table that left it out.
TRAILER = [("fire event count", 1)]

# Field-width audit. "wire" is what the record spends today; "draws" is the number of bits the
# client actually needs to draw the field. Those are different questions, and only the second
# one sizes a wire field -- width on the wire is decoupled from width in the simulation, which
# keeps its own precision regardless (R16). A reserved field is one deliberately widened for a
# decision already taken; its slack is not recoverable and is excluded from the total.
#                field              wire draws reserved why that many bits are enough
FIELD_AUDIT = [("identity",           16, 16, False, "correctness, not display: the client matches records across frames"),
               ("position x",         16, 16, False, "16,384 units over 65,536 steps is an exact fit at 1/4 unit"),
               ("position y",         16, 16, False, "as above -- there is no slack to find here"),
               ("heading",             8,  7, False, "7 bits is 2.8 deg and reads smooth; 6 steps visibly on a turning ship"),
               ("hull",                8,  4, False, "a hull BAR resolves about 16 levels; the percent is a number nobody reads"),
               ("design identity",     8,  8, True,  "RESERVED: widened on purpose for R24, research and the designer"),
               ("flags",               8,  7, False, "team 2 + state 3 + cargo 2 = 7 in use, 1 spare")]

# Upstream. Section 5 states the command packet in prose rather than a table -- a type, a target,
# the selected identities, a per-player sequence, retransmitted every packet until the snapshot
# acknowledges it. This is that prose costed. If section 5 ever gains a table, move this to match.
COMMAND_HEADER = [("version", 1), ("type", 1), ("player identity", 1), ("command count", 1)]
COMMAND_FIXED = [("sequence", 2), ("order type", 1), ("target point or entity", 4),
                 ("selection count", 1)]
IDENTITY_BYTES = 2

# Payload available to UDP under each path assumption, ascending. The design pins 1232: the
# IPv6 minimum-MTU payload exactly, which is the largest any conformant IPv6 path must carry
# unfragmented. It keeps no room for an extension header or a tunnel; the 1200 row is what
# keeping that room would cost.
PINNED = 1232
MTU = [("QUIC's floor, IPv6",   1200, "1232 less 32 B of room for an extension header or a tunnel"),
       ("IPv6 minimum MTU",     1232, "1280 - 40 IPv6 - 8 UDP; no room left for encapsulation"),
       ("typical VPN, IPv4",    1372, "1400 - 20 - 8"),
       ("PPPoE, IPv4",          1464, "1492 - 20 - 8"),
       ("LAN Ethernet, IPv4",   1472, "1500 - 20 - 8 -- the MVP's stated target")]


def budget(players, ships, modules, removals, extra_per_entity, extra_header):
    entities = players * (ships + 1 + modules)
    record = sum(b for _, b in RECORD) + extra_per_entity
    header = (sum(b for _, b in HEADER_FIXED)
              + players * sum(b for _, b in HEADER_PER_PLAYER) + extra_header)
    trailer = sum(b for _, b in TRAILER)
    return entities, record, header, entities * record + header + removals * 2 + trailer


def snapshot(a):
    entities, record, header, total = budget(a.players, a.ships, a.modules,
                                             a.removals, a.add_bytes, a.add_header)
    print(f"  {a.players} players x ({a.ships} ships + 1 station + {a.modules} modules)"
          f" = {entities} entities")
    print(f"  record {record} B ({sum(b for _, b in RECORD)} + {a.add_bytes} proposed)"
          f" · header {header} B · removals {a.removals * 2} B"
          f" · fire count {sum(b for _, b in TRAILER)} B")
    print(f"  SNAPSHOT {total} B   at {a.rate_hz} Hz = {total * a.rate_hz / 1000:.1f} KB/s per client\n")

    for name, cap, why in MTU:
        free = cap - total
        frags = -(-total // cap)
        fit = f"ONE datagram, {free:5d} B free = {free // record:3d} entities" if frags == 1 \
              else f"{frags} datagrams -- FRAGMENTS, loss rate roughly x{frags}"
        pin = " <- PINNED" if cap == PINNED else ""
        print(f"  {name:22s} {cap:5d} B  {fit}{pin}")
        print(f"  {'':22s}        {why}")

    if a.add_bytes or a.add_header:
        _, _, _, was = budget(a.players, a.ships, a.modules, a.removals, 0, 0)
        print(f"\n  the proposal costs {total - was} B "
              f"({was} -> {total}), {(total - was) // max(record, 1)} entities of headroom")
    return entities, record, total


def audit_fields(a):
    """What the record spends against what the client draws."""
    entities, record, _, total = budget(a.players, a.ships, a.modules, a.removals, 0, 0)
    print("  field              wire  draws  slack")
    wire = draws = recoverable = 0
    for name, w, d, reserved, why in FIELD_AUDIT:
        slack = w - d
        wire += w
        draws += d
        if not reserved:
            recoverable += slack
        mark = "  --" if reserved else f"{slack:4d}"
        print(f"  {name:18s} {w:4d} {d:6d} {mark}   {why}")
    held = (wire - draws) - recoverable
    note = f", {held} held by a reserved field" if held else ""
    print(f"  {'TOTAL':18s} {wire:4d} {draws:6d} {recoverable:4d}   recoverable bits per entity{note}\n")

    if not recoverable:
        print("  No recoverable slack. Every field is already the width it draws.")
        return
    packed_bits = wire - recoverable
    block_now = entities * record
    block_packed = -(-entities * packed_bits // 8)
    saved = block_now - block_packed
    print(f"  record {wire} bits -> {packed_bits} bits; the record stops being byte-aligned")
    print(f"  {entities} entities: {block_now} B -> {block_packed} B, saves {saved} B")
    print(f"  snapshot {total} B -> {total - saved} B,"
          f" headroom against {PINNED} goes {PINNED - total} -> {PINNED - total + saved} B\n")
    print("  Costs: a bit writer on both sides instead of a memcpy of a packed struct, and a")
    print("  record whose fields no longer line up with anything a debugger or a capture shows.")
    print("  Worth it when the alternative is a second datagram; not worth it before then.")


def upstream(a):
    """The command packet. Bounded by the sender's own entities, not by the world."""
    fixed = sum(b for _, b in COMMAND_HEADER)
    per_command = sum(b for _, b in COMMAND_FIXED)
    one = per_command + a.selection * IDENTITY_BYTES
    total = fixed + a.in_flight * one
    capacity = (PINNED - fixed - per_command) // IDENTITY_BYTES
    fits = (PINNED - fixed) // one
    print(f"  header {fixed} B \u00b7 per command {per_command} B + 2 B per selected identity")
    print(f"  one command at a {a.selection}-identity selection = {one} B")
    print(f"  {a.in_flight} unacknowledged in flight (retransmitted until the snapshot acks)"
          f" = COMMAND PACKET {total} B")
    verdict = "ONE datagram" if total <= PINNED else "FRAGMENTS"
    print(f"  against the pinned {PINNED} B: {PINNED - total} B free"
          f" \u2014 {100 * total // PINNED}% used, {verdict}\n")
    print(f"  Room for {capacity} identities in one command against a peak selection of"
          f" {a.selection} \u2014 the")
    cite = " section 5 makes the host validate away" if a.selection == 110 else \
           " an unvalidated selection would buy an attacker"
    print(f"  {capacity / max(a.selection, 1):.1f}x amplification{cite}. The FORMAT is not the")
    print(f"  constraint. The RETRANSMIT WINDOW is: at a full selection, {fits} commands in flight"
          f" fit and")
    print(f"  {fits + 1} fragment. That count is behavior, not format \u2014 a stalled ack, not a"
          " fast player.")
    print("  Section 5 answers it structurally: the packet is filled OLDEST-FIRST and stops when the")
    print("  next command will not fit, so it cannot exceed the payload, no order is dropped, and the")
    print("  sequence gains no gap the host would discard. No lever from the list below applies.")
    print(f"  A change is upstream's problem only if it makes ONE command exceed {PINNED - fixed} B,")
    print("  which today needs a designer that submits a design DEFINITION rather than an identity")
    print("  (M4+). Re-run with --selection raised, or --add-bytes on the command, the day it lands.")


def reshape(a):
    """Lever 0. Only available while no build has shipped -- see SKILL.md."""
    entities, record, _, total = budget(a.players, a.ships, a.modules, a.removals, 0, 0)

    print("  A DESIGN TABLE in the header, with a short index per entity, against a byte per record.")
    print(f"  Today: 1 B x {entities} entities = {entities} B of design identity.\n")
    print("   designs   index bits   table B   entity B   total B   saves")
    best = None
    for designs in (2, 4, 8, 16, 24, 32, 64):
        bits = max(1, (designs - 1).bit_length())
        table = a.players * designs
        block = -(-entities * bits // 8)
        cost = table + block
        saves = entities - cost
        if best is None or saves > best[1]:
            best = (designs, saves)
        print(f"  {designs:8d} {bits:12d} {table:9d} {block:10d} {cost:9d} {saves:7d}")
    print(f"\n  Break-even is where saves goes negative. A 50-ship fleet repeating a handful of")
    print(f"  designs is the left of this table, where it is worth {best[1]} B -- more than the whole")
    print(f"  field audit. It costs a header that varies with the designs in play.\n")

    print("  THE IDENTITY COLUMN is the largest single block in the snapshot"
          f" ({entities} x 2 = {entities * 2} B).")
    if not a.index_bits:
        print("  Section 4 does not state the index/generation split inside those 2 bytes, and the")
        print("  saving depends entirely on it. Settle the split, then re-run with --index-bits N.")
        return
    n = a.index_bits
    gen = 16 - n
    pool = 1 << n
    if entities > pool:
        print(f"  --index-bits {n} gives a {pool}-entity pool, below the {entities} live entities.")
        return
    # log2 C(pool, entities): the exact information content of a sorted distinct index set.
    floor_bits = (math.lgamma(pool + 1) - math.lgamma(entities + 1)
                  - math.lgamma(pool - entities + 1)) / math.log(2)
    floor = -(-int(floor_bits + entities * gen) // 8)
    print(f"  At a {n}-bit index ({pool} pool) and a {gen}-bit generation, a sorted distinct index set")
    print(f"  carries log2(C({pool},{entities})) = {floor_bits:.0f} bits, and generations add"
          f" {entities * gen}.")
    print(f"  FLOOR {floor} B against {entities * 2} B spent -- at best {entities * 2 - floor} B,"
          f" and no encoding beats it.")
    print(f"  That is a bound, not a design: a real encoder lands short of it. Quote the bound as a")
    print(f"  bound, and measure the encoder once it exists (AGENTS.md section 6).\n")
    print(f"  Snapshot floor if BOTH landed perfectly: {total} -> "
          f"{total - (entities * 2 - floor) - best[1]} B. Neither is written. Neither is free to")
    print("  keep: a columnar, delta-coded record is not a struct anyone can read off a capture.")


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--players", type=int, default=2)
    p.add_argument("--ships", type=int, default=50, help="ships per player")
    p.add_argument("--modules", type=int, default=4, help="modules per player")
    p.add_argument("--removals", type=int, default=3, help="typical removals per snapshot")
    p.add_argument("--add-bytes", type=int, default=0, help="proposed extra bytes per entity")
    p.add_argument("--add-header", type=int, default=0, help="proposed extra header bytes")
    p.add_argument("--rate-hz", type=int, default=20)
    p.add_argument("--selection", type=int, default=110,
                   help="peak identities in one command (section 5's own figure)")
    p.add_argument("--in-flight", type=int, default=4,
                   help="unacknowledged commands retransmitted in one packet")
    p.add_argument("--audit-fields", action="store_true",
                   help="what each field spends against what the client draws")
    p.add_argument("--upstream", action="store_true", help="the command packet")
    p.add_argument("--reshape", action="store_true",
                   help="lever 0: design table and identity column, while nothing has shipped")
    p.add_argument("--index-bits", type=int, default=0,
                   help="bits of the 2-byte identity that are index; section 4 does not say")
    p.add_argument("--all", action="store_true", help="snapshot, field audit and upstream")
    a = p.parse_args()

    sections = [("DOWNSTREAM -- the snapshot", snapshot)]
    if a.audit_fields or a.all:
        sections.append(("FIELD AUDIT -- spent against drawn", audit_fields))
    if a.reshape or a.all:
        sections.append(("RESHAPE -- lever 0, while nothing has shipped", reshape))
    if a.upstream or a.all:
        sections.append(("UPSTREAM -- the command packet", upstream))

    for i, (title, fn) in enumerate(sections):
        if len(sections) > 1:
            print(f"{'' if i == 0 else chr(10)}== {title} ==")
        fn(a)


if __name__ == "__main__":
    # This gets piped into head and grep constantly; a traceback there reads like a defect.
    try:
        main()
    except BrokenPipeError:
        os.dup2(os.open(os.devnull, os.O_WRONLY), sys.stdout.fileno())
        sys.exit(0)

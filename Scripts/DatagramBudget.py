#!/usr/bin/env python3
"""Compute the datagram budget for Outpost Commander -- both directions.

The design's figures are arithmetic on the record layout and the entity counts.
This reproduces that arithmetic so a proposal is costed rather than estimated
(AGENTS.md section 6), and so the same numbers come out every time.

Since ADR-024 the downstream unit is an UPDATE: one datagram per client per
tick, filled with the entity records a priority accumulator says are due. An
update never fragments, whatever the entity count; what the count changes is
how often each entity is refreshed. That is the figure this script exists to
state, and the one that used to be "does the snapshot still fit".

    python3 DatagramBudget.py                       # the update as it stands, MVP shape
    python3 DatagramBudget.py --players 100 --ships 50   # a different match shape
    python3 DatagramBudget.py --in-view 400         # refresh rate for a screen holding 400
    python3 DatagramBudget.py --add-bytes 1         # cost of one more byte per record
    python3 DatagramBudget.py --datagrams 2         # a second update per tick
    python3 DatagramBudget.py --audit-fields        # what each field spends vs what it draws
    python3 DatagramBudget.py --upstream            # the command packet, host-bound
    python3 DatagramBudget.py --all                 # every section, for a wire-format change
"""
import argparse
import os
import sys

# The record, field by field, as Design/TechnicalDesign.md section 4 defines it under ADR-024.
# Every record is a self-contained fact about one entity at the update's tick; nothing in it
# depends on an earlier record. The identity is three bytes -- a 16-bit index and an 8-bit
# generation -- because a 100-player match has thousands of live entities and the generation
# has to survive index reuse under loss. The owner is a byte, so a record says whose it is
# without the update having to be grouped or complete.
RECORD = [("identity", 3), ("owner", 1), ("position", 4), ("heading", 1),
          ("hull", 1), ("design identity", 1), ("flags", 1)]

# The update's header. The first four bytes are the TRANSPORT header (NeuronCore/PacketHeader.h):
# version, type, and a sequence the receiver uses only to count loss. ADR-024 removed the two
# fragment fields M0.2 reserved, because nothing fragments any more. Then the tick every record
# in the datagram describes, the live entity count (which is what lets the client bound how long
# an entity may go unrefreshed), the RECIPIENT'S OWN player block and nothing about anyone
# else's, and four counts -- the fourth since the owner's ruling of 2026-09-24 (OpenQuestions.md Q83), for the
# spent-rock mask. Nothing here scales with the player count, which is the point.
HEADER = [("version", 1), ("type", 1), ("sequence", 2),
          ("tick", 4), ("entity count", 2),
          ("own credits", 4), ("own last command applied", 2),
          ("own building design", 1), ("own build progress", 1),
          ("own unlocks", 1), ("own research progress", 1),     # M4.4b, OpenQuestions.md Q85
          ("record count", 1), ("removal count", 1), ("fire count", 1), ("spent-rock count", 1)]

# After the records. A removal is an identity, repeated in REMOVAL_REPEAT_TICKS consecutive
# updates so a lost datagram cannot leave a ghost; a fire event (ADR-004) is repeated for
# FIRE_REPEAT_TICKS so a lost datagram costs a tracer only when three in a row are lost.
REMOVAL_BYTES = 3
FIRE_BYTES = 7                      # shooter 3, target 3, weapon 1
REMOVAL_REPEAT_TICKS = 10
FIRE_REPEAT_TICKS = 3

# The caps on each repeated section of one update (GameCore/Update.h). They exist so that the records an
# update can ALWAYS hold is a constant, and THE SWEEP IS COMPUTED FROM THAT FLOOR, not from the typical
# fill: a guarantee computed from a typical figure is not one. The client computes the same number to
# decide when an entity it has heard nothing about is gone, so this must match the code exactly.
MAX_REMOVALS_PER_UPDATE = 48
MAX_FIRES_PER_UPDATE = 40

# Q83: which rocks are spent, one bit a rock over the largest field GenerateField makes (22 a region, four
# copies), on EVERY update so each is self-contained. Eleven bytes at most, and counted at its most in both the
# floor and the typical fill, because once a rock is spent every update carries it.
MAX_SPENT_ROCK_BYTES = 11
FORGET_AFTER_SWEEPS = 3

# Field-width audit. "wire" is what the record spends; "draws" is the number of bits the client
# actually needs to draw the field. Width on the wire is decoupled from width in the simulation,
# which keeps its own precision regardless (R16). A reserved field is one deliberately widened
# for a decision already taken; its slack is not recoverable and is excluded from the total.
#                field              wire draws reserved why that many bits are enough
FIELD_AUDIT = [("identity",           24, 24, False, "correctness, not display: the client matches records across ticks"),
               ("owner",               8,  7, False, "254 players is the PlayerId's own ceiling; the eighth bit is spare"),
               ("position x",         16, 16, False, "16,384 units over 65,536 steps is an exact fit at 1/4 unit"),
               ("position y",         16, 16, False, "as above -- there is no slack to find here"),
               ("heading",             8,  7, False, "7 bits is 2.8 deg and reads smooth; 6 steps visibly on a turning ship"),
               ("hull",                8,  4, False, "a bar resolves 16 levels; 0 and 100 are the two that must be exact"),
               ("design identity",     8,  8, True,  "R24 widened it on purpose: research and a designer extend it"),
               ("flags: state",        3,  3, False, "idle, moving, mining, returning, fighting -- five states, three bits"),
               ("flags: cargo",        3,  3, False, "0-4 lit chips, five states; took a spare bit at M2.7 (Q53)"),
               ("flags: spare",        2,  0, False, "two bits nothing has claimed; the team bits left with ADR-024")]

# Upstream. Section 4 states the command packet: the transport header, the player identity, the
# command count, and -- since ADR-024 -- the client's view center and radius, which is what the
# host's accumulator scores relevance against. Then commands: a per-player sequence, the type,
# a target, and the selected identities, repeated every packet until the update acknowledges.
COMMAND_HEADER = [("version", 1), ("type", 1), ("sequence", 2),
                  ("player identity", 1), ("command count", 1),
                  ("view center", 4), ("view radius", 2)]
COMMAND_FIXED = [("sequence", 2), ("order type", 1), ("target point or entity", 4),
                 ("selection count", 1)]
IDENTITY_BYTES = 3
# M2.11, Q55: a PlaceModule carries one design byte past the fixed part, and no other type does.
PLACEMENT_DESIGN_BYTES = 1

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


def ceil_div(n, d):
    return -(-n // d)


def budget(players, ships, modules, removals, fires, extra_per_record, extra_header,
           datagrams=1, payload=PINNED):
    """The update, as arithmetic. Returns a dict so the checker can name what it asserts."""
    entities = players * (ships + 1 + modules)
    record = sum(b for _, b in RECORD) + extra_per_record
    header = sum(b for _, b in HEADER) + extra_header
    tail = removals * REMOVAL_BYTES + fires * FIRE_BYTES + MAX_SPENT_ROCK_BYTES
    per_datagram = (payload - header - tail) // record
    per_tick = per_datagram * datagrams
    floor = (payload - header - MAX_REMOVALS_PER_UPDATE * REMOVAL_BYTES - MAX_FIRES_PER_UPDATE * FIRE_BYTES -
             MAX_SPENT_ROCK_BYTES) // record
    return {
        "entities": entities,
        "record": record,
        "header": header,
        "tail": tail,
        "records per datagram": per_datagram,
        "records per tick": per_tick,
        "datagram bytes": payload,
        "records floor": floor,
        # The sweep: ADR-024's guarantee that nothing goes longer than this unrefreshed, whatever its
        # relevance. From the FLOOR and the cap, as GameCore/Update.h's SweepTicks computes it.
        "sweep ticks": max(1, ceil_div(entities, floor * datagrams)),
    }


def update(a):
    b = budget(a.players, a.ships, a.modules, a.removals, a.fires, a.add_bytes, a.add_header,
               a.datagrams)
    per_client = b["datagram bytes"] * a.datagrams * a.rate_hz
    print(f"  {a.players} players x ({a.ships} ships + 1 station + {a.modules} modules)"
          f" = {b['entities']} live entities")
    print(f"  record {b['record']} B ({sum(v for _, v in RECORD)} + {a.add_bytes} proposed)"
          f" · header {b['header']} B · {a.removals} removals {a.removals * REMOVAL_BYTES} B"
          f" · {a.fires} fires {a.fires * FIRE_BYTES} B")
    print(f"  UPDATE {b['datagram bytes']} B, always one datagram: {b['records per datagram']} records"
          f" in it, {a.datagrams} per tick = {b['records per tick']} records a tick")
    print(f"  per client {per_client / 1000:.1f} KB/s ({per_client * 8 / 1000:.0f} kbit/s)"
          f" at {a.rate_hz} Hz, whatever the entity count;"
          f" host egress at {a.clients} clients {per_client * a.clients / 1e6:.2f} MB/s"
          f" ({per_client * a.clients * 8 / 1e6:.1f} Mbit/s)")
    sweep = b["sweep ticks"]
    print(f"  SWEEP {sweep} tick(s) = {sweep * 1000 / a.rate_hz:.0f} ms, from the {b['records floor']}-record floor:"
          f" no entity goes longer unrefreshed, and the client forgets one after"
          f" {FORGET_AFTER_SWEEPS * sweep} ticks with no record\n")

    in_view = a.in_view if a.in_view else b["entities"]
    refresh = max(1, ceil_div(in_view, b["records per tick"]))
    print(f"  A view holding {in_view} entities refreshes each every {refresh} tick(s)"
          f" = {refresh * 1000 / a.rate_hz:.0f} ms, if the accumulator sends nothing else.")
    if refresh > 1:
        need = ceil_div(in_view, b["records per datagram"])
        print(f"  Every tick would need {need} datagrams per tick (--datagrams {need}),"
              f" {b['datagram bytes'] * need * a.rate_hz / 1000:.1f} KB/s per client.")
    print()
    for name, cap, why in MTU:
        bb = budget(a.players, a.ships, a.modules, a.removals, a.fires, a.add_bytes,
                    a.add_header, a.datagrams, cap)
        pin = " <- PINNED" if cap == PINNED else ""
        print(f"  {name:22s} {cap:5d} B  {bb['records per datagram']:4d} records,"
              f" sweep {bb['sweep ticks']:3d} tick(s){pin}")
        print(f"  {'':22s}        {why}")

    if a.add_bytes or a.add_header:
        was = budget(a.players, a.ships, a.modules, a.removals, a.fires, 0, 0, a.datagrams)
        print(f"\n  the proposal costs {was['records per datagram'] - b['records per datagram']}"
              f" records per datagram ({was['records per datagram']} ->"
              f" {b['records per datagram']}) and moves the sweep"
              f" {was['sweep ticks']} -> {b['sweep ticks']} tick(s)")
    return b


def audit_fields(a):
    """What the record spends against what the client draws."""
    b = budget(a.players, a.ships, a.modules, a.removals, a.fires, 0, 0, a.datagrams)
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
    print(f"  {'TOTAL':18s} {wire:4d} {draws:6d} {recoverable:4d}   recoverable bits per record{note}\n")
    if not recoverable:
        print("  No recoverable slack. Every field is already the width it draws.")
        return
    packed_bits = wire - recoverable
    packed = (PINNED - b["header"] - b["tail"]) * 8 // packed_bits
    print(f"  record {wire} bits -> {packed_bits} bits; the record stops being byte-aligned")
    print(f"  records per datagram {b['records per datagram']} -> {packed},"
          f" sweep {b['sweep ticks']} -> {ceil_div(b['entities'], packed * a.datagrams)} tick(s)\n")
    print("  Costs: a bit writer on both sides instead of a memcpy of a packed struct, and a")
    print("  record whose fields no longer line up with anything a debugger or a capture shows.")
    print("  Under ADR-024 the alternative is a slower refresh, not a second datagram -- so this")
    print("  buys refresh rate, and it is worth taking only when a measured refresh is too slow.")


def upstream(a):
    """The command packet. Bounded by the sender's own entities, not by the world."""
    fixed = sum(b for _, b in COMMAND_HEADER)
    per_command = sum(b for _, b in COMMAND_FIXED)
    one = per_command + a.selection * IDENTITY_BYTES
    total = fixed + a.in_flight * one
    capacity = (PINNED - fixed - per_command) // IDENTITY_BYTES
    fits = (PINNED - fixed) // one
    print(f"  header {fixed} B · per command {per_command} B + {IDENTITY_BYTES} B per selected identity")
    print(f"  one command at a {a.selection}-identity selection = {one} B")
    print(f"  {a.in_flight} unacknowledged in flight (retransmitted until the update acks)"
          f" = {total} B: {'ONE datagram' if total <= PINNED else 'FRAGMENTS'}")
    print(f"  a packet holds {capacity} identities in one command, {fits} commands at this selection")
    print(f"  a module placement is {per_command + PLACEMENT_DESIGN_BYTES} B and an upgrade {per_command} B:"
          " neither carries a selection, so neither moves the worst case\n")
    print("  The format is not the constraint; the retransmit window is, and section 4 bounds it")
    print("  structurally: the packet is filled oldest-first and stops when the next will not fit.")


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--players", type=int, default=2)
    p.add_argument("--ships", type=int, default=50, help="ships per player")
    p.add_argument("--modules", type=int, default=4, help="modules per player")
    p.add_argument("--removals", type=int, default=3, help="removals riding a typical update")
    p.add_argument("--fires", type=int, default=2, help="fire events riding a typical update")
    p.add_argument("--clients", type=int, default=0,
                   help="connected clients for the egress figure; defaults to the player count")
    p.add_argument("--in-view", type=int, default=0,
                   help="entities inside one client's view; defaults to every live entity")
    p.add_argument("--datagrams", type=int, default=2, help="updates per client per tick; the design caps it at two")
    p.add_argument("--add-bytes", type=int, default=0, help="proposed extra bytes per record")
    p.add_argument("--add-header", type=int, default=0, help="proposed extra header bytes")
    p.add_argument("--rate-hz", type=int, default=20)
    p.add_argument("--selection", type=int, default=110,
                   help="peak identities in one command (section 4's own figure)")
    p.add_argument("--in-flight", type=int, default=4,
                   help="unacknowledged commands retransmitted in one packet")
    p.add_argument("--audit-fields", action="store_true",
                   help="what each field spends against what the client draws")
    p.add_argument("--upstream", action="store_true", help="the command packet")
    p.add_argument("--all", action="store_true", help="update, field audit and upstream")
    a = p.parse_args()
    if not a.clients:
        a.clients = a.players

    sections = [("DOWNSTREAM -- the update", update)]
    if a.audit_fields or a.all:
        sections.append(("FIELD AUDIT -- spent against drawn", audit_fields))
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

#!/usr/bin/env python3
"""Compute the snapshot datagram budget for Outpost Commander.

The design's figures are arithmetic on the record layout and the entity counts.
This reproduces that arithmetic so a proposal is costed rather than estimated
(AGENTS.md section 6), and so the same numbers come out every time.

    python3 budget.py                          # the design as it stands
    python3 budget.py --add-bytes 1            # cost of one more byte per entity
    python3 budget.py --players 4 --modules 4  # a different match shape
"""
import argparse

# The record, field by field, as Design/TechnicalDesign.md section 4 defines it.
RECORD = [("identity", 2), ("position", 4), ("heading", 1),
          ("hull", 1), ("design identity", 1), ("flags", 1)]

# Fixed header, then one block per player.
HEADER_FIXED = [("version", 1), ("type", 1), ("sequence", 4), ("tick", 4),
                ("entity count", 2), ("player count", 1), ("removal count", 1)]
HEADER_PER_PLAYER = [("credits", 4), ("last command applied", 2),
                     ("building design", 1), ("build progress", 1)]

# Payload available to UDP under each path assumption. The design pins 1200.
MTU = [("pinned, safe anywhere", 1200, "below IPv6's 1232 minimum-MTU payload; survives tunnels"),
       ("IPv6 minimum MTU",      1232, "1280 - 40 IPv6 - 8 UDP"),
       ("typical VPN, IPv4",     1372, "1400 - 20 - 8"),
       ("PPPoE, IPv4",           1464, "1492 - 20 - 8"),
       ("LAN Ethernet, IPv4",    1472, "1500 - 20 - 8 — the MVP's stated target")]


def budget(players, ships, modules, removals, extra_per_entity, extra_header):
    entities = players * (ships + 1 + modules)
    record = sum(b for _, b in RECORD) + extra_per_entity
    header = (sum(b for _, b in HEADER_FIXED)
              + players * sum(b for _, b in HEADER_PER_PLAYER) + extra_header)
    return entities, record, header, entities * record + header + removals * 2


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--players", type=int, default=2)
    p.add_argument("--ships", type=int, default=50, help="ships per player")
    p.add_argument("--modules", type=int, default=4, help="modules per player")
    p.add_argument("--removals", type=int, default=3, help="typical removals per snapshot")
    p.add_argument("--add-bytes", type=int, default=0, help="proposed extra bytes per entity")
    p.add_argument("--add-header", type=int, default=0, help="proposed extra header bytes")
    p.add_argument("--rate-hz", type=int, default=20)
    a = p.parse_args()

    entities, record, header, total = budget(a.players, a.ships, a.modules,
                                             a.removals, a.add_bytes, a.add_header)
    print(f"  {a.players} players x ({a.ships} ships + 1 station + {a.modules} modules)"
          f" = {entities} entities")
    print(f"  record {record} B ({sum(b for _, b in RECORD)} + {a.add_bytes} proposed)"
          f" · header {header} B · removals {a.removals * 2} B")
    print(f"  SNAPSHOT {total} B   at {a.rate_hz} Hz = {total * a.rate_hz / 1000:.1f} KB/s per client\n")

    for name, cap, why in MTU:
        free = cap - total
        frags = -(-total // cap)
        fit = f"ONE datagram, {free:5d} B free = {free // record:3d} entities" if frags == 1 \
              else f"{frags} datagrams — FRAGMENTS, loss rate roughly x{frags}"
        print(f"  {name:22s} {cap:5d} B  {fit}")
        print(f"  {'':22s}        {why}")

    if a.add_bytes or a.add_header:
        _, _, _, was = budget(a.players, a.ships, a.modules, a.removals, 0, 0)
        print(f"\n  the proposal costs {total - was} B "
              f"({was} -> {total}), {(total - was) // max(record, 1)} entities of headroom")


if __name__ == "__main__":
    main()

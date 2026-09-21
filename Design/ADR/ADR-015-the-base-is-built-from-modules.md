# ADR-015 — The base is built from modules, and a module is an entity

**Status:** Accepted
**Date:** 2026-09-21
**Owner:** Stefan Zwaal

## Context

`Design/GameDesign.md` §1 takes the *Warzone 2100* spine — a unit is a composition, not a type — and §5
gives each player one fixed station that builds, receives ore, shoots and dies. **What it does not have is
the other half of that lineage: building a base.** A station is a single object with a fixed set of
abilities, so the only thing a player constructs is ships, and the only thing an attacker can do to a base
is kill it outright.

The owner asked for modules: a base built out of parts, each of which does something, each upgradeable,
with the MVP carrying a shipyard, an ore processor and a research station, and more added later.

## Decision

**A module is a separate entity on the map, owned by a player, placed near their station, and destroyable
on its own.** It is not a component in a station slot and not a field on the station's record. That is the
decision, and it is taken for one reason: **it is the only version that lets an attacker cripple an economy
without killing the base.** A raid that kills the ore processor and leaves is a real play; an upgrade tree
inside one entity has no such move, and *Warzone 2100*'s buildings are separate structures precisely
because of it.

**A module is a composition, exactly as R24 requires.** It is a `ModuleFrame` hull — one slot, no drive —
carrying one module component. **Each upgrade level is its own component identity**: `ShipyardL1` and
`ShipyardL2` are two catalog rows, and upgrading replaces one with the next. The derived-stat function
needs no new axis, and research gating a level later is the same gate it already needs over any component.

**Placement is a tap, and it costs no new gesture.** With the station selected and a module chosen in the
build panel, a tap on empty space within **400 world units** of the station places it. That interaction was
dead: a tap on empty space is a move order, and the station cannot move. The radius is drawn while a module
is chosen, and a tap outside it, or on another module, does nothing.

**The radius is 400 because that is the point-defence range** (`GameDesign.md` §5), so the safe zone means
exactly "your base". The consequence is positional and is the point: a `MassDriver` reaches 600, so a
fighter standing off at 500 **can shell the modules on the near side while staying outside point-defence
cover**, and which side of your station you build on is therefore a decision.

**A station carries at most four modules.** That cap is as much a wire budget as a design one — see below.

**The MVP ships two working modules, not three.** `Shipyard` and `OreProcessor`, at two levels each. The
research station is designed here and built at **M4 with research**, because `GameDesign.md` §9 has no
research in the MVP and a module that costs credits and does nothing is the mistake the heavy design
already made once (§6).

**What the shipyard does in the MVP is build rate, not new hulls.** The MVP has two ship designs and the
station builds both, so a module gating *what* can be built would gate nothing. It gates *how fast*:
`ShipyardL1` and `L2` raise the station's build rate. From M4 the same levels also gate heavier hulls and
the designer, which is the effect the owner described. **The rate is applied as an integer percentage, and
where it rounds is the question ADR-014 is reserved for** (`Design/Plan/README.md`), not a new one.

**The ore processor raises what a delivered cargo is worth**, by an integer percentage per level.

## Consequences

**The base becomes a structure with internals, and that is the gain.** There is somewhere to spend credits
other than ships, a reason to defend a perimeter rather than a point, and a raid objective short of the
station. It also gives the station something to do in the first two minutes besides emit miners.

**It costs the single-datagram property most of its headroom.** Four modules a player at two players is
eight more entities: **110 entities, 1,136 bytes, still one datagram — but 64 bytes of headroom where there
were 144.** Six entities, where there were fourteen. [`ADR-003`](ADR-003-replication-is-full-snapshots.md)'s
headline benefit survives this change and would not survive another of the same size, which is why the cap
is four and why raising it is a replication decision rather than a design one.

**It adds a placement validity rule to the simulation** — inside the radius, clear of the station and of
other modules — which is simulation state and therefore obeys R16: integers, and candidates ordered by
entity identity.

**A player can now lose an investment without losing the match**, which is new. Whether that reads as depth
or as punishment is a balance question and it is on the register; the mitigation if it is punishment is the
build radius, not the module's hull.

**What it forecloses:** a module cannot be moved once placed, and there is no salvage or refund. Both are
additive later and neither is in the MVP.

**What would reopen the entity decision:** a measured entity count that breaks the single datagram, at
which point modules are the cheapest thing to fold back into the station's record — and the cripple play
goes with them.

## Measurements

None yet. Two are owed:

1. **The snapshot's encoded size at 110 entities** (M2), against the 1,136 bytes this ADR asserts and the
   1,200-byte payload. This is the measurement that decides whether the cap of four is right.
2. **Whether losing a module to a raid reads as depth or as punishment** (M3), which is played rather than
   computed.

Every figure here is arithmetic on the design's own numbers: 110 is 2 × (50 + 1 + 4); 1,136 is
110 × 10 + 30 + three removals; 400 is `GameDesign.md` §5's point-defence range, unchanged.

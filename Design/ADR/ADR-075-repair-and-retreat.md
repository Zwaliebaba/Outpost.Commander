# ADR-075 — A Repair Bay, the Command Station and the Shipyards repair ships near them, and a ship below its threshold goes back to one

Status: **accepted** · 2026-10-07

## Context

Phase 4 design §10 (gate L9) adds repair at a base and a retreat order, and §13 the client's part of them.

- **The Repair Bay** is a new structure: 300 Ore and 40 s of a Constructor's work, 2,000 hit points, armor 5, built only in a held sector. It repairs up to 4 friendly warships within 150 m, each at 3% of its full hit points a second, for nothing. It has no levels, and it borrows a mesh and a tint as the Relay does.
- **A retreat threshold per ship:** never, at 50% or at 25% of its hit points, 25% by default. A design sets it for every ship built to it, and a selection changes it.
- **Below its threshold a ship drops its order** and goes to the nearest Repair Bay, else the Command Station, else a Shipyard, and is repaired there. Once whole it goes back to its standing order (ADR-059), if it had one, and otherwise waits there.
- **The simulation runs it**, so it works while nobody is looking.
- **The AI's ships use it too.** The AI's group fall-back ([ADR-041](ADR-041-ai-plays-a-longer-match.md)) is unchanged.

The owner decided on 2026-10-07:

- **The Command Station and the Shipyards repair as a Bay does.** A retreat always ends in a repair, and a Bay's worth is that it stands near the front.
- **Any order the player gives a retreating ship ends its retreat.** The next hit that leaves it below its threshold starts it again.
- **Every ship, Constructors included,** retreats and is repaired. A Constructor has no design, so its retreat is set on a selection.
- **Until milestone 33 the AI's ships retreat at the default**, and the AI builds no Repair Bay. Milestone 33 has it build one at its front ([ADR-076](ADR-076-ai-plays-phase-4.md)).

What the design leaves open: which four a repairer takes when more are near, what "nearest" means, when a retreat starts and ends, how the tuning data, the protocol and the AI carry it, and how the client shows it.

## Decision

1. **The Repair Bay is `StructureKind::RepairBay`**, the seventh kind in the tuning data, which must list it.
   - It is built as any structure is ([ADR-016](ADR-016-base-building.md) decision 4), and only in a sector its player holds, as a Shipyard is ([ADR-056](ADR-056-territory.md) decision 5). One that stands in a sector its player loses keeps working.
   - Its footprint is 30 m. It has no gun and no levels.
2. **The tuning data's optional `"shipRepair"` says how a base repairs:** `ships` 4, `rangeMeters` 150 and `percentPerSecond` 3. Without it, only Constructors repair and no ship retreats.
3. **A repairer is a player's built Repair Bay, Command Station or Shipyard.** Each tick, after the Constructors' work:
   - each repairer, in identifier order, repairs the most damaged of its player's ships whose footprint is within `rangeMeters` of its own, as many as `ships`, by the share of their hit points that is most damaged, then by identifier;
   - a ship that an earlier repairer took that tick is not taken again, so two repairers side by side repair eight ships;
   - each ship gains `percentPerSecond` of its full hit points a second, as an integer step per tick, for nothing. Research does not raise it, as it does a Constructor's rate;
   - it repairs any damaged ship of its player's in reach, retreating or not, as a Constructor repairs any it is ordered to.
4. **A ship carries its retreat, `RetreatThreshold`: `Never`, `Half` or `Quarter`.**
   - A design carries one, `Quarter` unless its player saves it otherwise, and a ship built to it starts with it. A Constructor starts with `Quarter`.
   - Pirates' ships never retreat ([ADR-073](ADR-073-pirates.md)).
5. **A hit that leaves a ship below its threshold sends it back**, unless it is already going.
   - It is checked once the tick's hits land, splash included, so a ship destroyed by the hit does not go.
   - The ship's attack or work order ends, and it is given a move order to its repairer. It still fires on the move ([ADR-014](ADR-014-designs-and-combat.md) decision 9).
   - **Its repairer** is the nearest Repair Bay, by straight distance from the ship. Without one it is the Command Station, and without that the nearest Shipyard; a tie goes to the lower identifier. With none at all, the ship does not retreat and fights on.
   - Each ship goes on its own, to a point 40 m beyond its repairer's footprint on the side it comes from, so that every ship stops within the repair's reach rather than in a formation's back rows.
   - Once a second, a retreating ship whose repairer has fallen goes to the next one by the same rule.
6. **A retreat ends** when the ship is whole, or when its player gives it any order that names it, or sets it never to retreat.
   - Whole, it waits where it is. A ship on a standing order keeps that order while it retreats, its group goes on without it, and once whole it rejoins the group when the group next moves (ADR-059).
   - The next hit while it is below its threshold starts a retreat again. A player who sends a damaged selection back into a fight sees its damaged ships turn round as each is hit.
7. **`SetRetreatCommand`** sets the threshold of the player's ships it names, and gives them no order. `SaveDesignCommand` carries a design's retreat, and saving a design under its own name with a new retreat updates it. Ships already built keep theirs.
8. **The protocol** carries `SetRetreatCommand`, the Repair Bay's kind, a design's retreat on `DesignView`, and on `EntityView` a ship's retreat and whether it is retreating, which only its owner sees.
9. **The AI** ([ADR-020](ADR-020-ai-and-match-flow.md), [ADR-041](ADR-041-ai-plays-a-longer-match.md)):
   - Its designs keep the default, so its ships retreat at a quarter.
   - It takes a retreating ship out of every order it gives, since any order would end the retreat.
   - A retreating ship leaves its attack group or raid, as a lost one does, and rejoins the reserve once it is whole. A retreat so counts toward a group's fall-back (owner, 2026-10-07, after the measurement below).
   - A retreating Constructor leaves its crew, and is not idle until it is whole, so the AI neither waits on it nor gives it work.
   - It builds a Repair Bay behind its front ([ADR-076](ADR-076-ai-plays-phase-4.md) decision 6).
10. **The client** (design §13):
    - The build menu offers the Repair Bay, and its ghost is green only in a sector the player holds. It is drawn with the Shipyard's model, darker (`tint` 0.6 in `Models.json`), until it has a model of its own.
    - The selection's retreat is a row of three at the foot of its buttons, RETREAT over "25%", "50%" and "Never", the first ship's setting lit, or none lit and MIXED beside the label while the ships differ. A press on a cell sets every selected ship to it ([ADR-088](ADR-088-selection-and-commands.md) decision 3). A line counts the ships retreating to be repaired.
    - The designer shows the design's retreat between Rename and the stepper as the same row, RETREAT over "25%", "50%" and "NEVER", and a press sets it. Saving sends it. A saved design whose retreat changed under the same name is saved with UPDATE.
    - Alerts ([ADR-059](ADR-059-alerts-and-standing-orders.md)) add **Ship retreating** when one of the player's ships starts going back, and **Pirates cleared** when a sector the pirates guarded no longer is.
    - The match log names the Repair Bay "bay".

## Consequences

- **Retreat and repair work as §10 asks** (`RetreatTests`):
  - a ship goes to a Repair Bay before a nearer Station or Shipyard, to the Station before a Shipyard, and to the nearer of two Shipyards;
  - it is repaired whole there and waits;
  - one set never to retreat fights to its end;
  - an order ends a retreat and the next hit starts it again;
  - a standing order waits for the repair;
  - a repairer repairs four ships at a time, the most damaged first;
  - a design carries its retreat to the ships built to it.
- **Tests that measure a fight to the end say so.** The camp's balance ([ADR-073](ADR-073-pirates.md)), a camp attacking an intruder, and a group holding its sector against an enemy now set their ships never to retreat. Without that, a ship below a quarter turns for home and the fight ends differently.
- **AI against AI over seeds 1–40, measured in the Linux container** (design §2), the AI's ships retreating at a quarter and no Bay built:
  - 36 matches end, 35 by domination and 1 by production, at a median of 1:35:08, against 37 at 1:32:57 after milestone 31. Four are level at three hours, against three.
  - **Fleets last.** A side loses a median of 20.5 warships a match, against 52.5, and builds 35.5, against 69.5. It turns a median of 55.5 ships for home, and 40% of them come back whole; the rest are lost on the way, or the match ends first.
  - **The Ore piles up.** A side's fleet is full, with no room for one more ship of its production design, 64% of the match at the median, against 61% after milestone 31; the Ore it no longer spends on replacing ships piles up instead, 4,636 at minute 20 at the median, against 2,474.
  - **The engine:** the median match's 99th percentile tick is 1.5 ms, the worst match's 2.9 ms, as after milestone 31; the slowest single tick, 76 ms in one match, against 21 ms after milestone 31, is a spike of the kind milestone 29 saw (72 ms). The run was on an idle machine.
  - **Two alternatives were measured and set aside.** Keeping a retreating ship in its attack group, so that it does not count toward the fall-back, leaves 17 of the 40 level at three hours: a group whose ships keep leaving never falls back and never wins. Taken as idle, a retreating Constructor was given builds the AI then dropped, which held its Ore and its production back; with that fixed, the median match is 15 minutes longer and fleets are rebuilt.
- **A retreat is a path search.** Each ship that turns for home plans its own path, and a Repair Bay's reach is checked against every ship of its player each tick. Both are counted in the measurement's tick times above.

## What this forecloses

- A repair that costs Ore, or that research speeds.
- A threshold other than never, half and a quarter.
- A retreat that ignores the player's orders.
- A ship that returns to the fight on its own after a repair, other than by its standing order.

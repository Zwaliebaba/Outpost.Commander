# ADR-075 — A Repair Bay, the Command Station and the Shipyards repair ships near them, and a ship below its threshold goes back to one

Status: **accepted** · 2026-10-08
Owns: repair-bay, ship-repair, retreat-threshold, retreat

## Context

Ships are repaired at a base, and a damaged ship goes back to be repaired by itself.

- **The Repair Bay** is a structure of its own, built only in a held sector. It repairs a few friendly warships near it, each by a share of its full hit points a second, for nothing. Its cost, build time, hit points and armor are `Tuning.json`'s `"structures"`, and its reach and rate are `"shipRepair"`. It has no levels, and it borrows a mesh and a tint as the Relay does.
- **A retreat threshold per ship:** never, at 50% or at 25% of its hit points, 25% by default. A design sets it for every ship built to it, and a selection changes it.
- **Below its threshold a ship drops its order** and goes to the nearest Repair Bay, else the Command Station, else a Shipyard, and is repaired there. Once whole it goes back to its standing order ([ADR-059](ADR-059-alerts-and-standing-orders.md)), if it had one, and otherwise waits there.
- **The simulation runs it**, so it works while nobody is looking.
- **The AI's ships use it too** ([ADR-076](ADR-076-ai-plays-phase-4.md) decision 9).

The owner decided on 2026-10-07:

- **The Command Station and the Shipyards repair as a Bay does.** A retreat always ends in a repair, and a Bay's worth is that it stands near the front.
- **Any order the player gives a retreating ship ends its retreat.** The next hit that leaves it below its threshold starts it again.
- **Every ship, Constructors included,** retreats and is repaired. A Constructor has no design, so its retreat is set on a selection.
- **The AI's ships retreat at the default**, and the AI builds a Repair Bay at its front ([ADR-076](ADR-076-ai-plays-phase-4.md) decisions 6 and 9).

The engineering must therefore say which ships a repairer takes when more are near, what "nearest" means, when a retreat starts and ends, how the tuning data and the protocol carry it, and how the client shows it.

## Decision

1. **The Repair Bay is `StructureKind::RepairBay`**, the seventh kind in the tuning data, which must list it.
   - It is built as any structure is ([ADR-016](ADR-016-base-building.md) decision 4), and only in a sector its player holds, as a Shipyard is ([ADR-056](ADR-056-territory.md) decision 5). One that stands in a sector its player loses keeps working.
   - Its footprint is the tuning data's. It has no gun and no levels.
2. **The tuning data's optional `"shipRepair"` says how a base repairs:** `ships`, `rangeMeters` and `percentPerSecond`. Without it, only Constructors repair and no ship retreats.
3. **A repairer is a player's built Repair Bay, Command Station or Shipyard.** Each tick, after the Constructors' work:
   - each repairer, in identifier order, repairs the most damaged of its player's ships whose footprint is within `rangeMeters` of its own, as many as `ships`, by the share of their hit points that is most damaged, then by identifier;
   - a ship that an earlier repairer took that tick is not taken again, so two repairers side by side repair twice as many ships;
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
6. **A retreat ends** when the ship is whole, or when its player gives it an order that is applied: any order that names it, but for two. A scheduled order leaves the retreat alone until it fires, when its action is given as an order ([ADR-080](ADR-080-events-and-scheduled-orders.md) decision 5); and setting the ship's retreat ends it only when it sets the ship never to retreat (decision 7).
   - Whole, it waits where it is. A ship on a standing order keeps that order while it retreats, its group goes on without it, and once whole it rejoins the group when the group next moves ([ADR-059](ADR-059-alerts-and-standing-orders.md) decision 5).
   - The next hit while it is below its threshold starts a retreat again. A player who sends a damaged selection back into a fight sees its damaged ships turn round as each is hit.
7. **`SetRetreatCommand`** sets the threshold of the player's ships it names, and gives them no order. `SaveDesignCommand` carries a design's retreat, and saving a design under its own name with a new retreat updates it. Ships already built keep theirs.
8. **The protocol** carries `SetRetreatCommand`, the Repair Bay's kind, a design's retreat on `DesignView`, and on `EntityView` a ship's retreat and whether it is retreating, which only its owner sees.
9. *(retired)*
10. **The client:**
    - The build menu offers the Repair Bay, and its ghost is green only in a sector the player holds. It is drawn with the Shipyard's model, darker (its `tint` in `Models.json`), until it has a model of its own.
    - The selection's and the designer's retreat rows are [ADR-088](ADR-088-selection-and-commands.md) decision 3's. With the selection's, a line counts the ships retreating to be repaired. A saved design whose retreat changed under the same name is saved with UPDATE.
    - The alerts of a ship retreating and of pirates cleared are [ADR-059](ADR-059-alerts-and-standing-orders.md) decision 1's, and the match log's `retreat`, `repaired` and `again` records [ADR-038](ADR-038-phase-one-match-log.md) decision 1's.

## Consequences

- **Retreat and repair work as decisions 3 to 6 have it** (`RetreatTests`):
  - a ship goes to a Repair Bay before a nearer Station or Shipyard, to the Station before a Shipyard, and to the nearer of two Shipyards;
  - it is repaired whole there and waits;
  - one set never to retreat fights to its end;
  - an order ends a retreat and the next hit starts it again;
  - a standing order waits for the repair;
  - a repairer repairs `ships` at a time, the most damaged first;
  - a design carries its retreat to the ships built to it.
- **Tests that measure a fight to the end say so.** The camp's balance ([ADR-073](ADR-073-pirates.md)), a camp attacking an intruder, and a group holding its sector against an enemy set their ships never to retreat. Without that, a ship below a quarter turns for home and the fight ends differently.
- **A retreat keeps ships alive.** A ship that turns for home and reaches its repairer is repaired whole rather than lost, so a side replaces fewer ships and has more Ore to spend or bank.
- **A retreat is a path search.** Each ship that turns for home plans its own path, and a Repair Bay's reach is checked against every ship of its player each tick.

## What this forecloses

- A repair that costs Ore, or that research speeds.
- A threshold other than never, half and a quarter.
- A retreat that ignores the player's orders.
- A ship that returns to the fight on its own after a repair, other than by its standing order.

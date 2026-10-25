# ADR-016 — A base: Ore in hundredths, structures that block, Constructors that build and repair, and queues that pay at the start

Status: **accepted** · 2026-10-07
Owns: ore-in-hundredths, starting-base, structure-placement, constructor-work, production-queues, base-rules-in-snapshot, shipyard-numbers

## Context

Each player has a base: Ore from Mining Rigs, structures built by Constructors, a Shipyard and a Command Station that produce, and a Defence gun on the Command Station and the Defence Platform (Game §economy, Game §structures). Costs are paid when a job starts, and nothing is refunded. Structures have circular footprints that must not overlap, Mining Rigs snap to ore asteroids, and several Constructors build faster than one. How these combine tick by tick decides whether a replay reproduces a base (ADR-009) and what a tick costs.

## Decision

1. **Ore counts in hundredths.** A player's stockpile is an integer of hundredths of an Ore, and its rigs' income is paid a tick's share at a time, with its remainder carried (decision 11). Costs are whole Ore. A snapshot carries the stockpile rounded down to whole Ore, and the player's income in hundredths per second: the sum of its built rigs' earnings, each after its sector's share ([ADR-056](ADR-056-territory.md) decision 5) and a dry asteroid's trickle ([ADR-035](ADR-035-ore-depletion.md) decision 2).
2. **The tuning data holds the base's numbers** (ADR-008): each structure's `footprintRadiusMeters`; the `constructor` entry, with the Constructor's hit points, armor, speed, cost, build time, footprint and turn rate, and the rates `extraConstructorBuildShare` and `repairPercentPerSecond`; and the rules' `startingConstructors` and `startingOre`. The footprints, the Constructor's footprint and its turn rate are final, as the hulls' sizes are (owner, 2026-10-01). `Simulation::UseTuning` gives the simulation the data at match setup; without it, orders to build, repair or queue are rejected as `NotYetSupported`.
3. **A match starts with a base.** Each player is added with the tuning data's `startingOre` and the starting designs saved ([ADR-014](ADR-014-designs-and-combat.md) decision 2). `Simulation::PlaceStartingBases` puts each player's Command Station on its start, built and armed. The starting Constructors stand in a row in front of it, facing the map's center. The server refuses to start when either would overlap an obstacle or cross the edge. Warships come from a Shipyard.
4. **A structure is placed, and paid for, when the order is given.** `BuildStructureCommand` names Constructors only, and a structure a Constructor builds; any other, the Command Station among them, is `NotBuildable`.
   - **Placement.** The footprint must stay inside the map's edge and overlap no asteroid, field, structure of either player or derelict ([ADR-074](ADR-074-salvage.md) decision 1), or the order is `InvalidPlacement`. A Mining Rig instead snaps to the ore asteroid whose edge is within 40 m of the point (`RIG_SNAP_METERS`). That asteroid must not already hold a rig. The rig stands at the asteroid's center and its footprint covers the asteroid, so Constructors and attackers reach it from the asteroid's edge.
   - **Territory.** On a map with territory, a rig's asteroid, a Shipyard, a Repair Bay, a Research Lab and a Defence Platform must be in a sector the player holds (`SectorNotHeld`, [ADR-056](ADR-056-territory.md) decision 5), and a Relay snaps to its sector's node and is refused as [ADR-056](ADR-056-territory.md) decision 4 has it, `Guarded` and `CapReached` among its refusals. On a map without sectors a Relay is `NotBuildable` (ADR-056 decision 1).
   - **Limits.** A player may have one Research Lab, counting one under construction (`LimitReached`).
   - **Payment.** The cost is taken at once, and the order is rejected, costing nothing, when the player cannot pay (`NotEnoughOre`).
   - **The site.** It appears at once, blocks movement from then on, and can be attacked. It starts with a tenth of its hit points.
5. **Constructors build and repair on a work order.** A Constructor on a work order heads for its target. It works once its footprint is within 20 m of the target's, and stands while it does. A derelict is salvaged on the same order and at the same rate ([ADR-074](ADR-074-salvage.md)).
   - **Building.** Each tick, one Constructor on a site does a tick of work, and each further one adds `extraConstructorBuildShare` of a tick: two build in two thirds of the time, three in half. Work is counted in thousandths of a tick (ADR-014). The site's hit points rise with the work, from a tenth to full, so damage taken while it is built stays taken.
   - **Repair.** Each Constructor restores `repairPercentPerSecond` of the target's maximum hit points each second, for nothing. A Repair Bay, the Command Station and the Shipyards also repair the ships near them, with no order ([ADR-075](ADR-075-repair-and-retreat.md)).
   - **Research.** Its player's research raises the building and the repair rates alike ([ADR-033](ADR-033-research-tiers.md) decision 4).
   - **`RepairCommand`** is the order a right-click on a damaged friendly gives (Game §orders.constructors). It is also how further Constructors join a site under construction. A structure's next level is no Constructor's work ([ADR-064](ADR-064-structure-upgrades.md) decision 3).
   - **The end of the order.** It ends when the target is built and whole, and when the target is destroyed.
   - **What a structure does while it is built.** A structure under construction does nothing but stand, block and take damage. It does not produce, fire or earn.
6. **Queues pay at the start.** A built Shipyard queues ships of its player's saved designs, and the Command Station queues Constructors, up to five jobs each.
   - **Starting a job.** The front job starts when the player can pay, and a warship's also when it fits under the player's fleet cap ([ADR-071](ADR-071-fleet-cap.md)), and is paid then. Until then it waits at the front and the jobs behind it wait too. A Shipyard builds a hull only from the level that adds it ([ADR-064](ADR-064-structure-upgrades.md) decision 9).
   - **Timing.** A job takes its design's build time, or the Constructor's. The next job starts on the tick after one ends.
   - **Delivery.** A finished ship appears just beyond the producer's footprint, on the side facing the map's center, moved clear of obstacles.
   - **Numbers.** A Shipyard finished, built or placed whole, takes its owner's next number, counted from 1 and never reused. Only its owner's snapshot carries the number and the ships it has built.
   - **No cancel and no refund.** No command takes a job, a site or a level back, and nothing refunds Ore.
   - **What the snapshot shows.** It carries each producer's queue, and the front job's progress in thousandths: zero while the job waits.
7. **Built armed structures fire under the ship rules.** The Command Station and the Defence Platform carry the tuning data's Defence gun (`structureWeapons`), and the station's further guns by level are [ADR-064](ADR-064-structure-upgrades.md) decision 11's; research raises its fire rate ([ADR-033](ADR-033-research-tiers.md) decision 4). They pick targets, reload and land hits as ships do ([ADR-014](ADR-014-designs-and-combat.md)): ships before structures, a cold gun's first shot at a random moment, and hits together at the end of the tick. The Defence gun outranges the Lance and falls short of the Missile Rack, which `DefenseTests` checks of the tuning data. A shot from a Defence gun names no weapon, and the client draws it as a tracer.
8. **Structures block movement.** Every structure except a Mining Rig is an obstacle for pathing and for keeping ships clear; a rig stands on its asteroid, which already is one. How the pathfinder's graphs follow a structure placed or destroyed is [ADR-054](ADR-054-obstacle-grid-and-extended-graphs.md) decisions 5 to 7's, and ships whose way a new structure blocks search again ([ADR-010](ADR-010-movement-and-pathing.md)).
9. **A Constructor attacks nothing.** It has no weapon. An attack order that includes Constructors goes to the warships among them alone.
10. **The client learns the base's rules from the snapshot.** Each snapshot carries what the client needs to name, draw, place and price things, since the client never reads the tuning data (ADR-002):
    - every kind of structure, with its name, footprint, cost, levels ([ADR-064](ADR-064-structure-upgrades.md) decision 6) and whether a Constructor builds it (`StructureTypeView::buildable`);
    - the Constructor's cost and build time, and each design's cost;
    - the map's size;
    - the player's income;
    - each structure's construction in thousandths, and each producer's queue.
    The Mining Rig's snap distance and the queue's five jobs are protocol constants, shared by the two sides. Each snapshot repeats the static part rather than adding a message for it.
11. **Income is paid in full, however it divides into ticks.** A player's income is counted per second in hundredths, the sum of its built rigs' (decision 1). Each tick adds it to a remainder counted in hundredths times ticks a second; the player receives the remainder divided by the tick rate, and keeps what is left over. So an income that is not a whole number of hundredths a tick, as research and a sector's share make it, is paid in full, and integers keep a replay exact (ADR-009).

## Consequences

- **Every rate is an integer step per tick.** A replay reproduces a base exactly (ADR-009). The integer steps round the tuning data's rates once: `repairPercentPerSecond` of a structure's maximum hit points becomes hundredths a tick, and a rate that does not divide evenly is rounded to the nearest hundredth.
- **Ships come out where they come out.** There is no rally point; a new ship stands beside its producer until it is ordered.
- **Nothing is ever refunded.** A site destroyed before it is built loses its Ore, and a queued job that has started cannot be cancelled.
- **The measurement loads count the starting base.** The measurement load and the stress scene count it toward their 200 ships and 40 structures, which block and carry their Defence guns.

## What this forecloses

- Refunds and cancelling a job, without a new decision.
- Structures placed on each other, or ships built anywhere but beside their producer.
- Construction that costs Ore over time, as some RTS games charge it: the whole cost is paid when the job starts.

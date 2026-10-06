# ADR-016 — A base: Ore in hundredths, structures that block, Constructors that build and repair, and queues that pay at the start

Status: **accepted** · 2026-10-01

## Context

Milestone 4 gives each player a base (design §5, §6, §14): Ore from Mining Rigs, structures built by Constructors, a Shipyard and a Command Station that produce, and a Defence gun on the Command Station and the Defence Platform. Design §5 says costs are paid when a job starts and nothing is refunded. Design §6 says structures have circular footprints that must not overlap, Mining Rigs snap to ore asteroids, and several Constructors build faster. Gate G8 set the Constructor's numbers and the build and repair rates on 2026-10-01, as a baseline the owner made final the same day. The design leaves open how these combine tick by tick, and those details decide determinism (ADR-009) and the tick's cost (Q4).

## Decision

1. **Ore counts in hundredths.** A player's stockpile is an integer of hundredths of an Ore, so a rig's income per tick is whole: 5 Ore a second is 25 hundredths a tick at 20 Hz, and 8 is 40. An income that research makes uneven is paid with its remainder carried ([ADR-017](ADR-017-research-and-the-designer.md)). Costs are whole Ore. A snapshot carries the stockpile rounded down to whole Ore and the player's income in hundredths per second, the sum of its built rigs.
2. **The tuning data holds the base's numbers** (ADR-008).
   - Every structure has a `footprintRadiusMeters`: Command Station 45, Shipyard 40, Research Lab 30, Mining Rig 25, Defence Platform 20.
   - The Constructor is a `constructor` entry: 300 hit points, armor 2, 45 m/s, 60 Ore, 15 s, a 10 m footprint and a 150°/s turn rate. It also holds G8's rates: `extraConstructorBuildShare` 0.5 and `repairPercentPerSecond` 2.
   - The rules gain `startingConstructors`, 2.
   - The footprints, the Constructor's footprint and its turn rate are final, as gate G5 made the hulls' sizes (owner, 2026-10-01).
   - `Simulation::UseTuning` gives the simulation the data at match setup. Without it, orders to build, repair or queue are rejected as not yet supported, so movement and combat tests run as before.
3. **A match starts with a base.** `Simulation::PlaceStartingBases` puts each player's Command Station on its start, built and armed. The starting Constructors stand in a row in front of it, facing the map's center. The server refuses to start when either would overlap an obstacle or cross the edge. The provisional starting fleet of milestones 2 and 3 is gone (ADR-013), and so is the map's `startingFleet`. Warships come from a Shipyard.
4. **A structure is placed, and paid for, when the order is given.** `BuildStructureCommand` names Constructors only, and a structure a Constructor builds: not the Command Station.
   - **Placement.** The footprint must stay inside the map's edge and overlap no asteroid, field or structure of either player. A Mining Rig instead snaps to the ore asteroid whose edge is within 40 m of the point. That asteroid must not already hold a rig. The rig stands at the asteroid's center and its footprint covers the asteroid, so Constructors and attackers reach it from the asteroid's edge. On a map with territory the asteroid must be in a sector the player holds, and a Relay snaps to its sector's node ([ADR-056](ADR-056-territory.md)).
   - **Limits.** A player may have one Research Lab, counting one under construction.
   - **Payment.** The cost is taken at once, and the order is rejected, costing nothing, when the player cannot pay.
   - **The site.** It appears at once, blocks movement from then on, and can be attacked. It starts with a tenth of its hit points.
5. **Constructors build and repair on a work order.** A Constructor on a work order heads for its target. It works once its footprint is within 20 m of the target's, and stands while it does.
   - **Building.** Each tick, one Constructor on a site does a tick of work, and each further one adds `extraConstructorBuildShare` of a tick: two build in two thirds of the time, three in half. Work is counted in thousandths of a tick (ADR-014). The site's hit points rise with the work, from a tenth to full, so damage taken while it is built stays taken.
   - **Repair.** Each Constructor restores `repairPercentPerSecond` of the target's maximum hit points each second, for nothing.
   - **Research.** Its player's research raises the building and the repair rates alike ([ADR-033](ADR-033-research-tiers.md) decision 4).
   - **`RepairCommand`** is the new order a right-click on a damaged friendly gives (design §9). It is also how further Constructors join a site under construction. A structure's next level is no Constructor's work ([ADR-064](ADR-064-structure-upgrades.md) decision 3).
   - **The end of the order.** It ends when the target is built and whole, and when the target is destroyed.
   - **What a structure does while it is built.** A structure under construction does nothing but stand, block and take damage. It does not produce, fire or earn.
6. **Queues pay at the start.** A built Shipyard queues ships of its player's saved designs, and the Command Station queues Constructors, up to five jobs each.
   - **Starting a job.** The front job starts when the player can pay, and a warship's also when it fits under the player's fleet cap ([ADR-071](ADR-071-fleet-cap.md)), and is paid then (design §5). Until then it waits at the front and the jobs behind it wait too.
   - **Timing.** A job takes its design's build time, or the Constructor's. The next job starts on the tick after one ends.
   - **Delivery.** A finished ship appears just beyond the producer's footprint, on the side facing the map's center, moved clear of obstacles.
   - **What the snapshot shows.** It carries each producer's queue, and the front job's progress in thousandths: zero while the job waits for Ore.
7. **Built armed structures fire under the ship rules.** The Command Station and the Defence Platform carry the tuning data's Defence gun: 30 damage every 1.0 s at 250 m, against armor 10 on both; research raises its fire rate ([ADR-033](ADR-033-research-tiers.md) decision 4). They pick targets, reload and land hits as ships do (ADR-014): ships before structures, a cold gun's first shot at a random moment, and hits together at the end of the tick. A shot from a Defence gun names no weapon, and the client draws it as a tracer.
8. **Structures block movement.** Every structure except a Mining Rig is an obstacle for pathing and for keeping ships clear; a rig stands on its asteroid, which already is one. Placing one extends the pathfinder's graphs and destroying one drops them, each brought up to date when next needed ([ADR-054](ADR-054-obstacle-grid-and-extended-graphs.md)), and ships whose way a new structure blocks search again (ADR-010).
9. **A Constructor attacks nothing.** It has no weapon. An attack order that includes Constructors goes to the warships among them alone.
10. **The client learns the base's rules from the snapshot.** Each snapshot carries what the client needs to name, draw, place and price things, since the client never reads the tuning data (ADR-002):
    - every kind of structure, with its name, footprint, whether it is built and its cost;
    - the Constructor's cost, and each design's cost;
    - the map's size;
    - the player's income;
    - each structure's construction in thousandths, and each producer's queue.
    The Mining Rig's snap distance and the queue's five jobs are protocol constants, shared by the two sides. Each snapshot repeats the static part, which is a few dozen bytes, rather than adding a message for it.

## Consequences

- **Every rate is an integer step per tick.** A replay reproduces a base exactly (ADR-009). The integer steps round the tuning data's rates once: 2% of a Shipyard's 2,500 hit points is 250 hundredths a tick, and a rate that does not divide evenly is rounded to the nearest hundredth.
- **A structure's death is a graph rebuild.** ADR-010 measures it at 1.4–2.8 ms per radius in use with 53 obstacles in a Linux container. The graphs are built again on the quiet ticks after, one a tick, or by an order that needs one first ([ADR-032](ADR-032-order-ticks.md) decision 2). Q4's measurement includes it.
- **Ships come out where they come out.** There is no rally point yet; a new ship stands beside its producer until it is ordered.
- **Nothing is ever refunded.** A site destroyed before it is built loses its Ore, and a queued job that has started cannot be cancelled. The MVP has no cancel at all (design §5).
- **The measurement loads changed.** Task 2.7's load and task 3.7's stress scene count the starting base toward their 200 ships and 40 structures. Their structures now block, so the stress scene spaces them by the map's minimum gap. Its structures carry their Defence guns, which makes the scene's 40 structures fight.

## What this forecloses

- Refunds and cancelling a job, without a new decision.
- Structures placed on each other, or ships built anywhere but beside their producer.
- Construction that costs Ore over time, as some RTS games charge it: the whole cost is paid when the job starts.

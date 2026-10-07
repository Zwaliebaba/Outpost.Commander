# ADR-071 — A fleet cap in command points, bought with the Command Station's level, holds back a warship job until it fits

Status: **accepted** · 2026-10-06

## Context

Phase 4 design §5 bounds a player's fleet: each warship takes command points by its hull, Small 1, Medium 2 and Large 4, and the Command Station's level sets how many a player may have, 12, 20, 30, 40 and 50 at levels 1–5 (gate L2, owner, 2026-10-06). A Constructor takes none. A job that would go over the cap waits as a job waiting for Ore does, the cap never removes a ship, and a player without a Command Station has level 1's cap, as it has level 1's nodes (Phase 3 design §7).

What the design leaves open: where the numbers live, what counts against the cap before a ship exists, and what the client is told.

## Decision

1. **The numbers are data in `Tuning.json`** (ADR-008). Each hull may have `commandPoints`, at least 1. The Command Station may have `commandPoints` at level 1 and at each of its levels, at least 1, as it has `nodes`, and the loader refuses them on any other kind. Both are optional: data without a station's `commandPoints` sets no cap, and a hull without them takes none. `FleetCap(tuning, level)` reads the cap as `NodeCap` reads the nodes, the highest a level up to the given one names.
2. **A warship counts from the moment its job starts.** A player's command points are those of its warships, and of each warship job one of its producers has started and paid for. A job is counted from its start rather than from when its ship appears, so two Shipyards cannot both start the last ship that fits. Ships placed whole by a test or a load (`SpawnShip`) count, but nothing stops their placement.
3. **A warship job that does not fit waits at the front of its queue.** `Simulation::Produce` checks the cap before it checks the Ore, so a waiting job is neither paid for nor started, and the jobs behind it wait too, as they wait behind a job short of Ore (ADR-016 decision 6). A ship finished earlier in the same tick has left its job but is not yet an entity, and is counted from the tick's deliveries. A queued job is never refused for the cap: it only waits, so that a player can queue ahead of a battle's losses.
4. **The snapshot tells the client** the player's command points and its cap, each hull's points in `HullView`, and each Command Station level's cap in `StructureLevelView`, beside its nodes. The protocol's version is 7.

## Consequences

- **The stress and measurement loads are unchanged.** They place their ships whole, so Q4's 200 ships are still measured, past any cap.
- **The AI has to play by it** (plan task 27.4): its attack group is a number of ships, which a cap in points can put out of reach.
- **The cap is per player, not per match.** On a larger map, or across a galaxy of systems (the horizon), the numbers are the data's to change.

## What this forecloses

- **Upkeep**, Ore paid over time for a fleet, which Horizon §6.2 also named. A second bound is a new decision.
- **A cap tied to the nodes held**, which Phase 4 design §5 turned down, without a new decision.
- **Refusing a queued job for the cap.**

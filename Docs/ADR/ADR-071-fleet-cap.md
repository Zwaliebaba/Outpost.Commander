# ADR-071 — A fleet cap in command points, bought with the Command Station's level, holds back a warship job until it fits

Status: **accepted** · 2026-10-06
Owns: fleet-cap, command-points

## Context

A player's fleet is bounded (Game §economy.fleetcap). Each warship takes command points by its hull, and the Command Station's level sets how many a player may have, so the cap is bought with the station's upgrades (owner, 2026-10-06). A Constructor takes none. A job that would go over the cap waits as a job short of Ore does, and the cap never removes a ship. The numbers are the tuning data's, and the Catalogue shows them. The server has to count a ship against the cap before it exists, so that two Shipyards cannot both start the last ship that fits, and the client has to be told the cap and why a job waits.

## Decision

1. **The numbers are data in `Tuning.json`** (ADR-008; owner, 2026-10-06). Each hull may have `commandPoints`, at least 1. The Command Station may have `commandPoints` at level 1 and at each of its levels, at least 1, as it has `nodes`, and the loader refuses them on any other kind. Both are optional: data without a station's `commandPoints` sets no cap, and a hull without them takes none. `FleetCap(tuning, level)` reads the cap as `NodeCap` reads the nodes, the highest a level up to the given one names. A player without a Command Station has level 1's cap.
2. **A warship counts from the moment its job starts.** A player's command points are those of its warships, and of each warship job one of its producers has started and paid for. A job is counted from its start rather than from when its ship appears, so two Shipyards cannot both start the last ship that fits. Ships placed whole by a test or a load (`SpawnShip`) count, but nothing stops their placement.
3. **A warship job that does not fit waits at the front of its queue.** `Simulation::Produce` checks the cap before it checks the Ore, so a waiting job is neither paid for nor started, and the jobs behind it wait too, as they wait behind a job short of Ore (ADR-016 decision 6). A ship finished earlier in the same tick has left its job but is not yet an entity, and is counted from the tick's deliveries. A queued job is never refused for the cap: it only waits, so that a player can queue ahead of a battle's losses.
4. **The snapshot tells the client** the player's command points and its cap, each hull's points in `HullView`, and each Command Station level's cap in `StructureLevelView`, beside its nodes. The client shows the cap in the status panel's fleet line ([ADR-066](ADR-066-production-status-on-the-hud.md) decision 1), marks a warship job that waits for it WAITING FOR FLEET CAP in the production window, and says on a Shipyard's panel that its job is waiting for the fleet cap.

## Consequences

- **The stress scene and the measurement load place their ships whole, so the cap never stops them.**
- **How the AI plays within the cap** is [ADR-020](ADR-020-ai-and-match-flow.md) decision 15's and [ADR-041](ADR-041-ai-plays-a-longer-match.md) decision 1's.
- **The cap is per player, not per match.** On a larger map, or across a galaxy of systems, the numbers are the data's to change.

## What this forecloses

- **Upkeep**, Ore paid over time for a fleet (owner, 2026-10-06). A second bound is a new decision.
- **A cap tied to the nodes held** (owner, 2026-10-06), without a new decision.
- **Refusing a queued job for the cap.**

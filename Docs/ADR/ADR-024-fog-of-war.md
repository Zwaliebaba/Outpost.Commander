# ADR-024 — Fog of war: each side sees what its ships and structures see, remembers enemy structures, and sees a shooter that hits it

Status: **accepted** · 2026-10-08
Owns: fog-of-war, sight-rules, shooter-reveal, snapshot-under-fog, attack-visibility, client-fog-grid

## Context

ADR-002 decision 4 builds a snapshot per player, so fog is added on the server alone. The owner made these choices (owner, 2026-10-02):

- **Sight is the weapon's range and a margin.** An armed ship or structure sees its weapon's range plus a margin, and anything without a weapon sees a fixed distance. A ship with a module that sees further, the Sensor Array, sees as far as the module does ([ADR-058](ADR-058-modules.md)).
- **Warzone-style memory.** Ground never seen is dark, and ground seen before is dimmed. Enemy structures show where they were last seen, and enemy ships only while in sight.
- **A shooter is revealed to the side it hits**, for a while after each hit. Without this, a design with a longer weapon sees and shoots a shorter-ranged one from outside that one's sight: with the tuning data, the Lance line hits the swarm from 220 m, and the swarm sees 170 m.
- **The fog darkens the ground** in the world and on the minimap.
- **The balance check runs under fog.**

## Decision

1. **The numbers are in `Tuning.json`'s `sight` object** (ADR-008): `weaponMarginMeters`, `unarmedMeters` and `shotRevealSeconds`. The loader requires all three and takes only positive numbers. A shooter is revealed for `shotRevealSeconds` after each hit, so one that fires more often than that stays revealed while it keeps firing, and one that fires less often is seen only for that time after each of its hits. That is meant: the Rail Cannon, firing every 6 s against a 3 s reveal, is seen half the time it fires from beyond its target's sight (owner, 2026-10-25).
2. **`Simulation::UseFog` turns it on, and every match and world uses it.** `InProcessServer` calls it after `UseTuning` for every server but a battle matchup's, which is fought in the open ([ADR-083](ADR-083-battle-matchups.md) decision 2). Without it, every player sees everything, as the movement, combat and balance-check tests assume. It needs the tuning data, and it throws without it.
3. **Sight is measured from an entity's center to the other's edge.** An armed ship or structure sees its weapon's range plus `weaponMarginMeters`, and anything without a weapon `unarmedMeters`; a ship whose module sees further sees as far as the module does ([ADR-058](ADR-058-modules.md) decision 5). Each player sees an enemy entity that is within the sight of one of its own ships or structures, sight plus the enemy's footprint radius, or that has hit it within `shotRevealSeconds`. A structure under construction carries no weapon yet, so it sees as an unarmed one. The map's asteroids and fields are always seen. On a map with territory, a player also sees all of each sector it holds that is not suppressed ([ADR-056](ADR-056-territory.md) decision 9). The pirates are seen and remembered as an enemy is, and see nothing themselves ([ADR-073](ADR-073-pirates.md)). A derelict is seen and remembered as an enemy's structure is, with what it holds ([ADR-074](ADR-074-salvage.md)).
4. **Fog never changes what anything fires at.** Every armed entity sees beyond its weapon's range, so anything in range is in sight, and targeting does not consult vision at all. A battle therefore plays tick for tick the same with fog and without: `FogTests.NeverChangesABattle` compares every entity after a minute of battle, and `BalanceCheckTests.FogChangesNoBattle` checks the balance check's own battles. A splash hit reveals the shooter to every side it hits.
5. **Vision is settled at the end of each tick**, after everything has moved, and is part of the simulation's state. Each player keeps:
   - the enemy entities it sees, in identifier order;
   - the enemy structures it has seen, as it last saw them, and the tick of the last snapshot that showed each ([ADR-081](ADR-081-territory-and-memory-over-the-fog.md));
   - the shooters revealed to it, and the tick each reveal fades on.

   A remembered structure is forgotten once its place is in sight and it is not seen there, which is how a player learns that it is gone. The state is in `Simulation`'s equality and replays from the command log like the rest (ADR-009).
6. **A snapshot under fog holds:**
   - the player's own entities, with each one's `sightMeters`;
   - the asteroids and fields;
   - the enemy entities it sees, with their components;
   - the enemy structures it remembers and does not see, as last seen, marked `remembered`, with the tick it last saw them (`lastSeenTick`, [ADR-081](ADR-081-territory-and-memory-over-the-fog.md)).

   Entities stay in identifier order, as `SnapshotInterpolator` pairs them by it. An enemy structure shows no queue or research. A shot is shown when the player sees its shooter or its target, and a destruction when it happened within the player's sight. `fogOfWar` tells the client to draw the fog.
7. **An attack order needs a target the player sees, or an enemy structure it remembers.** Anything else is refused with `CommandResult::NotVisible`. An attack on a ship ends when no entity of the attacker's side sees it any more. An attack on a remembered structure goes where the player saw it. The server still refuses a structure placed on something the player cannot see, such as an enemy rig. The player is told of neither refusal: the order is simply not obeyed, as for any refused command (ADR-009; ADR-020 decision 7).
8. *(retired)*
9. **The client draws the fog.** `Outpost::FogOfWar` in `GameApp` keeps a 20 m grid over the map. Every cell whose center is within the sight of one of the player's own entities, or inside a sector it holds that is not suppressed (ADR-056), is clear and marked seen. A cell seen before is shaded 0.55, and one never seen 0.9; on the ground, what was seen before is darkened further ([ADR-081](ADR-081-territory-and-memory-over-the-fog.md) decision 6). When the grid is brought up to date, and how its shades reach the GPU, is [ADR-052](ADR-052-fog-texture.md)'s. The grid is the client's alone, a presentation (owner, 2026-10-25): the server neither sends nor saves which cells were seen, so a game that joins a world, or joins it again, starts the grid afresh, while the server's memory of what the player has seen (decision 5) arrives whole in its first snapshot.
   - **In the world**, `Neuron::GroundMaskPipeline` in `NeuronClient` draws the grid over the ground plane after the scene, the effects and the glows, and before the HUD. It is black, as opaque as the shade, and blended between cell centers so that no cell's edge shows. It has no depth test, so what stands in the fog is darkened with it. The territory and what the player only remembers are drawn over it, after it ([ADR-081](ADR-081-territory-and-memory-over-the-fog.md)).
   - **On the minimap**, the HUD draws the same fog over the marks ([ADR-052](ADR-052-fog-texture.md) decision 4).
   - **Outside the map** there is no fog: it is the sky.

## Consequences

- **Every match is played under fog.** So is the stress scene (`--stress`), which builds both players' snapshots. Vision is computed every tick for each player: each enemy entity is checked against each of the player's own, and so is each remembered structure.
- **The balance check's report is the same with fog and without** (`BalanceCheckTests.FogChangesNoBattle`).
- **The fog is only as good as the client is honest.** The server never sends what a player does not see, apart from the asteroids. A client that ignores `fogOfWar` sees nothing more. The fog the client draws is presentation.

## What this forecloses

- Line of sight, height or terrain that blocks sight. Sight is a circle.
- Sight that research changes, or that a component changes other than by a module's sight ([ADR-058](ADR-058-modules.md) decision 5), or that differs from the weapon's range by anything but the one margin.
- Fog that changes what a ship fires at. Targeting never consults vision, as long as the margin is positive, which the loader enforces.
- Sharing vision between allies: each player sees alone, and the pirates see nothing.
- Telling a client why a command was refused. The protocol has no way to.

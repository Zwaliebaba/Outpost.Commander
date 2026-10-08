# ADR-024 — Fog of war: each side sees what its ships and structures see, remembers enemy structures, and sees a shooter that hits it

Status: **accepted** · 2026-10-02

## Context

Design §4 and §13 named fog of war the first feature after the MVP, and ADR-002 decision 4 shaped the server for it: a snapshot is built per player, so fog is added on the server alone. The owner asked for it on 2026-10-02 and made these choices:

- **Sight is the weapon's range and a margin.** An armed ship or structure sees its weapon's range plus 50 m. Anything without a weapon sees 200 m. A ship with a module that sees further, the Sensor Array, sees as far as the module does ([ADR-058](ADR-058-modules.md)).
- **Warzone-style memory.** Ground never seen is dark, and ground seen before is dimmed. Enemy structures show where they were last seen, and enemy ships only while in sight.
- **A shooter is revealed to the side it hits**, for a while after each hit. Without this, a design with a longer weapon sees and shoots a shorter-ranged one from outside that one's sight: the Lance line hits the swarm from 220 m, and the swarm sees 170 m.
- **The AI plays under fog.** It counters what it has seen, and builds its default design until it has seen anything.
- **The fog darkens the ground** in the world and on the minimap.
- **The Q2 check runs under fog**, and its result is recorded in design §12.

## Decision

1. **The numbers are in `Tuning.json`'s `sight` object** (ADR-008): `weaponMarginMeters` 50, `unarmedMeters` 200 and `shotRevealSeconds` 3. Three seconds is longer than the slowest fire interval, the Lance's 2.7 s, so a ship that keeps firing stays revealed. The loader requires all three and takes only positive numbers.
2. **`Simulation::UseFog` turns it on, and every match uses it.** `InProcessServer` calls it after `UseTuning`. Without it, every player sees everything, as the movement, combat and Q2 tests have always assumed. It needs the tuning data, and it throws without it.
3. **Sight is measured from an entity's center to the other's edge.** Each player sees an enemy entity that is within the sight of one of its own ships or structures, sight plus the enemy's footprint radius, or that has hit it within `shotRevealSeconds`. A structure under construction carries no weapon yet, so it sees as an unarmed one. The map's asteroids and fields are always seen. On a map with territory, a player also sees all of each sector it holds that is not suppressed ([ADR-056](ADR-056-territory.md) decision 9). The pirates are seen and remembered as an enemy is, and see nothing themselves ([ADR-073](ADR-073-pirates.md)). A derelict is seen and remembered as an enemy's structure is, with what it holds ([ADR-074](ADR-074-salvage.md)).
4. **Fog never changes what anything fires at.** Every armed entity sees beyond its weapon's range, so anything in range is in sight, and targeting does not consult vision at all. A battle therefore plays tick for tick the same with fog and without: `FogTests.NeverChangesABattle` compares every entity after a minute of battle, and `BalanceCheckTests.FogChangesNoBattle` checks the Q2 check's own battles. The full Q2 check under fog is recorded in design §12. A splash hit reveals the shooter to every side it hits.
5. **Vision is settled at the end of each tick**, after everything has moved, and is part of the simulation's state. Each player keeps:
   - the enemy entities it sees, in identifier order;
   - the enemy structures it has seen, as it last saw them, and the tick of the last snapshot that showed each ([ADR-080](ADR-080-territory-and-memory-over-the-fog.md));
   - the shooters revealed to it, and the tick each reveal fades on.

   A remembered structure is forgotten once its place is in sight and it is not seen there, which is how a player learns that it is gone. The state is in `Simulation`'s equality and replays from the command log like the rest (ADR-009).
6. **A snapshot under fog holds:**
   - the player's own entities, with each one's `sightMeters`;
   - the asteroids and fields;
   - the enemy entities it sees, with their components;
   - the enemy structures it remembers and does not see, as last seen, marked `remembered`, with the tick it last saw them (`lastSeenTick`, [ADR-080](ADR-080-territory-and-memory-over-the-fog.md)).

   Entities stay in identifier order, as `SnapshotInterpolator` pairs them by it. An enemy structure shows no queue or research. A shot is shown when the player sees its shooter or its target, and a destruction when it happened within the player's sight. `fogOfWar` tells the client to draw the fog.
7. **An attack order needs a target the player sees, or an enemy structure it remembers.** Anything else is refused with `CommandResult::NotVisible`. An attack on a ship ends when no entity of the attacker's side sees it any more. An attack on a remembered structure goes where the player saw it. The server still refuses a structure placed on something the player cannot see, such as an enemy rig. The player learns only that the order was refused, which is no more than the AI already infers (ADR-020 decision 7).
8. **The AI plays by what it sees:**
   - **Counters.** At each review it counters the most common design among the enemy warships it has seen since the last review, and keeps its design when it has seen none. At the start of a match that is the default design.
   - **Base plan.** It plans its contested asteroids against the enemy's Command Station where it sees one. At the start of a match it sees none, so it takes the enemy's base to be across the map's center from its own, since the map is point-symmetric (design §4).
   - **Attack.** The attack group goes only for an enemy structure it sees or remembers, production first ([ADR-037](ADR-037-losing-all-production.md) decision 4). When it knows none, it goes across the center to look.
   - A rig refused because an enemy rig it cannot see holds the asteroid is handled by ADR-020 decision 7, as any refused site is. Against a passive player who already holds every contested asteroid, the AI still wins.
9. **The client draws the fog.** `Outpost::FogOfWar` in `GameApp` keeps a 20 m grid over the map. Every cell whose center is within the sight of one of the player's own entities, or inside a sector it holds that is not suppressed (ADR-056), is clear and marked seen. A cell seen before is shaded 0.55, and one never seen 0.9; on the ground, what was seen before is darkened further, to 0.8 ([ADR-080](ADR-080-territory-and-memory-over-the-fog.md)). When the grid is brought up to date, and how its shades reach the GPU, is [ADR-052](ADR-052-fog-texture.md)'s.
   - **In the world**, `Neuron::GroundMaskPipeline` in `NeuronClient` draws the grid over the ground plane after the scene, the effects and the glows, and before the HUD. It is black, as opaque as the shade, and blended between cell centers so that no cell's edge shows. It has no depth test, so what stands in the fog is darkened with it. The territory and what the player only remembers are drawn over it, after it ([ADR-080](ADR-080-territory-and-memory-over-the-fog.md)).
   - **On the minimap**, the HUD draws the same fog over the marks ([ADR-052](ADR-052-fog-texture.md) decision 4).
   - **Outside the map** there is no fog: it is the sky.

## Consequences

- **Every match is played under fog.** So is the Q4 stress scene (`--stress`), which builds both players' snapshots. Vision is computed every tick for each player: each enemy entity is checked against each of the player's own, and so is each remembered structure.
- **That costs about 0.03 ms a tick.** `StressLoadTests` ran 1,200 ticks of the stress scene three times with fog and three times without, in the Linux container (g++ 13, `-O2`).
  - With fog: a median tick of 0.19–0.20 ms, and 0.29–0.47 ms at the 99th percentile.
  - Without: 0.16–0.18 ms, and 0.24–0.44 ms.
  - Both fired the same 2,020 shots and destroyed the same 340 ships and structures.
  - The development machine has not run it.
- **The Q2 check passes as before.** The full check's report is byte-identical with fog and without (design §12).
- **The AI's results barely move.** It beats a player who does nothing at tick 6,516 (5:26), against 6,483 without fog. It beats one who holds the middle at tick 7,360. Both were measured in `AiPlayerTests` in the Linux container, which the simulation's determinism makes the game's figures (ADR-009).
- **The fog is only as good as the client is honest.** The server never sends what a player does not see, apart from the asteroids. A client that ignores `fogOfWar` sees nothing more. The fog the client draws is presentation.

## What this forecloses

- Line of sight, height or terrain that blocks sight. Sight is a circle (design §4).
- Sight that research or a component changes, or that differs from the weapon's range by anything but the one margin.
- Fog that changes what a ship fires at. Targeting never consults vision, as long as the margin is positive, which the loader enforces.
- Sharing vision between allies. There are two players, each alone.
- Telling a client why a command was refused. The protocol still has no way to.

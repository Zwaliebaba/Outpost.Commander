# ADR-073 — Pirates are a neutral owner whose outposts the seed places and a rule guards

Status: **accepted** · 2026-10-07
Owns: pirates, pirate-outpost-layout, pirate-guard-rule, guarded-sector

## Context

The map holds pirates. They are a neutral owner in the simulation, not a player: they have no Ore, production or research, and they send no commands. An outpost sits on a sector's node, holds the node for no one, and keeps the sector from being claimed until its structures are destroyed. A camp is a Defence Platform with Small ships round it. A stronghold is two Defence Platforms with Medium ships, one of them with a Lance, and guards a rich sector or the center. An outpost's ships attack any player's ship or structure within `guardMeters` of the node, chase it no further than `chaseMeters`, and go back. They never raid and never leave the sector. A cleared outpost stays cleared and leaves a derelict. Pirates are drawn with an existing mesh set in a third color, and fog of war hides them as it hides an enemy.

The owner decided on 2026-10-07:

- On each side, a camp goes in two of the four contested sectors, drawn by the seed. A stronghold goes on both rich corners and on the center.
- Pirates fight at the base level, with no research.
- The AI clears them with a detachment ([ADR-076](ADR-076-ai-plays-phase-4.md) decision 2).
- Every player sees which sectors are guarded (decision 7).

The simulation therefore needs a way to hold an owner that is no player, a layout for an outpost round its node, a guarding rule it applies tick by tick, and a way for a client to learn that a sector cannot be claimed.

## Decision

1. **The pirates are one owner, `PIRATES`, a valid identifier that no player has.**
   - It is `PlayerId` 0xFFFFFFFF, in `GameProtocol`, so no seat is ever given it.
   - It has no `PlayerState`, so it has no Ore, no research, no vision and no snapshot.
   - Every rule that takes an owner that is valid and not the player's as the enemy takes the pirates as every player's enemy, with no change to the rule. That covers targeting, splash, attack orders, a hold's answer, suppression, and fog of war, under which they are seen and remembered as an enemy is.
   - Their designs are saved under `PIRATES` at the base level, from `DesignStatsFor` without upgrades. Their structures have the base hit points, since an owner that is not added has researched nothing.
   - All outposts share the one owner, so pirates never fight each other.
2. *(retired)*
3. **The tuning data gives each size its makeup.** `Tuning.json`'s optional `"pirates"` holds `guardMeters` and `chaseMeters`, and the loader requires the chase to reach at least as far as the guard. Each outpost has its `name`, its `defensePlatforms`, its `ships` by hull, drive and weapon, and its `wreck` ([ADR-074](ADR-074-salvage.md) decision 3). The server checks each outpost the map names against the tuning data.
4. **An outpost is laid out facing the map's center, so that an outpost and its mirror's stand the same, turned half a turn.**
   - Its Defence Platforms stand in a row through the node, across the line to the center, 140 m apart. A camp's one platform stands on the node.
   - Its ships stand in a ring 120 m round the node, the first on the side facing the center.
   - The center is its own mirror, so its ring faces +x. Its Lance ship, the fourth in the ring, stands across from a Mass Driver ship, nearer player 1's start than player 2's: about 170 m nearer, computed from `Map.json`'s starts and the ring. That is the one asymmetry.
   - `Simulation::PlacePirates` throws if an outpost's chase would leave its sector, or if a platform would overlap an obstacle.
5. **Once a second, each outpost's ships act as one** (`Simulation::GuardOutposts`), after the standing orders ([ADR-059](ADR-059-alerts-and-standing-orders.md)):
   - A second in which any of them is still planning a large order's paths is skipped ([ADR-032](ADR-032-order-ticks.md)).
   - Their quarry is dropped once it is gone, or more than `chaseMeters` from the node.
   - Without a quarry, they take the player's ship or structure nearest the node within `guardMeters`, measured to its center.
   - With a quarry, they attack-move at it unless they are already engaged: one of them has a target, or is attack-moving to within `STANDING_RESEND_METERS`, 150 m, of the quarry.
   - With none, and not already headed home, they move back to the node once the group's center is more than `HOLD_RETURN_METERS`, 200 m, from it.
   - They see through fog of war, as a Defence gun does.
   - The chase is less than a sector's half-width, which `Simulation::PlacePirates` checks (decision 4), so they never leave their sector and never raid.
   - The Defence Platforms fire as any armed structure does.
6. **A sector is guarded while a pirate structure stands in it.**
   - A Relay is refused there as `Guarded`. The ships an outpost has left do not stop a claim once its structures have fallen, but they suppress the Relay as any enemy warship does ([ADR-056](ADR-056-territory.md)).
   - The node of a guarded sector has no holder, so it counts for no one in domination ([ADR-057](ADR-057-domination.md)).
   - Territory works out the flag at the end of every tick, in one pass over the entities.
7. **Every player sees which sectors are guarded:** `SectorView::guarded`, as it sees each sector's holder (owner, 2026-10-07).
   - The server reports no refused order, so without the flag a Relay ordered into an unscouted camp would do nothing and show nothing.
   - The pirates themselves stay under fog of war.
   - A Relay's ghost is red in a guarded sector.
8. **The outposts are placed once, when the server is made, and only for a match or a universe.** `InProcessServer` places them after the players are added, and the derelicts with them ([ADR-074](ADR-074-salvage.md)); nothing places one later, so a cleared outpost stays cleared. The measurement load, the stress scene and a battle matchup place none ([ADR-083](ADR-083-battle-matchups.md) decision 2).
9. *(retired)*
10. **The client draws pirates in violet.** `Models.json`'s `"Pirate"` set borrows the Tarkan meshes in a color of its own, and `"pirates"` names it as the set the pirates are drawn with ([ADR-011](ADR-011-meshes-and-shading.md) decisions 2 and 3). On the minimap their ships and structures are violet, and so are the wash and the outline of a guarded sector ([ADR-081](ADR-081-territory-and-memory-over-the-fog.md)); a guarded sector counts for neither side.
11. *(retired)*

## Consequences

- **A first fleet of Small ships clears a camp with losses when it carries Lances.** A Defence Platform's `armor` takes most of each Mass Driver hit's `damage` (both in `Tuning.json`), so Swarms fare worse than Pickets. `PirateTests.AFirstFleetClearsACampWithLosses` checks that four Pickets clear a camp with losses and three do not. Its ships are set never to retreat ([ADR-075](ADR-075-repair-and-retreat.md)), so they fight to the end. The pirates' own ships never retreat.
- **Fleets crossing the map fight the outposts on their way,** since nothing routes them round ([ADR-076](ADR-076-ai-plays-phase-4.md) decision 7).
- **Replay holds.** The outposts, their quarries and the guarded flags are state, compared with the rest, and `InProcessServerTests` replays a match with pirates from its command log.
- **The territory tests play without pirates.** `TerritoryMatch` takes the map's outposts out unless a test asks for them, since the center's stronghold sits where those tests send their ships. `AiPlayerTests` and the outcome tests keep them.

## What this forecloses

- Pirates as a player: they have no client and send no commands.
- More than one pirate side.
- Hidden outposts: which sectors are guarded is known from the start.
- An outpost anywhere but on a node.

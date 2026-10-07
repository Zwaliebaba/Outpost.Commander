# ADR-073 — Pirates are a neutral owner whose outposts the seed places and a rule guards

Status: **accepted** · 2026-10-07

## Context

Phase 4 design §8 (gate L7) adds pirates. They are a neutral owner in the simulation, not a player: they have no Ore, production or research, and they send no commands. An outpost sits on a sector's node, holds the node for no one, and keeps the sector from being claimed until its structures are destroyed. A camp is one Defence Platform and three Small Mass Driver ships. A stronghold is two Defence Platforms and four Medium ships, one of them with a Lance, and guards a rich sector or the center. An outpost's ships attack any player's ship or structure within 600 m of the node, chase it no further than 900 m, and go back. They never raid and never leave the sector. A cleared outpost stays cleared and leaves a derelict. Pirates are drawn with an existing mesh set in a third color, and fog of war hides them as it hides an enemy.

The owner decided on 2026-10-07:

- On each side, a camp goes in two of the four contested sectors, drawn by the seed. A stronghold goes on both rich corners and on the center.
- Pirates fight at the base level, with no research.
- The derelict a cleared outpost leaves waits for milestone 31.
- Until milestone 33, the AI leaves pirate sectors alone.
- Every player sees which sectors are guarded (decision 7).

What the design leaves open: how the neutral owner is represented, how an outpost stands round its node, what the guarding rule does tick by tick, how a client learns that a sector cannot be claimed, and what the client and the AI do in the meantime.

## Decision

1. **The pirates are one owner, `PIRATES`, a valid identifier that no player has.**
   - It is `PlayerId` 0xFFFFFFFF, in `GameProtocol`, so no seat is ever given it.
   - It has no `PlayerState`, so it has no Ore, no research, no vision and no snapshot.
   - Every rule that takes an owner that is valid and not the player's as the enemy takes the pirates as every player's enemy, with no change to the rule. That covers targeting, splash, attack orders, a hold's answer, suppression, and fog of war, under which they are seen and remembered as an enemy is.
   - Their designs are saved under `PIRATES` at the base level, from `DesignStatsFor` without upgrades. Their structures have the base hit points, since an owner that is not added has researched nothing.
   - All outposts share the one owner, so pirates never fight each other.
2. **The map places outposts by sector kind** ([ADR-072](ADR-072-seeded-placement.md), which owns the detail).
   - `Map.json`'s `"outposts"` give, for each size of outpost, a sector kind and a count of mirrored pairs of sectors.
   - The seed draws them after every asteroid, so a seed places the same asteroids with or without outposts. Each sector gets at most one outpost.
   - `Tools/MakeMap.py` writes the owner's choice for the repository map: a camp in two of the four `contested` sectors on each side, and a stronghold on the `rich` corners and the `center`.
3. **The tuning data gives each size its makeup.** `Tuning.json`'s optional `"pirates"` holds:
   - `guardMeters` 600 and `chaseMeters` 900. The loader requires the chase to reach at least as far as the guard.
   - Each outpost's `name`, its `defensePlatforms` and its `ships` by hull, drive and weapon.
   - A camp is 1 platform and 3 Small Ion Mass Driver ships. A stronghold is 2 platforms, 3 Medium Ion Mass Driver ships and 1 Medium Ion Lance ship.
   - The design names no drive, and the Ion drive is the only one open from the start.
   - The server checks each outpost the map names against the tuning data.
4. **An outpost is laid out facing the map's center, so that an outpost and its mirror's stand the same, turned half a turn.**
   - Its Defence Platforms stand in a row through the node, across the line to the center, 140 m apart. A camp's one platform stands on the node.
   - Its ships stand in a ring 120 m round the node, the first on the side facing the center.
   - The center is its own mirror, so its ring faces +x. Its Lance ship, the fourth in the ring, stands across from a Mass Driver ship, about 170 m nearer player 1's start than player 2's. That is the one asymmetry.
   - `Simulation::PlacePirates` throws if an outpost's chase would leave its sector, or if a platform would overlap an obstacle.
5. **Once a second, each outpost's ships act as one** (`Simulation::GuardOutposts`), after the standing orders ([ADR-059](ADR-059-alerts-and-standing-orders.md)):
   - Their quarry is dropped once it is gone, or more than the chase from the node.
   - Without a quarry, they take the player's ship or structure nearest the node within the guard, measured to its center.
   - With a quarry, they attack-move at it unless they are already engaged: one of them has a target, or is attack-moving to within 150 m of the quarry.
   - With none, and not already headed home, they move back to the node once the group's center is more than 200 m from it.
   - They see through fog of war, as a Defence gun does.
   - The chase, 900 m, is less than a sector's half-width of 1,000 m, so they never leave their sector.
   - The Defence Platforms fire as any armed structure does.
6. **A sector is guarded while a pirate structure stands in it.**
   - A Relay is refused there as `Guarded`. The ships an outpost has left do not stop a claim once its structures have fallen, but they suppress the Relay as any enemy warship does ([ADR-056](ADR-056-territory.md)).
   - The node of a guarded sector has no holder, so it counts for no one in domination ([ADR-057](ADR-057-domination.md)).
   - Territory works out the flag at the end of every tick, in one pass over the entities.
7. **Every player sees which sectors are guarded:** `SectorView::guarded`, as it sees each sector's holder (owner, 2026-10-07).
   - The server reports no refused order, so without the flag a Relay ordered into an unscouted camp did nothing and showed nothing, and the AI ordered one every second.
   - The pirates themselves stay under fog of war.
   - A Relay's ghost is red in a guarded sector.
   - `PROTOCOL_VERSION` goes from 7 to 8.
8. **A measurement or stress run places no pirates.** It is not a match, and its scenes stay as they were measured (task 2.7, task 3.7).
9. **Until milestone 33, the AI leaves pirates alone** ([ADR-020](ADR-020-ai-and-match-flow.md) decision 13).
   - It neither plans nor claims a Relay or a rig in a guarded sector, and a claim that is guarded is dropped and another made in its place.
   - It counts no pirate warship as the enemy's fleet to answer, and picks no pirate structure to attack.
   - Its fleets are not routed round the outposts, so a group whose way passes within 600 m of a node fights the outpost there.
10. **The client draws them in violet** ([ADR-011](ADR-011-meshes-and-shading.md)).
    - `Models.json`'s `"Pirate"` set names `"meshes": "Tarkan"`: it borrows that set's meshes and models in a color of its own. `"pirates"` names it as the set the pirates are drawn with.
    - On the minimap their ships and structures are violet, and so are the wash and the outline of a guarded sector. A guarded sector counts for neither side.
    - An alert names pirate ships as pirates.
    - The match log leaves out every shot by or at a pirate, so that its contacts and engagements stay the players' until milestone 34 counts the pirates.
11. **A cleared outpost leaves a derelict** where its last Defence Platform fell: 600 Ore for a camp and 1,200 for a stronghold, no topic. A camp's covers its node until it is salvaged ([ADR-074](ADR-074-salvage.md), which owns the detail).

## Consequences

- **A first fleet of Pickets fits §8, and one of Swarms does not.** Measured headless in the Linux container on the repository map, seed 3. Blue's ships of one design attack-move on an outpost's node from 850 m, sent again every 5 seconds, for up to 4 minutes:

  | Ships | Camp | Stronghold |
  |---|---|---|
  | Swarm, Small Mass Driver | 6 lose; 8 clear it, 3 lost | 10 lose |
  | Picket, Small Lance | 3 lose; 4 clear it, 1 lost; 6, none lost | 6 lose; 8 clear it, 2 lost; 10, none lost |
  | Brawler, Medium Mass Driver | 3 lose; 4 clear it, 2 lost | 8 lose; 10 clear it, 5 lost |
  | Lancer, Medium Lance | 3 clear it, none lost | 3 lose; 4 clear it, 3 lost |

  A Mass Driver's 14 damage does a Defence Platform, armor 10, 4 a hit. §8's "a first fleet of 4–6 Small ships clears it with losses" therefore holds for Pickets, not for Swarms. The owner kept the numbers and reworded §8 to say so (2026-10-07). `PirateTests` keeps the Picket result. The ships fight to the end: since milestone 32 a ship turns for home below a quarter of its hit points unless set never to, and the test sets them so ([ADR-075](ADR-075-repair-and-retreat.md)). The pirates' own ships never retreat.
- **The AI no longer feeds Constructors to the camps.** In 25 minutes on seed 3, with eight claims allowed, an AI that tried the guarded sectors ordered 145 Relays there and held 7 sectors. This one orders none and holds 9.
- **AI against AI over seeds 1–40** (design §2):
  - 37 matches end, 3 of them by production, the first such endings on the 10 km map. 3 are still level at three hours: the AI attacks only with a lead in nodes, and the guarded sectors were the ones that broke a tie.
  - At the end, the median match still has 7.5 of the 10 pirate structures and 14 of the 24 pirate ships: fleets crossing the map fight the outposts on their way.
- **Replay holds.** The outposts, their quarries and the guarded flags are state, compared with the rest, and `InProcessServerTests` replays a match with pirates from its command log.
- **The territory tests play without pirates.** `TerritoryMatch` takes the map's outposts out unless a test asks for them, since the center's stronghold sits where those tests send their ships. `AiPlayerTests` and the outcome tests keep them.

## What this forecloses

- Pirates as a player: they have no client and send no commands.
- More than one pirate side.
- Hidden outposts: which sectors are guarded is known from the start.
- An outpost anywhere but on a node.

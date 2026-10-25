# ADR-056 — Territory: a Relay holds its sector, the lattice grows from home, ore belongs to sectors, and an enemy warship suppresses a Relay

Status: **accepted** · 2026-10-25
Owns: territory, relay-claims, relay-suppression, cut-off-sectors, sector-income, sector-sight

## Context

Territory is what the players fight over (Game §territory). Which sectors a player holds decides where its rigs earn and its Shipyards stand, what it sees, and, in a match, who wins by domination ([ADR-057](ADR-057-domination.md)). The map's sectors are data (ADR-036). The rules have to say what holds a sector, when holding is worked out, what a snapshot shows of it, and how a map without sectors plays.

## Decision

1. **Territory is on when the map has sectors and the simulation has the tuning data** (`Simulation::HasTerritory`). The repository's map has sectors, so every match plays with territory. A map without them plays without it: no Relay can be built (`NotBuildable`), and a rig earns wherever it stands.
2. **The numbers are data** (owner, 2026-10-03). The Relay is a structure kind in `Tuning.json`, unarmed, with its hit points, armor, footprint, cost and build time as every kind has them. A `territory` object holds `cutOffIncomePercent` and `suppressionRadiusMeters`, beside domination's numbers ([ADR-057](ADR-057-domination.md) decision 1). The loader requires both, takes a percentage of at most 100 and a positive radius, and requires the Relay as it requires every structure kind.
3. **Who holds a sector follows from what stands on its node.** A sector is held by the owner of the finished Relay or the Command Station within 1 m of its node. A site under construction holds nothing yet, but its footprint takes the node. A Command Station holds the sector whose node its start is, which on the repository map is its home's. A sector held by a Command Station is a home sector.
4. **A Relay snaps to the node of the sector it is ordered in**, as a rig snaps to its asteroid. The order is refused as `InvalidPlacement` when something stands on the node, as `NotAdjacent` when no adjacent sector is held by the player, as `Guarded` while a pirate structure stands in the sector ([ADR-073](ADR-073-pirates.md) decision 6), and as `CapReached` when the player has taken as many nodes as its Command Station's level allows ([ADR-064](ADR-064-structure-upgrades.md) decision 11), a Relay site counting since it takes its node. A player without a Command Station has level 1's cap. The cap refuses claims and never takes a node away. A suppressed Relay still holds its sector, so it still counts for adjacency.
5. **Ore belongs to sectors.** A Mining Rig may be ordered only onto an asteroid in a sector the player holds (`SectorNotHeld`). It may be ordered there while the sector is suppressed, and earns nothing until the suppression lifts (owner, 2026-10-25). A rig earns its rate times its sector's share: nothing outside a sector its owner holds or in a suppressed one, the tuning data's share in a sector that is cut off, and all of it otherwise. A rig draws its asteroid's reserve by what it earns, so a rig that earns nothing draws nothing. **So does production** (owner, 2026-10-06): a Shipyard may be ordered only in a sector the player holds (`SectorNotHeld`), however far from its home, so that production moves forward as the territory does. A Shipyard that stands in a sector its player loses keeps working. So does a Repair Bay, which takes the same rule ([ADR-075](ADR-075-repair-and-retreat.md)).
6. **A Relay is suppressed while an enemy warship is within the radius of it and none of its owner's is** (owner, 2026-10-03). Distances are between centers, as weapon range is. A warship is a ship of the warship role with hit points; a Constructor never suppresses, and nor does a ship placed without hit points. A Command Station is never suppressed.
7. **A held sector is cut off** when its holder holds no path of adjacent sectors from it to one of its home sectors. Suppressed sectors keep the path. A player whose Command Station has fallen has no home sector, so every sector it still holds is cut off ([ADR-037](ADR-037-losing-all-production.md) decision 2).
8. **Territory is worked out again at the end of every tick**, after every ship has moved and before vision is settled, and when a Command Station or Relay is placed whole. It follows from the entities, so it adds no state of its own to replay; it is kept in the simulation and compared with the rest.
9. **A held sector that is not suppressed is in its holder's sight, all of it.** An entity, an asteroid's reserve, a destruction and a remembered structure's place are seen when they stand in such a sector, as when they are within an observer's sight (ADR-024). A Relay is unarmed, so it sees `sight.unarmedMeters` of its own, which is all it sees while suppressed. The home sector is never suppressed, so a player always sees its whole home sector.
10. **Every snapshot carries every sector** (`Snapshot::sectors`): its rectangle, name, node and adjacency as the map gives them, its holder, and whether it is suppressed, cut off or guarded by pirates ([ADR-073](ADR-073-pirates.md)). Both players see all of it, fog of war or not: the territory is the map both sides play for, and domination counts it. A Relay itself is seen only as any structure is.
11. **The client shows territory.** The marks in the world and on the minimap are [ADR-081](ADR-081-territory-and-memory-over-the-fog.md)'s, and the nodes panel [ADR-057](ADR-057-domination.md) decision 8's. Beside those:
    - **The ghost** of a Relay stands on the node of the sector under the cursor, green only where the server would build it by what the snapshot shows, by `CanClaim` ([ADR-081](ADR-081-territory-and-memory-over-the-fog.md) decision 2). A rig's ghost, a Shipyard's and a Repair Bay's is green only in a sector the player holds. `PlaceGhost` takes the snapshot's sectors, the player and its node cap; at the cap a Relay's ghost is red everywhere (`AtNodeCap`), and the placing hint reads "Relay: at the node cap; upgrade the Command Station to claim more".
    - **The fog** clears every sector the player holds that is not suppressed, as the server's sight does (decision 9).
    - **A Relay's or a rig's panel** says what its sector earns: held, suppressed, cut off, or not held.
    - **A Constructor offers the Relay** only on a map with sectors, dim with NODE CAP at the cap.
    - **The Relay is drawn with the Research Lab's model, brighter** (its `tint` in `Models.json`).
12. *(retired)*

## Consequences

- **Tests.** `TerritoryTests` covers the home sectors, a Relay holding once built, the lattice, rigs and Shipyards needing a held sector, a rig earning while held and half while cut off, a rig in the enemy's sector, suppression starting and being lifted, the home sector never suppressed, a held sector seen whole, and a map without sectors. `TuningTests` covers the `territory` object and the Relay's kind. `PlacementTests`, `HudTests` and `FogOfWarTests` cover the client's part.

## What this forecloses

- A node held by anything but a Relay or a Command Station, and a Relay standing anywhere but on its node.
- Territory hidden by fog of war, without a new decision.

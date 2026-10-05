# ADR-056 — Territory: a Relay holds its sector, the lattice grows from home, ore belongs to sectors, and an enemy warship suppresses a Relay

Status: **accepted** · 2026-10-03

## Context

Phase 2 makes territory the thing that is fought over (Phase 2 design §1). The owner accepted the design on 2026-10-03 and decided its gates: J1, a cut-off sector earns half; J2, the Relay's numbers and the 400 m suppression radius, as starting values. The design says what holding, suppression and the lattice mean. It leaves open where the rules live, when they are worked out, what a snapshot shows of them, and how a map without sectors plays. The map's sectors are already data (ADR-036).

## Decision

1. **Territory is on when the map has sectors and the simulation has the tuning data** (`Simulation::HasTerritory`). The repository map has nine, so every match plays Phase 2. A map without them plays Phase 1's rules: no Relay can be built (`NotBuildable`), and a rig earns wherever it stands. The tests built on open ground keep their meaning that way.
2. **The numbers are data.** The Relay is a sixth structure kind in `Tuning.json`: 3,000 hit points, armor 10, a 30 m footprint, 200 Ore and 40 s of a Constructor's work, unarmed. A `territory` object holds `cutOffIncomePercent` 50 and `suppressionRadiusMeters` 400. The loader requires both, takes a percentage of at most 100 and a positive radius, and requires the Relay as it requires every structure kind.
3. **Who holds a sector follows from what stands on its node.** A sector is held by the owner of the finished Relay or the Command Station within 1 m of its node. A site under construction holds nothing yet, but its footprint takes the node. A Command Station holds the sector whose node its start is, which on the repository map is its home's. A sector held by a Command Station is a home sector.
4. **A Relay snaps to the node of the sector it is ordered in**, as a rig snaps to its asteroid. The order is refused as `InvalidPlacement` when something stands on the node, as `NotAdjacent` when no adjacent sector is held by the player, and as `CapReached` when the player has taken as many nodes as its Command Station's level allows, a Relay site counting since it takes its node; a player without a Command Station has level 1's cap ([ADR-064](ADR-064-structure-upgrades.md) decision 11). The cap refuses claims and never takes a node away. A suppressed Relay still holds its sector, so it still counts for adjacency.
5. **Ore belongs to sectors.** A Mining Rig may be ordered only onto an asteroid in a sector the player holds (`SectorNotHeld`). A rig earns its rate times its sector's share: nothing outside a sector its owner holds or in a suppressed one, the tuning data's share in a sector that is cut off, and all of it otherwise. A rig draws its asteroid's reserve by what it earns, so a rig that earns nothing draws nothing.
6. **A Relay is suppressed while an enemy warship is within the radius of it and none of its owner's is.** Distances are between centers, as weapon range is. A warship is a ship of the warship role with hit points; a Constructor never suppresses, and nor does a ship placed without hit points. A Command Station is never suppressed.
7. **A held sector is cut off** when its holder holds no path of adjacent sectors from it to one of its home sectors. Suppressed sectors keep the path. A player whose Command Station has fallen has no home sector, so every sector it still holds is cut off (gate H5: the station is lost for good).
8. **Territory is worked out again at the end of every tick**, after every ship has moved and before vision is settled, and when a Command Station or Relay is placed whole. It follows from the entities, so it adds no state of its own to replay; it is kept in the simulation and compared with the rest.
9. **A held sector that is not suppressed is in its holder's sight, all of it.** An entity, an asteroid's reserve, a destruction and a remembered structure's place are seen when they stand in such a sector, as when they are within an observer's sight (ADR-024). A Relay is unarmed, so it sees 200 m of its own, which is all it sees while suppressed. The home sector is never suppressed, so a player always sees its whole home sector.
10. **Every snapshot carries every sector** (`Snapshot::sectors`): its rectangle, name, node and adjacency as the map gives them, its holder, and whether it is suppressed or cut off. Both players see all of it, fog of war or not: the territory is the map both sides play for, and domination counts it. A Relay itself is seen only as any structure is.
11. **The client shows it** ([task 14.3](../../GameDesign/Archive/ImplementationPlan-Phase2.md)):
    - **The minimap** washes each held sector faintly in its holder's color under the marks, outlines it in that color over the fog, and stripes a suppressed one.
    - **A panel under the Ore and the status panel** ([ADR-066](ADR-066-production-status-on-the-hud.md)) counts the nodes each side holds, "Nodes of 9", the player's figure in its color, against its cap, "4 / 5", and the enemy's in theirs.
    - **The ghost** of a Relay stands on the node of the sector under the cursor, green only where the server would build it by what the snapshot shows. A rig's ghost is green only in a sector the player holds. `PlaceGhost` takes the snapshot's sectors and the player for both, and the player's cap, at which a Relay's ghost is red everywhere (`AtNodeCap`); the placing hint then says why.
    - **The fog** clears every sector the player holds that is not suppressed, as the server's sight does.
    - **A Relay's or a rig's panel** says what its sector earns: held, suppressed, cut off, or not held.
    - **A Constructor offers the Relay** only on a map with sectors, dim with NODE CAP at the cap.
    - **The Relay is drawn with the Research Lab's model, brighter** (`tint` 1.6 in `Models.json`), until the owner adds a model of its own.
12. **The AI builds a Relay before a rig in another sector** (task 14.5). Its plan puts a Relay slot on a sector's node before the first rig it plans there. A Relay waits, without holding up the rest of the plan, until its sector is next to one the AI holds, and a rig until its sector is held; a rig or Relay in a sector the enemy holds is blocked, as a rig on another's asteroid is. Milestone 18 makes it play for territory.

## Consequences

- **Tests.** `TerritoryTests` covers the home sectors, a Relay holding once built, the lattice, rigs needing a held sector, a rig earning while held and half while cut off, a rig in the enemy's sector, suppression starting and being lifted, the home sector never suppressed, a held sector seen whole, and a map without sectors. `TuningTests` covers the new object and kind. `PlacementTests`, `HudTests` and `FogOfWarTests` cover the client's part.
- **The AI's matches end, and later.** Measured in the Linux container with clang 18 at `-O2` (ADR-038), the ten AI-against-AI matches of seeds 1 to 10 all end, at a median of 45:48, from 31:03 to 1:26:51. Before this change the same seeds gave a median of 1:06:29, from 29:37 to 1:39:58. The AI's rigs away from home are all on its flanks, so its plan adds two Relays, 400 Ore, before them.
- **Not built or run on Windows here.** CI's build and tests are the first on Windows. The minimap, the panel and the Relay's look are the owner's run.

## What this forecloses

- A node held by anything but a Relay or a Command Station, and a Relay standing anywhere but on its node.
- Territory hidden by fog of war, without a new decision.

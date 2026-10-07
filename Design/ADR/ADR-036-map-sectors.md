# ADR-036 — The 5 km map names its sectors, nodes and adjacency, which Phase 1 does not read

Status: **accepted** · 2026-10-03

## Context

Phase 1 design §8 gives the 5 km map four rings of ore (owner, 2026-10-02, gate H3). On 2026-10-03 the owner decided that territory is Phase 2's, shaped now: the 5 km map is laid out in sectors with a node each, so that it is the first map Phase 2 plays (Phase 1 design §8; the Phase 2 draft, §4 and §11). Phase 2's rules for sectors are a draft: what a sector is has to be in the data before the rules that read it are decided. The map is data the game loads (ADR-008), checked by its loader and by `MapTests`.

## Decision

1. **A sector is a rectangle with one node site and a list of the sectors it is adjacent to**, in `Map.json`'s optional `"sectors"`: an `"id"`, a `"name"`, `"minXMeters"`, `"maxXMeters"`, `"minZMeters"`, `"maxZMeters"`, a `"node"` position and `"adjacent"` identifiers. A rectangle, not a polygon: the first map's sectors are a 3 × 3 grid, and a polygon is the change to make when a map needs one.
2. **The loader checks what one map file can get wrong on its own**: identifiers unique; each rectangle on the map, with its node inside it; each node the map's minimum gap clear of every asteroid and field, as a start is; each adjacency naming another sector, once, which names it back. **`MapTests` checks the repository map's layout**: its sectors tile the map without overlap; every asteroid and start lies in one; two sectors are adjacent exactly when they share a border; and every sector can be reached from both starts' sectors.
3. **The rings are yields of their own**, `"home"`, `"near"`, `"contested"` and `"rich"`, each with its rate in `Tuning.json`'s rules, 3.5, 4, 5.5 and 6.5 Ore a second since Phase 4 lowered them by about a third (Phase 4 design §4).
4. **The map**: 5,000 m a side, the starts at (−1,750, −1,750) and (1,750, 1,750). Nine sectors on a grid of thirds: each home in its corner with its three home asteroids, each player's two flanks with two of its near asteroids each, the six contested asteroids round the center, and two rich asteroids in each of the two empty corners, one nearer each start. Asteroid fields stand on the sector borders, leaving passages, and at the junctions. Each node is at its sector's center, the homes' at their starts.
5. **The client follows the map's size**: the camera's focus may go 2,500 m from the center, and the ground's grid covers 5 km.

## Consequences

- **Phase 1 read no sector.** Phase 2 plays its territory on them ([ADR-056](ADR-056-territory.md)).
- **The AI had to change for the bigger map**, not the rules: an attack-move ends at each ship's place in its group's formation, and round a Command Station those places can stand out of a Mass Driver's reach. On the 2 km map that cost nothing; on the 5 km map the AI's whole fleet stood idle 150 m from the station it had come for. The AI now orders the ships of its attack group within 500 m of their target structure to attack it, once each.
- **The layout is unconfirmed** until the owner's run (task 11.2). Gate H8 kept G3's 1,600 m zoom limit on it (owner, 2026-10-03).

## What this forecloses

- Sectors that are not rectangles, and adjacency that is not a shared border, without a new decision.

# ADR-036 — The map names its sectors, nodes and adjacency

Status: **accepted** · 2026-10-03

## Context

Phase 1 design §8 gave the 5 km map four rings of ore (owner, 2026-10-02, gate H3). On 2026-10-03 the owner decided that territory is Phase 2's, shaped now: the 5 km map is laid out in sectors with a node each, so that it is the first map Phase 2 plays (Phase 1 design §8; the Phase 2 draft, §4 and §11). Phase 2's rules for sectors are a draft: what a sector is has to be in the data before the rules that read it are decided. The map is data the game loads (ADR-008), checked by its loader and by `MapTests`.

## Decision

1. **A sector is a rectangle with one node site and a list of the sectors it is adjacent to**, in `Map.json`'s optional `"sectors"`: an `"id"`, a `"name"`, `"minXMeters"`, `"maxXMeters"`, `"minZMeters"`, `"maxZMeters"`, a `"node"` position and `"adjacent"` identifiers. A rectangle, not a polygon: the first map's sectors are a 3 × 3 grid, and a polygon is the change to make when a map needs one.
2. **The loader checks what one map file can get wrong on its own**: identifiers unique; each rectangle on the map, with its node inside it; each node the map's minimum gap clear of every asteroid and field, as a start is; each adjacency naming another sector, once, which names it back. **`MapTests` checks the repository map's layout**: its sectors tile the map without overlap; every asteroid and start lies in one; two sectors are adjacent exactly when they share a border; and every sector can be reached from both starts' sectors.
3. **The rings are yields of their own**, `"home"`, `"near"`, `"contested"` and `"rich"`, each with its rate in `Tuning.json`'s rules, 3.5, 4, 5.5 and 6.5 Ore a second since Phase 4 lowered them by about a third (Phase 4 design §4).
4. **The map** is Phase 4's 10 km system (Phase 4 design §6, gate L3): 10,000 m a side, the starts at (−4,000, −4,000) and (4,000, 4,000). Twenty-five sectors of 2 km on a 5 × 5 grid, named by column A to E from west to east and row 1 to 5 from south to north, with their identifiers row by row from A1's 1 to E5's 25. Each home is in its corner with its three home asteroids, which the map lists. Every other asteroid is placed from the match's seed by its sector's kind ([ADR-072](ADR-072-seeded-placement.md)), mirrored through the center: for player 1, two near asteroids in each of its flanks, B1 and A2, and one in each of C1, B2 and A3; one contested asteroid in each of D1, C2, B3 and A4, two in each of B4 and D2, and four in the center, C3; and two rich asteroids in the corner A5. That is 3 home, 7 near, 8 contested and 2 rich asteroids a player, 459,000 Ore in all, the yields and reserves Phase 1's. Two asteroid fields stand on every border between two sectors, 470 m in from each end, so that each border has a passage at its middle and one at each corner. Each node is at its sector's center, the homes' at their starts. The map is point-symmetric about its center, and `Tools/MakeMap.py` writes it.
5. **The client follows the map's size**: the camera's focus may go 5,000 m from the center, the ground's grid covers 10 km, and the far plane reaches 8,000 m beyond the focus (Phase 4 plan task 28.1).

## Consequences

- **Phase 1 read no sector.** Phase 2 plays its territory on them ([ADR-056](ADR-056-territory.md)).
- **The AI had to change for the bigger map**, not the rules: an attack-move ends at each ship's place in its group's formation, and round a Command Station those places can stand out of a Mass Driver's reach. On the 2 km map that cost nothing; on the 5 km map the AI's whole fleet stood idle 150 m from the station it had come for. The AI now orders the ships of its attack group within 500 m of their target structure to attack it, once each.
- **The layout is unconfirmed** until the owner's run (task 11.2). Gate H8 kept G3's 1,600 m zoom limit on it (owner, 2026-10-03).

## What this forecloses

- Sectors that are not rectangles, and adjacency that is not a shared border, without a new decision.

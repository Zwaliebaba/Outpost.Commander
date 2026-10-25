# ADR-036 — The map names its sectors, nodes and adjacency

Status: **accepted** · 2026-10-07
Owns: map-sectors, map-layout, ore-yields

## Context

Territory is played on sectors: each has a node, and two sectors that share a border are next to one another ([ADR-056](ADR-056-territory.md)). What a sector is has to be in the map's data before the rules that read it. The map's ore lies in rings, from the homes outward, and each ring earns at a rate of its own. The map is data the game loads (ADR-008), checked by its loader and by `MapTests`.

## Decision

1. **A sector is a rectangle with one node site and a list of the sectors it is adjacent to**, in `Map.json`'s optional `"sectors"`: an `"id"`, a `"name"`, `"minXMeters"`, `"maxXMeters"`, `"minZMeters"`, `"maxZMeters"`, a `"node"` position and `"adjacent"` identifiers, and a `"kind"` where the seed places its content ([ADR-072](ADR-072-seeded-placement.md) decision 1). A rectangle, not a polygon: the map's sectors are a grid, and a polygon is the change to make when a map needs one.
2. **The loader checks what one map file can get wrong on its own**: identifiers unique; each rectangle on the map, with its node inside it; each node the map's minimum gap clear of every asteroid and field, as a start is; each adjacency naming another sector, once, which names it back. **`MapTests` checks the repository map's layout**: its sectors tile the map without overlap; every asteroid and start lies in one; two sectors are adjacent exactly when they share a border; and every sector can be reached from both starts' sectors.
3. **The rings are yields of their own**, `"home"`, `"near"`, `"contested"` and `"rich"`, each with its rate in `Tuning.json`'s rules, `miningRigOrePerSecondHome` and the others (owner, 2026-10-02; the rates, 2026-10-07).
4. **The repository's map is a 10 km system** (owner, 2026-10-06): 2 km sectors on a 5 × 5 grid, named by column A to E from west to east and row 1 to 5 from south to north, with their identifiers row by row from A1's 1 to E5's 25, and the two starts in the opposite corners A1 and E5. Each home has the home asteroids the map lists; every other asteroid is placed from the match's seed by its sector's kind, mirrored through the center ([ADR-072](ADR-072-seeded-placement.md) decision 1). Two asteroid fields stand on every border between two sectors, so that each border has a passage at its middle and one at each corner. Each node is at its sector's center, the homes' at their starts. The map is point-symmetric about its center, and `Tools/MakeMap.py` writes it.
5. *(retired)*

## Consequences

- **The rules read the sectors from the map.** Territory ([ADR-056](ADR-056-territory.md)) and the seed's placement ([ADR-072](ADR-072-seeded-placement.md) decision 1) take them from `Map.json`, and a map without `"sectors"` plays without territory, which `TerritoryTests` checks.
- **`MapTests` checks the repository map's shape** beyond decision 2's layout: its size, its starts, each yield's asteroids whatever the seed, its symmetry, and that every asteroid can be reached from both starts. `RejectsBrokenSectors` checks the loader's refusals.

## What this forecloses

- Sectors that are not rectangles, and adjacency that is not a shared border, without a new decision.

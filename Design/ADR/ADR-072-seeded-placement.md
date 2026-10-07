# ADR-072 — The match's seed places each sector's asteroids by the sector's kind, mirrored through the center

Status: **accepted** · 2026-10-07

## Context

Phase 4 design §7 (gate L10) keeps the map's size, its sectors, the asteroid fields that bound them, the starts and each home's three asteroids in `Map.json`. The match's seed places every other ore asteroid, every pirate outpost (milestone 30), and later every derelict (milestone 31). The map stays point-symmetric, so both seats get the same map. Placement is deterministic, so a match still replays from its seed and command log (ADR-009). The owner decided on 2026-10-07 that each sector keeps the count and yields of asteroids it had when the 10 km map was laid out by hand (ADR-036 decision 4), at places the seed draws.

What the design leaves open: how `Map.json` says what a sector gets, where in the code placement runs, and what keeps a drawn asteroid out of the way.

## Decision

1. **A sector names its kind, and a kind names what it gets.** `Map.json`'s sectors may each have a `"kind"`, and its `"sectorKinds"` list each kind's `"name"` and its `"ore"`: for each yield, a `"count"`, a `"radiusMeters"` and a `"reserve"`. A map with kinds has a `"placement"` object too: `"borderMeters"`, `"nodeClearanceMeters"` and `"oreSpacingMeters"`. The repository map's kinds are, by distance from the homes: `home` (its listed asteroids only), `flank` (2 near), `near` (1 near), `contested` (1 contested), `between` (2 contested, the sectors B4 and D2 beside the empty corners), `center` (4 contested) and `rich` (2 rich, the corners A5 and E1). That is 3 home, 7 near, 8 contested and 2 rich asteroids a player, 459,000 Ore in all, as before. `Tools/MakeMap.py` writes it.
2. **The loader checks the kinds**: each named once; each sector's kind one of them; the sector across the center from each, the one holding its node turned half a turn, of the same kind; and an even count of each yield in a sector that is its own mirror. A map without kinds lists all its asteroids, as every map did before, and is placed as it is.
3. **`PlaceContent(map, seed)` draws them once, before the match.** `InProcessServer` places its map so as it is built, from the match's seed, and every part of the server, `Simulation`, the stress and measurement loads and `MapData()`, sees only the placed map. Placement is a pure function of the map and the seed: a `Neuron::Random` seeded with the match's seed mixed with a constant, so that its draws are not the simulation's own.
4. **Each pair of mirrored sectors is drawn once, and the other half is its mirror.** Sectors are taken in order; the one with the lower identifier of a pair draws, and each asteroid it draws is added with its mirror through the center. A sector that is its own mirror, the center, draws half its count and their mirrors.
5. **A drawn asteroid stays out of the way.** It is drawn uniformly within its sector, the placement's border and its own radius in from each edge, so that it stays off the passages the fields leave, and it is drawn again, up to 10,000 times, until it is at least the node clearance from its sector's node, where a Relay and its defenders stand; the map's minimum gap from every obstacle, the edge and every start, both it and its mirror; and the ore spacing, center to center, from every ore asteroid, so that a rig has room for a Defence Platform beside it. The repository map's numbers are 250 m, 350 m and 500 m. A sector with no room for its kind's asteroids throws, naming the sector.
6. **Asteroids are listed fixed first, then in the order drawn**, each followed by its mirror. The pathfinder's obstacles follow that order (ADR-054).
7. **The pirates' outposts are drawn by kind too** ([ADR-073](ADR-073-pirates.md)).
   - `Map.json`'s `"outposts"` name, for each size of outpost the tuning data has, a sector kind and a `count` of pairs of sectors across the center from each other. A sector that is its own mirror counts as a pair.
   - The loader checks that each kind is known and has enough pairs for all the outposts that ask for it.
   - After every asteroid, the same generator draws each rule's pairs from those of its kind that no earlier rule took. The pair's sector with the lower identifier and its mirror both get the outpost. The asteroids are drawn first, so outposts change none of them.
   - The repository map has a camp in two of the four `contested` pairs, and a stronghold on the `rich` pair and on the `center` (owner, 2026-10-07).

## Consequences

- **Measured, every seed places.** Seeds 1 to 2,000 all placed the repository map in the Linux container, at about 10 µs each.
- **The gap check holds by construction.** A drawn asteroid keeps the minimum gap the loader checks of a listed one, so the passages the loader promises stay open whatever the seed. `MapTests` checks it over seeds 1 to 12, and reachability from both starts over seeds 1 to 3.
- **Tests that named an asteroid's place now find it.** `TerritoryMatch::AsteroidIn` returns an asteroid the seed placed in a sector. The fixtures place their starting bases and loads on the server's placed map.
- **Which start a contested asteroid of B4, D2 or the center lies nearer now varies by seed**, where the map laid out by hand gave each player one of each pair; the counts by sector do not vary.
- **Which contested sectors hold a camp varies by seed** (decision 7). `MapTests` checks the outposts over seeds 1 to 12: seven a map, one a sector, each with its mirror, of the kinds their rules name, and not the same camps on every seed.
- **Derelicts** are placed by the same kinds when milestone 31 adds them, and this ADR is edited then.

## What this forecloses

- A map that is not point-symmetric where it has kinds.
- Placement during a match: everything is placed before the first tick.
- Placement that depends on anything but the map and the seed.

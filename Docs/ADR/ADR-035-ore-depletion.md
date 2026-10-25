# ADR-035 — An asteroid's reserve is drawn by what its rig earns, and each player remembers what it last saw

Status: **accepted** · 2026-10-03
Owns: ore-reserves, ore-depletion, remembered-ore-reserves

## Context

Ore runs out (owner, 2026-10-02). Every ore asteroid holds a reserve, set in the map data; once it is gone, the asteroid's rig earns a trickle, `rules.exhaustedYieldPercent` of its yield, for the rest of the match. Improved Extraction drains a reserve faster, and Deep Core Survey's `oreReserve` effect enlarges every reserve, what is left included, for the player who researched it only. A player sees what is left in an asteroid it can see, and remembers the figure from when it last saw it, as it remembers structures (ADR-024). Income is counted in hundredths of an Ore a second and paid a tick's share at a time, the remainder carried ([ADR-016](ADR-016-base-building.md) decision 11), and the simulation is deterministic (ADR-009).

## Decision

1. **The map data gives every ore asteroid its reserve**, in whole Ore, required: a listed asteroid's `"reserve"`, and an asteroid the seed places its sector kind's ([ADR-072](ADR-072-seeded-placement.md) decision 1). An asteroid placed in code without one, as tests place them, never runs out. The tuning data gives the trickle, `rules.exhaustedYieldPercent`.
2. **The server holds the reserve on the asteroid**, in hundredths. A built rig's income each second is its asteroid's rate with its owner's income factor and its sector's share ([ADR-056](ADR-056-territory.md) decision 5), and that times the trickle once the reserve is zero.
3. **A rig draws from its asteroid what it earns, divided by its owner's reserve factor.** Improved Extraction raises what it earns, so the reserve drains as much faster; Deep Core Survey's factor makes the same Ore draw less, for whoever has researched it and only while they mine, so it stretches what is left as well as what is to come. The draw is carried in hundredths times ticks a second on the rig, as a player's income is ([ADR-016](ADR-016-base-building.md) decision 11), so that every tick of a second draws its share and a second draws its whole. Income is paid before the draw each tick, so a reserve's last tick pays its rate and the next pays the trickle. The trickle draws nothing.
4. **Under fog of war each player keeps the reserve of each asteroid as it last saw it.** At the end of each tick, every asteroid in the player's sight, an observer's ([ADR-024](ADR-024-fog-of-war.md)) or a held sector's that is not suppressed ([ADR-056](ADR-056-territory.md) decision 9), updates its figure; one never seen has none. Without fog every player knows every reserve.
5. **The snapshot carries the reserve as the player knows it**, `EntityView::oreReserveHundredths`, on an ore asteroid and on a Mining Rig, for its asteroid. The selection panel of a rig gives the Ore left, or that it has run out, and the minimap draws an asteroid that has run out in a dark rust instead of Ore's gold ([ADR-043](ADR-043-hud-in-the-windows-look.md) decision 7).

## Consequences

- **The reserve is the asteroid's, not the rig's.** A rig destroyed and rebuilt, by either side, mines what is left.
- **`EconomyTests` check it**: a reserve running out to the trickle, an asteroid without one never running out, both upgrades and that Deep Core Survey is its researcher's alone, and the remembered figure under fog. `MapTests` check the reserve is required; `HudTests` the panel's line and the dark mark.

## What this forecloses

- A reserve shared out between several rigs on one asteroid: one rig stands on an asteroid ([ADR-016](ADR-016-base-building.md) decision 4).
- An asteroid that refills.

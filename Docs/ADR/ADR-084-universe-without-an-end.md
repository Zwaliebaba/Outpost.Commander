# ADR-084 — A universe has no end, and a player who loses restarts at its start an hour later

Status: **accepted** · 2026-10-08
Owns: universe-rules, seat-restart, free-start, empire-fallen-banner

## Context

A match ends. Domination ([ADR-057](ADR-057-domination.md)) ends it in under an hour when one side holds one node more, and [ADR-037](ADR-037-losing-all-production.md) decision 1 ends it when a player has neither a Command Station nor a finished Shipyard. A universe runs for weeks, and neither ending fits it: a player who loses would have nothing to do for the rest of the universe. So in a universe a player who loses restarts at its start an hour later, with the starting Ore, keeping its research and its designs, and keeping its bank too, raised to the starting Ore when it has less (owner, 2026-10-08).

## Decision

1. **A universe's rules are the simulation's** (`Simulation::UseUniverseRules`), set by the server for a new universe before the bases are placed, and saved with the universe. A match does not have them.
2. **Domination is off in a universe.** No tickets are given and none drain, and a snapshot carries none. No match ends: `MatchOver` stays false.
3. **A player who loses is told so in the tick it does.** The rule of loss is [ADR-037](ADR-037-losing-all-production.md) decision 1's. `EventKind::EmpireLost` names the start's sector and the start. The player's state keeps the tick it lost on (`PlayerState::lostTick`), and its snapshot carries the tick its seat restarts at (`Snapshot::restartTick`).
4. **The seat restarts at its start an hour of ticks later, once the start is free.** A start is free while no other player holds its sector and no other player's structure stands in it. On a map without sectors, the ground is twice as far round the start as its base reaches. While the start is not free the seat waits, and `restartTick` stays at the hour that has passed.
   - On restart the player has a Command Station on its start and the starting Constructors in front of it, placed as at the universe's start (`PlaceStartingBase`). Its Ore is raised to the starting Ore when it has less (owner, 2026-10-08).
   - Its research, designs and Ore above the starting Ore, and its ships and structures that still stand, stay its own. `EventKind::EmpireRestarted` tells it.
   - A player that finishes a Shipyard of its own in the meantime stands again, and does not restart.
   - The delay is a setting of the universe's rules, saved with them: `RESTART_SECONDS`, 3,600, in a universe, and a few seconds in the tests.
5. **Each base player's start is state** (`Simulation::m_starts`), since a universe restored from a save has no map's starts to place a base on.
6. **An AI empire plans afresh when its seat restarts** (`AiEmpire`). It makes its `AiPlayer` anew on `EmpireRestarted`, which lays its plan round the new Command Station, as one made afresh after a server restart does ([ADR-079](ADR-079-seat-controller-and-deputy.md) decision 3). A deputy is a keeper with no plan to lay, and plays on.
7. **The client's banner says it** (`Hud::DescribeOutcome`): "Empire fallen", and when the seat restarts at the player's start, or that it restarts once the start is clear of the enemy. The way back to the menu stays.
8. **The protocol.** The two events are the last kinds of `EventKind`. Alerts and the report of a time away ([ADR-080](ADR-080-events-and-scheduled-orders.md)) tell neither; the banner tells the player.

## Consequences

- **`UniverseRulesTests` check it**, on the repository's map with both bases:
  - A raid takes Blue's base. Blue is told, Red is not, no match ends and no tickets drain.
  - Blue restarts on the hour, and not a tick before, at its start with the starting Constructors. Its Ore, which it spent on a Shipyard's site, is raised to the starting Ore, and its designs are kept.
  - A bank above the starting Ore is kept: a rig in a sector Blue still holds earns while it waits.
  - Blue waits past the hour while Red's Shipyard stands in its home sector, and restarts once Blue's ships have destroyed it.
  - A universe saved while Blue waits restarts it on the same tick as the universe that never stopped.
  - A match, under the same loss, ends.
  - `HudTests` lay out the banner. `WireFormatTests` carry the restart's tick and the last kind of event. `UniverseStateTests` record the layout's hash under version 1 ([ADR-077](ADR-077-universe-state-on-disk.md) decision 6).
- **A home sector held by the other player keeps the seat waiting for as long as it is held.** The repository's map has two starts, one a seat ([ADR-036](ADR-036-map-sectors.md) decision 4), so there is no other start to restart at. The universe log records how long a seat waits ([ADR-082](ADR-082-universe-log-and-universe-run.md) decision 1).
- **Enemy ships on the start do not keep it from being free:** the base is placed among them.

## What this forecloses

- A universe that ends by domination or by a loss, without a new decision.
- A restart at another start than the seat's own, until a map has spare starts.
- A restart that takes away a player's research, designs, bank or surviving ships and structures.

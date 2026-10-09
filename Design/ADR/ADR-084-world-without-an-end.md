# ADR-084 — A world has no end, and a player who loses restarts at its start an hour later

Status: **accepted** · 2026-10-08

## Context

Phase 5 design §8 (gate H6) takes away a world's end. Domination ([ADR-057](ADR-057-domination.md)) ends a match in under an hour when one side holds one node more. [ADR-037](ADR-037-losing-all-production.md) ends it when a player has neither a Command Station nor a finished Shipyard. Neither fits a world that runs for weeks: on day 3 a player who loses would have nothing to do for the rest of it. The design has such a player restart at a free start an hour later, with starting Ore, keeping its research and designs. The owner decided on 2026-10-08 that the player keeps its bank too, raised to the starting Ore when it has less.

## Decision

1. **A world's rules are the simulation's** (`Simulation::UseWorldRules`), set by the server for a new world before the bases are placed, and saved with the world (`WORLD_STATE_VERSION` 6). A match does not have them.
2. **Domination is off in a world.** No tickets are given and none drain, and a snapshot carries none. No match ends: `MatchOver` stays false.
3. **A player who loses is told so in the tick it does.** The rule of loss is ADR-037's. `EventKind::EmpireLost` names the start's sector and the start. The player's state keeps the tick it lost on (`PlayerState::lostTick`), and its snapshot carries the tick its seat restarts at (`Snapshot::restartTick`).
4. **The seat restarts at its start an hour of ticks later, once the start is free.** A start is free while no other player holds its sector and no other player's structure stands in it. On a map without sectors, the ground is twice as far round the start as its base reaches. While the start is not free the seat waits, and `restartTick` stays at the hour that has passed.
   - On restart the player has a Command Station on its start and the starting Constructors in front of it, placed as at the world's start (`PlaceStartingBase`). Its Ore is raised to the starting Ore when it has less (owner, 2026-10-08).
   - Its research, designs and Ore above the starting Ore, and its ships and structures that still stand, are its own as before. `EventKind::EmpireRestarted` tells it.
   - A player that finishes a Shipyard of its own in the meantime stands again, and does not restart.
   - The delay is a setting of the world's rules, saved with them: `RESTART_SECONDS`, 3,600, in a world, and a few seconds in the tests.
5. **Each base player's start is state** (`Simulation::m_starts`), since a world restored from a save has no map's starts to place a base on.
6. **An AI empire plans afresh when its seat restarts** (`AiEmpire`). It makes its `AiPlayer` anew on `EmpireRestarted`, which lays its plan round the new Command Station, as one made afresh after a server restart does ([ADR-079](ADR-079-seat-controller-and-deputy.md)). A deputy is a keeper with no plan to lay, and plays on.
7. **The client's banner says it** (`Hud::DescribeOutcome`): "Empire fallen", and when the seat restarts at the player's start, or that it restarts once the start is clear of the enemy. The way back to the menu stays.
8. **The protocol.** The two events raised `PROTOCOL_VERSION` to 15, and the drain's numbers ([ADR-057](ADR-057-domination.md) decision 7) to 16. The two events are the last kinds of `EventKind`. Alerts and the report of a time away ([ADR-080](ADR-080-events-and-scheduled-orders.md)) tell neither; the banner tells the player.

## Consequences

- **Tested in the Linux container.** `WorldRulesTests`, on the repository's map with both bases:
  - A raid takes Blue's base. Blue is told, Red is not, no match ends and no tickets drain.
  - Blue restarts on the hour, and not a tick before, at its start with the starting Constructors. Its Ore, which it spent on a Shipyard's site, is raised to the starting Ore, and its designs are kept.
  - A bank above the starting Ore is kept: a rig in a sector Blue still holds earns while it waits.
  - Blue waits past the hour while Red's Shipyard stands in its home sector, and restarts once Blue's ships have destroyed it.
  - A world saved while Blue waits restarts it on the same tick as the world that never stopped.
  - A match still ends.
  - `HudTests` lay out the banner. `WireFormatTests` carry the restart's tick and the last kind of event. `WorldStateTests` record the layout of version 6.
- **Each test was run against a deliberately broken build and failed:** a restart without the hour, a start always free, the bank reset to the starting Ore, no top-up, the lost tick not saved, and domination on in a world.
- **A home sector held by the other player keeps the seat waiting for as long as it is held.** Phase 4's map has two starts, one a seat, so there is no other start to restart at (design §8). The world log records how long a seat waits (ADR-082).
- **Enemy ships on the start do not keep it from being free:** the base is placed among them, and in the world run they take it again at once. In `--world-run`'s day of world (ADR-082), seed 1, the Normal AI in both seats, Red lost its base first at hour 7 and then 10 more times, and restarted 11 times, each exactly on the hour, never waiting. Seven of those bases fell 12–14.5 s after their restart, to the warships that had taken the one before; three stood 27, 27 and 74 minutes, and the last to the end of the day. Whether a start with enemy warships on it is free is the owner's to decide (gate H6).
- **Not run in play.** The banner, and an AI empire laying its plan afresh round its new station in a running world, are the owner's week to judge, and the world run's to log.

## What this forecloses

- A world that ends by domination or by a loss, without a new decision.
- A restart at another start than the seat's own, until a map has spare starts (Phase 7).
- A restart that takes away a player's research, designs, bank or surviving ships and structures.

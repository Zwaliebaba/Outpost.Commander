# ADR-059 — Alerts, and two standing orders the server keeps: hold a sector, and patrol

Status: **accepted** · 2026-10-10
Owns: alerts, standing-orders, hold-sector-order, patrol-order

## Context

A front of many sectors loses what nobody saw. The player has to be told what happens where it is not looking, and its groups need orders the server keeps while the player looks elsewhere (owner, 2026-10-03): a group that holds a sector, coming back to it after a chase and answering any enemy in it, and a group that patrols between two points, attacking what it meets.

The rules have to say where alerts and standing orders live, how a group acts as one, what ends a standing order, and the keys that give one.

## Decision

1. **Alerts are read from the server's events** ([ADR-080](ADR-080-events-and-scheduled-orders.md) decisions 1 and 2). `Outpost::Alerts` in `GameApp` reads every snapshot's events as it arrives, and raises:
   - **Relay suppressed** and **Relay under attack**;
   - **Mining Rig lost**;
   - **Enemy ships in** a sector the player holds, or **Pirate ships in** it;
   - **Ship retreating**, when one of the player's ships starts going back to be repaired ([ADR-075](ADR-075-repair-and-retreat.md));
   - **Pirates cleared**, when a sector the pirates guarded stops being guarded;
   - an **order fired**, held back or refused, when one of the player's scheduled orders fires ([ADR-080](ADR-080-events-and-scheduled-orders.md) decision 5);
   - a **command refused**, in the player's words, with no place ([ADR-094](ADR-094-a-refused-command-is-told.md)).

   Each names its sector and keeps where it happened, but a refusal, which has neither. One kind in one sector is not raised again within 20 seconds, but an order fired is told every time. What the player built and lost besides a rig, and the sectors it gained and lost, raise no alert; they are the report of a time away's ([ADR-080](ADR-080-events-and-scheduled-orders.md) decision 7). A universe's lost and restarted empire is the banner's to tell ([ADR-084](ADR-084-universe-without-an-end.md) decision 8).
2. **The HUD lists the alerts** of the last 8 seconds under the territory panel, at most four, newest first, the newest a warning chip ([ADR-085](ADR-085-one-meaning-per-color.md) decision 1). It marks each place on the minimap with an outlined square in that color, drawn over the fog. **Space moves the camera to the newest alert** that has a place.
3. **Standing orders are the server's**, as every order is (ADR-002), so they hold while the player looks elsewhere. They are two commands:
   - **`HoldSectorCommand`** names a point. The warships hold the sector that holds it, and it is refused as `NoSector` when no sector does or the map has no territory.
   - **`PatrolCommand`** names a destination, and patrols between it and the middle of the warships when ordered.

   Constructors take no part, as in an attack, and a selection of only Constructors is refused as `NoShips`.
4. **A standing order is given to a group, which acts as one.** The ships share a group number, `Simulation::m_lastStandingGroup` counting them, and keep the order's sector, or its two ends, on themselves. Once a second, each group, in the order it was given, acts with the game's group orders: attack-moves, laid as a formation (ADR-010 decision 4, ADR-047 decision 1). A group still planning a large order's paths waits for them (ADR-032).
   - **A hold** first attack-moves to the sector's node. It then attack-moves on the nearest enemy ship its player sees in the sector, unless it is already headed within 150 m of it or a ship of it is firing. Once no enemy is left, and it stands idle more than 200 m from the node, it attack-moves back to the node. A held sector is seen whole (ADR-056 decision 9), so a group holding its own sector sees all of it.
   - **A patrol** attack-moves to its destination. Each time the whole group stands idle at one end, it attack-moves to the other.
5. **Any other order the player gives a ship ends its standing order**: a move, an attack-move, an attack or a stop. The group orders a standing order gives do not end it. The order is state, so it replays (ADR-009). A ship retreating to be repaired keeps its standing order, and its group goes on without it until it is whole ([ADR-075](ADR-075-repair-and-retreat.md)).
6. **Only the owner sees a ship's standing order** (`EntityView::standing`). The selection panel says "Holding a sector" or "On patrol".
7. **Keys:** H, then a left-click, holds the sector clicked in. T, then a left-click, patrols to the point clicked. Escape or a right-click cancels either, and A, H or T replaces the other. P is taken by the production window, so patrol is T. A selection with a warship offers each as a button too, with Attack-move and Stop, beside the selection ([ADR-088](ADR-088-selection-and-commands.md) decision 2).

## Consequences

- **Tests.**
  - `StandingOrderTests`: a group holding its sector goes to the node, answers an enemy in the far corner, and comes back; a patrol reaches its point, returns and sets out again; another order ends a standing order; and the refusals.
  - `AlertsTests`: each alert from its event, its repeat time and how long it shows.
  - `PlayerControlsTests`: the keys. `HudTests`: the list, the minimap mark and the selection's line.

## What this forecloses

- Standing orders kept by the client, without a new decision.

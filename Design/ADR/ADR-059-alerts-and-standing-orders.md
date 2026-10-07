# ADR-059 — Alerts come from the client's snapshots, and the server keeps two standing orders: hold a sector, and patrol

Status: **accepted** · 2026-10-03

## Context

Phase 2 design §9 says that without tools to command it, a front of nine sectors is losses nobody saw. Gate J5 decided on 2026-10-03 that alerts and standing orders are Phase 2's, and the strategic view is not. The design lists:

- **alerts**: a Relay suppressed or attacked, a rig lost, an enemy group entering a held sector, each a short message and a mark on the minimap, with a key that jumps the camera to the latest;
- **"hold this sector"**: a group returns to the sector's Relay after it chases, and answers any enemy in the sector;
- **"patrol"**: between two points, attacking what it meets.

It leaves open where each lives, how a group acts as one, what ends a standing order, and the keys.

## Decision

1. **Alerts are the client's.** `Outpost::Alerts` in `GameApp` reads every snapshot as it arrives. A shot and a destruction are in the snapshot of their tick only, so it must see every one. It raises:
   - **Relay suppressed**, when a sector the player holds becomes suppressed;
   - **Relay under attack**, when a shot's target is one of the player's Relays;
   - **Mining Rig lost**, when one of the player's rigs is among a tick's destructions, for which `DestroyedView` now carries the structure's kind;
   - **Enemy ships in** a sector, when the player sees an enemy warship in a sector it holds, and saw none there in the snapshot before;
   - **Ship retreating**, when one of the player's ships starts going back to be repaired, and **Pirates cleared**, when a sector the pirates guarded no longer is (Phase 4 design §13, [ADR-075](ADR-075-repair-and-retreat.md)).

   Each names its sector and keeps where it happened. One kind in one sector is not raised again within 20 seconds.
2. **The HUD lists the alerts** of the last 8 seconds under the territory panel, at most four, newest first, the newest in the warning's color. It marks each place on the minimap with an outlined square in that color, drawn over the fog. **Space moves the camera to the newest alert.**
3. **Standing orders are the server's**, as every order is (ADR-002), so they hold while the player looks elsewhere. They are two new commands:
   - **`HoldSectorCommand`** names a point. The warships hold the sector that holds it, and it is refused as `NoSector` when no sector does or the map has no territory.
   - **`PatrolCommand`** names a destination, and patrols between it and the middle of the warships when ordered.

   Constructors take no part, as in an attack, and a selection of only Constructors is refused as `NoShips`.
4. **A standing order is given to a group, which acts as one.** The ships share a group number, `Simulation::m_lastStandingGroup` counting them, and keep the order's sector, or its two ends, on themselves. Once a second, each group, in the order it was given, acts with the group orders the game already has: attack-moves, laid as a formation (ADR-010, ADR-047). A group still planning a large order's paths waits for them (ADR-032).
   - **A hold** first attack-moves to the sector's node. It then attack-moves on the nearest enemy ship its player sees in the sector, unless it is already headed within 150 m of it or a ship of it is firing. Once no enemy is left, and it stands idle more than 200 m from the node, it attack-moves back to the node. A held sector is seen whole (ADR-056), so a group holding its own sector sees all of it.
   - **A patrol** attack-moves to its destination. Each time the whole group stands idle at one end, it attack-moves to the other.
5. **Any other order the player gives a ship ends its standing order**: a move, an attack-move, an attack or a stop. The group orders a standing order gives do not end it. The order is state, so it replays (ADR-009). A ship retreating to be repaired keeps its standing order, and its group goes on without it until it is whole ([ADR-075](ADR-075-repair-and-retreat.md)).
6. **Only the owner sees a ship's standing order** (`EntityView::standing`). The selection panel says "Holding a sector" or "On patrol".
7. **Keys:** H, then a left-click, holds the sector clicked in. T, then a left-click, patrols to the point clicked. Escape or a right-click cancels either, and A, H or T replaces the other. P is taken by the production window, so patrol is T.

## Consequences

- **Tests.**
  - `StandingOrderTests`: a group holding its sector goes to the node, answers an enemy in the far corner, and comes back; a patrol reaches its point, returns and sets out again; another order ends a standing order; and the refusals.
  - `AlertsTests`: each alert, once, its repeat time and how long it shows.
  - `PlayerControlsTests`: the keys. `HudTests`: the list, the minimap mark and the selection's line.
- **Not built or run on Windows here.** The alerts, Space and the H and T keys are the owner's run.

## What this forecloses

- Alerts made by the server, and standing orders kept by the client, without a new decision.

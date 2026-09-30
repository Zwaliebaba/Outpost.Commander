# ADR-013 — The client's view: interpolated snapshots, a provisional starting fleet, and event-driven controls

Status: **accepted** · 2026-09-30

## Context

Tasks 2.5 and 2.6 put the server's world on screen and let the player command it. ADR-002 decision 5 has the client draw only snapshots and interpolate between them, and design §9 lists the controls. Three things were open.

- **Nothing to draw.** The server placed only the map, and the plan left open which task spawns ships. The owner decided on 2026-09-30 that 2.5 adds a provisional starting fleet, held in the map data.
- **Keys claimed twice.** Design §4 panned the camera with WASD, and §9 makes A attack-move and S stop. The owner decided on 2026-09-30 that the arrow keys pan and A and S are orders.
- **A snapshot does not say how to draw a ship.** It carries a design, and designs do not exist until task 3.2.

## Decision

1. **The map's `startingFleet` gives every player the same ships:** a list of hull, drive and count, in `OutpostCommander/Assets/Map.json`. Today each player has four Small and two Medium hulls on the Ion drive, starting components only (design §8). `Simulation::PlaceStartingFleets` lays them out in a square-ish grid centered on each start, facing the map's center. The spacing is the widest footprint's diameter plus 8 m. The server refuses to start when a hull or drive is unknown, or when a ship would overlap an obstacle or cross the edge. `CreateInProcessServer` calls it after `PlaceMap`, as match setup. Tests that build a server themselves start empty, as before. This fleet is provisional: task 4.2 places the Command Station and Constructors that a match really starts with.
2. **A ship's `EntityView` carries its `HullId`.** The client picks the mesh by hull, and by the owner's set, through `players` and `hulls` in `Models.json`. A ship's design stays in the snapshot, empty until 3.2.
3. **`Server::TicksPerSecond()` tells the client the tick rate.** Over a network it would come once, at connection.
4. **`SnapshotInterpolator` shows the world one tick behind the newest snapshot**, interpolated between the two snapshots around that moment. Positions are interpolated linearly, and headings the short way round.
   - **The clock.** The view's clock runs at the tick rate. Each frame it is pulled 10% of the way toward "newest tick + time since it arrived − 1 tick". It jumps if it is more than 2 ticks off.
   - **No extrapolation.** It is clamped to the newest snapshot, so when snapshots stop the view holds still.
   - **History.** Eight snapshots are kept, enough for the five that one `Advance` can bring (ADR-009).
   - **Delay.** One tick at 20 Hz is 50 ms, the low end of ADR-002's 50–100 ms.
5. **Input arrives as events as well as state.** `Window` records key presses, not auto-repeats, and mouse button presses and releases, in order. Each event has its client position, the message's millisecond time, and Shift and Ctrl. Held state alone misses a click that goes down and up between frames. Any held button captures the mouse, so a drag survives leaving a window.
6. **`PlayerControls` in `GameApp` turns events into selection, control groups and `Command`s.**
   - **Selection:** a left press that moves more than 6 pixels is a box. Otherwise it is a click on the nearest ship within 12 pixels, or within its footprint on screen if that is larger. Shift adds, or takes a selected ship out. A second click on the same ship within 400 ms selects every ship on screen of its design and hull.
   - **Orders:** right-click orders on the button's press, not its release, to save latency for Q5: attack an enemy ship under the cursor, otherwise move to the ground point. A arms attack-move for the next left click, and Escape disarms it. S stops.
   - **Control groups:** Ctrl+digit stores the selection, and the digit recalls it. A second tap within 400 ms centers the camera on the group.
   - **Pruning:** ships that are gone, or no longer the player's, drop out of the selection and the groups.
   - **Feedback:** the selection is drawn as a ring on the ground around each ship, green, or amber while attack-move waits. The drag box is drawn as its outline on the ground, because there is no 2D overlay until gate G7.
7. **The arrow keys pan the camera.** A, S, W and D do not (ADR-012).

## Consequences

- **What a player sees is at least one tick old.** Q5's measurement (task 2.7) includes that tick. An order is sent as soon as it is given, and the server applies it at its next tick.
- **The server rejects attack and attack-move until task 3.3.** The controls already send them. The protocol cannot yet tell the client that a command was rejected.
- **Double-click compares hulls until designs exist.** Every ship's design is empty until 3.2, so "same design" means "same hull" meanwhile.
- **Picking measures a footprint at the focus's scale.** A ship far toward the horizon is picked with a slightly generous circle. The 12-pixel floor dominates at the default zoom anyway.

## What this forecloses

- Extrapolating ahead of the newest snapshot.
- Edge scroll or orders on W and D.

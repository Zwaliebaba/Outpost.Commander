# ADR-013 — The client's view: interpolated snapshots, and event-driven controls

Status: **accepted** · 2026-09-30

## Context

Tasks 2.5 and 2.6 put the server's world on screen and let the player command it. ADR-002 decision 5 has the client draw only snapshots and interpolate between them, and design §9 lists the controls. Two things were open.

- **Keys claimed twice.** Design §4 panned the camera with WASD, and §9 makes A attack-move and S stop. The owner decided on 2026-09-30 that the arrow keys pan and A and S are orders.
- **A snapshot did not say how to draw a ship.** It carried a design, and designs did not exist until task 3.2.

## Decision

1. **A ship's `EntityView` carries its `HullId`.** The client picks the mesh by hull, and by the owner's set, through `players` and `hulls` in `Models.json`. The ship's design is in the snapshot too, and the player's designs with their names (ADR-014).
2. **`Server::TicksPerSecond()` tells the client the tick rate.** Over a network it would come once, at connection.
3. **`SnapshotInterpolator` shows the world one tick behind the newest snapshot**, interpolated between the two snapshots around that moment. Positions are interpolated linearly, and headings the short way round.
   - **The clock.** The view's clock runs at the tick rate. Each frame it is pulled 10% of the way toward "newest tick + time since it arrived − 1 tick". It jumps if it is more than 2 ticks off.
   - **No extrapolation.** It is clamped to the newest snapshot, so when snapshots stop the view holds still.
   - **History.** Eight snapshots are kept, enough for the five that one `Advance` can bring (ADR-009).
   - **Delay.** One tick at 20 Hz is 50 ms, the low end of ADR-002's 50–100 ms.
4. **Input arrives as events as well as state.** `Window` records key presses, not auto-repeats, and mouse button presses and releases, in order. Each event has its client position, the message's millisecond time, and Shift and Ctrl. Held state alone misses a click that goes down and up between frames. Any held button captures the mouse, so a drag survives leaving a window.
5. **`PlayerControls` in `GameApp` turns events into selection, control groups and `Command`s.**
   - **Selection:** a left press that moves more than 6 pixels is a box, which takes the player's ships only. Otherwise it is a click on the nearest ship within 12 pixels, or within its footprint on screen if that is larger. A click on none of the player's ships, but on one of its structures, selects that structure alone, for its state and its queue (task 4.5). Shift adds, or takes a selected ship out. A second click on the same ship within 400 ms selects every ship on screen of its design and hull.
   - **Orders:** right-click orders on the button's press, not its release, to save latency for Q5. Orders go to the selection's ships; a selected structure takes none.
     - Selected Constructors right-clicked on one of the player's own ships or structures that is under construction or damaged repair or build it (task 4.2), and the warships with them move there.
     - Otherwise an enemy ship under the cursor is attacked, then an enemy structure.
     - Otherwise the ships move to the ground point.
     - A arms attack-move for the next left click, and Escape disarms it. S stops.
   - **Placing a structure** (task 4.2): the HUD's build button arms a placement while the selection holds a Constructor. The next left click on the ground orders the selected Constructors to build there, and Shift keeps the placement armed for another. Right-click or Escape cancels it. A ghost of the structure follows the cursor, green where `PlaceGhost` finds it may stand and red where not, a Mining Rig snapped to its asteroid. The ghost applies ADR-016's rules to what the snapshot shows, and the server decides.
   - **The HUD's own clicks** (task 4.5): a press on a HUD button queues a job or arms a placement, a left press or drag on the minimap moves the camera there, and a right press on it sends the selected ships there. None of these reach the controls as clicks in the world.
   - **Control groups:** Ctrl+digit stores the selection, and the digit recalls it. A second tap within 400 ms centers the camera on the group.
   - **Pruning:** ships that are gone, or no longer the player's, drop out of the selection and the groups.
   - **Feedback:** the selection is drawn as a ring on the ground around each ship, green, or amber while attack-move waits. The drag box is drawn as its outline on the ground, because there is no 2D overlay until gate G7.
6. **The arrow keys pan the camera.** A, S, W and D do not (ADR-012).

## Consequences

- **What a player sees is at least one tick old.** Q5's measurement (task 2.7) includes that tick. An order is sent as soon as it is given, and the server applies it at its next tick.
- **The protocol cannot tell the client that a command was rejected.** Attack and attack-move are applied since task 3.3 (ADR-014), and a rejected order is simply not obeyed.
- **Picking measures a footprint at the focus's scale.** A ship far toward the horizon is picked with a slightly generous circle. The 12-pixel floor dominates at the default zoom anyway.

## What this forecloses

- Extrapolating ahead of the newest snapshot.
- Edge scroll or orders on W and D.

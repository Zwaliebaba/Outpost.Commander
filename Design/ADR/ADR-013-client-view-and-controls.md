# ADR-013 — The client's view: interpolated snapshots, and event-driven controls

Status: **accepted** · 2026-10-03
Owns: snapshot-interpolation, client-tick-rate, input-events, player-controls, control-groups

## Context

ADR-002 decision 5 has the client draw only snapshots, never server state, and ADR-002 decision 2 has every change to the world reach the server as a `Command`. The client therefore needs two things of its own: a view that moves smoothly between snapshots that arrive at the server's tick rate, and controls that turn the player's mouse and keys into selection and `Command`s.

- **A snapshot has to say how to draw a ship.** The mesh depends on the ship's hull and its owner's set.
- **Held state misses input.** A click that goes down and up between two frames never shows in the state a frame reads.

## Decision

1. **A ship's `EntityView` carries its `HullId`.** The client picks the mesh by hull, and by the owner's set, through `players` and `hulls` in `Models.json`. The ship's design is in the snapshot too, and the player's designs with their names (ADR-014).
2. **The client takes the tick rate from its own build's tuning data.** The shell reads it once from its first skirmish server (`Server::TicksPerSecond()`) and gives it to `GameClient`, the AI and the match log, and uses the same rate for a world it joins on a dedicated server. Nothing on the wire carries the server's rate, and the hello checks the protocol's version and layout, not the server's data ([ADR-060](ADR-060-quic-in-process.md) decision 5).
3. **`SnapshotInterpolator` shows the world one tick behind the newest snapshot**, interpolated between the two snapshots around that moment. Positions are interpolated linearly, and headings the short way round.
   - **The clock.** The view's clock runs at the tick rate. Each frame it is pulled 10% of the way toward "newest tick + time since it arrived − 1 tick". It jumps if it is more than 2 ticks off.
   - **No extrapolation.** It is clamped to the newest snapshot, so when snapshots stop the view holds still.
   - **History.** Eight snapshots are kept, the newest last. The view needs only the two around the moment it shows, and after a stall its clock jumps rather than replaying what arrived meanwhile.
   - **Delay:** one tick.
4. **Input arrives as events as well as state.** `Window` records key presses, not auto-repeats, and mouse button presses and releases, in order. Each event has its client position, the message's millisecond time, and Shift and Ctrl. Held state alone misses a click that goes down and up between frames. Any held button captures the mouse, so a drag survives leaving a window.
5. **`PlayerControls` in `GameApp` turns events into selection, control groups and `Command`s.**
   - **Selection:** a left press that moves more than 6 pixels is a box, which takes the player's ships only. Otherwise it is a click on the nearest ship within 12 pixels, or within its footprint on screen if that is larger. A click on none of the player's ships, but on one of its structures, selects that structure alone, for its state and its queue. Shift adds, or takes a selected ship out. A second click on the same ship within 400 ms selects every ship on screen of its design and hull.
   - **Orders:** right-click orders on the button's press, not its release, to save latency. Orders go to the selection's ships; a selected structure takes none.
     - Selected Constructors right-clicked on one of the player's own ships or structures that is under construction or damaged repair or build it, and the warships with them move there.
     - Otherwise an enemy ship under the cursor is attacked, then an enemy structure.
     - Otherwise the ships move to the ground point.
     - A arms attack-move for the next left click, and Escape disarms it. S stops.
   - **Placing a structure:** the HUD's build button arms a placement while the selection holds a Constructor. The next left click on the ground orders the selected Constructors to build there, and Shift keeps the placement armed for another. Right-click or Escape cancels it. A ghost of the structure follows the cursor, green where `PlaceGhost` finds it may stand and red where not, a Mining Rig snapped to its asteroid. The ghost applies ADR-016's rules to what the snapshot shows, and the server decides. The client also keeps a Mining Rig off an asteroid the player has never seen, which the server allows ([ADR-046](ADR-046-second-look-at-the-screen.md) decision 8).
   - **The HUD's own clicks:** a press on a HUD button queues a job or arms a placement, a left press or drag on the minimap moves the camera there, and a right press on it sends the selected ships there. None of these reach the controls as clicks in the world.
   - **Control groups:** Ctrl+digit stores the selection, and the digit recalls it. A second tap within 400 ms centers the camera on the group.
   - **Pruning:** ships that are gone, or no longer the player's, drop out of the selection and the groups.
   - **Feedback:** the selection is drawn as a ring on the ground around each ship, green, or amber while attack-move or a standing order waits for its click ([ADR-059](ADR-059-alerts-and-standing-orders.md) decision 7). The drag box is drawn as its outline on the ground.
6. *(retired)*

## Consequences

- **What a player sees is at least one tick old.** An order is sent as soon as it is given, and the server applies it at its next tick.
- **The protocol cannot tell the client that a command was rejected.** A rejected order is simply not obeyed.
- **Picking measures a footprint at the focus's scale.** A ship far toward the horizon is picked with a slightly generous circle.

## What this forecloses

- Extrapolating ahead of the newest snapshot.

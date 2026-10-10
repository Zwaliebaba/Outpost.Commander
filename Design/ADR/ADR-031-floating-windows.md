# ADR-031 — Windows float over the HUD in layers, their state in a manager and their geometry in the layout

Status: **accepted** · 2026-10-02

## Context

The owner wants the game's screens to float as windows rather than sit in one place (Phase 1 design §12): the designer, the research and a structure's production queue float, and the Ore, the minimap, the selection panel and the buttons stay anchored. A window is dragged by its title bar and kept on the screen, comes to the front when clicked, closes with its × or with Esc, and keeps its place until the game is closed; the match runs on while one is open (owner, 2026-10-02). The HUD is laid out afresh every frame by `Hud::Lay`, which keeps no state and draws nothing (ADR-015): it lists panels, texts and buttons in pixels, and `GameClient` draws them in two passes, every panel and then every text. Input focus is a rectangle test on that list. Task 9.2 builds the windows; tasks 9.3 and 9.4 put the designer, the research and the production queue in them.

## Decision

1. **`Outpost::WindowManager` holds the state, and nothing else.** Which windows are open and in what order front to back, where each was left, which is being dragged and where it was taken hold of, all in the HUD's reference units (ADR-006), and which of the two places under the Ore an unmoved production or research window took as it opened (task 15.4). It knows no geometry, so it is tested on its own. `GameClient` owns one for as long as the game runs; leaving a match closes every window but keeps their places, and nothing is written to disk.
2. **`Hud::Lay` lays each open window out, back to front, after everything else.** It takes the manager, and places each window where the manager left it, or at the window's default place: the designer's top-right corner, and for production and research the first of the two places under the Ore that no open window held when it opened, moved only as far as `Hud::KeepOnScreen` needs to keep its whole title bar, and 120 units of its width, on the screen. The clamp is applied when the window is laid out, so a smaller screen moves it back too, and `GameClient` settles the manager on the clamped place, so that a window dragged past an edge moves again as soon as the pointer comes back. Without a manager, `Lay` shows every window the content has at its default place, which is what the HUD's tests rely on.
3. **A window is a frame, a hatched title bar, a close box and four corner brackets** (ADR-030), opened by `OpenWindow`, which records where in the layout's lists of panels, texts, sprites and buttons the window starts.
4. **The layout is drawn and clicked in layers.** Layer 0 is the HUD and layer i + 1 is the i-th window back to front; each layer's panels, then its sprites, then its texts, so that a window covers the text of the window behind it. The bars over the entities come before every layer ([ADR-088](ADR-088-selection-and-commands.md) decision 5). `Layout::LayerAt` finds the front layer under a point, and `ActionAt` looks for a button only in that layer, so a HUD button under a window takes no click. A press on any window is the HUD's, not the world's, as before (ADR-015).
5. **Input.** A press on a window brings it to the front; on its close box it closes it, and on its title bar it starts a drag that follows the pointer until the button is released. Esc closes the front window and goes no further; while the designer's name takes typing, the designer has Esc first, as before (ADR-017).
6. **A window opens from a button on its structure's selection panel, and from its key**: D for the designer, P for production and R for research, each of which also closes its window (Phase 1 design §12). F1 opens and closes the Controls window, which no structure has ([ADR-068](ADR-068-interface-polish.md)). O opens and closes the orders window, and the away window opens itself when the report of a time away arrives; neither belongs to a structure ([ADR-080](ADR-080-events-and-scheduled-orders.md)).

## Consequences

- **Esc closes a window before it cancels a placement or anything else the controls do with it.** With a window open, a second Esc reaches the controls.
- **The title bar is hatched by the pixel shader**, so a window costs a handful of quads beyond its content.
- **`GameAppTests` checks it without a GPU:** the manager's order, drag and settling (`WindowManagerTests`), and the designer's window, its layers, a HUD button under it, and its clamping on two screens (`HudTests`). Dragging and closing by hand are the owner's run.

## What this forecloses

- Docking, resizing and minimizing windows: a window has the size its content gives it.
- Writing window positions to disk, by the owner's decision of 2026-10-02.

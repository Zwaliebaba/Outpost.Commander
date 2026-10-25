# ADR-012 — The RTS camera: width-driven zoom, pitch from zoom, and the cursor held in full screen

Status: **accepted** · 2026-10-06
Owns: rts-camera, camera-zoom-and-pitch, camera-start, cursor-clip, edge-scroll, input-state

## Context

The camera pans by edge scroll, the arrow keys and middle-drag, zooms with the wheel within limits, and turns around the focus with Q and E (Game §interface.camera). The arrow keys pan because A and S are orders ([ADR-013](ADR-013-client-view-and-controls.md)). The pitch is fixed by the zoom. Edge scroll needs the screen's edges, which a window, or an edge shared with another monitor, does not give ([ADR-006](ADR-006-renderer-shape.md)). The camera is client state ([ADR-002](ADR-002-authoritative-server.md)).

## Decision

1. **The zoom is a view width: how much ground lies across the middle of the screen at the focus point.** The camera's distance follows from it and the horizontal field of view, `distance = width / (2 tan(hfov / 2))`. So a view width covers the same ground on any screen shape, the default `defaultViewWidthMeters` included, and the test measures exactly that, by casting rays through the screen's left and right edges to the ground.
2. **The pitch follows the view width in a straight line**, from `pitchAtMinimumDegrees` fully zoomed in to `pitchAtMaximumDegrees` fully zoomed out. Zooming out looks down more steeply.
3. **The numbers are data, in `OutpostCommander/Assets/Camera.json`,** which the [Catalogue](../../GameDesign/Reference/Catalogue.md) shows: the default view width and its limits, the pitch at either limit, the vertical field of view, the focus limit, the pan speed in view widths a second, the edge-scroll margin, the zoom per wheel notch and the turn rate. The view width's limits are final (owner, 2026-10-01), with the widest raised for the 10 km map (owner, 2026-10-06); the focus limit is the map's edge ([ADR-036](ADR-036-map-sectors.md) decision 4). Panning in view widths makes it feel the same at every zoom. The loader rejects a default outside the limits, a pitch past vertical and a zoom factor that does not zoom.
4. **A match opens on the player's own fleet, looking along +z.** The first snapshot puts the focus on the middle of the player's ships, wherever the map put its start, and the camera looks along +z, so the screen's right is +x and its top is +z, and a ship with heading 0 faces right.
5. **The cursor is held inside the window while the game is full screen and in the foreground.** `Window::ReadInput` sets `ClipCursor` to the client area every frame, because Windows can drop the clip, and releases it when the game loses the foreground, goes windowed or closes. This makes edge scroll work beside another monitor.
6. **Edge scroll works only while the cursor is held.** In a window the window's edge is not the screen's, so the cursor would leave before it scrolled. There, the arrow keys and middle-drag pan. Middle-drag captures the mouse, so a drag keeps going when the cursor leaves a window.
7. **Input is read once per frame into a `Neuron::InputState`:** foreground, cursor held, cursor position, wheel notches since the last read, and the keyboard state including the mouse buttons, with the frame's events ([ADR-013](ADR-013-client-view-and-controls.md) decision 4). The window knows no game concept. The camera reads its keys from `GameApp/KeyBindings.h` ([ADR-068](ADR-068-interface-polish.md) decision 4): the arrows pan, and Q and E turn it, Q counterclockwise seen from above. While the game is in the background, no key reads as down and the camera ignores input.
8. **The arrow keys pan the camera; A, S, W and D do not,** since A and S are orders (owner, 2026-09-30).
9. **The camera's far plane reaches `FAR_PLANE_BEYOND_FOCUS_METERS`, 8,000 m, beyond its focus,** so that from any focus inside `focusLimitMeters`, the map's edge, it reaches past the map's far corner.

## Consequences

- **Alt+Tab releases the cursor.** Coming back holds it again on the next frame.
- **The camera has no inertia or smoothing.** It moves exactly with input. If play shows that it needs smoothing, it is added here.
- **A camera number changes in `Camera.json` alone.**

## What this forecloses

- Edge scroll in a window.
- A free camera that pitches independently of the zoom.

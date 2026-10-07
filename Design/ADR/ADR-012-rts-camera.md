# ADR-012 — The RTS camera: width-driven zoom, pitch from zoom, and the cursor held in full screen

Status: **accepted** · 2026-09-30

## Context

Design §4 gives the camera. It pans by edge scroll, the arrow keys and middle-drag, zooms with the wheel within limits, and turns around the focus with Q and E. The arrow keys replaced WASD on 2026-09-30, because §9 gives A and S to attack-move and stop (ADR-013). The pitch is fixed by the zoom, and the default view is about 500 m wide. Gate G3, the zoom limits, was decided on 2026-10-01: the limits task 1.5 first held as data are final. ADR-006 left task 1.5 to decide two things. The first is whether the cursor is held inside the window while the game is active. The second is what edge scroll does in a window. The camera is client state (ADR-002).

## Decision

1. **The zoom is a view width: how much ground lies across the middle of the screen at the focus point.** The camera's distance follows from it and the horizontal field of view, `distance = width / (2 tan(hfov / 2))`. So the default 500 m is 500 m on any screen shape, and the test measures exactly that, by casting rays through the screen's left and right edges to the ground.
2. **The pitch follows the view width in a straight line**, from `pitchAtMinimumDegrees` fully zoomed in to `pitchAtMaximumDegrees` fully zoomed out. Zooming out looks down more steeply.
3. **The numbers are data, in `OutpostCommander/Assets/Camera.json`.** The view width's limits are gate G3's, final since 2026-10-01, kept on the 5 km map (gate H8), with the widest raised for the 10 km map (Phase 4 gate L6):

   | Setting | Value |
   |---|---|
   | View width | 500 m by default, from 150 m to 3,000 m |
   | Pitch | 40° to 70° |
   | Vertical field of view | 45° |
   | Focus limit | ±5,000 m, the map's edge ([ADR-036](ADR-036-map-sectors.md)) |
   | Pan speed | 0.8 view widths per second |
   | Edge-scroll margin | 4 pixels |
   | Zoom per wheel notch | a factor of 1.15 |
   | Turn rate | 90° per second |

   Panning in view widths makes it feel the same at every zoom. The loader rejects a default outside the limits, a pitch past vertical and a zoom factor that does not zoom.
4. **At the start the camera looks along +z**, so the screen's right is +x and its top is +z, and a ship with heading 0 faces right.
5. **The cursor is held inside the window while the game is full screen and in the foreground.** `Window::ReadInput` sets `ClipCursor` to the client area every frame, because Windows can drop the clip, and releases it when the game loses the foreground, goes windowed or closes. This makes edge scroll work beside another monitor.
6. **Edge scroll works only while the cursor is held.** In a window the window's edge is not the screen's, so the cursor would leave before it scrolled. There, the arrow keys and middle-drag pan. Middle-drag captures the mouse, so a drag keeps going when the cursor leaves a window.
7. **Input is read once per frame into a `Neuron::InputState`:** foreground, cursor held, cursor position, wheel notches since the last read, and the keyboard state including the mouse buttons. The window knows no game concept. Which key does what is the camera's. While the game is in the background, no key reads as down and the camera ignores input.

## Consequences

- **Alt+Tab releases the cursor.** Coming back holds it again on the next frame.
- **The camera has no inertia or smoothing.** It moves exactly with input. If play shows that it needs smoothing, it is added here.
- **A camera number changes in `Camera.json` alone.** G3 set the zoom limits without a change to the code.

## What this forecloses

- Edge scroll in a window.
- A free camera that pitches independently of the zoom.

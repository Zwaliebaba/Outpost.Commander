# Prompt for Claude Code

Paste this into Claude Code from the repository root, with the handoff folder available.

---

Implement the in-match HUD for Outpost Commander from the design handoff in
`design_handoff_hud/`. Read `design_handoff_hud/README.md` in full before writing code — it is
self-sufficient and every number in it is final.

**Scope.** The interface pass only. Do **not** implement the 3D scene: no camera, star field, galaxy
band, plane grid, ship/station/asteroid meshes. The design reference HTML draws a representative scene
behind the HUD purely so contrast could be judged; none of it is a specification. If a world position is
needed (hull bars, order marker, placement radius), assume a `project(worldPos) -> screenPos` function
exists or stub it behind an interface — the overlays' *geometry* is in scope, their *projection* is not.

**The design reference is HTML and is not code to port.** `design_handoff_hud/Outpost Commander HUD.dc.html`
opens in a browser and shows four 1440 × 960 frames plus state studies. Use it to see the design. The
`div` elements and `clip-path` in it are only a way to draw the target primitives in a browser.

**Target environment.** C++ / Direct3D 12 in `GameClient`, no UI framework — no XAML, no Direct2D, no
image assets, no texture atlas other than the DirectWrite Segoe UI glyph atlas. Follow this repository's
existing conventions (`AGENTS.md`, the `Design/ADR/` decisions, R13/R18/R19/R20/R21) over anything you
would otherwise reach for. Everything the HUD draws must be one of: an axis-aligned rect with per-vertex
colour and alpha, a line segment of a given width, a flat-filled triangle, or a glyph quad.

**Before you write UI code, do these in order:**

1. Read `Design/Interface.md` and `Design/ADR/ADR-007`, `-009`, `-011`, `-015`, `-017`, `-020`, then
   tell me anywhere the handoff and those documents disagree. Do not silently reconcile them.
2. Propose the module layout: where the layout constants live, where the hit table lives, where the
   draw list is built, and how the handedness boolean reaches them. Wait for my sign-off.
3. Write the tier and clearance test first. It loads `design_handoff_hud/geometry.json` and asserts, for
   both handedness states, that every interactive rect meets its tier (48 / 64 / 96) and every adjacent
   pair of interactive rects has at least 16 authored pixels of clear space. This is the acceptance
   criterion that is judged by a test rather than by eye, and it should fail before the panels exist.

**Then implement in this order**, one reviewable change per step: credits → system → selection → build →
damage alert → overlays and quit confirm → motion. Motion last; everything should be correct static first.

**Hard rules, all of which the README explains:**

- All coordinates are integers in a 1440 × 960 authored frame. A half-pixel value is a defect.
- The handedness flip is exactly `x' = 1440 - x - w` applied to the selection and build panels only,
  from a single boolean. Do not hand-write mirrored coordinates — `geometry.json` has them.
- No scrolling, no lists, no paging anywhere. Every panel fits its worst case in fixed space.
- No text wrapping, ellipsis, rich text or scrolling text. Text rects are fixed and clip.
- No rounded corners, drop shadows, glows, blur, radial gradients, textures or icons.
- The only diagonals in the entire interface pass are the alert direction triangle and the 45° hatch on
  an unavailable build button. If you find yourself adding a third, stop and ask.
- Do not invent a readout. The list of available data in the README is exhaustive; if something you want
  is not on it, say so and leave it out.
- Do not add a gesture, a tooltip, a hover state or a sixth surface.

**Flag rather than fix:** the build panel has one queue slot, credits are spent at start, and all six
buttons stay live while something is building — so a tap replaces the in-progress item and those credits
are lost. That is a `GameCore` refund decision, not an interface one. Implement the interface as
specified and raise it as an issue.

When each step is done, tell me what you changed, what the test asserts now, and anything in the design
you could not build with the available primitives.

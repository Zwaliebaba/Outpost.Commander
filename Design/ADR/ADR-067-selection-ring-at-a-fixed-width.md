# ADR-067 — The selection's ring is a fixed width on the screen

Status: **accepted** · 2026-10-05
Owns: selection-ring

## Context

[ADR-042](ADR-042-faint-footprint-lines.md) draws a structure's ring as a line a pixel wide, and the scene around it is lines a pixel wide. A selection has to stay the strongest mark on the ground in that scene. A band that is a share of the ring's radius grows with the structure and as the camera comes in, until it is a wide stripe of saturated green under a large structure. So the selection's ring is a fixed width on the screen at any zoom (owner, 2026-10-03).

## Decision

1. **The selection's ring is 3 of the HUD's reference units wide at any zoom.** That is 3 pixels at 1080p, 2 at 720p and 6 at 2160p: the width times `Hud::Scale` without the player's factor ([ADR-070](ADR-070-interface-scale.md) decision 7), rounded, and at least 1 pixel. It is green, and amber while an order waits for its click: attack-move, or a standing order, holding a sector or patrolling ([ADR-059](ADR-059-alerts-and-standing-orders.md)).
2. **It is drawn as one-pixel lines a pixel apart**, as many as it is pixels wide, centered on the ring's radius, which is 1.3 times the footprint. Each line is the footprint ring's mesh, 96 segments, drawn with the models' lines (`MeshPipeline::DrawLines`) in one instanced call.
3. **A pixel's length on the ground is measured at the selected entity's center.** `Camera::MetersPerPixelAt` gives it from the point's depth along the line of sight: the viewport's height spans twice that depth times the tangent of half the vertical field of view.
4. **The placement ghost keeps its band**, and so do the effects that use the ring mesh. A rig's selection ring is its asteroid's ([ADR-042](ADR-042-faint-footprint-lines.md) decision 4).

## Consequences

- **A selected structure's ring takes the place of its footprint line** ([ADR-042](ADR-042-faint-footprint-lines.md) decision 3).
- **The width is exact across the line of sight and narrower along it.** The ring lies on the ground, which the camera looks down on at 40 to 70 degrees, so the ring's near and far sides are foreshortened by about the sine of that angle: 0.64 of the width at 40 degrees and 0.94 at 70. This is computed from the camera's pitch, not measured on a screen.
- **The lines are tested against the depth of what is drawn, and write none**, as the models' lines are. A ring under a rock or a hull is hidden by it.
- **The cost is one instance per line.** A selection of 100 ships at 1080p is 300 instances, within the 16,384 a frame takes (`MeshPipeline::MAX_FRAME_INSTANCES`).
- **The test.** `CameraTests.SpansAPixelAtAnyDepth` lays a length of ground across the line of sight, three of `MetersPerPixelAt`'s pixels long, at three depths and three zooms, and finds it 3 pixels wide on the screen each time.

## What this forecloses

- A selection ring that grows with the zoom, without a new decision.

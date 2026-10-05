# ADR-067 — The selection's ring is a fixed width on the screen

Status: **accepted** · 2026-10-05

## Context

[ADR-042](ADR-042-faint-footprint-lines.md) made a structure's ring a line a pixel wide, and kept the selection's ring as a band from 1.0 to 1.15 of its radius, so that a selection stays the strongest mark on the ground. The review of 2026-10-03 measured that band under a Shipyard at about 15 pixels of saturated green, in a scene of lines a pixel wide. The band is a share of the ring's radius, so it grows with the structure and as the camera comes in.

Task 15.5 of the interface plan (`GameDesign/ImplementationPlan-Interface.md`) proposed a ring about 3 pixels wide at 1080p at any zoom. The owner decided gate K3 on 2026-10-03 as proposed. The plan left how it is drawn to this ADR.

## Decision

1. **The selection's ring is 3 of the HUD's reference units wide at any zoom.** That is 3 pixels at 1080p, 2 at 720p and 6 at 2160p: the width times `Hud::Scale`, rounded, and at least 1 pixel. It is the green ring, and attack-move's amber one.
2. **It is drawn as one-pixel lines a pixel apart**, as many as it is pixels wide, centered on the ring's radius, which stays 1.3 times the footprint. Each line is the footprint ring's mesh, 96 segments, drawn with the models' lines (`MeshPipeline::DrawLines`) in one instanced call.
3. **A pixel's length on the ground is measured at the selected entity's center.** `Camera::MetersPerPixelAt` gives it from the point's depth along the line of sight: the viewport's height spans twice that depth times the tangent of half the vertical field of view.
4. **The placement ghost keeps its band**, and so do the effects that use the ring mesh. A rig's selection ring is still its asteroid's ([ADR-042](ADR-042-faint-footprint-lines.md) decision 4).

## Consequences

- **[ADR-042](ADR-042-faint-footprint-lines.md) decision 3 is rewritten** to point here. A selected structure's ring still takes the place of its footprint line.
- **The width is exact across the line of sight and narrower along it.** The ring lies on the ground, which the camera looks down on at 40 to 70 degrees, so the ring's near and far sides are foreshortened by about the sine of that angle: 0.64 of the width at 40 degrees and 0.94 at 70. This is computed from the camera's pitch, not measured on a screen.
- **The lines are tested against the depth of what is drawn, and write none**, as the models' lines are. A ring under a rock or a hull is hidden by it, as the band was.
- **The cost is one instance per line.** A selection of 100 ships at 1080p is 300 instances, within the 16,384 a frame takes (`MeshPipeline::MAX_FRAME_INSTANCES`).
- **The test.** `CameraTests.SpansAPixelAtAnyDepth` lays a length of ground across the line of sight, three of `MetersPerPixelAt`'s pixels long, at three depths and three zooms, and finds it 3 pixels wide on the screen each time.

## What this forecloses

- A selection ring that grows with the zoom, without a new decision.

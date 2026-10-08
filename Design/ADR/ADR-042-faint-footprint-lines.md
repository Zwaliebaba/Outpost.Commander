# ADR-042 — A structure's ring is a faint line a pixel wide, and a Mining Rig's ring is its own, laid over its rock

Status: **accepted** · 2026-10-03

## Context

On 2026-10-03 the owner shared two screenshots of a match: one of a base, and one zoomed in on the Command Station. The owner disliked the ring under every structure and asked whether it could be drawn with a lower alpha. The owner also found the Mining Rig wrong.

Each ring was drawn as a flat band, opaque, in its side's color at 0.35.

- **The band is wide.** It runs from 1.0 to 1.15 of the ring's radius, which is 1.3 times the footprint. So the band is about a fifth of the footprint's radius: 8.8 m under a 45 m Command Station.
- **Up close it covers the screen.** Zoomed in, that band is a slab wider than the station's own details. With a ring under every structure, the rings were the loudest thing in the base.
- **A Mining Rig's ring was its asteroid's.** A rig's entity stands at its asteroid's center with the rock's radius (design §6), so on a 45 m rock its ring ran 117–135 m across. That is wider than a Shipyard's, and it surrounded a rig drawn 50 m long.

**Alpha is not what makes the band loud.** The mesh pipeline draws opaque, and nearly everything behind a ring is the black of space. A color at alpha 0.35 over black is the same pixel as that color at 0.35, so a blended ring would look the same as today's except where it crosses the grid or a rock. Blending would also need a second pipeline state with an order of its own. The band's width is the problem, and so is a ring under every structure at the same strength.

The owner decided on 2026-10-03 to make the rings thin and faint, at full strength only under the pointer, and to give the rig a ring of its own.

## Decision

1. **A structure's ring is a line one pixel wide at any zoom.**
   - It is 1.3 times the footprint radius, the size of the selection ring, in its side's color at 0.35 at most. How it fades as the camera comes in is [ADR-046](ADR-046-second-look-at-the-screen.md)'s.
   - It has 96 segments, and the models' lines draw it (`MeshPipeline::DrawLines`), unpulled since nothing lies under it.
   - A structure the player only remembers stands on none, since it may not be there ([ADR-080](ADR-080-territory-and-memory-over-the-fog.md)).
2. **Some rings are at full strength**, in the color of a structure's own lines (`EdgeColor`, ADR-040):
   - Under the structure the pointer is on. It is picked as a click picks it (`PickEntity`), and not through the HUD.
   - Under every structure while one is being placed, since then the player is choosing where it stands among them.
3. **The selection takes the place of the line.** A selected structure's green ring takes the place of its line, and stays the strongest mark on the ground. It is a fixed width on the screen, which [ADR-067](ADR-067-selection-ring-at-a-fixed-width.md) owns. The placement ghost keeps its band.
4. **A Mining Rig's ring is its own footprint's, laid over its rock.**
   - Its radius is 1.3 times the rig's 25 m footprint, 32.5 m.
   - Each of its 96 points stands 0.3 m above the rock's surface under it (`Neuron::SurfaceHeightAt`), or above the ground past the rock's edge.
   - It is pulled toward the eye as a model's lines are, so that it shows over the faces it lies on.
   - It is made on the CPU as the rig is drawn and drawn with `MeshPipeline::DrawLineList`. That new call draws a line list from the slot `DrawTriangles` uses for an explosion's shards.
   - It shows only under the pointer and while a structure is placed ([ADR-046](ADR-046-second-look-at-the-screen.md)).
   - A rig's selection ring is still its asteroid's, since a click anywhere on the rock selects the rig.

## Consequences

- **A flat ring could not have been used for the rig.** Measured in the Linux container with the shipped meshes:
  - On a 45 m rock, the surface 32.5 m out runs from 3 m below the ground to 25 m above it, and 6 of the 96 points are past its edge.
  - On a 60 m rock it runs from 19 m to 37 m.
  - A ring at any one height would be buried on one side or float over the other.
- **A rig can lose its ring for a frame.** The shards and the rigs' rings share the slot's 65,536 vertices a frame, and the shards are drawn first. A frame whose explosions have taken every vertex the slot holds draws its rigs without rings.
- **The cost is not measured.** Each rig's ring, while it shows, takes 96 surface lookups a frame on a rock of 32 to 78 triangles ([ADR-027](ADR-027-rock-crease-lines.md)), and each frame picks the structure under the pointer.
- **Where the rig stands is decided in [ADR-044](ADR-044-rig-stands-on-its-legs.md)**: on its legs, tilted to fit its rock. The ring lies over the rock wherever the rig stands.

## What this forecloses

- A blended overlay pipeline, without a new decision.
- A ring under every structure at the strength of the selection.

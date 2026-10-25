# ADR-042 — A structure's ring is a faint line a pixel wide, and a Mining Rig's ring is its own, laid over its rock

Status: **accepted** · 2026-10-05
Owns: structure-footprint-ring, rig-footprint-ring, footprint-ring-strength

## Context

A structure stands on a ring, which puts it on the ground and says its size: the grid is faint, and a structure with no mark under it reads as floating, its size hard to judge. How that ring is drawn is bound by these:

- **A band measured in meters grows with the zoom.** Up close it is a slab wider than a structure's own details, and with one under every structure at the same strength, the rings are the loudest thing in a base.
- **Alpha does not make a ring quieter.** The mesh pipeline draws opaque, and nearly everything behind a ring is the black of space. A color at alpha 0.35 over black is the same pixel as that color at 0.35, so blending would change nothing but where a ring crosses the grid or a rock, and it would need a second pipeline state with an order of its own.
- **A rig's entity has its rock's radius** (owner, 2026-10-03), since a rig is built on, and snaps to, an ore asteroid. A ring at that radius is wider than a Shipyard's round a rig much smaller than its rock, and a rock is not flat.

The owner decided that the rings are thin and faint, at full strength only under the pointer, and that the rig has a ring of its own (owner, 2026-10-03).

## Decision

1. **A structure's ring is a line one pixel wide at any zoom.**
   - It is 1.3 times the footprint radius, the size of the selection ring, in its side's color at 0.35 at most.
   - It has 96 segments, and the models' lines draw it (`MeshPipeline::DrawLines`), unpulled since nothing lies under it.
   - **A resting ring is for the far view** (owner, 2026-10-03). It is at full strength while its radius is at most 3% of the view's width, and fades to nothing by 6.5% of it, so that up close the bases are not ringed.
2. **Some rings are at full strength**, in the color of a structure's own lines (`EdgeColor`, [ADR-040](ADR-040-lines-over-dark-faces.md) decision 1), at any zoom:
   - Under the structure the pointer is on. It is picked as a click picks it (`PickEntity`), and not through the HUD.
   - Under every structure while one is being placed, since then the player is choosing where it stands among them.
3. **The selection takes the place of the line.** A selected structure's ring takes the place of its line, and stays the strongest mark on the ground; the selection's ring is [ADR-067](ADR-067-selection-ring-at-a-fixed-width.md)'s.
4. **A Mining Rig's ring is its own footprint's, laid over its rock.**
   - Its radius is 1.3 times the rig's footprint radius.
   - Each of its 96 points stands 0.3 m above the rock's surface under it (`Neuron::SurfaceHeightAt`), or above the ground past the rock's edge.
   - It is pulled toward the eye as a model's lines are, so that it shows over the faces it lies on.
   - It is made on the CPU as the rig is drawn and drawn with `MeshPipeline::DrawLineList`, from the slot `DrawTriangles` uses for an explosion's shards ([ADR-026](ADR-026-explosions-and-particles.md) decision 5).
   - It shows only under the pointer and while a structure is placed, at full strength; at rest the rig's own color says whose it is (owner, 2026-10-03).
   - A rig's selection ring is its asteroid's, since a click anywhere on the rock selects the rig.

## Consequences

- **A flat ring cannot be used for the rig.** Measured in the Linux container on 2026-10-03, with the shipped rock meshes:
  - On a 45 m rock, the surface 32.5 m out runs from 3 m below the ground to 25 m above it, and 6 of the 96 points are past its edge.
  - On a 60 m rock it runs from 19 m to 37 m.
  - A ring at any one height would be buried on one side or float over the other.
- **A rig can lose its ring for a frame.** The shards and the rigs' rings share the slot's 65,536 vertices a frame, and the shards are drawn first. A frame whose explosions have taken every vertex the slot holds draws its rigs without rings.
- **The ring strengths are set from the view's width, not measured on screen.** Decision 1's shares come from the structures' footprints and the camera's limits in `Camera.json`.
- **Where the rig stands is decided in [ADR-044](ADR-044-rig-stands-on-its-legs.md)**: on its legs, tilted to fit its rock. The ring lies over the rock wherever the rig stands.

## What this forecloses

- A blended overlay pipeline, without a new decision.
- A ring under every structure at the strength of the selection.
- A resting ring at full strength at close zoom, and a resting ring under a rig, without a new decision.

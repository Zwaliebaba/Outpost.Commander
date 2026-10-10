# ADR-087 — Each asteroid field is laid out from its identifier, its ring closed to the smallest hull

Status: **accepted** · 2026-10-10

## Context

The second interface review of 2026-10-08 ([interface plan 2](../../GameDesign/ImplementationPlan-Interface2.md), finding 14) found every asteroid field drawn as the same rosette: a rock in the middle, 0.45 of the field's radius in size, and six around it, each 0.3 of the radius in size and 0.62 of it from the center, turned by a fixed angle. The server blocks the whole circle (MVP design §4), but the gaps between the ring's rocks looked wide enough to pass.

They were. The three rock meshes are longer than they are wide: measured from their `.nmf` files, fitted as the client fits them to 2 m long, each reaches from its center on the ground between 0.60 and 1.0 of its radius depending on the direction, and the narrowest reaches 0.60 (Small), 0.71 (Medium) and 0.62 (Large). At those reaches, two neighbors of the old ring could leave about 46 m between them on a 180 m field, nearly three Small hulls' width.

The owner decided gate V8 as proposed on 2026-10-08. This ADR is task UI5.2's. Task UI5.1's darker rock faces are [ADR-040](ADR-040-lines-over-dark-faces.md) decision 1's.

## Decision

1. **A field's rocks are laid out from its identifier** (`FieldLayout::Of`), with `EffectRandom` seeded by the identifier mixed with a constant of the layout's own ([ADR-026](ADR-026-explosions-and-particles.md) decision 7), so that one field looks the same in every frame and on every machine, and no two look alike.
   - **A rock in the middle,** 0.36 to 0.46 of the field's radius, up to 0.06 of it off the center along each axis.
   - **A ring of five to eight around it,** evenly spaced round the field from a turn of its own, each up to a fifth of a step off its place, at 0.56 to 0.68 of the radius from the center and 0.20 to 0.32 of it in size, never past the field's edge.
   - **Every rock is turned** by an angle of its own.
2. **Smaller rocks close the ring.** Between two neighbors on the ring, the gap is the distance between their centers less each one's least reach on the ground: its radius times the narrowest reach of the three meshes, which `GameClient` measures from them as it loads them (`FieldLayout::NarrowestReach`, 0.60 today). Where that gap is wider than the smallest hull's footprint, a Small hull's 16 m across from the snapshot's hulls, rocks of 0.08 to 0.12 of the field's radius stand evenly along the line between the two, as few as leave no gap wider. With no hulls known, nothing is filled.
3. **It is the client's alone.** The server blocks the whole circle as before, and nothing on the wire changes. The rocks are instanced as every model is ([ADR-053](ADR-053-instanced-meshes.md)), and each field is laid out again as it is drawn.

## Consequences

- **The gaps the eye sees are narrower than the bound,** since the bound takes every rock at its narrowest. A ship's footprint fits through no gap in the ring.
- **Every field of the 10 km map needs fillers,** and the map draws more rocks: 14 to 18 a field, 15.7 on average, 1,254 over its 80 fields against the 560 of seven a field. Measured in the Linux container from the map's radii with the identifiers 1 to 80, at a reach of 0.60 and a gap of 16 m. They add instances, not draw calls, and the GPU time they cost is not measured; the owner's `--measure` run is where it is.
- **The ring is a polygon of rocks,** the fillers standing on the straight line between two of the ring's, inside the circle the ring rocks lie on.
- **Tests:** `FieldLayoutTests` checks one field laid out the same twice; two fields laid out apart, and rings of five, six, seven and eight among 200; every rock inside its circle at 150 m and 180 m over 200 fields each; no gap on the ring wider than 16 m at a reach of 0.60, the last rock to the first included, with fillers in some; and `NarrowestReach` on a 2 × 1 block, a square and no vertices. Run in the Linux container against a stand-in for the Windows headers. `GameClient`'s use of them needs D3D12, so CI builds it and the owner's run sees it.

## What this forecloses

- **A field drawn as another field is,** short of two identifiers drawing alike.
- **A field the server blocks rock by rock.** The server's circle is the field; the rocks only have to read as it.

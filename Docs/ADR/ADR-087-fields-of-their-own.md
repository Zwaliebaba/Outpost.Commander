# ADR-087 — Each asteroid field is laid out from its identifier, its ring closed to the smallest hull

Status: **accepted** · 2026-10-10
Owns: asteroid-field-layout, field-ring-fillers

## Context

An asteroid field is a circle that the server blocks whole ([ADR-010](ADR-010-movement-and-pathing.md) decision 1), and the player reads it from the rocks the client draws in it. Two things follow.

- **Each field has to look its own.** Fields drawn alike read as one pattern stamped across the map.
- **The rocks have to read as the wall the server blocks.** The three rock meshes are longer than they are wide: measured from their `.nmf` files, fitted as the client fits them to 2 m long, each reaches from its center on the ground between 0.60 and 1.0 of its radius depending on the direction, and the narrowest reaches 0.60 (Small), 0.71 (Medium) and 0.62 (Large). A ring of rocks spaced by their full size leaves gaps between neighbors that look wide enough for a hull to pass.

So each field's rocks are laid out from its identifier, and smaller rocks close its ring wherever a hull could seem to fit (owner, 2026-10-08).

## Decision

1. **A field's rocks are laid out from its identifier** (`FieldLayout::Of`), with `EffectRandom` seeded by the identifier mixed with a constant of the layout's own ([ADR-026](ADR-026-explosions-and-particles.md) decision 7), so that one field looks the same in every frame and on every machine, and no two look alike.
   - **A rock in the middle,** 0.36 to 0.46 of the field's radius, up to 0.06 of it off the center along each axis.
   - **A ring of five to eight around it,** evenly spaced round the field from a turn of its own, each up to a fifth of a step off its place, at 0.56 to 0.68 of the radius from the center and 0.20 to 0.32 of it in size, never past the field's edge.
   - **Every rock is turned** by an angle of its own.
2. **Smaller rocks close the ring.** Between two neighbors on the ring, the gap is the distance between their centers less each one's least reach on the ground: its radius times the narrowest reach of the three meshes, which `GameClient` measures from them as it loads them (`FieldLayout::NarrowestReach`, 0.60 for the shipped meshes). Where that gap is wider than the smallest hull's footprint across, twice the least `footprintRadiusMeters` among the snapshot's hulls, rocks of 0.08 to 0.12 of the field's radius stand evenly along the line between the two, as few as leave no gap wider. With no hulls known, nothing is filled.
3. **It is the client's alone.** The server blocks the whole circle ([ADR-010](ADR-010-movement-and-pathing.md) decision 1), and nothing on the wire carries the layout. The rocks are instanced as every model is ([ADR-053](ADR-053-instanced-meshes.md)), and each field is laid out again as it is drawn.

## Consequences

- **The gaps the eye sees are narrower than the bound,** since the bound takes every rock at its narrowest. A ship's footprint fits through no gap in the ring.
- **Every field of the repository's map needs fillers:** 14 to 18 rocks a field, 15.7 on average, 1,254 over its 80 fields, computed by `FieldLayout` from `Map.json`'s field radii with the identifiers 1 to 80, at a reach of 0.60 and a gap of 16 m. They add instances, not draw calls.
- **The ring is a polygon of rocks,** the fillers standing on the straight line between two of the ring's, inside the circle the ring rocks lie on.
- **Tests:** `FieldLayoutTests` checks one field laid out the same twice; two fields laid out apart, and rings of five, six, seven and eight among 200; every rock inside its circle at 150 m and 180 m over 200 fields each; no gap on the ring wider than 16 m at a reach of 0.60, the last rock to the first included, with fillers in some; and `NarrowestReach` on a 2 × 1 block, a square and no vertices. `GameClient`'s use of them needs D3D12, so it has no test; CI builds it.

## What this forecloses

- **A field drawn as another field is,** short of two identifiers drawing alike.
- **A field the server blocks rock by rock.** The server's circle is the field; the rocks only have to read as it.

# ADR-044 — A Mining Rig stands on its legs, tilted to fit its rock

Status: **accepted** · 2026-10-03
Owns: rig-stance

## Context

A Mining Rig's legs touch its rock, with only the drill going in (owner, 2026-10-03). A rock is lumpy and a rig is rigid, so a rig held level stands with some legs in the rock or hanging over it: its legs can only all touch the rock if the rig tilts.

Where it tilts about matters. Tilted about its middle, its origin, which stands well above its feet, a foot swings sideways across the steep shoulder of the rock as the rig tilts, and the fit diverges. Tilted about the middle of its feet, the feet hardly move across the rock, and the fit settles in a few passes.

## Decision

1. **A rig tilts to fit its rock, then lifts until no foot is in it.** `Outpost::StandOnFeet` takes the feet in meters along the rig's own axes, and the rock's height under a point given the same way: `Neuron::SurfaceHeightAt` on the rock mesh, turned by its asteroid's heading and drawn at its radius.
   - It tilts the rig about the middle of its feet, by a bank and then a pitch, to the plane that fits the rock under the feet best by least squares.
   - It fits again as the tilt moves the feet, three times in all.
   - Then it lifts the rig until no foot is under the rock.
   - A rig on three legs then stands on all three. A rig on four rests on those the rock lets it, and none is in the rock.
2. **The tilt is at most 0.5 radians either way**, about 29°.
3. **Where it is drawn:** `Stance` holds the pivot, the bank, the pitch and the lift. `StanceMatrix` draws a mesh where a pose and a stance put it, and `StandPoint`, followed by `PlacePoint` at a scale of 1, puts a point where the matrix does. The rig and its explosion's shards are drawn with them.
4. **Only a Mining Rig stands this way.** Its feet are its lowest points out past half its half length from its middle, within 5% of its height of the lowest of them (`RIG_FOOT_RADIUS_SHARE`, `RIG_FOOT_HEIGHT_SHARE`): its legs' tips, not a drill under its middle. A rig without feet over the rock stands its lowest point on the rock's top.

## Consequences

- **No foot is in the rock**, whatever the rock: the lift of decision 1 guarantees it.
- **A rig on four legs rests on some and hangs the others.** Four rigid legs cannot all meet a lumpy rock, and the fit puts the gap evenly on the legs that do not.
- **`HardpointsTests` check it:**
  - The stance matrix against `StandPoint`.
  - Level ground and a slope.
  - A tripod and a table on uneven ground.
  - No ground under the feet, and feet in a line.

## What this forecloses

- A rig with a leg in its rock, without a new decision.

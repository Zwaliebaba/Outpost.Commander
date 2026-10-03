# ADR-044 — A Mining Rig stands on its legs, tilted to fit its rock

Status: **accepted** · 2026-10-03 · supersedes [ADR-027](ADR-027-rock-crease-lines.md) decision 5

## Context

Under ADR-027 decision 5, a Mining Rig is lowered until its lowest foot reaches the rock, so every other foot goes into it. With the owner's rig meshes the rig sinks deep into its rock, measured in the Linux container with the shipped meshes. On a 45 m rock, whose top is 26.8 m up, the Human rig's feet stood between 0.4 m and 6.9 m up, depending on its turn, so the rig sat about 20 m into the rock (ADR-042).

The owner decided on 2026-10-03 that the legs should touch the rock, with only the drill going in.

A rig is rigid, so the legs can only all touch the rock if the rig tilts. These figures were measured in the container with the shipped meshes, over five turns of the rig on each rock:

- **Held level, with no foot in the rock:**
  - A Human leg hangs up to 17.5 m over the rock.
  - A Tarkan leg hangs up to 12.8 m.
- **Tilted about its middle, its origin**, which is 16 m above its feet: a foot swings sideways across the steep shoulder of the rock as it tilts, and the fit diverges. A Human leg hung up to 34 m.
- **Tilted about the middle of its feet**, the feet hardly move across the rock, and the fit settles in three passes.

## Decision

1. **A rig tilts to fit its rock, then lifts until no foot is in it.** `Outpost::StandOnFeet` takes the feet in meters along the rig's own axes, and the rock's height under a point given the same way.
   - It tilts the rig about the middle of its feet, by a bank and then a pitch, to the plane that fits the rock under the feet best by least squares.
   - It fits again as the tilt moves the feet, three times in all.
   - Then it lifts the rig until no foot is under the rock.
   - A rig on three legs then stands on all three. A rig on four rests on those the rock lets it, and none is in the rock.
2. **The tilt is at most 0.5 radians either way**, about 29°. The game's rocks need up to 18°.
3. **Where it is drawn:** `Stance` holds the pivot, the bank, the pitch and the lift. `StanceMatrix` draws a mesh where a pose and a stance put it, and `StandPoint` puts a point where the matrix does. The rig and its explosion's shards are drawn with them.
4. **Only a Mining Rig stands this way.** Its feet are found as ADR-027 found them, its legs' tips and not its drill. A rig without feet over the rock still stands its lowest point on the rock's top.

## Consequences

All measured in the Linux container with the shipped meshes, over the same five turns on each rock:

- **The Tarkan rig, on three legs, stands on all three.** No foot is more than 0.6 m over the rock, which is the spread of a leg's tip, and the rig tilts up to 18°.
- **The Human rig, on four legs, rests on two and hangs the others.**
  - On a 45 m rock a foot hangs 0.7–6.2 m.
  - On a 60 m rock a foot hangs 1.9–5.7 m.
  - Four rigid legs cannot all meet a lumpy rock, and this puts the gap evenly on the legs that do not.
- **No foot is in the rock** in any of the twenty stances.
- **On a 45 m rock, the rock's crown goes up to 10 m into the Human rig's body** between its legs. Its legs are short for that rock: with its feet on the shoulders, the crown rises higher than its legs reach. The drills of both rigs go 6–20 m into the rock, as they should. Longer legs, or a narrower stance, in the Human mesh would let its body clear the crown. That is the owner's art to change, if the run shows it matters.
- **`HardpointsTests` check it:**
  - The stance matrix against `StandPoint`.
  - Level ground and a slope.
  - A tripod and a table on uneven ground.
  - No ground under the feet, and feet in a line.
- **The cost is not measured.** Each rig takes about four passes of a surface lookup for each foot a frame: 16 feet for the Human rig and 42 for the Tarkan.
- **The look is not run.** Only the owner's run shows whether a tilted rig reads well from the camera.

## What this forecloses

- A rig with a leg in its rock, without a new decision.

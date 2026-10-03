# ADR-029 — Ships bank in their turns, drawn by the client from the snapshots it already has

Status: **accepted** · 2026-10-02

## Context

The owner asked for ships to lean into their turns, so that they move more naturally (Phase 1 design §9, task 7.1), and decided on 2026-10-02 that the client draws it alone: the simulation, the snapshot, the replay (ADR-009) and the Q2 check do not change. Ships steer by heading on a plane: each tick a ship turns toward its next waypoint at its turn rate and flies forward, slower in a sharp turn (ADR-010, [ADR-039](ADR-039-ships-turn-in-arcs.md)). The server sends a position and a heading twenty times a second, and the client draws between two snapshots (ADR-013). A ship's guns, exhaust, crease lines and explosion shards are all placed from one pose (ADR-018, ADR-019, ADR-026, ADR-027).

## Decision

1. **The bank comes from the sideways acceleration.** A ship banks in proportion to its speed times how fast its heading turns, counterclockwise positive, up to its limit. A ship flying straight, or turning on the spot, does not bank. `Outpost::TargetBankRadians` gives it.
2. **The motion is read from the two snapshots around the view.** `SnapshotInterpolator::Motions` gives each entity's speed and turn rate between the older and the newer, the same for every frame between them, so it does not depend on the frame rate. With no newer snapshot, when snapshots stop, there are no motions, as the view does not extrapolate.
3. **A spring smooths it.** The target steps with each snapshot, so `Outpost::ShipBanking` follows it with a critically damped spring, solved exactly over each frame, so it too is the same at any frame rate. Its rate is 4 over the settle time, so a step is about nine tenths followed after one settle time. A ship the view no longer holds is forgotten and comes back level.
4. **Each hull and the Constructor have their own limits, in `Models.json`**, as presentation data beside each model's length (ADR-011): `maxDegrees`, at most 60; `fullAtMetersPerSecondSquared`, the sideways acceleration that reaches it; and `settleSeconds`. The values, the owner's of 2026-10-03 (Phase 1 design §9):

   | Ship | Bank | Full at | Settles in |
   |---|---|---|---|
   | Small | 50° | 60 m/s² | 0.25 s |
   | Medium | 45° | 26 m/s² | 0.3 s |
   | Large | 35° | 8 m/s² | 0.4 s |
   | Constructor | 30° | 55 m/s² | 0.3 s |

   Each hull's "full at" is the sideways acceleration of its Fusion design in a sharp turn at half speed, so every ship banks fully in a sharp turn, on any drive, and only a gentle bend leans part of the way. A faster drive on the same hull turns harder and so leans further, up to the same limit. Both are optional in the file: a ship without them flies level.
5. **The bank is part of the pose.** `ModelPose` carries `bankRadians`, a roll about the model's front that lowers its left side, +z, for a positive angle. `Outpost::PoseMatrix` draws it, and `PlacePoint` and `PlaceDirection` place hardpoints the same way, so the guns and the exhaust roll with the hull. `GameClient` draws ships, their crease lines and their explosions' shards by that matrix, so a ship destroyed mid-turn breaks apart banked. The footprint, the selection ring, the health bar and picking stay flat on the plane.

## Consequences

- **Every turn is a bank.** A ship turns as it flies and never on the spot (ADR-039), so it banks in every corner.
- **The figures are the owner's.** They are data, so changing them is an edit to `Models.json`.
- **No protocol change.** The server, the AI and the simulation's tests are unaffected. `GameAppTests` checks the bank without a GPU: none flying straight or turning on the spot, both signs, the limit, the spring, the same result at 30 and 120 frames a second, a hardpoint rolled where `PoseMatrix` puts it, the motions read from two snapshots, and the limits in the file.

## What this forecloses

- Pitch, for climbing or diving: the battlefield is flat (MVP design §4).

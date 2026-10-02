# ADR-029 — Ships bank in their turns, drawn by the client from the snapshots it already has

Status: **accepted** · 2026-10-02

## Context

The owner asked for ships to lean into their turns, so that they move more naturally (Phase 1 design §9, task 7.1), and decided on 2026-10-02 that the client draws it alone: the simulation, the snapshot, the replay (ADR-009) and the Q2 check do not change. Ships steer by heading on a plane (ADR-010): each tick a ship turns toward its next waypoint at its turn rate and moves at its cruise speed times the cosine of how far off course it is, so it slows into a sharp corner and does not move while it faces away. The server sends a position and a heading twenty times a second, and the client draws between two snapshots (ADR-013). A ship's guns, exhaust, crease lines and explosion shards are all placed from one pose (ADR-018, ADR-019, ADR-026, ADR-027).

## Decision

1. **The bank comes from the sideways acceleration.** A ship banks in proportion to its speed times how fast its heading turns, counterclockwise positive, up to its limit. A ship flying straight, or turning on the spot, does not bank. `Outpost::TargetBankRadians` gives it.
2. **The motion is read from the two snapshots around the view.** `SnapshotInterpolator::Motions` gives each entity's speed and turn rate between the older and the newer, the same for every frame between them, so it does not depend on the frame rate. With no newer snapshot, when snapshots stop, there are no motions, as the view does not extrapolate.
3. **A spring smooths it.** The target steps with each snapshot, so `Outpost::ShipBanking` follows it with a critically damped spring, solved exactly over each frame, so it too is the same at any frame rate. Its rate is 4 over the settle time, so a step is about nine tenths followed after one settle time. A ship the view no longer holds is forgotten and comes back level.
4. **Each hull and the Constructor have their own limits, in `Models.json`**, as presentation data beside each model's length (ADR-011): `maxDegrees`, at most 60; `fullAtMetersPerSecondSquared`, the sideways acceleration that reaches it; and `settleSeconds`. The first values:

   | Ship | Bank | Full at | Settles in |
   |---|---|---|---|
   | Small | 35° | 150 m/s² | 0.25 s |
   | Medium | 22° | 70 m/s² | 0.3 s |
   | Large | 12° | 20 m/s² | 0.4 s |
   | Constructor | 15° | 60 m/s² | 0.3 s |

   Each "full at" is about half the hardest turn the hull makes with the Ion Drive at full speed, its speed times its turn rate (§12 of the MVP design), so a ship banks fully in a hard turn and partly on a gentle curve. A faster drive on the same hull turns harder and so leans further, up to the same limit. Both are optional in the file: a ship without them flies level.
5. **The bank is part of the pose.** `ModelPose` carries `bankRadians`, a roll about the model's front that lowers its left side, +z, for a positive angle. `Outpost::PoseMatrix` draws it, and `PlacePoint` and `PlaceDirection` place hardpoints the same way, so the guns and the exhaust roll with the hull. `GameClient` draws ships, their crease lines and their explosions' shards by that matrix, so a ship destroyed mid-turn breaks apart banked. The footprint, the selection ring, the health bar and picking stay flat on the plane.

## Consequences

- **Banking shows on curves more than at corners.** A ship slows into a sharp corner and pivots at little speed (ADR-010), so it banks little there. It banks most as it speeds out of a turn, on a long curved route and in formation.
- **The figures are first guesses.** Whether the bank reads from the RTS camera at the default 500 m view, and how far each hull should lean, are for the owner's run. They are data, so changing them is an edit to `Models.json`.
- **No protocol change.** The server, the AI and the simulation's tests are unaffected. `GameAppTests` checks the bank without a GPU: none flying straight or turning on the spot, both signs, the limit, the spring, the same result at 30 and 120 frames a second, a hardpoint rolled where `PoseMatrix` puts it, the motions read from two snapshots, and the limits in the file.

## What this forecloses

- Turning in arcs in the simulation, which would make every corner a bank. It changes movement, pathing, kiting and the final turn rates, and is not part of Phase 1 (Phase 1 design §9).
- Pitch, for climbing or diving: the battlefield is flat (MVP design §4).

# ADR-039 — Ships turn in arcs, slowing into a sharp turn, and never turn on the spot

Status: **accepted** · 2026-10-03
Owns: ship-turning, turning-speed, coming-about

## Context

Ships move as aircraft do: forward while they turn, banking in a corner (owner, 2026-10-03). A ship slows to turn, so that it is slower in a sharp turn, and it stops at its destination and stands still to fire ([ADR-010](ADR-010-movement-and-pathing.md) decisions 6 and 7). The client banks a ship in proportion to its speed times its turn rate ([ADR-029](ADR-029-ship-banking.md)), so a ship that stopped and turned on the spot would not bank just where a turn is sharpest.

The turn rates are final ([ADR-010](ADR-010-movement-and-pathing.md) decision 8). At full speed they give every ship a turn radius of its cruise speed over its turn rate: computed from the packaged `Tuning.json`, about 20 m for Small+Ion and 24 m for Large+Fusion. A ship that cannot turn on the spot can miss a point inside that circle forever, circling it, and ADR-010 decision 6's stall rule would end that circling short of the point.

## Decision

1. **A ship flies the way it faces and turns as it goes.** Each tick it turns toward its next waypoint by at most its turn rate, and moves forward. It never turns on the spot, from a standstill either.
2. **It slows into a turn.** Its speed is its cruise speed times ½ + ½·cos(bearing), where the bearing is the angle between its heading and its waypoint, and never less than half its cruise speed on this count. So it flies full speed on course and half speed with its waypoint abeam or behind. Decision 3 can slow it further. It comes about in a loop about twice its half-speed turn radius across: computed from the packaged `Tuning.json`, 20 m for Small+Ion, 24 m for Large+Fusion.
3. **It never flies faster than lets it reach its waypoint on an arc.** The circle that leaves the ship along its heading and passes through the waypoint has radius *d* / (2 sin |bearing|), where *d* is the distance to the waypoint. The ship flies no faster than its turn rate times that radius. Its own turning circle is then never wider than that circle, so the waypoint is never inside it, and the ship reaches it without circling. Near a point to its side, it slows down to make it.
4. **Coming about is not stalling.** Only a tick in which the ship heads within 45° of its waypoint and gets no closer counts toward ADR-010 decision 6's one-second stall. While it heads further off than that, its progress is measured afresh, from where it comes about.
5. *(retired)*

## Consequences

- **Every turn is a bank.** A ship turns at speed, so it banks in every corner, and reaches ADR-029's limits in a sharp one.
- **A U-turn is a loop, and a point close abeam is reached directly.** A ship sent behind itself comes about in its loop and arrives exactly, and a ship sent to a point inside its turning circle slows and reaches it without circling.
- **The battles change only in how ships close.** Weapons fire in any direction, and a ship standing to fire does not move.
- **An arc can graze an obstacle.** A ship that comes about near an asteroid or a structure swings up to about its loop's width outside its path. ADR-010 decision 7's rule that a ship inside an obstacle leaves it by the shortest way keeps it out.
- **`MovementTests` check it.** `AShipComesAboutInALoop`: a Small+Ion and a Large+Fusion ship sent behind themselves move every tick, turn no faster than their rate, come about at half speed or less in a loop to one side, and arrive exactly. `AShipReachesAPointInsideItsTurn`: a point 15 m abeam is reached exactly within a second.

## What this forecloses

- A ship turning on the spot, without a new decision.
- Momentum, and a ship that cannot stop. They would change how battles are fought and what the balance check assumes.

# ADR-039 — Ships turn in arcs, slowing into a sharp turn, and never turn on the spot

Status: **accepted** · 2026-10-03 · supersedes ADR-010's decision 5, how ships steer, and the stall rule's exemption in its decision 6

## Context

Under ADR-010 decision 5, a ship turns toward its next waypoint at its turn rate and moves at its cruise speed times the cosine of how far off course it is. It does not move while it faces away. So at a sharp corner, and whenever an order sends it somewhere behind it, a ship stops, turns on the spot and then sets off. Task 7.1 made ships bank, drawn by the client only, and that rule kept them from banking just where a turn is sharpest (Phase 1 design §9).

The owner judged the result unnatural, and asked on 2026-10-03 for ships to move as aircraft do: forward while they turn, banking in a corner. The owner decided that:

- **Ships can still stop.** They stop at their destination and stand still to fire, as now.
- **A ship slows to turn.** It is slower in a sharp turn.
- **It is task 7.2 of Phase 1**, which moves "turning in arcs" out of design §14's list of what Phase 1 leaves out.

The turn rates are final (ADR-010 decision 8). At full speed they give every ship a turn radius of about 20–25 m, its cruise speed over its turn rate: Small+Ion 78 m/s at 225° a second is 19.9 m, and Large+Fusion 20 m/s at 48° a second is 23.9 m. A ship that cannot turn on the spot can still miss a point inside that circle forever. The old rule did exactly that, in a milder form: sent to a point 15 m abeam, a Small+Ion ship took 6.3 s and gave up 2.1 m short, when the stall rule ended its circling.

## Decision

1. **A ship flies the way it faces and turns as it goes.** Each tick it turns toward its next waypoint by at most its turn rate, and moves forward. It never turns on the spot, from a standstill either.
2. **It slows into a turn.** Its speed is its cruise speed times ½ + ½·cos(bearing), where the bearing is the angle between its heading and its waypoint, and never less than half its cruise speed on this count. So it flies full speed on course and half speed with its waypoint abeam or behind. Decision 3 can slow it further. It comes about in a loop about twice its half-speed turn radius across: 20 m for Small+Ion, 24 m for Large+Fusion.
3. **It never flies faster than lets it reach its waypoint on an arc.** The circle that leaves the ship along its heading and passes through the waypoint has radius *d* / (2 sin |bearing|), where *d* is the distance to the waypoint. The ship flies no faster than its turn rate times that radius. Its own turning circle is then never wider than that circle, so the waypoint is never inside it, and the ship reaches it without circling. Near a point to its side, it slows down to make it.
4. **Coming about is not stalling.** Only a tick in which the ship heads within 45° of its waypoint and gets no closer counts toward ADR-010's one-second stall. While it heads further off than that, its progress is measured afresh, from where it comes about.
5. **Nothing else in ADR-010 changes.**
   - The paths, the formation and its slots, and the cruise speeds that bring a group in together stay as they were.
   - So does passing corners loosely: a ship drops a corner within its radius of it, or when it sees the next waypoint.
   - Ships still part rather than collide, still stop dead at their destination, and still stand to fire on an attack-move, with no momentum.
   - The turn rates and speeds are unchanged.

## Consequences

- **Every turn is a bank.** Task 7.1's banking leans a ship in proportion to its speed times its turn rate. A ship now turns at speed, so it banks in every corner. ADR-029's limits were set for a full-speed turn, and it reaches them.
- **A U-turn takes a little longer, a point close abeam much less.** A single ship sent 100 m behind itself arrives in 2.05 s instead of 1.90 s as Small+Ion, and in 8.55 s instead of 7.90 s as Large+Fusion. A point 15 m abeam takes 0.75 s instead of 6.3 s, and is reached exactly instead of 2.1 m short. A route with gentle bends is flown as before. All measured in the Linux container, one ship on an open map.
- **The battles change only in how ships close.** Weapons fire in any direction, and a ship standing to fire does not move. The balance check was run again with arcs in the Linux container, 88 minutes on four threads that it shared with other runs:
  - **(a), (b) and (c) pass at every stage.**
  - **(d) is UNSURE at tier 1.** With Hull Plating on the swarm's side, at 2,000 Ore under spread fire, its best answer, the brawler, wins 53% of 480 battles, and the interval straddles one half. Before the arcs it won 55% and passed, narrowly: the same pairing of the MVP's numbers was already on the edge.
  - Every other verdict holds.
  - Nothing is retuned until the owner decides.
- **Two AIs end a match a little later.** Over 140 seeds the median is 23:32, against 22:11 over 40 seeds before; P1 asks for 45–60 minutes. The seat that wins about two-thirds of the matches changed: player 2 now wins 89 of the 140, where player 1 won 30 of 40. That is a bias whose direction follows the movement rule, and its cause is not yet known (ADR-038).
- **The stress scene's ticks did not move**: a median of 0.24 ms, and a slowest tick of 1.6–2.4 ms over three runs, against 1.5 ms before. Measured in the Linux container.
- **An arc can graze an obstacle.** A ship that comes about near an asteroid or a structure swings up to about its loop's width outside its path. ADR-010's rule that a ship inside an obstacle leaves it by the shortest way keeps it out.
- **`MovementTests` check it.** `AShipComesAboutInALoop`: a Small+Ion and a Large+Fusion ship sent behind themselves move every tick, turn no faster than their rate, come about at half speed or less in a loop to one side, and arrive exactly. `AShipReachesAPointInsideItsTurn`: a point 15 m abeam is reached exactly within a second. Both fail under ADR-010's old rule. Every other GameLogic suite passes unchanged.
- **Not run.** Only the owner's run shows whether the arcs and their banking look natural.

## What this forecloses

- A ship turning on the spot, without a new decision.
- Momentum, and a ship that cannot stop. Both were the other choice on 2026-10-03, and they would change how battles are fought and what the balance check assumes.

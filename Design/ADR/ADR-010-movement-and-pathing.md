# ADR-010 — Ships path on a visibility graph, steer by heading, and part without colliding

Status: **accepted** · 2026-09-30

## Context

Design §9 says ships path around asteroid obstacles and hold a loose formation at the pace of the slowest ship when moved as a group, and that they avoid overlapping but do not collide physically. Design §4 makes every obstacle a circle on a plane. Implementation plan task 2.4 builds that in `GameLogic`, with provisional footprint radii and turn rates held as data until gate G5 sets ship sizes. Q4 asks that a tick take at most 5 ms with 200 ships (design §3). ADR-009 makes the tick the only clock and allows floats, since a replay only has to reproduce on the same binary.

## Decision

1. **Paths come from a visibility graph over grown circles.** `Outpost::Pathfinder` grows every obstacle by the ship's footprint radius plus a 0.5 m margin, rings each grown circle with a 16-cornered polygon whose edges touch it, and joins every pair of corners that can see each other. A path is the shortest route through that graph from the ship to its goal, found by A* with the straight-line distance. When the straight line is clear, the path is the goal alone. On a map of circles this gives shortest paths without a grid, and the graph has a few hundred corners on the 2.3 map. The search checks lazily whether the start sees a corner: each corner is first given the straight line from the start, and the line is tested only when the corner comes off the queue. Testing every corner up front was most of a search's cost.
2. **One graph per footprint radius, built at match start.** The graph depends only on the obstacles and the radius. `InProcessServer` builds one for each hull's radius before the first tick and keeps them, so an order pays only for its search. The graphs are a cache: `Simulation`'s equality ignores them.
3. **A goal inside an obstacle moves to its edge.** A ship ordered onto an asteroid stops beside it, and a goal beyond the map's edge is pulled inside it.
4. **A group moves as a formation, along one route.** A move order for several ships lays a grid of slots around the destination, facing the way the group travels, with three footprint radii of the group's widest ship between neighbors. The ships that are ahead now take the front slots, so few paths cross. The group searches once, from its ships' mean position to the destination at its widest ship's clearance (`Outpost::GroupRoutes`, decided by the owner on 2026-09-30). Each ship goes straight to its slot when it can. Otherwise it joins that route at the furthest corner it can see and follows it, through the destination when the route's end does not see its slot. Only a ship that sees no route, or whose way along one would be more than 1.5 times the straight line to its slot, searches for itself. Its path then becomes a route the ships after it may join, so ships cut off from the group together pay for one search. The limit matters because every ship cruises at its path's length divided by the time the slowest ship needs for its own path, so all arrive together and none goes faster than it can: one ship's detour would slow the whole group. A single ship is a group of one and searches for itself.
5. **Ships steer by heading.** Each tick a ship turns toward its next waypoint at its turn rate, and moves the way it faces at its cruise speed times the cosine of how far off course it is. It does not move while it faces away, so it turns before it sets off and does not circle a waypoint.
6. **Corners are passed loosely and slots exactly.** A ship drops the next corner of its path once it is within its own radius of it, or can see the waypoint after it. Only the last waypoint, its slot, has to be reached. A ship that has tried to move for a second without getting 0.1 m closer to its waypoint gives it up: it passes a corner, or counts itself arrived at its slot. A ship turning on the spot is not stalled. This is what ends every jam: ships crowding round the same corner, and slots that an obstacle or the edge pushed onto each other.
7. **Ships part, they do not collide.** After moving, each pair of overlapping ships is pushed apart along the line between their centers, half each, in identifier order. Then a ship inside an obstacle leaves it by the shortest way, and a ship past the edge is pulled back. There is no momentum and no damage. Checking every pair is O(n²), which is 20,000 pairs at 200 ships.
8. **Movement numbers are data.** Each hull has `footprintRadiusMeters` and `turnRateDegreesPerSecond`, and each drive a `turnRateFactor`, in `OutpostCommander/Assets/Tuning.json` (ADR-008). They are provisional until G5. `MovementFor` combines them with the hull's speed and the drive's speed factor. The server refuses to start when a hull's footprint is wider than the map's minimum gap (ADR-008, task 2.3), because such a ship could be walled off.

## Consequences

- **Measured cost.** Task 2.7 measured the order ticks on the development machine at up to 8.3 ms, against Q4's 5 ms, with one search per ship (design §3). That is why the group now searches once. The figures below compare the two ways on task 2.7's load: 200 ships and 40 structures, with both 100-ship fleets ordered across the map in one tick, 20 times. They ran in a Linux container built with clang 18 at `-O2`, not on the development machine, so read them as a ratio. The development machine's figures come from the owner's `--measure --load` run.
  - One search per ship: a mean of 3.1–4.1 ms per order tick over three runs.
  - The group's route, with the lazy search: 0.8–1.3 ms, about a quarter.
  - The load's fleets are scattered over half the map, which is the worst case for a shared route: a clump sees its route. About one ship in ten still searches for itself.
  - In both, all 200 ships arrived within 60 s of the last order, and no two overlapped. A first version that also pulled hidden slots to the near side of an obstacle was faster still, but left ships jostling at the obstacle's edge, so slots stay where the grid puts them.
  - Building the three graphs takes about 9 ms, which is why that happens at match start.
- **A very large group's formation outgrows the space.** Two hundred ships make a grid about a kilometer across. Slots past the edge or inside obstacles are moved clear, some land on each other, and the stall rule settles those ships near their slots. The formation is loose, as the design asks, and the ships do not overlap.
- **Arrival is a position, not a promise.** A ship that stalls "arrives" where it stopped, up to about a footprint from its slot.
- **Structures become obstacles when they exist** (task 4.2). The graphs then have to be rebuilt, or patched, when a structure is placed or destroyed.

## What this forecloses

- Grid or navigation-mesh pathing, flow fields and steering behaviors such as boids, without a new ADR.
- Physical collision between ships: ships part, but they do not bounce or damage each other.
- Formation shapes other than the grid, or formations that turn with the group on the way. The grid faces the direction of travel when the order is given.

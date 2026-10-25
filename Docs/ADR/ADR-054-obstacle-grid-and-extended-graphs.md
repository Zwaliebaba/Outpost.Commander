# ADR-054 — Line tests use a grid over the obstacles, and the graphs are brought up to date as obstacles come and go

Status: **accepted** · 2026-10-07
Owns: obstacle-grid, path-graph-maintenance, obstacle-overlap-check

## Context

**Line tests.** `Pathfinder::IsStraightPathClear` decides whether a ship can go straight, and a graph build asks it of every pair of corners. Testing a line against an obstacle is a cheap test of the obstacle's box against the line's, and the distance to the line for the obstacles the box does not rule out. A graph build also asks, of every corner round every obstacle, whether any obstacle covers it, which is a square root per obstacle per corner. Most obstacles are nowhere near a short line or a corner.

**Graph builds.** Structures are placed and destroyed all match long, and a ship whose path a new structure blocks searches again in the same tick. Building a graph whole tests every pair of its corners, so it costs far more than the change that made it, and on the repository's map ([ADR-036](ADR-036-map-sectors.md) decision 4) a graph spans every obstacle on the map.

**One answer.** Every path, tick and snapshot has to be what a test of every obstacle and a whole build give, since a different answer would change a match (ADR-009).

## Decision

1. **`Pathfinder` keeps a uniform grid over the obstacles,** built whenever the obstacles change. Each cell lists the obstacles whose circle's square overlaps it. Cells are `GRID_CELL_METERS`, 100 m, wide, about as wide as an ore asteroid or a structure grown by a ship's clearance, or wider when the obstacles would need more than `GRID_MOST_CELLS`, 256, cells across; on the repository's map ([`sizeMeters`](../Reference/Catalogue.md#sizeMeters)) that is about 100 to a side.
2. **A line that spans at most three cells along either axis tests only the obstacles the grid finds near it.** The lookup walks the line's longer axis column by column, and in each column the cells across that the line passes within its clearance, the margin and a meter of slack. Each obstacle found is tested once, with the test `IsStraightPathClearOfEveryObstacle` makes of every obstacle, so the answer cannot differ:
   - An obstacle that blocks the line has its square within its clearance and margin of a point of the line, in both axes. A cell index never falls as the coordinate grows, and the grid places obstacles and looks up lines with the same function, so the lookup reaches a cell holding it.
   - The slack, 1 m, is many times the rounding of every sum the lookup makes, which is a few hundredths of a meter at most within 65,536 m of the origin. An obstacle or a line beyond that, a negative clearance, or a number that is not one, is tested against every obstacle.
   - Which obstacles are tested, and in what order, changes nothing: the answer is whether all of them pass.
3. **A longer line tests every obstacle.** It would look in so many cells that testing every obstacle's box costs less.
4. **A graph build asks the grid which obstacles might cover a corner,** the same lookup for a point, and tests each of those as it would test any obstacle.
5. **The graphs are kept as the obstacles change** (owner, 2026-10-07), and only a new edge to the map drops them. Each graph keeps the obstacles it is built over. When it is next needed, by the path that needs it or by a quiet tick, as a dropped graph is built again (ADR-032 decision 2), it is brought up to date, and that is counted as a graph build. Its obstacles are matched in order against those there are now, bit for bit: the obstacles are the map's, then the structures in identifier order (`Simulation::UpdateObstacles`), so a structure destroyed leaves a gap in the list and one placed comes last. The graph's obstacles that do not match are taken away (decision 7), and those after the last that matched are added (decision 6). With more than half of them taken away it is built whole instead, which then costs less.
6. **A graph is extended over the obstacles added.** The extended graph is the very graph a build over every obstacle makes, corner for corner and edge for edge, in the same order and to the bit:
   - **Its corners** are those round each obstacle in turn that the edge and no obstacle cover: the graph's own corners that no added obstacle covers, in their order, then the added obstacles' free corners.
   - **Its edges.** A build tests each pair of corners from the lower to the higher, so each corner's edges are in the order of the corners they reach, and so are an extended graph's.
   - **An edge the graph had stands unless an added obstacle blocks it,** and keeps its length. Whether its line touches both corners' polygons depends on the two corners alone, and the graph's own obstacles let it pass.
   - **Every line to an added obstacle's corner is tested whole,** as a build tests it. Each corner keeps its outward direction and how far a line may turn there and still touch its polygon, for that test.
   - **A whole build is an empty graph extended over every obstacle,** so the two are one function.
7. **A graph is reduced by the obstacles taken away,** to the very graph a build over those left makes, to the bit, as decision 6's is:
   - **Its corners** are made again round each obstacle left, in order, by the function that makes every graph's, and each is matched against the graph's next corner, bit for bit. One that matches stays; any other is one a removed obstacle covered, and it is kept, as a build keeps it, if the edge and no obstacle cover it.
   - **An edge the graph had stands,** with its length: taking an obstacle away blocks nothing.
   - **A line between two of the graph's corners that is not an edge** can become one only if a removed obstacle blocked it, since whether it touches both polygons depends on its corners alone and nothing else that blocked it has changed. Such a line crosses the removed obstacle's grown circle, and two corners outside a circle are joined across it only when they lie nearly opposite each other round its center: the angle between them, seen from the center, falls short of a half turn by at most the sum of each one's asin(radius / distance). So the corners are sorted by their angle round the removed obstacle, in sixteen bands by that asin, each band's widest twice the next's, and each corner looks across into each band only as far as its own and the band's widest allow, with a thousandth of a radian for rounding. The lines found that touch both polygons, are not edges and that a removed obstacle blocks are tested whole. A corner far from the removed obstacle looks across a sliver of the map; on the repository's map most corners are.
   - **Every line to a corner that came back is tested whole,** as a build tests it.
   - **Each corner's edges are sorted** by the corner they reach, as a build orders them, where any were added.
8. **A ship that overlaps no obstacle is not pushed out of one.** `Simulation::KeepShipsClear` first asks `Pathfinder::OverlapsObstacle`, which looks up the grid for a point and tests what it finds with the very test the push makes. A ship that does overlap one is pushed out of every obstacle in their order, since a push can move it into another. The push itself is [ADR-010](ADR-010-movement-and-pathing.md) decision 7's.

## Consequences

- The tick's budget, and what the grid and the graphs cost against it, are in `Docs/Budgets.md`.
- **Tests:**
  - `PathfinderTests.TheGridAnswersAsATestOfEveryObstacleDoes` holds the grid to a test of every obstacle on 24,000 seeded lines and points, short and long, many grazing an obstacle's grown edge, over 40 seeded fields of up to 160 obstacles. `IsStraightPathClearOfEveryObstacle` is that test of every obstacle, public for it.
  - `PathfinderTests.TakenAwayObstaclesReduceTheGraphsToWhatAWholeBuildMakes` does the same for one to three obstacles taken away from anywhere in the list, with one added every other step, over 6 seeded maps, half of them with their structures packed into a base.
  - `PathfinderTests.AddedObstaclesExtendTheGraphsToWhatAWholeBuildMakes` compares graphs extended over 6 seeded maps' structures, added one to three at a time, with graphs built whole: corner for corner and edge for edge, to the bit, half of them extended over several additions at once, and 30 seeded paths each time. `GraphFor` is public for it.
  - `MovementTests.RebuildsDroppedGraphsOnQuietTicks` holds the quiet ticks after a structure is placed to bringing the graphs up to date, one a tick.
- **The grid and its lookups are the pathfinder's own scratch,** written by `const` line tests, as the lazily built graphs are: only the server's thread uses a pathfinder. Each graph also keeps a direction and a touch limit per corner, and a copy of its own obstacles.

## What this forecloses

- A line test whose answer depends on which obstacles it tests, or on their order.
- A cell function that is not the same for placing an obstacle and looking a line up, or that can fall as a coordinate grows.
- Listing the obstacles in another order, such as structures before the map's, without giving up graphs brought up to date: a placement would not only add, nor a destruction only take away.

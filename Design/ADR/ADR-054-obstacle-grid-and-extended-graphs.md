# ADR-054 — Line tests use a grid over the obstacles, and an added obstacle extends the graphs

Status: **accepted** · 2026-10-03

## Context

**Line tests.** `Pathfinder::IsStraightPathClear` tested a line against every obstacle: a cheap test of the obstacle's box against the line's, and the distance to the line for the obstacles the box does not rule out. ADR-032 counted line tests at 44% of an order's planning, and tried a uniform grid over the obstacles, looking in every cell of the line's box: on its load's long lines and 54 obstacles that cost 6% more than testing every obstacle. A graph build also asked, of every corner round every obstacle, whether any obstacle covers it, which is a square root per obstacle per corner.

**Graph builds.** A structure placed or destroyed used to drop every graph. A ship whose path a new structure blocks searches again in the same tick, and that search built its radius's graph whole, up to four in one tick; the quiet ticks after built the rest, one a tick ([ADR-032](ADR-032-order-ticks.md) decision 2). Patching a graph instead had been named as the next step. Two AIs place structures all match long, and by its second half a graph spans some 150 obstacles.

The owner's server work asked for both, with every path, tick and snapshot exactly as before, since a different answer would change a match (ADR-009).

Measured in the Linux container with clang 18 at `-O2`. Instructions were counted with callgrind, as ADR-032 counted them; times were taken on the server's own clock, each build running alone. The container has no AVX2, and the development machine's figures come from the owner's `--measure` run. The loads:

- **Two AIs' match:** both players the AI on the 5 km map, as `--ai-matches` plays them.
- **ADR-032's measurement load:** 200 ships and 40 structures, both fleets ordered across the map every 200 ticks, 2,400 ticks.
- **Task 3.7's stress scene:** 2,000 ticks.

## Decision

1. **`Pathfinder` keeps a uniform grid over the obstacles,** built whenever the obstacles change. Each cell lists the obstacles whose circle's square overlaps it. Cells are 100 m wide, about as wide as an ore asteroid or a structure grown by a ship's clearance, 50 to a side on the 5 km map, or wider when the obstacles would need more than 256 cells across.
2. **A line that spans at most three cells along either axis tests only the obstacles the grid finds near it.** The lookup walks the line's longer axis column by column, and in each column the cells across that the line passes within its clearance, the margin and a meter of slack. Each obstacle found is tested once, with the very test every line test made of every obstacle before, so the answer cannot differ:
   - An obstacle that blocks the line has its square within its clearance and margin of a point of the line, in both axes. A cell index never falls as the coordinate grows, and the grid places obstacles and looks up lines with the same function, so the lookup reaches a cell holding it.
   - The slack, 1 m, is many times the rounding of every sum the lookup makes, which is a few hundredths of a meter at most within 65,536 m of the origin. An obstacle or a line beyond that, a negative clearance, or a number that is not one, is tested against every obstacle.
   - Which obstacles are tested, and in what order, changes nothing: the answer is whether all of them pass.
3. **A longer line tests every obstacle, as before.** It would look in so many cells that testing every obstacle's box costs less.
4. **A graph build asks the grid which obstacles might cover a corner,** the same lookup for a point, and tests those with the test it made of every obstacle.
5. **Obstacles only added keep the graphs.** `SetObstacles` keeps them when the obstacles there were are the start of the new list, bit for bit and in order, and the map's edge is unchanged. Each graph knows how many obstacles it was built over. Any other change drops them, as before. The obstacles are the map's, then the structures in identifier order (`Simulation::UpdateObstacles`), and a structure placed is the newest, so a placement only adds; a structure destroyed drops them.
6. **A graph is extended over the obstacles added since it was built when it is next needed:** by the path that needs it, or by a quiet tick, as a dropped graph is built again (ADR-032 decision 2). It is counted as a graph build. The extended graph is the very graph a build over every obstacle makes, corner for corner and edge for edge, in the same order and to the bit:
   - **Its corners** are those round each obstacle in turn that the edge and no obstacle cover: the graph's own corners that no added obstacle covers, in their order, then the added obstacles' free corners.
   - **Its edges.** A build tests each pair of corners from the lower to the higher, so each corner's edges are in the order of the corners they reach, and so are an extended graph's.
   - **An edge the graph had stands unless an added obstacle blocks it,** and keeps its length. Whether its line touches both corners' polygons depends on the two corners alone, and the obstacles there were let it pass already.
   - **Every line to an added obstacle's corner is tested whole,** as a build tests it. Each corner keeps its outward direction and how far a line may turn there and still touch its polygon, for that test.
   - **A whole build is an empty graph extended over every obstacle,** so the two are one function.

## Consequences

- **Measured, every line through the grid costs more, as ADR-032 found.** With 200 m cells, the first 12,000 ticks of seed 1's match took 8.40 G instructions against 7.41 G before, 13% more: a line across the map crosses dozens of cells, each a few more instructions than an obstacle's box. Taking every line longer than 3 cells to every obstacle instead, with 100 m cells, was the cheapest of the settings tried.
- **Measured, as decided,** in instructions: in the simulation's ticks, and in the graph builds within them or before the first.

  | Load | | Before | Grid | Grid and extended graphs |
  |---|---|---|---|---|
  | Seed 1's match, first 12,000 ticks | ticks | 7.41 G | 7.08 G | 5.00 G |
  | | graph builds | 2.71 G | 2.43 G | 0.36 G |
  | Measurement load | ticks | 3.83 G | 3.83 G | 3.83 G |
  | | graph builds | 170 M | 108 M | 109 M |
  | Stress scene | ticks | 5.00 G | 5.00 G | 5.00 G |
  | | graph builds | 173 M | 110 M | 112 M |

  The grid saves on graph builds' corners and short lines; the ships' own line tests cost about what they did. The measurement load and the stress scene place no structure once they run, so extending graphs does not touch them.
- **Measured in time,** seeds 1 to 4 played for 72,000 ticks each: the slowest tick went from 52–71 ms to 15–34 ms, and ticks over Q4's 5 ms from 413–558 a match to 262–399. The slowest ticks left are whole builds of a graph a destroyed structure dropped. Extending graphs past a removal is the next step if the development machine's figures need it.
- **Unchanged, bit for bit.** Built the same way in the container before and after, eight two-AI matches of 72,000 ticks each, hashing every snapshot and, every 6,000 ticks, the whole world, the measurement load and the stress scene give exactly the snapshots and states they gave before, and four `--ai-matches` matches of 15 minutes write exactly the same log.
- **Tests:**
  - `PathfinderTests.TheGridAnswersAsATestOfEveryObstacleDoes` holds the grid to a test of every obstacle on 24,000 seeded lines and points, short and long, many grazing an obstacle's grown edge, over 40 seeded fields of up to 160 obstacles. `IsStraightPathClearOfEveryObstacle` is that test of every obstacle, public for it.
  - `PathfinderTests.AddedObstaclesExtendTheGraphsToWhatAWholeBuildMakes` compares graphs extended over 6 seeded maps' structures, added one to three at a time, with graphs built whole: corner for corner and edge for edge, to the bit, half of them extended over several additions at once, and 30 seeded paths each time. `GraphFor` is public for it.
  - `MovementTests.RebuildsDroppedGraphsOnQuietTicks` holds as it did: the quiet ticks after a structure is placed now extend the graphs, one a tick.
- **The grid and its lookups are the pathfinder's own scratch,** written by `const` line tests, as the lazily built graphs are: only the server's thread uses a pathfinder. Each graph also keeps a direction and a touch limit per corner.

## What this forecloses

- A line test whose answer depends on which obstacles it tests, or on their order.
- A cell function that is not the same for placing an obstacle and looking a line up, or that can fall as a coordinate grows.
- Listing the obstacles in another order, such as structures before the map's, without giving up extended graphs: a placement would no longer only add.

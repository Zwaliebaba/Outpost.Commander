# ADR-054 — Short line tests and corners look through a grid over the obstacles

Status: **accepted** · 2026-10-03 · takes up the grid [ADR-032](ADR-032-order-ticks.md) tried and did not keep, for short lines and a graph's corners only

## Context

`Pathfinder::IsStraightPathClear` tests a line against every obstacle: a cheap test of the obstacle's box against the line's, and the distance to the line for the obstacles the box does not rule out. ADR-032 counted line tests at 44% of an order's planning, and tried a uniform grid over the obstacles, looking in every cell of the line's box: on its load's long lines and 54 obstacles that cost 6% more than testing every obstacle. A graph build also asks, of every corner round every obstacle, whether any obstacle covers it, which is a square root per obstacle per corner.

The owner's server work asked for the grid again, with an answer exactly the same as today's for every input, since a different answer would change a path and so a match (ADR-009).

Measured in the Linux container with clang 18 at `-O2`, by counting instructions with callgrind, as ADR-032 counted them. The container has no AVX2; the development machine's figures come from the owner's `--measure` run. The loads:

- **Two AIs' match:** seed 1's first 12,000 ticks, both players the AI on the 5 km map, as `--ai-matches` plays them.
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

## Consequences

- **Measured, every line through the grid costs more, as ADR-032 found.** With 200 m cells, the two AIs' match took 8.40 G instructions in its ticks against 7.41 G before, 13% more: a line across the map crosses dozens of cells, each a few more instructions than an obstacle's box. Taking every line longer than 3 cells to every obstacle instead, and 100 m cells, was the cheapest of the settings tried.
- **Measured, as decided:**

  | Load | Ticks before | Ticks after | Graph builds before | Graph builds after |
  |---|---|---|---|---|
  | Two AIs' match | 7.41 G | 7.08 G | 2.71 G | 2.43 G |
  | Measurement load | 3.83 G | 3.83 G | 170 M | 108 M |
  | Stress scene | 5.00 G | 5.00 G | 173 M | 110 M |

  Instructions in the simulation's ticks, and in building the graphs within them or before the first. The saving is the graph builds' corners and short lines; the ships' own line tests cost about what they did.
- **Unchanged, bit for bit.** Built the same way in the container before and after, eight two-AI matches of 72,000 ticks each, hashing every snapshot and, every 6,000 ticks, the whole world, the measurement load and the stress scene give exactly the snapshots and states they gave before, and four `--ai-matches` matches of 15 minutes write exactly the same log. `PathfinderTests.TheGridAnswersAsATestOfEveryObstacleDoes` holds the grid to a test of every obstacle on 24,000 seeded lines and points, short and long, many grazing an obstacle's grown edge, over 40 seeded fields of up to 160 obstacles.
- **`IsStraightPathClearOfEveryObstacle`** is the test of every obstacle, kept public so that tests can hold the grid to it.
- **The grid is the pathfinder's own scratch.** It is rebuilt with the obstacles, and its lookups write a pass number per obstacle, so a `const` pathfinder is no more safe to share between threads than it was with its lazily built graphs: only the server's thread uses one.

## What this forecloses

- A line test whose answer depends on which obstacles it tests, or on their order.
- A cell function that is not the same for placing an obstacle and looking a line up, or that can fall as a coordinate grows.

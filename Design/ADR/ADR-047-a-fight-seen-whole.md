# ADR-047 — A group passes an obstacle side by side, in lanes

Status: **accepted** · 2026-10-03
Owns: group-lanes, lane-corner-release, attack-group-repath

## Context

Fleets move as whole groups: the AI sends each attack as one attack-move order for the whole group ([ADR-041](ADR-041-ai-plays-a-longer-match.md) decision 1), and a player orders a selection the same way. A group in single file loses a fight a ship at a time: an attack-moving ship stands to fire once an enemy is in range, so the head of a column stops and fights while the rest arrive one after another.

Two rules of movement, working together, would turn any group into a column at an obstacle:

- **A group that shares one route passes an obstacle through one corner.** If every ship whose way to its slot is blocked joins the group's route at a corner, every ship of the group passes the obstacle through the same corner.
- **A ship that lets a corner go as soon as it sees the waypoint after it cuts to the same place.** Past an obstacle, a ship sees its slot as soon as the straight line to the slot clears the obstacle's edge. Every such line touches the obstacle at the same place, so even a ship given its own way round would pass the obstacle where every other ship does.

## Decision

1. **A group keeps a band of lanes along its route.**
   - The band is as wide as the group's grid of slots: half its width is `(columns − 1) × spacing / 2`. A Move or attack-move order gives each ship a lane at its slot's place across the grid.
   - At each corner of the route the band lies along the corner's mitre. A lane is stretched so that it keeps its distance from the route on both legs, by at most 2 times at a sharp corner.
   - **A corner hugs the obstacle the route turns round**, so the band moves across by its half width to the side that is clear, and the innermost lane passes the route's corner. A band that fits on neither side stays centered.
   - Each ship's lane is line-tested. Where a lane's corner is blocked, the ship takes the point halfway back to the route's corner, and then the corner itself. A lane that cannot be made clear is dropped, and the ship takes the route's corners.
   - A route a ship searched for itself gets a band too, so the ships cut off with it keep lanes as well.
2. **A ship keeps its lane's corners.** It lets one go at sight of the next waypoint only once it is within three of its footprint radii of it, which is one place in the formation. It also lets a corner go within its own radius, and gives up a corner after a second without progress ([ADR-010](ADR-010-movement-and-pathing.md) decision 6).
   - Every other waypoint is let go at sight. That includes the group's destination, which a ship passes through when the route's end does not see its slot.
   - A ship knows its lane by the lane's last waypoint, kept with its path. The rule holds only while that waypoint is in the path, so a path that replaces it ends the rule.
3. **An Attack order keeps lanes too.** Its ships have no slots: each takes a lane at its place across a grid of the group's size, as a moving group's ships do, and the band leads to the target. When the target has moved 40 m from where they last pathed to, at most once a second, the ships of one player that path again to it in the same tick path again together, along a fresh route in a band, instead of each alone (`ChaseTargets`). A lone ship paths alone.
4. *(retired)*
5. *(retired)*
6. *(retired)*

## Consequences

- **Through a gap narrower than the band, the ships close up to pass it.**
- **An order tick costs more for a group.** Each ship's lane takes a line test for each corner it passes and one for the leg after it.
- **Tests:**
  - `PathfinderTests.ShipsOfAGroupKeepTheirLanesRoundAnObstacle`: three ships' lanes past the route's last corner are a lane apart, and the innermost is at the corner.
  - `MovementTests.AGroupPassesAnObstacleSideBySide`: 25 ships five to a row, ordered past a field 200 m in radius across their way, are never more than 2.0 times as long along their mean heading as they are wide across it.

## What this forecloses

- A group passing an obstacle in file through its route's corner, without a new decision, wherever its band fits.
- A ship cutting from its lane to its slot as soon as it sees the slot.

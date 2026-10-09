# ADR-047 — A group passes an obstacle side by side, health bars keep a least size and show on Alt, and no income is a warning

Status: **accepted** · 2026-10-03

## Context

On 2026-10-03 the owner shared a screenshot of a big fight between two AIs and asked for a review of it. The review found three faults, and the owner asked for all three to be fixed:

1. **Both fleets fought as single-file columns.** The Tarkan line was about 700 pixels long. Its head was being destroyed while its tail was still marching in.
2. **The health bars could not be read.** At that zoom they were a few pixels of green, so the player could not tell which ships were hurt.
3. **The Ore panel wrote "+0/s" in the figures' blue**, as calmly as any other income.

**The columns came from two rules of movement working together**, not from the AI. The AI sends its fleets as whole groups, 20 or more warships with one attack-move order.

- **A group shared one route.** Every ship whose way to its slot was blocked joined the group's route at a corner, so every ship of the group passed the obstacle through the same corner.
- **A ship let a corner go as soon as it saw the waypoint after it.** Past an obstacle, a ship saw its slot as soon as the straight line to the slot cleared the obstacle's edge, and then it headed straight for the slot. Every such line touches the obstacle at the same place, so even a ship given its own way round would pass the obstacle where every other ship did.
- An attack-moving ship stands to fire once an enemy is in range. The head of a column stopped, and the rest arrived one after another.

**Measured** in the Linux container with a probe of 25 Small+Ion ships in a 5 × 5 block, ordered 3 km past one field 200 m in radius. It measured the group's length along its mean heading against its width across it, worst while the group moved:

- Open space: 1.0.
- With the field across the way: 4.9.
- With the field 150 m off the line: 4.1.
- Giving each ship its own lane round the field changed this only to 3.9, because the second rule undid the lanes.
- Keeping the lanes' corners without the lanes changed nothing: 4.9.

## Decision

1. **A group keeps a band of lanes along its route.**
   - The band is as wide as the group's grid of slots: half its width is `(columns − 1) × spacing / 2`. A Move or attack-move order gives each ship a lane at its slot's place across the grid.
   - At each corner of the route the band lies along the corner's mitre. A lane is stretched so that it keeps its distance from the route on both legs, by at most 2 times at a sharp corner.
   - **A corner hugs the obstacle the route turns round**, so the band moves across by its half width to the side that is clear, and the innermost lane passes the route's corner. A band that fits on neither side stays centered.
   - Each ship's lane is line-tested. Where a lane's corner is blocked, the ship takes the point halfway back to the route's corner, and then the corner itself. A lane that cannot be made clear is dropped, and the ship takes the route's corners as before.
   - A route a ship searched for itself gets a band too, so the ships cut off with it keep lanes as well.
2. **A ship keeps its lane's corners.** It lets one go at sight of the next waypoint only once it is within three of its footprint radii of it, which is one place in the formation. It still lets a corner go within its own radius, and still gives up a corner after a second without progress.
   - Every other waypoint is let go at sight as before. That includes the group's destination, which a ship passes through when the route's end does not see its slot.
   - A ship knows its lane by the lane's last waypoint, kept with its path. The rule holds only while that waypoint is still in the path, so a path that replaces it ends the rule.
3. **An Attack order keeps lanes too** (Phase 2 plan task 14.4, carried over from Phase 1's task 7.3). Its ships have no slots: each takes a lane at its place across a grid of the group's size, as a moving group's ships do, and the band leads to the target. When the target has moved 40 m from where they last pathed to, at most once a second, the ships of one player that path again to it in the same tick path again together, along a fresh route in a band, instead of each alone (`ChaseTargets`). A lone ship paths as before.
4. **Health bars:**
   - **Held, Alt shows a bar over every ship and structure**, whole or not. Released, only damaged ones show, as before. A structure the player only remembers shows none ([ADR-081](ADR-081-territory-and-memory-over-the-fog.md)).
   - **A bar is never less than 32 × 5 of the HUD's reference units on screen**, measured as the camera's view width falls across the screen's middle. Below that it stays as long as its footprint and 2.5 m thick.
   - On a 1,920-pixel-wide screen, the least size first applies past a 960 m view. At the default 500 m view, nothing changes. At the widest 1,600 m view, a Small hull's bar goes from about 19 × 3 pixels to 32 × 5.
   - **The window ignores the keyboard's menu key.** Alt pressed and released alone would open the window's menu and hold the frame loop in the menu's own loop until it closed, so the window swallows `SC_KEYMENU`. Alt+F4 and Alt+Enter still work.
5. **An income of nothing is written in the HUD's warning color**, the salmon of a designer name the server would refuse. Any income above nothing stays in the figures' blue.
6. **The AI's regroup after a fall-back is [ADR-041](ADR-041-ai-plays-a-longer-match.md) decision 2's** (`regroupSeconds` in `Opponent.json`), 240 seconds since Phase 2's tuning with territory, which the owner kept on 2026-10-04. On Phase 1's map with lanes it was 120 seconds, not 150: two AIs' matches had grown longer than the owner's 45–60 minutes, and the owner chose 120 on 2026-10-03 from the settings measured below.

## Consequences

- **Measured in the Linux container with the probe above:**

  | Case | Before | After |
  |---|---|---|
  | Field across the way | 4.9 | 1.3 |
  | Field 150 m off the line | 4.1 | 1.2 |
  | Open space | 1.0 | 1.0 |
  | Two fields with a gap narrower than the band | 4.1 | 2.4 |
  | Field across the way, an Attack order on a target moving 5 m/s beyond it (task 14.4) | 6.2 | 2.5 |

  - Every case arrives in the same time as before, 38 to 40 seconds.
  - Through a gap narrower than the band, the ships still close up to pass it.
- **Order ticks cost more.** Each ship's lane takes a line test for each corner it passes and one for the leg after it.
  - Measured in the Linux container on ADR-032's measurement load, as ADR-032 measured it: each order tick's least time over seven runs, 24 order ticks over 2,400 ticks.
  - The worst order tick goes from 1.02 to 1.26 ms, and the mean from 0.65 to 0.85 ms.
  - The ships' paths are all of it: 0.50 to 0.74 ms in an order tick on average.
  - At ADR-032's ratio of about 3.3 between the container and the development machine, that is about 4.2 ms against Q4's 5 ms. The owner's `--measure --load` run is what records it.
- **AI matches**, measured in the Linux container as ADR-041 measured them: the median length of two AIs' matches, with a match not ended at 150 minutes counted as the longest.

  | Setting | Seeds 1–40 | Seeds 41–80 |
  |---|---|---|
  | Before lanes | 53.9, all ended | — |
  | Lanes, AI unchanged | 66.4, all ended | 60.5, 2 not ended |
  | Lanes, 120 s regroup | 47.5, 2 not ended | 51.1, all ended |

  - With the 120 s regroup, 11 of seeds 1–40 and 15 of seeds 41–80 end within 45–60 minutes. Each side wins about half: 20–18 and 18–22.
  - Before lanes, the same harness gave 53.9 on seeds 1–40, which is ADR-041's figure.
  - **Other settings were tried on seeds 1–40 with lanes and dropped:**
    - Attack groups of 16: 50.0, 2 not ended, and one side won 28 of 38.
    - Attack groups of 18: 42.6, and one side won 31 of 40.
    - Falling back at 40% losses: 49.6, 5 not ended.
    - One platform per Shipyard: 60.7, and one side won 30 of 40.
    - One platform per Shipyard with groups of 18: 41.8.
    - One platform per Shipyard with a 135 s regroup: 41.0.
  - **The medians move by up to ten minutes between neighboring settings.** Match lengths spread from about 30 to over 100 minutes, so a median of 40 seeds is a rough measure.
  - **A match that does not end is the stall ADR-041 found** in 3 of seeds 41–80 and did not look into. One AI holds about 200 warships against about 50 for the last hour and never finishes the match.
- **Tests:**
  - `PathfinderTests.ShipsOfAGroupKeepTheirLanesRoundAnObstacle`: three ships' lanes past the route's last corner are a lane apart, and the innermost is at the corner.
  - `MovementTests.AGroupPassesAnObstacleSideBySide`: the probe's case with the field across the way, at most 2.0. It is 1.3 with this decision and 4.9 without it.
  - `HudTests.WarnsOfNoIncome`.
- **Not built or run in the container:** `GameClient` and `Window` need D3D12 and Win32, so CI is their first build. The health bars, the Alt key and the window's menu key are the owner's run.

## What this forecloses

- A group passing an obstacle in file through its route's corner, without a new decision, wherever its band fits.
- A ship cutting from its lane to its slot as soon as it sees the slot.
- A health bar smaller on screen than 32 × 5 reference units.
- Alt opening the window's menu.

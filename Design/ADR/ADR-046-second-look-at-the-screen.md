# ADR-046 — After the owner's second look: rings that fade with zoom, white crosses, a clearer HUD, and rigs only on explored ore

Status: **accepted** · 2026-10-03

## Context

On 2026-10-03, after task 9.5 merged, the owner shared a screenshot of a base and asked for a review of the scene. The review found these faults:

1. **A Mining Rig's ring, laid over its rock (ADR-042), read as one more of the rock's lines.**
2. **At the camera's usual zoom the resting rings told the player nothing the models did not.** A Shipyard is long, so its ring is a wide ellipse, which ran under the units beside it and behind the selection panel.
3. **The brightest stars' crosses took the stars' warm tints.** An orange cross near a rig read as a marker in the Ore's gold.
4. **The Ore panel's figure ended at a fixed edge**, which left a dead gap at the panel's left.
5. **On the minimap the fields' gray squares outweighed the ore's small gold marks.**
6. **The selection panel wrote "2 x Constructor" and gave hit points only as figures.**
7. **The Research Lab's button was dim with no reason given.**

The owner asked for all seven to be done. The owner also found two faults while playing:

- **An asteroid in space never explored could be claimed with a Mining Rig.**
- **`--stress` stood Mining Rigs on open ground.**

## Decision

1. **A Mining Rig's ring shows only under the pointer and while a structure is placed.** It is at full strength then, laid over its rock as ADR-042 has it. At rest the rig's own color says whose it is.
2. **A resting ring is for the far view.** Its strength falls as the camera comes in:
   - It is at full strength while its radius is at most 3% of the view's width, so its full strength is ADR-042's 0.35.
   - It is gone from 6.5% of the view's width.
   - At the default 500 m view, the Command Station's, a Shipyard's and the Research Lab's rings are gone, and a Defence Platform's is at a third.
   - At the widest 1,600 m view, every ring is at 80% or more.
   - Under the pointer, and while a structure is placed, a ring is at full strength at any zoom.
3. **A cross is white, or a hot star's blue-white, and never warm.** The points keep every tint.
4. **The Ore panel writes the stockpile from its left.** Ore's gem stays put, and the figure grows to its right. The income keeps to the panel's right edge.
5. **On the minimap:**
   - An asteroid field is near the map's own darkness: (0.06, 0.06, 0.065), down from (0.13, 0.13, 0.14).
   - An ore asteroid's mark is at least 8 units across, the largest of the marks' smallest sizes.
   - The neutral marks are drawn first, so that a rig's mark shows over its asteroid's.
6. **The selection panel writes a group of one design once, as its title, "2 × Constructor"**, and a group of several designs as its count over a line for each, "3 × Swarm". It shows its hit points in figures, and as bars under its lines: one for each ship of a group of up to 24, or else one for the whole selection ([ADR-088](ADR-088-selection-and-commands.md) decision 4). A bar is green above half, amber above a quarter, and red below, as the bar over a damaged ship is.
7. **A button that cannot be pressed for a reason other than its cost says why, in place of the cost.** The Research Lab's says "ONE PER PLAYER" once the player has one. A button the player only cannot afford is still dim with its cost, as the windows' cards are. The reason is on the button rather than on hover, so it shows without the pointer.
8. **A Mining Rig is ordered only onto an asteroid the player has seen.**
   - `FogOfWar::HasSeen` says whether any cell of the asteroid's circle was ever in sight; without fog of war, all of the map was.
   - The client gives the controls and the rig's ghost the view less the asteroids never seen. The ghost is then red there, and a click orders nothing and leaves the placement armed.
   - The server still accepts such an order. Making it a rule of the server would also bind the AI, which follows the ore without exploring first (task 11.3).
9. **`--stress` stands its Mining Rigs on the free ore asteroids nearest each player's structures**, as a rig built in a match stands. A map with too few asteroids for them stops the run with a message.

## Consequences

- **Tests:**
  - `HudTests`:
    - `KeepsTheOreGemStillAsTheFigureChanges`
    - `DrawsTheSelectionsHealthAsABar`
    - `DrawsARigsMarkOverItsAsteroid`
    - In `DescribesOneShip`, `DescribesAGroupByDesign` and `OffersTheStructuresToConstructors`: the × sign, the health share and the button's note. ADR-088 decision 4's tests have the title of one design and the ships' own bars.
  - `FogOfWarTests.SaysWhetherAnyOfACircleWasSeen`.
  - `PlayerControlsTests.OrdersAMiningRigOnlyByAKnownAsteroid`.
  - `StarfieldTests.DrawsNoBurstWarm`.
  - `StressLoadTests` checks that every Mining Rig in the scene stands on an asteroid.
- **The ring strengths are set from the view's width, not measured on screen.** The figures in decision 2 come from the structures' footprints and the camera's limits in `Camera.json`. Whether the rings are where the owner wants them at each zoom is the owner's run.
- **Not built or run in the container:** `GameClient`, `PlayerControls`, `Starfield` and their tests need DirectXMath or D3D12. The HUD's, the fog's and the stress load's tests ran there, and the HUD was drawn from `Hud::Lay`'s output with substitute fonts.

## What this forecloses

- A resting ring at full strength at close zoom, and a resting ring under a rig, without a new decision.
- A warm cross.
- A rig claimed in space the player has never seen, through the game's own interface.

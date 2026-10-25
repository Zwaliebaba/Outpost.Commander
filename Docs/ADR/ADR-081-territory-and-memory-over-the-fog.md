# ADR-081 — The world shows the territory and what the player only remembers, over the fog, and its fog is darker than the minimap's

Status: **accepted** · 2026-10-09
Owns: territory-marks, last-seen-tick, claimable-node-rule, memory-drawing, ground-fog-knee, minimap-territory

## Context

A match is decided by territory, by what a player remembers and by what it sees, and the main view must show each:

- **Territory.** A sector's border, its node and its state, held, guarded, suppressed or cut off, decide where a player builds and fights, and every player sees them, fog or not (ADR-056 decision 10), so the world shows them as well as the minimap.
- **Memory.** A remembered structure (ADR-024) is a picture of the past. The fog's darkening alone does not mark it, since it darkens the empty space around it as much, and the player needs to tell a memory from what it sees, and how old it is.
- **Sight.** The ground's fog is black over a scene that is mostly black, so the edge of sight has to show by itself.

The owner chose on 2026-10-08 to show the territory and the edge of sight in the world and to have the server keep a memory's age, and on 2026-10-09 to mark the nodes a player could claim only while it places a Relay.

## Decision

1. **The server keeps when a remembered structure was last seen.**
   - `EntityView::lastSeenTick` is the tick of the last snapshot the player saw the entity in. `Simulation::UpdateVision` sets it on each structure and derelict it records as seen: the next tick's at the end of a tick, and the current one when fog is first turned on. A remembered view keeps it while it is out of sight. An entity the player sees now carries zero.
   - It is part of what `Simulation` holds, so it is saved, survives a seat taken again ([ADR-078](ADR-078-dedicated-server.md) decision 4) and replays, and it is part of the save's layout (AGENTS.md R18). It is on the wire too.
2. **One rule says whether the player could claim a node now.**
   - `CanClaim` in `GameProtocol` holds when the node is free and nothing stands on it as a Relay's footprint, a sector next to it is the player's, no pirate guards it, and the player is under its node cap.
   - The Relay's ghost (`PlaceGhost`), the world's rings on the nodes the player could claim and the minimap's outlines of their sectors all use it, so they cannot disagree.
3. **The world draws the territory over the fog,** since every player sees it, fog or not (ADR-056 decision 10). `MarkTerritory` in `GameApp` works it out from the snapshot, once a snapshot, and is tested without a GPU.
   - **The lattice:** every side of every sector, each once, as one-pixel lines in (0.04, 0.043, 0.063) linear. That is brighter than the grid, and never wider.
   - **The holders' outlines:** inside each held or guarded sector, 4 m in from its border, an outline in its holder's color at half its strength. Where two holders' sectors meet, both outlines show, one either side of the lattice's line.
   - **Patterns, not colors, for a sector's state:** a suppressed sector's outline is dashed, and a cut-off one's dotted. A sector both suppressed and cut off is dashed, since suppression earns nothing. A pirate-guarded sector is outlined in the pirates' color.
   - **Rings on the nodes the player could claim now, and only while it places a Relay.** Each has two rings in the player's own lines' color, the outer the size of a Relay's footprint ring and the inner three quarters of it, so that the Relay's ghost stands in them. No other node has a mark. The map's nodes are at their sectors' centers (`Tools/MakeMap.py`), so a ring on every node would say only what the borders say (owner, 2026-10-09). A held node has its Relay's or Command Station's own footprint ring.
   - **One mesh per side.** Each side is one instance of a line mesh 1 m long, solid, dashed or dotted, scaled to the side's length and laid along it. A 2,000 m side so has 40 dashes of 25 m, or 100 dots of 4 m. The meshes are made at startup, in its batch of uploads ([ADR-048](ADR-048-batched-static-uploads.md)), so nothing is uploaded mid-match, and the lines take none of the frame's vertices that explosions' shards use.
4. **What the player only remembers is drawn as a memory.**
   - In the world, a remembered structure or derelict is drawn over the fog as its lines alone. It has no faces, no health or build bar, and no footprint ring, in its side's color halfway to gray at 0.6.
   - Under the pointer, it says what it is and how long ago it was seen: "Shipyard · last seen 4:12 ago, 34% built". A remembered derelict's hint ends with its age too. Without the server's tick rate, the age reads "last seen earlier".
   - `Hud::Describe` lays out a remembered structure's selection panel with "Last seen 4:12 ago" first, and what follows as it was then: "34% built when seen", "Hit points 1,200 / 3,000 when seen". A click on another player's structure, seen or remembered, selects it alone and opens this panel ([ADR-013](ADR-013-client-view-and-controls.md) decision 5; owner, 2026-10-25), which offers no button.
   - On the minimap, a memory is a cross in its side's color, 8 units across, drawn over the fog. What is seen stays a filled square under the fog, and an ore asteroid an outline.
5. **The mesh pipeline can be bound again within a frame.** `MeshPipeline::Resume` binds it after another pipeline drew, such as the ground mask, without resetting the frame's instances and vertices as `BeginDrawing` would. The territory and the memories are drawn after it, once the fog is down.
6. **The ground's fog is darker than the minimap's.**
   - `GroundMaskPipeline::FrameConstants` has a knee. A shade up to `kneeShade` covers in proportion, up to `kneeOpacity`; a darker one covers as much as its shade, and never less than `kneeOpacity`. It rises with the shade, with no step, so the blend between cells stays smooth. A knee of zero leaves every shade as it is.
   - `GameClient` sets the knee at `FogOfWar::SEEN_BEFORE_SHADE`, 0.55, to `GROUND_SEEN_BEFORE_SHADE`, 0.8. On the ground, what was seen before is so at 0.8, and what was never seen keeps its 0.9.
   - The minimap samples the same texture with no knee ([ADR-052](ADR-052-fog-texture.md) decision 4). It keeps 0.55, where telling explored from never seen matters for scouting.
7. **The minimap draws the territory:**
   - each held or guarded sector washed faintly in its holder's color under the marks;
   - over its fog, every sector's border in (0.11, 0.13, 0.17), at 2:1 against the map;
   - the held and guarded sectors' outlines over it, a cut-off one's in dashes of 6 units with gaps of 4, and a suppressed sector hatched over its whole square in its holder's color;
   - no mark on a node, for the reason decision 3 gives;
   - while the player places a Relay, each sector whose node it could claim now outlined 3 units inside its border in the player's blue, clear of the outline of its held sector beside it.

## Consequences

- **Tests:**
  - `PlacementTests.ClaimsANodeByTheGhostsRule`: `CanClaim` and the Relay's ghost agree at every node, for two players and three caps, with the node clear, covered by a derelict, or guarded.
  - `FogTests.RemembersWhenAStructureWasLastSeen`: the tick is the last snapshot that showed the structure, and it stands while the structure stays out of sight. `WireFormatTests` carries the field. `WorldStateTests.TheLayoutIsTheVersions` holds the layout's hash.
  - `TerritoryMarksTests`: each border drawn once; the outlines inset; the patterns; the claimable nodes against the ghost, and none while a Relay cannot be built; nothing without sectors.
  - `HudTests`:
    - `DescribesAMemoryWithItsAge`, with another player's finished Shipyard, seen or remembered, offering no button;
    - `DescribesADerelict`, with its age;
    - `DrawsAMemoryAsACrossOverTheFog`;
    - `DrawsTheTerritoryOnTheMinimap`, with the border's contrast, no mark on a node, and the claimable sector's outline only while a Relay is placed.
  - `FogOfWarTests.DarkensWhatWasSeenBeforeOnTheGround`.
  - `PlayerControlsTests.SelectsAnotherPlayersStructureForItsPanel`: a click selects it alone, Shift or not, seen or remembered; it gives no order, joins no control group, and leaves the selection once forgotten.
- **Every line and ring is an instance** of the frame's 16,384 (`MeshPipeline::MAX_FRAME_INSTANCES`). On the repository's map ([Catalogue: sectors](../Reference/Catalogue.md#sectors)) that is at most 60 lattice sides, 100 outline sides and 50 rings.

## What this forecloses

- A node marked claimable by any other rule than the Relay's ghost's.
- A remembered structure drawn with faces or bars, without a new decision.
- A sector's state shown by its outline's color alone.
- A mark on every node, without a new decision.

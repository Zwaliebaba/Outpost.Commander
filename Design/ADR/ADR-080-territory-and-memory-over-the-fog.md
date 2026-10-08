# ADR-080 — The world shows the territory and what the player only remembers, over the fog, and its fog is darker than the minimap's

Status: **accepted** · 2026-10-08

## Context

The second interface review of 2026-10-08 ([interface plan 2](../../GameDesign/ImplementationPlan-Interface2.md)) found that the main view did not show what a match is decided by:

- **Territory.** `GameClient` drew no sector border and no node. The minimap washed and outlined only the held and pirate-guarded sectors, and "cut off" was a line on a selected Relay's or rig's panel. A player found a node by arming a Relay and sweeping the cursor.
- **Memory.** A remembered structure (ADR-024) was drawn exactly as a seen one, with its last-seen bars. Its only mark was the fog's darkening, which darkens the empty space around it as much. Nothing said how old the memory was.
- **Sight.** The ground mask is black over a scene that is mostly black. Measured on the review's screenshots as the Rec. 709 luma of the sRGB pixels, a grid line peaked at 27 of 255 in sight and at 15 where the player had seen before.

The owner accepted the plan's gates on 2026-10-08, V1 to V8 as proposed. Milestone UI1 is this ADR: tasks UI1.1 to UI1.4 under gates V2 and V3.

## Decision

1. **The server keeps when a remembered structure was last seen.**
   - `EntityView::lastSeenTick` is the tick of the last snapshot the player saw the entity in. `Simulation::UpdateVision` sets it on each structure and derelict it records as seen: the next tick's at the end of a tick, and the current one when fog is first turned on. A remembered view keeps it while it is out of sight. An entity the player sees now carries zero.
   - It is part of what `Simulation` holds, so it is saved, survives a seat taken again (Phase 5) and replays. `WORLD_STATE_VERSION` goes from 2 to 3, with version 3's layout hash recorded beside the others (AGENTS.md R18). It is on the wire, so `PROTOCOL_VERSION` goes from 12 to 13.
2. **One rule says whether the player could claim a node now.**
   - `CanClaim` in `GameProtocol` holds when the node is free and nothing stands on it as a Relay's footprint, a sector next to it is the player's, no pirate guards it, and the player is under its node cap.
   - The Relay's ghost (`PlaceGhost`), the world's node rings and the minimap's node marks all use it, so they cannot disagree.
3. **The world draws the territory over the fog,** since every player sees it, fog or not (ADR-056 decision 10). `MarkTerritory` in `GameApp` works it out from the snapshot, once a snapshot, and is tested without a GPU.
   - **The lattice:** every side of every sector, each once, as one-pixel lines in (0.04, 0.043, 0.063) linear. That is brighter than the grid, and never wider, which is the way back [ADR-028](ADR-028-vector-grid-and-crosses.md) left for marking scale.
   - **The holders' outlines:** inside each held or guarded sector, 4 m in from its border, an outline in its holder's color at half its strength. Where two holders' sectors meet, both outlines show, one either side of the lattice's line.
   - **Patterns, not colors, for a sector's state:** a suppressed sector's outline is dashed, and a cut-off one's dotted. A sector both suppressed and cut off is dashed, since suppression earns nothing. A pirate-guarded sector is outlined in the pirates' color.
   - **A ring on every node,** the size of a Relay's footprint ring: in its holder's color, or the pirates' while they guard it, or the lattice's color brightened while it is free. Where the player could claim it now, there are two rings in its own lines' color.
   - **One mesh per side.** Each side is one instance of a line mesh 1 m long, solid, dashed or dotted, scaled to the side's length and laid along it. A 2,000 m side so has 40 dashes of 25 m, or 100 dots of 4 m. The meshes are made at startup, in its batch of uploads ([ADR-048](ADR-048-batched-static-uploads.md)), so nothing is uploaded mid-match, and the lines take none of the frame's vertices that explosions' shards use.
4. **What the player only remembers is drawn as a memory.**
   - In the world, a remembered structure or derelict is drawn over the fog as its lines alone. It has no faces, no health or build bar, and no footprint ring, in its side's color halfway to gray at 0.6.
   - Under the pointer, it says what it is and how long ago it was seen: "Shipyard · last seen 4:12 ago, 34% built". Its selection panel opens with "Last seen 4:12 ago", and what follows is as it was then: "34% built when seen", "Hit points 1,200 / 3,000 when seen". A remembered derelict's hint ends with its age too. Without the server's tick rate, the age reads "last seen earlier".
   - On the minimap, a memory is a cross in its side's color, 8 units across, drawn over the fog. What is seen stays a filled square under the fog, and an ore asteroid an outline.
5. **The mesh pipeline can be bound again within a frame.** `MeshPipeline::Resume` binds it after another pipeline drew, such as the ground mask, without resetting the frame's instances and vertices as `BeginDrawing` would. The territory and the memories are drawn after it, once the fog is down.
6. **The ground's fog is darker than the minimap's.**
   - `GroundMaskPipeline::FrameConstants` gains a knee. A shade up to `kneeShade` covers in proportion, up to `kneeOpacity`; a darker one covers as much as its shade, and never less than `kneeOpacity`. It rises with the shade, with no step, so the blend between cells stays smooth. A knee of zero leaves every shade as it is.
   - `GameClient` sets the knee at `FogOfWar::SEEN_BEFORE_SHADE`, 0.55, to `GROUND_SEEN_BEFORE_SHADE`, 0.8. On the ground, what was seen before is so at 0.8, and what was never seen keeps its 0.9.
   - Applied in linear light to the measured 27 of 255, 0.8 gives about 7, computed rather than measured: the edge of sight is where the grid and the stars go out.
   - The minimap samples the same texture with no knee ([ADR-052](ADR-052-fog-texture.md) decision 4). It keeps 0.55, where telling explored from never seen matters for scouting.
7. **The minimap draws the territory over its fog** (task UI1.4):
   - every sector's border in (0.11, 0.13, 0.17), at 2:1 against the map;
   - the held and guarded sectors' outlines over it, a cut-off one's in dashes of 6 units with gaps of 4, and a suppressed one's hatching as before;
   - a dot of 4 units on every node, in its holder's color or a free node's gray;
   - round a node the player could claim now, a 12-unit outline in the player's blue, a dot in a square where an ore asteroid's outline is empty.

## Consequences

- **Tests:**
  - `PlacementTests.ClaimsANodeByTheGhostsRule`: `CanClaim` and the Relay's ghost agree at every node, for two players and three caps, with the node clear, covered by a derelict, or guarded.
  - `FogTests.RemembersWhenAStructureWasLastSeen`: the tick is the last snapshot that showed the structure, and it stands while the structure stays out of sight. `WireFormatTests` carries the field. `WorldStateTests.TheLayoutIsTheVersions` holds version 3's hash.
  - `TerritoryMarksTests`: each border drawn once; the outlines inset; the patterns; the node marks against the ghost; nothing without sectors.
  - `HudTests`:
    - `DescribesAMemoryWithItsAge`;
    - `DescribesADerelict`, with its age;
    - `DrawsAMemoryAsACrossOverTheFog`;
    - `DrawsTheTerritoryOnTheMinimap`, with the border's contrast.
  - `FogOfWarTests.DarkensWhatWasSeenBeforeOnTheGround`.
- **Run in the Linux container** against a stand-in for the Windows headers and the test framework, with substitute fonts: GameProtocolTests 17, GameLogicTests 292 (all but `QuicTransportTests`), OpponentTests 31, and the GameAppTests of the HUD, the fog, the territory, the alerts, the designer, the production target and the window manager, 110.
- **Built only in CI:** `GameClient`, `MeshPipeline`, the ground mask's shaders and the pipeline's constants need D3D12 and fxc, so CI's Debug|x64 build is their only one. Every suite above also passes there. The look is the owner's run: the lattice's and outlines' strengths, the 4 m inset, the dashes and dots, the memory's shade and the ground's 0.8.
- **Every line and ring is an instance** of the frame's 16,384 (`MeshPipeline::MAX_FRAME_INSTANCES`). On the 10 km map's 25 sectors that is at most 60 lattice sides, 100 outline sides and 50 rings.

## What this forecloses

- A node marked claimable by any other rule than the Relay's ghost's.
- A remembered structure drawn with faces or bars, without a new decision.
- A sector's state shown by its outline's color alone.

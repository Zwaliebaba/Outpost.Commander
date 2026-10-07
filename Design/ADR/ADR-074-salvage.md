# ADR-074 — Derelicts lie where the seed places them or an outpost falls, and a Constructor salvages them for Ore and research

Status: **accepted** · 2026-10-07

## Context

Phase 4 design §9 (gate L8) adds derelicts, wrecks of the old fleet.

- A Constructor salvages one with a new work order: 30 s of one Constructor's work, more Constructors helping as on a site. When the work is done the derelict is gone and its owner is paid. Warships cannot salvage.
- It pays 300 Ore near the homes and up to 900 in the far sectors. An outpost's derelict pays 600 for a camp and 1,200 for a stronghold (§8).
- About one in three also names a research topic, and recovers half of that topic's research time for the salvaging player. Half is taken off a topic under way, and a topic not yet started takes half its time when it starts. Its Ore cost is unchanged. A topic of a tier not yet open is recovered all the same and waits for the tier.
- What a derelict holds is seen once it is within a player's sight. Under fog of war it is remembered as a structure is.
- It is drawn with a hull mesh in grey, and blocks no path.
- §7 proposes derelicts in roughly every other sector, richer the further out.

The owner decided on 2026-10-07:

- **Where and how many.** On each side, the seed picks one of the two flank sectors at 300 Ore, two of the three near sectors at 450, two of the four contested at 600, and the between sector at 750. That is six a side, mirrored. The rich corners and the center get none, since their strongholds leave a wreck.
- **Research.** A third of the pairs name a topic, drawn from the whole research table.
- **Outpost wrecks.** A cleared outpost's wreck blocks its node until it is salvaged.
- **AI.** The AI leaves derelicts alone until milestone 33.

What the design leaves open: how the map and the tuning data say all this, what a derelict is in the simulation, how its work and its research are counted, and how the client shows it.

## Decision

1. **A derelict is an entity of a new kind, `EntityKind::Derelict`, with no owner and no hit points.**
   - It keeps the Ore it pays, the topic it names (or none), and its salvage, in thousandths of a tick of one Constructor's work.
   - It is not an obstacle, so no path goes round it. No structure stands on one, though: the server refuses the placement, and the ghost shows red.
   - Its hull and radius say how it is drawn. It lies facing away from the map's center, so that it and its mirror lie alike.
2. **The map places them by sector kind** ([ADR-072](ADR-072-seeded-placement.md), which owns the detail).
   - `Map.json`'s `"derelicts"` give, for a sector kind, a count of mirrored pairs, the Ore each pays, a hull and a radius. `"derelictResearchPercent"` gives the share of pairs that name a topic: 33, so two of the six.
   - The seed draws them after the asteroids and outposts. Each sector gets at most one derelict, placed as an asteroid is: inside its sector by the border and its radius, clear of the node, and the minimum gap from every obstacle, start and other derelict.
   - The pairs that name a topic, and their topics, are drawn last. A topic comes from the tuning data's table, which `InProcessServer` hands to `PlaceContent`.
3. **The tuning data says how salvage goes.** `Tuning.json`'s optional `"salvage"` holds `workSeconds` 30 and `recoveryPercent` 50, and each outpost has a `"wreck"`: 600 Ore for a camp and 1,200 for a stronghold, both drawn as a Large hull. Without salvage, no derelict is placed and no wreck is left.
4. **`SalvageCommand` orders Constructors to salvage a derelict their player sees or remembers.**
   - It is refused as `NotAConstructor` for a warship, as `NotSalvageable` for anything but a derelict, and as `NotVisible` for one the player has neither seen nor remembers.
   - The Constructors work as on a site ([ADR-016](ADR-016-base-building.md) decision 5): a tick of work each tick for the first, and the tuning data's share of one more for each after it, at their own player's Rapid Construction rate.
   - Two players' crews on one derelict work on the same salvage. The crew that finishes it is paid, and any other crew on it that tick finds it gone.
   - When it is done, the derelict leaves, every work order on it ends, and its player is paid.
   - The protocol carries the order, the derelict's kind, its Ore, topic and progress on `EntityView`, and `ResearchTopicView::recovered`. `PROTOCOL_VERSION` goes from 8 to 9.
5. **A named topic recovers `recoveryPercent` of its research time.**
   - If a Lab of the player's is researching it, that much comes off at once, and the topic may finish on the next tick.
   - Otherwise the player keeps it as recovered, whatever its tier, and the topic starts that far along. It still costs its full Ore.
   - A topic researched already gains nothing, and a second recovery of a topic still waiting adds nothing.
   - The snapshot marks a recovered topic, and its card in the research window says SALVAGED.
6. **A cleared outpost leaves its wreck** ([ADR-073](ADR-073-pirates.md) decision 11).
   - It is left where its last Defence Platform fell, its platforms being all one size, the first in identifier order if the last fall together.
   - It pays the outpost's wreck's Ore and names no topic.
   - On a camp, whose platform stands on the node, the wreck covers the node, so a Relay waits until it is salvaged.
7. **Fog of war treats a derelict as an enemy's structure** ([ADR-024](ADR-024-fog-of-war.md)): seen within sight, remembered out of it with what it holds, and forgotten once its place is seen without it.
8. **A measurement or stress run places none**, as it places no pirates.
9. **The AI leaves derelicts alone until milestone 33.** It plans no Relay on a node a wreck covers, and otherwise salvages nothing. A derelict blocks its structures as it blocks the player's ghost.
10. **The client** ([ADR-011](ADR-011-meshes-and-shading.md)):
    - draws a derelict with its hull's model across its radius, from a `"Wreck"` set that borrows the Human meshes in grey;
    - marks it light grey on the minimap;
    - shows, with the pointer over one, what it holds: its Ore, its topic, how far its salvage has come, and whether it is remembered;
    - salvages it when selected Constructors are right-clicked on it, the warships with them going there.

## Consequences

- **Salvage works as §9 asks** (`SalvageTests`): 30 s for one Constructor and two thirds of that for two; the Ore paid; half of a topic's time recovered whether it is under way or not yet started; nothing gained for a topic researched already; the derelict seen and remembered under fog with what it holds; and a cleared stronghold leaving a 1,200 Ore wreck where a platform stood.
- **A camp is cleared in three steps:** fight it, salvage its wreck, then build the Relay (`PirateTests.ARelayWaitsForTheOutpostToFall`).
- **AI against AI over seeds 1–40, measured in the Linux container** (design §2):
  - The AI salvages nothing, so the economies are as they were.
  - 37 matches end, as after milestone 30, and the same three stay level.
  - The median match ends with 14 derelicts left: the 12 placed, and wrecks from the outposts the fleets cleared.
  - The 99th percentile tick at the median is 1.9 ms, against 1.7 ms after milestone 30, with the derelicts in every player's vision pass. The run was repeated on an idle machine after a first one shared it with a build.
- **Placement is unchanged for asteroids and outposts.** The derelicts are drawn after them, so a seed places the same asteroids and outposts with or without derelicts (`MapTests.PlacesDerelictsFromTheSeed`).

## What this forecloses

- Salvage by warships.
- A derelict that blocks movement, or that combat can touch.
- More than one derelict in a sector from the seed. An outpost's wreck can share a sector with a seeded derelict.

# ADR-074 — Derelicts lie where the seed places them or an outpost falls, and a Constructor salvages them for Ore and research

Status: **accepted** · 2026-10-07
Owns: derelicts, salvage-order, salvage-research-recovery, outpost-wreck

## Context

The map holds derelicts, wrecks of the old fleet.

- A Constructor salvages one with a work order of its own: `workSeconds` of one Constructor's work, more Constructors helping as on a site. When the work is done the derelict is gone and its owner is paid. Warships cannot salvage.
- It pays Ore. A cleared outpost leaves a wreck, which pays more for a stronghold than for a camp.
- A share of them also names a research topic, and recovers `recoveryPercent` of that topic's research time for the salvaging player. That much is taken off a topic under way, and a topic not yet started takes it off when it starts. Its Ore cost is unchanged. A topic of a tier not yet open is recovered all the same and waits for the tier.
- What a derelict holds is seen once it is within a player's sight. Under fog of war it is remembered as a structure is.
- It is drawn with a hull mesh in gray, and blocks no path.

The owner decided on 2026-10-07:

- **Where and how many.** On each side, the seed places derelicts in the flank, near, contested and between sectors, mirrored, each kind with the count and the Ore that `Map.json`'s `"derelicts"` give it. The rich corners and the center get none, since their strongholds leave a wreck.
- **Research.** A share of the pairs, `derelictResearchPercent`, name a topic, drawn from the whole research table.
- **Outpost wrecks.** A cleared outpost's wreck blocks its node until it is salvaged.
- **AI.** The AI salvages them ([ADR-076](ADR-076-ai-plays-phase-4.md) decision 5).

The map and the tuning data must therefore say where derelicts lie and what they pay, the simulation must hold a derelict and count its work and its research, and the client must show it.

## Decision

1. **A derelict is an entity of a new kind, `EntityKind::Derelict`, with no owner and no hit points.**
   - It keeps the Ore it pays, the topic it names (or none), and its salvage, in thousandths of a tick of one Constructor's work.
   - It is not an obstacle, so no path goes round it. No structure stands on one, though: the server refuses the placement, and the ghost shows red.
   - Its hull and radius say how it is drawn. It lies facing away from the map's center, so that it and its mirror lie alike.
2. *(retired)*
3. **The tuning data says how salvage goes.** `Tuning.json`'s optional `"salvage"` holds `workSeconds` and `recoveryPercent`, and each pirate outpost has a `"wreck"` with its `ore`, `hull` and `radiusMeters`. Without salvage, no derelict is placed and no wreck is left.
4. **`SalvageCommand` orders Constructors to salvage a derelict their player sees or remembers.**
   - It is refused as `NotAConstructor` for a warship, as `NotYetSupported` without the tuning data's salvage, as `UnknownTarget` for a derelict that is gone, as `NotSalvageable` for anything but a derelict, and as `NotVisible` for one the player has neither seen nor remembers.
   - The Constructors work as on a site ([ADR-016](ADR-016-base-building.md) decision 5): a tick of work each tick for the first, and the tuning data's share of one more for each after it, at their own player's Constructor rate ([ADR-033](ADR-033-research-tiers.md) decision 4).
   - Two players' crews on one derelict work on the same salvage. The crew that finishes it is paid, and any other crew on it that tick finds it gone.
   - When it is done, the derelict leaves, every work order on it ends, and its player is paid.
   - The protocol carries the order, the derelict's kind, its Ore, topic and progress on `EntityView`, and `ResearchTopicView::recovered`.
5. **A named topic recovers `recoveryPercent` of its research time.**
   - If a Lab of the player's is researching it, that much comes off at once, and the topic may finish on the next tick.
   - Otherwise the player keeps it as recovered, whatever its tier, and the topic starts that far along. It still costs its full Ore.
   - A topic researched already gains nothing, and a second recovery of a topic still waiting adds nothing.
   - The snapshot marks a recovered topic, and its card in the research window says SALVAGED.
6. **A cleared outpost leaves its wreck, and nothing else destroyed leaves one.**
   - It is left where its last Defence Platform fell, its platforms being all one size, the first in identifier order if the last fall together.
   - It pays the outpost's wreck's Ore and names no topic.
   - On a camp, whose platform stands on the node, the wreck covers the node, so a Relay waits until it is salvaged (owner, 2026-10-07).
7. **Fog of war treats a derelict as an enemy's structure** ([ADR-024](ADR-024-fog-of-war.md)): seen within sight, remembered out of it with what it holds, and forgotten once its place is seen without it.
8. **Derelicts are placed with the outposts** ([ADR-073](ADR-073-pirates.md) decision 8): once, when the server is made, and none for the measurement load, the stress scene or a battle matchup.
9. *(retired)*
10. **The client** ([ADR-011](ADR-011-meshes-and-shading.md)):
    - draws a derelict with its hull's model across its radius, from a `"Wreck"` set that borrows the Human meshes in gray;
    - marks it light gray on the minimap;
    - shows, with the pointer over one, what it holds: its Ore, its topic, how far its salvage has come, and how long ago the player last saw it when it only remembers it ([ADR-081](ADR-081-territory-and-memory-over-the-fog.md) decision 4);
    - salvages it when selected Constructors are right-clicked on it, the warships with them going there.

## Consequences

- **Salvage works as decisions 3 to 6 have it** (`SalvageTests`): one Constructor takes `workSeconds`, and a second adds the constructor's `extraConstructorBuildShare` of one more; the Ore is paid; `recoveryPercent` of a topic's time is recovered whether it is under way or not yet started; a topic researched already gains nothing; the derelict is seen and remembered under fog with what it holds; and a cleared stronghold leaves its wreck, with the wreck's Ore, where a platform stood.
- **A camp is cleared in three steps:** fight it, salvage its wreck, then build the Relay (`PirateTests.ARelayWaitsForTheOutpostToFall`).
- **Derelicts are drawn after asteroids and outposts, so they change neither** ([ADR-072](ADR-072-seeded-placement.md) decision 8): a seed places the same asteroids and outposts with or without derelicts (`MapTests.PlacesDerelictsFromTheSeed`).

## What this forecloses

- Salvage by warships.
- A derelict that blocks movement, or that combat can touch.
- More than one derelict in a sector from the seed. An outpost's wreck can share a sector with a seeded derelict.

# ADR-037 — A player loses with its production, its last Shipyards are revealed, and the AI attacks production first

Status: **accepted** · 2026-10-03 · supersedes ADR-020's decision 9, the match's end, and the attack in ADR-024's decision 8

## Context

In the MVP a match ends when a Command Station falls (ADR-020 decision 9), so one raid that reaches the station settles it. Phase 1 design §4 changes the win condition so that a match lasts (owner, 2026-10-02, gate H5): a player loses when it has neither a Command Station nor a finished Shipyard; a lost Command Station is lost for good; and a player that has lost it has its remaining Shipyards shown to its opponent through fog of war, as remembered structures (ADR-024), so that the end of a match on the 5 km map is not a search for one building. Design §13 has the AI attack production first: Shipyards, then the Command Station, then anything else. Under ADR-024 decision 8 its attack group attack-moves on the nearest enemy structure it sees or remembers.

## Decision

1. **A player whose base was placed loses when it has neither a Command Station nor a finished Shipyard.** The server checks it every tick, as it checked the station, and the match ends once: the winner is the player still standing, or nobody when both fall in the same tick, and the world runs on (ADR-020). A Shipyard still under construction keeps nobody in the match, nor do Constructors, Research Labs, rigs or platforms.
2. **A lost Command Station stays lost, and no code makes it so.** The tuning data gives the Command Station no cost, so a Constructor's order to build one is refused as `NotBuildable`, as it was in the MVP, and only a Command Station queues Constructors.
3. **A player without a Command Station has its finished Shipyards revealed.** At the end of each tick under fog of war, every finished Shipyard of such a player is added to each opponent's remembered structures, or refreshed there, unless the opponent sees it. A snapshot shows it marked `remembered`, and an attack order may name it (ADR-024 decision 7). A site is not revealed, since it keeps nobody in the match; a Shipyard is revealed from the tick it is finished.
4. **The AI's attack group goes for production first.** It ranks the enemy structures it sees or remembers: Shipyards, then the Command Station, then the rest. It attack-moves on the nearest of the best rank to the group's center, and keeps that target until it is gone, or until a structure of a better rank comes to light. When it knows none, it goes across the center to look, as before.
5. **The AI plays on without its Command Station.** It still keeps its Shipyards' queues full, researches, builds its plan and repairs with the Constructors it has, and plans new Shipyards round where its station stood. It queues no Constructors.

## Consequences

- **The win condition changes nothing against a player who builds no Shipyard.** The AI beats a player who does nothing at tick 9,390, 7.8 minutes in. It beats one who holds the middle at tick 10,811. Both were measured in `AiPlayerTests` in the Linux container.
- **A raid on the Command Station alone no longer ends a match** while a finished Shipyard stands elsewhere. A rush still ends it when the only Shipyard stands beside the station, which design §3 accepts.
- **The banner is unchanged**: Victory, Defeat or Draw, with the match's length.
- **The reveal costs a pass over the entities for each player each tick**, beside what vision already does, and only while some player has lost its station. It has not been measured.
- **`MatchOutcomeTests` check the rule.** The last Shipyard keeps a player in, is revealed to the opponent as a remembered structure, and losing it loses the match. A site does not count, and a Command Station cannot be built. `AiPlayerTests.AttacksProductionFirst` checks the order of the AI's targets and the switch to a Shipyard that comes to light; `PlaysOnWithoutItsStation` checks that its Shipyards keep working.
- **Not run.** The revealed Shipyards are drawn as remembered structures are, in the world and on the minimap. The owner's run checks them.

## What this forecloses

- Building a Command Station again, without a new decision.
- Anything other than a Command Station or a finished Shipyard keeping a player in the match.
- Revealing more of a beaten player's base than its finished Shipyards.

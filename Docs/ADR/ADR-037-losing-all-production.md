# ADR-037 — A player loses with its production, and its last Shipyards are revealed

Status: **accepted** · 2026-10-03
Owns: loss-by-production, lost-command-station, beaten-player-reveal

## Context

One raid that reaches the Command Station should not settle a match, so a player stays in while it can still produce (owner, 2026-10-02): a player loses when it has neither a Command Station nor a finished Shipyard; a lost Command Station is lost for good; and a player that has lost it has its remaining Shipyards shown to its opponent through fog of war, as remembered structures (ADR-024), so that the end of a match is not a search for one building.

## Decision

1. **A player whose base was placed loses when it has neither a Command Station nor a finished Shipyard** (owner, 2026-10-02). The server checks it every tick, and the match ends once, with a winner or a draw, as [ADR-020](ADR-020-ai-and-match-flow.md) decision 9 has it. In a universe nothing ends; what a loss does there is [ADR-084](ADR-084-universe-without-an-end.md) decisions 3 and 4's. A Shipyard still under construction keeps nobody in the match, nor does anything else.
2. **A lost Command Station stays lost in a match, and no code makes it so.** The tuning data gives the Command Station no cost, so a Constructor's order to build one is refused as `NotBuildable`, and only a Command Station queues Constructors. A universe's restarted seat is given a new one ([ADR-084](ADR-084-universe-without-an-end.md) decision 4).
3. **A player without a Command Station has its finished Shipyards revealed.** At the end of each tick under fog of war, every finished Shipyard of such a player is added to each opponent's remembered structures, or refreshed there, unless the opponent sees it. A snapshot shows it marked `remembered`, and an attack order may name it (ADR-024 decision 7). A site is not revealed, since it keeps nobody in the match; a Shipyard is revealed from the tick it is finished.
4. *(retired)*
5. *(retired)*

## Consequences

- **A raid on the Command Station alone does not end a match while a finished Shipyard stands** elsewhere. A rush ends it when the only Shipyard stands beside the station (owner, 2026-10-02).
- **The reveal costs a pass over the entities for each player each tick**, beside what vision already does, and only while some player has lost its station.
- **The revealed Shipyards are drawn as remembered structures are**, in the world and on the minimap.
- **`MatchOutcomeTests` check the rule.** The last Shipyard keeps a player in, is revealed to the opponent as a remembered structure, and losing it loses the match. A site does not count, and a Command Station cannot be built.

## What this forecloses

- Building a Command Station again, without a new decision; a universe's restart places one ([ADR-084](ADR-084-universe-without-an-end.md) decision 4).
- Anything other than a Command Station or a finished Shipyard keeping a player in the match.
- Revealing more of a beaten player's base than its finished Shipyards.

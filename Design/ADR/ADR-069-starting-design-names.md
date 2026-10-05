# ADR-069 — The starting designs' short names live in the tuning data

Status: **accepted** · 2026-10-05

## Context

Every player starts with four designs saved, the hull, drive and weapon combinations no research unlocks (design §7). Each was named for its components, "Small+Ion+Mass Driver" beside its code "S·I·MD". The review of 2026-10-03 found the names repeating the codes and cut short in the designer's chips.

The MVP design gives the four nicknames in its §7. Task 16.5 of the interface plan (`GameDesign/ImplementationPlan-Interface.md`) proposed them as the names. The owner decided gate K6 on 2026-10-03 as proposed, and chose the mockup's Lancer over Line on 2026-10-05.

## Decision

1. **`Tuning.json` names them in `startingDesigns`,** a list of each named design's hull, drive and weapon by id, and its name ([ADR-008](ADR-008-tuning-data.md)):
   - Swarm is Small+Ion+Mass Driver.
   - Picket is Small+Ion+Lance.
   - Brawler is Medium+Ion+Mass Driver.
   - Lancer is Medium+Ion+Lance.
2. **The list is optional, and strict when present.** `LoadTuning` rejects an entry whose components do not exist, or that research unlocks, so that it is not a starting design. It also rejects a name the server would refuse (`IsValidDesignName`), and an entry whose components or name another entry already has.
3. **Match setup saves each starting design under its short name** (`StartingDesignName`). A starting design the file does not name keeps its components' name (`DesignName`), and so does any design the AI saves.
4. **A name is only a name.** The design's components, its code and its stats are what they were, and a player can still rename it.

## Consequences

- **The match log names a starting design by its short name**, "Swarm", where it wrote "Small+Ion+Mass Driver". The log records the components beside every name ([ADR-038](ADR-038-phase-one-match-log.md)), so the analysis that reads it is unchanged. The balance check names designs by their codes and does not read the names.
- **The AI is unaffected.** It finds a design by its components, not by its name.
- **The tests.**
  - `TuningTests.LoadsEveryNumberInTheRepositoryFile` reads the names.
  - `RejectsABrokenStartingDesignName` refuses each kind of bad entry.
  - `DesignTests.StartingDesignsAreTheFourOfTheFirstMinutes` checks the four short names and the components' names.
  - `EveryPlayerStartsWithItsDesignsAndABase` finds Swarm saved.

## What this forecloses

- A starting design's name written anywhere but the tuning data.

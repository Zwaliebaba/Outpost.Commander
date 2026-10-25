# ADR-069 — The starting designs' short names live in the tuning data

Status: **accepted** · 2026-10-10
Owns: starting-design-names, module-design-names

## Context

Every player starts with the starting designs saved, the hull, drive and weapon combinations no research unlocks ([ADR-014](ADR-014-designs-and-combat.md) decision 2). A name made of a design's components, "Small+Ion+Mass Driver", repeats its code, "S·I·MD", and is cut short in the designer's chips. A short name says what the design is for, and a design that adds a module to a starting design reads best as that design with the module's initials (owner, 2026-10-03; Lancer, 2026-10-05; decision 5, 2026-10-08).

## Decision

1. **`Tuning.json` names them in `startingDesigns`,** a list of each named design's hull, drive and weapon by id, and its name ([ADR-008](ADR-008-tuning-data.md)), such as Swarm for Small+Ion+Mass Driver. The Catalogue lists them.
2. **The list is optional, and strict when present.** `LoadTuning` rejects an entry whose components do not exist, or that research unlocks, so that it is not a starting design. It also rejects a name the server would refuse (`IsValidDesignName`), and an entry whose components or name another entry already has.
3. **Match setup saves each starting design under its short name** (`StartingDesignName`). A starting design the file does not name keeps its components' name (`DesignName`), and so does any design the AI saves.
4. **A name is only a name.** It changes nothing of the design's components, its code or its stats, and a player can rename the design.
5. **A module on a starting design's hull, drive and weapon takes its name**, with the module's initials: a Sensor Array on the Lancer's components is "Lancer+SA" (`Designer::Name`). The client has no list of the starting designs, so it reads them as the snapshot gives them: a saved design without a module, of the same hull, drive and weapon, whose name is not its components'. A starting design the player has renamed lends its new name.
   - Where no such design is saved, or its name and the initials are longer than a name may be (`IsValidDesignName`), the components name the new design, "Small+Ion+Lance+Sensor Array", or with the module's initials where the whole is too long.
   - A typed name wins, and so does a saved design's own name.

## Consequences

- **The match log names a starting design by its short name**, "Swarm". The log records the components beside every name ([ADR-038](ADR-038-phase-one-match-log.md)), so the analysis that reads it does not depend on the names. The balance check names designs by their codes and does not read the names.
- **The AI finds a design by its components**, not by its name.
- **The tests.**
  - `TuningTests.LoadsEveryNumberInTheRepositoryFile` reads the names.
  - `RejectsABrokenStartingDesignName` refuses each kind of bad entry.
  - `DesignTests.StartingDesignsAreTheFourOfTheFirstMinutes` checks the four short names and the components' names.
  - `EveryPlayerStartsWithItsDesignsAndABase` finds Swarm saved.
  - `DesignerTests.NamesAModuleAfterItsStartingDesign` checks "Lancer+SA", a typed name kept, and the components' name where no saved design leads or the name would be too long.

## What this forecloses

- A starting design's name written anywhere but the tuning data.

# ADR-058 — A design has an optional module, and the Sensor Array lets a ship see 700 m

Status: **accepted** · 2026-10-03

## Context

Phase 2 design §10 gives a design an optional fourth slot beside hull, drive and weapon, empty by default. Its first module is the Sensor Array: the ship sees 700 m whatever its weapon, it costs 40 Ore, and it slows its ship by 10%. Gate J6 took those numbers as starting values on 2026-10-03, with no other module in Phase 2. The design says the designer gains a fourth row, a saved design gains a field, and a design's abbreviation gains the module's initials. It leaves open how a module reaches the protocol, whether research unlocks one, how sight combines with the weapon's, and what to call a design whose full name is too long.

## Decision

1. **Modules are data.** `Tuning.json` gains a `modules` list, each with an `id`, a `name`, `sightMeters`, `speedFactor` and `cost`. The Sensor Array is module 1: 700 m, a factor of 0.9 and 40 Ore. The loader requires the list, a positive speed factor and unique identifiers.
2. **No research unlocks a module.** Every module is available from the start, as the design's minute-3 scout needs. A research effect that unlocks one is the change to make when a module needs it.
3. **A module is part of a design's components.** `DesignComponents`, `DesignView`, `EntityView` and `SaveDesignCommand` carry a `ModuleId`, and no identifier means no module. The same hull, drive and weapon with and without a module are two designs. A starting design has none. The server refuses a module the tuning data does not have, as it refuses any other unknown component (`UnknownComponent`).
4. **One function still derives a design's stats** (ADR-017 decision 5). `DesignStatsOf` takes an optional module view: its cost is added, its factor slows the ship, and its sight is kept as `DesignStats::moduleSightMeters`. Every snapshot lists the modules (`Snapshot::modules`), so the designer derives what the server does.
5. **A ship sees the further of its module's sight and its own** (ADR-024). The Sensor Array's 700 m is beyond every weapon's range plus the 50 m margin, so a ship with one sees 700 m.
6. **A design's name follows the weapon's with its module's.** The server writes "Small+Ion+Mass Driver+Sensor Array". The designer uses the module's initials instead, "Small+Ion+Mass Driver+SA", where the whole name would be longer than the 32 characters a name may have (ADR-017 decision 6). A chip's initials gain the module's: "S·I·MD·SA".
7. **The designer's fourth row is the module.** Its first card is None, which every design starts with; then one card per module, with its sight, speed factor and cost. The pick stays empty until the player picks a module. A hovered module previews the design it would make, as a hovered part does. A Sensors bar shows the module's sight against the furthest any module gives, and "-" without one. The window still fits a screen of 1,080 lines.
8. **The balance check leaves modules out**, as it leaves out the Pulse Drive's speed (Phase 1 gate H4). Its battles are between clumps that a module's sight does not change, and it builds its designs from hull, drive and weapon alone, so a design with a module is never one of them.

## Consequences

- **Tests.** `SaveDesignTests.SavesADesignWithASensorArray` covers its cost, speed and sight on the server, a ship of it seeing 700 m, and an unknown module refused. `DesignTests.AModuleJoinsTheDesign` covers its name and stats. `TuningTests` covers the list. `DesignerTests.PicksAModuleOrNone` covers the designer, and `HudTests.OffersTheModulesInAFourthRow` covers the row, the chip, the bar, the preview and the window's height.
- **Not built or run on Windows here.** CI is the first build. The fourth row is the owner's run.

## What this forecloses

- More than one module to a design, without a new decision.

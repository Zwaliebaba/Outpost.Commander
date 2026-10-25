# ADR-058 — A design has an optional module, and the Sensor Array sets how far its ship sees

Status: **accepted** · 2026-10-06
Owns: ship-modules, sensor-array, module-sight

## Context

A design has a fourth slot beside hull, drive and weapon, which is optional and empty by default (owner, 2026-10-03). Its module is the Sensor Array, which lets a ship see far whatever its weapon, at a cost and a little of its speed. The rules have to say how a module reaches the protocol, whether research unlocks one, how its sight combines with the weapon's, and what a design with one is called.

## Decision

1. **Modules are data.** `Tuning.json` has a `modules` list, each with an `id`, a `name`, `sightMeters`, `speedFactor` and `cost`; the Sensor Array is module 1. The loader requires the list, a positive speed factor and unique identifiers.
2. **No research unlocks a module.** Every module is available from the start, as the AI's early scout needs ([ADR-020](ADR-020-ai-and-match-flow.md) decision 13). A research effect that unlocks one is the change to make when a module needs it.
3. **A module is part of a design's components.** `DesignComponents`, `DesignView`, `EntityView` and `SaveDesignCommand` carry a `ModuleId`, and no identifier means no module. The same hull, drive and weapon with and without a module are two designs. A starting design has none. The server refuses a module the tuning data does not have, as it refuses any other unknown component (`UnknownComponent`).
4. **One function derives a design's stats** (ADR-017 decision 5). `DesignStatsOf` takes an optional module view: its cost is added, its factor slows the ship, and its sight is kept as `DesignStats::moduleSightMeters`. Every snapshot lists the modules (`Snapshot::modules`), so the designer derives what the server does.
5. **A ship sees the further of its module's sight and its own** ([ADR-024](ADR-024-fog-of-war.md)): its weapon's range plus the sight margin. The Sensor Array's sight is beyond every weapon's range plus the margin, so a ship with one sees as far as the module.
6. **A design's name follows the weapon's with its module's.** The server writes "Small+Ion+Mass Driver+Sensor Array". How the designer names new picks with a module is [ADR-069](ADR-069-starting-design-names.md) decision 5's. A design's code of initials gains the module's, "S·I·MD·SA", in the designer's chips and in the production window alike, and the match log names a ship's module after its components ([ADR-038](ADR-038-phase-one-match-log.md)).
7. **The designer's fourth row is the module.** Its first card is None, which every design starts with; then one card per module, with its sight, speed factor and cost. The pick stays empty until the player picks a module. A hovered module previews the design it would make, as a hovered part does. A Sensors bar shows the module's sight against the furthest any module gives, and "-" without one. The window fits a screen of 1,080 lines.
8. **The balance check leaves modules out.** Its battles are between clumps that a module's sight does not change, and it builds its designs from hull, drive and weapon alone, so a design with a module is never one of them. What else it leaves out is [ADR-033](ADR-033-research-tiers.md) decision 6's.

## Consequences

- **Tests.** `SaveDesignTests.SavesADesignWithASensorArray` covers its cost, speed and sight on the server, a ship of it seeing as far as the module, and an unknown module refused. `DesignTests.AModuleJoinsTheDesign` covers its name and stats. `TuningTests` covers the list. `DesignerTests.PicksAModuleOrNone` covers the designer, and `HudTests.OffersTheModulesInAFourthRow` covers the row, the chip, the bar, the preview and the window's height. `HudTests.NamesAModuleInTheProductionWindow` covers the production window's code, and `MatchLogTests.RecordsAShipsModule` the match log's.

## What this forecloses

- More than one module to a design, without a new decision.

# ADR-033 — Research has tiers opened by the Research Lab's levels, and upgrades of one stat add

Status: **accepted** · 2026-10-04
Owns: research-tiers, upgrade-stacking, research-effect-kinds, balance-check-exemptions

## Context

Research runs across a match in three tiers (owner, 2026-10-02), and the Research Lab's levels open the later ones (owner, 2026-10-04). Upgrades of one stat add rather than multiply: two topics of +15% on one rate make +30%. The tiers' topics reach kinds of effect beyond a component's own rate: every structure's hit points, a structure weapon's fire rate, every ship's speed, the Constructors' build and repair rate, and the ore an asteroid holds. Upgrades change rates, never the size of a hit or a range.

## Decision

1. **A topic states its tier** in `Tuning.json`, `"tier"`, from 1 to `RESEARCH_TIERS`, 3. It is required, so that no topic falls into a tier by default.
2. **A level of the Research Lab opens a tier** (owner, 2026-10-04). Which level opens which tier, and the topics its upgrade requires, are the Lab's level data ([ADR-064](ADR-064-structure-upgrades.md) decision 10). The loader checks that each tier after the first that has topics is opened by one level, a later tier by a later level, and that no topic requires one of a later tier. A Lab refuses a topic of a tier its level has not opened (`LevelTooLow`), and a topic queued at the right level stays queued. No topic opens a tier, and the identifiers 9 and 18 are not reused.
3. **Upgrades of one stat add their percentages.** `UpgradesFrom` sums each rate's percentages and makes one factor of the sum. A weapon's or a structure weapon's rate is summed per weapon.
4. **Beyond a component's own rate, an effect may target** `allStructures` hit points, a `structureWeapon`'s fire rate (naming the weapon), `allShips` speed, the `constructors`' build rate, or the `asteroids`' ore reserve, each with its one rate. In the simulation:
   - **Structure hit points** are the kind's, times the factor, when a structure is placed and when its owner finishes the topic. One standing keeps its share of its hit points, as a ship does under Hull Plating. A structure placed with numbers of its own, as a test or a measurement load places them, keeps them.
   - **A structure weapon's fire rate** divides its interval when it fires, as a weapon's does.
   - **Ship speed** is in the hull's speed, so a design's stats, the designer and every warship of the design follow it; a Constructor's speed is the rule's times the factor. Turn rates do not change. A move under way keeps its pace until the next order.
   - **The Constructors' rate** multiplies their build work, their repair and their salvage ([ADR-074](ADR-074-salvage.md) decision 4), by their player's research.
   - **The ore reserve** factor divides what its player's rigs draw from their asteroids ([ADR-035](ADR-035-ore-depletion.md) decision 3).
5. **The snapshot** gives each topic its tier, and the player's open tier (`Snapshot::researchTier`), which its finished Lab's level sets and which is 1 without one. The research window lists the topics tier by tier, and a topic of a tier not open is dim and says which level of the Lab opens it.
6. **The balance check**, the C++ one and `Tools/BattleModel.py`, adds percentages the same way, reads every kind of effect, and leaves out by name those a battle between two clumps cannot feel: economy, structures, speed, Constructors and ore. Its criterion (d) runs per tier. For the same reason its criterion (b) does not ask a drive whose case is speed, the Pulse Drive, to be worth building (owner, 2026-10-02), and it leaves modules out ([ADR-058](ADR-058-modules.md) decision 8).

## Consequences

- **No tier-1 rate has two topics.**
- **The open tier is in the snapshot**, so the AI ([ADR-041](ADR-041-ai-plays-a-longer-match.md) decision 1) and the match log ([ADR-038](ADR-038-phase-one-match-log.md)) read it as the HUD does.
- **A lost Lab closes its tiers** until a rebuilt one is upgraded again. The research already finished stays finished.

## What this forecloses

- A topic that opens a tier, or tiers past the third, without a new decision.
- Upgrades that multiply, or that change the size of a hit, armor or a range.

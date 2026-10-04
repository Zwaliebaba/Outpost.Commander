# ADR-033 — Research has tiers opened by the Research Lab's levels, and upgrades of one stat add

Status: **accepted** · 2026-10-03

## Context

Phase 1 design §6 turns the MVP's eight research topics into three tiers (owner, 2026-10-02, gate H2), so that research runs most of a 45-minute match. Phase 1 opened each tier after the first with a gateway, a long and costly topic that did nothing itself. Phase 3 design §6 replaces the gateways with the Research Lab's levels 2 and 3 (gate K2), which leaves 23 topics. Upgrades of one stat add rather than multiply: Hull Plating's +15% and Composite Plating's +15% make +30%. And the tiers bring kinds of effect the MVP had no use for: every structure's hit points, a structure weapon's fire rate, every ship's speed, the Constructors' build and repair rate, and the ore an asteroid holds. Upgrades still change rates, never the size of a hit or a range (MVP §8).

## Decision

1. **A topic states its tier** in `Tuning.json`, `"tier"`, from 1 to `RESEARCH_TIERS`, 3. It is required, so that no topic falls into a tier by default.
2. **A level of the Research Lab opens a tier** (Phase 3 design §6, [ADR-064](ADR-064-structure-upgrades.md) decision 10). The Lab's level in `Tuning.json` names the tier it opens, `"opensTier"`, and the topics its upgrade requires, `"requires"`: level 2 opens tier 2 and requires Improved Extraction and Hull Plating, which Relay Archives required, and level 3 opens tier 3 and requires Large Hull, which Precursor Vault required besides the gateway before it. The loader checks that each tier after the first that has topics is opened by one level, a later tier by a later level, and that no topic requires one of a later tier. A Lab refuses a topic of a tier its level has not opened (`LevelTooLow`), and a topic queued at the right level stays queued. The gateway topics, Relay Archives (9) and Precursor Vault (18), are gone, and their identifiers are not reused. No topic opens a tier: `GatewayEffect` is gone too.
3. **Upgrades of one stat add their percentages.** `UpgradesFrom` sums each rate's percentages and makes one factor of the sum. A weapon's or a structure weapon's rate is summed per weapon. The MVP's eight topics never stack, so tier 1 gives exactly the factors it gave before.
4. **The new kinds of effect** are each a target with its one rate, as before: `allStructures` hit points, `structureWeapon` fire rate (naming the weapon), `allShips` speed, `constructors` build rate, and `asteroids` ore reserve. In the simulation:
   - **Structure hit points** are the kind's, times the factor, when a structure is placed and when its owner finishes the topic. One standing keeps its share of its hit points, as a ship does under Hull Plating. A structure placed with numbers of its own, as a test or a measurement load places them, keeps them.
   - **A structure weapon's fire rate** divides its interval when it fires, as a weapon's does.
   - **Ship speed** is in the hull's speed, so a design's stats, the designer and every warship of the design follow it; a Constructor's speed is the rule's times the factor. Turn rates do not change (Phase 1 design §6). A move under way keeps its pace until the next order.
   - **The Constructors' rate** multiplies both their build work and their repair, by their player's research.
   - **The ore reserve** factor divides what its player's rigs draw from their asteroids ([ADR-035](ADR-035-ore-depletion.md) decision 3).
5. **The snapshot** gives each topic its tier, and the player's open tier (`Snapshot::researchTier`), which its finished Lab's level sets and which is 1 without one. The research window lists the topics tier by tier, and a topic of a tier not open is dim and says which level of the Lab opens it.
6. **Both Q2 checks**, the C++ one and `Tools/BattleModel.py`, add percentages the same way, read the new kinds of effect, and leave out by name those a battle between two clumps cannot feel: economy, structures, speed, Constructors and ore. Their (d) runs per tier, as Phase 1 design §7 has it.

## Consequences

- **Tier 1 is unchanged**, numbers and verdicts: no tier-1 rate has two topics.
- **The open tier is in the snapshot**, so the AI ([ADR-041](ADR-041-ai-plays-a-longer-match.md) decision 1) and the match log ([ADR-038](ADR-038-phase-one-match-log.md)) read it as the HUD does.
- **A lost Lab closes its tiers** until a rebuilt one is upgraded again. The research already finished stays finished.

## What this forecloses

- A topic that opens a tier, or tiers past the third, without a new decision.
- Upgrades that multiply, or that change the size of a hit, armor or a range.

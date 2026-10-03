# ADR-033 — Research has tiers opened by gateways, and upgrades of one stat add

Status: **accepted** · 2026-10-03 · beside ADR-017, which it changes where it says two upgrades multiply

## Context

Phase 1 design §6 turns the MVP's eight research topics into three tiers of 25 (owner, 2026-10-02, gate H2), so that research runs most of a 45-minute match. A tier opens with a gateway, a long and costly topic that does nothing itself and that every other topic of its tier requires. Upgrades of one stat add rather than multiply: Hull Plating's +15% and Composite Plating's +15% make +30%. And the tiers bring kinds of effect the MVP had no use for: every structure's hit points, a structure weapon's fire rate, every ship's speed, the Constructors' build and repair rate, and the ore an asteroid holds. Upgrades still change rates, never the size of a hit or a range (MVP §8).

## Decision

1. **A topic states its tier** in `Tuning.json`, `"tier"`, from 1 to `RESEARCH_TIERS`, 3. It is required, so that no topic falls into a tier by default.
2. **A gateway is an effect of its own**, `"effect": { "opensTier": 2 }`, read as `GatewayEffect`. It unlocks and upgrades nothing. What makes it a gate is the prerequisites, which the loader checks: a gateway opens its own tier, a tier has one, and every other topic of a tier after the first requires its gateway directly. No topic requires one of a later tier. The research rules themselves are unchanged (ADR-017): a topic may be queued once its prerequisites are researched or queued, so a lab can queue a gateway and a topic of its tier one after the other.
3. **Upgrades of one stat add their percentages.** `UpgradesFrom` sums each rate's percentages and makes one factor of the sum. A weapon's or a structure weapon's rate is summed per weapon. The MVP's eight topics never stack, so tier 1 gives exactly the factors it gave before.
4. **The new kinds of effect** are each a target with its one rate, as before: `allStructures` hit points, `structureWeapon` fire rate (naming the weapon), `allShips` speed, `constructors` build rate, and `asteroids` ore reserve. In the simulation:
   - **Structure hit points** are the kind's, times the factor, when a structure is placed and when its owner finishes the topic. One standing keeps its share of its hit points, as a ship does under Hull Plating. A structure placed with numbers of its own, as a test or a measurement load places them, keeps them.
   - **A structure weapon's fire rate** divides its interval when it fires, as a weapon's does.
   - **Ship speed** is in the hull's speed, so a design's stats, the designer and every warship of the design follow it; a Constructor's speed is the rule's times the factor. Turn rates do not change (Phase 1 design §6). A move under way keeps its pace until the next order.
   - **The Constructors' rate** multiplies both their build work and their repair, by their player's research.
   - **The ore reserve** is held in `Upgrades` for the asteroids of task 11.1, which are the first to have a reserve.
5. **The snapshot** gives each topic its tier and whether it is a gateway, and the research window lists the topics tier by tier, a gateway edged in gold.
6. **Both Q2 checks**, the C++ one and `Tools/BattleModel.py`, add percentages the same way, read the new kinds of effect, and leave out by name those a battle between two clumps cannot feel: economy, structures, speed, Constructors and ore. Their (d) stays over tier 1's topics, the MVP's, until task 10.3 gives each tier its stage.

## Consequences

- **Tier 1 is unchanged**, numbers and verdicts: no tier-1 rate has two topics, and the Q2 check of record runs tier 1 only.
- **Four of the 17 topics arrive with their components** in task 10.2: the Pulse Drive's, the Flak Battery's and the Rail Cannon's unlocks, and Proximity Fuses, which raises the Flak Battery's rate. The loader refuses a topic that names a component the data does not have, so they cannot come first.
- **Deep Core Survey does nothing yet**: its factor waits for task 11.1's reserves, and its topic says what it will do.
- **A gateway is visible as data**, so the AI (task 10.4) and the match log read it the same way the HUD does.

## What this forecloses

- Topics that open a tier without being its gateway, or tiers past the third, without a new decision.
- Upgrades that multiply, or that change the size of a hit, armor or a range.

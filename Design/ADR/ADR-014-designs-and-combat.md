# ADR-014 — Designs are derived stats, hit points count in hundredths, and hits land together at the end of a tick

Status: **accepted** · 2026-10-01

## Context

Tasks 3.2 and 3.3 give ships designs and make them fight. Design §7 sets the rules: a design is a hull, a drive and a weapon. Hits are instant and weapons are turrets. Auto-targeting takes the nearest enemy ship in range, otherwise the nearest structure, and keeps it. An attack-moving ship stops at its own range. Damage taken is `max(damage × 0.25, damage − armor)`. `Tools/BattleModel.py` already plays these rules in the abstract, and task 3.4 checks the simulation against it, so wherever design §7 leaves a detail open the simulation follows the model. ADR-009 left open whether hit points stay integers once damage is applied. The owner decided on 2026-10-01 that the provisional starting fleet carries every starting design.

## Decision

1. **A design's stats are derived from the tuning data, never stored in it.** `Outpost::DesignStatsFor` combines a hull, a drive and a weapon into hit points, armor, speed, cost, build time, damage, fire interval and range, as the model's `designs_from` does. A saved design (`ShipDesign`) keeps its stats, so a ship reads them from its design rather than carrying a copy. Research (task 5.1) changes them by design.
2. **Every player starts with the starting designs saved**, the components no research topic unlocks, in hull, drive and weapon order (design §7, §9): four with today's data. `InProcessServer` saves them and adds each player with the starting Ore as match setup. The map's `startingFleet` names a weapon as well as a hull and drive, and each starting ship is of its player's design of those components: two Small+Ion+Mass Driver, two Small+Ion+Lance, one Medium+Ion+Mass Driver and one Medium+Ion+Lance per player.
3. **Hit points and damage count in hundredths of a point,** as integers (`hitPointsHundredths`). A drive's factor gives whole hundredths for every hull today (220 × 0.9 = 198.00), and the armor rule's quarter hit always does: a Mass Driver hit on a Large hull is 3.50. Integers make the order of additions irrelevant, so ADR-009's replay holds without reasoning about float sums. A research upgrade of 15% still gives whole hundredths for every hull.
4. **A weapon reloads in thousandths of a tick.** The fire interval becomes `interval × tick rate × 1000` milliticks, rounded once. Each tick takes 1000 off. The weapon fires on the tick the count reaches zero or less, and the interval is added back, so an interval that is not a whole number of ticks still fires at its average rate. Today's intervals are 8, 40 and 60 ticks exactly.
5. **A ship's first shot after it finds a target comes at a random moment within its interval**, drawn from the simulation's PRNG, as in the model. Volleys of a group then do not land together. An idle weapon finishes reloading and waits; finding a target never makes it fire sooner than its last shot allows.
6. **Shots are chosen from where everything stands at the start of the tick, and land together at its end.** No shot depends on which ship was handled first. As in the model, a target can take more than it needs, so focus fire overkills. Then the destroyed leave the world. Orders on them end, and so do targets.
7. **Range is measured between centers**, as the model measures it between clumps. A big target is not easier to reach.
8. **Targeting follows design §7**, ships before structures, and keeps the target until it dies or leaves range. Two other rules exist only for the Q2 check: random and weakest, each chosen again for every shot, the model's spread and focus fire (`Simulation::SetTargetRule`, task 3.4). A match never uses them.
9. **Combat runs before movement in a tick.** An attack-moving ship with a target in range stands still that tick. A ship on an attack order closes along a route the group searched once (ADR-010) at its own top speed, stands while its target is in range, and paths again when the target has moved 40 m from where it last pathed to, at most once a second. Every other ship fires on the move.
10. **What has no hit points is out of combat.** Asteroids and fields have none. So do ships and structures placed without them: movement tests and task 2.7's measurement load place those, and their timings stay comparable. Nothing targets them, and they do not fire.
11. **A snapshot says what happened in its tick**: each shot, with where it went from and to when it was fired, and each entity destroyed, with where it was. The client draws effects from those (task 3.5). The snapshot also carries hit points for every entity, the player's Ore, and the player's designs with their names.

## Consequences

- **Targeting is O(n²) per tick when targets are lost.** A ship that keeps its target checks only that one. A ship looking for one checks every enemy: at 200 ships, 20,000 distance tests on a tick when everyone looks at once. Task 3.7 measures what that costs.
- **Splash is not applied yet.** The Missile Rack's 30 m splash waits for task 5.3 and ship sizes (gate G5). Until then a Missile Rack hit hurts only its target, and no player can build one before research exists (task 5.1).
- **Structures fire with task 4.4.** Until then a structure is a target only.
- **The client cannot see why a command was rejected.** Attack now answers `UnknownTarget` or `NotAnEnemy`, but as ADR-009 says, the protocol has no message for it yet.

## What this forecloses

- Projectiles with flight time, accuracy, tracking or damage types (design §13), without a new ADR.
- Friendly fire.
- Float hit points.

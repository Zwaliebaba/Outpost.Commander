# ADR-046 — A Mining Rig is ordered only onto ore the player has seen, and --stress stands its rigs on ore

Status: **accepted** · 2026-10-03
Owns: rig-order-on-seen-asteroid, stress-load-rigs

## Context

A Mining Rig is built on an ore asteroid, and the player finds the ore by exploring under the fog ([ADR-024](ADR-024-fog-of-war.md)). An asteroid in space the player has never seen is one it cannot know is there, so the game's own interface must not let the player claim it (owner, 2026-10-03).

The stress scene, `--stress`, builds a match to measure the game's load. A Mining Rig in it has to stand where a rig built in a match stands, on an ore asteroid, and not on open ground (owner, 2026-10-03).

## Decision

1. *(retired)*
2. *(retired)*
3. *(retired)*
4. *(retired)*
5. *(retired)*
6. *(retired)*
7. *(retired)*
8. **A Mining Rig is ordered only onto an asteroid the player has seen.**
   - `FogOfWar::HasSeen` says whether any cell of the asteroid's circle was ever in sight; without fog of war, all of the map was.
   - The client gives the controls and the rig's ghost the view less the asteroids never seen. The ghost is then red there, and a click orders nothing and leaves the placement armed.
   - The server accepts such an order. Making it a rule of the server would also bind the AI, which follows the ore without exploring first ([ADR-020](ADR-020-ai-and-match-flow.md) decision 5).
9. **`--stress` stands its Mining Rigs on the free ore asteroids nearest each player's structures**, as a rig built in a match stands. A map with too few asteroids for them stops the run with a message.

## Consequences

- **Tests:**
  - `FogOfWarTests.SaysWhetherAnyOfACircleWasSeen`.
  - `PlayerControlsTests.OrdersAMiningRigOnlyByAKnownAsteroid`.
  - `StressLoadTests` checks that every Mining Rig in the scene stands on an asteroid.

## What this forecloses

- A rig claimed in space the player has never seen, through the game's own interface.

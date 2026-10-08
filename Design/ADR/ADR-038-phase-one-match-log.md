# ADR-038 — The match log records tiers, fleets, dry asteroids, the fight for territory, structure levels, and Phase 4's salvage, pirates and repairs, and a switch plays ten AI-against-AI matches headlessly

Status: **accepted** · 2026-10-03

## Context

Phase 1's questions P1, P2 and P4 are answered from the match log (Phase 1 design §2, §10). P1 needs each match's length. P2 needs what each side builds before and after each tier's gateway. P4 needs the warship counts a match reaches. The log of ADR-020 decision 11 holds the seed, each research topic as it finishes, each warship as it first appears, and the end. P1 also asks for a figure that repeats: the median length of 10 seeded matches between two AIs on the real server, with the spread.

Phase 2's questions S1 to S4 are answered from the same log (Phase 2 design §2, plan task 19.1). S1 needs the first shot, S2 each engagement before minute 20 and the sector it was in, S3 how each match ended, and S4 the length, as P1.

Phase 4's U1 to U5 are too (Phase 4 design §2, plan task 34.1). U1 needs each derelict a side salvages and each pirate outpost it fights before the players' first shot at each other, and U3 each warship a side turns for home, has repaired and sees fight again.

Three things stand in the way:

- **The server is paced by wall time.** Started, it ticks on its own thread at 20 Hz (ADR-025), so a 60-minute match takes 60 minutes.
- **Its timing is not reproducible.** The AI reads snapshots on the client's thread, so when its commands land depends on timing, and the match does not reproduce from its seed.
- **Only the executable can hold both halves.** The AI is in `Opponent` and the log is in `GameApp`. Of the projects that run the server, only the executable may include both (AGENTS.md §2): `GameLogicTests` may not include `GameApp`.

## Decision

1. **The log gains four records**, and keeps every record it had, so an older log still reads:
   - `tier <tick> player <player> tier <tier>`: the first snapshot in which the player's Research Lab has opened the tier by its level (Phase 3 design §6).
   - `fleet <tick> player <player> warships <count>`: every 30 seconds, from the player's own snapshots.
   - `dry <tick> asteroid <id>`: once, when a snapshot first shows the asteroid's reserve at zero.
   - `peak <tick> player <player> warships <count>`: the most warships the player had at once, and when it first had them. Written for each player just before `end`, or before `left` when the match is left.

   Phase 2 adds five more, for S1 to S3:
   - `contact <tick> sector <id>`: the match's first shot, in the sector it was fired from, or 0 for none.
   - `engagement <tick> sector <id>`: ships of both sides fired in one sector within 10 seconds of each other. Another engagement there counts only after 30 seconds without a shot in it. A shot is shown to each player who sees its shooter or its target (ADR-024), so a tick's shots are counted once by shooter and target. The side that fired is the shooter's owner, or the side its target is not on when the snapshot does not show the shooter.
   - A shot by or at a pirate is neither a contact nor part of an engagement, which are the players' ([ADR-073](ADR-073-pirates.md)); Phase 4's `pirates` record counts it.
   - `sector <tick> sector <id> holder <player or 0>`: a sector's holder changed, the homes' at the start included. Both players see the territory alike (ADR-056 decision 10).
   - `tickets <tick> player <player> tickets <count>`: with each `fleet` record, on a map with territory.
   - `ending <tick> <production or domination>`: how the match ended (ADR-057 decision 6), just before `end`.

   Phase 3 adds three more, for T2 to T4 (Phase 3 plan task 25.1):
   - `upgrade <tick> player <player> structure <id> <kind> level <level> <started, finished or lost>`: from the owner's own snapshots, the first in which a level is under way, the first that shows it in, and the one whose destroyed structures show the structure lost with it under way ([ADR-064](ADR-064-structure-upgrades.md)). The kind is one word: `station`, `shipyard` or `lab`.
   - `attacked <tick> player <owner> structure <id> <kind> level <level>`: the first shot at a structure at a level above 1, once for each structure and level, from the snapshot that shows the shot and its target.
   - `stall <tick> ticks <count>`: written with the peaks on a map with territory, the ticks during which both players were at their node caps, a Relay site counting, and held as many nodes as each other. A tick counts once both players' snapshots of it are in.

   Phase 4 adds six more, for U1 and U3 (Phase 4 plan task 34.1):
   - `salvaged <tick> player <player> derelict <id> ore <ore>`: a derelict that one of the player's Constructors was within 30 m of in one of its snapshots, beyond both footprints, and that is gone from its next. A derelict leaves only by its salvage ([ADR-074](ADR-074-salvage.md)), and a player sees what its Constructors are at, so the snapshots tell who salvaged it without the server saying. The 30 m is the server's 20 m reach for work and a margin for a snapshot's age. Two players' crews at one derelict would both count.
   - `pirates <tick> player <player> sector <id>`: the first shot between the player and the pirates in a sector, in the sector of the pirates' end of the shot, once for each player and sector.
   - `cleared <tick> sector <id>`: a sector the pirates guarded no longer is.
   - `retreat <tick> player <player> ship <id>`: one of the player's warships turned for home to be repaired ([ADR-075](ADR-075-repair-and-retreat.md)), from its own snapshots, each time it does.
   - `repaired <tick> player <player> ship <id>`: a warship's retreat ended with it whole. One ended by an order is neither.
   - `again <tick> player <player> ship <id>`: a repaired warship fired, at a player or the pirates, the first time since its repair.
2. **`Server::Step` runs one tick now, on the caller's thread.** It applies the commands that have arrived and sends each player its snapshot. A started server refuses it. A headless run steps the server instead of starting it, so no wall time enters it at all; the tick stays the clock (ADR-009).
3. **The executable's `--ai-matches` switch plays seeds 1 to 10.** Both seats are the AI with the packaged settings. Its options name other seeds, each AI's settings and the log, and `--quiet` runs it for a script without a message ([ADR-063](ADR-063-self-play-probe.md)).
   - Each match is stepped a tick at a time until it ends or reaches 120 minutes. Both AIs get each tick's snapshot, and their commands apply at the next tick, so a match reproduces from its seed.
   - The matches run on as many threads as the machine has, each into a buffer of its own. They are written in seed order to `OutpostCommander-ai-matches.log` in the temporary folder, which each run replaces.
   - No window opens, and a message says when the run is done.
   - The code is `OutpostCommander/AiMatches.cpp`.
4. **`Tools/MatchLog.py` reports Phase 1's figures**:
   - a match's length against P1's 45 to 60 minutes;
   - the asteroids that ran dry;
   - for each player, the tiers it opened, its peak warship count, and its warships by design in each tier.

   For Phase 2 it adds, for each match, how it ended, its first shot against S1's 5 minutes, and its engagements before minute 20 and their sectors against S2's five in three.

   For Phase 3 it adds, for each match, the stall against T2's 5 minutes, the structures above level 1 attacked, and each player's levels as they finished. Over the matches it reports the stall's median (T2), the matches in which each side reached level 3 of the Command Station, a Shipyard and the Research Lab and those in which tier 3 was opened (T3), and the median of the structures above level 1 attacked (T4).

   For Phase 4 it adds, for each match, the players' first shot at each other against U1's minute 8 to 20, the outposts cleared, the engagements before minute 40 against U4's five in three, and for each player what it salvaged and fought of the pirates before that shot, its warships at minute 20, and the warships it turned for home, had repaired and saw fight again, each counted once. Over the matches it scores U1 to U5 as design §2 words them: a side's figures at the median for U1 and U3, the larger side's in the median match for U2. It leaves the pirates out of the players, though their warships are written as `built`.

   `--ai-matches` reads the AI-against-AI log, names the players AI 1 and AI 2, and ends with every match's length: the median, the spread, and how many did not end. Then come S1 to S3 over the matches: how many had a shot by minute 5, the median of their engagements and sectors before minute 20 and how many met five in three, and how many ended each way.

## Consequences

- **The ten matches took 9 seconds** in the Linux container: clang 18 at `-O2` on four threads, building `AiMatches.cpp` against the real server and data.
- **With the AI of the time, they ended at about 22 minutes, not 45–60.** All ten ended: the median was 21:50, and the spread 19:47 to 32:18. Seeds 11 to 40 gave a median of 22:44, from 19:37 to 29:14, and all 40 together 22:11. In the 40 matches, every AI opened tier 2 at 15:04, but one, at 15:09, and none reached tier 3 before the end. Over the 40, a side's peak was a median of 80 warships, from 62 to 116. These are clang's outcomes: floats are promised to replay only on the same build (ADR-009), so MSVC's build may play the same seeds differently. [ADR-041](ADR-041-ai-plays-a-longer-match.md) changed the AI's play to lengthen them, and gives the figures since.
- **Player 1 won 30 of the 40 matches**, on a point-symmetric map with the same AI in both seats. That was a seat bias, and its cause is not known. It was not the order the server applies the two players' commands: connecting player 2 first gave the same 30 to 10. [ADR-041](ADR-041-ai-plays-a-longer-match.md) measures it gone, for now.
- **`MatchLogTests` check the records**: the tier, the 30-second counts, the peak and a dry asteroid written once; for Phase 2, the first shot, an engagement counted once across both players' snapshots and again only after the gap, a sector's holder written when it changes, the tickets, and the ending; for Phase 3, an upgrade started, finished and lost, a structure above level 1 attacked once, and the stall counted once a tick; for Phase 4, a fight with the pirates counted once and apart from the players', a sector cleared, a salvage credited to the crew's player and not to one who only saw it, and a warship's retreat, repair and next shot. `InProcessServerTests.StepsOneTickAtOnce` checks `Step`, and `RunsItsTicksOnItsOwnThread` checks that a started server refuses it.
- **Phase 4's records were checked against the Linux harness** (design §2): four AI-against-AI matches logged through `MatchLog` there gave the same derelicts salvaged, before the players' first shot and in all, the same pirate outposts fought before it, and the same warships fighting again, side for side, as the harness counted from the server. The harness had counted a retreat each time a ship went; U3 counts each warship once, as the log does.
- **The switch has no test in CI.** No test project may include both the AI and the log. It was run in the container. The message and the switch itself have not run on Windows.

## What this forecloses

- AI-against-AI matches in CI.
- A record not listed here or in ADR-020, without a new decision.

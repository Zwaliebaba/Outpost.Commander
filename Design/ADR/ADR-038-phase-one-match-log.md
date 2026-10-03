# ADR-038 — The match log records tiers, fleets and dry asteroids, and a switch plays ten AI-against-AI matches headlessly

Status: **accepted** · 2026-10-03 · beside ADR-020's decision 11, whose records it keeps

## Context

Phase 1's questions P1, P2 and P4 are answered from the match log (Phase 1 design §2, §10). P1 needs each match's length. P2 needs what each side builds before and after each tier's gateway. P4 needs the warship counts a match reaches. The log of ADR-020 decision 11 holds the seed, each research topic as it finishes, each warship as it first appears, and the end. P1 also asks for a figure that repeats: the median length of 10 seeded matches between two AIs on the real server, with the spread.

Three things stand in the way:

- **The server is paced by wall time.** Started, it ticks on its own thread at 20 Hz (ADR-025), so a 60-minute match takes 60 minutes.
- **Its timing is not reproducible.** The AI reads snapshots on the client's thread, so when its commands land depends on timing, and the match does not reproduce from its seed.
- **Only the executable can hold both halves.** The AI is in `Opponent` and the log is in `GameApp`. Of the projects that run the server, only the executable may include both (AGENTS.md §2): `GameLogicTests` may not include `GameApp`.

## Decision

1. **The log gains four records**, and keeps every record it had, so an older log still reads:
   - `tier <tick> player <player> tier <tier>`: after the research line of a gateway topic.
   - `fleet <tick> player <player> warships <count>`: every 30 seconds, from the player's own snapshots.
   - `dry <tick> asteroid <id>`: once, when a snapshot first shows the asteroid's reserve at zero.
   - `peak <tick> player <player> warships <count>`: the most warships the player had at once, and when it first had them. Written for each player just before `end`, or before `left` when the match is left.
2. **`Server::Step` runs one tick now, on the caller's thread.** It applies the commands that have arrived and sends each player its snapshot. A started server refuses it. A headless run steps the server instead of starting it, so no wall time enters it at all; the tick stays the clock (ADR-009).
3. **The executable's `--ai-matches` switch plays seeds 1 to 10.** Both seats are the AI with the packaged settings.
   - Each match is stepped a tick at a time until it ends or reaches 120 minutes. Both AIs get each tick's snapshot, and their commands apply at the next tick, so a match reproduces from its seed.
   - The matches run on as many threads as the machine has, each into a buffer of its own. They are written in seed order to `OutpostCommander-ai-matches.log` in the temporary folder, which each run replaces.
   - No window opens, and a message says when the run is done.
   - The code is `OutpostCommander/AiMatches.cpp`.
4. **`Tools/MatchLog.py` reports Phase 1's figures**:
   - a match's length against P1's 45 to 60 minutes;
   - the asteroids that ran dry;
   - for each player, the tiers it opened, its peak warship count, and its warships by design in each tier.

   `--ai-matches` reads the AI-against-AI log, names the players AI 1 and AI 2, and ends with every match's length: the median, the spread, and how many did not end.

## Consequences

- **The ten matches take 9 seconds** in the Linux container: clang 18 at `-O2` on four threads, building `AiMatches.cpp` against the real server and data.
- **They end at about 22 minutes, not 45–60.** All ten ended: the median is 21:50, and the spread 19:47 to 32:18. Seeds 11 to 40 give a median of 22:44, from 19:37 to 29:14, and all 40 together 22:11. In the 40 matches, every AI opens tier 2 at 15:04, but one, at 15:09, and none reaches tier 3 before the end. Over the 40, a side's peak is a median of 80 warships, from 62 to 116. These are clang's outcomes: floats are promised to replay only on the same build (ADR-009), so MSVC's build may play the same seeds differently.
- **Player 1 wins 30 of the 40 matches**, on a point-symmetric map with the same AI in both seats. That is a seat bias, and its cause is not known. It is not the order the server applies the two players' commands: connecting player 2 first gives the same 30 to 10. P1's AI-against-AI figure is clean only once the bias is understood or removed.
- **`MatchLogTests` check the records**: the tier, the 30-second counts, the peak and a dry asteroid written once. `InProcessServerTests.StepsOneTickAtOnce` checks `Step`, and `RunsItsTicksOnItsOwnThread` checks that a started server refuses it.
- **The switch has no test in CI.** No test project may include both the AI and the log. It was run in the container. The message and the switch itself have not run on Windows.

## What this forecloses

- AI-against-AI matches in CI.
- A record not listed here or in ADR-020, without a new decision.

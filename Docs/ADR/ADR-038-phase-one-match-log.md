# ADR-038 — The match log records the match for the owner, a server steps a tick at a time, and --ai-matches plays AI against AI headlessly

Status: **accepted** · 2026-10-07
Owns: match-log, server-step, ai-matches-switch, match-log-report

## Context

The owner's runs, and the figures of matches between two AIs, are read from a log of each match: how long it lasts and how it ends, what each side researches and builds and when, how large the fleets grow, where and when the sides fight, and how the territory, the structures' levels, the pirates, salvage and repair play out. A client sees all of that in its snapshots, so the log can be kept from snapshots alone, as any client keeps its view.

A figure of matches between two AIs has to repeat from its seeds. Three things stand in the way:

- **The server is paced by wall time.** Started, it ticks on its own thread at 20 Hz (ADR-025), so a 60-minute match takes 60 minutes.
- **Its timing is not reproducible.** The AI reads snapshots on the client's thread, so when its commands land depends on timing, and the match does not reproduce from its seed.
- **Only the executable can hold both halves.** The AI is in `Opponent` and the log is in `GameApp`. Of the projects that run the server, only the executable may include both (AGENTS.md §2): `GameLogicTests` may not include `GameApp`.

## Decision

1. **The log's other records,** beside decision 5's, each one line with every time in ticks. Records are only ever added, so an older log still reads.
   - `matchup <n>`: after the match's first line, when the match is a battle matchup ([ADR-083](ADR-083-battle-matchups.md) decision 6).
   - `tier <tick> player <player> tier <tier>`: the first snapshot in which the player's Research Lab has opened the tier by its level.
   - `fleet <tick> player <player> warships <count>`: every 30 seconds, from the player's own snapshots.
   - `dry <tick> asteroid <id>`: once, when a snapshot first shows the asteroid's reserve at zero.
   - `peak <tick> player <player> warships <count>`: the most warships the player had at once, and when it first had them. Written for each player just before `end`, or before `left` when the match is left.
   - `contact <tick> sector <id>`: the match's first shot, in the sector it was fired from, or 0 for none.
   - `engagement <tick> sector <id>`: ships of both sides fired in one sector within 10 seconds of each other. Another engagement there counts only after 30 seconds without a shot in it. A shot is shown to each player who sees its shooter or its target (ADR-024), so a tick's shots are counted once by shooter and target. The side that fired is the shooter's owner, or the side its target is not on when the snapshot does not show the shooter. A shot by or at a pirate is neither a contact nor part of an engagement, which are the players' ([ADR-073](ADR-073-pirates.md)); the `pirates` record counts it.
   - `sector <tick> sector <id> holder <player or 0>`: a sector's holder changed, the homes' at the start included. Both players see the territory alike (ADR-056 decision 10).
   - `tickets <tick> player <player> tickets <count>`: with each `fleet` record, on a map with territory.
   - `upgrade <tick> player <player> structure <id> <kind> level <level> <started, finished or lost>`: from the owner's own snapshots, the first in which a level is under way, the first that shows it in, and the one whose destroyed structures show the structure lost with it under way ([ADR-064](ADR-064-structure-upgrades.md)).
   - `attacked <tick> player <owner> structure <id> <kind> level <level>`: the first shot at a structure at a level above 1, once for each structure and level, from the snapshot that shows the shot and its target.
   - A structure's kind is one word: `station`, `shipyard`, `lab`, `rig`, `platform`, `relay` or `bay`.
   - `stall <tick> ticks <count>`: written with the peaks on a map with territory, the ticks during which both players were at their node caps, a Relay site counting, and held as many nodes as each other. A tick counts once both players' snapshots of it are in.
   - `salvaged <tick> player <player> derelict <id> ore <ore>`: a derelict that one of the player's Constructors was within 30 m of in one of its snapshots, beyond both footprints, and that is gone from its next. A derelict leaves only by its salvage ([ADR-074](ADR-074-salvage.md)), and a player sees what its Constructors are at, so the snapshots tell who salvaged it without the server saying. The 30 m is the server's 20 m reach for work and a margin for a snapshot's age. Two players' crews at one derelict would both count.
   - `pirates <tick> player <player> sector <id>`: the first shot between the player and the pirates in a sector, in the sector of the pirates' end of the shot, once for each player and sector.
   - `cleared <tick> sector <id>`: a sector the pirates guarded no longer is.
   - `retreat <tick> player <player> ship <id>`: one of the player's warships turned for home to be repaired ([ADR-075](ADR-075-repair-and-retreat.md)), from its own snapshots, each time it does.
   - `repaired <tick> player <player> ship <id>`: a warship's retreat ended with it whole. One ended by an order is neither.
   - `again <tick> player <player> ship <id>`: a repaired warship fired, at a player or the pirates, the first time since its repair.
   - `ending <tick> <production, domination, fleet or time>`: how the match ended, just before `end`: a match by the loss of production or by domination ([ADR-057](ADR-057-domination.md) decision 6), a battle matchup by a fleet destroyed or its time run out ([ADR-083](ADR-083-battle-matchups.md) decision 3).
2. **`Server::Step` runs one tick now, on the caller's thread.** It applies the commands that have arrived and sends each player its snapshot. A started server refuses it. A headless run steps the server instead of starting it, so no wall time enters it at all; the tick stays the clock (ADR-009).
3. **The executable's `--ai-matches` switch plays seeds 1 to 10.** Both seats are the AI with the packaged settings. Its options name other seeds, each AI's settings and the log, and `--quiet` runs it for a script without a message ([ADR-063](ADR-063-self-play-probe.md) decisions 1 and 2).
   - Each match is stepped a tick at a time until it ends or reaches 120 minutes. Both AIs get each tick's snapshot, and their commands apply at the next tick, so a match reproduces from its seed.
   - The matches run on as many threads as the machine has, each into a buffer of its own. They are written in seed order to `OutpostCommander-ai-matches.log` in the temporary folder, which each run replaces.
   - No window opens, and a message says when the run is done.
   - The code is `OutpostCommander/AiMatches.cpp`, and the options it is given are read in `Opponent/AiMatchesOptions.cpp` ([ADR-063](ADR-063-self-play-probe.md)).
4. **`Tools/MatchLog.py` reports the log.** It shows the last match, or every match with `--all`. `--ai-matches` reads the AI-against-AI log instead, shows every match, and names the players AI 1 and AI 2.
   - **For each match:** its length, its outcome and how it ended; its first shot, and its engagements before minute 20 and the sectors they were in; the nodes each player holds at the end; the asteroids that ran dry; on a map with territory, the stall and the structures above level 1 attacked; and with the pirates', salvage's and repair's records, the players' first shot at each other, the outposts cleared and the engagements before minute 40.
   - **For each player:** its research with the time it finished, the tiers it opened, its peak warship count, the levels it finished and the upgrades it lost with their structure; with those records, what it salvaged and fought of the pirates before the players' first shot at each other, its warships at minute 20, and the warships it turned for home, had repaired and saw fight again, each counted once; and its warships by design in each tier, and in windows of the match, 5 minutes long unless `--window-minutes` says otherwise.
   - **Over more than one match:** the lengths' median and spread, the matches not ended, each battle matchup's wins for player 1 ([ADR-083](ADR-083-battle-matchups.md) decision 6), and the median of the sides' peaks. Then come the figures the owner's runs are scored by, each against the threshold the script holds and under the labels it prints: the first shot by minute 5, the engagements and their sectors before minute 20, the endings, the stall, the levels and tier 3 reached, the structures above level 1 attacked, and over the matches with the pirates', salvage's and repair's records, a side's salvage and pirate fights before the first shot, the larger side's peak and its warships at minute 20, the warships a side turns for home and sees fight again, the engagements before minute 40, and how many matches ended, and how.
   - It leaves the pirates out of the players, though their warships are written as `built`.
5. **Every match against the AI is added to `OutpostCommander-matches.log` in the temporary folder,** by `Outpost::MatchLog` in `GameApp`, which reads only snapshots, as any client does: every skirmish and battle matchup the game starts, but not a run under `--load` or `--stress`, nor a joined world. Its base records are:
   - `match seed <seed> ticks_per_second <rate>`, first;
   - `research <tick> player <player> topic <id> <name>`: each player's research as it finishes;
   - `built <tick> player <player> hull <id> drive <id> weapon <id> <names>`: each warship as it first appears, by its components and its module;
   - `end <tick> winner <player, or 0 for a draw>`, or `left <tick>` when the match is left before it ends.

   `Tools/MatchLog.py` prints, for each match, the research times and the designs each side built in 5-minute windows, with the figures of decision 4, the match's length among them.

## Consequences

- **`MatchLogTests` check the records**: the base records, a ship's module, a matchup and how it ended, and a match left early; the tier, the 30-second counts, the peak and a dry asteroid written once; the first shot, an engagement counted once across both players' snapshots and again only after the gap, a sector's holder written when it changes, the tickets, and the ending; an upgrade started, finished and lost, a structure above level 1 attacked once, and the stall counted once a tick; a fight with the pirates counted once and apart from the players', a sector cleared, a salvage credited to the crew's player and not to one who only saw it, and a warship's retreat, repair and next shot. `InProcessServerTests.StepsOneTickAtOnce` checks `Step`, and `RunsItsTicksOnItsOwnThread` checks that a started server refuses it.
- **A seed's match repeats on one build.** Floats replay only on the same binary (ADR-009), so another build may play the same seeds differently, and a figure of AI-against-AI matches is read on the build that played them.
- **The switch has no test in CI.** No test project may include both the AI and the log.

## What this forecloses

- AI-against-AI matches in CI.
- A record not listed here, without a new decision.

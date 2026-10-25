# ADR-082 — A world keeps a log beside its saves, and the world run kills a world and compares it with one never killed

Status: **accepted** · 2026-10-08
Owns: world-log, world-log-battles, world-run

## Context

A world needs two instruments: one that says how it went, and one that shows it survives being killed.

- **A world log,** as [ADR-038](ADR-038-phase-one-match-log.md)'s match log is for a match. It records each save and how long it took, each restart and recovery, each hand-over between a player and its deputy, each scheduled order fired, each loss and restart, every hour each seat's bank, income, fleet, nodes and research, and each battle with whether its players were present.
- **`--world-run`,** beside `--ai-matches` ([ADR-063](ADR-063-self-play-probe.md)). It plays a world with an AI in every seat as fast as the processor allows, kills and recovers it at random ticks, and compares it at the end with the same world run straight through.

[ADR-077](ADR-077-world-state-on-disk.md) brings a world back from its newest save and its command log. A server made afresh has none of what the last one held in memory: its hosted players' memory and the orders they had not sent are gone. The command log also records only ticks that had commands, so a world killed after quiet ticks comes back at the tick after its last logged command, or at its save if that is later.

## Decision

1. **The world's server writes its log** (`WorldLog`) to `World.log` in the world's folder, appending, a line a record with every time in ticks. The records are listed in `WorldLog.h`: `start`, `save`, `seat`, `order`, `lost`, `waiting`, `restart`, `hour` and `battle`.
   - It reads what the server knows after each tick: each connection's snapshot, built once for both its player and the log, and who played the seat (`SeatPlay`).
     - A seat whose deputy plays it is the deputy's.
     - A seat its player plays, or a loopback client, is the player's.
     - A hosted player without a seat is an AI empire's.
   - A save's time is how long encoding it held up the server's thread. The folder writes it on a thread of its own, which the log does not time.
2. **A battle is the match log's engagement between two seats,** in the sector a shot was fired from:
   - Each seat's own shots at another seat's ship or structure, from its own snapshot, within 10 seconds of each other.
   - It ends after 30 seconds with no such shot there.
   - A side is present when its player played the seat at any of its shots in the battle, and is otherwise its deputy or the AI.
   - Shots at or by the pirates are not a battle.
3. **The hour's record** is the seat's Ore, income in hundredths a second, command points against its fleet cap, nodes held and topics researched, as its snapshot of that tick shows them. With them come the seconds of the hour its player and its deputy played it.
4. **The world run is `RunWorlds`**, in `GameLogic`, which the dedicated server's `--world-run <folder> [--seed] [--hours] [--kills] [--ai]` drives with an AI empire in every seat. The default is 24 hours of world, 10 kills and the Normal AI.
   - **The killed world runs first.** It is stepped on one thread. At each kill, its server is destroyed between two ticks and made afresh from its folder (`CreateWorldServer`), with its AI empires made anew. The tick it comes back at is recorded.
   - **The straight world runs after it,** in a folder of its own. Its AI empires are made anew at each tick the killed world came back at, with the orders they had not sent dropped (`InProcessServer::ReplaceHosted`). The two worlds differ only in what the folder kept.
     - By construction, no order is waiting at those ticks: one sent after a tick would have been logged at the next, and the killed world came back after its last logged command.
     - The drop keeps the comparison honest if the log ever changes.
   - **At the end the two simulations compare equal or not** (`Simulation::operator==`, ADR-009 decision 8). The switch exits with failure when they do not.
   - **The kills' ticks come from the seed** by SplitMix64, so a run reproduces on any platform.
5. **`CreateWorldServer`** makes a server as `CreateInProcessServer` does, as `InProcessServer`, for what needs more of it than `Server` shows.

## Consequences

- **Tests.**
  - `WorldLogTests`: each seat record, in order, from a scripted hour, with the hour's figures. A battle led by the player and fought on by the deputy is present; one fought by the deputy alone is the deputy's; fire too far apart is no battle. A world's server writes its start, a save a minute, its AI empires' seats, and, made afresh, where it came back from.
  - `WorldRunTests`: three minutes of world, killed three times, ends equal to the world run straight through, and a folder that holds a run already is refused.
  - Dropping the replaced players' waiting orders is not caught by a test, since none waits at those ticks (decision 4).
- **A killed world comes back at most a save's interval back.** A tick without a command writes nothing to the command log, so the folder comes back at the tick after its last logged command, or at its last save if that is later ([ADR-077](ADR-077-world-state-on-disk.md) decision 8). The run records each tick it came back at.
- **A kill in the world run is the server destroyed between ticks,** with the folder's writer finishing the save it was handed. A process killed while a save is written loses that save, and recovery comes back from the one before. `WorldFolderTests` cover that path, and the run does not.
- **The world log is not saved:** a recovery writes `start ... recovered`, and a battle the server did not see begin is not one it records.

## What this forecloses

- A world log that the client writes, or one that reads anything the server does not send its players, without a new decision.
- A world run whose worlds differ in more than what the folder kept.

# ADR-082 — A universe keeps a log beside its saves, and the universe run kills a universe and compares it with one never killed

Status: **accepted** · 2026-10-08
Owns: universe-log, universe-log-battles, universe-run

## Context

A universe needs two instruments: one that says how it went, and one that shows it survives being killed.

- **A universe log,** as [ADR-038](ADR-038-phase-one-match-log.md)'s match log is for a match. It records each save and how long it took, each restart and recovery, each hand-over between a player and its deputy, each scheduled order fired, each loss and restart, every hour each seat's bank, income, fleet, nodes and research, and each battle with whether its players were present.
- **`--universe-run`,** beside `--ai-matches` ([ADR-063](ADR-063-self-play-probe.md)). It plays a universe with an AI in every seat as fast as the processor allows, kills and recovers it at random ticks, and compares it at the end with the same universe run straight through.

[ADR-077](ADR-077-universe-state-on-disk.md) brings a universe back from its newest save and its command log. A server made afresh has none of what the last one held in memory: its hosted players' memory and the orders they had not sent are gone. The command log also records only ticks that had commands, so a universe killed after quiet ticks comes back at the tick after its last logged command, or at its save if that is later.

## Decision

1. **The universe's server writes its log** (`UniverseLog`) to `Universe.log` in the universe's folder, appending, a line a record with every time in ticks. The records are listed in `UniverseLog.h`: `start`, `save`, `written`, `tick`, `dropped`, `join`, `seat`, `order`, `lost`, `waiting`, `restart`, `hour` and `battle`.
   - A `restart` says how long past its hour the seat waited for its start to be free ([ADR-084](ADR-084-universe-without-an-end.md) decision 4), from the restart tick its snapshot carried the tick before. A server that came back in the tick the seat restarted has not seen it, and leaves the wait out.
   - It reads what the server knows after each tick: each connection's snapshot, built once for both its player and the log, and who played the seat (`SeatPlay`).
     - A seat whose deputy plays it is the deputy's.
     - A seat its player plays, or a loopback client, is the player's.
     - A hosted player without a seat is an AI empire's.
   - A save's time is how long encoding it held up the server's thread. The folder writes it on a thread of its own, which times the write from opening its file to renaming it into place, and the next tick writes that as a `written` record. The write reaches the system's cache, not the disk.
   - **The server's own records,** for the owner and the universe's overseer:
     - A minute's longest tick is a `tick` record when it took `UniverseLog::LONG_TICK`, 5 ms, the tick's budget, or more: its tick and time, how many of the minute's ticks took that long, and each part of it (`TickPart`) that took a quarter of it or more, longest first.
     - The ticks a minute dropped after stalls, which `TickHost` counts and never runs ([ADR-009](ADR-009-deterministic-core.md)), are a `dropped` record.
     - Both are written at the minute's last tick, so a server whose every tick is long writes one record a minute, not twenty a second.
     - What came of each connection is a `join` record with the address it came from: a hello welcomed; one refused for another build, with the layout it named, for a seat it may not take, as malformed, or for no hello in time; and a seat's connection gone, taken by a newer one, closed by the game with its code, or lost without a word. One the server closed itself is the refusal's record alone, and a seat's connection its channel closed for a broken framing is refused as malformed. A close whose code never reached the server, as one the game makes and drops at once may be, reads as lost: the record is what the server saw. A first message over a hello's limit is closed by the channel before the server reads it, and has no record. MsQuic's threads keep the records, and the server's thread writes them at its next tick ([ADR-060](ADR-060-quic-in-process.md) decision 7).
2. **A battle is the match log's engagement between two seats,** in the sector a shot was fired from:
   - Each seat's own shots at another seat's ship or structure, from its own snapshot, within 10 seconds of each other.
   - It ends after 30 seconds with no such shot there.
   - A side is present when its player played the seat at any of its shots in the battle, and is otherwise its deputy or the AI.
   - Shots at or by the pirates are not a battle.
3. **The hour's record** is the seat's Ore, income in hundredths a second, command points against its fleet cap, nodes held and topics researched, as its snapshot of that tick shows them. With them come the seconds of the hour its player and its deputy played it, and the commands the server applied for the seat in the hour, whoever gave them, and refused it ([ADR-094](ADR-094-a-refused-command-is-told.md)).
   - When the seat's player was connected over QUIC at any of the server's samples in the hour ([ADR-004](ADR-004-quic-transport.md) decision 6), the record ends with the connection's round trip at the median and the 95th percentile of those samples, by the nearest rank, the packets sent and lost in the hour, its largest backlog, and the bytes it sent in the hour. A sample whose connection sent fewer packets than the one before is a newer connection, counted from its own start.
   - A record without them reads as before, so `Tools/UniverseLog.py` reads a log written before them, and reports a seat's connection over the hours that have it.
4. **The universe run is `RunUniverses`**, in `GameLogic`, which the dedicated server's `--universe-run <folder> [--seed] [--hours] [--kills] [--ai]` drives with an AI empire in every seat. The default is 24 hours of a universe, 10 kills and the Normal AI.
   - **The killed universe runs first.** It is stepped on one thread. At each kill, its server is destroyed between two ticks and made afresh from its folder (`CreateUniverseServer`), with its AI empires made anew. The tick it comes back at is recorded.
   - **The straight universe runs after it,** in a folder of its own. Its AI empires are made anew at each tick the killed universe came back at, with the orders they had not sent dropped (`InProcessServer::ReplaceHosted`). The two universes differ only in what the folder kept.
     - By construction, no order is waiting at those ticks: one sent after a tick would have been logged at the next, and the killed universe came back after its last logged command.
     - The drop keeps the comparison honest if the log ever changes.
   - **At the end the two simulations compare equal or not** (`Simulation::operator==`, ADR-009 decision 8). The switch exits with failure when they do not.
   - **The kills' ticks come from the seed** by SplitMix64, so a run reproduces on any platform.
5. **`CreateUniverseServer`** makes a server as `CreateInProcessServer` does, as `InProcessServer`, for what needs more of it than `Server` shows.

## Consequences

- **Tests.**
  - `UniverseLogTests`: each seat record, in order, from a scripted hour, with the hour's figures, its commands and refusals, and an hour's connection, across a newer connection, from scripted samples. A battle led by the player and fought on by the deputy is present; one fought by the deputy alone is the deputy's; fire too far apart is no battle. A minute's longest tick and its dropped ticks, from scripted timings, and every kind of join, from scripted records. A universe's server writes its start, a save a minute and its write, its AI empires' seats, and, made afresh, where it came back from.
  - `UniverseFolderTests.TimesEachWrite`: each save written, timed, and handed over once; `TickHostTests.CountsTheTicksItDrops`.
  - `QuicTransportTests.LogsEveryJoin`, over a real connection: another build's hello, a wrong token, a welcome, a seat taken again, a connection the game closes and one that says no hello. QUIC runs in CI's Windows run only.
  - `UniverseRunTests`: three minutes of a universe, killed three times, ends equal to the universe run straight through, and a folder that holds a run already is refused.
  - Dropping the replaced players' waiting orders is not caught by a test, since none waits at those ticks (decision 4).
- **A killed universe comes back at most a save's interval back.** A tick without a command writes nothing to the command log, so the folder comes back at the tick after its last logged command, or at its last save if that is later ([ADR-077](ADR-077-universe-state-on-disk.md) decision 8). The run records each tick it came back at.
- **A kill in the universe run is the server destroyed between ticks,** with the folder's writer finishing the save it was handed. A process killed while a save is written loses that save, and recovery comes back from the one before. `UniverseFolderTests` cover that path, and the run does not.
- **The universe log is not saved:** a recovery writes `start ... recovered`, and a battle the server did not see begin is not one it records.

## What this forecloses

- A universe log that the client writes, or one that reads anything the server does not send its players, without a new decision.
- A universe run whose universes differ in more than what the folder kept.

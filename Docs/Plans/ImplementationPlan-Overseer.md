# Outpost Commander — The Universe Overseer

Status: **open** · From the owner's request of 2026-10-27 to start building [the overseer's proposal](UniverseOverseer.md) · Its gates are the proposal's, D1 to D10, of which D1 and D4 to D10 are decided

The overseer is the dedicated server's management console: a separate application, with a channel of its own to the server, that oversees the whole universe (Game §session.server). What it shows, how it reaches the server and what the server has to measure for it are [the proposal](UniverseOverseer.md)'s. This plan is the order the work is done in. It is a work queue, not an authority: where it disagrees with a design, AGENTS.md or an ADR, those win and this plan gets fixed.

---

## How an agent uses this plan

1. **Read AGENTS.md, then the proposal and the ADRs the task names.**
2. **Take the lowest-numbered task whose status is `todo` and whose dependencies and gates are `done` or decided.** One PR per milestone (owner, 2026-09-30), in the milestone order below.
3. **ADRs are edited in place** (AGENTS.md §6), in the same commit as the change that implements them.
4. **QUIC is checked in CI's Debug|x64 run only** (ADR-060), since the Linux container has no Schannel. A task that changes `QuicChannel`, `QuicListener` or the server's seats is verified there, and says so; the container runs the rest.
5. **A task that touches rendering, input or presentation is run, not only built**, and the owner's run accepts it.

Task numbers carry the prefix OV, and the gates are the proposal's, lettered D.

---

## Task board

| Task | Title | Depends on | Gate | Status |
|---|---|---|---|---|
| OV1.1 | The minute's longest tick and its dropped ticks are logged | — | — | merged, [#143](https://github.com/Zwaliebaba/Outpost.Commander/pull/143) |
| OV1.2 | A seat's orders, refusals and bytes are in its hour | — | — | merged, [#143](https://github.com/Zwaliebaba/Outpost.Commander/pull/143) |
| OV1.3 | Every join and every link's end is logged | — | — | merged, [#143](https://github.com/Zwaliebaba/Outpost.Commander/pull/143) |
| OV1.4 | A save's write is timed | — | — | merged, [#143](https://github.com/Zwaliebaba/Outpost.Commander/pull/143) |
| OV2.1 | The overseer's listener and `Overseer.json` | OV1 | D4 | merged, [#156](https://github.com/Zwaliebaba/Outpost.Commander/pull/156) |
| OV2.2 | `Health` once a second, and the process's figures | OV2.1 | — | merged, [#156](https://github.com/Zwaliebaba/Outpost.Commander/pull/156) |
| OV2.3 | The universe's view and the log's stream | OV2.1 | — | merged, [#156](https://github.com/Zwaliebaba/Outpost.Commander/pull/156) |
| OV2.4 | `Closing`, and the server's state | OV2.2 | — | merged, [#156](https://github.com/Zwaliebaba/Outpost.Commander/pull/156) |
| OV3.1 | The overseer's project, its window and its link | OV2 | D1, D5, D6 | merged, [#161](https://github.com/Zwaliebaba/Outpost.Commander/pull/161); awaiting the owner's run |
| OV3.2 | The overview: header, tiles, seat table and the log | OV3.1, the canvas plan's CV9.1 | D7 | merged, [#161](https://github.com/Zwaliebaba/Outpost.Commander/pull/161); awaiting the owner's run |
| OV4.1 | Twenty-four hours of the lanes' records at the welcome | OV3 | — | merged, [#162](https://github.com/Zwaliebaba/Outpost.Commander/pull/162) |
| OV4.2 | Needs attention and who played | OV4.1 | — | merged, [#162](https://github.com/Zwaliebaba/Outpost.Commander/pull/162); awaiting the owner's run |
| OV4.3 | The map: one system's sectors, full sight and a seat's | OV3 | — | merged, [#162](https://github.com/Zwaliebaba/Outpost.Commander/pull/162); awaiting the owner's run |
| OV5.1 | A day of the windows' records at the welcome, and each away seat's report | OV4 | D8, D9 | todo |
| OV5.2 | The header states | OV4 | — | todo |
| OV5.3 | The overseer's windows, and the server window | OV5.1 | D10 | todo |
| OV5.4 | The seat window | OV5.3 | D8, D9, D10 | todo |
| OV6.1 | Save now and Stop | OV5 | — | todo |
| OV6.2 | Drop link and New token | OV6.1 | D2, D3 | todo |

### Milestone order

OV1, OV2, OV3, OV4, OV5, OV6.

- **OV1 changes no message and nothing a universe saves,** so a universe made before it runs on after it. It is the server's own instrument, written to `Universe.log` whether or not an overseer ever connects, and it waits on no gate.
- **The process's figures are OV2.2's,** not OV1's as the proposal's §10 has it: nothing reads them before `Health` does.
- **OV2 changes no message of the game's and nothing a universe saves.** Its messages are a set of their own, and a universe made before it runs on after it, its settings taking the overseer's defaults.
- **The galaxy view comes with the galaxy** (the Horizon plan's milestone 42). Until then a universe is one system, and OV4.3's map is its sector map.
- **OV3 is built on the canvas** (gate D1), whose widgets, scroll view (CV9.1) and drop-downs are on `main` ([ADR-100](../ADR/ADR-100-the-canvas.md)), with the HUD and the minimap ported to it ([the canvas plan](ImplementationPlan-Canvas.md)'s CV8). OV3.2 adds the table to the canvas with its first use (gate D7). OV1 and OV2 need nothing of it.
- **OV3 was split in two** (owner, 2026-10-28), so that each is a pull request of a size the owner's run can accept: OV3 the overseer's project, window, link, header, tiles, seat table and log, and OV4 what needs attention, who played and the map. The seat window and the header states became OV5, and the actions OV6.

---

## Milestone OV1 — The server measures

### OV1.1 — The minute's longest tick and its dropped ticks are logged

- **Proposal:** §5, long ticks and ticks dropped.
- **Scope:**
  - `TickHost` counts the ticks it drops (`DroppedTicks`): those due beyond its bound after a stall, which the simulation never runs (ADR-009).
  - The universe log keeps each minute's longest tick, and at the minute's end writes it as a `tick` record when it took 5 ms or more, the tick's budget ([the budgets](../Budgets.md)): its tick, its time, how many of the minute's ticks took that long, and each part of it that took a quarter of it or more, longest first.
  - At the minute's end it writes the ticks dropped in it, when any were, as a `dropped` record.
  - A minute's records bound the log: a server whose every tick is long writes one `tick` record a minute, not twenty a second.
  - `Tools/UniverseLog.py` reports the long minutes, the longest tick and the part most often longest, and the ticks dropped.
- **ADR:** [ADR-082](../ADR/ADR-082-universe-log-and-universe-run.md) decision 1, the records.
- **Acceptance:** `TickHostTests.CountsTheTicksItDrops`; `UniverseLogTests.WritesTheMinutesLongestTickAndItsDroppedTicks`, from scripted timings: a quiet minute writes nothing, a minute with long ticks writes its longest with its parts, and dropped ticks are summed over the minute.
- **Verify:** the checkers; the container's tests; CI's Debug|x64 run.

### OV1.2 — A seat's orders, refusals and bytes are in its hour

- **Proposal:** §5, each seat's traffic and orders.
- **Scope:**
  - A seat's hour record counts the commands the server applied for it, whoever gave them, its player, its deputy or the AI, and those it refused (ADR-094), as `orders` and `refused` after the seconds its player and its deputy played it.
  - `QuicStatistics` carries the bytes the connection has sent, MsQuic's `SendTotalBytes`, and the hour's connection figures end with them as `sent_bytes`, counted across a newer connection as the packets are.
  - `Tools/UniverseLog.py` reports each seat's orders, refusals and bytes an hour.
- **ADR:** [ADR-082](../ADR/ADR-082-universe-log-and-universe-run.md) decision 3, the hour's record; [ADR-004](../ADR/ADR-004-quic-transport.md) decision 6, a connection's figures.
- **Acceptance:** `UniverseLogTests.WritesASeatsRecords` and `EndsAnHourWithItsConnection`, extended; `QuicChannelTests.ReportsItsConnection` checks the bytes sent, on both ends.
- **Verify:** the checkers; the container's tests; CI's Debug|x64 run for the QUIC test.

### OV1.3 — Every join and every link's end is logged

- **Proposal:** §5, joins.
- **Scope:**
  - A `QuicChannel` knows its peer's address, which the listener reads from MsQuic as the connection arrives (`PeerAddress`), and the code its own end closed the connection with (`LocalErrorCode`), as it knows the peer's (`PeerErrorCode`). It knows no game concept (R9).
  - `QuicListener::Sweep` returns the addresses of the silent connections it closed.
  - The universe log writes a `join` record for each hello's outcome, welcomed or refused and why, with the layout of the game that sent a hello of another build (ADR-060 decision 5); for each connection a newer one took a seat from (ADR-078 decision 4); for each seat's connection seen gone without the server closing it, closed by the game with its code or lost; and for each connection closed for saying no hello in time. Each names the address it came from.
  - MsQuic's threads queue the records for the server's thread, which writes them at its next tick (ADR-060 decision 7).
  - `Tools/UniverseLog.py` reports the joins by outcome, the refusals by reason and the addresses they came from.
- **ADR:** [ADR-082](../ADR/ADR-082-universe-log-and-universe-run.md) decision 1; [ADR-004](../ADR/ADR-004-quic-transport.md) decision 6; [ADR-060](../ADR/ADR-060-quic-in-process.md) decision 7.
- **Acceptance:** `UniverseLogTests.WritesEveryJoin`, from scripted records; `QuicTransportTests.LogsEveryJoin`: a welcome, a hello of another build, a wrong token, a seat taken by a newer connection, a connection the game closes, and one that says no hello, each written as its `join` record; `QuicChannelTests.KnowsItsPeer`.
- **Verify:** the checkers; the container's tests; CI's Debug|x64 run for the QUIC tests.

### OV1.4 — A save's write is timed

- **Proposal:** §5, saves.
- **Scope:**
  - The folder's writer times each save it writes, from opening the file to renaming it into place, and the server writes it at its next tick as a `written` record: the save's tick and the write's time. The write reaches the system's cache, not the disk: the time is what holding the save up costs the writer's thread.
  - `Tools/UniverseLog.py` reports the writes' median and longest.
- **ADR:** [ADR-082](../ADR/ADR-082-universe-log-and-universe-run.md) decision 1, which left the write untimed; [ADR-077](../ADR/ADR-077-universe-state-on-disk.md) decision 7, the writer.
- **Acceptance:** `UniverseFolderTests.TimesEachWrite`; `UniverseLogTests.AUniversesServerWritesItsLog` finds the first save's `written` record.
- **Verify:** the checkers; the container's tests; CI's Debug|x64 run.

### As built

- **Run in the container:** `NeuronServerTests` 12, `GameLogicTests` 337, `GameProtocolTests` 29 and `OpponentTests` 32 pass, built by clang 18 against stand-ins for the Windows headers, C++/WinRT and the test framework; the QUIC tests fail there as before, and `QuicTransportTests` and `ImpairedLink` are not built, since they need Windows sockets. Each new test that runs there was checked against a broken build: a tick of exactly 5 ms not counted, the bytes of a newer connection not counted from its own start, refusals not counted, and dropped ticks not counted.
- **Not run in the container:** `QuicChannelTests.KnowsItsPeer`, the bytes in `QuicChannelTests.ReportsItsConnection`, the addresses `SweepsAwayWhatHasGoneAndClosesWhatSaysNothing` now reads, and `QuicTransportTests.LogsEveryJoin`. `QuicChannel.cpp` and the two test files compile there against MsQuic 2.6.3's own headers. CI's Debug|x64 run of `b8b478d`, run 638, built the solution and ran every one of the 834 tests the suites list, these among them, and all passed; clang-tidy, clang-format and the documents' checks passed there too. The pull request's own run then failed `LogsEveryJoin` once: a game's connection dropped at once by `QuicTransport`'s destructor reached the server without its close code and was written as lost, where run 638 had seen it closed. The record is right to say what the server saw, and ADR-082 now says so; the test closes its connection with a code itself and waits for the close to go, so that it no longer leans on the timing.
- **The checkers:** `CheckFormat.py` on clang-format 18.1.3, `CheckProjectFiles.py`, `GenerateDocs.py --check` and `CheckCitations.py` pass; clang-tidy 18, with the repository's checks, finds nothing in the changed files. `RunClangTidy.py` itself needs a Developer PowerShell, so CI runs it.

---

## Milestone OV2 — The channel

The overseer's own channel to the server, as the proposal's §4 has it, but for the actions, which are OV6's. Nothing in OV2 draws: its messages are checked by tests that play the overseer's end, and OV3 is the first to show them.

### OV2.1 — The overseer's listener and `Overseer.json`

- **Proposal:** §4, the second listener, the certificate, the token and one overseer at a time; gate D4.
- **Scope:**
  - `Universe.json` takes the overseer's address, `overseerAddress`, `127.0.0.1` unless it names another, and its port, `overseerPort`, 45001 unless it names another. Settings without them read as those defaults, so a universe made before OV2 runs on. `--new-universe` takes `--overseer-address` and `--overseer-port`.
  - The overseer's messages are a set of their own in `GameProtocol` (`OverseerProtocol.h`), with the application protocol `outpost-overseer/1`, a version, and a layout whose hash a hello carries as the game's does (ADR-060 decision 5). The layout is the overseer's messages' followed by the game's, since the universe's view crosses as the game's own state messages (OV2.3): a change to either refuses an overseer of another build.
  - `Overseer.json` in the universe's folder (`OverseerTicket`): the host the overseer reaches, the port, the certificate's SHA-256 and a 128-bit token, as a join file holds a seat's (ADR-078 decision 8). The server makes the token on its first run and keeps it; it writes the file at each start, so that the host and the port follow the settings.
  - The server's end of the channel (`OverseerLink`) in `GameLogic`: a second `QuicListener` on the overseer's address and port, presenting the universe's kept certificate (ADR-078 decision 7). A hello's version and layout are read first, then its token. The newest connection that shows the token holds the channel, and the one before it is closed. The welcome says the game's wire layout, protocol and state version, the tick rate, the universe's folder, its seats, each a player's or an AI empire's with its difficulty, and its sectors and asteroid fields, which never change. It is made before the server starts and sent from MsQuic's thread, so that an overseer is welcomed while the server's thread is stuck.
  - The universe log writes `overseer <tick> opened from <address>`, `overseer <tick> closed from <address>` and `overseer <tick> refused <version, token, malformed or silent> from <address>` (gate D4). MsQuic's threads queue them for the server's thread, which writes them at its next tick, as it writes joins (ADR-060 decision 7).
  - `Tools/UniverseLog.py` reports the overseer's openings, its time open, and its refusals by reason and address.
- **ADR:** a new ADR for the overseer's channel; [ADR-078](../ADR/ADR-078-dedicated-server.md) decisions 1 and 2, the usage and the settings; [ADR-082](../ADR/ADR-082-universe-log-and-universe-run.md) decision 1, the records.
- **Acceptance:** `OverseerProtocolTests`: each message decoded as encoded, a hello of another version or layout read before it decodes, and the game's layout within the overseer's; `OverseerTicketTests`; `UniverseSettingsTests`, extended with the overseer's address and port and their defaults; `UniverseLogTests.WritesTheOverseersComingsAndGoings`; `OverseerLinkTests`: an overseer welcomed with the token, refused for another version, another token, a message that is no hello and silence, and the newest connection holding the channel, each written as its record, and the certificate the game's listener presents.
- **Verify:** the checkers; the container's tests; CI's Debug|x64 run for the QUIC tests.

### OV2.2 — `Health` once a second, and the process's figures

- **Proposal:** §4, `Health` and the threads; §5, the process and the listener's closes.
- **Scope:**
  - `Neuron::ProcessMeter` in `NeuronServer`: the process's working set and private bytes, its CPU since the last measure in hundredths of a core, its threads and handles, how long it has run, and the disk free under a folder, through `GetProcessMemoryInfo`, `GetProcessTimes`, `GetProcessHandleCount`, a Toolhelp snapshot and `GetDiskFreeSpaceExW`. It knows no game concept (R9).
  - The server's thread, each second of ticks, as it samples the seats' connections (ADR-004 decision 6), keeps for the main thread: the tick; each seat's player, who plays it, its link (an AI empire's none, a seat never taken, online since a tick, or gone since a tick and the tick its deputy takes it), its connection's figures, the commands applied for it and refused it in the second, its Ore, income, fleet against its cap, nodes and topics researched, and while it has lost the tick it restarts or that it waits; the battles under way; and the joins by outcome since the server started, with the connections the game's listener holds.
  - The main thread, once a second, sends `Health` while an overseer holds the channel: the wall time; each tick's time in the second, which lets the overseer take any percentile over any window, the parts summed over the second and the longest tick's largest part; the ticks dropped in the second; what the server's thread kept; and the process's figures. A stuck tick still sends `Health`, whose tick then stands still.
- **ADR:** the overseer's channel's.
- **Acceptance:** `ProcessMeterTests`; `InProcessServerTests` for what the server's thread keeps each second, from a server stepped by hand; `OverseerLinkTests.SendsHealthEverySecond`.
- **Verify:** the checkers; the container's tests; CI's Debug|x64 run for the QUIC test and `ProcessMeterTests`, whose calls the container stands in for.

### OV2.3 — The universe's view and the log's stream

- **Proposal:** §4, `UniverseView`, `LogRecord` and `WatchSight`.
- **Scope:**
  - `Simulation::BuildFullSight`: every entity in detail with its owner, every shot and destruction, and every sector with its holder and its state, under fog of war or not; no player's own figures. It changes nothing `Simulation` holds (R18).
  - After every tenth tick, while an overseer holds the channel, the server's thread sends the view: full sight, or the seat's own snapshot of the tick while `WatchSight` names that seat. It crosses as the game's own state message, without its catalogue and events, whole or as what changed since the newest the overseer says it holds (ADR-098), on a stream of its own that a newer view abandons (ADR-096). It is timed as a tick part of its own, `TickPart::Overseer`.
  - Every record the universe log writes goes to the overseer as the server's thread writes it, a tick's records in one message. One that connects is sent the log's last 500 records after its welcome, read from the end of `Universe.log`, so that they reach back before the server last started.
- **ADR:** the overseer's channel's; `Server.h`'s tick parts.
- **Acceptance:** `FogTests.FullSightSeesThroughTheFog`; `UniverseLogTests.ReadsItsLastRecords`; `OverseerLinkTests.SendsTheViewAndTheLog`: a view every tenth tick, put together by `HeldStates`, a seat's sight and back, and the log's last records then each new one.
- **Verify:** the checkers; the container's tests; CI's Debug|x64 run for the QUIC test.

### OV2.4 — `Closing`, and the server's state

- **Proposal:** §3, the header states; §4, `Closing`.
- **Scope:**
  - `Closing` says how the server ends: stopped by its owner (Ctrl+C, Ctrl+Break or its window closed), by Windows (signing out or shutting down), or by a failure, with the failure's message; and the tick of the universe's last save, which it comes back from. `ServerConsole` says which event asked for the stop. `OutpostServer` sends it before the server goes, and before it reports a failure the server's thread met; the link then waits, a second at most, for the overseer to have it. A failed assertion ends the process at once and sends nothing (ADR-093).
  - No state says the server is recovering (owner, 2026-10-27): it replays its command log before its listeners exist, at most a minute of commands since its last save, so the overseer reads No word meanwhile. The welcome says what the server came back from instead: a new universe, or a save's tick and the commands replayed.
  - Running, Stalled and No word are the overseer's to tell from `Health` (OV5).
- **ADR:** the overseer's channel's.
- **Acceptance:** `OverseerLinkTests.SaysHowTheServerEnds`, for a stop and a failure, and that the welcome says what the server came back from.
- **Verify:** the checkers; the container's tests; CI's Debug|x64 run for the QUIC test.

### As built

- **The channel** is [ADR-101](../ADR/ADR-101-the-overseers-channel.md), and [ADR-078](../ADR/ADR-078-dedicated-server.md) decisions 1 and 2 and [ADR-082](../ADR/ADR-082-universe-log-and-universe-run.md) decision 1 are edited for it. ADR-004 decision 6 needed no change: `Health` reads the seats' connections where the server already samples them.
- **`Health` carries each tick's time** since the last one, not the second's median, 99th percentile and longest as the proposal had it, so that the overseer takes any percentile over any window; the proposal's §4 now says so.
- **The acceptance's names as built:** `OverseerLinkTests.WelcomesAnOverseerWithTheToken`, `RefusesWhatIsNotItsOverseerAndLogsIt`, `GivesTheChannelToTheNewestOverseer`, `SendsHealthEverySecond`, `SendsTheViewAndTheLog` and `SaysHowTheServerEnds`; `InProcessServerTests.KeepsEachSecondForTheOverseersHealth`; `UniverseLogTests.WritesTheOverseersComingsAndGoings`, `ReadsItsLastRecords` and `TellsTheBattlesUnderWay`; `UniverseSettingsTests.ReadsWhereTheOverseerIsListenedFor` and `WritesTheOverseersFile`.
- **Run in the container:** `NeuronServerTests` 15, `GameProtocolTests` 37, `OpponentTests` 32 and `GameLogicTests` 345 pass, built by clang 18 against stand-ins for the Windows headers, C++/WinRT and the test framework; `ProcessMeterTests` ran there against stand-ins for its Windows calls, built from Linux's own figures. `KeepsEachSecondForTheOverseersHealth` was checked against two broken builds, one that counted a second's commands into the next and one that lost the dropped ticks, and failed each. The QUIC tests fail there as before, and `OverseerLinkTests` is not built there, since it needs MsQuic.
- **Not run in the container:** `OverseerLinkTests`, `ProcessMeterTests` against Windows itself, and `OutpostServer`, which the container does not build; `OutpostServer`'s two changed files and `OverseerLinkTests.cpp` compile there against the stand-ins. CI's Debug|x64 run 677, of `991592e`, built the solution, ran every suite and passed clang-tidy. Every new test passed there but `WelcomesAnOverseerWithTheToken`, which found that a log written on Windows ends each line with a carriage return, which `ReadLastRecords` kept; it strips it now, and `ReadsItsLastRecords` reads such a log, which fails without the fix. Run 684, of `b059cea`, passed all six jobs, every new test among them.
- **The checkers:** `CheckFormat.py` on clang-format 18.1.3, `CheckProjectFiles.py`, `GenerateDocs.py --check` and `CheckCitations.py` pass; clang-tidy 18, with the repository's checks, finds nothing in the changed files.

---

## Milestone OV3 — The overseer

The overseer itself: an executable of its own, its window and its end of the channel, and the overview's header, tiles, seat table and log. It is presentation: it is run against a running universe, and the owner's run accepts it.

### OV3.1 — The overseer's project, its window and its link

- **Proposal:** §6; gates D1, D5 and D6.
- **Scope:**
  - `OutpostOverseer`, a Win32 executable, not packaged (gate D5), in the solution for x64 and ARM64, Debug and Release, with ADR-002's row: it includes `NeuronCore`, `NeuronClient`, `GameProtocol` and `GameApp`, as gate D1 has it. `CheckProjectFiles.py`'s table and `.clang-tidy`'s header filter take it.
  - It finds `Overseer.json` (ADR-101 decision 2) from its argument, a file or the folder that holds it, or in the default universe's folder in the owner's documents, as `OutpostServer` finds the universe (ADR-078 decision 1), and says plainly when there is none.
  - Its window opens windowed and resizable, and Alt+Enter toggles full screen as the game's does; `Neuron::Window` takes where it starts. The renderer takes a hardware adapter only (gate D6): on a machine with none the overseer says so and ends.
  - Its end of the channel, in `GameApp`: a link that connects with the file's address and certificate, says hello, sends the sight and the views it holds, and joins again every 2 s while it has no connection; and a model that keeps what the server said, apart from QUIC, so that `GameAppTests` feeds it messages: the welcome, the newest `Health` and an hour of them, the log's records, and the newest view, put together with `HeldStates` and the welcome's sectors and fields.
  - Its frame loop draws the canvas built from the model, at the window's scale (ADR-006 decision 5; ADR-070).
- **ADR:** a new ADR for the overseer application; [ADR-002](../ADR/ADR-002-authoritative-server.md), its row; [ADR-006](../ADR/ADR-006-renderer-shape.md) decision 1, where its window starts.
- **Acceptance:** `OverseerModelTests`: a welcome, an hour of `Health` kept and no more, the log's records, views put together whole and as changes, the sight sent, a closing, and a connection gone and joined again; `CheckProjectFiles.py` with the new project; CI's build of it.
- **Verify:** the checkers; the container's tests for what it builds; CI; the owner's run against a running universe.

### OV3.2 — The overview: header, tiles, seat table and the log

- **Proposal:** §3, the overview; gate D7.
- **Scope:**
  - **The canvas's table** (gate D7), in `NeuronClient`: headings that sort on a press and show the sort's direction, a picked row in the picked card's face and edge, cells of a figure over a detail line, scrolling past its height on the canvas's scroll view; and the Style Guide's rule for a table.
  - **The overview,** built on the canvas in the game's look, which `GameApp` makes reachable from `Hud.cpp`:
    - The header: the universe's folder and address, the server's state, Running while `Health` comes and No word after 3 s without it (the other states are OV5's), universe time and tick, how long the process has run, the last save, the wire layout, protocol and state version, the overseer's own round trip, and what the server came back from.
    - The tiles: the tick's 99th percentile over the last minute against the 5 ms budget, with the hour's longest; ticks a second and ticks dropped; the last save, its size and encoding time, from the log's `save` records; the bytes out to every seat; the process's memory, CPU and disk.
    - The seat table: a row a seat, with the proposal's columns; filters as a row of cells; sorting by any heading; a picked row, whose sight the map shows (OV4.3); each seat in its color, seats 1 and 2 in their sets' colors and the rest in the overseer's own palette, clear of the warning's color (owner, 2026-10-28; ADR-085).
    - The universe log: its records as they come, filtered by kind, scrolled back through.
  - **The layout and contrast guards** hold over the overview at 1920×1080 and 1280×720, through a helper `HudTests` shares.
- **ADR:** the canvas's for the table ([ADR-100](../ADR/ADR-100-the-canvas.md)); the overseer application's for the overview; the Style Guide.
- **Acceptance:** `NeuronClientTests` for the table: sorting, its direction, picking, a figure over a detail line and scrolling; `OverseerScreenTests`: each region from a scripted model, a filter, a sort, a pick, the seats' colors, and the guards.
- **Verify:** the checkers; CI; run against a running universe at 1920×1080 and in a smaller window; the owner's run.

### As built

- **The application** is [ADR-103](../ADR/ADR-103-the-overseer-application.md); [ADR-002](../ADR/ADR-002-authoritative-server.md)'s diagram, table and boundary, [ADR-006](../ADR/ADR-006-renderer-shape.md) decision 1, [ADR-100](../ADR/ADR-100-the-canvas.md)'s theme and widgets, the Style Guide, AGENTS.md §2, the Glossary and Game §session.overseer are edited for it.
- **`Neuron::Window` also takes whether it holds the cursor** while full screen: the overseer has no edge scroll, and leaves it free.
- **The tiles are five, not six:** the process's working set, its share of a core and the disk free share one, so that every tile's detail line fits it on the reference frame.
- **A seat's color marks its row,** a bar at its left, and its words stay in the text's color: a side's color on the picked row's face would fall under 4.5:1.
- **The seat table's filter for what needs a look** came with what needs attention, OV4.2.
- **The log shows its newest records first,** so that a record stands at the top as it comes, with no scroll to follow; it shows 400 at most.
- **A table cell's figure and detail line stand at offsets the theme gives,** 5 and 25 units down a row of 46, so that the two never meet at a scale's rounding.
- **The guards' helper** is `Tests/GameAppTests/LayoutGuards.h`, which `HudTests` reads too.
- **The acceptance's names as built:** `OverseerModelTests`, seven; `OverseerClientTests.SaysHelloAndTheSightAndJoinsAgain`; `OverseerScreenTests`, eleven, `HoldsTheLayoutAndContrastGuards` among them; and `CanvasTests.SetsATableUnderItsHeadings` and `ScrollsATablesRowsUnderItsHeadings`.
- **Run in the container:** `NeuronClient`'s CPU half and `GameApp` but its GPU files, built by clang 18 against DirectXMath and stand-ins for Direct3D, DirectWrite, Windows and the test framework, with stand-in fonts of an even advance: `NeuronClientTests`' canvas, layout, display list and interaction suites, 45, pass; `GameAppTests`' overseer suites pass but `OverseerClientTests`, which needs MsQuic; and `HudTests` passes on `LayoutGuards.h` as it did before, but for `LaysOutTheOrdersWindow`, which fails there on the stand-in fonts before this milestone and after it. `OutpostOverseer`'s `WinMain.cpp` compiles there against stand-ins for the renderer's names.
- **Not run in the container:** the executable, `OverseerClientTests`, and the guards with the fonts Windows installs. CI's Debug|x64 run 700, of `0ba16ef`, OV3.1, built the solution with `OutpostOverseer` and passed every new test, the QUIC one among them; clang-tidy found that `OverseerModel`'s move could throw under MSVC's debug library, so it is not copyable and the tests fill one in place, and that a test's link was built as a static, now a function; `QuicChannelTests.CarriesTheNewestMessagesApart` failed there, which this milestone does not touch, as its four messages crossed before its fifth was sent. Run 702, of `b8ee9bf`, OV3.2, built the solution and passed every suite on both runners, `CarriesTheNewestMessagesApart` among them; clang-tidy asked for a test's rows to be reserved before its loop, which they are.
- **The checkers:** `CheckFormat.py` on clang-format 18.1.3, `CheckProjectFiles.py`, `GenerateDocs.py --check` and `CheckCitations.py` pass; clang-tidy 18, with the repository's checks, finds nothing in the changed files but what the stand-ins make.
- **The owner's run** against a running universe, at 1920×1080 and in a smaller window, is outstanding.

---

## Milestone OV4 — Attention, who played and the map

What the overview lacks after OV3: what needs the owner's attention, who played each seat over the last day, and one system's sector map, in full sight or the picked seat's. It was laid out before OV3 merged, at the owner's word to go on to OV5 (owner, 2026-10-28), and stands on OV3's branch.

### OV4.1 — Twenty-four hours of the lanes' records at the welcome

- **Proposal:** §3, who played; §4, the welcome.
- **Scope:**
  - **The welcome carries the lanes' records** (owner, 2026-10-28): the universe log's `seat`, `start` and `battle` records of the last 24 hours of universe time, and each seat's last hand-over before them, which says who played it as the day began; with the seconds of ticks between two saves, which what needs attention measures the last save by (OV4.2).
  - **The server reads them from the whole of `Universe.log` once,** as the link is made, and keeps them as the log writes them, so that an overseer that connects days later is told the day before it; a recovery's `start` drops what the log wrote past the tick it came back to. The rule is one helper, in `GameProtocol`, which the link and the overseer's model both keep their lanes by.
  - **The overseer's model keeps them,** the welcome's and every such record the stream brings, for the day that ends at the newest tick it was told of.
- **ADR:** [ADR-101](../ADR/ADR-101-the-overseers-channel.md) decisions 3 and 7; [ADR-103](../ADR/ADR-103-the-overseer-application.md) decision 6.
- **Acceptance:** `LaneRecordsTests`: a record's kind and tick, the day and each seat's hand-over before it, and what a recovery went back past; `OverseerProtocolTests` with the welcome's new fields; `UniverseLogTests.ReadsTheLanesRecords`; `OverseerLinkTests.WelcomesWithTheLanesRecords`, over QUIC; `OverseerModelTests.KeepsADayOfTheLanesRecords`.
- **Verify:** the checkers; the container's tests; CI.

### OV4.2 — Needs attention and who played

- **Proposal:** §3, needs attention and who played.
- **Scope:**
  - **What needs attention,** from rules over `Health` and the log whose thresholds live in one place (`OverseerAttention.h`): a chip for what to act on now, a game of another build refused within the hour, the disk under 5 GB, the last save older than two of its intervals (ADR-085 decision 1; ADR-066 decision 2); plain text for what to know, a seat's backlog grown for 30 s, a round trip over 250 ms at the 95th percentile of the last minute, loss over 2% in the last minute, a tick over 5 ms in the last minute, a seat that has waited over 10 minutes for its start, a deputy that has played a seat for over 24 hours, and every battle under way; and the checks that pass, so that a quiet list says all is well.
  - **The seat table's filter for what needs a look,** the seats that a rule names.
  - **Who played:** a lane a player's seat over the last day of universe time, solid in its color while its player played it and hatched while its deputy did, each recovery a mark across the lanes; and a lane of battles, each mark filled when a player was present and hollow when only deputies and AI fought. AI empires have no lane.
- **ADR:** ADR-103, the overview.
- **Acceptance:** `OverseerAttentionTests`: each rule firing and passing; `OverseerScreenTests`: the attention panel's chips, text and quiet list, the filter, the lanes, and the guards.
- **Verify:** the checkers; the container's tests; CI; the owner's run.

### OV4.3 — The map: one system's sectors, full sight and a seat's

- **Proposal:** §3, the map.
- **Scope:** the universe's one system as the minimap draws it, sharing its drawing (`Hud::DrawMinimap`): the sectors by holder, suppressed and cut off, and the ships, structures, ore and derelicts, in each seat's color as the seat table marks it. It shows full sight, or the picked seat's own, and an "All" cell over it gives full sight back (owner, 2026-10-27). No fog is drawn over a seat's sight: its snapshot holds only what the seat sees.
- **ADR:** ADR-103, the overview; [ADR-100](../ADR/ADR-100-the-canvas.md) decision 7, the minimap shared.
- **Acceptance:** `OverseerScreenTests`: the map's marks in full sight and in a seat's, the "All" cell, and the guards.
- **Verify:** the checkers; the container's tests; CI; the owner's run against a running universe.

### As built

- **The overseer's model takes no lanes' record twice.** The log's last records, which the server sends again after the welcome, held lanes' records the welcome gave and older ones the day had dropped, and an old `start 0 new` among them went back past every record of the day; a lanes' record the stream brings is now taken only when it is of the day and not yet kept.
- **A hand-over to the play a seat already has continues its span,** and only a recovery's `start` is a mark across the lanes: a new universe's is where its lanes begin.
- **The overview's body:** the middle column holds a row 360 units tall over the seat table, the map at its left, 330 units wide around a square of 306, and what needs attention beside it; the right column, 560 units wide, holds who played over the universe log.
- **Full sight carries the map's size,** which `Simulation::BuildFullSight` had left at zero, so that the map had nothing to be drawn to; a seat's snapshot carried it already.
- **The map's heading says whose sight the view shows,** from the view's own player, not the pick: a pick reads as full sight until the seat's first view comes.
- **The acceptance's names as built:** `LaneRecordsTests`, three; `UniverseLogTests.ReadsTheLanesRecords`; `OverseerLinkTests.WelcomesWithTheLanesRecords`; `OverseerModelTests.KeepsADayOfTheLanesRecords` and `TakesNoLanesRecordTwice`; `OverseerAttentionTests`, four; `OverseerScreenTests.ShowsWhatNeedsAttention`, `FiltersTheSeatsThatNeedALook`, `DrawsWhoPlayed` and `DrawsTheMap`, with `HoldsTheLayoutAndContrastGuards` run over a view; and `FogTests.FullSightSeesThroughTheFog`, with the map's size.
- **Run in the container:** the server's suites, and `GameAppTests`' overseer suites, pass on the clang 18 harnesses OV3 used, but the QUIC tests and `NeuronCoreTests`' two `FileSysTests`, which this milestone does not touch and the Linux harness fails; `HudTests.LaysOutTheOrdersWindow` still fails there on the stand-in fonts, at OV4's base as after it. The overview's display list at 1920×1080, written out as an image with the stand-in fonts, shows each region where it is laid out, the map's sector washes and marks in the seats' colors, and the lanes.
- **Not run in the container:** the QUIC tests, the executable, and the guards with the fonts Windows installs. CI's Debug|x64 run 709, of `03dc178`, built the solution and passed every suite on both runners; clang-tidy on MSVC's library found a span of the backlog's last seconds made from two iterators, and a lanes' record's kind read without its check, which it does not find on the container's library; the span is now the figures' last ones, and the kind is read with a default.

---

## Milestone OV5 — The seat window, the server window and the header states

What the overseer lacks after OV4: the server's state told whole in the header, and the two windows the proposal's §3 has, one a seat's and one the server's, which reach back over the day. One PR, laid out once OV4 merged (owner, 2026-10-28).

### OV5.1 — A day of the windows' records at the welcome, and each away seat's report

- **Proposal:** §3, the seat window and the server window; §4, the welcome and `Health`.
- **Scope:**
  - **The welcome's day of records reaches the windows' kinds** (gate D8): beside the lanes' `seat`, `start` and `battle`, each seat's `join` and `hour` records, and the server's `tick` and `dropped`, of the last day of universe time, which the server reads from the whole of `Universe.log` once and keeps as the log writes them, as OV4.1 keeps the lanes'. The lanes' records become the day's records under one helper, whose rule does not change: a recovery's `start` drops what was written past its tick, and each seat's last hand-over before the day stays. Saves, one a minute, are not among them: the log's last records hold hours of them.
  - **Each away seat's report rides in its `SeatHealth`** (gate D9): what the server keeps for its player while its deputy plays it (ADR-080 decision 7), the tick the deputy took it, ships and structures built and lost, sectors gained and lost, and the scheduled orders that fired. The server's thread keeps it with the seat's second.
  - **The overseer's model keeps the day's records** as it keeps the lanes', for the day that ends at the newest tick.
- **ADR:** [ADR-101](../ADR/ADR-101-the-overseers-channel.md) decisions 5 and 7; [ADR-103](../ADR/ADR-103-the-overseer-application.md) decision 6.
- **Acceptance:** the day's records' tests in `GameProtocolTests`, with joins, hours, long and dropped ticks kept for the day and a save not; `OverseerProtocolTests` with a seat's report; `UniverseLogTests`, the day's records read from a whole log; `InProcessServerTests.KeepsEachSecondForTheOverseersHealth`, with an away seat's report; `OverseerLinkTests`, the welcome's day of records, over QUIC; `OverseerModelTests`.
- **Verify:** the checkers; the container's tests; CI's Debug|x64 run for the QUIC test.

### OV5.2 — The header states

- **Proposal:** §3, header states.
- **Scope:** the header tells the server's state as the proposal's table has it: Running while `Health` comes and the tick moves; Stalled while `Health` comes and the tick has not moved for 2 s, since when and how much wall time has gone unrun; No word after 3 s without `Health` or without a connection, the last word's time and tick, and that the overseer joins again every 2 s; Stopped, once `Closing` said the owner or Windows stopped the server, when and the tick of its last save; and Failed, once `Closing` carried a failure, its message and the save it comes back from. Stopped and Failed hold until a connection's welcome.
- **ADR:** ADR-103 decision 7.
- **Acceptance:** `OverseerScreenTests`: each state from a scripted model, Stalled told from Running by the tick and from No word by `Health` still coming, and the guards.
- **Verify:** the checkers; the container's tests; CI; the owner's run.

### OV5.3 — The overseer's windows, and the server window

- **Proposal:** §3, the server window; §6, a window stack of its own.
- **Scope:**
  - **The overseer's windows** stand over the overview on a `WindowStack` of the overseer's own, as the game's do (ADR-100), dragged by their title bars and closed by their close boxes: the server window, and a seat window for each seat opened.
  - **The header's Server button opens the server window** (gate D10); OV6 sets Save now and Stop beside it.
  - **The server window:** the tick split by part over the hour of `Health` the overseer holds; the longest tick of each minute of it against the 5 ms budget; the day's long ticks from the `tick` records, each with the parts that took most of it, and the ticks dropped from the `dropped` records; the saves the log's records hold, each with its size, encoding and write; the game's listener's connections and its joins by outcome since the server started; and the process.
  - **Its charts are composites of the overseer's own,** bars filled on the canvas as who played's lanes are, so that the canvas gains no widget; the style guide gains their rule.
- **ADR:** ADR-103 decision 7.
- **Acceptance:** `OverseerScreenTests`: the Server button opening the window and its close box closing it, a drag, each section from a scripted model, and the guards with the window open.
- **Verify:** the checkers; the container's tests; CI; the owner's run.

### OV5.4 — The seat window

- **Proposal:** §3, the seat window.
- **Scope:**
  - **Picking a seat's row sets an action bar under it,** whose Seat button opens the seat's window (gate D10); OV6 sets Drop link and New token on it.
  - **The seat window:** the link over the last four hours, from the seat's `hour` records, the round trip at the median and the 95th percentile, loss, the largest backlog and the bytes sent of each hour, with the last hour heard a minute at a time from `Health`, and the time the link was down from its `join` records; every join of the last day with its outcome, the address it came from and the layout a game of another build named; the seat's hour records of the day; and what waits for its player, its report of the time away from `Health` (gate D9) and its scheduled orders, from its own view, which picking it watches.
- **ADR:** ADR-103 decision 7.
- **Acceptance:** `OverseerScreenTests`: the action bar under the picked row and its Seat button, each section from a scripted model, a seat with no records and one away, and the guards with the window open.
- **Verify:** the checkers; the container's tests; CI; the owner's run against a running universe.

---

## Later milestones

Each is laid out task by task when the one before it is merged, and its gates are decided.

- **OV6 — The actions:** Save now and Stop, then Drop link and New token as gates D2 and D3 decide.

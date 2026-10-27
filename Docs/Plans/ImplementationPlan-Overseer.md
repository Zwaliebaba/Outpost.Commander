# Outpost Commander — The Universe Overseer

Status: **open** · From the owner's request of 2026-10-27 to start building [the overseer's proposal](UniverseOverseer.md) · Its gates are the proposal's, D1 to D7, of which D1, D4 and D7 are decided

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
| OV1.1 | The minute's longest tick and its dropped ticks are logged | — | — | in review |
| OV1.2 | A seat's orders, refusals and bytes are in its hour | — | — | in review |
| OV1.3 | Every join and every link's end is logged | — | — | in review |
| OV1.4 | A save's write is timed | — | — | in review |
| OV2.1 | The overseer's listener and `Overseer.json` | OV1 | D4 | todo |
| OV2.2 | `Health` once a second, and the process's figures | OV2.1 | — | todo |
| OV2.3 | The universe's view and the log's stream | OV2.1 | — | todo |
| OV2.4 | `Closing`, and the server's state | OV2.2 | — | todo |
| OV3.1 | The overseer's project and window | OV2 | D1, D5, D6 | todo |
| OV3.2 | The overview: header, tiles, seat table, attention, who played, the log | OV3.1, the canvas plan's CV9.1 | D7 | todo |
| OV3.3 | The map: one system's sectors, full sight and a seat's | OV3.1 | — | todo |
| OV4.1 | The seat window, the server window and the header states | OV3 | — | todo |
| OV5.1 | Save now and Stop | OV4 | — | todo |
| OV5.2 | Drop link and New token | OV5.1 | D2, D3 | todo |

### Milestone order

OV1, OV2, OV3, OV4, OV5.

- **OV1 changes no message and nothing a universe saves,** so a universe made before it runs on after it. It is the server's own instrument, written to `Universe.log` whether or not an overseer ever connects, and it waits on no gate.
- **The process's figures are OV2.2's,** not OV1's as the proposal's §10 has it: nothing reads them before `Health` does.
- **The galaxy view comes with the galaxy** (the Horizon plan's milestone 42). Until then a universe is one system, and OV3.3's map is its sector map.
- **OV3 is built on the canvas** (gate D1), whose widgets are merged ([ADR-100](../ADR/ADR-100-the-canvas.md)), and waits on none of the windows the canvas plan still ports. Before OV3.2, [the canvas plan](ImplementationPlan-Canvas.md)'s scroll view, CV9.1, is merged, as a pull request of its own if nobody has taken it yet (owner, 2026-10-27); OV3.2 adds the table to the canvas with its first use (gate D7). The sector map shares the minimap's drawing, which the canvas plan ports last (CV8.3), so OV3.3 is laid out against whichever of the two stands then. OV1 and OV2 need nothing of it.

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

## Later milestones

Each is laid out task by task when the one before it is merged, and its gates are decided.

- **OV2 — The channel** (proposal §4): the second listener, `Overseer.json`, the messages but the actions, the `Overseer` tick part, and the process's figures. It changes no message of the game's.
- **OV3 — The overseer** (proposal §3 and §6): the project, its window, the overview and the map.
- **OV4 — The seat window, the server window and the header states.**
- **OV5 — The actions:** Save now and Stop, then Drop link and New token as gates D2 and D3 decide.

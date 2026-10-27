# The Universe Overseer — Proposal

Status: **proposal** · 2026-10-27 · From the owner's request of 2026-10-27 for a management console of the dedicated server: a separate application, with a channel of its own to the server, that oversees the whole universe, its health, where its players are, and what goes wrong in running it. Nothing here is decided until its ADRs, and the owner's questions are gates D1 to D7 (§9).

This document proposes what the overseer shows, how it reaches the server, what the server has to start measuring for it, and where it sits in the code. The mock-ups are on [the owner's canvas](https://claude.ai/artifact/FBWqgZ69SS3dSNa2r2phY4), which only the owner can open; every figure on them is illustrative, not measured, and where they differ from this document, as the map's sight does, this document holds.

---

## 1. What it is for

A universe runs for weeks on a dedicated server that nobody watches (Game §session.server). Today the server says what it does in its console window and in its universe log (Game §universe.log), and the owner learns that something went wrong when a friend says so. The overseer answers three questions at a glance, in this order:

1. **Is anything wrong now?** The server runs, keeps pace and saves; no seat's link is wearing down; nobody is refused at the door.
2. **Who is where?** Who plays each seat, its player or its deputy (Game §universe.handover); who is connected, from where and how well; and what each empire holds.
3. **What happened?** Who played when over the last day, which battles were fought with a player present, and the universe log as it is written.

It is the owner's instrument, not a player: it never sends a command to the simulation.

## 2. What the owner decided (2026-10-27)

- **A separate application with a channel of its own to the server** (owner, 2026-10-27). It earns its cost twice: it watches a server on a Windows Server VM (ADR-078), and it sees a server die or hang from outside, which a view in the server's own console window cannot report.
- **Full sight** (owner, 2026-10-27). The overseer sees every entity through every player's fog. ADR-002 applies fog on the server so that no client holds what its player cannot see, and the overseer is the one client that does: open beside the owner's own game, it shows the owner what the owner's rivals hide. The owner accepted that, and put it on the record: the universe log writes when the overseer is connected (gate D4, decided 2026-10-27).
- **It watches, and acts only on the server, never on the simulation** (owner, 2026-10-27): save now, stop, drop a seat's link, and issue a seat a new token. Nothing it does reaches `Simulation`, so a universe replays from its seed and its log as it does today (ADR-009, ADR-077). Giving Ore, changing an AI's difficulty or restarting a seat stay out, and so does a pause, which the horizon rules out (its §12).
- **Native, drawn with the game's own interface** (owner, 2026-10-27): a Win32 executable that draws with `NeuronClient`, in the game's look (Game §interface.style; [the style guide](../StyleGuide.md)). No web page: that would reopen ADR-001 and R14, as horizon O7 says.
- **Reached from the same machine or the LAN** (owner, 2026-10-27): its listener is on the loopback unless the owner names another address.
- **It assumes more than four seats** (owner, 2026-10-27): the seats are a table, and the map a galaxy, as gate G5 of [the Horizon plan](ImplementationPlan-Horizon.md) has a galaxy: a home system for each seat around shared systems in the middle.

## 3. What it shows

The interface is laid out on ADR-006's 1920×1080 reference frame and scaled as one, as the game's HUD is (ADR-006 decision 5), in a window the owner may resize.

### The overview

| Region | What it shows | Where it comes from |
|---|---|---|
| Header | The universe's folder and address; the server's state (§3, header states); universe time and tick; how long the process has run; the build's wire layout, the protocol and the state version; the overseer's own round trip; Save now and Stop server | `OverseerWelcome`, `Health` (§4) |
| Health tiles | The tick's 99th percentile over the last minute against the 5 ms budget ([the budgets](../Budgets.md)), with the hour's longest; ticks run a second and ticks dropped (§5); the last save, its size and encode time; bytes out to every seat; the process's memory, CPU and disk | `Health` |
| Map | The galaxy: each system a ring of its 25 sectors by holder, the lanes between systems, fleets on them, battles. A system opens into its sector map: holders, suppression and cut-off as the minimap draws them, ships, structures, ore and derelicts. Picking a seat's row in the seat table shows that seat's own snapshot instead of full sight, which is what a deputy's choice has to be judged by, and an "All" cell over the map gives full sight back: the choice takes no drop-down, which the canvas plan's gate C6 keeps to the orders form (owner, 2026-10-27) | `UniverseView`; the universe with one system shows its sector map alone |
| Seat table | A row a seat: who plays it (player, deputy, or an AI and its difficulty); status (online and for how long, away, link gone and when the deputy takes it, fallen and when it restarts); round trip, loss and backlog; orders a minute and refused; Ore and income; fleet against the cap; nodes; research. Filters as a row of cells (all, players, AI, away, needs a look), sorting by any heading (gate D7), and the map's sight and an action bar for the row picked. It scrolls when its seats run past its panel | `Health`, `UniverseView` |
| Needs attention | What to act on, as a chip; what to know, in plain text; and the checks that pass, so a quiet list says "all well" rather than nothing | Rules over `Health` (below) |
| Who played | A lane a player's seat over the last 24 hours: solid while its player played it, hatched while its deputy did, a gap while the server was down; and a lane of battles, each mark filled when a player was present and hollow when only deputies and AI fought. AI empires have no lane | The universe log's `seat`, `start` and `battle` records |
| Universe log | The log's records as they are written, filtered by kind, and scrolled back through | `LogRecord` |

**What needs attention.** A rule fires on `Health` and the log. A chip marks only what the owner could act on now (ADR-085 decision 1; ADR-066 decision 2): a seat refused for another build, the server's disk under 5 GB, a save older than two of its intervals. Plain text marks the rest: a seat whose backlog has grown for 30 s, a round trip whose 95th percentile passes 250 ms, loss over 2%, a tick over 5 ms, a seat that has waited over 10 minutes for its start (ADR-084 decision 4), a deputy that has played a seat for over 24 hours, and every battle under way. The thresholds are proposals, and live in one place in the overseer's code.

### The seat window

Opened from a row: the link over the last four hours (round trip at the median and the 95th percentile, loss, backlog and bytes out, with the time the link was down); every join of the last day with its outcome, the address it came from and the wire layout of the game that made it; the seat's hour records from the universe log; what waits for the player, its report of the time away and its scheduled orders; and the seat's actions.

### The server window

The tick split by part over the hour (Server.h's `TickPart`); the longest tick of each minute against the budget; the day's longest ticks, each with the part that took most of it; the saves; the listener's connections and its refusals by reason; and the process.

### Header states

| State | When | What it says |
|---|---|---|
| Running | `Health` arrives and the tick moves | universe time, uptime, the last save, and what the server came back from as it started: a new universe, or a save's tick and the commands replayed of those logged (ADR-077 decision 8). It replays them before its listeners exist, so that no state says it is recovering: meanwhile it reads as No word (owner, 2026-10-27) |
| Stalled | `Health` arrives, and the tick has not moved for 2 s | since when, and how much wall time has gone unrun: the server's thread may be stuck |
| No word | no `Health` for 3 s, or the link is gone | the last word's time and tick; the overseer joins again every 2 s; everything shown is as it was then |
| Stopped | `Closing` said the owner or Windows stopped it | when, and the tick of its last save |
| Failed | `Closing` carried a failure's message | the message, and the save it will come back from |

Stalled and No word are told apart because `Health` comes from the server's main thread and the tick from the server's thread (ADR-025): a stuck tick still sends `Health`, a hung main thread or a dead process sends nothing. A failed assertion ends the process at once (ADR-093), so it reads as No word, with no message.

## 4. The channel

- **A second listener.** `OutpostServer` listens for the overseer on a QUIC listener of its own, with the application protocol `outpost-overseer/1`, beside the game's `outpost-commander/1`. It listens on `127.0.0.1` unless `Universe.json` names another address, `overseerAddress`, and on port 45001 unless it names another, `overseerPort`; `--new-universe` takes `--overseer-address` and `--overseer-port`. A player's game cannot reach it, since it negotiates another protocol, and nothing reaches it from another machine unless the owner opens it (ADR-078 decision 6's listen addresses).
- **The same certificate.** The listener presents the universe's kept certificate (ADR-078 decision 7), so the overseer pins it once, as a player does.
- **A token, kept like the certificate.** The server makes `Overseer.json` in the universe's folder on its first run and keeps it: the host, the port, the certificate's SHA-256 and a 128-bit token from `std::random_device`, as a seat's (ADR-078 decision 3). On the server's machine the overseer reads it from the default universe's folder with no argument, as `OutpostServer` finds the default universe (ADR-078 decision 1); elsewhere the owner copies it. That is ADR-078's trust of a circle of friends: whoever reads the file has the overseer's powers.
- **One overseer at a time, and the newest connection holds it**, as a seat (ADR-078 decision 4).

**The messages** are a set of their own, declared in `GameProtocol` (`OverseerProtocol.h`) with a layout hash of their own, so the game's messages and their `WireLayoutHash` do not change (ADR-060 decision 5).

| From | Message | When | What it carries |
|---|---|---|---|
| overseer | `OverseerHello` | first | its version and layout hash, and the token |
| server | `OverseerWelcome` | once | the server's wire layout, protocol and state version; the tick rate; the seats, each a player's or an AI empire's with its difficulty; the systems and their sectors, which never change |
| server | `Health` | every second, from the main thread | the wall time; the tick reached; each tick's time in the second, which lets the overseer take any percentile, the parts summed, the longest tick's largest part, and the ticks dropped; each seat's controller, link figures, bytes out, commands applied and refused and its state; the process's figures; the listener's counts |
| server | `UniverseView` | twice a second, the newest winning (ADR-096) | every entity as an `EntityView` with its owner, each sector's holder and state, each battle under way: full sight, or one seat's snapshot while `WatchSight` names that seat; as what changed (ADR-098) |
| server | `LogRecord` | as the universe log writes each record, in order; the last 500 on connecting | the record's line as `Universe.log` holds it |
| server | `Closing` | before the process ends | how it ends, stopped by the owner, by Windows or by a failure, and the failure's message |
| server | `ActionResult` | after each action | done, or refused and why |
| overseer | `WatchSight` | when the owner picks | full sight, or a seat |
| overseer | `SaveNow`, `Stop`, `DropLink`, `NewToken` | when the owner presses | the seat, for the last two, and for `DropLink` whether its deputy takes it at once (gate D3) |

**Threads.** The listener hands each message to its Receiver on MsQuic's thread, which queues it for the main thread and does nothing else. The main thread, which already wakes every second (ADR-078 decision 1), builds `Health` from `Server::TakeTickTimings` and `Server::TakeSeatFigures`, which today it discards and never takes, and from the process; it carries out Stop as Ctrl+C does, and hands SaveNow to the server's thread. The server's thread builds `UniverseView` after every tenth tick at 20 Hz, and only while an overseer is connected, timed as a tick part of its own, `Overseer`. "Sight of" a seat costs nothing more: that seat's snapshot is built every tick already.

## 5. What the server must start measuring

The overseer shows nothing the server does not know. These it does not know or keep today, and each is the server's to measure whether or not an overseer is connected:

- **Ticks dropped.** `TickHost` drops a backlog beyond `MAX_TICKS_PER_ADVANCE`, 5 ticks, after a stall, so a server does not fall behind the wall clock: it loses the time. The count of dropped ticks is how much universe time a stall cost, and it is the "Pace" tile.
- **Each seat's traffic and orders:** the bytes sent, from MsQuic's own statistics, beside the figures of ADR-004 decision 6; the commands applied and refused (ADR-094) a minute; and the address the connection comes from (`QUIC_PARAM_CONN_REMOTE_ADDRESS`), which `QuicChannel` reports as text and knows no game concept for (R9).
- **Joins.** Each hello's outcome, welcomed or refused and why, with the wire layout the game sent (ADR-060 decision 5) and its address; and each connection's end, closed by the game, ended by the idle timeout, or taken by a newer connection (ADR-078 decision 4). They are a new universe-log record, `join`.
- **Long ticks.** A tick over 5 ms is a new universe-log record, `tick`, with its time and its largest part, so that a spike is told apart from a save, a battle or a path graph built whole.
- **Saves:** the write's own time, on the folder's thread, which ADR-082 decision 1 leaves untimed. A write that fails already ends the process; `Closing` now says why.
- **The listener's closes by reason:** a hello that did not come in time, a broken framing, a refusal.
- **The process:** its working set, CPU, threads and handles, and the disk free under the universe's folder, through `GetProcessMemoryInfo`, `GetProcessTimes`, `GetProcessHandleCount` and `GetDiskFreeSpaceExW`, all in the Windows SDK, so R14 holds.

The new records change ADR-082 decision 1's list, and `Tools/UniverseLog.py` reads them.

## 6. Where it sits

- **`OutpostOverseer`, a Win32 executable** (gate D1), with a row of its own in ADR-002's table: it includes `NeuronCore`, `NeuronClient`, `GameProtocol` and `GameApp`. Its model and its layout live in `GameApp`, which holds the game's look, `GameTheme` with its typefaces and sprites, and the minimap's drawing that its sector map shares, so `GameAppTests` lays them out without a GPU, as `HudTests` lays out the HUD; the executable is its shell. `GameTheme` stands in `Hud.cpp`'s anonymous namespace, and OV3.1 makes it reachable. Neither the game nor the server includes the overseer.
- **It is built on the canvas** ([ADR-100](../ADR/ADR-100-the-canvas.md)): `Neuron::Canvas`'s widgets, its `Theme`, `TextMetrics`, `WindowStack` and `DrawList`, and nothing of `Painter`, which the canvas plan deletes. Its windows are rows of a table of its own, over a `WindowStack` of its own, since the game's table names the game's windows. It needs two widgets the canvas does not have yet: a scroll view, for its seat table, its log and its seat window, which [the canvas plan](ImplementationPlan-Canvas.md)'s CV9.1 adds before the overseer or the game needs it; and a table (gate D7), which OV3.2 adds to the canvas with its first use. It takes no drop-down (§3, the map).
- **Not packaged** (gate D5), as `OutpostServer` (ADR-078 decision 1).
- **The server's side** is in `GameLogic`, which owns the listeners and the seats, and in `OutpostServer`, whose main thread adds the process's figures.
- **No new dependency** (R14): QUIC is MsQuic's, already in `NeuronCore` (ADR-004), and the rest is the Windows SDK.
- **The look is the game's.** A table's rule goes into the style guide with its widget (gate D7). Colors for more than two sides have none yet: the friends' seats decide them (gate F1 of [the Horizon plan](ImplementationPlan-Horizon.md); ADR-085), and the overseer draws each seat in its set's color, whatever those become.

## 7. What it forecloses

- An overseer that sends a command to the simulation, or pauses it.
- A web page or an HTTP server, without reopening ADR-001 and R14.
- A second overseer beside the first.
- Anything of the overseer in a save.
- The overseer's port reached from the internet as the game's is: a token in a file is too weak for a channel that can stop the server, and stronger authentication would be a decision of its own.

## 8. What could go wrong

- **Full sight while the owner plays.** The owner sees through the fog the rivals rely on. Accepted (§2); the universe log records when the overseer was connected (gate D4), which a friend sees only if the owner shares the log, since the game itself shows nothing of it.
- **The overseer costs the tick.** The full-sight view is a snapshot of everything, built on the server's thread. It is timed as its own part, and the tick's budget of a 5 ms 99th percentile holds with it in ([the budgets](../Budgets.md)); its measurement goes there.
- **A second door.** The listener is closed to other machines by default, and opened only by the owner's own setting and firewall.
- **Two tracks on one toolkit.** The canvas plan's ports and the overseer both change the canvas in `NeuronClient`, ADR-100, `Hud.cpp` and `HudTests.cpp`. The scroll view is one task of the canvas plan's, CV9.1, done by whichever track needs it first, so that two are not written. The overseer touches `Hud.cpp` only to reach `GameTheme`, and the layout and contrast guards it shares with `HudTests` move to a helper both read. Its sector map shares the minimap's drawing, which the canvas plan ports last (CV8.3).
- **A server VM without a GPU.** ADR-006 decision 6 skips software adapters, so the overseer cannot run over RDP on a Windows Server VM that has no GPU (gate D6). Run from the owner's PC across the LAN, it needs none.
- **One taker of the seat figures.** `Server::TakeSeatFigures` hands each sample once. The main thread becomes its taker; the universe log reads its own samples through `SeatTick` (ADR-082 decision 3), and is untouched.

## 9. The owner's questions

| Gate | Decision | Proposed in | Blocks | Status |
|---|---|---|---|---|
| D1 | **The overseer's project.** `OutpostOverseer`, a Win32 executable with a row of its own in ADR-002's table, including NeuronCore, NeuronClient, GameProtocol and GameApp; its model and layout in GameApp, tested in GameAppTests, and built on the canvas as merged (ADR-100): `Neuron::Canvas`, `Theme`, `TextMetrics` and `WindowStack`, in the game's look, which GameApp exposes (`GameTheme`, its typefaces and its sprites), and nothing of `Painter`. Proposed: as written. The canvas's widgets are merged, so building the overseer before the canvas is no longer an alternative. **Decided on 2026-10-27:** as proposed. | this document, §6 | OV3, the overseer | decided 2026-10-27 |
| D2 | **A new token, written by the server.** New token makes a seat's token afresh, writes it into `Universe.json` and writes the seat's join file again, where ADR-078 decision 2 has the server only read its settings. Proposed: yes, written to a new file and renamed into place, as a save is; or New token stays out of the first version. | this document, §4 | OV5, the actions | open |
| D3 | **Dropping a link.** Drop link and Hand to deputy become one action, with an option for the deputy to take the seat at once rather than after ADR-079's minute. The game reads a new close reason, `DroppedByOwner`, as one it does not join again after (ADR-078 decision 10), and goes back to the menu saying the server's owner closed the connection. Proposed: as written. | this document, §4 | OV5, the actions | open |
| D4 | **Full sight on the record.** The universe log writes `overseer <tick> opened` and `closed`, so a friend can see when the owner had full sight open. Proposed: yes. **Decided on 2026-10-27:** as proposed, each with the address the overseer came from, and a connection refused for a wrong token written as `overseer <tick> refused`, which also makes the log an audit of who reached the overseer's port. The game shows nothing of it, so it reaches a friend only if the owner shares the log. | this document, §2 | OV2, the channel | decided 2026-10-27 |
| D5 | **Packaged or not.** Proposed: not packaged, as `OutpostServer`, so it runs beside the server on a VM without registering a package. | this document, §6 | OV3, the overseer | open |
| D6 | **A server without a GPU.** Proposed: the overseer, and not the game, takes the WARP adapter when no hardware adapter qualifies, which amends ADR-006 decision 6 for it alone; or the overseer always runs from a machine with a GPU. | this document, §8 | OV3, the overseer | open |
| D7 | **A table in the interface.** The style guide has no rule for a table, and the canvas's widgets (ADR-100) have no table. Proposed: a table widget of the canvas, whose heading sorts on a click and shows the sort's direction, whose picked row takes the picked card's face and edge, and whose cell holds a figure over a detail line, written into the style guide with the ADR that owns them. **Decided on 2026-10-27:** as proposed, in `NeuronClient` under ADR-100 and tested in `NeuronClientTests`, added by OV3.2 with its first use. A heading names a payload as any widget does and the overseer sorts its rows, so that the canvas keeps no sort state. | this document, §6 | OV3, the overseer | decided 2026-10-27 |

## 10. The work, in order

Each would be a milestone of a plan of its own, once the gates it waits on are decided.

1. **OV1 — The server measures** what §5 lists but the process's figures, which nothing reads before OV2's `Health`; writes the `written`, `tick`, `dropped` and `join` records; and `Tools/UniverseLog.py` reads them. It changes no message, so a universe made before it runs on after it.
2. **OV2 — The channel:** the listener, `Overseer.json`, the messages of §4 but the actions, `Closing`, and the `Overseer` tick part. It changes no message of the game's. QUIC is checked in CI's Windows run only (ADR-060).
3. **OV3 — The overseer:** the project, its window, the overview, and the map as one system's sector map, which is all a universe has until the galaxy (the Horizon plan's milestone 42); the galaxy view comes with it. It takes the canvas plan's scroll view (CV9.1) and adds the table to the canvas (gate D7). It is run, not only built (AGENTS.md §3), and the owner's run accepts it.
4. **OV4 — The seat window, the server window and the header states.**
5. **OV5 — The actions:** Save now and Stop, then Drop link and New token as gates D2 and D3 decide.

The ADRs that would record it: a new one for the overseer, its project, its channel, its messages and `Overseer.json`; and amendments to ADR-002 (its row), ADR-004 decision 6 (bytes sent and the address), ADR-078 (the settings' two keys, and gates D2 and D3), ADR-082 decision 1 (the records), the server's tick parts (Server.h's `TickPart`), and ADR-006 decision 6 if gate D6 is taken.

## References

- [ADR-002](../ADR/ADR-002-authoritative-server.md): the server model, fog on the server, and the projects' table.
- [ADR-004](../ADR/ADR-004-quic-transport.md): QUIC, and a connection's figures.
- [ADR-025](../ADR/ADR-025-server-thread.md): the server's thread, and where its failure reaches.
- [ADR-077](../ADR/ADR-077-universe-state-on-disk.md): saves, the command log and recovery.
- [ADR-078](../ADR/ADR-078-dedicated-server.md): the dedicated server, its settings, seats, tokens and certificate.
- [ADR-079](../ADR/ADR-079-seat-controller-and-deputy.md): who plays a seat.
- [ADR-082](../ADR/ADR-082-universe-log-and-universe-run.md): the universe log.
- [ADR-085](../ADR/ADR-085-one-meaning-per-color.md): one color, one meaning.
- [ADR-096](../ADR/ADR-096-events-and-state-cross-apart.md) and [ADR-098](../ADR/ADR-098-a-state-crosses-as-what-changed.md): the newest state wins, and crosses as what changed.
- [The Horizon plan](ImplementationPlan-Horizon.md): the galaxy (gate G5) and the friends' seats (gate F1).
- [The network plan](ImplementationPlan-Network.md): the listener's limits and the connection's figures this builds on.

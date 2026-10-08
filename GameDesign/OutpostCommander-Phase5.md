# Outpost Commander — Phase 5 Design: A World That Persists

Status: **accepted** · Owner: Stefan Zwaal · Started 2026-10-08, from [the horizon plan](ImplementationPlan-Horizon.md) §4 · Accepted on 2026-10-08, with gates H1 to H9 decided as proposed and H10 left to the week (§14) · The order of the work is [the Phase 5 plan](ImplementationPlan-Phase5.md)

This document says what Phase 5 builds on top of Phase 4, and amends [the Phase 4 design](Archive/OutpostCommander-Phase4.md) and the designs before it where they differ. It takes up the first step of [the horizon](OutpostCommander-Horizon.md) (its §11.1), and from here it, not the horizon, is the authority for what that step builds.

---

## 1. What Phase 5 is for

The owner's question of 2026-10-04 asked how the game grows towards a galaxy that keeps running and is played a few times a day (horizon §1). Its first step: **does a world that keeps running while its player is away work at all?**

A world is today's match that does not stop. It runs on a server the owner keeps going, on Phase 4's 10 km system, with two seats: the owner and an AI empire, or the owner and one friend. While a player is online the world is played as a match is today. While the player is away, a deputy keeps the player's empire running, and orders the player gave in advance carry on. The server saves the world every minute and comes back from a restart with what it had.

**What could go wrong.**

- **A front that does not move.** No base fell in any of Phase 4's 40 measured matches (its U5), and domination, which ended all of them, is off in a world (§8). Nothing else ends a stalemate. W6 is where it shows.
- **The Ore piles up.** After Phase 4's milestone 33 the median side holds 8,159 Ore at minute 20, with its fleet full for 69% of the match. A world runs for days. Phase 5 records the bank and changes nothing (§9).
- **A deputy that plays much worse, or much better, than its player** leaves presence deciding the world, or makes absence the better strategy. W3 and W4 measure both.
- **State that does not survive.** A save that misses one field restores a different world, and nothing a player sees says so. The tests of §5 compare a restored world with one that never stopped.

**The question Phase 5 answers: is a world that keeps running while its players are away a game, and what does being present buy?**

---

## 2. What Phase 5 must show

| # | Question | How we know |
|---|---|---|
| W1 | Does a world survive a week? | The owner's world runs for 7 days on the dedicated server, through at least one planned restart and one killed process, and loses no command the log had written. |
| W2 | What does persistence cost? | Saving a late world takes under 50 ms of the server's thread. U7's 99th percentile tick, at most 5 ms, holds with the deputies and the saves in. The save's size is recorded. |
| W3 | Does a deputy keep an empire running? | The share of an absent player's time that its Shipyards and Lab stand idle with Ore to spend, against the AI's own over the same hours. |
| W4 | What does presence buy? | Horizon §9's battle measurement; and the owner's income, fleet against its cap and research done per hour, against its deputy's. |
| W5 | Do orders run while away? | Each trigger, action and condition fires on its tick and decides as written, in tests; the owner judges a 02:00 attack in play. |
| W6 | Is the rhythm a game? | The owner judges, over W1's week, whether three check-ins a day are worth making. |

W1, W4 and W6 are the owner's week. W2's tick figure is the owner's run in Release, as U7 was; the container measures the rest.

**Where W2 stands after milestone 35** (plan task 35.2). In the Linux container, the server built by clang 18 at `-O2` against a stand-in for the Windows headers: a save of a world at minute 60 of two Normal AIs on the 10 km map holds 274–293 entities in 75–78 KB, and encodes in 0.36–0.38 ms of the server's thread, over seeds 1 to 3. The deputies, which also run on that thread, come with milestone 37.

---

## 3. Decided by the owner on 2026-10-08

- **The design, then one milestone at a time,** one PR each, starting with the world that survives a restart, the part the container can verify end to end.
- **H1 — The server runs on the owner's PC,** or a Windows Server 2022 VM if the owner chooses one at the run. A client finds it by an address, the certificate's hash and a seat token, handed over out of band (§4).
- **H2 — A deputy takes a seat 60 s after its player's connection drops,** and gives it back when the player takes the seat again. What it ordered stands until the player changes it (§6).
- **H3 — The deputy is a keeper** (§6).
- **H4 — Triggers are evaluated on the server** (§7).
- **H5 — The order vocabulary is fixed and small,** as §7 lists it.
- **H6 — A player who loses restarts at a free start** after an hour, with starting Ore, keeping its research and designs (§8).
- **H7 — A save every 60 s of ticks; the command log written as each tick applies it; and a world that pauses while its server is down** (§5).
- **H8 — A save is pinned to a state version, not to a build** (§5).
- **H9 — Phase 4's economy is unchanged,** and the bank is recorded (§9).
- **H10 — Whether a galaxy or friends come next** is decided from W6 and the week's log, after Phase 5.

---

## 4. The dedicated server

*Decided (gate H1).*

- **`OutpostServer` is a console executable** that links `GameLogic`, `NeuronServer`, `GameProtocol` and `NeuronCore`, and none of the client libraries. ADR-002 foresaw it. With milestone 37 it links `Opponent` too, for its deputies and AI empires (§6), whose own files still include only `GameProtocol`. It is not packaged as MSIX: a server is run from a folder.
- **A world is a folder:** its settings, its saves and its command log (§5). `OutpostServer --new-world <folder>` makes one, with a seed and the seats, and `OutpostServer <folder>` runs it.
- **It binds the address and port the world's settings give**, not only `127.0.0.1` (ADR-060 decision 8 binds the loopback).
- **Its certificate and key are kept with the world,** so a client pins the certificate once. Today the listener makes a new one every time and deletes its key when it goes (ADR-060 decision 6).
- **A seat is taken with its token as well as its player.** The server makes a 128-bit token for each seat when the world is made. Today the first hello for an open seat takes it.
- **A seat can be taken again while the world runs.** Today a hello after `Start` is refused (ADR-060 decision 5). A player whose connection dropped takes its seat again with its token. A snapshot is whole, so the first one after it shows everything the player sees and remembers; the shots and destructions of the ticks it missed are lost, which is presentation.
- **The server writes a join file for each seat:** the address, the port, the certificate's hash, the player and the token. The owner hands it to the friend.
- **The client's menu gains "Join world"** when a join file is in the player's documents, as `Outpost Commander\Join.json`, and `--join <file>` joins from the command line. The documents rather than the game's own folder, which Windows redirects for a packaged game ([ADR-078](../Design/ADR/ADR-078-dedicated-server.md)). This changes the menu, which gate K4 kept as it was (owner, 2026-10-05).
- **A lost connection takes a client back to the menu,** saying why, in the server's words when the server closed it, where it was reported as an error that closed the game.

---

## 5. A world that survives a restart

*Decided (gates H7, H8).*

- **The whole of `Simulation`'s state is saved,** written field by field as the wire format writes a message (ADR-060 decision 4), from field lists that a structured binding checks, so a field added to a saved type and not to its list does not compile. A save is everything a world needs to run on: entities, designs, players and what each sees and remembers, sectors, outposts, the PRNG, the planned group orders, and how the world stands.
- **What follows from the state is not saved:** the path graphs, which are built again on load and are then the very graphs the running world had (ADR-054 decision 7), and each player's research effects, which follow from the topics researched. The tuning data and the map are not saved either: the save names them by a hash of their files, and refuses to load against others.
- **A save is pinned to a state version** (gate H8). Its header carries `WORLD_STATE_VERSION`. A change to what `Simulation` holds raises it, and a test fails until it is raised (AGENTS.md R18). A save of another version is refused. So a build that fixes a crash and changes nothing the world holds loads the world it crashed in. Upgrade steps between versions are Phase 8's (horizon §7).
- **Every 60 s of ticks the server saves** (gate H7). It encodes the state between two ticks, on its own thread, and a writer thread writes the bytes to a new file and renames it into place, so a killed process never leaves half a save. The newest three saves are kept.
- **The command log is written to the world's folder** as each tick applies its commands, and handed to the system before the next tick runs. It begins a new file at each save. ADR-009's log was only in memory.
- **On start, the server loads the newest save that reads whole, and applies the logged commands since it,** tick by tick, as fast as it can. It comes back at the last tick that applied a command, or at the save's tick if that is later, so a killed process loses at most the ticks since then that applied nothing. On the same build the world it comes back to is the world that ran, bit for bit (ADR-009). On another build of the same state version it is a world that follows the same orders, which no player can tell from it: the server is the only copy of the world.
- **A world pauses while its server is down** (gate H7). The tick host drops a backlog of more than five ticks (ADR-009 decision 4), and a world does not race to catch up hours. Horizon §12 excludes pausing the world; Phase 5 accepts it for an outage.
- **What a killed process keeps.** Everything handed to the system: the log of every tick that ran, and every save renamed into place. What a lost machine keeps is not promised: nothing is forced to the disk.
- This ends Phase 1's accepted risk 3, that there is no save and load, for a world. A match is still played in one sitting.

---

## 6. The deputy

*Decided (gates H2, H3).*

- **A seat has one controller at a time:** its player, or its deputy. The deputy takes the seat 60 s after the player's connection drops, and gives it back when the player takes the seat again. What the deputy ordered stands until the player changes it. A seat nobody has taken yet is the deputy's from the start.
- **A deputy is a keeper.** It keeps its player's research, production queues, standing orders and retreat thresholds going: a research queue that empties is filled as the AI fills its own, and a Shipyard whose queue empties builds again what it last built, within the cap and the Ore. It keeps as many Constructors as its player had when it took the seat (owner, 2026-10-08). It rebuilds a lost rig on a known asteroid in a held sector, repairs, and upgrades its Command Station when the cap stops production, as the AI does. Its idle warships defend the player's held sectors, and go back to where they stood once the attack is over (owner, 2026-10-08). It never attacks, raids, claims a sector or clears pirates, except by a scheduled order the player gave (§7).
- **An AI empire is the AI**, playing to win, as ADR-076 has it.
- **A deputy and an AI empire run on the server's thread,** stepped after each tick with that tick's snapshot, and their commands are applied at the next tick and logged as a player's are. A world with deputies therefore replays from its seed and its log. ADR-025 forecloses the AI on the server's thread "until clients and server are separate processes", which is now. `GameLogic` still does not include `Opponent`: the server hands `InProcessServer` a client interface declared in `GameProtocol`.
- **The AI starts from any state.** A deputy takes over an empire it did not build: ships in no group of its own, designs it did not make, structures where it would not have placed them. It also starts again from a world the server restored. So `AiPlayer` must pick up any snapshot of its player and play on, and its own state is never saved.

---

## 7. Orders that run while away

*Decided (gates H4, H5).*

- **A scheduled order is one trigger, one action and at most one condition,** given to a selection, and kept by the server as a standing order is (ADR-059 decision 3). It fires once and is gone. Any other order to its ships ends it, as it ends a standing order.
- **Triggers:**
  - a time of day, on the player's own clock (owner, 2026-10-08);
  - enemy ships in a sector the player holds;
  - a Relay of the player's suppressed, or under attack;
  - a Mining Rig of the player's lost.

  Each event trigger watches one sector the player picks, and the action's target is fixed when the order is given (owner, 2026-10-08).
- **Actions:** move, attack-move, attack, hold a sector, patrol, and build a Mining Rig on an asteroid, which is how "mine this field at 03:00" is given.
- **The condition:** "unless the enemy warships the player sees in the action's sector take more than a number of command points the player sets; then hold the sector the ships are in". No nesting, no variables, and a new trigger or action only when a missing case keeps coming up in play (horizon §5).
- **A time of day becomes a tick in the server's host,** when the command arrives: the host is where wall time is allowed (ADR-009 decision 3), so `Simulation` keeps the tick as its only clock, and the log replays. An order for 02:00 that a pause has pushed past fires late, at its tick.
- **The server raises the events the triggers need, and the client's alerts read the same events** from the snapshot, so an order's trigger and the alert its player sees are one event. This overrules ADR-059 decision 1, which kept alerts the client's.
- **The client** gains an orders window, listing a seat's scheduled orders with a time and a sector to pick, and the selection's panel names a ship's pending order.

---

## 8. A world without an end

*Decided (gate H6).*

- **Domination is off in a world.** A world ends when the owner stops it.
- **A player who loses restarts.** A player with neither a Command Station nor a finished Shipyard has lost (ADR-037). In a world, an hour later its seat restarts at a free start: a Command Station and the starting Constructors as at the world's start, and its Ore raised to the starting Ore when it has less, keeping its bank, its research and its designs (owner, 2026-10-08). Its ships and structures that still stand stay its own.
- **A start is free** when no other player holds its home sector or has a structure in it. Phase 4's map has two starts, one for each seat, so a seat restarts only at its own, and waits while it is not free; the world log says so. A map with spare starts, which Phase 7 brings, makes the wait rare. This is the weakest rule of Phase 5, and the week will show how often it binds.

---

## 9. The economy

*Decided (gate H9).*

Phase 4's economy is unchanged: its costs, income, cap, pirates, derelicts and repair. The world log records each seat's bank, income and fleet against its cap every hour. Whether the bank needs a sink or a bound is decided in Phase 6 with the systems that change it (horizon plan, gate G3).

---

## 10. The world log and the week

- **The world log** is ADR-038's match log for a world: each save and how long it took, each restart and recovery, each hand-over between a player and its deputy, each scheduled order fired, each restart after a loss, and every hour each seat's bank, income, fleet, nodes and research. Each battle is logged with whether its players were present.
- **`--world-run`**, beside `--ai-matches` (ADR-063): a world with an AI in every seat, stepped as fast as the processor allows, killed and recovered at random ticks, and compared at the end with the same world run straight through. A week of world is a few hours of a laptop's time. Every later phase measures with it.
- **Horizon §9's battle matchups**, run headlessly over many seeds, with the AI's battle behavior on both sides, for W4's baseline.

---

## 11. The client

- "Join world" on the menu and `--join` (§4).
- A lost connection back to the menu (§4).
- An orders window, and a ship's pending order on its panel (§7).
- Alerts read from the server's events (§7).
- **On taking its seat again,** a panel says what happened while the player was away: what the deputy built and lost, sectors gained and lost, and the orders that fired. The server keeps it with the world, so a restart while the player is away loses none of it (owner, 2026-10-08).

---

## 12. What changes elsewhere

- **The horizon:** its §11.1 is taken up here, and O4 and O5 are decided by H3 and H4.
- **Phase 1:** accepted risk 3 ends for a world (§5).
- **Phase 2 §8:** domination is off in a world (§8).
- **ADR-037:** a player who loses in a world restarts (§8).
- **AGENTS.md:** R18, that a change to what `Simulation` holds raises the state version (§5); the solution gains `OutpostServer`.
- **ADRs edited in place:** ADR-002 for `OutpostServer` and its row in the project table; ADR-009 for the log on disk; ADR-020 for the AI's start from any state and the keeper; ADR-025 for the clients on the server's thread and the writer's thread; ADR-059 for the server's events; ADR-060 for the bind, the kept certificate, tokens and a seat taken again. **New ADRs:** a world's state on disk; the dedicated server and its seats; the seat's controller and the deputy; scheduled orders and the server's events; the world log.

---

## 13. Out of scope for Phase 5

Several systems and travel between them; more than two seats; notifications; upgrade steps between state versions; a season and its end; any change to the economy; a web view of the world; and everything Phase 4 kept out.

---

## 14. Gates

Each was an owner decision, and the owner decided H1 to H9 on 2026-10-08 as proposed in [the horizon plan](ImplementationPlan-Horizon.md) §4:

- **H1 — Where the server runs:** the owner's PC, or a Windows Server 2022 VM chosen at the run; a client finds it by its address, the certificate's hash and a seat token.
- **H2 — The hand-over:** 60 s after the connection drops; back when the player takes the seat again.
- **H3 — The deputy:** a keeper.
- **H4 — Triggers:** on the server.
- **H5 — The vocabulary:** as §7 lists it.
- **H6 — A player who loses:** restarts at a free start after an hour, with starting Ore, keeping research and designs.
- **H7 — Recovery:** a save every 60 s, the log as each tick applies it, and a pause during an outage.
- **H8 — The pin:** the state version, not the build.
- **H9 — The economy:** unchanged, the bank recorded.
- **H10 — A galaxy or friends next:** open, decided from W6 and the week's log.

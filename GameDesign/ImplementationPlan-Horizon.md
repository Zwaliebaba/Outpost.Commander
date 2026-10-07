# Outpost Commander — Horizon Plan: From Phase 4 to a Galaxy That Keeps Running

Status: **draft, proposed, nothing decided** · Started 2026-10-07, at the owner's request for a plan to build [the horizon](OutpostCommander-Horizon.md) once Phase 4 is done · Derived from the horizon, [the Phase 4 design](OutpostCommander-Phase4.md) and the code on `main` at that date

The horizon says where the game goes in the long run and decides almost nothing. This plan proposes how to get there: four phases, each of which becomes a phase design of its own, with its gates, before any of its code is written (horizon §11, AGENTS.md). It is not an authority, and it binds nothing until the owner accepts a phase design built from it. Where it disagrees with the horizon, §2 says so and why.

---

## 1. Before Phase 5 starts

Phase 4 is open: milestones 33 (the AI plays Phase 4) and 34 (U1–U8) are `todo`, and the owner's runs of 27.3, 28.1, 28.4, 29.2, 30.2, 31.2 and 32.2, and of the Interface plan's 14–17, are outstanding. Phase 5 should not start before 34.2, for three reasons:

- **The deputy is milestone 33's AI.** Everything §4 asks of a deputy is built on the AI that plays Phase 4's rules: pirates, salvage, the Repair Bay and the cap.
- **U5 and the Ore that piles up decide the economy a world inherits.** After milestone 32 the median side holds 4,636 Ore at minute 20, and after milestone 28 every station had reached the cap of 50 points by then (Phase 4 §2). A world that runs for weeks multiplies whatever milestone 34 finds.
- **U7 is the tick budget.** Phase 5 adds deputies and saves to the server's thread, and Phase 6 adds systems. Both are measured against U7's figure on the development machine, which does not exist yet.

---

## 2. Where this plan departs from the horizon

Each is a proposal for the owner, not a decision.

1. **Recovering a world does not need a replay that matches to the bit.** Horizon §3 and §7 tie a season to one build because ADR-009's replay holds only on the same binary. But the server is authoritative: nobody holds a copy of the world to disagree with. After a crash, a fixed build that loads the last saved state and applies the logged commands since then reaches a plausible world, not the identical one, and no player can tell. What has to survive a new build is the **layout of the saved state**, not the binary. So the season is pinned to a state version, not to a build: a fix that does not change what `Simulation` holds can ship mid-season. At this repository's pace, 87 pull requests since 2026-09-29, a season pinned to a binary is a season with no fixes. The same-binary replay stays what it is for: debugging.
2. **A galaxy is one `Simulation` with several systems, not one `Simulation` per system** (horizon §3). Ore, research, designs and the fleet cap are an empire's, and today they live in `PlayerState` inside `Simulation`. A simulation per system needs an empire ledger that every system reads and writes each tick, which is the strategy layer above the battles that the horizon says there is not, plus a hand-over protocol for fleets and the command ordering between systems that ADR-009 decision 6 defines today for one. One simulation, with a system as a level above the sector and a pathfinding region per system, keeps one tick, one command order and one replay. Splitting systems across threads is an optimization for when Phase 6's measurement says one thread misses, and an ADR with that figure.
3. **Triggers are evaluated on the server, not by the deputy** (horizon O5). A timed or conditional order has to fire while its player is online too, and the deputy may be replaced (the self-play network, gate N6). Standing orders and retreat are the server's already (ADR-059, ADR-075). A time of day enters as a tick: the server's host, which is where wall time is allowed (ADR-009 decision 3), stamps the order's target tick when the command arrives, so `Simulation` keeps the tick as its only clock and the log replays.
4. **Deputies run on the server's thread, stepped after each tick.** Their commands are then logged as a human's are, and a world with deputies replays from its seed and the log. On threads of their own their timing would depend on wall time. ADR-025 forecloses the AI on the server's thread "until clients and server are separate processes", which is exactly this case. `GameLogic` still does not include `Opponent`: the server executable hands `InProcessServer` a client interface declared in `GameProtocol`, as `GameLogicTests` drives the AI today.
5. **A deputy has to start from any state.** Handing a seat from its player to a deputy, back again, and recovering a restarted server all require the AI to pick up an empire it did not build: ships in no group of its own, designs it did not make, structures where its plan would not put them. `AiPlayer` today builds its plan from the start of its own match. That cold start is the largest single AI change on the horizon, and it is Phase 5's first AI task. It also means the AI's own state never has to be saved.
6. **The horizon does not say what happens to a player who loses.** ADR-037: a player with neither a Command Station nor a finished Shipyard is out, and a lost Command Station stays lost. In a match that is the end. On day 3 of a season it is a friend with nothing to do for the rest of it. Phase 5 needs a rule (gate H6).
7. **Domination ends a world in hours.** A lead of one node takes 50 minutes (ADR-057). A world that keeps running has no domination, or a season's version of it, which is horizon O9's season end brought forward to Phase 5 in its simplest form: nothing ends a Phase 5 world but the owner.
8. **Ore that comes back makes the pile-up worse** (horizon §6.3). With the cap bought by levels and no upkeep, Ore after the first day buys only replacements. A bank without a bound means a lost fleet is rebuilt at once, so the cap, not income, decides how fast a player recovers, which is the opposite of the "territory is capacity and recovery" horizon §6.2 expects. A sink or a bound on the bank comes before fields that recover (gate G3).
9. **Presence is measured on both sides.** Horizon §9 measures what a present player buys in battle. A deputy also runs production and research around the clock without forgetting a queue. If it out-builds its player, being offline becomes the better strategy, the inverse of the failure the horizon names. Phase 5 measures the deputy's economy against the owner's as well (W4).

On the order of the phases, this plan keeps the horizon's: the galaxy before friends. The reason is not the horizon's. A single 10 km system goes stale fast under a persistent world: every reserve runs dry in 25–40 minutes, every node is claimed on the first day, and on the 10 km map every station was at its cap by minute 20 (Phase 4 §2, after milestone 28). A first season with friends on one stale system would spend their goodwill on a stalemate of capped fleets on a trickle. Phase 5's week-long run will show whether that is so; if it is not, friends in one system can come first (gate H10).

---

## 3. The phases

| Phase | Horizon step | Its question | Seats | Map |
|---|---|---|---|---|
| 5 — A world that persists | §11.1 | Does a world that keeps running while its player is away work at all? | The owner and one AI empire, or one friend | Phase 4's system |
| 6 — A galaxy | §11.2 | Do several systems, travel between them and progression from the galaxy keep a world moving for weeks? | As Phase 5 | Generated, 10–30 systems |
| 7 — Friends | §11.3 | Does a season of weeks hold with a circle of friends and their deputies? | 3–6 friends and AI empires | Phase 6's |
| 8 — Seasons of months | §11.4 | Does a season of months keep its arc, and survive changes to the code? | As Phase 7 | Phase 6's, with an arc |

Milestones continue the Phase 4 plan's numbers, from 35. Each phase's milestones are proposed in outline here and scoped in its own plan once its design is accepted.

---

## 4. Phase 5 — A world that persists

### What it must show

| # | Question | How we know |
|---|---|---|
| W1 | Does a world survive a week? | The owner's world runs for 7 days on the dedicated server, through at least one planned restart and one killed process, and loses no command the log had written. |
| W2 | What does persistence cost? | Saving a late world takes under 50 ms of the server's thread; U7's 99th percentile tick holds with the deputies and saves in. Save size recorded. |
| W3 | Does a deputy keep an empire running? | Share of an absent player's time that its Shipyards and Lab stand idle with Ore to spend, against the AI's own over the same hours. |
| W4 | What does presence buy? | Horizon §9's battle measurement, and the owner's income, fleet at cap and research done per hour against its deputy's. |
| W5 | Do orders run while away? | Each trigger and condition fires on its tick and decides as written, in tests; the owner judges a 02:00 attack in play. |
| W6 | Is the rhythm a game? | The owner judges whether three check-ins a day are worth making, over the week of W1. |

### Gates

- **H1 — Where the server runs.** The owner's PC, or a Windows Server 2022 VM, which MsQuic's Schannel build needs (ADR-004). How a client finds it: an address, the certificate's hash and a seat token, handed over out of band.
- **H2 — The hand-over.** Proposed: a deputy takes a seat 60 s after its player's connection drops, and gives it back when the player takes the seat again. What the deputy ordered stands until the player changes it.
- **H3 — The deputy's directives** (horizon O4). Proposed for Phase 5: the deputy keeps its player's research queue, production queues, standing orders and retreat thresholds going, defends held sectors with idle warships, and never attacks except by a scheduled order. Alternative: the whole AI, with the player's queues as its starting plan.
- **H4 — Where triggers are evaluated** (horizon O5): the server, as §2.3 proposes.
- **H5 — The order vocabulary**, fixed and small (horizon §5). Proposed triggers: a time of day; an enemy in a held sector; a Relay suppressed or under attack; a rig lost. Actions: move, attack-move, attack, hold a sector, patrol, build a rig on an asteroid. One condition each, "unless the enemy's strength the player sees in the target sector exceeds a figure the player sets; then hold". No nesting, no variables.
- **H6 — A player who loses.** Proposed: its seat restarts at a free start with starting Ore and its research kept, after a delay of an hour. Alternatives: out for good, as ADR-037; or a Command Station that can be rebuilt.
- **H7 — Recovery.** Proposed: a save every 60 s of ticks, the log written as each tick's commands are applied, and a world that pauses while its server is down, since `TickHost` drops a backlog over five ticks (ADR-009 decision 4). That is a pause, which horizon §12 excludes; it is accepted for outages rather than racing to catch up hours of ticks.
- **H8 — State pinned by version, not by build** (§2.1).
- **H9 — The economy in Phase 5.** Proposed: Phase 4's economy unchanged, the bank recorded; the sink is decided in Phase 6 with the systems that change it (gate G3).
- **H10 — Galaxy or friends next** (§2, last paragraph). Decided from W6 and the week's log.

### Milestones

**35 — The dedicated server.**
- `OutpostServer`, a console executable that links `GameLogic`, `NeuronServer`, `GameProtocol`, `NeuronCore` and `Opponent` (for deputies), and none of the client libraries. ADR-002 foresees it; its row in the project table and AGENTS.md §2 gain `Opponent`.
- It binds the address it is configured with, not only `127.0.0.1`, and keeps its certificate's key across restarts, so a client pins it once. Today the key is deleted when the listener goes (ADR-060 decision 6).
- A seat is taken with a token as well as the player. Today the first hello for an open seat takes it.
- A dropped seat can be taken again after the server has started; today a hello after `Start` is refused (ADR-060 decision 5). The first snapshot after it carries everything the player sees and remembers; the per-tick shots and destructions it missed are lost, which is presentation.
- The client's menu gains "Join a world": address, hash and token. That reopens K4's menu decision.
- **ADR:** new, the dedicated server and its seats. ADR-002, ADR-004 and ADR-060 edited in place.
- **Verify:** `GameLogicTests` over the loopback for tokens and retaken seats, in the container; `QuicTransportTests` for a remote bind and a retaken seat, in CI; **owner run** on a second machine.

**36 — A world that survives a restart.**
- A save format for the whole of `Simulation`'s state, written field by field from field lists, as `WireFormat` writes messages (ADR-060 decision 4), and headed by a state version that a change to `Simulation`'s layout must raise. The path graphs are not saved: they are built on load, which ADR-054 decision 7 makes the very graph the running world had.
- The tuning data and the map are saved by their hash; a world refuses to load against others.
- The server copies `Simulation` between ticks (it is copyable and compares equal, ADR-009 decision 8) and writes the copy on another thread. The command log is appended to a file as each tick's commands are applied.
- On start, the server loads the newest save and applies the log since it.
- This ends Phase 1's accepted risk 3, that there is no save and load.
- **ADR:** new, a world's state on disk. ADR-009 edited (the log is written to a file), ADR-025 edited (the copy and the writer's thread).
- **Acceptance:** a world saved, loaded and compared equal; a world killed at a random tick and recovered compares equal at a later tick to the same world run straight through, on the same binary; a save of another version, another tuning or a cut file refused.
- **Verify:** in the container, including the copy's and the write's time on a late 10 km world.

**37 — Deputies in the server.**
- The AI starts from any state: given a snapshot of a mid-match empire it did not build, it groups its ships, adopts its designs and production, and plays on (§2.5).
- Deputies and AI empires are clients stepped on the server's thread after each tick, through an interface declared in `GameProtocol` (§2.4). The shell's AI moves there too, so there is one way the AI runs.
- A seat has one controller at a time, its player or its deputy, and hands over as H2 decides. The deputy plays by H3's directives.
- **ADR:** ADR-020 and ADR-025 edited in place; a new one for the seat's controller and the deputy's directives.
- **Acceptance:** `AiPlayerTests`: the AI takes over a human-built empire at minutes 5, 20 and 60 of a scripted match and keeps its production running; hands it back and leaves the player's orders standing; a world with deputies replays from its seed and log.
- **Verify:** in the container; seeds 1–40 with the seat handed over at random.

**38 — Orders that run while away.**
- `ScheduledOrderCommand`: one trigger, one action and one condition from H5, kept by the server as a standing order is (ADR-059 decision 3), and ended by any other order to its ships.
- A time of day becomes a tick in the server's host, when the command arrives (§2.3).
- The events the triggers need are raised by the server. The client's alerts read the same events from the snapshot, so an order's trigger and the alert its player sees are one event. That overrules ADR-059 decision 1 and lifts its foreclosure.
- The client: an orders window listing a seat's scheduled orders, with a time and a sector to pick, and the selection's panel naming a ship's pending order.
- **ADR:** new, scheduled orders and the server's events. ADR-059 edited in place.
- **Acceptance:** `ScheduledOrderTests` for each trigger, action and condition, and an order that meets a reinforced sector and holds; `AlertsTests` on the server's events.
- **Verify:** in the container for the server; CI and **owner run** for the client.

**39 — A world without an end, and the week.**
- Domination is off in a world; H6's rule for a player who loses.
- `--world-run`, beside `--ai-matches` (ADR-063): a world with an AI in every seat, stepped as fast as the processor allows, killed and recovered at random ticks. The container played 2 h 56 min of Phase 3 matches in 60 s of processor time (ADR-063), so a week of world is a few hours of a laptop's time. This is the harness every later phase measures with.
- A world log, as ADR-038's match log: saves and their times, hand-overs, deputy hours, each seat's bank, fleet and nodes, and each battle with whether its players were present.
- Horizon §9's battle matchups, headless over many seeds.
- **Verify:** W1–W6. W1, W4 and W6 are the owner's week.

### Out of Phase 5

Several systems, more than two seats, notifications, upgrade steps between state versions, a season's end, and any change to the economy beyond recording it.

---

## 5. Phase 6 — A galaxy

Outlined for its design; nothing here is scoped.

### Gates

- **G1 — Progression gated by the galaxy** (horizon O1). Research tiers opened by holding systems with derelicts and a precursor vault. Phase 4's derelicts that recover research are its first step (ADR-074). What losing a vault does to a tier not finished, and whether the Lab's levels stay as a second gate.
- **G2 — Travel between systems** (horizon O8): a jump between linked Relays, or a gate at a system's edge; its time, and whether a fleet in transit can be met.
- **G3 — The economy over weeks:** a sink or a bound on the bank before fields that recover (§2.8); then fields that recover, or fields that appear (horizon O3).
- **G4 — The fleet cap across systems** (horizon O2): one cap per empire from its best Command Station, or a Command Station per system.
- **G5 — The galaxy's shape:** how many systems, their kinds, and how a generated galaxy is fair to N seats. Phase 4's placement mirrors through the center (ADR-072), which is a rule for two.
- **G6 — The strategic view.** Phase 2's gate J5 kept it out of one map; a galaxy needs a view of systems.

### Milestones in outline

- **40 — Systems in one simulation** (§2.2): a system above the sector in the map's format (ADR-036), a pathfinding region and fog grid per system, snapshots that carry only the systems a player has presence in.
- **41 — Travel between systems**, by G2.
- **42 — The galaxy from the seed**, by G5, extending ADR-072.
- **43 — Progression from the galaxy**, by G1.
- **44 — The economy over weeks**, by G3.
- **45 — The client's galaxy:** the strategic view, a system's selection, the minimap per system, alerts that name their system.
- **46 — The AI plays a galaxy**, deputies and AI empires alike.
- **47 — Measured:** a 30-system world's tick at U7's percentile, with every seat an AI; a two-week world run headlessly by `--world-run`, logging when each system is first reached, held and drained, the banks, and the share of the world where nothing happens.

**The engineering risk is the tick.** Horizon §3 estimates 0.6 of a core for 30 systems from the MVP's 0.88 ms mean. Phase 4's container figures are a 99th percentile of 1.5–2.9 ms per system, so 30 busy systems on one thread can miss 50 ms. Most systems will be quiet most of the time, and an empty system's tick costs little, but that is to be measured in 47, not assumed. If it misses, systems tick in parallel with an empire ledger merged between ticks, and that is an ADR.

---

## 6. Phase 7 — Friends

### Gates

- **F1 — Seats:** how many, and how a friend joins a running season or leaves it (a deputy, then an AI empire, horizon §4).
- **F2 — A season of weeks:** how it starts, how it ends (horizon O9 in its first form), and what is kept between seasons.
- **F3 — Diplomacy** (horizon O6): accepted as it comes, or designed for. Proposed: as it comes, with the world log recording who fought whom.
- **F4 — Reach** (horizon O7): the server posts to the friends' group chat through WinHTTP, in the Windows SDK, so R14 holds. Which service, and which events are worth a message. The dependency on a service is an ADR.
- **F5 — Bandwidth.** A snapshot is 8–19 KB at 20 Hz (ADR-060), 1.3–3 Mbit/s for each connected player. Acceptable for friends on broadband; deltas, or datagrams for state that may be dropped, are a new protocol version if it is not.

### Milestones in outline

- **48 — Seats for N players**, with the map and the galaxy fair to them.
- **49 — Seasons:** made, run, ended and archived on the server; the client lists the seasons it can join.
- **50 — Notifications**, by F4.
- **51 — The first season**, a few weeks with friends: what ended it, who was present when, and whether a front stayed interesting (horizon §9, last line).

### Operations

A server that runs for weeks needs what a match did not: its saves copied off the machine, a way to know it is down, and a way to update it between seasons. They are small on a VM or the owner's PC and listed here so they are not discovered in week two. None of them changes the game.

---

## 7. Phase 8 — Seasons of months

- **Upgrade steps between state versions** (horizon §7). From here, every change to `Simulation`'s layout comes with the step that loads the old one, and a test that loads a save of every version a running season has. That is a cost on every later change to the server, and the reason it comes last.
- **The galaxy's arc:** regions that open at set points in a season, and a race to an end in its last weeks.
- **A lead that does not decide the season**, and a season's end designed for months.
- **Deputies over long absences:** W3 and W4 again, over absences of weeks.

---

## 8. What runs beside the phases

- **Self-play.** The network of [the self-play plan](ImplementationPlan-SelfPlay.md) is the benchmark for the deputy. Whether a learned policy may command a seat in a season is its gate N6. Nothing in Phases 5–8 waits on it.
- **What the container verifies.** The server, the AI, saves, scheduled orders and `--world-run` build and run in the Linux container against stand-ins, as Phases 2–4 did. The dedicated server's QUIC over Schannel, the client's new windows and every week-long run are CI's or the owner's.
- **R14 holds throughout.** Saves are files written through `FileSys`, notifications go through WinHTTP, and the server needs no database, web server or library outside the Windows SDK. A web view of the galaxy would reopen ADR-001 and R14 (horizon O7), and is not in this plan.

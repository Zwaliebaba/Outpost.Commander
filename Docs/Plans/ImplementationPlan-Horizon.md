# Outpost Commander — Horizon Plan: From a World That Persists to a Galaxy That Keeps Running

Status: **draft, proposed** · 2026-10-07, at the owner's request for a plan to build [the horizon](../Horizon.md) · [The Phase 5 design](OutpostCommander-Phase5.md) took this plan's §4 up (owner, 2026-10-08); Phases 6 to 8 are proposals, each to become a phase design of its own

The horizon says where the game goes in the long run and decides almost nothing. This plan proposes how to get there: four phases, each of which becomes a phase design of its own, with its gates, before any of its code is written (horizon §11, AGENTS.md). It is not an authority, and it binds nothing until the owner accepts a phase design built from it. Where it disagrees with the horizon, §2 says so and why.

---

## 1. Where it starts

The game is [the living design](../OutpostCommander.md): a match against the AI on a 10 km system, and a world that keeps running on a dedicated server, with deputies for absent players and AI empires (Game §world, Game §session.server, Game §ai.deputy, Game §ai.empire). Whether a galaxy or friends come next is gate H10 of [the Phase 5 design](OutpostCommander-Phase5.md), decided from its week. What each closed phase found, and what the next had to take on, is in [the post-mortems](../Records/PostMortems.md); the measured figures are in [the balance log](../Records/BalanceLog.md), and the tick that every later phase is held to is in [the budgets](../Budgets.md).

---

## 2. Where this plan departs from the horizon

Each is a proposal for the owner, not a decision, until a phase design takes it up. Those Phase 5 took up are decided in the ADRs it names.

1. **A world is pinned to a state version, not to a build** (horizon §3 and §7), so that a fix which changes nothing a world holds can ship mid-season, and the same-binary replay stays what it is for, debugging. Decided (owner, 2026-10-08): [ADR-077](../ADR/ADR-077-world-state-on-disk.md) decisions 6 and 8, and AGENTS.md R18.
2. **A galaxy is one `Simulation` with several systems, not one `Simulation` per system** (horizon §3). Ore, research, designs and the fleet cap are an empire's, and they live in `PlayerState` inside `Simulation`. A simulation per system needs an empire ledger that every system reads and writes each tick, which is the strategy layer above the battles that the horizon says there is not, plus a hand-over protocol for fleets and the command ordering between systems that [ADR-009](../ADR/ADR-009-deterministic-core.md) decision 6 defines for one. One simulation, with a system as a level above the sector and a pathfinding region per system, keeps one tick, one command order and one replay. Splitting systems across threads is an optimization for when Phase 6's measurement says one thread misses, and an ADR with that figure.
3. **Triggers are evaluated on the server, not by the deputy** (horizon O5), so that a scheduled order fires while its player is online too, and a time of day enters as a tick that the host stamps as the command arrives. Decided (owner, 2026-10-08): [ADR-080](../ADR/ADR-080-events-and-scheduled-orders.md) decisions 1, 5 and 6, and [ADR-009](../ADR/ADR-009-deterministic-core.md) decision 3.
4. **Deputies run on the server's thread, stepped after each tick,** so that their commands are logged as a human's are and a world with deputies replays from its seed and its log; `GameLogic` still does not include `Opponent`. Decided (owner, 2026-10-08): [ADR-079](../ADR/ADR-079-seat-controller-and-deputy.md) decisions 1 and 2.
5. **A deputy has to start from any state:** an empire it did not build, after a hand-over or a restart, so that the AI's own state never has to be saved. Decided: [ADR-020](../ADR/ADR-020-ai-and-match-flow.md) decision 16.
6. **A player who loses restarts** rather than sitting out a season. Decided (owner, 2026-10-08): [ADR-084](../ADR/ADR-084-world-without-an-end.md) decisions 3 and 4; when a start is free is gate H11 of the Phase 5 design.
7. **Nothing ends a world but the owner:** domination, which ends a match in hours, is off in a world, the simplest form of horizon O9's season end. Decided (owner, 2026-10-08): [ADR-084](../ADR/ADR-084-world-without-an-end.md) decision 2.
8. **Ore that comes back makes the pile-up worse** (horizon §6.3). With the cap bought by levels and no upkeep, Ore after the first day buys only replacements. A bank without a bound means a lost fleet is rebuilt at once, so the cap, not income, decides how fast a player recovers, which is the opposite of the "territory is capacity and recovery" horizon §6.2 expects. A sink or a bound on the bank comes before fields that recover (gate G3).
9. **Presence is measured on both sides.** Horizon §9 measures what a present player buys in battle. A deputy also runs production and research around the clock without forgetting a queue. If it out-builds its player, being offline becomes the better strategy, the inverse of the failure the horizon names. So the deputy's economy is measured against the owner's as well, which is the Phase 5 design's W4.

On the order of the phases, this plan keeps the horizon's: the galaxy before friends. The reason is not the horizon's. A single 10 km system goes stale fast under a persistent world: every reserve runs dry and every node is claimed within the first day, and in the container's AI matches on the 10 km map every Command Station reached its top level by minute 20 ([the balance log](../Records/BalanceLog.md)). A first season with friends on one stale system would spend their goodwill on a stalemate of capped fleets on a trickle. Phase 5's week shows whether that is so; if it is not, friends in one system can come first (gate H10 of the Phase 5 design).

---

## 3. The phases

| Phase | Horizon step | Its question | Seats | Map |
|---|---|---|---|---|
| 5 — A world that persists | §11.1 | Does a world that keeps running while its player is away work at all? | The owner and one AI empire, or one friend | The 10 km system |
| 6 — A galaxy | §11.2 | Do several systems, travel between them and progression from the galaxy keep a world moving for weeks? | As Phase 5 | Generated, 10–30 systems |
| 7 — Friends | §11.3 | Does a season of weeks hold with a circle of friends and their deputies? | 3–6 friends and AI empires | Phase 6's |
| 8 — Seasons of months | §11.4 | Does a season of months keep its arc, and survive changes to the code? | As Phase 7 | Phase 6's, with an arc |

Milestones are numbered on from Phase 5's, 35 to 39, so Phase 6's start at 40. Each phase's milestones are proposed in outline here and scoped in its own plan once its design is accepted.

---

## 4. Phase 5 — A world that persists

[The Phase 5 design](OutpostCommander-Phase5.md) and [its plan](ImplementationPlan-Phase5.md) carry it: of the gates this plan proposed for it, H1 to H9 are decided as proposed (owner, 2026-10-08), and H10 is open in that design's gate table.

---

## 5. Phase 6 — A galaxy

Outlined for its design; nothing here is scoped.

### Gates

| Gate | Decision | Proposed in | Blocks | Status |
|---|---|---|---|---|
| G1 | **Progression gated by the galaxy** (horizon O1). Research tiers opened by holding systems with derelicts and a precursor vault; the derelicts that recover research are its first step ([ADR-074](../ADR/ADR-074-salvage.md) decision 5). What losing a vault does to a tier not finished, and whether the Lab's levels stay as a second gate. **Decided on 2026-10-25:** a vault lost pauses a tier not yet finished, and the topics finished are kept; the Lab's levels stay as a second gate. | this plan, §5 | Milestone 43, progression from the galaxy | decided 2026-10-25 |
| G2 | **Travel between systems** (horizon O8): a jump between linked Relays, or a gate at a system's edge; its time, and whether a fleet in transit can be met. **Decided on 2026-10-25:** a jump between linked Relays, so the territory decides where a fleet can go. Its time, and whether a fleet in transit can be met, are milestone 41's. | this plan, §5 | Milestone 41, travel between systems | decided 2026-10-25 |
| G3 | **The economy over weeks:** a sink or a bound on the bank before fields that recover (§2.8); then fields that recover, or fields that appear (horizon O3). **Decided on 2026-10-25:** a bound on the bank first, then fields that recover. | this plan, §2.8 and §5 | Milestone 44, the economy over weeks | decided 2026-10-25 |
| G4 | **The fleet cap across systems** (horizon O2): one cap per empire from its best Command Station, or a Command Station per system. **Decided on 2026-10-25:** one cap per empire, from its best Command Station. | this plan, §5 | Milestone 40, systems in one simulation | decided 2026-10-25 |
| G5 | **The galaxy's shape:** how many systems, their kinds, and how a generated galaxy is fair to N seats. Placement mirrors through the center ([ADR-072](../ADR/ADR-072-seeded-placement.md) decision 4), which is a rule for two. **Decided on 2026-10-25:** a home system for each seat, generated alike, around shared systems in the middle, which hold the vaults and the derelicts. How many systems, and their kinds, are milestone 42's. | this plan, §5 | Milestone 42, the galaxy from the seed | decided 2026-10-25 |
| G6 | **The strategic view.** One map has no strategic view (owner, 2026-10-03; Game §limits); a galaxy needs a view of systems. **Decided on 2026-10-25:** the camera zooms out past a system to the galaxy, in the same view. | this plan, §5 | Milestone 45, the client's galaxy | decided 2026-10-25 |

### Milestones in outline

- **40 — Systems in one simulation** (§2.2): a system above the sector in the map's format ([ADR-036](../ADR/ADR-036-map-sectors.md)), a pathfinding region and fog grid per system, snapshots that carry only the systems a player has presence in.
- **41 — Travel between systems**, by G2.
- **42 — The galaxy from the seed**, by G5, extending [ADR-072](../ADR/ADR-072-seeded-placement.md).
- **43 — Progression from the galaxy**, by G1.
- **44 — The economy over weeks**, by G3.
- **45 — The client's galaxy:** the strategic view, a system's selection, the minimap per system, alerts that name their system.
- **46 — The AI plays a galaxy**, deputies and AI empires alike.
- **47 — Measured:** a 30-system world's tick at the tick budget's percentile ([the budgets](../Budgets.md)), with every seat an AI; a two-week world run headlessly by `--world-run`, logging when each system is first reached, held and drained, the banks, and the share of the world where nothing happens.

**The engineering risk is the tick.** One system's tick in an AI match, at the 99th percentile [the budgets](../Budgets.md) give, is small, but 30 busy systems on one thread can miss the tick's period. Most systems will be quiet most of the time, and an empty system's tick costs little, but that is to be measured in 47, not assumed. If it misses, systems tick in parallel with an empire ledger merged between ticks, and that is an ADR.

---

## 6. Phase 7 — Friends

### Gates

| Gate | Decision | Proposed in | Blocks | Status |
|---|---|---|---|---|
| F1 | **Seats:** how many, and how a friend joins a running season or leaves it (a deputy, then an AI empire, horizon §4). | this plan, §6 | Milestone 48, seats for N players | open |
| F2 | **A season of weeks:** how it starts, how it ends (horizon O9 in its first form), and what is kept between seasons. **Decided on 2026-10-25:** a season runs a fixed number of weeks, its standings are recorded, and the next starts afresh, keeping nothing. How many weeks is milestone 49's. | this plan, §6 | Milestone 49, seasons | decided 2026-10-25 |
| F3 | **Diplomacy** (horizon O6): accepted as it comes, or designed for. Proposed: as it comes, with the world log recording who fought whom. **Decided on 2026-10-25:** as it comes, as proposed, with the world log recording who fought whom. | this plan, §6 | Phase 7's design | decided 2026-10-25 |
| F4 | **Reach** (horizon O7): the server posts to the friends' group chat through WinHTTP, in the Windows SDK, so R14 holds. Which service, and which events are worth a message. The dependency on a service is an ADR. | this plan, §6 | Milestone 50, notifications | open |
| F5 | **Bandwidth:** what a snapshot takes each connected player, as [the budgets](../Budgets.md) measure it. Proposed: acceptable for friends on broadband; if it is not, deltas, or datagrams for state that may be dropped, are a new protocol version ([ADR-060](../ADR/ADR-060-quic-in-process.md)). **Decided on 2026-10-25:** as proposed: the snapshots as they are, measured against the budgets; deltas or datagrams only if the measurement says so, as a new protocol version. | this plan, §6 | Milestone 48, seats for N players over the network | decided 2026-10-25 |

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
- **Deputies over long absences:** the Phase 5 design's W3 and W4 again, over absences of weeks.

---

## 8. What runs beside the phases

- **Self-play.** The network of [the self-play plan](ImplementationPlan-SelfPlay.md) is the benchmark for the deputy. Whether a learned policy may command a seat in a season is its gate N6 ([the network's blueprint](SelfPlayNetwork.md)). Nothing in Phases 6–8 waits on it.
- **What the container verifies.** The server, the AI, saves, scheduled orders and `--world-run` build and run in the Linux container against stand-ins. The dedicated server's QUIC over Schannel, the client's new windows and every week-long run are CI's or the owner's.
- **R14 holds throughout.** Saves are files written through `FileSys`, notifications go through WinHTTP, and the server needs no database, web server or library outside the Windows SDK. A web view of the galaxy would reopen ADR-001 and R14 (horizon O7), and is not in this plan.

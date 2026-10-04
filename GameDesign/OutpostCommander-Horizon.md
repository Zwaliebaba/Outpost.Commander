# Outpost Commander — Horizon: A Galaxy That Keeps Running

Status: **horizon, not a phase** · Owner: Stefan Zwaal · Started 2026-10-04, from the owner's answers of that day · Nothing here is planned or accepted; a phase design that takes part of it up decides it there

This document records where the owner wants the game to go in the long run, and what follows from it. It amends nothing. The [MVP](Archive/OutpostCommander-MVP.md), [Phase 1](Archive/OutpostCommander-Phase1.md), [Phase 2](Archive/OutpostCommander-Phase2.md) and [Phase 3](Archive/OutpostCommander-Phase3.md) designs say what is built and what comes next, and this document is not among the design authorities AGENTS.md names. A phase design that takes something from here becomes the authority for it, and this document is edited in place to say so.

---

## 1. What the horizon is

The owner's question of 2026-10-04: how the game could evolve towards a game like **Starborne: Sovereign Space**, as a dot on the horizon rather than a plan.

**What Starborne was.** A massively multiplayer real-time strategy game by Solid Clouds: 4,000–5,000 players on a server, a map of nearly a million hexes, rounds of two to three months, and fleets that travel for hours. It entered open beta in April 2020, could not keep its player base, and its last server closed in September 2024. Its post-mortem names bloat, and that a game which runs day and night had no way in without a client on a gaming PC: no browser and no phone (§13).

**What the owner takes from it** (owner, 2026-10-04). Not the massively multiplayer part. Two things: its **scale**, a galaxy that lasts longer than a match, and its **rhythm**, a world played a few times a day rather than in one sitting. Playing against real people is not what draws the owner to it.

**What it becomes.** A persistent world for a circle of friends, played in seasons of two to four weeks at first and of two to four months later (§7). While a player is online the game is played as it is today; while the player is away, the orders the player gave carry on. After a short ramp it is a war over territory, which is closer to PlanetSide, the reference Phase 2 §1 already names, than to Starborne's slow build-up over a season.

---

## 2. Decided by the owner on 2026-10-04

- **The draw is scale and rhythm**, not playing against real people (§1).
- **The players are a circle of friends** on one server, playing **against each other and against AI empires**.
- **A battle is played live by whoever is present**, and fought by orders for whoever is not.
- **Online, a player plays as today**: starts research, builds Mining Rigs and structures, queues ships. **Offline, fleets carry out orders given in advance**, in the owner's examples: defend against an attack, attack this station at 02:00, mine this field at 03:00. The owner's aim is depth of planning, and an empire that keeps running.
- **Every action keeps today's pace**: moving and fighting, building rigs and structures, producing ships, research, and structure levels. None moves to a slower clock.
- **A season lasts two to four weeks at first, and two to four months later**, once the world keeps its state across builds and a season's arc has been shown to hold (§7). The short seasons come first because a season is the experiment: seasons of weeks give a dozen or more a year to learn from, and seasons of months three or four.

---

## 3. One world that keeps running

*Proposed, not decided.*

- **One simulation runs all the time.** There is no pause and no separate strategy layer above the battles. Each star system is a `Simulation` of its own, on the fixed tick it has today (ADR-002, ADR-009), and a fleet that leaves one system is handed to the next.
- **What it costs, estimated, not measured.** The MVP's Q4 measured a mean tick of 0.88 ms in combat at 200 ships and 40 structures, on the development machine (MVP §3). Thirty systems at about a millisecond a tick, 20 ticks a second, take about 0.6 of one core. Ship counts over weeks are bounded only by what §6 decides.
- **The dedicated server** that ADR-002 sketches hosts it, on Windows Server 2022 or later, which MsQuic's Schannel build needs (ADR-004). The shell already reaches its server over QUIC on the loopback address, so a remote server changes the address it connects to (ADR-060).
- **A season is tied to one build.** The world is kept as periodic snapshots of every system and the command log since the last one, and ADR-009's replay on the same binary is enough to recover from a restart. A breaking patch starts a new season, so saved state never has to migrate between versions. This ends Phase 1's accepted risk 3, that there is no save and load. It holds for seasons of weeks only (§7).

---

## 4. A seat is commanded by its player or by a deputy

*Proposed, not decided.*

- **A present player commands the seat. An absent player's seat is commanded by a deputy**: `Opponent`, playing by that player's directives (its research queue, its build priorities and its fleets' orders), at the full rate, around the clock. The AI empires are the same code with directives of their own.
- **Why the deputy is needed.** Friends compete, and every action keeps today's pace (§2). Without a deputy, an empire grows with its player's hours online, and a friend with six free hours a day outgrows one who checks in three times. With one, being online buys better decisions and live battles, not more actions.
- **A friend who drops out** mid-season leaves an AI empire behind rather than a hole.
- **Deputies and AI empires run in the server process**, as clients over the `LoopbackTransport`. Today the AI runs from the shell's frame loop, which a dedicated server does not have. `Opponent` still includes only `GameProtocol` (ADR-002).
- **The risk:** a deputy that plays much worse than its player leaves presence deciding the season after all. §9 measures how much presence buys.

---

## 5. Orders that run while a player is away

*Proposed, not decided.*

- **An order meets a changed world.** An attack ordered at 18:00 for 02:00 finds whatever the enemy did in between, so an order carries a condition: "attack at 02:00, unless they have reinforced; then hold." That is where the depth of planning comes from, and also how an order system grows into a programming language.
- **The vocabulary is mostly built already:**
  - **Triggers:** a time of day, and the events Phase 2's alerts raise: a Relay suppressed, a Relay under attack, a Mining Rig lost, enemy ships in a held sector (ADR-059).
  - **Actions:** move, attack-move, attack and build, where "mine this field" is a rig built on it, and the two standing orders, hold a sector and patrol (ADR-059).
  - **Retreat:** the AI's fall-back, where an attack that has lost 10% of its ships falls back (ADR-041). Without it an absent player's fleet fights to its last ship, and under Lanchester's square law that loses everything.
- **The vocabulary is a small fixed set**: no nesting, no variables, and a new trigger or action only when a missing case keeps coming up in play. Screeps, where the orders are code, is the far end of this road.
- **Where triggers are evaluated is a new decision.** Alerts are the client's today, computed from its snapshots (ADR-059 decision 1), and that ADR forecloses alerts made by the server without a new one. With no client connected, either the server or the deputy, from its own snapshots, evaluates them.

---

## 6. What today's pace leaves open

Every action keeps today's pace (§2), so nothing stretches over a season of two to four weeks. On the first day, with today's data:

- **Research:** Phase 3's tree is 23 topics and 2,260 s, about 38 minutes with one Lab (Phase 3 §6).
- **Ore:** every reserve lasts 25–40 minutes at its yield, and then pays a 20% trickle for good (Phase 1 §8). Within a day of being taken, a system is on its trickle for the rest of the season.
- **Ships:** a Small hull builds in 10 s (`Tuning.json`), so one Shipyard working around the clock under its deputy makes thousands a day. Q4 was measured at 200 ships (MVP §3).
- **Structure levels:** Phase 3's upgrades take 30–120 s of Constructor work.

The limits and the progression of a season must therefore come from the world, not from timers. Three proposals, each open (§10):

1. **Progression gated by the galaxy.** Research is recovery (MVP §2), of the technology lost with the old fleet (MVP §1). A research topic keeps today's time, but a tier opens by holding a system with a derelict in it, and the last tier by holding a precursor vault deep in contested space. The season then moves as fast as the conquest, and a vault system is the target worth a fleet, as Phase 3 §1 makes an upgraded Shipyard worth a raid. Finished topics stay finished, as today (MVP §8); what losing a vault does to a tier not yet finished is open, and so is whether Phase 3's Lab levels stay as a second gate.
2. **Fleet size bounded by territory.** Upkeep, or a command cap tied to the nodes a player holds, as Phase 3 §7 caps nodes by the Command Station's level. A bound of some kind is unavoidable: Phase 1's accepted risk 2, that there is no unit cap, does not survive a world that produces for weeks. Its consequence: every player reaches the bound within days, and from then on income decides how fast a player recovers from a lost battle, not how large its fleet grows. Pillar 2 moves from "territory is income" to "territory is capacity and recovery".
3. **Ore that comes back.** A mined-out field recovers over a day, or new fields appear in the galaxy. That also gives "mine this field at 03:00" its meaning: a field is worth timing only if it changes over time.

---

## 7. Seasons of months

*Decided by the owner on 2026-10-04: seasons of two to four weeks first, and of two to four months later (§2). What months cost is proposed, not decided.*

Months change three things. The rest stays as it is at weeks: one world that keeps running (§3), the deputy (§4), the orders (§5), and the server's cost, under one core. The counters last too: the simulation and the snapshot count ticks in 64 bits and entities in 32, far beyond four months of production.

- **The world has to keep its state across builds.** A season of weeks can do without a breaking change. A season of months cannot, at the pace this repository moves: a crash fixed in its third week is a new binary, and ADR-009's replay holds only on the same binary. Recovery then comes from snapshots alone, and a snapshot has to load in a later build: a versioned save format for the whole of `Simulation`'s state, with an upgrade step for each version. That is the first real engineering cost of this horizon, where §3's build pin was free.
- **The ramp is still one day, so the plateau lasts months.** Two things that are optional at weeks become necessary:
  - **The galaxy needs an arc.** Progression gated by the galaxy (§6.1) has to stretch over months: regions that open at set points in the season, and a race to an end in its last weeks, for example.
  - **An early lead must not decide the season.** Over months a lead compounds, and by the third week the rest of a season can be a foregone conclusion. Phase 2's domination tickets (Phase 2 §8) would end such a season early rather than prevent it, so a season's end needs a design of its own.
- **The deputy carries more of it.** Over months, a friend who drops out is close to certain, and so is a player away for two weeks. Being online must still buy only better decisions (§4) over absences that long, not only overnight, so §9's measurement matters more.

---

## 8. What the game already has for it

- **The server model** (ADR-002): an authoritative server, fog of war applied per player on the server, and the AI as a client that sends the same commands as a human. That last rule is what lets a deputy and an AI empire fill a seat.
- **Territory** (Phase 2): nodes claimed by building on them, claims only next to what a player holds, half income for a sector cut off, and suppression by presence. It is the strategic layer of a galaxy at the scale of one system.
- **Growth** (Phase 3): structure levels, and the Command Station's level capping the nodes a player holds.
- **The designer** (MVP §7). With deputies fighting many of the battles, a fleet's composition and its orders are what a player decides, so Pillar 1 counts for more, not less.
- **Headless play**: `GameLogicTests` plays the AI against the real server, and the Q2 check fights battles without a window (ADR-020, MVP §3). That is the harness §9 needs.

---

## 9. The first measurement

How much being present buys decides how fair the deputy is (§4), and it can be measured with what exists:

1. Pick a handful of fleet matchups.
2. Run each headlessly, with the AI's battle behavior on both sides, over many seeds, as the Q2 check runs its battles. This needs a small harness.
3. The owner plays the same matchups live against the same AI.

The owner's win rate above the AI-against-AI baseline is what presence buys. If it is small, a battle fought by orders is fair enough as it is. If it is large, the battles that decide a system have to become appointments. EVE's answer would fit Phase 2: an attack on a held Relay suppresses it and starts a timer, and the deciding fight happens when the timer ends, in a window the defender chose. Phase 2's Relay that is suppressed but still held (Phase 2 §5) is half of it.

Whether a front stays interesting for weeks with a few friends and their deputies is answered only by a first season.

---

## 10. Open questions

- **O1 — Progression** (§6.1): tiers opened by holding derelicts and vaults; what losing one does; whether the Lab's levels stay.
- **O2 — The bound on fleets** (§6.2): upkeep or a command cap, and how it follows territory.
- **O3 — Ore over a season** (§6.3): fields that recover, or fields that appear.
- **O4 — The deputy's directives** (§4): what a player can tell its deputy, and how well it must play.
- **O5 — Where triggers are evaluated** (§5): the server or the deputy.
- **O6 — Diplomacy.** A free-for-all among friends and AI empires grows alliances by itself, and with them a player who decides the winner between the others. Accepted as it comes, or designed for.
- **O7 — Reach.** A check-in needs the PC. The server can post "40 ships inbound to Kessler-3, arriving 21:40" to the friends' group chat through WinHTTP, which is in the Windows SDK, so R14 is not touched, though a dependency on a service deserves an ADR. A web view of the galaxy would reopen ADR-001 and R14.
- **O8 — The world beyond one map.** A galaxy grows by systems, not by kilometers, and a relay jump becomes travel between systems. So the 10 km world, relay jumps, forward Shipyards and pathing by sector, which Phase 3's draft had moved to a Phase 4, now wait on this horizon, and Phase 3 §3 says so (owner, 2026-10-04); Phase 3's rules are unchanged. Open: which of them a galaxy of systems still needs inside a system.
- **O9 — Seasons of months** (§7): the versioned save format, the galaxy's arc over a season, and how a season ends.

---

## 11. A possible order of work

*Proposed, not decided.* Each step would be a phase design of its own, with its questions and gates, as Phases 1–3 were.

1. **A world that persists.** Today's map as one system, run without stopping by the dedicated server, recovering from a restart, with a deputy in an absent seat; the owner against the AI or one friend. Its question: does a world that keeps running while its players are away work at all?
2. **A galaxy.** Several systems and travel between them, with §6's limits and progression.
3. **Friends.** Seats, seasons of weeks and the notifications of O7.
4. **Seasons of months.** A save format that loads in later builds, the galaxy's arc and a season's end (§7).

---

## 12. Not part of it

A massively multiplayer game: public servers, live operations, moderation and monetization. Lockstep networking (ADR-002). Starborne's slow build-up over a season, given up by the decision that every action keeps today's pace (§2). Pausing the world.

---

## 13. Sources

Searched on 2026-10-04:

- [Starborne: Sovereign Space, Wikipedia](https://en.wikipedia.org/wiki/Starborne:_Sovereign_Space): open beta in April 2020, the closure of September 2024.
- [Starborne: Sovereign Space post-mortem, Kai's Game Dev Blog](https://kaiwueest.com/insights/starborne-sovereign-space/): rounds of about three months, bloat, and no way in without a gaming PC.
- [Starborne devlog, itch.io](https://itch.io/t/193543/starborne-sovereign-space-strategy-mmo-from-the-creators-of-eve-online): players per server and the size of the map.
- [Solid Clouds: Starborne enters open beta](https://www.solidclouds.com/news/starborne-sovereign-space-enters-open-beta).

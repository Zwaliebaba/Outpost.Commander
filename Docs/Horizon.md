# Outpost Commander — Horizon: A Galaxy That Keeps Running

Owner: Stefan Zwaal · It decides nothing: a phase design that takes part of it up decides it there · Its open questions are the gates of [the Horizon plan](Plans/ImplementationPlan-Horizon.md), and O9 is §10's

This document says where the owner wants the game to go in the long run, and what follows from it. It amends nothing and decides nothing. The game as built is [the living design](OutpostCommander.md), and its engineering decisions are the ADRs; this document is not among the authorities AGENTS.md names. A phase design that takes something from here becomes the authority for it, what that phase builds is the living design's, and this document keeps only what is still ahead. [The Horizon plan](Plans/ImplementationPlan-Horizon.md) proposes the phases that would build it, and [the pillars](Pillars.md) are what it is checked against.

---

## 1. What the horizon is

The owner's question (owner, 2026-10-04): how the game could evolve towards a game like **Starborne: Sovereign Space**, as a dot on the horizon rather than a plan.

**What Starborne was.** A massively multiplayer real-time strategy game by Solid Clouds: 4,000–5,000 players on a server, a map of nearly a million hexes, rounds of two to three months, and fleets that travel for hours. It entered open beta in April 2020, could not keep its player base, and its last server closed in September 2024. Its post-mortem names bloat, and that a game which runs day and night had no way in without a client on a gaming PC: no browser and no phone (§13).

**What the owner takes from it** (owner, 2026-10-04). Not the massively multiplayer part. Two things: its **scale**, a galaxy that lasts longer than a match, and its **rhythm**, a universe played a few times a day rather than in one sitting. Playing against real people is not what draws the owner to it.

**What it becomes.** A persistent universe for a circle of friends, played in seasons of two to four weeks at first and of two to four months later (§7). While a player is online the game is played as a match is; while the player is away, the orders the player gave carry on. After a short ramp it is a war over territory, closer to PlanetSide's fights over areas of space than to Starborne's slow build-up over a season. Its first step, one system that keeps running, is the game's universe (Game §universe; §11).

**A premise, proposed.** The game tells no story, and no screen names a faction (Game §limits). The one proposed for it: a century after the jump relays went dark, the colonists left on the far side, the Terrakin, rebuild in the Kessler Reach from its ore and the wrecks of a fleet that never went home, against the Tarkan High Command, which plants its own outposts there, so that research is the recovery of what was lost.

---

## 2. Decided by the owner (2026-10-04)

- **The draw is scale and rhythm**, not playing against real people (§1).
- **The players are a circle of friends** on one server, playing **against each other and against AI empires**.
- **A battle is played live by whoever is present**, and fought by orders for whoever is not.
- **Online, a player plays as in a match**: starts research, builds Mining Rigs and structures, queues ships. **Offline, fleets carry out orders given in advance**, in the owner's examples: defend against an attack, attack this station at 02:00, mine this field at 03:00. The owner's aim is depth of planning, and an empire that keeps running.
- **Every action keeps a match's pace**: moving and fighting, building rigs and structures, producing ships, research, and structure levels. None moves to a slower clock.
- **A season lasts two to four weeks at first, and two to four months later**, once the universe keeps its state across builds and a season's arc has been shown to hold (§7). The short seasons come first because a season is the experiment: seasons of weeks give a dozen or more a year to learn from, and seasons of months three or four.

---

## 3. One universe that keeps running

*The game's universe is one system (Game §universe). A galaxy of systems is what this section proposes.*

- **One system.** A universe is one system, the game's map, run without stopping by the dedicated server from its folder (Game §session.server; ADR-078). It is saved as it runs and comes back from its newest save and the commands logged since it (Game §universe.persistence; ADR-077). A save is pinned to a state version, not a build (AGENTS.md R18), and a universe pauses while its server is down (ADR-077 decision 9). The Windows the server needs is [ADR-078](ADR/ADR-078-dedicated-server.md)'s, and a game on another machine joins it over QUIC (Game §session.join; ADR-078).
- **One simulation runs all the time.** There is no pause beyond an outage, and no separate strategy layer above the battles. A galaxy is several star systems, each on the fixed tick (ADR-009), and a fleet that leaves one system enters the next. Whether each system is a `Simulation` of its own that hands the fleet on, or one `Simulation` holds them all, is O8's (§10).
- **What it costs, estimated, not measured.** Thirty busy systems at about a millisecond a tick take about 0.6 of one core at the game's tick rate. The tick's budget and its measurements are in [Budgets](Budgets.md). Ship counts over weeks are bounded by the fleet cap (Game §economy.fleetcap) and by what §6 leaves open.

---

## 4. A seat is commanded by its player or by a deputy

*A universe has a seat for each of the map's starts (Game §universe.seats). More seats are what this section proposes.*

- **Who plays a seat.** A seat is played by its player while the player is connected, and by its deputy from a minute after the player's connection is gone until the player takes the seat again (Game §universe.handover; ADR-079 decision 4). An AI empire is the AI playing a seat of its own to win (Game §ai.empire). Both are played by the server, on its thread, so a universe with deputies replays from its seed and its log (ADR-079 decision 2). The deputy is a keeper: it keeps its player's research, production, rigs and repairs going and defends the player's held sectors, and it never attacks, claims a sector or clears pirates (Game §ai.deputy; ADR-079 decision 6). A scheduled order its player gave fires whoever plays the seat (Game §orders.scheduled).
- **Why the deputy is needed.** Friends compete, and every action keeps a match's pace (§2). Without a deputy, an empire grows with its player's hours online, and a friend with six free hours a day outgrows one who checks in three times. With one, being online buys better decisions and live battles, not more actions ([Pillar 5](Pillars.md)).
- **A friend who drops out** mid-season leaves an AI empire behind rather than a hole.
- **The risk:** a deputy that plays much worse than its player leaves presence deciding the season after all, and one that out-builds its player makes absence the better strategy. The battle matchups measure how much presence buys in battle (§9), and a universe's log records every hour each seat's growth with how much of the hour its player and its deputy played it (Game §universe.log), which is where both would show.

---

## 5. Orders that run while a player is away

*Scheduled orders are the game's (Game §orders.scheduled). How their vocabulary grows is what this section proposes.*

- **An order meets a changed universe.** An attack ordered at 18:00 for 02:00 finds whatever the enemy did in between, so an order carries a condition: "attack at 02:00, unless they have reinforced; then hold." That is where the depth of planning comes from, and also how an order system grows into a programming language.
- **A scheduled order** is one trigger, one action and at most one condition, kept by the server and fired once (Game §orders.scheduled; ADR-080). Its triggers are a time of day on the player's clock and events the server raises, such as a Relay suppressed or enemy ships in a held sector; the client's alerts read the same events (ADR-080 decision 1; ADR-059 decision 1). Its actions are move, attack-move, attack, the two standing orders (Game §orders.standing) and a Mining Rig built on an asteroid, which is how "mine this field at 03:00" is given.
- **Retreat.** A ship goes back to be repaired once a hit leaves it below the threshold its design or its player set (Game §orders.retreat; ADR-075), and the AI's attack falls back once it has lost `retreatLossShare` of the ships it set out with, a setting of each difficulty's file that [the Catalogue](Reference/Catalogue.md) shows (ADR-041 decision 2). Without retreat an absent player's fleet fights to its last ship, and under Lanchester's square law that loses everything.
- **The vocabulary is a small fixed set**: no nesting, no variables, and a new trigger or action only when a missing case keeps coming up in play (owner, 2026-10-08). Screeps, where the orders are code, is the far end of this road.

---

## 6. What a match's pace leaves open

Every action keeps a match's pace (§2), so nothing stretches over a season of weeks. On a universe's first day, with the game's data, which [the Catalogue](Reference/Catalogue.md) shows:

- **Research:** the whole tree, at the research times of `research` in `Tuning.json`, is done with one Lab (Game §research).
- **Ore:** each asteroid's `reserve` in `Map.json` runs dry at its yield, and the asteroid then pays the `exhaustedYieldPercent` of `Tuning.json` for good (Game §economy.reserves). Within a day of being taken, a system is on its trickle for the rest of the season.
- **Ships:** a hull builds in its `buildSeconds` in `Tuning.json`, so one Shipyard working around the clock under its deputy could make hundreds a day; the fleet cap, not production, bounds a fleet (Game §economy.fleetcap).
- **Structure levels:** an upgrade builds itself in the time its level gives in `Tuning.json` (Game §structures.levels).

The limits and the progression of a season must therefore come from the universe, not from timers. Three proposals, each open (§10):

1. **Progression gated by the galaxy** (O1). Research is recovery ([Pillar 3](Pillars.md)), of the technology lost with the old fleet. A research topic keeps its time, but a tier opens by holding a system with a derelict in it, and the last tier by holding a precursor vault deep in contested space. The season then moves as fast as the conquest, and a vault system is the target worth a fleet, as an upgraded Shipyard is worth a raid (Game §structures.levels). A derelict that recovers research is its first step (Game §neutrals.salvage). Finished topics stay finished, as in a match (Game §research); what losing a vault does to a tier not yet finished is open, and so is whether the Lab's levels (Game §research.tiers) stay as a second gate.
2. **The bound on a fleet** (O2). A fleet is bounded by a cap on command points that the Command Station's level buys, and ships are dear (owner, 2026-10-06; Game §economy.fleetcap). Over a season every player reaches its cap on the first day, and from then on income decides how fast a player recovers from a lost battle, not how large its fleet grows: territory buys recovery more than size ([Pillar 2](Pillars.md)). A bank that nothing drains (Game §economy.bank) weakens even that, since a lost fleet is rebuilt from the bank at once. What a galaxy asks of the cap is how it works across systems.
3. **Ore that comes back** (O3). A mined-out field recovers over a day, or new fields appear in the galaxy. That also gives "mine this field at 03:00" its meaning: a field is worth timing only if it changes over time. A bank that nothing drains makes Ore that comes back pile up the more, so [the Horizon plan](Plans/ImplementationPlan-Horizon.md) puts a sink or a bound on the bank before it.

---

## 7. Seasons of months

*Seasons of two to four weeks come first, and of two to four months later (owner, 2026-10-04; §2). What months cost is what this section proposes, and §10's O9 is the owner's.*

Months change three things. The rest stays as it is at weeks: one universe that keeps running (§3), the deputy (§4), the orders (§5), and the server's cost, under one core. The counters last too: the simulation and the snapshot count ticks in 64 bits and entities in 32, far beyond four months of production.

- **A universe has to load in a build that changes what it holds.** A save loads in any build of its state version (Game §universe.persistence; ADR-077), so a fix that changes nothing the universe holds ships mid-season. A season of months also meets changes to what the universe holds, at the pace this repository moves, and then a universe comes back only with an upgrade step for each state version, and a test that loads a save of every version a running season has. That is a cost on every later change to the server, and the first real engineering cost of this horizon.
- **The ramp is still one day, so the plateau lasts months.** Two things that are optional at weeks become necessary:
  - **The galaxy needs an arc.** Progression gated by the galaxy (§6.1) has to stretch over months: regions that open at set points in the season, and a race to an end in its last weeks, for example.
  - **An early lead must not decide the season.** Over months a lead compounds, and by the third week the rest of a season can be a foregone conclusion. Domination (Game §territory.domination), which a universe turns off, would end such a season early rather than prevent it, so a season's end needs a design of its own.
- **The deputy carries more of it.** Over months, a friend who drops out is close to certain, and so is a player away for two weeks. Being online must still buy only better decisions (§4) over absences that long, not only overnight, so §9's measurement matters more.

---

## 8. What the game already has for it

- **The server model** (ADR-002): an authoritative server, fog of war applied per player on the server, and the AI as a player that sends the same commands as a human. That last rule is what lets a deputy and an AI empire fill a seat.
- **A universe** (Game §universe): one system that keeps running on the dedicated server, saved and recovered, with a deputy in an absent seat, scheduled orders, and a player who loses restarting rather than leaving.
- **Territory** (Game §territory): nodes claimed by building on them, claims only next to what a player holds, a share of the income for a sector cut off, and suppression by presence. It is the strategic layer of a galaxy at the scale of one system.
- **Growth** (Game §structures.levels; Game §territory.cap): structure levels, and the Command Station's level capping the nodes a player holds and its fleet.
- **The designer** (Game §ships.designer). With deputies fighting many of the battles, a fleet's composition and its orders are what a player decides, so [Pillar 1](Pillars.md) counts for more, not less.
- **Headless play.** `GameLogicTests` plays the AI against the real server, and the balance check fights battles without a window (ADR-014; ADR-033 decision 6). `--universe-run` runs a universe with an AI in every seat, killed and recovered at random ticks (ADR-082 decision 4), and `--matchups` fights the battle matchups AI against AI (ADR-083 decision 5). That is the harness §9 and every later phase measure with.

---

## 9. The first measurement

How much being present buys decides how fair the deputy is (§4). The battle matchups measure it (Game §session.matchup; ADR-083): a handful of fleet matchups, each fought headless over many seeds with the AI's battle behavior on both sides for the baseline, and played live by the owner against the same AI. The owner's win rate above the AI-against-AI baseline is what presence buys.

If it is small, a battle fought by orders is fair enough as it is. If it is large, the battles that decide a system have to become appointments. EVE's answer would fit the territory: an attack on a held Relay suppresses it and starts a timer, and the deciding fight happens when the timer ends, in a window the defender chose. A Relay that is suppressed but still held (Game §territory.suppression) is half of it.

Whether a front stays interesting for weeks with a few friends and their deputies is answered only by a first season.

---

## 10. Open questions

Each is the owner's. A phase design that takes one up decides it there. Until then each open one is a gate of [the Horizon plan](Plans/ImplementationPlan-Horizon.md), which proposes the phase that would take it up, and is listed there rather than here; O9 alone is a gate here. Where the game already answers part of a question, the living design says how, and the question is what is left.

- **O1 — Progression** (§6.1).
- **O2 — The bound on fleets** (§6.2). A fleet is bounded by a cap on command points that the Command Station's level buys, not by the nodes a player holds, and its ships are dear (owner, 2026-10-06; Game §economy.fleetcap; ADR-071). What is left is how the cap works across a galaxy's systems.
- **O3 — Ore over a season** (§6.3), with a sink or a bound on the bank first.
- **O4 — The deputy's directives** (§4). In a universe the deputy is a keeper (owner, 2026-10-08; Game §ai.deputy; ADR-079 decision 6).
- **O5 — Where triggers are evaluated** (§5). The server evaluates them, and the client's alerts read the same events (owner, 2026-10-08; Game §orders.scheduled; ADR-080 decision 1; ADR-059 decision 1).
- **O6 — Diplomacy.**
- **O7 — Reach.** A web view of the galaxy would reopen ADR-001 and R14.
- **O8 — The universe beyond one map.** A universe is one system, the game's map, `sizeMeters` in `Map.json`, and a galaxy's systems are each as large (owner, 2026-10-06; Game §map; Game §universe.overview); a Shipyard and a Repair Bay are built only in a held sector (Game §territory.gives). What is left is how a fleet travels between systems, whether a galaxy is one `Simulation` or one for each system, and whether a system needs pathing by sector, which the tick's budget in Release decides ([Budgets](Budgets.md)).
- **O9 — Seasons of months** (§7), the gate below.

| Gate | Decision | Proposed in | Blocks | Status |
|---|---|---|---|---|
| O9 | **Seasons of months** (§7): upgrade steps between state versions, the galaxy's arc over a season, how a season ends, and how far a deputy carries a seat over absences of weeks. | this document | Seasons of months (§7; §11) | open |

---

## 11. A possible order of work

Each step would be a phase design of its own, with its questions and gates.

1. **A universe that persists.** The game's map as one system, run without stopping by the dedicated server, recovering from a restart, with a deputy in an absent seat; the owner against the AI or one friend. Its question: does a universe that keeps running while its players are away work at all? It is the game's universe (Game §universe), and what a week of it shows is [the open phase's design](Plans/OutpostCommander-Phase5.md)'s to ask.
2. **A galaxy.** Several systems and travel between them, with §6's limits and progression, and a strategic view of the systems. Whether one system needs a strategic view of its own, a run of the game's map shows ([the PlaytestLog](Records/PlaytestLog.md#phase-4s-runs)).
3. **Friends.** More seats, maps with spare starts so that a seat that loses can restart at another start than its own (Game §universe.restart), seasons of weeks, and the notifications of O7.
4. **Seasons of months.** A save that loads in later builds of another state version, the galaxy's arc and a season's end (§7).

Which comes second, a galaxy or friends, the owner decides from a week of play in a universe ([the open phase's design](Plans/OutpostCommander-Phase5.md)).

---

## 12. Not part of it

A massively multiplayer game: public servers, live operations, moderation and monetization. Lockstep networking (ADR-002). Starborne's slow build-up over a season, given up by the decision that every action keeps a match's pace (§2). Pausing the universe, beyond its pause while its server is down (ADR-077 decision 9).

---

## 13. Sources

Searched on 2026-10-04:

- [Starborne: Sovereign Space, Wikipedia](https://en.wikipedia.org/wiki/Starborne:_Sovereign_Space): open beta in April 2020, the closure of September 2024.
- [Starborne: Sovereign Space post-mortem, Kai's Game Dev Blog](https://kaiwueest.com/insights/starborne-sovereign-space/): rounds of about three months, bloat, and no way in without a gaming PC.
- [Starborne devlog, itch.io](https://itch.io/t/193543/starborne-sovereign-space-strategy-mmo-from-the-creators-of-eve-online): players per server and the size of the map.
- [Solid Clouds: Starborne enters open beta](https://www.solidclouds.com/news/starborne-sovereign-space-enters-open-beta).

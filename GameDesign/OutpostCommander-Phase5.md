# Outpost Commander — Phase 5 Design: A World That Persists

Status: **accepted** (owner, 2026-10-08) · Owner: Stefan Zwaal · It holds Phase 5's open questions and its gates: the week's questions W1 to W6 (§2), and gates H10 and H11 (§3) · The order of the work is [the Phase 5 plan](ImplementationPlan-Phase5.md)

What Phase 5 builds is in [the living design](OutpostCommander.md) and the ADRs: a world (Game §world), its dedicated server and how a game joins it (Game §session.server; Game §session.join), the deputy and the AI empires (Game §ai.deputy; Game §ai.empire), scheduled orders (Game §orders.scheduled), the world log and the world run (Game §world.log; ADR-082), and the battle matchups (Game §session.matchup; ADR-083). Its engineering decisions are ADR-077 to ADR-080 and ADR-082 to ADR-084. This document holds only what is still open.

---

## 1. The question

A world is a match that does not stop, on the game's map, with two seats: the owner and an AI empire, or the owner and one friend (Game §world). It is the first step of [the horizon](OutpostCommander-Horizon.md) (its §11). The question Phase 5 answers: **is a world that keeps running while its players are away a game, and what does being present buy?**

What could go wrong, and where it shows:

- **A front that does not move.** A world has no domination (Game §world.overview), which ends the AI-against-AI matches on the game's map ([the BalanceLog](Records/BalanceLog.md)), and nothing else ends a stalemate. W6 is where it shows.
- **The Ore piles up.** The bank has no limit and nothing drains it (Game §economy.bank), and a world runs for days. The world log records each seat's bank every hour (Game §world.log), and whether the bank needs a sink or a bound is decided with a galaxy ([the Horizon plan](ImplementationPlan-Horizon.md)).
- **A deputy that plays much worse, or much better, than its player** leaves presence deciding the world, or makes absence the better strategy. W3 and W4 measure both.
- **State that does not survive.** A save that misses a field restores a different world, and nothing a player sees says so. ADR-077's tests compare a restored world with one that never stopped, and W1 is the week's test of it.

## 2. The week's questions

| # | Question | How we know |
|---|---|---|
| W1 | Does a world survive a week? | The owner's world runs for 7 days on the dedicated server, through at least one planned restart and one killed process, and loses no command the log had written. |
| W2 | What does persistence cost? | Saving a late world takes under 50 ms of the server's thread. A tick's 99th percentile stays at most 5 ms on the development machine in Release, with the deputies and the saves in ([Budgets](../Design/Budgets.md)). The save's size is recorded. |
| W3 | Does a deputy keep an empire running? | The share of an absent player's time that its Shipyards and Lab stand idle with Ore to spend, against the AI's own over the same hours. |
| W4 | What does presence buy? | The battle matchups, fought AI against AI for the baseline and played live by the owner against the same AI (Game §session.matchup; [the horizon](OutpostCommander-Horizon.md) §9); and the owner's income, fleet against its cap and research done per hour, against its deputy's. |
| W5 | Do orders run while away? | Each trigger, action and condition fires on its tick and decides as written, in tests; the owner judges a 02:00 attack in play. |
| W6 | Is the rhythm a game? | The owner judges, over W1's week, whether three check-ins a day are worth making. |

W1, W4 and W6 are the owner's week, and so is W5's attack in play. W2's tick is the owner's run in Release; the container measures the rest of W2, and W3. Each answer is recorded where its kind lives: a run in [the PlaytestLog](Records/PlaytestLog.md), a balance figure in [the BalanceLog](Records/BalanceLog.md), and a cost against its target in [Budgets](../Design/Budgets.md).

## 3. Gates

| Gate | Decision | Proposed in | Blocks | Status |
|---|---|---|---|---|
| H10 | Whether a galaxy or friends come next, decided from W6 and the week's world log (§4) | [the Horizon plan](ImplementationPlan-Horizon.md) | The next phase design: a galaxy, or a season with friends on one system ([the horizon](OutpostCommander-Horizon.md) §11) | open |
| H11 | Whether a start with enemy warships on it is free, or a restarted base is spared for a while (§5) | [the Phase 5 plan](ImplementationPlan-Phase5.md) | A change to when a lost seat restarts, and to what meets it there (Game §world.restart; ADR-084 decision 4); nothing built waits on it | open |

## 4. H10: a galaxy or friends next

The horizon puts a galaxy before friends (its §11). The Horizon plan keeps that order because a single system may go stale in a world that runs for weeks: its reserves run dry, its nodes are claimed, and its fleets sit at their caps. A first season with friends on a stale system would spend their goodwill on a stalemate. The week shows whether one system goes stale; if it does not, friends on one system can come first.

## 5. H11: a start that enemy warships hold

A seat that loses in a world restarts at its own start an hour of ticks later, once the start is free: no other player holds its home sector, and no other player's structure stands in it (ADR-084 decision 4; Game §world.restart). Enemy warships on the start do not keep it from being free, so the new base is placed among them. In the world run's day of world, the warships that had taken a base took most of the restarted bases again within seconds ([the BalanceLog](Records/BalanceLog.md)).

The question is the owner's: whether that stands; whether a start is not free while enemy warships hold it, so that the seat waits until they leave or are destroyed, as it waits for an enemy structure; or whether a restarted base is spared for a while. Until it is decided, the rule is ADR-084 decision 4's. A restart at another start waits on a map with spare starts ([the horizon](OutpostCommander-Horizon.md) §11).

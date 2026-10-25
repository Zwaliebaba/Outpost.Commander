# Outpost Commander — Pillars

Owner: Stefan Zwaal · Drafted from the game as built and [the horizon](Horizon.md); whether the owner accepts them is §6's gate

A pillar is what a design decision is checked against. Each pillar below is a one-line statement, what it asks of a decision, and the chapters of [the living design](OutpostCommander.md) it bears on. Where the game as built strains a pillar, the pillar says so: a strain is for the owner to weigh, and a pillar does not hide it. What the game is, rule by rule, is the living design (Game §overview).

---

## 1. Your fleet is your design

**A ship is built from parts the player picks, and the best design depends on what the enemy fields.**

It asks that every hull, drive and weapon have a stage of the match where it is worth building, that every design have a counter, and that no research topic be a trump card, so that changing a design mid-match is a real decision. The balance check holds the simulation to it in battles between two clumps (ADR-014; ADR-033 decision 6), and its results are in [the BalanceLog](Records/BalanceLog.md).

Bears on: Game §ships, Game §combat, Game §research, Game §ai.counters.

**Where the game strains it.** The balance check cannot feel what a battle between two clumps does not show, such as a drive's speed or a module, and leaves them out (ADR-033 decision 6). A deputy gives each Shipyard again the design it last queued (Game §ai.deputy), so while its player is away a fleet does not change its design in answer to what the enemy fields.

## 2. Territory is income

**Ore comes from asteroids in the sectors a player holds, and production follows the territory.**

It asks that something between the bases be worth fighting for, so that a match is fought along a front rather than settled by one siege, and that expanding, defending and raiding Mining Rigs and Relays be the strategic layer. A Mining Rig, a Shipyard and a Repair Bay are built only in a held sector, and a rig earns only while its sector is held (Game §territory.gives), so losing ground costs income and reach, not only the structures lost.

Bears on: Game §territory, Game §economy, Game §map, Game §structures.building.

**Where the game strains it.** Once a player's fleet fills its cap, income decides how fast it recovers from a lost battle, not how large its fleet grows, and the bank has no limit and nothing drains it (Game §economy.fleetcap; Game §economy.bank); [the horizon](Horizon.md) §6.2 follows this over a season. A world has no domination (Game §world.overview), so there holding more ground does not, by itself, win.

## 3. Research is recovery

**Research recovers the technology lost with the old fleet, and what a player has researched decides which designs it can build.**

It asks that research unlock components and upgrades, so that a player's choices visibly change what it builds in the mid-game, and that a finished topic stay finished. It also asks that technology be found on the map as well as in the Lab, as a derelict's salvage recovers research (Game §neutrals.salvage).

Bears on: Game §research, Game §structures.lab, Game §neutrals.derelicts, Game §ships.

**Where the game strains it.** Every action keeps a match's pace in a world too ([the horizon](Horizon.md) §2), so the whole tree is done on a world's first day (the horizon §6), and research paces a match but not a world. The horizon proposes tiers that the galaxy opens, in its §6.1.

## 4. Readable at a glance

**A flat battlefield under an RTS camera, where every player sees who holds what and how the match stands.**

It asks that what a player needs in order to decide be on the screen, in the world or on the HUD, without opening a window, and that each mark and color have one meaning (ADR-085). Fog of war hides where the enemy's forces are, never the state of the match: the territory is always seen, and drawn over the fog (Game §territory.shown; Game §sight.drawn).

Bears on: Game §interface, Game §sight, Game §territory.shown, Game §look, Game §map.plane.

**Where the game strains it.** Under fog a player does not always see who is where: an enemy ship is seen only while it is in sight, and an enemy structure as it was last seen (Game §sight). What every player always sees is the territory and, in a match, both players' tickets (Game §territory.domination). A world has no tickets, so there the territory alone says who is winning. A player in a world's second seat sees its own ships in the Tarkan set's orange-red in the world and in blue on the minimap; which of the two a player's own side should be is the owner's to decide ([open decisions](Reference/OpenDecisions.md)).

## 5. Being there buys decisions, not actions

**A world keeps running while its players are away, at the pace of a match, and being present buys better decisions and live battles, not more actions.**

It asks that nothing move to a slower clock to fit a world (owner, 2026-10-04), and that whatever keeps an empire running while its player is online have a form that runs while the player is away: a deputy's rule or an order given in advance (Game §ai.deputy; Game §orders.scheduled). The depth of planning comes from orders that meet a changed world, in a small fixed vocabulary rather than a language (owner, 2026-10-08).

Bears on: Game §world, Game §ai.deputy, Game §orders.standing, Game §orders.scheduled, Game §orders.retreat, Game §session.server.

**Where the game strains it.** The deputy is a keeper: it defends what its player holds and gives its Shipyards again what they last queued, but it never attacks, claims a sector or clears pirates (Game §ai.deputy), so a present player gains ground that an absent one only holds. How much presence buys is what the battle matchups measure in battle (Game §session.matchup), and what a world's log records every hour, each seat's growth with how much of the hour its player and its deputy played it (Game §world.log).

---

## 6. Gate

| Gate | Decision | Proposed in | Blocks | Status |
|---|---|---|---|---|
| P1 | The pillars, as proposed here | this document | nothing | open |

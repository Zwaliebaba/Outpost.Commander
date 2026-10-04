# Outpost Commander — Phase 2 Design: Territory

Status: **accepted** · Owner: Stefan Zwaal · Started 2026-10-03, from the owner's answers of that day · Revised 2026-10-03 after the owner's review · Accepted 2026-10-03, with every gate decided (§13) · Built and merged on 2026-10-04, with S1–S4 recorded for two AIs in §2 · Run by the owner on 2026-10-04, and S5 answered

This document says what Phase 2 builds on top of Phase 1, and amends [the Phase 1 design](Archive/OutpostCommander-Phase1.md) and [the MVP design](Archive/OutpostCommander-MVP.md) where they differ. The owner accepted it on 2026-10-03 and decided its gates (§13); the order of the work is [the Phase 2 plan](ImplementationPlan-Phase2.md). Its one effect on Phase 1 was that Phase 1's 5 km map is laid out in sectors, so that Phase 2 is played on it (§11).

---

## 1. What Phase 2 is for

The owner's question of 2026-10-03: **today a match is two players building ships and, in the end, one siege of the enemy's base. There are no real skirmishes along the way.** The owner wants the fights of Warzone 2100, where a scout meets a scout in the first five minutes and a small enemy outpost is a target halfway through, and, beyond that, a much bigger world with fights over big areas of space to control, as in PlanetSide.

**Why the game sieges today.** It is not a missing feature; the rules make a siege the right play. *This is the game as it stood when Phase 2 began, on 2026-10-03; §2 says what Phase 2 changed of the AI's numbers.*

- **Nothing in between is worth fighting for.** The home asteroids stand 260 m from the Command Station, under one Defence Platform's cover. The rings pay 5 Ore a second at home, 6 near, 8 contested and 10 rich (ADR-036), and Phase 1's depletion (Phase 1 §8) only begins to push a player outward.
- **A small fight is a bad trade.** Battles between clumps follow Lanchester's square law, so concentration wins; the armed Command Station beats seven raiders and a Defence Platform five. A raid of three ships feeds the defender.
- **Winning a small fight earns nothing back.** A rig destroyed costs its owner 50 Ore.
- **Seeing the enemy means fighting it.** Sight is a ship's weapon range and 50 m (ADR-024): a Small Mass Driver ship sees 170 m.
- **The AI masses before it moves.** It attacks with a group of 20 warships, 12 more for each tier opened, and regroups for 120 seconds after a fall-back (ADR-041, ADR-047); the player meets the game through the AI.

**What makes it work in PlanetSide is players, not area.** Thousands of people each run one soldier, so a continent is full of fights. Here one commander runs an army, and a bigger map alone gives long travel, losses nobody saw, and still one decisive battle at the end. So Phase 2 makes **territory** the thing that is fought over, a **lattice** that keeps several fights going along a front, and the **tools to command it**, on the map the owner already has. **The world grows in Phase 3** (§7), once Phase 2 has shown that territory works (owner, 2026-10-03).

---

## 2. What Phase 2 must show

These play the part of Phase 1's P1–P5. A failed answer is still a result. Each question that depends on the owner's play also has a figure from AI-against-AI matches, so that it has an answer whether or not the owner measures it (owner, 2026-10-03).

| # | Question | How we know |
|---|---|---|
| S1 | Is there contact early? | In a match the owner plays against the AI without rushing, the first shot is fired by minute 5. Over 10 seeded AI-against-AI matches, the first shot is fired by minute 5 in at least 8. The match log records it. |
| S2 | Are there skirmishes before the decisive battle? | The match log records each engagement: ships of both sides firing within one sector. Before minute 20 there are at least five, in at least three sectors, in the owner's match and in the median of the 10 AI-against-AI matches. |
| S3 | Does territory decide matches? | Over the same 10 matches, both endings happen at least once: domination, and losing the Command Station and every Shipyard (§8). |
| S4 | Does a match still last 45–60 minutes? | The median of the 10 AI-against-AI matches is 45–60 minutes, as Phase 1's P1 (owner, 2026-10-03). |
| S5 | Can one commander follow it? | The owner judges the alerts and the standing orders (§9) in play. The strategic view is not in Phase 2 (gate J5). |

**Where they stand on 2026-10-04, after plan task 19.2.** The AI-against-AI figures are recorded as the Linux container measured them, with the match log of ADR-038 and the AI of ADR-020 decision 13 and ADR-041. Floats replay only on the same build (ADR-009), so MSVC's build may play the same seeds differently. The owner ran Phase 2 on 2026-10-04 and judged that it works. That match was not measured with the match log, so S1, S2 and S4 rest on the two AIs' figures.

- **S1, contact: answered for two AIs.** All 10 of seeds 1–10 have a shot by minute 5, and all 40 of seeds 1–40.
- **S2, skirmishes: answered for two AIs.** Before minute 20, seeds 1–10 have a median of 16.5 engagements in 6 sectors, and every one of the 40 has at least five in three sectors.
- **S3, territory decides: yes.** Of seeds 1–10, 6 end by domination and 4 by production; of seeds 1–40, 27 and 13.
- **S4, the length: met over seeds 1–40, 10 seconds short over seeds 1–10.** Seeds 1–40 end at a median of 47:55, 25 of them within 45–60 minutes, every one ended (ADR-041). Seeds 1–10 alone give 44:50. Phase 1's P1 was measured over seeds 1–40 (ADR-041), and with S4 asking for the same, 19.2 tuned against the 40 rather than the ten. It took the AI's fall-back from 30% losses and 120 s to 10% and 240 s, its own settings, and the owner kept the 240 s on 2026-10-04: with territory's play at its starting values two AIs ended a match in a median of 20:23.
- **S5, one commander: yes.** The owner's judgement in play, on 2026-10-04: "Phase 2 works fine."

---

## 3. Decided by the owner on 2026-10-03

- **Territory is Phase 2, on Phase 1's map.** Phase 1's 5 km map is laid out in nine sectors with a node each (ADR-036), and Phase 2 is played on it. The 10 km world, pathing by sector, forward Shipyards and relay jumps are Phase 3 (§7).
- **A match lasts 45–60 minutes**, Phase 1's target. Territory makes those minutes busier, not longer.
- **A node is claimed by building a relay on it, and contested by attacking the relay** (§5). A Constructor builds a Relay on a free node; the enemy must destroy it to take the node. Enemy warships near a relay suppress it while they stay (§5).
- **The home sector cannot be suppressed.** The Command Station holds it from the first tick, and enemies near it stop nothing but what they shoot (§4).
- **Ore belongs to sectors, flanks included.** A rig may stand only in a sector its player holds, so a player's near asteroids, in its two flank sectors, are mined only after it has built a Relay there. The flank Relays are a match's first territory decisions (§4).
- **A suppressed Relay is still held.** Suppression stops the sector's income and the Relay's sight; ownership, the lattice and domination are unchanged until the Relay is destroyed (§5, §6, §8).
- **Relays do not repair ships** in Phase 2. Repair waits for a module of its own (§10).
- **A match ends by losing the base, or by domination** (§8). Phase 1's condition stays: a player without a Command Station or a finished Shipyard loses. Beside it, holding more of the nodes wins, drained in proportion to the share held, so territory decides most matches and the siege stays possible.
- **Scouts see further through an optional module** (§10). A design gains a fourth slot, empty by default; its first module is a Sensor Array that lets a ship see far beyond its weapon.

---

## 4. Sectors and nodes

*Decided (§3, ADR-036, gate J1).*

- **The map is Phase 1's**: 5 km a side in nine sectors on a grid of thirds, each about 1.7 km across, bounded by asteroid fields with passages (ADR-036). Each sector has **one node site** at its center, where a Relay can stand; each home's is at its start.
- **Two sectors are adjacent when they share a border**, as `Map.json` and `MapTests` already have it.
- **Each player's start is its home sector**, and its Command Station holds that sector's node from the first tick. A Command Station counts as a relay for the lattice and for domination, but **it is never suppressed**: the home sector always earns (owner, 2026-10-03).
- **Ore belongs to sectors.** A Mining Rig may be built only in a sector its player holds, and **a rig earns only while its player holds the sector and the sector is not suppressed.** This is a sector's income, and the whole of it: a Relay earns nothing itself. The sector's asteroids, Phase 1's depletion included, are income only for whoever holds it.
- **The opening.** Each home sector has its three home asteroids. A player's near asteroids, two in each of its flank sectors, wait for a Relay in that flank; the contested asteroids round the center and the rich ones in the two empty corners wait for a Relay there.

---

## 5. Relays: claiming and contesting a node

*Decided (§3, gate J2): the numbers are starting values.*

- **A Relay is a structure a Constructor builds on a free node site** of a sector adjacent to one its player holds (§6). About 200 Ore and 40 s of a Constructor's work; 3,000 hit points, armor 10, unarmed. That is tougher than a Shipyard, on purpose: a raid suppresses a Relay rather than destroys it, and destroying one takes a force. A Defence Platform beside it is how a player fortifies a node.
- **While its Relay stands, a player holds the sector**: its rigs there earn (§4), and it sees the whole sector.
- **A Relay is suppressed** while an enemy warship is within 400 m of it and none of its owner's is. A suppressed Relay's sector earns nothing, and the Relay sees only as a structure does, so raiders hurt a sector without destroying anything, and a defender has to come and fight. Suppression lasts as long as the enemy stays. A suppressed Relay is still held (§3).
- **A destroyed Relay frees its node**, and the sector's rigs stop earning until their player holds it again. It leaves no salvage (gate J7).

---

## 6. The lattice

*Decided (§3, gate J1).*

- **A Relay can only be built on a node adjacent to one its player holds.** A suppressed Relay counts. Territory therefore grows outward from the home sector and is taken back along a front, not leapfrogged.
- **A Relay can be attacked anywhere.** A fast group can still raid deep, suppress a sector or destroy its Relay, which is a skirmish worth having, but it cannot claim the node unless the sector is next to its own.
- **A sector cut off** from its owner's home sector through the lattice, by destroyed Relays, **earns half** until the link is restored. A suppressed Relay keeps the link. This is what makes a pincer pay, and what a front is for.

---

## 7. The world: Phase 3

*Deferred by the owner on 2026-10-03.* What follows is kept as the starting point for Phase 3, which is designed after Phase 2's answers.

- **A map of about 10 × 10 km**, four times Phase 1's, with about 25 sectors, point-symmetric, its front between the two homes three to five sectors wide.
- **Travel takes minutes.** A Small+Ion ship crosses 14 km in about three minutes, a Large+Fusion in about twelve. That is the reason for forward Shipyards (a Shipyard in any sector its player holds) and for relay jumps: a group at one of its player's Relays jumps to another after a 10 s charge, with a cooldown.
- **Pathing has to plan by sector.** One visibility graph over the whole map takes about 4 ms to build on the 2.3 km map, on the development machine (ADR-032). A path is planned first across sectors, then within each sector on its own graph, built and dropped sector by sector.
- **Its question is the engine's at that scale**, as Phase 1's P4: ticks within 5 ms at the ship counts its matches reach.

---

## 8. Winning

*Decided (§3, gate J4): the numbers are starting values.*

- **Losing the base** is Phase 1's condition, unchanged: a player with neither a Command Station nor a finished Shipyard loses.
- **Domination, in proportion to the share held** (owner, 2026-10-03). Each player starts with 1,000 tickets. Every 10 seconds, the player who holds fewer nodes loses 30 × the difference ÷ the number of nodes on the map. A player whose tickets reach zero loses. A node nobody holds counts for no one.
- **What that gives on nine nodes**, proposed so that a clear lead ends a 45–60 minute match and a narrow one does not: a lead of one node drains a side in 50 minutes, two in 25, three, a third of the map, in about 17, and five in 10. The same shares give the same times on Phase 3's 25 nodes.
- Suppressed Relays count for their owner, so a domination win has to be taken, not raided. The home sector always counts.

---

## 9. Commanding at this scale

*Decided (gate J5): alerts and standing orders are Phase 2's, and the strategic view is not.* Without these, a front of nine sectors is losses nobody saw.

- **Alerts.** A short message and a mark on the minimap when a Relay is suppressed or attacked, a rig is lost, or an enemy group enters a held sector; a key jumps the camera to the latest.
- **A strategic view, not in Phase 2** (gate J5). Zooming out beyond the RTS camera's 1,600 m limit (Phase 1's gate H8) to a map of sectors: who holds what, what is suppressed, where groups are.
- **Standing orders.** "Hold this sector": a group returns to the sector's Relay after it chases, and answers any enemy in the sector. "Patrol": between two points, attacking what it meets.
- **Relay jumps** are Phase 3's, with the larger map that needs them (§7).

---

## 10. Modules

*Decided (§3, gate J6): the numbers are starting values.*

- **A design has an optional fourth slot, the module**, empty by default, beside hull, drive and weapon. The designer window from Phase 1's mockup gains a fourth row, a saved design gains a field, and a design's abbreviation gains its module's initials when it has one.
- **The first module is the Sensor Array**: the ship sees 700 m, whatever its weapon; costs 40 Ore; and slows its ship by 10%. On a Small+Ion hull it is the minute-3 scout; on a heavy it is a spotter for the Rail Cannon.
- **Later modules use the same slot**: a repair module, extra plating, a jammer that hides a group from enemy sensors. Each is its own decision; repair is the one §3 points to.
- **The balance check leaves out modules a battle between clumps cannot see**, as it leaves out the Pulse Drive's speed (Phase 1 gate H4): a design with a Sensor Array is not required to be worth building.

---

## 11. What Phase 1 did for it

Phase 1's 5 km map (Phase 1 §8, task 11.2) is laid out in nine sectors, each with a node site, and the map data names the sectors, their node sites and their adjacency (ADR-036). Phase 1 reads none of it; Phase 2 is played on it.

**Carried over from Phase 1** (owner, 2026-10-03), for Phase 2's plan: task 7.3, a group given an Attack order on one enemy keeps its lanes round an obstacle, as a group given a Move or an attack-move order does since task 9.7 (ADR-047).

---

## 12. The AI

The node graph is what an AI can reason about: the threat in each sector, the weakest node adjacent to its own, where to reinforce. Phase 2's AI:

- builds Relays on its two flanks early, since its near asteroids wait for them (§4);
- sends a scout with a Sensor Array in the first two minutes;
- claims nodes adjacent to its territory, and fortifies the ones on the front with a Defence Platform;
- raids a sector it sees weakly held with a few fast ships, suppressing it, and pulls back when it loses;
- holds its sectors with standing orders;
- masses for a main attack only when it has a lead in nodes or the enemy's front has thinned.

---

## 13. Gates

Each was a gate for Phase 2's plan. The owner decided all of them on 2026-10-03, when the design was accepted.

- **J1 — The cut-off penalty** (§6): a cut-off sector earns half. **Decided:** as proposed.
- **J2 — The Relay's numbers and suppression** (§5): 200 Ore and 40 s, 3,000 hit points and armor 10, and the 400 m suppression radius. **Decided:** as proposed, as starting values.
- **J3 — The world** moved to Phase 3 (§7). **Decided.**
- **J4 — Domination's numbers** (§8): 1,000 tickets, and 30 × the node difference ÷ the map's nodes every 10 s. **Decided:** as proposed, as starting values.
- **J5 — Commanding at scale** (§9). **Decided:** alerts and standing orders are in Phase 2; the strategic view is not.
- **J6 — Modules** (§10): the Sensor Array's 700 m sight, 40 Ore and 10% speed. **Decided:** as proposed, as starting values; no other module follows it in Phase 2.
- **J7 — Salvage** (§5). **Decided:** none. A destroyed Relay, rig or platform leaves nothing.

Decided earlier on 2026-10-03, from the owner's review of the draft: territory on Phase 1's map and the world in Phase 3; the 45–60 minute target; a home sector never suppressed; rigs only in held sectors, flanks included; a suppressed Relay still held; no repair by Relays; domination in proportion to the share held; and AI-against-AI figures for S1 and S2.

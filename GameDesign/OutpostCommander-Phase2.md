# Outpost Commander — Phase 2 Design: Territory

Status: **draft** · Owner: Stefan Zwaal · Started 2026-10-03, from the owner's answers of that day

This document says what Phase 2 builds on top of Phase 1. It is a **draft**: the owner's decisions of 2026-10-03 (§3) are settled, and everything else is a proposal for the owner to review, gated in §13 as Phase 1's were. Until it is accepted it binds nothing, and [the Phase 1 design](Archive/OutpostCommander-Phase1.md) and [the MVP design](Archive/OutpostCommander-MVP.md) stay the authority for what is built. Its one effect on Phase 1 is that Phase 1's 5 km map is laid out so that Phase 2 can be played on it (§11).

---

## 1. What Phase 2 is for

The owner's question of 2026-10-03: **today a match is two players building ships and, in the end, one siege of the enemy's base. There are no real skirmishes along the way.** The owner wants the fights of Warzone 2100, where a scout meets a scout in the first five minutes and a small enemy outpost is a target halfway through, and, beyond that, a much bigger world with fights over big areas of space to control, as in PlanetSide.

**Why the game sieges today.** It is not a missing feature; the rules make a siege the right play:

- **Nothing in between is worth fighting for.** The home asteroids stand 260 m from the Command Station, under one Defence Platform's cover; a contested asteroid pays 8 Ore a second against home's 5; and in the MVP home ore never runs out. Phase 1's depletion (Phase 1 §8) starts to change this.
- **A small fight is a bad trade.** Battles between clumps follow Lanchester's square law, so concentration wins; the armed Command Station beats seven raiders and a Defence Platform five. A raid of three ships feeds the defender.
- **Winning a small fight earns nothing back.** A rig destroyed costs its owner 50 Ore.
- **Seeing the enemy means fighting it.** Sight is a ship's weapon range and 50 m (ADR-024): a Small Mass Driver ship sees 170 m.
- **The AI waits for twelve ships**, then attacks, and the player meets the game through the AI.

**What makes it work in PlanetSide is players, not area.** Thousands of people each run one soldier, so a continent is full of fights. Here one commander runs an army, and a bigger map alone gives long travel, losses nobody saw, and still one decisive battle at the end. So Phase 2 makes **territory** the thing that is fought over, a **lattice** that keeps several fights going along a front, and the **tools to command at that scale**. The world grows with them, not before them.

---

## 2. What Phase 2 must show

These play the part of Phase 1's P1–P5. A failed answer is still a result.

| # | Question | How we know |
|---|---|---|
| S1 | Is there contact early? | In a match the owner plays against the AI without rushing, the first shot is fired by minute 5. The match log records it. |
| S2 | Are there skirmishes before the decisive battle? | The match log records each engagement (ships of both sides firing within one sector). Before minute 20 there are at least five, in at least three sectors. |
| S3 | Does territory decide matches? | Over 10 seeded AI-against-AI matches, some end by domination and some by losing the Command Station and every Shipyard (§8); neither never happens. |
| S4 | Does the engine hold at Phase 2's scale? | Ticks meet 5 ms on the Phase 2 map at the ship counts its matches reach, as Phase 1's P4. |
| S5 | Can one commander follow it? | The owner judges the alerts, the zoom and the standing orders (§9) in play. |

---

## 3. Decided by the owner on 2026-10-03

- **Territory is Phase 2, shaped now.** Phase 1 is finished as planned, its tuning, AI and windows included, but its 5 km map is laid out in sectors with a node each, so that it is the first map Phase 2 plays (§11).
- **A node is claimed by building a relay on it, and contested by attacking the relay** (§5). A Constructor builds a Relay on a free node; the enemy must destroy it to take the node. Enemy warships near a relay stop its income and vision while they stay (§5).
- **A match ends by losing the base, or by domination** (§8). Phase 1's condition stays: a player without a Command Station or a finished Shipyard loses. Beside it, holding most of the nodes long enough wins, so territory decides most matches and the siege stays possible.
- **Scouts see further through an optional module** (§10). A design gains a fourth slot, empty by default; its first module is a Sensor Array that lets a ship see far beyond its weapon.

---

## 4. Sectors and nodes

*Proposal (gate J1).*

- **The map is divided into sectors**, each an area of space of about 1.5–2 km across, bounded by asteroid fields and open borders. Each sector has **one node site**: a place where a Relay can stand, among the sector's ore asteroids.
- **Two sectors are adjacent when they share a border.** The map data names the sectors, their node sites and their adjacency, and the map's tests check that adjacency is symmetric, that every asteroid lies in one sector, and that every sector can be reached from both starts.
- **Each player's start is its home sector**, and its Command Station holds that sector's node from the first tick. A Command Station counts as a relay (§5).
- **Ore belongs to sectors.** A Mining Rig may be built only in a sector its player holds. This is what makes a node worth taking: the sector's asteroids, Phase 1's depletion included, are income only for whoever holds it.

---

## 5. Relays: claiming and contesting a node

*The rule is decided (§3); the numbers are proposals (gate J2).*

- **A Relay is a structure a Constructor builds on a free node site** of a sector adjacent to one its player holds (§6). About 200 Ore and 40 s of a Constructor's work; 3,000 hit points, armor 10, unarmed. A Defence Platform beside it is how a player fortifies a node.
- **While its Relay stands, a player holds the sector**: it may build rigs there, it sees the whole sector, and its ships there are repaired, slowly, by the Relay.
- **A Relay is suppressed** while an enemy warship is within 400 m of it and none of its owner's is. A suppressed Relay earns its sector nothing and sees only as a structure does, so raiders hurt a sector without destroying anything, and a defender has to come and fight. Suppression lasts as long as the enemy stays.
- **A destroyed Relay frees its node**, and the sector's rigs stop earning until someone holds it again. Its wreck is worth part of its cost to whoever collects it with a Constructor (gate J7).

---

## 6. The lattice

*Proposal (gate J1).*

- **A Relay can only be built on a node adjacent to one its player holds.** Territory therefore grows outward from the home sector and is taken back along a front, not leapfrogged.
- **A Relay can be attacked anywhere.** A fast group can still raid deep, suppress a sector or destroy its Relay, which is a skirmish worth having, but it cannot claim the node unless the sector is next to its own.
- **Cutting a sector off** from its owner's home sector through the lattice stops its Relay repairing and its rigs earning half, until the link is restored. This is what makes a pincer pay, and what a front is for.

---

## 7. The world

*Proposal (gate J3).*

- **Phase 2's map is about 10 × 10 km**, four times Phase 1's, with about 25 sectors. It is point-symmetric, as every map has been, and its sectors are laid out so that the front between the two homes is three to five sectors wide.
- **Travel takes minutes.** A Small+Ion ship crosses 14 km in about three minutes, a Large+Fusion in about twelve. That is the reason for forward Shipyards (a Shipyard may be built in any sector its player holds) and for relay jumps (§9).
- **Pathing has to plan by sector.** One visibility graph over the whole map already takes about 4 ms to build on the 2.3 km map (ADR-032). A path is planned first across sectors, then within each sector on its own graph, which is also built and dropped sector by sector.

---

## 8. Winning

*The rule is decided (§3); the numbers are proposals (gate J4).*

- **Losing the base** is Phase 1's condition, unchanged: a player with neither a Command Station nor a finished Shipyard loses.
- **Domination.** Each player starts with 1,000 tickets. Every 10 seconds, the player who holds fewer nodes loses twice the difference in tickets: a lead of five nodes costs the other side one ticket a second, about 17 minutes from full to nothing. A player whose tickets reach zero loses.
- Suppressed Relays count for their owner, so a domination win has to be taken, not raided.

---

## 9. Commanding at this scale

*Proposals (gate J5).* Without these, a big map is long travel and losses nobody saw.

- **Alerts.** A short message and a mark on the minimap when a Relay is suppressed or attacked, a rig is lost, or an enemy group enters a held sector; a key jumps the camera to the latest.
- **A strategic view.** Zooming out beyond the RTS camera's limit (Phase 1's gate H8) to a map of sectors: who holds what, what is suppressed, where groups are.
- **Standing orders.** "Hold this sector": a group returns to the sector's Relay after it chases, and answers any enemy in the sector. "Patrol": between two points, attacking what it meets.
- **Relay jumps.** A group at one of its player's Relays that is not suppressed can jump to another it holds, after a 10 s charge, with a cooldown. A reinforcement then takes seconds rather than minutes, and holding a connected territory is worth more than holding scattered nodes.

---

## 10. Modules

*The slot is decided (§3); the numbers are proposals (gate J6).*

- **A design has an optional fourth slot, the module**, empty by default, beside hull, drive and weapon. The designer gains a fourth row, a saved design gains a field, and a design's abbreviation gains its module's initials when it has one.
- **The first module is the Sensor Array**: the ship sees 700 m, whatever its weapon; costs 40 Ore; and slows its ship by 10%. On a Small+Ion hull it is the minute-3 scout; on a heavy it is a spotter for the Rail Cannon.
- **Later modules use the same slot**: a repair module, extra plating, a jammer that hides a group from enemy sensors. Each is its own decision.
- **The balance check leaves out modules a battle between clumps cannot see**, as it leaves out the Pulse Drive's speed (Phase 1 gate H4): a design with a Sensor Array is not required to be worth building.

---

## 11. What Phase 1 does now

Phase 1's 5 km map (Phase 1 §8, task 11.2) is laid out in sectors: about nine, each with a node site among its asteroids, and the map data names the sectors, their node sites and their adjacency, as §4 has them. Phase 1 reads none of it; its rules, rings, yields and reserves are unchanged. It makes the 5 km map the first one Phase 2 plays, and lets Phase 2's lattice be tried on a map the owner has already played.

**Carried over from Phase 1** (owner, 2026-10-03), for Phase 2's plan: task 7.3, a group given an Attack order on one enemy keeps its lanes round an obstacle, as a group given a Move or an attack-move order does since task 9.7 (ADR-047).

---

## 12. The AI

The node graph is what an AI can reason about: the threat in each sector, the weakest node adjacent to its own, where to reinforce. Phase 2's AI:

- sends a scout with a Sensor Array in the first two minutes;
- claims nodes adjacent to its territory, and fortifies the ones on the front with a Defence Platform;
- raids a sector it sees weakly held with a few fast ships, and pulls back when it loses;
- masses for a main attack only when it has a lead in nodes or the enemy's front has thinned.

---

## 13. Open questions

Each is a gate for Phase 2's plan.

- **J1 — Sectors, nodes and the lattice** (§4, §6): sectors of 1.5–2 km, one node each, rigs only in held sectors, Relays only adjacent to held nodes, and cut-off sectors earning half.
- **J2 — The Relay's numbers and suppression** (§5): its cost, hit points and armor, and the 400 m suppression radius.
- **J3 — The world** (§7): a 10 km map of about 25 sectors, forward Shipyards, and pathing by sector.
- **J4 — Domination's numbers** (§8): 1,000 tickets, and twice the node difference every 10 s.
- **J5 — Commanding at scale** (§9): which of alerts, the strategic view, standing orders and relay jumps are in Phase 2.
- **J6 — Modules** (§10): the Sensor Array's sight, cost and penalty, and which modules follow it.
- **J7 — Salvage** (§5): whether a destroyed Relay, rig or platform leaves a wreck worth part of its cost, and how much, so that a raid pays. Too much makes the first raid decide the match.

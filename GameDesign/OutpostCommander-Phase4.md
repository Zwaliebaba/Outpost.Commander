# Outpost Commander — Phase 4 Design: Fewer Ships, a Wider Reach

Status: **accepted** · Owner: Stefan Zwaal · Started 2026-10-06, from the owner's answers of that day · Accepted on 2026-10-06, with every gate decided as proposed (§16) · The order of the work is [the Phase 4 plan](ImplementationPlan-Phase4.md)

This document says what Phase 4 builds on top of Phase 3, and amends [the Phase 3 design](Archive/OutpostCommander-Phase3.md), [the Phase 2 design](Archive/OutpostCommander-Phase2.md), [the Phase 1 design](Archive/OutpostCommander-Phase1.md) and [the MVP design](Archive/OutpostCommander-MVP.md) where they differ. What the owner decided on 2026-10-06 is in §3, and the owner decided every gate of §16 as proposed the same day; the numbers are starting values, as Phase 3's were.

---

## 1. What Phase 4 is for

The owner's question of 2026-10-06: **a match is a swarm from the first minutes. A ship should take more effort to build, so that the opening is spent finding out what is around: mining, salvage and small fights. The universe has to be much bigger for that.**

**Why the game swarms today.** How many ships a player has is decided by income and cost, not by build time.

- **Ships are cheap against income.** A Swarm (Small+Ion+Mass Driver) costs 87 Ore and builds in 10 s. Three home rigs pay 15 Ore/s, and three contested ones bring it to about 40 Ore/s, which buys about 27 Small ships a minute. The 1,000 starting Ore buys 11 at once.
- **Build time is no brake.** One Shipyard spends about 8.7 Ore/s on Swarms, and a Shipyard costs 300 Ore. The AI plans another for each 10 Ore/s of income (ADR-020 decision 6), and a player can do the same. Doubling build time doubles the Shipyards, and the number of ships stays the same.
- **Nothing bounds the fleet.** There is no unit cap (Phase 1, accepted risk 2). The AI built 443 warships in the MVP's first 25-minute match.

**Why a bigger map is not enough on its own.** Phase 2 §1 said this already: a bigger map gives long travel, losses nobody saw, and still one decisive battle at the end. Exploring is worth doing only if there is something to find. Today the map holds only ore, it is point-symmetric, and it is the same in every match, so it is known after the first. Warzone 2100's opening works because of its scavengers: weak neutrals to fight before the enemy is met.

**So Phase 4 does five things together.** Ships cost more and take longer (§4), and a fleet cap bounds them (§5). The map becomes a 10 km system (§6), filled from the match's seed (§7) with pirate outposts (§8) and derelicts to salvage (§9). A ship that costs this much is worth bringing home, so a Repair Bay and a retreat order come with it (§10).

**What could go wrong.**

- **An empty opening.** At 10 km a Large+Fusion ship takes about nine and a half minutes from one start to the other (§6). If the pirates and derelicts are too thin, the opening is waiting, not exploring. U1 measures it.
- **Turtling.** Dear ships and a cap make static defense relatively cheap. A Defence Platform is therefore rescaled with the ships (§4), and U4 checks that fights still happen between the bases.
- **An early lead that decides the match.** Salvage and cleared pirates give Ore to whoever gets there first. The placement is point-symmetric (§7), so both sides have the same chances, but a faster opening compounds. U5 records how matches end.
- **The engine at four times the area.** Path graphs, the fog texture and the minimap all grow with the map (§11). Fewer ships pay for part of it. U7 measures it.

**The question Phase 4 answers: with fewer, dearer ships on a map worth exploring, is the opening spent finding things out, and does a ship become worth keeping?**

---

## 2. What Phase 4 must show

These play the part of Phase 3's T1–T5. A failed answer is still a result. Each question that depends on the owner's play also has a figure from AI-against-AI matches over seeds 1–40, as T1 was measured. Phase 2's S1, contact by minute 5, and S4, a match of 45–60 minutes, are retired for Phase 4 (§3).

| # | Question | How we know |
|---|---|---|
| U1 | Is the opening spent exploring? | The match log records the first shot between the two players, each derelict salvaged and each pirate outpost fought. In the median match, each side salvages at least 2 derelicts and fights at least 1 pirate outpost before the players first fire on each other. That first shot comes between minute 8 and minute 20 in at least 30 of the 40. |
| U2 | Has the swarm gone? | In the median match, neither player ever has more than 40 warships, and at minute 20 each has fewer than 20. |
| U3 | Is a ship worth keeping? | The match log records each ship that retreats and each one repaired. In the median match, each side sends at least 10 warships back to be repaired, and at least half of them fight again. |
| U4 | Are there still skirmishes? | Before minute 40, the median match has at least five engagements between the players, in at least three sectors, as Phase 2's S2 counts them. |
| U5 | Does every match end? | All 40 end, by domination or by production, and both endings happen. The length is recorded, with no target (§3). |
| U6 | Does design still matter? | The Q2 check of MVP §3 passes at budgets three times today's (§4). |
| U7 | Does the engine hold at 10 km? | On the development machine, in Release: the 99th percentile tick at most 5 ms in an AI-against-AI match. 99% of frames take at most 16.7 ms at 1920×1080, with the camera zoomed out over the largest fight. |
| U8 | Does it read and play? | The owner judges, in play, whether the opening feels like exploring, and whether a 10 km map can be followed with the camera, the minimap and the alerts. |

**Where they stand on 2026-10-07, after milestone 27, on today's 5 km map.** An interim answer to U2 and U5, before the map changes (plan tasks 27.5 and 27.6). Two Normal AIs over seeds 1–40, run in the Linux container on the server and AI built with clang 18 against a stand-in for the Windows headers, and the same harness on `main` before milestone 27 for comparison. Floats replay only on the same build (ADR-009), so MSVC's build may play the same seeds differently. The middle column is milestone 27 at Phase 3's income; the last is with the income lowered by about a third (owner, 2026-10-07; §4), which is what is built.

| Over seeds 1–40 | Before milestone 27 | Phase 3's income | Income lowered |
|---|---|---|---|
| Most warships a side has at once, median of the 80 sides (largest) | 193 (565) | 24.5 (50) | 21 (38) |
| Sides over 40 warships at once | — | 14 of 80 | 0 of 80 |
| Warships a side has at minute 20, median; sides with 20 or more | 123.5; — | 15.5; 36 of 80 | 15; 8 of 80 |
| Warships a side builds in a match, median | 400.5 | 87 | 66 |
| First shot, median | 1:58 | 2:47 | 3:14 |
| Match length, median (shortest–longest) | 41:27 (24:24–62:48) | 53:50 (44:48–1:29:18) | 1:01:05 (46:10–1:31:10) |
| Endings | 16 domination, 24 production | 40 domination | 40 domination |
| Ore a side holds at minute 20, median; sides over 2,000 | 136; — | 8,823; 50 of 80 | 733; 19 of 80 |

- **U2, the swarm: met.** With the lower income no side goes over 40 warships, the median side peaks at 21, and at minute 20 the median side has 15, with 8 of the 80 at 20 or more.
- **U5, every match ends: yes, but by one ending only.** All 40 end, and all by domination. No AI took the other's base: a fleet capped at 50 points does not break a base of Defence Platforms and a level 5 Command Station's three guns, where the siege ended 24 of the 40 before. The owner decided on 2026-10-07 to measure it again on the 10 km map (§3) rather than tune it on this one.
- **Ore no longer piles up in the median match.** At Phase 3's income the median side held 8,823 Ore at minute 20 that its capped fleet could not use; with the income lowered by about a third it holds 733. A quarter of the sides still hold over 2,000, mostly the side ahead.
- **Halving the income instead** was measured too: the median side peaks at 21 warships and builds 47.5, the first shot comes at 4:03, three of the 40 end by production, and the median side holds 421 Ore at minute 20. It is the stronger brake on the opening, and it is not what is built.

**Where they stand on 2026-10-07, after milestone 28, on the 10 km map** (plan task 28.4). The same harness and seeds, with each match played for up to four hours. The map is empty of pirates and derelicts until milestones 29 to 31, and the AI does not yet play Phase 4 (milestone 33).

| Over seeds 1–40 | 5 km, after milestone 27 | 10 km, after milestone 28 |
|---|---|---|
| Most warships a side has at once, median of the 80 sides (largest) | 21 (38) | 25.5 (50) |
| Sides over 40 warships at once | 0 of 80 | 14 of 80 |
| Warships a side has at minute 20, median; sides with 20 or more | 15; 8 of 80 | 13; 0 of 80 |
| Warships a side builds in a match, median | 66 | 129.5 |
| First shot, median | 3:14 | 4:42 |
| Match length, median (shortest–longest) | 1:01:05 (46:10–1:31:10) | 2:08:55 (1:40:30–3:04:20) |
| Endings | 40 domination | 40 domination |
| Ore a side holds at minute 20, median; sides over 2,000 | 733; 19 of 80 | 2,625; 50 of 80 |
| Ore a side holds at minute 30, median | — | 18,236 |

- **U2, the swarm: met in the median**, as on the 5 km map: the median side peaks at 25.5 warships and has 13 at minute 20. But 14 of the 80 sides go over 40 at some point, all late in a long match, filling the cap of 50 points with Small hulls.
- **U5, every match ends: yes, by domination only, and in about two hours.** No AI takes the other's base on the bigger map either. Domination is the slow part: its rule carries over unchanged (§6), so a lead of one node of 25 takes 139 minutes where one of nine took 50.
- **Ore piles up again.** Every side's Command Station is at level 5, its cap 50 points, by minute 20, and every side fills that cap at some point in its match; with more sectors to mine, the median side holds 18,236 Ore at minute 30 that it has nothing to spend on.
- **The first shot comes at 4:42 in every match,** the scouts meeting at the enemy's flank: there is nothing yet to find on the way (U1 waits for milestones 29 to 31).
- **The engine, in the container:** the median match's 99th percentile tick is 2.5 ms, and the worst match's 4.2 ms, under U7's 5 ms; the slowest single tick is 20 ms in the median match and 39 ms at worst. U7 itself is the owner's run in Release.

**After milestone 29** (plan task 29.1): a lead of one node takes 50 minutes on any map (29.0), and each sector's asteroids are placed from the seed, so the 40 seeds play 40 different maps. The same harness and seeds.

| Over seeds 1–40 | 10 km, after milestone 28 | 10 km, after milestone 29 |
|---|---|---|
| Most warships a side has at once, median of the 80 sides (largest) | 25.5 (50) | 21 (46) |
| Sides over 40 warships at once | 14 of 80 | 4 of 80 |
| Warships a side has at minute 20, median; sides with 20 or more | 13; 0 of 80 | 14; 12 of 80 |
| Warships a side builds in a match, median | 129.5 | 82 |
| First shot, median (earliest–latest) | 4:42 (4:42–4:43) | 4:42 (4:35–5:21) |
| Match length, median (shortest–longest) | 2:08:55 (1:40:30–3:04:20) | 1:37:06 (54:09–2:30:39) |
| Endings; won by player 1 | 40 domination; 22 | 40 domination; 20 |
| Ore a side holds at minute 20, median; sides over 2,000 | 2,625; 50 of 80 | 1,777; 32 of 80 |
| Ore a side holds at minute 30, median | 18,236 | 17,135 |

- **U5: matches are shorter, and still end by domination only.** A third shorter at the median, and the longest under two and a half hours. Half the matches last 1:23 to 1:50: the sides stay within a node of each other for most of a match, so the drain runs slowly until one pulls ahead. No AI takes the other's base.
- **Placement from the seed is fair.** Each player wins 20 of the 40, and the first shot varies by up to 46 seconds where every map was the same before.
- **Ore still piles up** behind the cap, as the owner expected, for Phase 4's later sinks to take (milestones 30 to 32) and milestone 34 to measure.
- **The engine:** the median match's 99th percentile tick is 2.1 ms in the container, the worst match's 4.2 ms. One match's slowest tick took 72 ms, the most yet; the median match's slowest is 23 ms.

---

## 3. Decided by the owner on 2026-10-06

- **Ships become harder to build, and the opening is for exploring, mining and small fights.** That is the owner's direction for Phase 4 (§1).
- **The map is 10 km a side, and it is the horizon's first star system.** This keeps the decision of 2026-10-04 that the world grows by systems and not into one large map ([Horizon](OutpostCommander-Horizon.md) O8, Phase 3 §3). A system is 10 km a side, and what Phase 4 builds on it is what the horizon's systems are built from.
- **The swarm is bounded by cost and by a fleet cap.** Ships cost more against income (§4), and a cap counts the warships a player has (§5).
- **The opening holds pirate outposts and derelicts, placed from the match's seed.** Pirates are static guardians that guard their place and never raid (§8). A derelict pays Ore and research progress when salvaged (§9). Placement changes with the seed, and is the same for both sides (§7).
- **A Repair Bay and a retreat order are in Phase 4. Veterancy is not** (§10).
- **No match length target.** The game becomes a server game (the horizon), so Phase 4's length is measured and recorded, not tuned to fit one sitting. A match that does not finish in one sitting is accepted while there is no save (Phase 1, accepted risk 3).

**Decided by the owner on 2026-10-07**, from milestone 27's measurement (§2):

- **The rigs' income is lowered by about a third** (§4), so that Ore runs short before the fleet cap binds, rather than piling up behind it.
- **That no match ends by taking a base is measured again on the 10 km map** (milestone 28), before anything is tuned for it.
- **Both land with milestone 27.**

---

## 4. Ships cost more

*Decided (gate L1): the factor is a starting value.*

- **Every hull, drive, weapon and module costs three times as much, and every hull takes three times as long to build.** A Swarm becomes 261 Ore and 30 s. A Large+Fusion+Rail Cannon goes from 530 Ore and 40 s to 1,590 Ore and 120 s.
- **Cost and build time move by the same factor**, so a Shipyard spends Ore at today's rate: about 8.7 Ore/s on Swarms. The AI's rule of one Shipyard per 10 Ore/s of income still fits (ADR-020 decision 6).
- **A Defence Platform also costs three times as much**, 450 Ore. A platform is a ship that cannot move. If it stayed at 150 Ore while ships tripled, it would trade at a third of today's price against a fleet, and both sides would turtle. The Command Station's guns are part of the station and do not change.
- **The rigs earn about a third less** (owner, 2026-10-07): 3.5, 4, 5.5 and 6.5 Ore a second at home, near, contested and rich, where Phase 3 had 5, 6, 8 and 10. With Phase 3's income the fleet cap bound from the mid-game on and the median side held 8,823 Ore at minute 20 that it could not use (§2).
- **What does not change:** the Constructor (60 Ore, 15 s), Mining Rigs, Relays, the Shipyard and the Research Lab, structure levels, research and starting Ore. So research and building get cheaper relative to ships, though the lower income makes everything slower to pay for. The opening's Ore goes to rigs, Relays, salvage trips and the Lab, and its first few warships go against pirates.
- **The Q2 check is unchanged by a single factor.** It fights designs bought with equal Ore, so if every component's cost moves by the same factor, its budgets move with it, from 2,000–12,000 Ore to 6,000–36,000 Ore, and every win rate stays the same. U6 runs it again anyway. A later retune of one component's cost is a change like any other, and is checked like any other.

---

## 5. The fleet cap

*Decided (gate L2): the numbers are starting values.*

- **Each warship takes command points by its hull:** Small 1, Medium 2, Large 4. A Constructor takes none.
- **The Command Station's level sets the cap**, as it sets the nodes a player may hold (Phase 3 §7):

  | Level | Command points | Nodes held, home included (§6) |
  |---|---|---|
  | 1 | 12 | 4 |
  | 2 | 20 | 7 |
  | 3 | 30 | 10 |
  | 4 | 40 | 13 |
  | 5 | 50 | 16 |

  At level 5 that is, for example, 12 Large ships and 2 Small, or 25 Medium. Two players at the top cap have at most 100 warships between them, half of what Q4 measured.
- **A ship's points are taken when its job starts and is paid**, as Ore is (ADR-016 decision 6). A job that would go over the cap waits at the front of its queue, as a job waiting for Ore does, and the jobs behind it wait too. The designer and the production window show why.
- **The cap never removes a ship.** A player who loses the Command Station has level 1's cap, as it has level 1's nodes. It keeps every ship it has, and builds none while it is at or over 12 points.
- **Why the Command Station, and not the nodes held.** Horizon §6.2 suggests tying the cap to territory. That makes a lead in nodes a lead in fleet as well, on top of domination, so a match snowballs faster, the risk Horizon §7 names. A cap bought with levels is a choice of what to spend Ore on, and both sides can make it. Gate L2 is where the owner chooses.

---

## 6. The map: a 10 km system

*Decided (gates L3–L6).*

- **10,000 m a side, 25 sectors** on a 5×5 grid of 2 km, each with one node at its center (ADR-036's format, unchanged). The homes are in opposite corners, with their starts at (−4,000, −4,000) and (4,000, 4,000). Sectors are bounded by asteroid fields with passages, as now. The map is point-symmetric about its center.
- **Travel time.** Start to start is about 11.3 km. Today's 5 km map has 4.95 km between the starts.

  | Ship | Speed | Start to start, 10 km | Today, 5 km |
  |---|---|---|---|
  | Small+Pulse | 96 m/s | 1:58 | 0:52 |
  | Small+Ion | 78 m/s | 2:25 | 1:03 |
  | Constructor | 45 m/s | 4:11 | 1:50 |
  | Medium+Ion | 52 m/s | 3:38 | 1:35 |
  | Large+Fusion | 20 m/s | 9:26 | 4:08 |

- **Node caps for 25 nodes** (gate L4): 4, 7, 10, 13 and 16 at Command Station levels 1–5 (§5's table). The reasoning is Phase 3 §7's, scaled up. From level 4, two caps add up to more than the map's 25 nodes, so both sides cannot sit at their cap at once and domination can decide. Domination's drain no longer divides by the map's nodes: a lead of one node takes 50 minutes on any map, as it did on the 5 km map's nine (owner, 2026-10-07, after plan task 28.4 found it took 139 minutes on 25; ADR-057).
- **Production follows territory** (gate L5). Today only Mining Rigs and Relays need a held sector (ADR-056). A Shipyard can stand anywhere a Constructor reaches, which on a 10 km map makes a Shipyard hidden next to the enemy's base the cheapest attack. In Phase 4, a Shipyard and a Repair Bay may be built only in a sector the player holds. Pushing the front forward is then how production moves forward. Relay jumps stay with the horizon, as travel between systems (Horizon O8).
- **The camera** (gate L6). The widest view is 1,600 m today (ADR-012, gate H8), a sixth of the map. The proposal raises it to 3,000 m, and U7's frame figure is measured at that view. The strategic view stays out (Phase 2 gate J5) unless U8 asks for it.

---

## 7. Placement from the seed

*Decided (gate L10).*

- **Fixed in `Map.json`:** the size, the sector grid, the asteroid fields that bound the sectors, and their passages. The starts and the three home asteroids of each home are fixed too.
- **Placed from the seed:** every other ore asteroid, every derelict and every pirate outpost. `Map.json` gives the rules for each kind of sector: how many asteroids and of which yield, how many derelicts, and whether pirates hold it. It no longer gives the positions.
- **Point-symmetric.** One half of the map is drawn from the seed and the other half mirrors it through the center, so both seats get the same map. The center sector is its own mirror, and its content is placed symmetrically within it.
- **Deterministic.** Placement uses `Neuron::Random` seeded from the match's seed, inside `Simulation`'s setup, so a match still replays from its seed and command log on the same build (ADR-009).
- **The proposed rule by distance from the homes:** pirate camps one sector beyond the flanks, pirate strongholds on the rich sectors and in the center, and derelicts in roughly every other sector, richer the further out they lie.
- **As built** (plan task 29.1, ADR-072): each sector of `Map.json` names a kind, and each kind the asteroids it gets, today's count and yields in each sector (owner, 2026-10-07). The seed draws their places inside the sector, clear of its borders, its node and each other.

---

## 8. Pirates: static guardians

*Decided (gate L7): the numbers are starting values.*

- **Pirates are a neutral owner in the simulation, not a player.** They have no Ore, production or research, and they send no commands. Their behavior is a rule that the simulation runs, as a Command Station's gun is, so it is deterministic and costs nothing in transport. The AI as a client (ADR-002) is for players. An ADR records the neutral owner when it is built.
- **An outpost sits on a sector's node and holds it for no one.** The sector cannot be claimed until the outpost's structures are destroyed. Its asteroids earn nothing for anyone until then. A node held by pirates counts for no one in domination (Phase 2 §8).
- **Two sizes:**
  - **A camp:** one Defence Platform and 3 Small Mass Driver ships. A first fleet of 4–6 Small ships clears it with losses.
  - **A stronghold:** two Defence Platforms and 4 Medium ships, one of them with a Lance. It guards a rich sector or the center.
- **Guarding.** An outpost's ships attack any player's ship or structure within 600 m of its node, chase no further than 900 m, and then go back to the node. They never raid, and they never leave the sector.
- **No respawn.** A cleared outpost stays cleared. Pirates are the opening's content and the mid-game's obstacles, not a third side for the whole match.
- **Cleared, an outpost leaves a derelict** where its largest structure stood (§9).
- **Drawn** with one of the two existing mesh sets in a third color of their own, until there is art for them. Models.json already reuses a mesh with a tint for the Relay. Fog of war applies to pirates as it does to an enemy (ADR-024).

---

## 9. Derelicts: salvage

*Decided (gate L8): the numbers are starting values.*

- **A derelict is a wreck of the old fleet** (MVP §1), drawn with a hull mesh in grey, without its owner's color. It is not an obstacle that blocks a path, any more than a rig is.
- **A Constructor salvages it** with a new work order, as it builds a site: 30 s of one Constructor's work, with more Constructors helping at `extraConstructorBuildShare` (ADR-016 decision 5). When the work is done the derelict is gone, and its owner is paid. Warships cannot salvage, so the opening's trip is a Constructor's, escorted or not.
- **It pays Ore:** 300 near the homes, up to 900 in the far sectors. An outpost's derelict pays 600 for a camp and 1,200 for a stronghold.
- **About one in three also pays research.** It names a research topic, and once salvaged it recovers half of that topic's research time for the salvaging player: half is taken off a topic under way, and a topic not yet started will take half its time when it is. Its Ore cost is unchanged. A topic of a tier the player's Lab has not opened is recovered all the same, and waits for the tier. This is "research is recovery" (MVP §2) made literal, and a first step towards Horizon §6.1.
- **What a derelict holds is seen** once it is within a player's sight: its Ore, and the topic it names, if any. Under fog it is remembered as a structure is.

---

## 10. Repair Bay and retreat

*Decided (gate L9): the numbers are starting values.*

- **The Repair Bay is a new structure:** 300 Ore and 40 s of a Constructor's work, 2,000 hit points, armor 5, built only in a held sector (§6). It repairs up to 4 friendly warships within 150 m of it, each at 3% of its maximum hit points a second, at no cost. Constructors still repair as today (ADR-016 decision 5). It has no levels, and it is drawn with an existing mesh and a tint until it has art of its own, as the Relay is.
- **A retreat threshold per ship:** never, at 50% or at 25% of hit points. It is set in the designer for a design, carried by every ship built to it, and changed on a selection. Below its threshold a ship drops its order and goes to the nearest Repair Bay, else the Command Station, else a Shipyard, and is repaired there. Once whole it goes back to its standing order (ADR-059) if it had one, and otherwise waits there.
- **The simulation runs it, not the client**, so it works while nobody is looking. That is what an absent player's fleet needs on the horizon (Horizon §5).
- **The default is 25%.** With dear ships, a fight to the last ship should be a choice the player makes, not something that happens by default. The AI's group fall-back (ADR-041) is unchanged, and the AI's ships use the per-ship threshold too.

---

## 11. The engine at 10 km

- **Path graphs.** The visibility graph covers the whole map (ADR-010), and line tests use ADR-054's grid. At four times the area, with roughly four times the obstacles, a full graph build grows faster than the area does. U7 measures it. A destroyed structure, which used to make every graph be built whole, is now taken out of each in place (plan task 28.0, ADR-054, owner, 2026-10-07): in the container, on the 10 km map, that took the 99th percentile tick from about 20 ms to 2 ms. Pathing by sector (Phase 2 §7) is built only if U7 misses, and that is a decision recorded in an ADR.
- **Fog.** The 20 m grid becomes 500×500 cells, and the fog texture grows from 256×256 to 512×512 (ADR-052). It follows the map's size instead of being fixed.
- **The minimap** shows four times the area at the same size, so a mark covers less, and its marks are checked at U8.
- **Snapshots** carry more asteroids, derelicts and pirates, and far fewer ships. Their size is recorded.
- **The client follows the map's size,** as it did when the map went from 2.3 km to 5 km (ADR-036 decision 5).

---

## 12. The AI

The AI plays by every rule above, and plays them as a player would:

- it sends its scout out first, and its first Constructors on salvage trips to the derelicts nearest its home, before its contested rigs;
- it clears a pirate camp once it has a group that outguns the camp by an Ore margin set in `Opponent.json`, and a stronghold later, when it wants that sector;
- it spends at its cap on levels, research and Repair Bays rather than waiting, and it upgrades its Command Station when its cap stops its production;
- its ships use the retreat threshold, and it builds a Repair Bay behind its front;
- it places Shipyards only in sectors it holds (§6);
- its counters (ADR-020 decision 8) are unchanged.

The three difficulty files (ADR-065) carry the new numbers. Normal is tuned against U1–U5.

---

## 13. The client

- **The HUD** shows command points against the cap at the end of the status panel's Shipyards line, "fleet 18 / 30", and counts the Shipyards waiting for the cap apart from those waiting for Ore. That line is there whatever the map, where the territory panel is only on a map with sectors.
- **The production window** says when a Shipyard's front job waits on the cap, as it says when one waits for Ore. Queue stays live, since the job only waits (§5). The designer sets a design's retreat threshold (§10).
- **A selection's panel** shows and changes the retreat threshold.
- **Derelicts and pirate outposts** are marked on the minimap once seen. A derelict's panel says what it holds. The Constructor's right-click on a derelict salvages it.
- **Alerts** (ADR-059) add a ship retreating and a pirate outpost cleared.

---

## 14. What changes elsewhere

- **Phase 1:** accepted risk 2, that there is no unit cap, ends (§5).
- **Phase 2:** S1 and S4 are retired for Phase 4 (§2). The map of §4 is replaced by §6's, and §8's drain takes its tickets for each node behind without dividing by the map's nodes (§6).
- **Phase 3 §7:** the node caps are rescaled for 25 nodes (§6), as its own §7 foresaw.
- **MVP §3:** the Q2 check's budgets scale with §4's factor.
- **The horizon:** O8 records that a system is 10 km a side and that Phase 4's map is the first one (§3). O2's bound is proposed here as a cap bought with Command Station levels (§5). Derelicts that pay research are a first step towards §6.1.
- **ADRs** edited in place: ADR-036 for the new map and the placement rules, ADR-056 for the caps and the held-sector rule for Shipyards, ADR-016 for the costs and the new structure, ADR-012 for the camera, ADR-052 and ADR-024 for the fog's size, and ADR-020, ADR-041 and ADR-065 for the AI. New ADRs: the neutral owner, salvage, the Repair Bay and retreat, the fleet cap, and seeded placement.

---

## 15. Out of scope for Phase 4

Everything Phase 3 kept out stays out. Also out: veterancy (§3); relay jumps and travel between systems (the horizon); the strategic view, unless U8 asks for it; pathing by sector, unless U7 misses; pirates that raid or respawn; save and load; new hulls and weapons, including artillery and spotting; AI personalities; and art for the pirates, the derelicts and the Repair Bay, which reuse existing meshes.

---

## 16. Gates

Each was an owner decision. **The owner decided all of them on 2026-10-06, as proposed**, with L11 waived: Phase 4 starts without waiting for the Interface plan's owner runs.

- **L1 — The rescale** (§4): ×3 on every component's cost and on every hull's build time, ×3 on the Defence Platform's cost, nothing else changed.
- **L2 — The fleet cap** (§5): 1, 2 and 4 points for Small, Medium and Large hulls, Constructors free; 12, 20, 30, 40 and 50 points at Command Station levels 1–5; tied to the Command Station's level rather than to the nodes held.
- **L3 — The map** (§6): 10 km, 25 sectors of 2 km, homes in opposite corners, point-symmetric.
- **L4 — The node caps** (§6): 4, 7, 10, 13 and 16 nodes.
- **L5 — Production follows territory** (§6): Shipyards and Repair Bays only in held sectors.
- **L6 — The camera** (§6): the widest view raised from 1,600 m to 3,000 m.
- **L7 — Pirates** (§8): camps and strongholds as proposed, guarding 600 m and chasing to 900 m, never respawning, holding their node for no one.
- **L8 — Derelicts** (§9): Constructors only, 30 s of work, 300–900 Ore (600 and 1,200 from an outpost), one in three recovering half of a named topic's time.
- **L9 — Repair and retreat** (§10): the Repair Bay's numbers, the thresholds never / 50% / 25%, set per design and per ship, 25% by default.
- **L10 — Placement** (§7): what is fixed and what the seed places, point-symmetric, with the rule by distance from the homes.
- **L11 — When Phase 4 is accepted.** Proposed for after the owner has run the Interface plan's open milestones 14–17. **Waived on 2026-10-06:** Phase 4 is accepted now, and those runs stay open.

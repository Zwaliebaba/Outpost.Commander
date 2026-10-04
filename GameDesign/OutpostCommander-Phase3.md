# Outpost Commander — Phase 3 Design: Structures That Grow

Status: **accepted** · Owner: Stefan Zwaal · Started 2026-10-04, from the owner's answers of that day · Every gate decided on 2026-10-04 (§12) · Revised on 2026-10-04 for [the horizon](OutpostCommander-Horizon.md) · Accepted on 2026-10-04, after the owner's run of Phase 2 (gate K7) · The order of the work is [the Phase 3 plan](ImplementationPlan-Phase3.md)

This document says what Phase 3 builds on top of Phase 2, and amends [the Phase 2 design](Archive/OutpostCommander-Phase2.md), [the Phase 1 design](Archive/OutpostCommander-Phase1.md) and [the MVP design](Archive/OutpostCommander-MVP.md) where they differ. The owner decided its gates on 2026-10-04 (§3, §12); the numbers are starting values, as Phase 2's were. The owner accepted it the same day, after running Phase 2 (gate K7).

---

## 1. What Phase 3 is for

On 2026-10-03 the owner gave the Command Station, the Shipyard and the Research Lab five levels each, in both sets, "to be used later as the structures grow" (ADR-045). The game draws level 1. The owner's direction of 2026-10-04: **a Shipyard is upgraded before it builds bigger ships, a Command Station is upgraded to control more sectors, and a Research Lab is upgraded before it researches further.**

**Why it serves the game.** Phase 2 gave the map something to fight over between the bases: territory. Phase 3 gives the base the same thing. Today a Shipyard is 300 Ore, wherever it stands and however long it has stood, so losing one costs little more than the time to rebuild it. Once a Shipyard at level 3 is the only place a Large hull comes from, it is a target worth a raid, and defending it is worth a fleet. Upgrades also add a choice to every minute of a match: Ore spent on a level is Ore not spent on ships or Relays.

**What could go wrong.** A hard cap on sectors (§7) is a second brake on territory, beside the lattice and the Relay's cost, and domination only decides a match while the two sides hold different numbers of nodes. Gating hulls and tiers behind Constructor work could also slow a match beyond 45–60 minutes, or push tier 3 further out of reach: in Phase 1, only 2 of 80 AI-against-AI matches reached it (Phase 1 P2). §2 measures each of these.

**The question Phase 3 answers: does a base that grows add decisions and targets, without undoing what Phase 2 showed?**

---

## 2. What Phase 3 must show

These play the part of Phase 2's S1–S5. A failed answer is still a result. Each question that depends on the owner's play also has a figure from AI-against-AI matches, over seeds 1–40, as S4 was measured.

| # | Question | How we know |
|---|---|---|
| T1 | Does Phase 2 still hold? | S1–S4 are met again over seeds 1–40, by the same tests as Phase 2 §2. |
| T2 | Does the cap stall territory? | The match log records the time during which both players are at their cap and hold equal numbers of nodes. Over the 40 matches, the median is under 5 minutes a match. |
| T3 | Are the levels reached? | In at least half of the 40 matches, each side reaches level 3 of the Command Station, of a Shipyard and of the Research Lab. Tier 3 is opened in at least half, against Phase 1's 2 in 80. |
| T4 | Are upgraded structures fought over? | The match log records each structure above level 1 that is attacked. In the median match, at least one is attacked before the match ends. |
| T5 | Does it read? | The owner judges, in play, whether a structure's level is visible at a glance and whether what the next level gives is clear before buying it. |

**Where they stand on 2026-10-04, after plan task 25.2.** The AI-against-AI figures are over seeds 1–40, as the Linux container measured them with the match log of ADR-038 and the AI of ADR-020 and ADR-041 tuned by 25.2. Floats replay only on the same build (ADR-009), so MSVC's build may play the same seeds differently.

- **T1, Phase 2 holds: yes.** All 40 end, at a median of 52:45, 28 of them within 45–60 minutes (S4; Phase 2 had 47:55 and 25). S1 is met in all 40, S2 in 39, and S3 sees both endings, 29 by domination and 11 by production. Player 1 wins 28 of the 40, a seat bias not chased here.
- **T2, the cap's stall: missed, and kept** (owner, 2026-10-04). Both players sit at their caps with equal nodes for a median of 9:28 a match, from 6:14 to 10:33, against the 5 minutes asked. Most of it is the opening: both reach level 1's three nodes at about 2:25 and wait for level 2, then sit at four each until one reaches level 3. No setting of the AI's moved it. A level 2 of 150 Ore and 30 s gave 8:40, and a cap one node higher at every level 5:10 but a median match of 25:08 that never needed level 3, so the numbers of §7 stay. The stall does not stall the match: most end by domination.
- **T3, the levels reached: missed on the Research Lab, and kept** (owner, 2026-10-04). Each side reaches level 3 of the Command Station and of a Shipyard in all 40, and of the Research Lab in 11, so all three in 11 of the 40. Tier 3 is opened in 33 of the 40 matches and by 44 of the 80 players, against Phase 1's 2 in 80.
- **T4, upgraded structures fought over: yes.** A median of 4 structures above level 1 is attacked a match, from 0 to 12.
- **T5, does it read: yes.** The owner's judgement in play, on 2026-10-04: a structure's level reads at a glance, and what the next level gives is clear before buying it.

---

## 3. Decided by the owner on 2026-10-04

- **Phase 3 is upgrades, on Phase 2's 5 km map.** The 10 km world, forward Shipyards, relay jumps and pathing by sector, which Phase 2 §7 kept for Phase 3, wait on **the horizon** (below). Phase 3 is measured against the same map and the same S1–S4 as Phase 2, so a change in the figures can be traced to it.
- **The long-term direction is [the horizon](OutpostCommander-Horizon.md)**: a galaxy that keeps running, played by a circle of friends in seasons of weeks and later of months. There the world grows by star systems, each a map like this one, rather than into one 10 km map, and a relay jump becomes travel between systems (Horizon O8). Phase 3's rules are unchanged by it. What Phase 3 builds is what the horizon builds on: an upgraded structure is a target worth a raid, as a vault system would be worth a fleet (Horizon §6.1), and the Command Station's cap on nodes is the pattern for a bound on fleets (Horizon §6.2).
- **Constructors upgrade a structure, and it keeps working** (§4). A Shipyard keeps building, a Lab keeps researching and a Command Station keeps firing while its next level is built.
- **The Research Lab's level opens research tiers** (§6), in place of the gateway topics. Research is tied to the Lab and not to the Command Station, because a lost Command Station is lost for good (Phase 1 gate H5), and losing it would otherwise end a player's research for the rest of the match.
- **The Command Station's level is a hard cap on the nodes a player holds** (§7).
- **The Large Hull research topic stays.** A Large hull needs both the topic and a Shipyard at level 3 (§5).
- **The gates of §12**: a level 1 Shipyard builds Small hulls only (K1); the Lab's levels 2 and 3 replace the gateways, at their Ore and prerequisites (K2); the Lab gains a second research slot at level 4, and the Shipyard stops at level 3 and the Lab at level 4 (K3); the Command Station's cap and upgrade costs as §7 has them (K4); every level fitted to level 1's size (K5); +20% hit points a level, and more guns on the Command Station (K6); and Phase 3 accepted after Phase 2's owner run (K7).

---

## 4. Upgrading a structure

*Decided (gates K5, K6).*

- **A structure upgrades one level at a time**, from level 1 to the highest level its kind has in Phase 3 (§5–§7). The player orders it from the structure's panel or its production window, and assigns Constructors to the work as for any other build. Several Constructors share the work, as they share a build (ADR-016). Its Ore is paid when the order is given, as a structure's is, and nothing is refunded (ADR-016).
- **The structure keeps working** while it is upgraded (§3). The work under way is shown as construction is. If the structure is destroyed during the upgrade, the Ore is lost.
- **Each level adds 20% of the kind's base hit points**, and never armor (Phase 1 §6). The level's percent adds to research's, so a level 3 Lab with Reinforced Structures has 1 + 0.40 + 0.25 times its base hit points (owner, 2026-10-04). A structure being upgraded keeps its share of its hit points, as it does when Reinforced Structures is researched.
- **A Constructor on a structure that is both damaged and being upgraded** builds the level first, since its Ore is paid, and repairs it once the level is in (owner, 2026-10-04).
- **A destroyed structure is rebuilt at level 1.** Its levels are lost with it, which is what makes an upgraded structure a target.
- **The snapshot carries each structure's level**, and the client draws that level's mesh (ADR-045). The upgrade under way is in the snapshot as construction is. Under fog of war, a player remembers an enemy structure at the level it last saw, and an upgrade made out of its sight is not shown until it sees the structure again (owner, 2026-10-04).
- **Every level is drawn at level 1's size** (gate K5). The footprint, and so placement, collision and pathing, are unchanged by an upgrade, and each level's mesh is fitted to the kind's length as level 1's is (ADR-045). At level 1's scale the baked meshes would grow up to 2.6 times in their longest dimension, and the Human Shipyard 5 times in depth (measured from the baked `.nmf` files on 2026-10-04); fitted, a higher level shows as more detail, not more size. This forgoes a level that reads by its size alone, which T5 judges.

---

## 5. Shipyard levels: bigger hulls

*Decided (gates K1, K3); the costs are starting values.*

| Level | Builds | Upgrade from the level below |
|---|---|---|
| 1 | Small hulls | (300 Ore, as a Shipyard today) |
| 2 | Small and Medium | 150 Ore, 30 s |
| 3 | Small, Medium and Large | 300 Ore, 60 s |

- **A Shipyard refuses a job whose hull is above its level.** The designer still designs and saves every hull the player has; its Queue is dim, with the reason, for a design the target Shipyard cannot build.
- **The Large Hull topic stays** (Phase 1 §6, topic 6; owner, 2026-10-04). Researching it lets the player design and save Large hulls; a Shipyard at level 3 is where one is built. A player needs both.
- **This changes the opening** (owner, 2026-10-04). Medium is available from the first second today; in Phase 3 it waits for a Shipyard at level 2. A scout is a Small hull, so S1 should not move, but T1 checks it.
- **The Shipyard stops at level 3** (gate K3). There is no fourth hull for levels 4 and 5 to unlock, and their art waits for content that needs it.

---

## 6. Research Lab levels: research tiers

*Decided (gates K2, K3): the numbers are starting values.*

| Level | Opens | Upgrade from the level below | Requires |
|---|---|---|---|
| 1 | Tier 1 | (200 Ore, as a Lab today) | — |
| 2 | Tier 2 | 400 Ore, 60 s | Improved Extraction and Hull Plating researched |
| 3 | Tier 3 | 600 Ore, 90 s | Large Hull researched |
| 4 | A second research slot | 800 Ore, 120 s | — |

- **The two gateway topics are removed**: Relay Archives (topic 9) and Precursor Vault (topic 18). Their Ore carries over to the upgrade, and their prerequisites carry over to its requirements: Relay Archives required Improved Extraction and Hull Plating, and Precursor Vault required Large Hull, besides the gateway before it.
- **Every topic of tier 2 or 3 requires the Lab's level** in place of its gateway. The other prerequisites are unchanged.
- **The Lab keeps researching while it is upgraded** (§3).
- **A destroyed Lab** loses its levels. Research already finished stays finished, and a rebuilt Lab starts at level 1, so it can queue only tier 1 topics until it is upgraded again.
- **What the tree becomes.** 23 topics, in 2,260 s of research, about 38 minutes with one Lab. The 330 s the gateways took moves to Constructors, as 150 s of upgrade work. The S4 match length is measured again with this in place (T1).
- **Level 4 researches two topics at once** (gate K3). The Lab's queue stays five topics long, and its front two run side by side, each paid when it starts (ADR-017). A topic whose prerequisite is still being researched waits for it, so the second slot never runs a topic before what it requires. This brings in, as a level, what the second Research Lab that Phase 1 kept out of scope would have given, but in one building: destroying the Lab loses both slots and every level with it.
- **What the second slot does to the tree.** Every topic still takes its own time, and the slot shortens the tree's wall-clock time by what runs in parallel. Tier 3's topics, 830 s without its gateway, take about 7 minutes at best with two slots, where prerequisites allow. T1 measures what it does to the match's length, and T3 whether tier 3 is reached.
- **The Lab stops at level 4** (gate K3). A third slot at level 5 would empty the tiers, and level 5's art waits for content that needs it.

---

## 7. Command Station levels: the nodes a player may hold

*Decided (gates K4, K6): the numbers are starting values.*

| Level | Nodes held, home included | Defence guns | Upgrade from the level below |
|---|---|---|---|
| 1 | 3 | 1 | — |
| 2 | 4 | 1 | 300 Ore, 45 s |
| 3 | 5 | 2 | 500 Ore, 60 s |
| 4 | 6 | 2 | 700 Ore, 75 s |
| 5 | 7 | 3 | 900 Ore, 90 s |

- **A Relay beyond the cap is refused.** A Relay under construction counts toward the cap, as it takes its node (ADR-056).
- **The cap refuses new claims; it never takes a node away.** Nodes already held stay held, whatever happens to the Command Station.
- **A player without a Command Station** has level 1's cap, 3 nodes. It keeps every Relay it has, and builds no new one while it holds 3 or more.
- **Why these numbers.** Domination drains only while the two sides hold different numbers of nodes (Phase 2 §8). Two players at the same cap hold equal numbers, and nothing drains: that is the stall T2 measures. On nine nodes, two players at level 1 can each hold their home and both flanks, which is today's opening, and the first player to reach level 2 can go ahead by one node. From level 3, two caps of 5 add up to more than the map's nine nodes, so both sides cannot be at their cap at once: the nodes are contested, and domination decides as it does today.
- **On a larger map, or across a galaxy of systems** (the horizon, §3), these caps have to be scaled to the number of nodes. That is for the phase that builds it to decide.
- **The station's guns** (gate K6). A second Defence gun at level 3 and a third at level 5, each the gun the station carries today, aiming and firing on its own. The simulation gives both sides the same guns; the art does not, with the Human Command Station's 4 hardpoints at level 1 and 12 at level 5 and the Tarkan's 3 and 6, so the client spreads each gun's shots over its set's hardpoints. Three guns and +80% hit points make a level 5 home much harder to raid than today's: the siege that ends a match by the base takes a larger fleet, and T1 checks that S3 still sees it happen.

---

## 8. The AI

The AI must play by every rule above, and it plays these as a player would:

- it upgrades its Command Station to level 2 before its first claim beyond its two flanks, and further whenever it is at its cap and has a node it wants to claim;
- it upgrades a Shipyard to level 2 early, and to level 3 when it wants Large hulls;
- it upgrades its Lab where it would have researched a gateway topic, and to level 4 once tier 3 is open;
- it counts an enemy structure's level when it chooses what to attack: a Shipyard at level 3 is worth more than one at level 1.

Its numbers are starting values in `Opponent.json`, tuned against T1–T3.

---

## 9. The client

- **The selected structure's panel** names its level, "SHIPYARD 01 · L2", and has an Upgrade button showing the next level's cost, its time, and what it gives. When the server would refuse the upgrade, the button is dim and gives the reason: a requirement not met, or too little Ore. Every level keeps level 1's footprint (§4), so no upgrade is refused for room (owner, 2026-10-04).
- **The territory line** shows nodes held against the cap, "4 / 5".
- **The designer** dims Queue for a design the target Shipyard cannot build, and says why (§5).
- **The world** draws each structure at its level, with its own creases and spinning parts (ADR-045).

---

## 10. What changes elsewhere

- **Phase 1 §6:** the research tree loses topics 9 and 18, the gateways (§6). The balance check is unchanged: it checks battles at each tier's components, not when they become available.
- **The match log** records each upgrade started, finished or lost with its structure, each attack on a structure above level 1 (T4), and the time both sides spend at their cap with equal nodes (T2).
- **ADR-033** is edited in place: a tier is opened by the Research Lab's level, and `GatewayEffect` is removed. **ADR-045** is edited in place: the level is in the simulation and the snapshot, and every level is fitted to level 1's size. **ADR-017** is edited in place for the second research slot. **ADR-056** is edited in place for the cap. A new ADR records the upgrade itself.
- **Phase 2 §7** names the horizon, not Phase 3, as the place for the world (§3).

---

## 11. Out of scope for Phase 3

Everything Phase 2 kept out stays out. Also out: the 10 km world, forward Shipyards, relay jumps and pathing by sector (the horizon, §3); levels for the Relay, the Defence Platform and the Mining Rig, which have no art for them; levels 4 and 5 of the Shipyard and level 5 of the Lab (gate K3); new hulls.

---

## 12. Gates

Each is an owner decision, and each blocks the plan's tasks that depend on it.

- **K1 — The Shipyard's levels** (§5). **Decided on 2026-10-04:** Small at level 1, Medium at level 2 and Large at level 3, with the Large Hull topic kept; the upgrade costs are starting values.
- **K2 — The Lab's levels 1 to 3** (§6): the gateways replaced by levels 2 and 3, their Ore and prerequisites carried over, the upgrade times, and a rebuilt Lab back at level 1. **Decided on 2026-10-04:** as proposed, the upgrade times as starting values.
- **K3 — The top levels** (§5, §6). **Decided on 2026-10-04:** the Lab's level 4 is a second research slot, at 800 Ore and 120 s as starting values; the Shipyard stops at level 3 and the Lab at level 4.
- **K4 — The Command Station's cap** (§7). **Decided on 2026-10-04:** 3 to 7 nodes, upgrades of 300, 500, 700 and 900 Ore, and level 1's cap without a station.
- **K5 — The footprint** (§4). **Decided on 2026-10-04:** every level fitted to level 1's size; placement unchanged.
- **K6 — Hit points and guns** (§4, §7). **Decided on 2026-10-04:** +20% of base hit points a level, and 1, 1, 2, 2 and 3 Defence guns on the Command Station at levels 1 to 5.
- **K7 — When Phase 3 is accepted.** **Decided on 2026-10-04:** after the owner has run Phase 2's open tasks (14.3, 15.2, 16.2, 17.1, 17.2, 19.2) and answered S5, so that Phase 3 starts on a Phase 2 that has been played and not only built. **Met on 2026-10-04:** the owner ran Phase 2, answered S5 yes, and accepted Phase 3.

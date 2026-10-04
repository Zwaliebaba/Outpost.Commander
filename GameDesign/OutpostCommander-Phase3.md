# Outpost Commander — Phase 3 Design: Structures That Grow

Status: **draft** · Owner: Stefan Zwaal · Started 2026-10-04, from the owner's answers of that day · Not accepted: the gates in §12 are open, and acceptance waits on Phase 2's owner run (gate K7)

This document says what Phase 3 builds on top of Phase 2, and amends [the Phase 2 design](OutpostCommander-Phase2.md), [the Phase 1 design](Archive/OutpostCommander-Phase1.md) and [the MVP design](Archive/OutpostCommander-MVP.md) where they differ. The names, numbers and layouts here are proposals for the owner to review, as Phase 1's and Phase 2's were first written; what the owner has decided is in §3, and what is still open is in §12.

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

---

## 3. Decided by the owner on 2026-10-04

- **Phase 3 is upgrades, on Phase 2's 5 km map.** The 10 km world, forward Shipyards, relay jumps and pathing by sector, which Phase 2 §7 kept for Phase 3, move to **Phase 4**. Phase 3 is measured against the same map and the same S1–S4 as Phase 2, so a change in the figures can be traced to it.
- **Constructors upgrade a structure, and it keeps working** (§4). A Shipyard keeps building, a Lab keeps researching and a Command Station keeps firing while its next level is built.
- **The Research Lab's level opens research tiers** (§6), in place of the gateway topics. Research is tied to the Lab and not to the Command Station, because a lost Command Station is lost for good (Phase 1 gate H5), and losing it would otherwise end a player's research for the rest of the match.
- **The Command Station's level is a hard cap on the nodes a player holds** (§7).

---

## 4. Upgrading a structure

*Proposed (gates K5, K6).*

- **A structure upgrades one level at a time**, from level 1 to the highest level its kind has in Phase 3 (§5–§7). The player orders it from the structure's panel or its production window, and assigns Constructors to the work as for any other build. Several Constructors share the work, as they share a build (ADR-016). Its Ore is paid when the order is given, as a structure's is, and nothing is refunded (ADR-016).
- **The structure keeps working** while it is upgraded (§3). The work under way is shown as construction is. If the structure is destroyed during the upgrade, the Ore is lost.
- **Each level adds 20% of the kind's base hit points**, and never armor (Phase 1 §6). A structure being upgraded keeps its share of its hit points, as it does when Reinforced Structures is researched.
- **A destroyed structure is rebuilt at level 1.** Its levels are lost with it, which is what makes an upgraded structure a target.
- **The snapshot carries each structure's level**, and the client draws that level's mesh (ADR-045). The upgrade under way is in the snapshot as construction is.
- **The footprint grows with the level** (gate K5). At level 1's scale, the baked meshes grow from level 1 to level 5: the Human Command Station about 2.4 times in its longest dimension, the Human Shipyard about 1.8 times in length and 5 times in depth, the Human Research Lab about 2.6 times (measured from the baked `.nmf` files on 2026-10-04). Proposed: each level has its own footprint radius in the tuning data, and an upgrade whose larger footprint would overlap another structure or an asteroid is refused, as a placement is.

---

## 5. Shipyard levels: bigger hulls

*Proposed (gates K1, K3).*

| Level | Builds | Upgrade from the level below |
|---|---|---|
| 1 | Small hulls | (300 Ore, as a Shipyard today) |
| 2 | Small and Medium | 150 Ore, 30 s |
| 3 | Small, Medium and Large | 300 Ore, 60 s |

- **A Shipyard refuses a job whose hull is above its level.** The designer still designs and saves every hull the player has; its Queue is dim, with the reason, for a design the target Shipyard cannot build.
- **The Large Hull topic is removed** (Phase 1 §6, topic 6). Researching a hull and then upgrading a Shipyard for it would charge twice for the same thing. A Shipyard at level 3 is how a player gets Large hulls.
- **This changes the opening.** Medium is available from the first second today; under this proposal it waits for a Shipyard at level 2. A scout is a Small hull, so S1 should not move, but T1 checks it.
- **Levels 4 and 5 are not built in Phase 3** (gate K3). There is no fourth hull for them to unlock, and filling them with hit points and build speed would be levels for the sake of the art. The art gives the Human Shipyard two hardpoints from level 4, which would suit an armed Shipyard if the owner wants one.

---

## 6. Research Lab levels: research tiers

*Proposed (gates K2, K3).*

| Level | Opens | Upgrade from the level below | Requires |
|---|---|---|---|
| 1 | Tier 1 | (200 Ore, as a Lab today) | — |
| 2 | Tier 2 | 400 Ore, 60 s | Improved Extraction and Hull Plating researched |
| 3 | Tier 3 | 600 Ore, 90 s | A Shipyard at level 3 |

- **The two gateway topics are removed**: Relay Archives (topic 9) and Precursor Vault (topic 18). Their Ore carries over to the upgrade, and their prerequisites carry over to its requirements. Precursor Vault required the Large Hull topic, which §5 removes, so level 3 requires a Shipyard at level 3 in its place.
- **Every topic of tier 2 or 3 requires the Lab's level** in place of its gateway. The other prerequisites are unchanged.
- **The Lab keeps researching while it is upgraded** (§3).
- **A destroyed Lab** loses its levels. Research already finished stays finished, and a rebuilt Lab starts at level 1, so it can queue only tier 1 topics until it is upgraded again.
- **What the tree becomes.** 22 topics, in 2,140 s of research, about 36 minutes with one Lab. The 330 s the gateways took moves to Constructors, as 150 s of upgrade work. The S4 match length is measured again with this in place (T1).
- **Levels 4 and 5 are not built in Phase 3** (gate K3). A second research slot would suit level 4, but it is the second Research Lab that Phase 1 kept out of scope, under another name.

---

## 7. Command Station levels: the nodes a player may hold

*Proposed (gate K4). The owner chose a hard cap (§3).*

| Level | Nodes held, home included | Upgrade from the level below |
|---|---|---|
| 1 | 3 | — |
| 2 | 4 | 300 Ore, 45 s |
| 3 | 5 | 500 Ore, 60 s |
| 4 | 6 | 700 Ore, 75 s |
| 5 | 7 | 900 Ore, 90 s |

- **A Relay beyond the cap is refused.** A Relay under construction counts toward the cap, as it takes its node (ADR-056).
- **The cap refuses new claims; it never takes a node away.** Nodes already held stay held, whatever happens to the Command Station.
- **A player without a Command Station** has level 1's cap, 3 nodes. It keeps every Relay it has, and builds no new one while it holds 3 or more.
- **Why these numbers.** Domination drains only while the two sides hold different numbers of nodes (Phase 2 §8). Two players at the same cap hold equal numbers, and nothing drains: that is the stall T2 measures. On nine nodes, two players at level 1 can each hold their home and both flanks, which is today's opening, and the first player to reach level 2 can go ahead by one node. From level 3, two caps of 5 add up to more than the map's nine nodes, so both sides cannot be at their cap at once: the nodes are contested, and domination decides as it does today.
- **On Phase 4's 25 nodes**, these caps have to be scaled to the map's size. That is Phase 4's to decide.
- **The station's gun** is unchanged by its level, unless gate K6 decides otherwise. The art gives the Human Command Station 4 hardpoints at level 1 and 12 at level 5, and the Tarkan's 3 and 6.

---

## 8. The AI

The AI must play by every rule above, and it plays these as a player would:

- it upgrades its Command Station to level 2 before its first claim beyond its two flanks, and further whenever it is at its cap and has a node it wants to claim;
- it upgrades a Shipyard to level 2 early, and to level 3 when it wants Large hulls;
- it upgrades its Lab where it would have researched a gateway topic;
- it counts an enemy structure's level when it chooses what to attack: a Shipyard at level 3 is worth more than one at level 1.

Its numbers are starting values in `Opponent.json`, tuned against T1–T3.

---

## 9. The client

- **The selected structure's panel** names its level, "SHIPYARD 01 · L2", and has an Upgrade button showing the next level's cost, its time, and what it gives. When the server would refuse the upgrade, the button is dim and gives the reason: a requirement not met, too little Ore, or no room for the larger footprint.
- **The territory line** shows nodes held against the cap, "4 / 5".
- **The designer** dims Queue for a design the target Shipyard cannot build, and says why (§5).
- **The world** draws each structure at its level, with its own creases and spinning parts (ADR-045).

---

## 10. What changes elsewhere

- **Phase 1 §6:** the research tree loses topics 6, 9 and 18 (§5, §6). The balance check is unchanged: it checks battles at each tier's components, not when they become available.
- **The match log** records each upgrade started, finished or lost with its structure, each attack on a structure above level 1 (T4), and the time both sides spend at their cap with equal nodes (T2).
- **ADR-033** is edited in place: a tier is opened by the Research Lab's level, and `GatewayEffect` is removed. **ADR-045** is edited in place: the level is in the simulation and the snapshot, and its footprint is the tuning data's. **ADR-056** is edited in place for the cap. A new ADR records the upgrade itself.
- **Phase 2 §7** names Phase 4, not Phase 3, as the place for the world.

---

## 11. Out of scope for Phase 3

Everything Phase 2 kept out stays out. Also out: the 10 km world, forward Shipyards, relay jumps and pathing by sector (Phase 4); levels for the Relay, the Defence Platform and the Mining Rig, which have no art for them; levels 4 and 5 of the Shipyard and the Lab, unless gate K3 decides otherwise; new hulls.

---

## 12. Gates

Each is an owner decision, and each blocks the plan's tasks that depend on it.

- **K1 — The Shipyard's levels** (§5): Small, Medium and Large at levels 1 to 3, the upgrade costs, and the Large Hull topic removed. The alternative, levels 1 and 2 only, with Small and Medium at level 1, keeps today's opening.
- **K2 — The Lab's levels** (§6): the gateways replaced, the upgrade costs, level 3 requiring a Shipyard at level 3, and a rebuilt Lab back at level 1.
- **K3 — Levels 4 and 5 of the Shipyard and the Lab** (§5, §6). Proposed: not built in Phase 3. Alternatives: an armed Shipyard at level 4, as the Human art suggests; a second research slot at Lab level 4.
- **K4 — The Command Station's cap** (§7): 3 to 7 nodes, the upgrade costs, and level 1's cap without a station.
- **K5 — The footprint** (§4). Proposed: it grows with the level, and an upgrade with no room is refused. The alternative is to fit every level to level 1's footprint, which keeps the placement rules as they are but draws the structure no bigger.
- **K6 — Hit points and guns** (§4, §7). Proposed: +20% of base hit points a level, and the station's gun unchanged. The alternative uses the art's added hardpoints for more guns at higher levels.
- **K7 — When Phase 3 is accepted.** Proposed: after the owner has run Phase 2's open tasks (14.3, 15.2, 16.2, 17.1, 17.2, 19.2) and answered S5, so that Phase 3 starts on a Phase 2 that has been played and not only built.

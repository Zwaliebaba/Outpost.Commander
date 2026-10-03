# Outpost Commander — Phase 1 Design

Status: **accepted** · Owner: Stefan Zwaal · Started 2026-10-02, from the owner's answers of that day; gates H1–H5 and H7 decided the same day

This document says what Phase 1 builds on top of the finished MVP. It **amends** [the MVP design](OutpostCommander-MVP.md) rather than replacing it: everything the MVP design says still holds unless this document changes it, and where the two differ, this one is the authority. AGENTS.md still says how the code is written, `Design/ADR/` records the engineering decisions, and [the Phase 1 plan](ImplementationPlan-Phase1.md) gives the order of the work.

The names, numbers and layouts here were first written as proposals for the owner to review, and the owner accepted them on 2026-10-02 (gates H1–H5). The numbers are first guesses, as the MVP's §12 began: the Q2 check and the P1 measurement tune them. What is still open is in §15.

---

## 1. What Phase 1 is for

The MVP answered "is this game worth building?" with yes. Phase 1 answers the next question the owner asked: **can a match last long enough to be a campaign of its own, and still be the same game?** A match today is over in about five minutes when the owner rushes the AI, and in 15–25 minutes when he does not. The owner wants it to last up to two hours.

Making a match long is easy, and making it worth playing for that long is not. Slowing every clock by five times gives a two-hour match that is the MVP's fifteen in slow motion. The MVP runs out of things to do at about minute 13: its eight research topics take 11½ minutes, and it has 18 designs. So **Phase 1 makes a match longer by giving it stages**, not by slowing it down: tiers of research that change what is built, a map that has to be taken outward as its ore runs out, and a win condition that one raid cannot settle.

**The target is 45–60 minutes, not two hours** (owner, 2026-10-02). Phase 1 builds the stages, measures how long a match lasts with them, and the owner decides from that measurement whether and how to go on to two hours.

Phase 1 also brings the designer to the owner's mockup, makes the game's windows float, banks ships in their turns, and fixes the slow order ticks the MVP left behind.

---

## 2. What Phase 1 must show

These play the part the MVP's Q1–Q5 played. A failed answer is still a result.

| # | Question | How we know |
|---|---|---|
| P1 | Does a match last 45–60 minutes? | A match the owner plays against the AI **without rushing it** lasts 45–60 minutes. Two AIs on the real server give a repeatable figure besides: the median length of 10 seeded AI-against-AI matches is recorded, with the spread. |
| P2 | Does each tier change the game? | In the match log, what each side builds after it finishes a tier's gateway topic differs visibly from what it built before (as Q3, per tier). |
| P3 | Does ship design still matter? | The Q2 check, restructured per tier (§7), passes all four criteria at every tier. |
| P4 | Does the engine hold at Phase 1's scale? | The order ticks that missed Q4 meet 5 ms at 200 ships. Frame and tick times are recorded on the 5 km map at the ship counts a 60-minute match actually reaches, which the match log records (§10). |
| P5 | Does the presentation read? | The owner judges the designer against the mockup, the floating windows, and the banking, in play. |

---

## 3. Decided by the owner on 2026-10-02

- **The length comes from stages: 45–60 minutes first.** Tiers, the win condition and the map make the match longer, not slower clocks. Two hours is decided after P1 is measured.
- **The win condition.** A player loses when its Command Station **and** every Shipyard are destroyed (§4).
- **Content.** Two new weapons and one new drive (§5), and no new hulls, so no new meshes. A research tree of three tiers and about 25 topics, with structure upgrades (§6). One map of about 5 km a side, on which ore runs out (§8).
- **Banking is drawn by the client only** (§9). The simulation, the replay and the Q2 check do not change. Ships still turn on the spot at sharp corners, as ADR-010 has them, and so do not bank there.
- **The designer** is one window for all of a player's designs, and its queue goes to a Shipyard picked in it. Its ×N adds N separate jobs, as many as the Shipyard's queue has free slots. The match keeps running while it is open (§11).
- **Windows float and the HUD stays put** (§12). A window's position lasts until the game is closed.
- **Fonts:** Bahnschrift and Cascadia Mono, which Windows installs, so the game still ships no font (§11).
- **The slow order ticks are fixed** in Phase 1 (§10).

### Accepted risks (owner, 2026-10-02)

The owner kept three answers that pull against the length target, and accepted what they cost:

1. **The AI is not changed to resist a rush.** A rush that reaches the AI's only Shipyard, which stands beside its Command Station, still ends the match in minutes. So P1 is judged on matches where the owner does not rush, and on AI-against-AI matches. The AI is changed only as far as it must be to play Phase 1's rules and content (§13).
2. **There is no unit cap.** Q4 was measured at 200 ships, and the AI built 443 warships in the MVP's first 25-minute match. A 60-minute match on a larger map will go beyond anything measured, in frame time, tick time and the O(n²) parting of ships (ADR-010). P4 records what it reaches. If it fails, a cap is the first answer to propose.
3. **There is no save, load or pause.** A 45–60 minute match is played in one sitting. ADR-009's replay from the seed and command log works only on the same build, and would replay up to 72,000 ticks to resume an hour's match, so it is not a substitute.

---

## 4. The win condition

- **A player loses when it has neither a Command Station nor a finished Shipyard.** A Shipyard still under construction does not count. Constructors, Research Labs, rigs and platforms do not keep a player alive. The match ends, as now, on the tick it happens: a banner says Victory, Defeat or Draw with the match's length, and the world runs on (ADR-020).
- **A lost Command Station is lost for good.** Constructors still come only from the Command Station (MVP §6), so a player who loses it can build no more Constructors, but it keeps the ones it has, its Shipyards keep building warships, and its Constructors can still build Shipyards. The MVP's reason for building Constructors only at the Command Station, that it lasts as long as the match, no longer holds; Phase 1 keeps the rule anyway, so that losing the Command Station costs something real short of the match (owner, 2026-10-02, gate H5).
- **The last production is revealed.** A player who has lost its Command Station has its remaining Shipyards shown to its opponent, through fog of war, as remembered structures (ADR-024), so that the end of a match is not a search of a 5 km map for one building (owner, 2026-10-02, gate H5).
- **What this changes elsewhere.** Auto-targeting is unchanged. The AI's attack group goes for the player's production — Shipyards first, then the Command Station — before any other structure (§13).

---

## 5. Components

Phase 1 adds two weapons and one drive. With three hulls, three drives and five weapons, there are **45 designs**, where the MVP had 18. None is available at the start: each is unlocked by research in tier 2 or 3 (§6). **Every number in this section is a first guess** for the Q2 check to tune, as the MVP's §12 began, accepted as the starting values (owner, 2026-10-02, gate H1).

### The range ladder holds

No new weapon reaches past the Defence gun, so **the Missile Rack is still the only weapon that outranges a Defence Platform** (MVP §6), and no research extends a range (MVP §8). The ladder becomes: Mass Driver 120 m, **Flak Battery 200 m**, Lance 220 m, **Rail Cannon 240 m**, Defence gun 250 m, Missile Rack 280 m. The default 500 m view still fits two groups trading at full range.

### Weapons

| Weapon | Tier | Damage | Interval | Range | Splash | Cost | Role |
|---|---|---|---|---|---|---|---|
| Flak Battery | 2 | 20 | 0.5 s | 200 m | 26 m | 60 | **Breaks the swarm, and holds off the brawler.** Its hits reach the next Small hull of a formation round its target, and still tell against a Medium hull's armour; the Lance line and the missiles outrange it. |
| Rail Cannon | 3 | 320 | 6.0 s | 240 m | — | 150 | **Kills the heavy, and outranges the Lance.** Six hits break a Large+Fusion hull, where a Lance needs 21. Against a Small hull most of the hit is wasted. |

- **The Flak Battery** is the second answer to the swarm, beside the brawler's armour (MVP §7), and the one that scales with the swarm's size. Splash works as the Missile Rack's does (ADR-014): every other enemy within 26 m of the target's center takes the hit, after its own armour.
- **The Rail Cannon** gives a heavy line an answer to the heavy Lance line: Large+Fusion+Rail beats Large+Fusion+Lance. It was meant to answer the Lance picket too, by outranging it by 20 m; in the simulation it does not, since ships stand at their own range rather than kite, and the picket still beats every heavy.

### Tuned by the Q2 check (task 10.3)

The numbers above are the check's, not the first guesses (gate H1). What moved and why:

- **The Flak Battery: 9 → 20 a hit, 140 → 200 m, a 20 → 26 m splash, 70 → 60 Ore.** As first written it beat every Small design and lost every battle to every Medium and Large one, so it was never worth building in either tier (owner, 2026-10-03, gate H10). Its 20 m splash reached no neighbor: a formation stands Small hulls 24 m apart, three footprint radii (ADR-010), and 26 m reaches the four nearest. A splash alone, or a hit of 12 or 14, still lost every battle to a Medium design; the Lance does about 32 damage a second to a Medium hull, and the Flak Battery needed a hit that armour does not quarter and nearly the Lance's reach. With these numbers it beats the swarm, the picket, the brawler and the Medium Missile Rack, trades evenly with the Lance line under spread fire, and loses to it under focus fire and to the heavy Lance and missile lines.
- **The Pulse Drive and the Rail Cannon** kept their first numbers.


### Drive

| Drive | Tier | Speed | Hit points | Turn rate | Cost | Role |
|---|---|---|---|---|---|---|
| Pulse Drive | 2 | ×1.6 | ×0.75 | ×1.5 | 40 | **The raider.** The fastest ships in the game, and the most fragile. Small+Pulse moves at 96 m/s and goes from one start to the other in under a minute. |

The Pulse Drive exists for the larger map. With ore that runs out, a player's income comes from rigs further and further from home (§8), and speed is what reaches them and gets away. **The Q2 check cannot price speed** (MVP §3): in a battle between two clumps, a Pulse ship is a weaker Ion ship. So the Pulse Drive will not be worth building in the check, and (b) exempts it (§7), as the MVP decided not to tighten (b) for drives (owner, 2026-10-02, gate H4).

### What the designer shows for each

Each component gets an abbreviation for the designer's saved-design chips (§11): the hulls **S**, **M** and **L**; the drives **I**, **F** and **P**; the weapons **MD**, **L**, **MR**, **FB** and **RC**. A chip reads hull·drive·weapon, so the position tells the Lance's **L** from the Large hull's.

---

## 6. Research: three tiers

A Research Lab still researches one topic at a time, a player still has one lab, and the lab still queues five topics (MVP §8, ADR-017). What changes is how much there is to research. The MVP's eight topics become **tier 1**, unchanged. Tiers 2 and 3 add 17 topics, for **25 in all, about 42 minutes of research** with one lab. A player who researches without stopping finishes around minute 45, and every topic taken early is another taken late, as in the MVP.

**A tier opens with a gateway topic.** A gateway unlocks nothing by itself: every other topic in its tier requires it. It is expensive and long, so reaching the next tier is a decision with a price, not a side effect.

**Upgrades of the same stat add up.** Hull Plating's +15% and Composite Plating's +15% make +30% hit points, not +32.25%. **Upgrades still change rates, never the size of a hit, and never a range** (MVP §8). Structure upgrades raise hit points, never armour, because armour moves the same breakpoints the size of a hit does.

### The topics

Accepted (owner, 2026-10-02, gate H2). Ore and time are first guesses. Tier 1 is the MVP's, unchanged, so the starting stage of the Q2 check stays the check of record.

| # | Tier | Topic | Requires | Effect | Ore | Time |
|---|---|---|---|---|---|---|
| 1 | 1 | Improved Extraction | — | Mining Rig income +25% | 150 | 60 s |
| 2 | 1 | Hull Plating | — | All hulls' HP +15% | 150 | 60 s |
| 3 | 1 | Mass Driver Calibration | — | Mass Driver fire rate +10% | 150 | 75 s |
| 4 | 1 | Lance Focusing | — | Lance fire rate +15% | 150 | 75 s |
| 5 | 1 | Fusion Drive | 2 | Unlocks the Fusion Drive | 200 | 90 s |
| 6 | 1 | Large Hull | 2 | Unlocks the Large hull | 250 | 120 s |
| 7 | 1 | Missile Rack | 3 | Unlocks the Missile Rack | 250 | 120 s |
| 8 | 1 | Automated Shipyards | 1 | Shipyard build speed +25% | 200 | 90 s |
| 9 | 2 | **Relay Archives** (gateway) | 1, 2 | Opens tier 2 | 400 | 150 s |
| 10 | 2 | Pulse Drive | 9 | Unlocks the Pulse Drive | 300 | 100 s |
| 11 | 2 | Flak Battery | 9, 3 | Unlocks the Flak Battery | 300 | 100 s |
| 12 | 2 | Deep Core Survey | 9, 1 | Every asteroid's ore reserve +30% (§8) | 300 | 90 s |
| 13 | 2 | Composite Plating | 9, 2 | All hulls' HP +15% | 300 | 90 s |
| 14 | 2 | Reinforced Structures | 9 | All structures' HP +25% | 250 | 90 s |
| 15 | 2 | Defence Autoloader | 9 | Defence gun fire rate +20% | 250 | 90 s |
| 16 | 2 | Lance Capacitors | 9, 4 | Lance fire rate +10% | 300 | 90 s |
| 17 | 2 | Missile Guidance | 9, 7 | Missile Rack fire rate +15% | 300 | 90 s |
| 18 | 3 | **Precursor Vault** (gateway) | 9, 6 | Opens tier 3 | 600 | 180 s |
| 19 | 3 | Rail Cannon | 18, 4 | Unlocks the Rail Cannon | 500 | 150 s |
| 20 | 3 | Ablative Armour | 18, 13 | All hulls' HP +5% | 450 | 120 s |
| 21 | 3 | Coilgun Mass Drivers | 18, 3 | Mass Driver fire rate +10% | 400 | 110 s |
| 22 | 3 | Proximity Fuses | 18, 11 | Flak Battery fire rate +15% | 400 | 110 s |
| 23 | 3 | Drive Harmonics | 18 | Every ship's speed +10% | 450 | 120 s |
| 24 | 3 | Fleet Automation | 18, 8 | Shipyard build speed +25% | 450 | 120 s |
| 25 | 3 | Rapid Construction | 18 | Constructor build and repair rate +25% | 350 | 100 s |

Tier 1 takes 690 s, tier 2 890 s and tier 3 1,010 s: 2,590 s, about 43 minutes. Its Ore is 1,500, 2,700 and 3,600.

**Ablative Armour was tuned by the Q2 check (task 10.3): +15% → +5%.** A Medium+Ion+Lance line with every hull plating of tiers 1 and 2 holds 585 hit points, 6.7 of the picket's 87-point Lance hits. Anything above about 4% more takes it to eight hits, and then the picket, its only answer among the tier 2 designs with the same upgrades, wins under half its battles at 6,000 Ore (33% under focus fire at +15%, 45% at +10%). At +5% it still breaks in seven, and the picket wins 95–100% at every tier 3 budget. A hit-point upgrade moves the same breakpoints armour would (§6); the next one should be checked against them first.

**New kinds of effect** the tuning data and the battle model have to learn: a gateway that does nothing itself, an ore reserve, structure hit points, a structure weapon's fire rate, every ship's speed, and the Constructor's rates. Drive Harmonics changes speed only, so the turn rates stay final (MVP §12).

---

## 7. The Q2 check, per tier

The MVP's check plays every design against every other at each budget. With 45 designs instead of 18, a stage that fields them all plays about six times the battles. Cutting the check along the tiers, below, is not what saves time: it is what asks the right question, since a tier-3 component does not exist in a 2,000 Ore fight. Counted from the MVP's 1 min 51 s on the development machine, the three stages below take about 13 minutes there, before the 5% robustness sweep, which grows with the number of components too. It still runs only when `OUTPOST_Q2_FULL` is set, as now.

**The check runs at three stages**, each with the components available by then and budgets that fit when they arrive:

| Stage | Components | Budgets (Ore a side) |
|---|---|---|
| Starting | Those no topic unlocks (as the MVP) | 2,000, 3,000, 4,500 |
| Tier 1 | Every tier-1 component: the MVP's 18 designs | 2,000 – 12,000 (as the MVP) |
| Tier 2 | Adds the Pulse Drive and the Flak Battery: 36 designs | 4,500, 6,000, 9,000, 12,000 |
| Tier 3 | Adds the Rail Cannon: 45 designs | 6,000, 9,000, 12,000 |

**The criteria are the MVP's** (a)–(d), with three changes:

- **(d) runs per tier.** A topic taken by one side only is tested at its own tier's budgets, against the designs the other side has at the tier before. The starting stage keeps the MVP's (d) for tier 1's topics.
- **(b) and the Pulse Drive** (owner, 2026-10-02, gate H4): the Pulse Drive's case is speed, which the check cannot see (§5), so (b) does not require it to be worth building, and play judges it, as the MVP left the Ion Drive's speed to play. What a stage leaves out is still reported.
- **(b) at a later tier judges what the tier adds** (owner, 2026-10-03, gate H9): tier 2's stage asks the Flak Battery to be worth building at one of its budgets, and tier 3's the Rail Cannon. A component of an earlier tier was judged at that tier's budgets, and need not stay worth building as the budgets grow: the Mass Driver, worth building at 2,000–4,500 Ore, is not under focus fire from 4,500 on, with the MVP's numbers.

Budgets stay at or below 12,000 Ore. A battle's time grows with its ships, and at 12,000 Ore the cheapest design already fields 138 a side. A tier-3 fight is about which designs are fielded, not how many.

Tier 1's numbers are already tuned and are not moved by Phase 1 unless a tier-2 or tier-3 result requires it, so that the starting stage remains the MVP's check of record.

**How the check reads this (task 10.3).** A stage plays the components through its tier at their own numbers, with no research, as the MVP's stages do. In (d), the side that does not take the topic has every topic of the tiers before, so it fields every design of the tier before with every upgrade; the side that takes it has the same and the topic with its prerequisites. Only the designs a topic adds or changes are tested: one the other side has with the same numbers is not the topic's doing. For tier 1 that is the MVP's (d).

---

## 8. The map and ore that runs out

### Ore runs out

Accepted (owner, 2026-10-02, gate H3).

- **Every ore asteroid holds a reserve**, set in the map data. Its Mining Rig draws its income from the reserve until the reserve is gone.
- **An exhausted asteroid still pays a trickle**: its rig earns 20% of its yield for the rest of the match. So the map never runs completely dry, a match cannot stall for want of any income, and a player who has lost everything far from home still earns something.
- **Improved Extraction drains faster.** It raises the rate, so the reserve lasts a fifth less. Deep Core Survey (§6) adds 30% to every reserve on the map, including what is left in the ones being mined, for the player who researches it only.
- **A player sees what is left** in an asteroid it can see, and remembers the figure from when it last saw it, as it remembers structures (ADR-024).
- The MVP put depletion out of scope (MVP §13). Phase 1 brings it in because it is what moves the fight across the map over an hour: home rigs run dry around minute 25–30, and from then the income is out where the fleets meet.

### The map

Accepted (owner, 2026-10-02, gate H3); the layout itself is confirmed in its task.

One map, **5,000 × 5,000 m**, point-symmetric as the MVP's, with the starts in opposite corners about 750 m in from the edges, about 4.9 km apart. Four rings of ore, each richer and further out than the last:

| Ring | Asteroids | Yield | Reserve | Lasts at its yield |
|---|---|---|---|---|
| Home | 3 a side | 5 Ore/s | 7,500 | 25 min |
| Near | 4 a side | 6 Ore/s | 9,000 | 25 min |
| Contested (the middle) | 6 | 8 Ore/s | 12,000 | 25 min |
| Rich (the two empty corners) | 2 a corner | 10 Ore/s | 24,000 | 40 min |

That is 24 ore asteroids holding 285,000 Ore. Asteroid fields stand between the rings as obstacles and chokepoints, and every passage is at least as wide as the map's minimum gap, as in the MVP. The rich corners are equally far from both starts and far from both: whoever holds one is exposed. The layout itself is written in the map task and confirmed by the owner before it is played (as MVP task 2.3).

**Laid out in sectors for Phase 2** (owner, 2026-10-03). The map is divided into about nine sectors, each with a node site among its asteroids, and the map data names the sectors, their node sites and their adjacency, as [the Phase 2 draft](OutpostCommander-Phase2.md) §4 has them. Phase 1 reads none of it, and its rings, yields and reserves are unchanged; it makes this the first map Phase 2's territory is played on.

**What a 5 km map costs:**

- **Travel time.** From one start to the other, a Small+Ion ship takes about a minute, a Medium+Ion about 95 s and a Large+Fusion about four minutes. Heavies become a decision about where they will be needed, which is intended; the Pulse Drive is the answer to "too slow to get there".
- **The camera** still zooms out only to a view 1,600 m wide (gate G3), a third of the map. The minimap becomes the way to move around it. Whether to widen the zoom is decided after the owner's first matches on it (gate H8).
- **Fog of war** draws 20 m cells: 62,500 of them instead of 10,000.
- **Pathing** builds its visibility graphs from more obstacles: their cost grows faster than the number of obstacles (ADR-010), and they are rebuilt whenever a structure is placed or destroyed. This is measured in the order-tick work (§10).

---

## 9. Banking

Ships lean into their turns as aircraft do. **It is drawn by the client and nothing else** (owner, 2026-10-02): the server still knows only a ship's position and heading, and the simulation, the snapshot, the replay and the Q2 check are unchanged.

- **The angle comes from the turn.** A ship banks in proportion to its sideways acceleration: how fast its heading turns times how fast it moves. A ship turning on the spot moves at nothing and does not bank. A ship flying straight levels out.
- **Each hull has its own limit and its own response**, as presentation data: a Small hull leans hard and quickly, up to about 35°, and a Large one slowly and only about 12°. The Constructor banks too, gently. The numbers are presentation data beside each model's length (ADR-011, ADR-018), not tuning data.
- **The bank is smoothed.** The heading arrives 20 times a second (ADR-009), so its rate steps every tick; the angle follows it through a spring that settles in about a quarter of a second, so the lean rolls in and out rather than jumping.
- **Everything attached rolls with the hull**: the gun hardpoints that shots leave from, the exhaust (ADR-019), the crease lines (ADR-027), and the shards of an explosion that starts mid-turn (ADR-026). The footprint, the selection ring and the health bar stay flat on the plane.
- **The limit.** ADR-010's ships turn toward their next waypoint before they move, and move slowly while they turn, so at a sharp corner a ship slows, pivots and goes on, and banks very little. Banking shows on the gentle curves of a long route and in formation. Turning in arcs, which would make every corner a bank, changes movement, pathing, kiting and the turn rates, and is not part of Phase 1.

---

## 10. The order ticks

The MVP recorded Q4's tick half as missed when 200 ships are ordered at once: 5.7–8.7 ms on the development machine, and 20.7 ms on the first such tick (MVP §3). The owner left them for after the MVP, and Phase 1 fixes them, before the larger map and longer matches make them worse.

- **The target is Q4's:** no tick over 5 ms with 200 ships and 40 structures, the orders included, on the development machine in Release|ARM64.
- **It starts with a measurement**, not a fix: where the time in an order tick goes. ADR-010 suspects forming up and pathing two groups of 100, and the 20.7 ms first tick looks like building the path graphs that a structure change dropped. The fix follows what the measurement finds, and is recorded as an ADR. The ways open to it include building the graphs off the tick, spreading a large order's per-ship work over the following ticks within Q5's 150 ms, and a spatial grid for the pairs that part ships.
- **What the measurement found** (task 8.2, ADR-032), in a Linux build with clang 18, which runs the MVP's order ticks in about a third of the development machine's time: the 20.7 ms first order tick was the four path graphs, which match setup dropped by placing the bases after building them, and the later order ticks were the ships' paths, about 60 full searches each, mostly line tests against every obstacle. Parting ships cost 0.07–0.2 ms a tick. The fix builds the graphs after setup and on quiet ticks, plans an order for more than 32 ships over two ticks, and makes a search cheaper for the same answer: the worst order tick there falls from 2.58 ms to 1.24 ms, about 4.2 ms on the development machine. The owner's `--measure --load` run records the development machine's figures here.
- **The 5 km map is measured too** once it exists (§8, P4), at the ship counts the match log shows a 60-minute match reaching. With no unit cap (§3), that count is a result, not a limit.
- **The match log records** each side's warship count over time and its peak, and the times each tier's gateway finishes, for P1, P2 and P4.

---

## 11. The ship designer

The designer becomes the owner's mockup, [`Mockups/ShipDesigner.png`](Mockups/ShipDesigner.png): a window of its own rather than a panel beside a selected Shipyard (MVP §9). What the MVP decided about designs stands: a saved design is renamed, never changed; Queue on components that are not yet a saved design saves them first (ADR-023); a name is up to 32 characters.

### One window, a Shipyard picked in it

- **There is one designer window**, because designs belong to the player, not to a Shipyard. Its title names the Shipyard its queue goes to — "SHIPYARD 01 · DESIGNER" — and arrows beside the name step through the player's finished Shipyards. Selecting a Shipyard in the world while the window is open makes it the target. With no Shipyard, the designer still designs and saves, and Queue is dim.
- **The header** holds the target Shipyard's queue as five slots, how many ships it has built this match, and the player's Ore.
- **Shipyards are numbered** in the order they were finished, 01, 02 and on, and a number is not reused.

### What it holds, top to bottom

1. **The name** with its count of characters, and a button that reads Save, or Saved when the name and components are a saved design.
2. **The saved designs** as chips, each its name and its abbreviation (§5), the one shown highlighted. Clicking a chip loads it. The chips run in one row that scrolls sideways when there are more than fit.
3. **One row per slot — hull, drive, weapon** — with the slot's choice on the left and a card for each component: its name, its cost, and its numbers in a line. The chosen card is highlighted. A **locked** card is hatched and dimmed, with the topic that unlocks it ("RESEARCH · LARGE HULL"), and cannot be chosen. A row holds three cards and wraps to a second line, so the weapon row, with five, takes two.
4. **Performance:** hit points, armour, speed, range, cost and build time, each a bar and a figure. A bar is measured against the best value any design in the game reaches, locked components included, so that the scale does not shift as research unlocks parts.
5. **Damage per second after armour against each hull**, a card for each: the figure per ship, large, and per 100 Ore beneath it, with a bar colored by how it compares to the best any design does to that hull: green from two thirds up, amber from one third, red below. This is the MVP's per-ship and per-Ore figure (MVP §9), laid out.
6. **Hovering a card previews it:** the performance bars and the damage cards show the design with that component in place, and the change against the current one. The help line says so.
7. **The bottom row:** Rename; a stepper, − ×N +; and Queue, with the build time of one ship and the cost. **×N adds N separate jobs** to the target Shipyard (owner, 2026-10-02), at most as many as its queue has free, so the stepper runs from 1 to the free slots. The figure on Queue is N times the design's cost. Each job is paid when it starts, as now (MVP §5).

### The look

The mockup's look is the interface's from Phase 1 on: a dark navy panel with a hatched header and corner brackets, uppercase labels in small spaced capitals, condensed headings, figures in a monospaced face, gold for Ore with its ◆ mark, a green Queue button, and amber for what research unlocks. The exact colors are taken from the mockup in the interface task.

### What the interface needs for it

The MVP's interface cannot draw the mockup (ADR-015): it has one size of one font, Segoe UI Semibold, printable ASCII only. Phase 1 needs:

- **Two faces, at several sizes:** **Bahnschrift**, condensed and semibold, for headings, labels and names, and **Cascadia Mono** for figures. Both are installed with Windows 11, so the game still ships no font (owner, 2026-10-02). That Cascadia Mono is installed on the development machine is checked first; if it is not, Consolas takes its place.
- **Spaced capitals** for labels, as the mockup's.
- **A few characters beyond ASCII**: the × of the stepper and the · between words. The ◆ of Ore, the checkbox of a locked part, the hatching and the corner brackets are drawn as small images in the atlas, not as characters.

This is a new ADR that supersedes ADR-015's one size and its ASCII-only text, and keeps its glyph atlas drawn as quads.

---

## 12. Floating windows

The game's screens become windows that float over the battlefield, and the HUD stays where it is.

- **What floats:** the designer (§11), the Research Lab's research, and a structure's production queue. Each opens from a button on the selection panel when its structure is selected, and from a key, clear of the orders, the camera and the control groups: **D** for the designer, **P** for production and **R** for research, each of which also closes its window.
- **The production window** shows one producer's queue and the buttons that add to it (owner, 2026-10-03): the Command Station's Constructor, or a Shipyard's saved designs. It opens on the producer selected, selecting another while it is open switches to it, and arrows step through the player's finished producers, the Command Station first and then the Shipyards by number.
- **The research window** shows the player's Research Lab: its queue, and each topic not researched or queued yet with what it does and costs, dim while its prerequisites are neither, as the Lab's panel did.
- **The queue buttons live in the windows only** (owner, 2026-10-03). A selected structure's panel keeps its name, hit points and construction, and the buttons that open its windows: Production on the Command Station and on a Shipyard, Ship designer on a Shipyard, Research on the Research Lab. Queuing a Constructor takes one click or key more than in the MVP.
- **What stays put:** the Ore and income, the minimap, the selection panel, and the build and order buttons, anchored where they are (ADR-015).
- **A window** is dragged by its title bar and kept on the screen, at least its title bar. Clicking it brings it to the front. It closes with its × or with Esc, which closes the front window first. Several can be open at once.
- **A window remembers where it was** until the game is closed (owner, 2026-10-02): closing and reopening it, or starting another match, puts it back where it was left. A new launch puts every window at its default place. Nothing is written to disk.
- **The match keeps running** while any window is open (owner, 2026-10-02). The designer is large — about 38% of the screen's width and 65% of its height at the mockup's size — and it covers that much of the battlefield while it is open.
- **Input focus stays a rectangle test** (ADR-015), now in the windows' order from front to back, then the HUD, then the world. A drag begun in the world still ends wherever it is let go.

---

## 13. The opponent in Phase 1

The AI is not made harder to rush (§3). It is changed only so that it can play Phase 1 at all, through its settings file where it can be (ADR-020):

- **It researches the 25 topics** in a fixed order, extended past tier 1.
- **It counters the new designs:** answers to the Flak Battery, the Rail Cannon and the Pulse Drive's designs in its counters, and the new designs as answers where the Q2 check says they win.
- **It follows the ore.** It builds on the nearest asteroids with ore left and moves on when one runs dry, rather than holding a fixed list of home and contested asteroids.
- **It attacks production first** (§4): Shipyards, then the Command Station, then anything else.

---

## 14. Out of scope for Phase 1

Everything the MVP put out of scope (MVP §13) stays out, apart from asteroid depletion, which Phase 1 brings in. Also out of Phase 1, by the owner's answers of 2026-10-02: new hulls and new meshes; turning in arcs; a unit cap; save, load and pause; an AI that resists a rush, and difficulty levels; more than one map; a second Research Lab; writing window positions to disk. Two hours, as a target, waits for P1.

---

## 15. Open questions

Each is a gate in the plan, and blocks the tasks that need it.

- **H6 — The look's details** (§11): the window's colors and sizes, taken from the mockup, confirmed when the owner first runs it.
- **H7 — Cascadia Mono** (§11): task 9.1 checks whether it is installed on the development machine, and uses Consolas if it is not (owner, 2026-10-02). The task records which.
- **H8 — The camera's zoom on the 5 km map** (§8): whether the 1,600 m limit of gate G3 stays, after the owner's first matches on the map.

Decided on 2026-10-02, from the owner's answers: §3.

Decided on 2026-10-02, the gates:

- **H1:** the Flak Battery, the Rail Cannon and the Pulse Drive start at the numbers in §5, for the Q2 check to tune.
- **H2:** the research tree of §6 as written: tier 1 unchanged, the two gateways, and the 17 new topics.
- **H3:** the 5 km map of §8: four rings, 24 ore asteroids, their yields and reserves, and the 20% trickle once one runs dry.
- **H4:** Q2's (b) does not require the Pulse Drive to be worth building; play judges it (§7).
- **H5:** a lost Command Station is lost for good, and a player without one has its remaining Shipyards revealed to its opponent (§4).
- **H7:** task 9.1 checks for Cascadia Mono, and falls back to Consolas.

Decided on 2026-10-03, from task 10.3's first runs:

- **H9:** Q2's (b) at tiers 2 and 3 asks only the components the tier adds to be worth building (§7). The run found the Mass Driver never worth building at tier 3, nor under focus fire at tier 2, and that is the MVP's numbers: with them no Mass Driver design was worth building under focus fire at 4,500 Ore or more. Tier 1's numbers do not move for it.
- **H10:** the Flak Battery gets a role against the Medium hull, beside breaking the swarm, and task 10.3 tunes it to one (§5). As first written it beat every Small design and lost every battle to every Medium and Large one, and was never worth building; its 20 m splash also reached no neighbor of a Small hull, which the formation stands 24 m away.

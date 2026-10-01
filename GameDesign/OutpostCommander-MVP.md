# Outpost Commander — MVP Design

Status: **draft for review** · Owner: Stefan Zwaal · Started 2026-09-29 · Revised 2026-09-30 after the first and second reviews

This is the design authority AGENTS.md refers to: it says *what* is built. AGENTS.md says *how* the code is written, and `Design/ADR/` records the engineering decisions taken while building it.

Outpost Commander is a real-time strategy game in space, built on the model of **Warzone 2100**: build a base with constructor units on contested resource points, research new technology, **design your own ships from components**, and use them to destroy the enemy.

The MVP exists to answer one question — **is this game worth building?** — and everything in it serves that. Anything that does not help answer it is out of scope, however cheap it looks.

---

## 1. Premise (proposal — edit freely)

A century ago the jump relays went dark. The colonies on the far side — the **Terrakin**, as the frontier settlers came to call themselves — were left with what they had brought: mining tools, a few shipyards, and the salvage of a fleet that would never go home.

The **Kessler Reach** is an asteroid belt thick with ore and the wrecks of that fleet, and whoever holds it can rebuild. Every faction plants its outposts there. The player is an **Outpost Commander**: land a Command Station, mine the rocks, recover lost technology, and design the ships that will keep what you have taken.

The echo of Warzone 2100 is deliberate — rebuilding after a collapse, with research as *recovery* of what was lost rather than invention — and it gives the MVP a reason for its mechanics without needing a campaign.

For the MVP, the opponent is **The Tarkan High Command**, the Tarkan for short: a rival power that plants its own outposts in the Reach. It is the AI player's faction (§10), drawn with the Tarkan mesh set (§11) in its own colour, while the player's Terrakin are drawn with the Human set. The Tarkan fight with the same components, research and rules as the player, so the match stays symmetric and Q2 is not muddied. A faction with rules of its own, and any further race, is post-MVP.

---

## 2. Pillars

1. **Your fleet is your design.** Ships are assembled from a hull, a drive and a weapon. The best design depends on what the enemy fields, and changing it mid-match is a real decision.
2. **Territory is income.** Ore comes from asteroids you hold. Expanding, defending and raiding mining rigs is the strategic layer.
3. **Research is recovery.** Tech unlocks new components and upgrades. What you research determines which designs you can build.
4. **Readable at a glance.** A flat battlefield under an RTS camera. You can always see who is where and who is winning.

---

## 3. What the MVP must prove

| # | Question | How we know |
|---|---|---|
| Q1 | Is the loop — mine, build, research, design, fight — fun? | A full match against the AI is something the owner wants to play again. It lasts 15–25 minutes. |
| Q2 | Does ship design matter? | The Q2 check below passes: from the first minute to the late game every design has a counter and every hull, drive and weapon is worth building, no research topic is a trump card, and no counter hangs on a single number. |
| Q3 | Does research drive the pacing? | Research choices visibly change what the player builds in the mid-game. |
| Q4 | Is the tech feasible? | 200 ships and 40 structures in combat at 1920×1080 on the development machine, with the HUD drawn over the scene: 99% of frames take ≤ 16.7 ms (a steady 60 fps), and a simulation tick takes ≤ 5 ms at that load. |
| Q5 | Does the server boundary hold, and what does it cost? | The build enforces it: the client and the AI can include only the protocol headers, so reaching into server state does not compile (ADR-002). The delay from an order to the ship visibly responding is measured on the development machine and is ≤ 150 ms. |

Q4 and Q5 are the engineering risk. Q1–Q3 are the design risk. **A failed answer is still a result.** The MVP is done when all five are answered, not when they are all "yes".

Q4 is measured with the HUD on screen because the game draws its own UI (ADR-001), and that is frame time the scene alone does not show. Q5's 150 ms is ADR-002's 50–100 ms for the tick and interpolation, plus about two frames for input and presentation. Both are targets until the first measurement, and the measurement is what gets recorded. When a target is missed, PIX is the tool for finding where the time goes. The client's regions are named in Debug builds only (ADR-005), so the recorded figures come from the game's own timings of a Release build.

**Measured on 2026-09-30 (task 2.7).**

The machine was a laptop with an Intel Core i7-12700H, 16 GB of memory, and an NVIDIA GeForce RTX 3070 Ti Laptop GPU rendering. The display was a 1920×1080 panel at 165 Hz driven by the integrated Intel Iris Xe. It ran Windows 11 Pro, build 26200, and the Release|x64 build. The runs were driven by a script that injected the input; the owner has not yet repeated them by hand.

| Question | What | Figure | Target |
|---|---|---|---|
| Q5 | From an injected right-click to the first presented frame in which an ordered ship visibly responds, 30 move orders | mean 43 ms, median 44 ms, 95th percentile 66 ms, worst 67 ms | ≤ 150 ms: **met** |
| Q5 | The same, from when the game read the click | mean 38 ms, worst 62 ms | — |
| Q5 | The boundary: `#include "GameLogic.h"` added to `GameApp` and to `Opponent` | both fail with C1083 | fails to compile: **met** |
| Q4, tick half | 200 ships and 40 structures, both fleets kept moving, 1,181 ticks over 60 s | mean 0.18 ms, 99th percentile 0.37 ms, and under 1 ms on every tick but those below | ≤ 5 ms |
| Q4, tick half | The ticks where both 100-ship fleets are ordered at once, every 10 s | 3.0, 3.7, 2.7, 5.0, 6.8 and 8.3 ms, rising as the fleets spread | ≤ 5 ms: **missed on 2 of 6** |
| Q4 | 200 ships and 40 structures in combat at 1920×1080 with the HUD (task 3.7, `--measure --stress`): frame CPU and GPU work, and every tick | **not measured yet**: the owner's run | 99% of frames ≤ 16.7 ms; ticks ≤ 5 ms |

How each figure was measured:

- **Q5.** `--measure` stamps each click when the game reads it from the queue, on `std::chrono::steady_clock`, which counts `QueryPerformanceCounter`.
  - **Visibly responds** means the first frame in which an ordered ship's center or nose has moved at least one pixel on screen, at the default 500 m view.
  - **The end time** is taken after `Present` returns for that frame.
  - **The start time** is the script's own `QueryPerformanceCounter` reading just before it injects the click, so the figure includes the time the click waits in the queue.
  - **Not included:** scanout, and the display's own latency.
  - **Setup:** each order was given to ships at rest, and a random 150–400 ms pause placed the clicks across the frame and tick cycles.
- **Q4.** The server times each tick on its own clock: applying its commands, the simulation step and every snapshot (`Server::TakeTickDurations`). `--load` adds the load (`PlaceMeasurementLoad`), and both players' ships are ordered across the map every 200 ticks.

- **Q4 in combat (task 3.7).** `--stress` runs the stress scene (`StressLoad`): each player keeps 100 ships of its four starting designs and 20 structures with the tuning data's hit points and armor. Both fleets attack-move on each other's rally a third of the way in from the starts, lost ships come back at the rally with the same order, and ships left standing idle are sent after the enemy once a second, so the whole of both fleets keeps fighting for as long as the run lasts. Since milestone 4 the scene's structures block movement and its Command Stations and Defence Platforms fire, so a structure's death rebuilds the path graphs (ADR-010), and the starting base counts toward the 40 structures. The rival connects, so the server builds both players' snapshots, as it will against the AI. With `--measure` the game logs, for every frame:
  - **CPU work:** from the moment the swap chain lets the frame start to the return from `Present`. It takes in the window's messages, the server's ticks, the client's update and the recording of the frame, since all of them run on that thread.
  - **GPU work:** between two timestamps at the start and the end of the frame's command list, which holds the scene, the effects and the HUD, read back once the frame's fence has passed.
  - **Not included:** the compositor, scanout and the display. The present interval is not the measure: with vsync on, a frame of 5 ms of work still presents every vsync interval (ADR-006).
  - **The display:** the back buffer's size and the refresh rate of the display the window is on, logged whenever the size changes.
  - `python Tools/FrameTimes.py` summarizes the log: mean, median, 95th and 99th percentiles and worst, for frames, ticks and order responses, leaving out the first 120 frames of loading. The run records the view it was taken at; zoomed out so that both fleets are in view is the heavy case.

The slow ticks are the orders: pathing and forming up two groups of 100 ships in one tick. ADR-010 expected about one tick budget for a 200-ship order, and it grows as the ships spread out. Every other tick is a twentieth of the budget. The owner decided on 2026-10-01 that a group's order paths once for the group rather than once per ship (ADR-010), which cut the same order tick from about 3.4 ms to about 1 ms in a Linux container; the development machine's figure comes from repeating `--measure --load`.

**The Q2 check.** Designs fight in clumps bought with equal Ore, under two targeting extremes: every ship shoots a random enemy (spread fire), or every ship shoots the weakest one (focus fire). Real targeting sits between the two (§7). The battles run at two stages of a match:

- **Every component**, at 2,000, 3,000, 4,500, 6,000, 9,000 and 12,000 Ore a side. The largest is about five minutes of income for a player who holds half the middle (§5): a late-game fleet.
- **The starting components**, those no research topic unlocks (§8), at 2,000, 3,000 and 4,500 Ore: the armies of the first minutes, before the first unlock.

Q2 is "yes" when, at every budget of both stages and in both modes (for (b), over each stage's budgets):

- **(a)** every design has a counter that beats it in at least four battles out of five;
- **(b)** the designs worth building, those the equilibrium mix of the win-rate matrix gives 5% or more, use every hull, drive and weapon available at that stage, at one budget of the stage or more, in each mode (owner, 2026-10-01). A heavy hull need not pay its way in the smallest fight, nor a medium one in the largest: research and scale are what bring the heavies in;
- **(c)** none of the counters in (a) against those designs stops winning when any single number in §12 moves by 5%, so no result hangs on a breakpoint;
- **(d)** at the starting budgets, no research topic, taken with its prerequisites by one side only, gives that side a design that none of the other side's starting designs beats in at least half the battles. Research is an edge, not a trump card. The research comes free in this test, which favours the side that has it.

A threshold counts as met only when the 95% confidence interval of the win rate clears it. A result whose interval straddles its threshold is run again with more battles, and if it is still in doubt, the check fails.

The check cannot judge the drive. Ion buys speed, and in a battle between two clumps that close and fire, speed only decides who fires first. What speed is worth — reaching a raid, leaving a losing fight, crossing the map — is judged in play, by Q1.

`Tools/BattleModel.py` runs the check against the numbers in `OutpostCommander/Assets/Tuning.json` (§12), and §12 is tuned against it until milestone 3. From milestone 3 the same battles run as scripted headless tests in `GameLogicTests` against the real simulation. Where the two disagree, the simulation is right and the model is what gets fixed. Where §12 stands against the check today is recorded in §12.

---

## 4. The battlefield

- **Flat plane, 3D rendering.** Every ship, structure and asteroid sits on the plane (y = 0). Ships turn and move in 2D. The renderer draws full 3D meshes under a tilted perspective camera. There is no altitude in the simulation.
- **One map.** About 2,000 × 2,000 m, roughly 4 × 4 screens at the default view. Two start positions in opposite corners. About 12 ore asteroids: 3 **home** asteroids near each base and 6 **contested** ones in the middle, which yield more (§5). Non-mineable asteroid fields act as obstacles and chokepoints.
- **Obstacles are circles.** Asteroids block movement as circular footprints. There is no terrain, height or line of sight in the MVP.
- **The layout is data:** [`OutpostCommander/Assets/Map.json`](../OutpostCommander/Assets/Map.json) (ADR-008). It is point-symmetric, so neither start is favored, and every passage is at least as wide as the file's `minimumGapMeters`, so every asteroid can be reached from both starts. The owner confirmed the layout on 2026-09-30.
- **Camera.** Pan (edge scroll, arrow keys, middle-drag), zoom (wheel, clamped), rotate around the focus point (Q/E). The arrow keys pan because A and S are orders (§9). The pitch is fixed and comes from the zoom level.
- **The default view shows a whole engagement.** It is about 500 m wide. The longest reach in the game is the Missile Rack's 280 m (§6, §7), so two groups trading at full range fit on one screen with room around them. How far the camera zooms in and out from there is open (§15); until it is decided, the camera's data holds provisional limits of 150 m and 1,600 m (ADR-012).
- **No fog of war in the MVP.** It is the first feature after the MVP, and the server model is shaped so that it can be added (§9).

---

## 5. Economy

- **One resource: Ore.** It works like Warzone 2100's oil: a **Mining Rig** built on an ore asteroid produces a fixed income into the owner's stockpile. There is no hauling and no depletion.
- **The middle pays more.** A home asteroid yields 5 Ore/s and a contested one 8 Ore/s (§12). A player who holds only their home asteroids earns 15 Ore/s; one who also holds half the middle earns 39. The middle is the prize, and fighting for it is when the fleets meet. What keeps one lost fight from deciding the match is home defence: a Defence Platform holds off light raiders, and an attacker has to bring a Lance line or a Missile Rack to break it (§6).
- Each ore asteroid holds one rig. A rig can be destroyed, and the asteroid is then free to rebuild on.
- **A rig is cheap on purpose.** At 50 Ore it pays for itself in 10 s at home. What an expansion costs is exposure — a rig far from home needs the fleet or a platform to keep it — not its price.
- Everything costs Ore: structures, ships and research. Costs are paid when the job **starts**, as in Warzone 2100. This keeps the rules simple. The MVP has no way to cancel a job once it has started, so nothing is ever refunded.
- Starting stockpile and income rates are tuning values (§12).

---

## 6. Structures

| Structure | Role | Placeholder mesh |
|---|---|---|
| **Command Station** | The base. Builds Constructors, queue of up to 5. Carries a Defence gun. Losing it loses the match. Pre-placed at start. | `Station` |
| **Shipyard** | Builds ships from designs. Queue of up to 5. | `Station`, scaled and tinted |
| **Research Lab** | Researches one topic at a time. One per player. | `Satellite` |
| **Mining Rig** | Built on an ore asteroid. Produces Ore. | `Mine` |
| **Defence Platform** | Stationary turret with a Defence gun. | `Mine`, tinted |

- Structures are built by **Constructor** ships. Several constructors on one site build faster, as in Warzone 2100.
- Structures are placed freely on the plane with a circular footprint that must not overlap anything. Mining Rigs snap to an ore asteroid.
- There are no structure upgrades or modules in the MVP.
- **Constructors come only from the Command Station.** It exists for as long as the match does, so a player can always rebuild, and the Shipyard's queue stays free for warships. If Shipyards built them, a player who lost every Shipyard and Constructor could never build again for the rest of the match.
- **Only the Missile Rack outranges a Defence Platform.** The Defence gun reaches 250 m, beyond the Mass Driver (120 m) and the Lance (220 m). A Lance line can still break a platform, but it takes fire while it does. Only the Missile Rack (280 m) destroys one without being shot at, and that is the Missile Rack's job (§7). No research extends a range (§8), so this ladder holds for the whole match.
- **The armed structures are armoured.** The Defence Platform and the Command Station have an armour of 10, which cuts a Mass Driver hit from 14 to 4, so light raiders cannot simply swarm them: a lone platform outlasts a raid of five Small+Ion+Mass Driver ships and destroys it, in the simulation in 42 s with 28% of its hit points left (§12). Every other structure has no armour, so raiding a Mining Rig, Shipyard or Research Lab works.
- **The Command Station is armed** so that a handful of early ships cannot end a match. Unarmed and unarmoured, it would fall to seven Small+Ion+Mass Driver ships — about 610 Ore, ready around 1:30 — in about 25 s. Armed and armoured, its gun destroys all seven in under a minute, with more than half its hit points left. The simulation plays both (task 4.6, §12).

---

## 7. Ships and the designer

Every combat ship is a **design**: **hull + drive + weapon**. The player names a design, saves it, and queues it at a Shipyard. Design stats are derived from the components and shown live in the designer.

The **Constructor** is the one fixed design. It has no weapon, it can build and repair, and it uses the `Colonizer` mesh. Players start with two, and more are built at the Command Station (§6). Its numbers are a provisional baseline the owner set on 2026-10-01 (§15), in the tuning data with everything else (§12).

### Components (MVP set)

Research unlocks the Large hull, the Fusion Drive and the Missile Rack (§8). Everything else is available from the start, so the first minutes are played with four designs: a Small or Medium hull, the Ion Drive, and a Mass Driver or a Lance. Every match starts with those four saved (§9).

**Hulls** set hit points, armour, base speed, size and cost.

| Hull | Mesh | Character |
|---|---|---|
| Small | `Small` | Cheap, fast to build, fragile |
| Medium | `Medium` | The all-rounder |
| Large | `Large` | Slow, expensive, very tough |

**Drives** multiply speed, turn rate and hit points. A drive has no mesh, so the hull mesh is used as is.

| Drive | Character |
|---|---|
| Ion Drive | Fast, turns quickly, lightly protected |
| Fusion Drive | Slow, much tougher, costs more |

The drive trades mobility for durability. The Q2 check cannot price speed (§3), so whether that trade is a real choice is judged in play.

**Weapons** set damage, rate of fire, range and projectile type. Hits are instant in the simulation: the projectile or beam on screen is presentation (§11), and nothing dodges it.

| Weapon | Range | Character |
|---|---|---|
| Mass Driver | 120 m | Short range, fast fire, cheap. The most damage per Ore against light armour, and armour blunts it. |
| Lance | 220 m | Long range, slow, heavy hit. Armour barely slows it, and a small hull wastes much of its hit. |
| Missile Rack | 280 m | The longest range, with splash damage. Breaks defended positions and clumps. |

### Combat rules

- **Weapons are turrets.** A ship fires in any direction and keeps firing while it moves, as in Warzone 2100. Turn rates therefore affect movement only.
- **Auto-targeting.** A ship with no target order fires at the nearest enemy ship in range, or, with none in range, the nearest enemy structure. It keeps that target until the target dies or leaves range.
- **Attack-move** heads for the destination. A ship that meets an enemy stops at its own weapon range and fires. This is the behaviour the Q2 model assumes.
- **Kiting is allowed.** Firing on the move lets a faster ship with a longer gun hold a slower one at bay: Small+Ion+Lance (78 m/s) against Medium+Ion+Mass Driver (52 m/s), for example. The answer is a design at least as fast, and Small+Ion+Mass Driver beats Small+Ion+Lance in every modelled battle. The AI does not kite (§10).
- **Groups move together.** A group ordered as one holds a loose formation at the pace of its slowest ship (§9).

### Damage model (MVP)

Hit points and one **armour** value per ship or structure: `damage taken = max(damage × 0.25, damage − armour)`. Warzone 2100 uses the same shape. It makes heavy weapons matter against heavy hulls without needing damage types.

### How the counters work

There is no tracking, accuracy or damage type (§13). Every counter comes from four things the designer shows: damage per second after armour, shots to kill, range and cost. With the numbers in §12, the model (§3) gives this picture.

**The first minutes are a triangle of three designs.** With the starting components, in every battle the model runs at 2,000–4,500 Ore under both kinds of targeting:

- **The swarm beats the line.** Small+Ion+Mass Driver beats Medium+Ion+Lance every time. The Lance needs three hits for a 198 HP Small hull, so nearly a third of its damage is overkill, and for the same Ore the swarm fields more than twice as many guns.
- **The line beats the brawler.** Medium+Ion+Lance beats Medium+Ion+Mass Driver every time. An armour of 8 takes a Mass Driver hit from 14 down to 6, and it takes a Lance hit only from 95 to 87.
- **The brawler beats the swarm.** Medium+Ion+Mass Driver beats Small+Ion+Mass Driver every time, for the same reason: the swarm's small hits break on the brawler's armour.

All three are worth building, each at about a third of the equilibrium mix. Small+Ion+Lance is not worth building yet, because the swarm beats it.

**Research adds the heavy, and the heavy has its own answer.** Large+Fusion+Lance beats the brawler every time, and beats the swarm under spread fire but not under focus fire. Large+Fusion+Mass Driver beats the swarm every time. Both fall to the Small+Ion+Lance **picket** every time: the Lance loses little to armour, and the Ore buys more than three Lances for each gun the heavy carries. The picket falls to the swarm. At every budget of the full set, the designs worth building are the three starting designs, the picket and one heavy: Large+Fusion+Lance under spread fire, Large+Fusion+Mass Driver under focus fire. Each takes between 11% and 26% of the mix.

**Every hull, drive and weapon has a use, but the Fusion Drive only on the Large hull.** That is the trade the drive is meant to be (§7 Components), and speed, the other half of it, is judged in play.

The Missile Rack is meant to break defended positions and clumps. Its hit also hits every other enemy whose center is within 30 m of its target's, as hard as the target (ADR-014). With the hulls' sizes (§11, gate G5), a formed group of Small hulls stands close enough for a missile to reach four neighbours, and Medium and Large ones are pressed that close only while they fight. Where it stands against the check is in §12.

The other hull, drive and weapon combinations are legal but not worth building at these numbers. That is expected with twelve combinations: Q2 asks only that every component has a use.

---

## 8. Research

A **Research Lab** researches one topic at a time, and a player can have **one** lab. Topics cost Ore and time (§12), and some require another topic first. With one lab the order is the decision: the whole tree takes 11½ minutes of research, so a player who starts at once finishes around minute 12–13, and every topic taken early is another taken late.

The eight topics, what each requires, what it does, and its Ore and time are in the `research` list of [`OutpostCommander/Assets/Tuning.json`](../OutpostCommander/Assets/Tuning.json) (§12, ADR-008). Four upgrade a rate: Mining Rig income, the HP of all hulls, the Mass Driver's and the Lance's fire rates, and Shipyard build speed. Three unlock a component: the Fusion Drive, the Large hull and the Missile Rack. Each unlock, and Automated Shipyards, requires one other topic first.

Upgrades apply at once to every existing ship and structure, as in Warzone 2100.

**How the lab works** (owner, 2026-10-01).
- A lab queues up to five topics, as a Shipyard queues ships. Each is paid for when it starts, and waits at the front of the queue for its Ore.
- A topic may follow its prerequisite in the queue.
- A lab destroyed mid-topic loses that topic and its Ore. Nothing is refunded (§5), and finished topics stay.
- Hull Plating keeps a damaged ship's share of its hit points, so an undamaged ship gains the whole upgrade.
- The details are in ADR-017.

**Upgrades change rates, never the size of a hit.** A weapon upgrade raises its fire rate, not its damage per hit, and no upgrade extends a range. Damage per hit is where armour and the shots-to-kill breakpoints act. At the numbers in §12, +15% Mass Driver damage would be +35% against a Medium hull's armour of 8, and +15% Lance damage would kill a Small hull in two hits instead of three. A fire-rate upgrade adds the same share against every target. Range stays fixed so that the range ladder (§6) holds all match.

`Tools/BattleModel.py` reads the topics from the same file for the one-sided research test (§3).

---

## 9. Control and the server model

The server is **authoritative**. In the MVP the server runs **inside the client process**. The code is shaped for a dedicated server from the first line. The engineering decision is in `Design/ADR/ADR-002-authoritative-server.md`. The consequences for design are:

- **Players issue commands, not changes.** Move, attack, attack-move, stop, build structure, queue ship, start research, save design, and repair (added with milestone 4's structures). There is no cancel (§5). The server validates each one (ownership, cost, legality) and may reject it.
- **The AI is a client.** It sees the snapshot its player is allowed to see and sends the same commands a human does. It cannot cheat by reading server state, and the build enforces that: it can include only the protocol headers (ADR-002).
- **What a player sees is a per-player snapshot.** There is no fog in the MVP, but snapshots are addressed per player so that fog of war can be added on the server alone.
- **The simulation ticks at a fixed rate**, independent of the frame rate. The client interpolates between snapshots for smooth rendering.
- **The network comes after the MVP, over QUIC.** When the server moves to its own process, commands and snapshots cross the network over QUIC (ADR-004). In the MVP they stay in-process, and Q5 measures that. Network play will need Windows 11 or Windows Server 2022.

### Player controls

- Left-click select. Drag for box select. Shift adds. Double-click selects all visible of that design. Until designs exist (§7), "that design" means that hull.
- Right-click: move, or attack if the target is an enemy. `A` + click: attack-move. `S`: stop.
- Ctrl+0–9 assigns control groups. 0–9 recalls them, and a double tap centres the camera on the group.
- Constructors: a build menu, placing a ghost structure, and right-clicking a damaged friendly to repair.
- Movement: ships path around asteroid obstacles and hold a loose formation, at the pace of the slowest ship, when moved as a group. Ships avoid overlapping but do not collide physically.

### UI (drawn by the game over the D3D12 view)

The game draws its own UI (ADR-001): text and panels as quads from one DirectWrite glyph atlas, and input focus as a rectangle test (ADR-015, gate G7). The renderer (ADR-006) fixes the layout: 1920×1080 reference units scaled to the screen.

- **HUD:** Ore stockpile and income, the selection panel, build and research queues, and a minimap.
- **Ship designer:** part of the Shipyard panel rather than a screen of its own: a picker for each slot, live stats, cost and build time, save/rename, and queue, drawn as an in-game panel. The stats are damage per second after armour against each hull, both per ship and per 100 Ore, because Ore is what a counter is bought with: per ship the Lance out-damages the Mass Driver against every hull, and per Ore it does not against light ones. It pauses nothing, because the match keeps running as in Warzone 2100. Every match starts with the four starting designs saved (§7), so the designer is first needed when research unlocks a component. A saved design is renamed, never changed: other components are a new design, so the ships already built stay what they are. A name is up to 32 characters of the HUD's font (ADR-017).
- **Menu:** Start skirmish, Quit. Nothing else.

---

## 10. The opponent

The opponent is **The Tarkan High Command** (§1). It is one scripted AI, deliberately simple. It exists to test the loop, not to be clever:

1. Builds a Shipyard, a Research Lab and Mining Rigs on its home asteroids, then expands to the contested middle.
2. Researches in a fixed order.
3. Picks designs from a small list, favouring whatever counters the player's most common **design**. It reads the whole design, not the hull, because a counter depends on the weapon as much as the hull: Small+Ion+Mass Driver beats Medium+Ion+Lance and loses to Medium+Ion+Mass Driver. It looks at the player's fleet again only once every review interval, 60 s, so it answers a switch late, as a player would, rather than at once from a map it sees in full.
4. Gathers an attack group. When the group reaches 12 ships (gate G9), it attack-moves on the nearest player structure, and repeats.
5. Defends: when any of its structures is attacked, its ships outside the attack group go to it.
6. Rebuilds destroyed Mining Rigs, and replaces lost Constructors at its Command Station.

A difficulty setting is out of scope. One AI tuned to "beatable by a careful player" is enough. It does not kite (§7).

**Its numbers are its own** (owner, 2026-10-01). They are in `OutpostCommander/Assets/Opponent.json`, not with the match's rules in §12's file, because they are how one player plays (ADR-020):

- It keeps 4 Constructors.
- It builds rigs on its 3 home asteroids, then a Shipyard, a Research Lab and a Defence Platform at home, then rigs on the 3 contested asteroids nearest it, each with a platform beside it. It builds a Shipyard for each 10 Ore/s of income: the N-th once its income reaches N × 10 Ore/s, so that its Shipyards spend about what its rigs earn.
- It researches economy first and then the heavies: Improved Extraction, Hull Plating, Fusion Drive, Large Hull, Mass Driver Calibration, Lance Focusing, Automated Shipyards, Missile Rack.
- Its counters are §7's triangle. It answers the swarm (Small+Ion+Mass Driver) with the brawler (Medium+Ion+Mass Driver), the brawler with the line (Medium+Ion+Lance), and the line and the picket (Small+Ion+Lance) with the swarm. It answers each heavy (Large+Fusion) with the picket. Once it has the Large hull and the Fusion drive, it answers the swarm with Large+Fusion+Mass Driver and the brawler with Large+Fusion+Lance instead (§7). Anything else, and no fleet yet, gets the brawler. It builds only what it has unlocked.

---

## 11. Art and presentation

- **Meshes:** two ship sets with the same fourteen models, one per side, plus the asteroid. The **Human** set is the player's Terrakin, and the **Tarkan** set is the AI's (§1, §10). They are **placeholders, to be replaced** (owner, 2026-10-01). Their sources are glTF files in `Art/Models/Human`, `Art/Models/Tarkan` and `Art/Models/Asteroids`, edited in Blender. `Tools/BakeMeshes.py` bakes each into an `.nmf` file in `OutpostCommander/Assets/Models/Human/`, `.../Tarkan/` and `.../Asteroids/`, which the executable packages under `Assets\Models\` (ADR-018). Hull meshes map to hull components, and the other meshes are placeholders (§6, §7). Team colour still marks the side, so the two sets are told apart by shape and colour.
- **The meshes do not share a scale or orientation.** In the Tarkan set, measured extents range from 1.8 units (`Mine`) to 921 units (`Small`), `Medium` is larger than `Large`, and `Colonizer` points along z while the hulls point along x. The Human set is more regular: every model points along z, and the hulls grow in order, `Small` 445, `Medium` 597, `Large` 1,088, `VeryLarge` 1,493 and `Huge` 3,407 units long (measured from the original `.obj` sources on 2026-09-30). The two sets share no scale with each other either. Since 2026-10-01 every source faces the same way, and each model's **length is recorded in data**, in `OutpostCommander/Assets/Models.json`, applied when the mesh is loaded (ADR-011, ADR-018). Up is y on every Tarkan model: each hull is mirror-symmetric across z (checked on 2026-09-30 by reflecting its vertices), so a scale and a forward axis are enough and no roll correction is needed. The Human set has not been checked for roll yet.
- **From the RTS camera the Tarkan hulls are needles.** Seen from above, `Small` is 7.5 times longer than it is wide, `Medium` 7.8 times and `Large` 13.6 times. The Human hulls are stubbier: 3.2, 2.2 and 2.7 times. Their bulk is in height, which a top-down view hides. A circle sized to a hull's length wastes most of its area, and one sized to its width lets ships overlap on screen. So the footprint radius is chosen for movement and formation, not read off the mesh. The owner set the hulls' sizes on 2026-10-01 (gate G5), as the radii they had moved with since milestone 2: Small 8 m, Medium 14 m, Large 24 m. A group forms up three radii apart (ADR-010), which is also what decides how many ships a Missile Rack's splash reaches (§7).
- **Materials are missing.** The converted meshes carry no materials the game uses, and there are no textures. The MVP shades with flat lighting and a **team colour**. Materials are post-MVP.
- **Hardpoints:** a mesh marks where things attach to it, as empties in its source (ADR-018). A shot leaves from the shooter's `gun` nearest its target, and an `exhaust` shows where an engine's exhaust leaves.
- **Effects:** the minimum needed to read combat — muzzle flash, projectile or beam, hit spark and explosion. These are placeholder sprites or simple geometry.
- **Exhaust** (owner, 2026-10-01): every ship's exhaust glows in its drive's color, so that a design's drive reads on sight, and it grows longer and brighter as the ship goes faster. The Constructor, which has no drive, has its own color. The colors are provisional (§15, ADR-019).
- **Audio:** placeholder weapon and explosion sounds at most. Audio is not part of any MVP question.
- **LODs:** there are none. Each set has one mesh per model.
- **Provenance and licence are not recorded.** Nothing in `Art/` says where the placeholder meshes come from or under what terms. They are to be replaced, so this matters only if one of them ships. If one does and it is under a licence, AGENTS.md R14 needs its text to travel with it (§15).

---

## 12. Starting numbers (to be tuned)

These started as first guesses, written down so that tuning has a baseline. They are data, not code constants, and they live in [`OutpostCommander/Assets/Tuning.json`](../OutpostCommander/Assets/Tuning.json): the match rules, the hulls, drives and weapons, the structures and the Defence gun, and the research topics of §8 with their Ore and time. The game loads that file and `Tools/BattleModel.py` reads it, so the two cannot disagree (ADR-008). This section keeps no copy of the numbers. It keeps why they are what they are, and where they stand against the Q2 check.

The research times add up to 690 s. The structure numbers, the Defence gun and the research costs are first guesses that the model does not check (§15); the simulation's hand checks below test the structure numbers against §6's intents.

**Tuned on 2026-09-30, in two passes.** The first pass, after the first review, moved four numbers against the first version of the Q2 check. The second, after the second review, took the range ladder and the research rules as decided (§15) and tuned against the extended check: the starting stage, check (d) and budgets up to 12,000 Ore. It started from a search over fourteen hull, drive and weapon numbers. A review of the model then found that its win rates were measuring where each budget cut a design's ship count, and that 60 unstratified battles could not tell 53% from 48%. So the model now spreads its budgets evenly, fields the leftover Ore as a fractional ship, and takes its verdicts on 95% confidence intervals (§3), and §12 was retuned against that. Every change was then reverted one at a time, and those that were not needed went back: the Large hull returned to 300. Each reason below is what the check reports when that one number goes back, with everything else as it is now. Every number that has moved from the first guesses, with the value that pass chose (the file is what the game plays, if the two ever differ):

| Number | First guess | Now | Why |
|---|---|---|---|
| Lance range | 280 m | 220 m | The range ladder (§6): the Defence gun has to outrange it and the default view has to fit the fight. |
| Missile Rack range | 200 m | 280 m | The range ladder: the one weapon that outranges a platform. |
| Mass Driver Calibration | damage +20% | fire rate +15% | Upgrades change rates, never the size of a hit (§8). At +20% fire rate, one side's upgraded Medium+Ion+Mass Driver has no starting answer that wins half its battles under focus fire (10–47%). |
| Lance Focusing | range +15% | fire rate +15% | A range upgrade would break the range ladder. |
| Small hull HP | 200 | 220 | With Ion it had 180 HP against two Lance hits of 176. At 200, Medium+Ion+Lance has no counter that wins four battles in five at 2,000–3,000 Ore (25–48%). |
| Small hull cost | 50 | 32 | At 35, one-sided Lance Focusing leaves the upgraded Medium+Ion+Lance without an answer that wins half its battles (25–32%). At 45, Medium+Ion+Lance has no counter at 2,000–3,000 Ore under spread fire (25–27%). |
| Medium hull armour | 6 | 8 | At 6, Medium+Ion+Mass Driver was the only starting design worth building. Going back to 6 now leaves the swarm and the brawler without a counter in the starting stage (0–68%). |
| Medium hull cost | 120 | 110 | At 120, one-sided Hull Plating or Mass Driver Calibration leaves the upgraded Small+Ion+Mass Driver without an answer (0–8% under focus fire). |
| Mass Driver damage | 12 | 14 | Against armour 8 a hit of 13 does only 5. At 13, one-sided Lance Focusing leaves the upgraded Medium+Ion+Lance without an answer (17–18%). |
| Mass Driver cost | 30 | 35 | At 30, one-sided Mass Driver Calibration leaves the upgraded Medium+Ion+Mass Driver without an answer (2–15% under focus fire). |
| Lance damage | 90 | 95 | At 90, one-sided Hull Plating leaves the upgraded Medium+Ion+Mass Driver without an answer (12–42% under focus fire). |
| Lance cost | 90 | 85 | At 90, one-sided Mass Driver Calibration leaves the upgraded Medium+Ion+Mass Driver without an answer (8–41% under focus fire). |

**Where §12 stood against the model's Q2 check before the Missile Rack** (`python Tools/BattleModel.py`: 60 battles per pairing, re-run with 480 where a verdict is in doubt; 30 per robustness case; 2,016 robustness checks). How both checks stand with it is below, under *With the Missile Rack*:

- **(a) passes** at every budget of both stages, in both modes. Every design's best counter wins all 60 of its battles.
- **(b) passed for every modelled component.** It was reported as incomplete while the Missile Rack was not modelled.
- **(c) passes:** no counter flips when any single number moves 5%.
- **(d) passes.** The closest case is one-sided Mass Driver Calibration: Medium+Ion+Lance still beats the upgraded Medium+Ion+Mass Driver in 80% of battles at 3,000 Ore under focus fire. Every other topic leaves an answer that wins at least 92%.

With the leftover Ore fielded, the model is close to deterministic: nearly every pairing is won by the same side at every budget in the window. Two margins are still thin and are the first place to look if a later change fails the check. A Mass Driver hit against a Medium hull is 6, so the brawler's matchups move sharply with Mass Driver damage or Medium armour. And a Small+Ion hull sits 6% above the Lance's two-hit breakpoint.

**Where §12 stands against the simulation** (task 3.4, 2026-10-01). The same check now runs as headless battles in `GameLogicTests` (`Q2CheckTests.TheFullCheck`), against the real simulation: the same stages, budgets, fire modes, criteria and confidence intervals. From here the simulation is right where the two disagree (§3). It differs from the model in what the simulation is. Armies are whole ships: battle *k* of *n* spends the Ore at the center of the *k*-th of *n* equal slices of the ±15% window, and the Ore left over is not fielded. And each side is a grid of real ships with real footprints, which close by attack-move, stand at their own range and fire, so a ship reaches only the enemies within its range, where the model's clump puts every ship in range of every enemy. Spread fire picks a random enemy in range, focus fire the weakest in range. The 20 Hz tick, the cold-weapon rule and the stand-off rule are ADR-014's and ADR-010's. **§12 fails all four criteria there, where it passes all four in the model:**

- **(a) fails at one point.** At 2,000 Ore in the starting stage under spread fire, the line does not beat the brawler: Medium+Ion+Lance wins 53% against Medium+Ion+Mass Driver. Everywhere else each design's best counter wins at least 84% of its battles, nearly all of them 100%.
- **(b) fails throughout spread fire.** Under spread fire the Large hull and the Fusion drive are never worth building, at any budget. The mix is only the brawler and the line at 2,000 Ore, in both stages; the swarm, the brawler and the line at 3,000–6,000; and the swarm, the brawler and the picket at 9,000–12,000. Under focus fire the Medium hull drops out of the mix at 3,000–6,000 Ore, where the heavy Large+Fusion+Lance and the picket take its place, and the Large hull and the Fusion drive drop out again at 9,000–12,000.
- **(c) fails on that one matchup.** The line against the brawler at 2,000 Ore under spread fire flips with eight different single 5% changes: Medium armor, Medium speed, the Ion drive's speed factor, the Mass Driver's damage, interval and cost, and the Lance's interval and cost.
- **(d) fails on Mass Driver Calibration** under spread fire. One side's upgraded brawler is beaten by the line in only 10% of battles at 2,000 Ore and 32% at 3,000. One side's upgraded swarm is beaten by the brawler in only 37–44% at 2,000–4,500 Ore. Hull Plating and Lance Focusing sit on the edge, at 51% for the line against the brawler.

**Why the two disagree.** The counters the model found hold in the simulation where focus fire decides them, and `Q2CheckTests.TheRecordedCountersHold` keeps the ones that hold under both modes in CI: the swarm beats the line, the brawler beats the swarm, and the picket beats the heavy Large+Fusion+Mass Driver, each at 2,000 Ore; the line beats the brawler under focus fire only. Spread fire is where they part. The simulation's spread fire is spread over the enemies in range, not over the whole enemy army, so a short-range weapon spreads its hits over the few ships at the front of the fight and works much like focus fire, while a long-range weapon spreads over many. That moves exactly the matchups that fail: the line, whose Lance reaches deep and wastes its heavy hits on spread targets, against the brawler, whose Mass Driver does not. This is reasoned from the battles, not isolated one cause at a time, and why the Large hull and the Fusion drive drop out of the spread-fire mix is not traced yet.

**With the Missile Rack** (task 5.3, 2026-10-01). Both checks now field all eighteen designs. The model gives each clump a square grid three footprint radii apart, the game's formation spacing (ADR-010), and a missile hits every ship of the target's clump within 30 m of it. The simulation plays ADR-014's splash rule. The figures come from the Linux container, where the model took about two minutes and the simulation's full check about four, each on four cores.

- **The model** (2,150 robustness checks) passes (a), (c) and (d). It fails (b) throughout spread fire: at no budget from 2,000 to 12,000 Ore is a design worth building that uses the Missile Rack. Under focus fire Large+Fusion+Missile Rack takes 11% of the mix at every budget. In a clump at formation spacing a missile reaches four neighbours only among Small hulls, and the Missile Rack's 40 damage every 2 s gives less damage per second than the Lance against every hull, and less than the Mass Driver against Small and Medium hulls.
- **The simulation** fails all four, and the Missile Rack is strong there:
  - **(a)** fails at the one point it failed before: the line wins 53% against the brawler at 2,000 Ore under spread fire.
  - **(b)** changes. A Missile Rack design is worth building at every budget, in both modes, and Large+Fusion+Missile Rack takes 14–34% of the mix at 11 of the 12 budgets and modes. What drops out is the Medium hull, under focus fire at 3,000–12,000 Ore and under spread fire at 6,000–12,000. The Mass Driver drops out under focus fire at 3,000–12,000 Ore. The Fusion drive drops out at 12,000 Ore under spread fire, and the Small hull from the starting stage at 2,000 Ore under spread fire.
  - **(c)** fails on the same matchup as before: the line against the brawler at 2,000 Ore under spread fire.
  - **(d)** fails as before on one-sided Mass Driver Calibration. It now also fails on the Missile Rack, which requires Mass Driver Calibration. Taken by one side only, it is a trump card. At 3,000–4,500 Ore no starting design wins a single battle out of 60 against the upgraded Small+Ion+Missile Rack or Medium+Ion+Missile Rack, in either mode. At 2,000 Ore the best answer to the upgraded Medium+Ion+Missile Rack wins 35% under spread fire and 2% under focus fire.
- **Why the two disagree** is reasoned from the battles, not isolated one cause at a time. In the simulation, ships standing to fire give way only sideways round their targets (ADR-010) and press together to their footprints, 16 m apart for Small hulls and 28 m for Medium ones. So a missile there reaches Medium hulls that the model's formation spacing keeps out of reach. And the starting designs reach 120 or 220 m against the Missile Rack's 280 m, so they take fire all the way in.
- **Q2 stays "no"** in the simulation, now with every component in it, and (b) fails in the model too. The Missile Rack is the first number to look at, and retuning is the owner's call (§15).

**What is open** (§15): whether §12 is retuned against the simulation, and whether the model's clump learns the simulation's geometry so that it stays a fast guide to tuning. The full check took about ten minutes on four threads in a Linux container, so it runs only when the `OUTPOST_Q2_FULL` environment variable is set, which CI never does, and the owner runs it: `set OUTPOST_Q2_FULL=1`, then `vstest.console.exe x64\Release\GameLogicTests.dll`. The report goes to `Q2Check-report.txt` in the temporary folder.

**The base's numbers** (milestone 4).

- **The Constructor and its rates are the owner's baseline** (gate G8, 2026-10-01): 300 hit points, armour 2, 45 m/s, 60 Ore and 15 s. A second Constructor on a site adds half of one, so two build in two thirds of the time and three in half. Each repairs 2% of the target's hit points a second, for nothing. The reasons are the owner's to record when the baseline is reviewed.
- **Footprint radii are final** (owner, 2026-10-01), as they were set with milestone 4: the Command Station 45 m, the Shipyard 40, the Research Lab 30, the Mining Rig 25 and the Defence Platform 20, and the Constructor 10, with a 150°/s turn rate. They follow the hulls' sizes, which gate G5 set the same day. A rig's footprint covers its asteroid, so it is reached from the asteroid's edge.

**The hand checks** (task 4.6, `HandCheckTests`): the raid gathers 400 m off, out of the gun's reach, and attacks; the run ends when one side is gone. Measured on 2026-10-01 in the Linux container the other figures in this section come from; the simulation is deterministic, so the figures are the game's.

| Scenario | Intent (§6) | Result |
|---|---|---|
| A lone Defence Platform against five Small+Ion+Mass Driver ships | the platform outlasts the raid and destroys it | **met:** the raid is destroyed in 41.7 s; the platform keeps 420 of 1,500 hit points |
| The Command Station against seven | its gun destroys all seven in under a minute | **met:** in 57.6 s, keeping 2,952 of 5,000 hit points |
| A Command Station without its gun or armour against seven | it falls in about 20 s, which is why it is armed | it falls in 24.9 s, approach included |

Two of the margins are thin. The platform keeps 28% of its hit points, so a sixth raider, or one more Mass Driver damage, may tip it. And the station's minute is 2.4 s from failing. They are left as they are until the owner's playtests show whether raids play out as §6 intends (owner, 2026-10-01). The retune of 2026-10-01 moved no number they depend on.

**Turn rates** for hulls and the drive multiplier on them only affect movement, because hits are instant. They are final as they are in the data file (owner, 2026-10-01). The AI's numbers, its attack-group threshold among them, are not here but in its own file (§10).

---

## 13. Out of scope for the MVP

Campaign; multiplayer over a network; save and load; fog of war (first after the MVP); other races; carriers, fighters and VTOL-style rearming; commanders; sensors and artillery; transports and haulers; asteroid depletion; Huge and VeryLarge hulls; structure upgrades; damage types; unit veterancy; textures and materials; music; options, key rebinding and a real menu; AI difficulty levels.

Something from this list goes into the MVP only if an MVP question cannot be answered without it.

---

## 14. Milestones

Each milestone is playable or visible on screen, and each is **run**, not just built.

1. **A ship on screen.** A Win32 window with a D3D12 flip-model swap chain, a mesh loaded with its scale fixed, and the camera working.
2. **Ships that obey.** The in-process server ticking, selection, move commands, pathing around asteroids, and interpolated rendering. **This answers Q5, including the order-to-response delay, and the tick-time half of Q4.**
3. **Ships that fight.** Weapons, damage and destruction, with designs as data from §12 (the designer UI waits for milestone 5). A 200-ship stress scene under a representative HUD, and the Q2 check as scripted headless battles in `GameLogicTests`. **This answers Q2 and Q4.**
4. **A base.** Constructors built at the Command Station, structures with the Defence gun, Ore, and the Shipyard queue.
5. **Designs and research.** The designer in the Shipyard panel, components and the research tree. The Missile Rack's splash follows once ship sizes are set (§15), split off by the owner on 2026-10-01.
6. **An opponent.** The AI player and the win/lose condition. **This answers Q1 and Q3.**

Q2 moved from milestone 6 to milestone 3 in the first review. It is the design question most likely to change §7 and §12, and it needs no UI to answer.

---

## 15. Open questions

- Team colours for the player and the Tarkan. Until they are decided, the player is blue and the Tarkan orange-red (ADR-011). The exhaust colours are provisional too: Ion cyan, Fusion magenta (ADR-019).
- How far can the camera zoom in and out from the 500 m default view (§4)? Warzone 2100 limits it hard. Sins of a Solar Empire goes to a strategic view.
- What replaces the placeholder meshes, and when (§11). Their provenance matters only if one of them ships.
- Q4 in combat is not measured yet: the owner's run of `--measure --stress` on the development machine, in Release (§3).

Decided on 2026-09-30, first review:

- Constructors are built at the Command Station only (§6).
- Counters come from stats alone: instant hits, no tracking, and §12 tuned against the Q2 check (§7, §12).
- Q2 is answered at milestone 3, and `Tools/BattleModel.py` checks §12 until then (§3, §14).
- The app stays MSIX-packaged for day-to-day development (ADR-001).

Decided on 2026-09-30, second review:

- The default view is about 500 m wide, and the ranges form a ladder: Mass Driver 120 m, Lance 220 m, Defence gun 250 m, Missile Rack 280 m. No research extends a range (§4, §6, §8).
- Only the Missile Rack outranges a Defence Platform (§6).
- Contested asteroids yield more than home ones. The Mining Rig stays cheap, and nothing depletes (§5).
- One Research Lab per player, and research lasts into the mid-game (§8). Upgrades change rates, never the size of a hit.
- The Command Station carries the Defence gun (§6).
- Q2(b) is not tightened for drives. What speed is worth is judged in play (§3).
- Weapons are turrets and fire on the move (§7).
- Q1 stays the owner's judgement (§3).
- The Q2 check adds the starting components, one-sided research (d), and budgets up to 12,000 Ore (§3).
- The AI counters designs rather than hulls, reviews the player's fleet at an interval, and defends its rigs (§10).
- The designer lives in the Shipyard panel, and every match starts with the four starting designs saved (§9).
- The executable is a plain Win32 app, still MSIX-packaged, and the game draws its own UI. WinUI 3, XAML and the Windows App SDK are gone (ADR-001).
- Auto-targeting prefers ships over structures (§7).
- The Defence gun and structure armour in §12 are the baseline for milestone 4, the AI reviews the player's fleet every 60 s, and the research tree lasts about 11½ minutes (§12).

Decided on 2026-09-30, after the second review:

- QUIC, through MsQuic, is the network transport once the server moves out of process. The MVP keeps the in-process loopback and has no QUIC code (§9, ADR-004).
- PIX event markers name the client's regions in Debug builds only (§3, ADR-005).

Decided on 2026-09-30, once implementation began:

- The renderer: borderless full screen with an Alt+Enter window, vsync, a native back buffer, and the UI laid out in 1920×1080 reference units (ADR-006).
- The numbers are data: `OutpostCommander/Assets/Tuning.json`, read by the game and the model, and §12 keeps no copy (§12, ADR-008).
- The map layout in `OutpostCommander/Assets/Map.json` (§4).
- There is no cancel, so nothing is refunded (§5); repair is a command, added with milestone 4.
- A match replays from its seed and command log on the same build, not across machines (ADR-009).
- Meshes reach the game in DirectX's `.cmo` format, converted by the owner (§11). Their provenance is still open.
- The AI opponent is The Tarkan High Command, using the Tarkan mesh set, with the same rules as the player. The player's Terrakin use the Human mesh set (§1, §10, §11). The converted meshes and the data files live in `OutpostCommander/Assets/` (§11, ADR-008).
- Each model's forward axis and length are data, and the scene is drawn flat-lit in team colors (§11, ADR-011). The camera's zoom is a view width with the pitch following it, and the cursor is held inside the window in full screen (§4, ADR-012).
- The arrow keys pan the camera; A attack-moves and S stops (§4, §9, ADR-013).
- Every player starts with its Command Station and two Constructors (§6, ADR-016). The provisional fleet of four Small and two Medium ships that milestones 2 and 3 used is gone.

Decided on 2026-10-01, milestone 3:

- The in-game UI draws text and panels as quads from one DirectWrite glyph atlas, and input focus is a rectangle test (§9, ADR-015, gate G7).
- A group's order paths once for the group, not once per ship, after task 2.7's order ticks of up to 8.3 ms (§3, ADR-010).
- The provisional starting fleet carries all four starting designs: two Small+Ion+Mass Driver, two Small+Ion+Lance, one Medium+Ion+Mass Driver and one Medium+Ion+Lance per player (ADR-014).
- A ship standing to fire holds its range when its own side pushes it, giving way only sideways round its target (§7, ADR-010).
- The Constructor: 300 HP, armour 2, 45 m/s, 60 Ore, 15 s at the Command Station, no weapon. Building: one Constructor takes the structure's build time, and each further Constructor on the site adds half of one more. Repair: 2% of the structure's or ship's maximum hit points per second per Constructor, free, a provisional baseline, in the tuning data since milestone 4 (§7, §12, gate G8).

Decided on 2026-10-01, milestone 5:

- Milestone 5 is split: research and the designer first, and the Missile Rack's splash once ship sizes are set (§14, §15).
- A Research Lab queues up to five topics, each paid for when it starts. Hull Plating keeps a damaged ship's share of its hit points. A lab destroyed mid-topic loses the topic and its Ore (§8, ADR-017).
- Ship sizes (gate G5): the hulls' footprint radii are final at Small 8 m, Medium 14 m and Large 24 m (§11). The Constructor's size and the structure footprints stay open (§15).
- A Missile Rack's splash hits every other enemy ship and structure whose center is within 30 m of its target's, as hard as the target, after each one's armour, with no falloff and no friendly fire (§7, ADR-014).

Decided on 2026-10-01, meshes and exhaust:

- The current meshes are placeholders, to be replaced (§11).
- Meshes are edited in Blender as glTF sources and baked into the game's own `.nmf` format, which carries hardpoints. A model faces the same way in the game as in Blender, no longer its mirror image, and its front is in its mesh rather than in `Models.json` (§11, ADR-018). This supersedes the `.cmo` files and the forward axes decided on 2026-09-30.
- Exhaust is in the MVP, colored by drive (§11, ADR-019).

Decided on 2026-10-01, milestone 6:

- The AI's attack group is 12 ships (gate G9). Its numbers are in `OutpostCommander/Assets/Opponent.json`, apart from the match's rules (§10, ADR-020).
- The AI researches economy first and then the heavies, counters by §7's triangle, and builds rigs with a platform beside each contested one and a Shipyard for each 10 Ore/s of income (§10). The second Shipyard at 30 Ore/s, decided first, left it unable to spend what it earned.
- The AI defends every structure of its, not only its rigs and platforms (§10).
- Once the AI has the Large hull and the Fusion drive, it answers the swarm and the brawler with the heavies that beat them (§7, §10).
- Every player sees every ship's components, as it sees the ship (§10, ADR-020).
- A match ends when a player loses its Command Station. A banner says Victory, Defeat or Draw with the match's length, and the world runs on until the player goes back to the menu (§6, §9).

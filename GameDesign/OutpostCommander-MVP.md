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

**The Q2 check.** Designs fight in clumps bought with equal Ore, under two targeting extremes: every ship shoots a random enemy (spread fire), or every ship shoots the weakest one (focus fire). Real targeting sits between the two (§7). The battles run at two stages of a match:

- **Every component**, at 2,000, 3,000, 4,500, 6,000, 9,000 and 12,000 Ore a side. The largest is about five minutes of income for a player who holds half the middle (§5): a late-game fleet.
- **The starting components**, those no research topic unlocks (§8), at 2,000, 3,000 and 4,500 Ore: the armies of the first minutes, before the first unlock.

Q2 is "yes" when, at every budget of both stages and in both modes:

- **(a)** every design has a counter that beats it in at least four battles out of five;
- **(b)** the designs worth building, those the equilibrium mix of the win-rate matrix gives 5% or more, use every hull, drive and weapon available at that stage;
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
- **The armed structures are armoured.** The Defence Platform and the Command Station have an armour of 10, which cuts a Mass Driver hit from 14 to 4, so light raiders cannot simply swarm them: by hand estimate a lone platform outlasts a raid of five Small+Ion+Mass Driver ships and destroys it. Every other structure has no armour, so raiding a Mining Rig, Shipyard or Research Lab works.
- **The Command Station is armed** so that a handful of early ships cannot end a match. Unarmed and unarmoured, it would fall to seven Small+Ion+Mass Driver ships — about 610 Ore, ready around 1:30 — in about 20 s. Armed and armoured, it needs about 70 s against them and its gun destroys all seven in under a minute. Both are hand estimates: the model has no structures.

---

## 7. Ships and the designer

Every combat ship is a **design**: **hull + drive + weapon**. The player names a design, saves it, and queues it at a Shipyard. Design stats are derived from the components and shown live in the designer.

The **Constructor** is the one fixed design. It has no weapon, it can build and repair, and it uses the `Colonizer` mesh. Players start with two, and more are built at the Command Station (§6). Its numbers are not set yet (§15).

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

The Missile Rack is meant to break defended positions and clumps. It can only be checked once ship sizes say how many ships its 30 m splash reaches (§15).

The other hull, drive and weapon combinations are legal but not worth building at these numbers. That is expected with twelve combinations: Q2 asks only that every component has a use.

---

## 8. Research

A **Research Lab** researches one topic at a time, and a player can have **one** lab. Topics cost Ore and time (§12), and some require another topic first. With one lab the order is the decision: the whole tree takes 11½ minutes of research, so a player who starts at once finishes around minute 12–13, and every topic taken early is another taken late.

The eight topics, what each requires, what it does, and its Ore and time are in the `research` list of [`OutpostCommander/Assets/Tuning.json`](../OutpostCommander/Assets/Tuning.json) (§12, ADR-008). Four upgrade a rate: Mining Rig income, the HP of all hulls, the Mass Driver's and the Lance's fire rates, and Shipyard build speed. Three unlock a component: the Fusion Drive, the Large hull and the Missile Rack. Each unlock, and Automated Shipyards, requires one other topic first.

Upgrades apply at once to every existing ship and structure, as in Warzone 2100.

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

The game draws its own UI (ADR-001). How it draws text, panels and input focus is gate G7, decided in its own ADR when the HUD is built; the renderer (ADR-006) already fixes the layout: 1920×1080 reference units scaled to the screen.

- **HUD:** Ore stockpile and income, the selection panel, build and research queues, and a minimap.
- **Ship designer:** part of the Shipyard panel rather than a screen of its own: a picker for each slot, live stats, cost and build time, save/rename, and queue, drawn as an in-game panel. The stats are damage per second after armour against each hull, both per ship and per 100 Ore, because Ore is what a counter is bought with: per ship the Lance out-damages the Mass Driver against every hull, and per Ore it does not against light ones. It pauses nothing, because the match keeps running as in Warzone 2100. Every match starts with the four starting designs saved (§7), so the designer is first needed when research unlocks a component.
- **Menu:** Start skirmish, Quit. Nothing else.

---

## 10. The opponent

The opponent is **The Tarkan High Command** (§1). It is one scripted AI, deliberately simple. It exists to test the loop, not to be clever:

1. Builds a Shipyard, a Research Lab and Mining Rigs on its home asteroids, then expands to the contested middle.
2. Researches in a fixed order.
3. Picks designs from a small list, favouring whatever counters the player's most common **design**. It reads the whole design, not the hull, because a counter depends on the weapon as much as the hull: Small+Ion+Mass Driver beats Medium+Ion+Lance and loses to Medium+Ion+Mass Driver. It looks at the player's fleet again only once every review interval (§12), so it answers a switch late, as a player would, rather than at once from a map it sees in full.
4. Gathers an attack group. When the group reaches a size threshold, it attack-moves on the nearest player structure, and repeats.
5. Defends: when one of its Mining Rigs or Defence Platforms is attacked, its ships outside the attack group go to it.
6. Rebuilds destroyed Mining Rigs, and replaces lost Constructors at its Command Station.

A difficulty setting is out of scope. One AI tuned to "beatable by a careful player" is enough. It does not kite (§7).

---

## 11. Art and presentation

- **Meshes:** two ship sets with the same fourteen models, one per side, plus the asteroid. The **Human** set is the player's Terrakin, and the **Tarkan** set is the AI's (§1, §10). Their `.obj` sources are in `Art/Models/Human`, `Art/Models/Tarkan` and `Art/Models/Asteroids`. The owner converts each one to DirectX's `.cmo` format with `Tools/meshconvert.exe`, and the game loads the converted copies from `OutpostCommander/Assets/Models/Human/`, `.../Tarkan/` and `.../Asteroids/`, which the executable packages under `Assets\Models\`. Hull meshes map to hull components, and the other meshes are placeholders (§6, §7). Team colour still marks the side, so the two sets are told apart by shape and colour.
- **The meshes do not share a scale or orientation.** In the Tarkan set, measured extents range from 1.8 units (`Mine`) to 921 units (`Small`), `Medium` is larger than `Large`, and `Colonizer` points along z while the hulls point along x. The Human set is more regular: every model points along z, and the hulls grow in order, `Small` 445, `Medium` 597, `Large` 1,088, `VeryLarge` 1,493 and `Huge` 3,407 units long (measured from the `.obj` sources on 2026-09-30). The two sets share no scale with each other either. Each model of each set has a **forward axis and a length recorded in data**, in `OutpostCommander/Assets/Models.json`, applied when the mesh is loaded (ADR-011). Up is y on every Tarkan model: each hull is mirror-symmetric across z (checked on 2026-09-30 by reflecting its vertices), so a scale and a forward axis are enough and no roll correction is needed. The Human set has not been checked for roll yet.
- **From the RTS camera the Tarkan hulls are needles.** Seen from above, `Small` is 7.5 times longer than it is wide, `Medium` 7.8 times and `Large` 13.6 times. The Human hulls are stubbier: 3.2, 2.2 and 2.7 times. Their bulk is in height, which a top-down view hides. A circle sized to a hull's length wastes most of its area, and one sized to its width lets ships overlap on screen. So the footprint radius is chosen for movement and formation, not read off the mesh, and is set with ship sizes (§15).
- **Materials are missing.** The converted meshes carry no materials the game uses, and there are no textures. The MVP shades with flat lighting and a **team colour**. Materials are post-MVP.
- **Effects:** the minimum needed to read combat — muzzle flash, projectile or beam, hit spark and explosion. These are placeholder sprites or simple geometry.
- **Audio:** placeholder weapon and explosion sounds at most. Audio is not part of any MVP question.
- **LODs:** there are none. Each set has one mesh per model.
- **Provenance and licence are not recorded.** Nothing in `Art/` says where the meshes come from or under what terms. If they are under a licence, AGENTS.md R14 needs its text to travel with them (§15).

---

## 12. Starting numbers (to be tuned)

These started as first guesses, written down so that tuning has a baseline. They are data, not code constants, and they live in [`OutpostCommander/Assets/Tuning.json`](../OutpostCommander/Assets/Tuning.json): the match rules, the hulls, drives and weapons, the structures and the Defence gun, and the research topics of §8 with their Ore and time. The game loads that file and `Tools/BattleModel.py` reads it, so the two cannot disagree (ADR-008). This section keeps no copy of the numbers. It keeps why they are what they are, and where they stand against the Q2 check.

The research times add up to 690 s. The structure numbers, the Defence gun and the research costs are first guesses that the model does not check (§15).

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

**Where §12 stands against the Q2 check** (`python Tools/BattleModel.py`: 60 battles per pairing, re-run with 480 where a verdict is in doubt; 30 per robustness case; 2,016 robustness checks):

- **(a) passes** at every budget of both stages, in both modes. Every design's best counter wins all 60 of its battles.
- **(b) passes for every modelled component.** It is reported as incomplete because the Missile Rack is not modelled, so Q2 cannot be "yes" until it is.
- **(c) passes:** no counter flips when any single number moves 5%.
- **(d) passes.** The closest case is one-sided Mass Driver Calibration: Medium+Ion+Lance still beats the upgraded Medium+Ion+Mass Driver in 80% of battles at 3,000 Ore under focus fire. Every other topic leaves an answer that wins at least 92%.

With the leftover Ore fielded, the model is close to deterministic: nearly every pairing is won by the same side at every budget in the window. Two margins are still thin and are the first place to look if a later change fails the check. A Mass Driver hit against a Medium hull is 6, so the brawler's matchups move sharply with Mass Driver damage or Medium armour. And a Small+Ion hull sits 6% above the Lance's two-hit breakpoint.

**Not set yet** (each is open in §15):

- ship sizes in metres, which give the footprint radius and how many ships the Missile Rack's splash reaches. Movement uses provisional radii from the data file until then (ADR-010);
- turn rates for hulls and the drive multiplier on them, which only affect movement because hits are instant. These are provisional in the data file too;
- the Constructor's HP, speed, cost and build time, and the build and repair rates;
- the AI's attack-group threshold.

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
5. **Designs and research.** The designer in the Shipyard panel, components and the research tree.
6. **An opponent.** The AI player and the win/lose condition. **This answers Q1 and Q3.**

Q2 moved from milestone 6 to milestone 3 in the first review. It is the design question most likely to change §7 and §12, and it needs no UI to answer.

---

## 15. Open questions

- Team colours for the player and the Tarkan. Until they are decided, the player is blue and the Tarkan orange-red (ADR-011).
- How far can the camera zoom in and out from the 500 m default view (§4)? Warzone 2100 limits it hard. Sins of a Solar Empire goes to a strategic view.
- Ship sizes in metres: the footprint radius for movement and formation (§11), and the spacing the Missile Rack's splash depends on (§7). Needed before the Missile Rack can be checked, and until it is, Q2 cannot be "yes". Movement uses provisional radii from the data file meanwhile (ADR-010).
- The Defence gun and structure armour (§6, §12) are first guesses. The model has no structures, so they are checked by hand at milestone 4.
- The AI's attack-group threshold (§10).
- Turn rates for hulls and drives (§7, §12). They affect movement only, and provisional ones are in the data file (ADR-010).
- The Constructor's numbers, and the build and repair rates (§12). Needed by milestone 4.
- Where the meshes in `Art/` come from and under what terms (§11).
- How the in-game UI draws text, panels and input focus over the D3D12 scene (§9). Decided with the first HUD, in its own ADR; the layout frame is ADR-006's.

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
- Until milestone 4 places the Command Station and Constructors, every player starts with a provisional fleet held in the map data: four Small and two Medium ships on the Ion drive (ADR-013).

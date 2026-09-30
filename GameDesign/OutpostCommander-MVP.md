# Outpost Commander — MVP Design

Status: **draft for review** · Owner: Stefan Zwaal · Started 2026-09-29 · Revised 2026-09-30 after the first review

This is the design authority AGENTS.md refers to: it says *what* is built. AGENTS.md says *how* the code is written, and `Design/ADR/` records the engineering decisions taken while building it.

Outpost Commander is a real-time strategy game in space, built on the model of **Warzone 2100**: build a base with constructor units on contested resource points, research new technology, **design your own ships from components**, and use them to destroy the enemy.

The MVP exists to answer one question — **is this game worth building?** — and everything in it serves that. Anything that does not help answer it is out of scope, however cheap it looks.

---

## 1. Premise (proposal — edit freely)

A century ago the jump relays went dark. The colonies on the far side — the **Terrakin**, as the frontier settlers came to call themselves — were left with what they had brought: mining tools, a few shipyards, and the salvage of a fleet that would never go home.

The **Kessler Reach** is an asteroid belt thick with ore and the wrecks of that fleet, and whoever holds it can rebuild. Every faction plants its outposts there. The player is an **Outpost Commander**: land a Command Station, mine the rocks, recover lost technology, and design the ships that will keep what you have taken.

The echo of Warzone 2100 is deliberate — rebuilding after a collapse, with research as *recovery* of what was lost rather than invention — and it gives the MVP a reason for its mechanics without needing a campaign.

For the MVP, the opponent is a rival Terrakin outpost in another colour. Other races are post-MVP.

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
| Q2 | Does ship design matter? | The Q2 check below passes: every design has a counter, every hull, drive and weapon is worth building, and no counter hangs on a single number. |
| Q3 | Does research drive the pacing? | Research choices visibly change what the player builds in the mid-game. |
| Q4 | Is the tech feasible? | 200 ships and 40 structures in combat at 1920×1080 on the development machine, with XAML drawn over the swap chain: 99% of frames take ≤ 16.7 ms (a steady 60 fps), and a simulation tick takes ≤ 5 ms at that load. |
| Q5 | Does the server boundary hold, and what does it cost? | The build enforces it: the client and the AI can include only the protocol headers, so reaching into server state does not compile (ADR-002). The delay from an order to the ship visibly responding is measured on the development machine and is ≤ 150 ms. |

Q4 and Q5 are the engineering risk. Q1–Q3 are the design risk. **A failed answer is still a result.** The MVP is done when all five are answered, not when they are all "yes".

Q4 is measured with XAML on screen because composition is the part of each frame this architecture adds (ADR-001). Q5's 150 ms is ADR-002's 50–100 ms for the tick and interpolation, plus about two frames for input, rendering and composition. Both are targets until the first measurement, and the measurement is what gets recorded.

**The Q2 check.** Designs fight in clumps bought with equal Ore, at 2,000, 3,000, 4,500 and 6,000 Ore a side, under two targeting extremes: every ship shoots a random enemy (spread fire), or every ship shoots the weakest one (focus fire). Real targeting sits between the two. Q2 is "yes" when, at every budget and in both modes:

- **(a)** every design has a counter that beats it in at least four battles out of five;
- **(b)** the designs worth building, those the equilibrium mix of the win-rate matrix gives 5% or more, use every hull, drive and weapon;
- **(c)** none of the counters in (a) against those designs stops winning when any single number in §12 moves by 5%, so no result hangs on a breakpoint.

`Tools/BattleModel.py` runs the check against §12 as written, and §12 is tuned against it until milestone 3. From milestone 3 the same battles run as scripted headless tests in SimulationTests against the real simulation. Where the two disagree, the simulation is right and the model is what gets fixed. Where §12 stands against the check today is recorded in §12.

---

## 4. The battlefield

- **Flat plane, 3D rendering.** Every ship, structure and asteroid sits on the plane (y = 0). Ships turn and move in 2D. The renderer draws full 3D meshes under a tilted perspective camera. There is no altitude in the simulation.
- **One map.** About 2,000 × 2,000 m, roughly 8 × 8 screens at default zoom. Two start positions in opposite corners. About 12 ore asteroids: 3 near each base, 6 contested in the middle. Non-mineable asteroid fields act as obstacles and chokepoints.
- **Obstacles are circles.** Asteroids block movement as circular footprints. There is no terrain, height or line of sight in the MVP.
- **Camera.** Pan (edge scroll, WASD, middle-drag), zoom (wheel, clamped), rotate around the focus point (Q/E). The pitch is fixed and comes from the zoom level.
- **The default zoom shows a whole engagement.** At about 8 × 8 screens, one screen is 250 m wide. That is less than the Lance's 280 m range, so two Lance lines trading at full range are never both in view, which breaks pillar 4. Either the default view widens or the longest range comes down. It is settled with the zoom limit (§15).
- **No fog of war in the MVP.** It is the first feature after the MVP, and the server model is shaped so that it can be added (§9).

---

## 5. Economy

- **One resource: Ore.** It works like Warzone 2100's oil: a **Mining Rig** built on an ore asteroid produces a fixed income into the owner's stockpile. There is no hauling and no depletion.
- Each ore asteroid holds one rig. A rig can be destroyed, and the asteroid is then free to rebuild on.
- Everything costs Ore: structures, ships and research. Costs are paid when the job **starts**, as in Warzone 2100. This keeps the rules simple, and there is no refund on cancel in the MVP.
- Starting stockpile and income rates are tuning values (§12).

---

## 6. Structures

| Structure | Role | Placeholder mesh |
|---|---|---|
| **Command Station** | The base. Builds Constructors, queue of up to 5. Losing it loses the match. Pre-placed at start. | `Station` |
| **Shipyard** | Builds ships from designs. Queue of up to 5. | `Station`, scaled and tinted |
| **Research Lab** | Researches one tech at a time. | `Satellite` |
| **Mining Rig** | Built on an ore asteroid. Produces Ore. | `Mine` |
| **Defence Platform** | Stationary turret with a Mass Driver. | `Mine`, tinted |

- Structures are built by **Constructor** ships. Several constructors on one site build faster, as in Warzone 2100.
- Structures are placed freely on the plane with a circular footprint that must not overlap anything. Mining Rigs snap to an ore asteroid.
- There are no structure upgrades or modules in the MVP.
- **Constructors come only from the Command Station.** It exists for as long as the match does, so a player can always rebuild, and the Shipyard's queue stays free for warships. If Shipyards built them, a player who lost every Shipyard and Constructor could never build again for the rest of the match.
- Structures have no armour value yet (§15). The Missile Rack's role against defended positions depends on it (§7).
- As §12 stands, the Lance (280 m) outranges the Defence Platform's Mass Driver (120 m) by 160 m, so Lance ships destroy platforms without taking fire. A platform meant to hold ground needs the range or the armour to matter. That is decided with the Missile Rack's tuning.

---

## 7. Ships and the designer

Every combat ship is a **design**: **hull + drive + weapon**. The player names a design, saves it, and queues it at a Shipyard. Design stats are derived from the components and shown live in the designer.

The **Constructor** is the one fixed design. It has no weapon, it can build and repair, and it uses the `Colonizer` mesh. Players start with two, and more are built at the Command Station (§6). Its numbers are not set yet (§15).

### Components (MVP set)

**Hulls** set hit points, armour, base speed, size and cost.

| Hull | Mesh | Available | Character |
|---|---|---|---|
| Small | `Small` | start | Cheap, fast to build, fragile |
| Medium | `Medium` | start | The all-rounder |
| Large | `Large` | research | Slow, expensive, very tough |

**Drives** multiply speed, turn rate and hit points. A drive has no mesh, so the hull mesh is used as is.

| Drive | Available | Character |
|---|---|---|
| Ion Drive | start | Fast, turns quickly, lightly protected |
| Fusion Drive | research | Slow, much tougher, costs more |

**Weapons** set damage, rate of fire, range and projectile type. Hits are instant in the simulation: the projectile or beam on screen is presentation (§11), and nothing dodges it.

| Weapon | Available | Character |
|---|---|---|
| Mass Driver | start | Short range, fast fire, cheap. Good against small hulls. |
| Lance | start | Long range, slow, heavy hit. Good against large hulls. |
| Missile Rack | research | Medium range with splash damage. Good against structures and clumps. |

### Damage model (MVP)

Hit points and one **armour** value per ship: `damage taken = max(damage × 0.25, damage − armour)`. Warzone 2100 uses the same shape. It makes heavy weapons matter against heavy hulls without needing damage types.

### How the counters work

There is no tracking, accuracy or damage type (§13). Every counter comes from four things the designer shows: damage per second after armour, shots to kill, range and cost. At equal Ore the numbers in §12 give this triangle (checked with `Tools/BattleModel.py`, §3):

- **The swarm beats the line.** Small+Ion+Mass Driver beats Medium+Ion+Lance in every battle the model runs. The Lance needs three shots for a 198 HP Small hull, so a quarter of its damage is overkill, while the Mass Driver's small hits still get through the line's armour of 6.
- **The line beats the heavy.** Medium+Ion+Lance beats Large+Fusion+Lance in 83–100% of battles. The Lance loses little to armour, and the line fields more than twice as many guns for the same Ore.
- **The heavy beats the swarm, but only under spread fire.** Large+Fusion+Lance cuts every Mass Driver hit to the 25% floor, and wins 98–100% of spread-fire battles from 3,000 Ore up (65% at 2,000). Under focus fire the two trade about evenly. There the swarm's dependable counter is Medium+Ion+Mass Driver, which beats it in every battle because its armour of 6 nearly halves each Mass Driver hit.

The Missile Rack is meant to break defended positions and clumped swarms, which would give the swarm a hard counter under any targeting. It can only be checked once ship sizes say how many ships its 30 m splash reaches (§15). Until then the heavy-versus-swarm leg is the weak one, and §12 cannot fix it alone. Making the heavy stronger against the swarm makes it stronger against the line too, because both legs turn on how much heavy the Ore buys. Only the 25% floor in the damage model separates them.

The other eight hull, drive and weapon combinations are legal but not worth building at these numbers. That is expected with twelve combinations; Q2 asks only that every component has a use.

---

## 8. Research

A **Research Lab** researches one topic at a time. Topics cost Ore and time, and some require another topic first. Eight topics:

| # | Topic | Requires | Effect |
|---|---|---|---|
| 1 | Improved Extraction | — | Mining Rig income +25% |
| 2 | Hull Plating | — | All hulls: HP +15% |
| 3 | Mass Driver Calibration | — | Mass Driver damage +20% |
| 4 | Lance Focusing | — | Lance range +15% |
| 5 | Fusion Drive | 2 | Unlocks the Fusion Drive |
| 6 | Large Hull | 2 | Unlocks the Large hull |
| 7 | Missile Rack | 3 | Unlocks the Missile Rack |
| 8 | Automated Shipyards | 1 | Shipyard build speed +25% |

Upgrades apply at once to every existing ship and structure, as in Warzone 2100.

---

## 9. Control and the server model

The server is **authoritative**. In the MVP the server runs **inside the client process**. The code is shaped for a dedicated server from the first line. The engineering decision is in `Design/ADR/ADR-002-authoritative-server.md`. The consequences for design are:

- **Players issue commands, not changes.** Move, attack, attack-move, stop, build structure, queue ship, start research, save design. The server validates each one (ownership, cost, legality) and may reject it.
- **The AI is a client.** It sees the snapshot its player is allowed to see and sends the same commands a human does. It cannot cheat by reading server state, and the build enforces that: it can include only the protocol headers (ADR-002).
- **What a player sees is a per-player snapshot.** There is no fog in the MVP, but snapshots are addressed per player so that fog of war can be added on the server alone.
- **The simulation ticks at a fixed rate**, independent of the frame rate. The client interpolates between snapshots for smooth rendering.

### Player controls

- Left-click select. Drag for box select. Shift adds. Double-click selects all visible of that design.
- Right-click: move, or attack if the target is an enemy. `A` + click: attack-move. `S`: stop.
- Ctrl+0–9 assigns control groups. 0–9 recalls them, and a double tap centres the camera on the group.
- Constructors: a build menu, placing a ghost structure, and right-clicking a damaged friendly to repair.
- Movement: ships path around asteroid obstacles and hold a loose formation when moved as a group. Ships avoid overlapping but do not collide physically.

### UI (WinUI 3 over the D3D12 view)

- **HUD:** Ore stockpile and income, the selection panel, build and research queues, and a minimap.
- **Ship designer:** a component picker for each slot, live stats, cost and build time, and save/rename. It is a XAML dialog that pauses nothing, because the match keeps running as in Warzone 2100.
- **Menu:** Start skirmish, Quit. Nothing else.

---

## 10. The opponent

One scripted AI, deliberately simple. It exists to test the loop, not to be clever:

1. Builds a Shipyard, a Research Lab and Mining Rigs on its nearest asteroids, then expands to the contested middle.
2. Researches in a fixed order.
3. Picks designs from a small list, favouring whatever counters the player's most common hull.
4. Gathers an attack group. When the group reaches a size threshold, it attack-moves on the nearest player structure, and repeats.
5. Rebuilds destroyed Mining Rigs, and replaces lost Constructors at its Command Station.

A difficulty setting is out of scope. One AI tuned to "beatable by a careful player" is enough.

---

## 11. Art and presentation

- **Meshes:** the Terrakin set in `Art/Models/Race1` plus `Art/Models/Asteroids`. Hull meshes map to hull components, and the other meshes are placeholders (§6, §7).
- **The meshes do not share a scale or orientation.** Measured extents range from 1.8 units (`Mine`) to 921 units (`Small`). `Medium` is larger than `Large`. `Colonizer` points along z while the hulls point along x. Each model needs a **scale and a forward axis recorded in data**, applied when the mesh is loaded or baked. This is the first art-pipeline task. Up is y on every model: each hull is mirror-symmetric across z (checked on 2026-09-30 by reflecting its vertices), so a scale and a forward axis are enough and no roll correction is needed.
- **From the RTS camera the hulls are needles.** Seen from above, `Small` is 7.5 times longer than it is wide, `Medium` 7.8 times and `Large` 13.6 times. Their bulk is in height, which a top-down view hides. A circle sized to a hull's length wastes most of its area, and one sized to its width lets ships overlap on screen. So the footprint radius is chosen for movement and formation, not read off the mesh, and is set with ship sizes (§15).
- **Materials are missing.** The `.obj` files name `.mtl` libraries that are not in the tree, and there are no textures. The MVP shades with flat lighting and a **team colour**. Materials are post-MVP.
- **Effects:** the minimum needed to read combat — muzzle flash, projectile or beam, hit spark and explosion. These are placeholder sprites or simple geometry.
- **Audio:** placeholder weapon and explosion sounds at most. Audio is not part of any MVP question.
- **LODs:** the existing LOD meshes (Carrier, Drone, Fighter, Mine) are not needed for the MVP set. Three of the files are byte-identical copies: `Fighter` and `FighterMk2High`, `Mine` and `MineHigh`, `Drone` and `DroneHigh`.
- **Provenance and licence are not recorded.** Nothing in `Art/` says where the meshes come from or under what terms. If they are under a licence, AGENTS.md R14 needs its text to travel with them (§15).

---

## 12. Starting numbers (to be tuned)

These started as first guesses, written down so that tuning has a baseline. They are data, not code constants, and will be loaded from a data file (engineering detail to follow). Until then this section is the data: `Tools/BattleModel.py` reads the Hull, Drive and Weapon tables by their column headings, so a heading changes together with the tool.

| Item | Value |
|---|---|
| Simulation tick | 20 Hz |
| Starting Ore | 1,000 |
| Mining Rig income | 5 Ore/s |
| Command Station HP | 5,000 |

| Hull | HP | Armour | Speed (m/s) | Cost | Build (s) |
|---|---|---|---|---|---|
| Small | 220 | 2 | 60 | 45 | 10 |
| Medium | 500 | 6 | 40 | 110 | 20 |
| Large | 1,200 | 14 | 25 | 300 | 40 |

| Drive | Speed × | HP × | Cost + |
|---|---|---|---|
| Ion | 1.3 | 0.9 | 20 |
| Fusion | 0.8 | 1.4 | 80 |

| Weapon | Damage | Fire interval (s) | Range (m) | Cost + |
|---|---|---|---|---|
| Mass Driver | 13 | 0.4 | 120 | 30 |
| Lance | 90 | 3.0 | 280 | 90 |
| Missile Rack | 40 (splash 30 m) | 2.0 | 200 | 110 |

| Structure | HP | Cost | Build (constructor-seconds) |
|---|---|---|---|
| Shipyard | 2,500 | 300 | 40 |
| Research Lab | 1,500 | 200 | 30 |
| Mining Rig | 800 | 50 | 10 |
| Defence Platform | 1,500 | 150 | 20 |

Each research topic costs 100–250 Ore and 30–60 s.

**Retuned on 2026-09-30** against the Q2 check (§3). Four numbers changed from the first guesses, and nothing else:

| Number | Was | Now | Why |
|---|---|---|---|
| Small hull HP | 200 | 220 | With Ion it had 180 HP against two Lance hits of 176. At 195 base HP the swarm went from winning 68–100% of its battles against the line to winning 0–2%. It now has 198 HP, 11% clear of the breakpoint. |
| Mass Driver damage | 12 | 13 | Under spread fire the swarm beat the line in only 68–77% of battles. |
| Medium hull cost | 120 | 110 | The line beat the heavy in only 42–88% of battles. The cheaper hull buys more Lances for the Ore. |
| Small hull cost | 50 | 45 | With the other three changes alone, no Small design was worth building under focus fire from 3,000 Ore up, because Medium+Ion+Mass Driver took its place. |

**Where §12 stands against the Q2 check** (`python Tools/BattleModel.py`: 60 battles per pairing, 30 per robustness case):

- **(a) passes.**
- **(c) passes:** no counter flips when any single number moves 5%.
- **(b) passes in part.** It holds under spread fire at every budget and under focus fire at 3,000 and 4,500 Ore. Under focus fire it fails at 2,000 Ore, where no Medium design is worth building, and at 6,000 Ore, where no Small design is.

The first guesses failed all three parts. The Missile Rack is not in the model yet, so Q2 cannot be "yes" until it is.

**Not set yet** (each is open in §15):

- ship sizes in metres, which give the footprint radius and how many ships the Missile Rack's splash reaches;
- structure armour;
- turn rates for hulls and the drive multiplier on them, which only affect movement because hits are instant;
- the Constructor's HP, speed, cost and build time, and the build and repair rates.

---

## 13. Out of scope for the MVP

Campaign; multiplayer over a network; save and load; fog of war (first after the MVP); other races; carriers, fighters and VTOL-style rearming; commanders; sensors and artillery; transports and haulers; Huge and VeryLarge hulls; structure upgrades; damage types; unit veterancy; textures and materials; music; options, key rebinding and a real menu; AI difficulty levels.

Something from this list goes into the MVP only if an MVP question cannot be answered without it.

---

## 14. Milestones

Each milestone is playable or visible on screen, and each is **run**, not just built.

1. **A ship on screen.** The WinUI window with a D3D12 swap chain panel, a mesh loaded with its scale fixed, and the camera working.
2. **Ships that obey.** The in-process server ticking, selection, move commands, pathing around asteroids, and interpolated rendering. **This answers Q5, including the order-to-response delay, and the tick-time half of Q4.**
3. **Ships that fight.** Weapons, damage and destruction, with designs as data from §12 (the designer UI waits for milestone 5). A 200-ship stress scene under a representative XAML overlay, and the Q2 check as scripted headless battles in SimulationTests. **This answers Q2 and Q4.**
4. **A base.** Constructors built at the Command Station, structures, Ore, and the Shipyard queue.
5. **Designs and research.** The designer UI, components and the research tree.
6. **An opponent.** The AI player and the win/lose condition. **This answers Q1 and Q3.**

Q2 moved from milestone 6 to milestone 3 in the 2026-09-30 revision. It is the design question most likely to change §7 and §12, and it needs no UI to answer.

---

## 15. Open questions

- Team colours and faction naming for the AI opponent.
- How far can the camera zoom out? Warzone 2100 limits it hard. Sins of a Solar Empire goes to a strategic view. The default zoom must also fit the longest weapon range (§4).
- Ship sizes in metres: the footprint radius for movement and formation (§11), and the spacing the Missile Rack's splash depends on (§7). Needed before the Missile Rack can be checked.
- Structure armour, and whether the Defence Platform should outrange or outlast a Lance (§6). Decided with the Missile Rack's tuning.
- Turn rates for hulls and drives (§7, §12). They affect movement only.
- The Constructor's numbers, and the build and repair rates (§12). Needed by milestone 4.
- Where the meshes in `Art/` come from and under what terms (§11).

Decided on 2026-09-30:

- Constructors are built at the Command Station only (§6).
- Counters come from stats alone: instant hits, no tracking, and §12 retuned against the Q2 check (§7, §12).
- Q2 is answered at milestone 3, and `Tools/BattleModel.py` checks §12 until then (§3, §14).
- The app stays MSIX-packaged for day-to-day development. ADR-001 records this with the milestone 1 scaffold.

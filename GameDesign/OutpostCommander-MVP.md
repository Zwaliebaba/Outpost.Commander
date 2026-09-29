# Outpost Commander — MVP Design

Status: **draft for review** · Owner: Stefan Zwaal · Started 2026-09-29

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
| Q2 | Does ship design matter? | At least three designs each win a different matchup in scripted test battles. No single design beats all others. |
| Q3 | Does research drive the pacing? | Research choices visibly change what the player builds in the mid-game. |
| Q4 | Is the tech feasible? | 200 ships and 40 structures in combat at a steady 60 fps at 1920×1080 on the development machine. A simulation tick takes ≤ 5 ms at that load. |
| Q5 | Does the server boundary hold? | All state changes go through commands to the in-process server. The AI plays through the same command interface as the human. |

Q4 and Q5 are the engineering risk. Q1–Q3 are the design risk. **A failed answer is still a result.** The MVP is done when all five are answered, not when they are all "yes".

---

## 4. The battlefield

- **Flat plane, 3D rendering.** Every ship, structure and asteroid sits on the plane (y = 0). Ships turn and move in 2D. The renderer draws full 3D meshes under a tilted perspective camera. There is no altitude in the simulation.
- **One map.** About 2,000 × 2,000 m, roughly 8 × 8 screens at default zoom. Two start positions in opposite corners. About 12 ore asteroids: 3 near each base, 6 contested in the middle. Non-mineable asteroid fields act as obstacles and chokepoints.
- **Obstacles are circles.** Asteroids block movement as circular footprints. There is no terrain, height or line of sight in the MVP.
- **Camera.** Pan (edge scroll, WASD, middle-drag), zoom (wheel, clamped), rotate around the focus point (Q/E). The pitch is fixed and comes from the zoom level.
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
| **Command Station** | The base. Losing it loses the match. Pre-placed at start. | `Station` |
| **Shipyard** | Builds ships from designs. Queue of up to 5. | `Station`, scaled and tinted |
| **Research Lab** | Researches one tech at a time. | `Satellite` |
| **Mining Rig** | Built on an ore asteroid. Produces Ore. | `Mine` |
| **Defence Platform** | Stationary turret with a Mass Driver. | `Mine`, tinted |

- Structures are built by **Constructor** ships. Several constructors on one site build faster, as in Warzone 2100.
- Structures are placed freely on the plane with a circular footprint that must not overlap anything. Mining Rigs snap to an ore asteroid.
- There are no structure upgrades or modules in the MVP.

---

## 7. Ships and the designer

Every combat ship is a **design**: **hull + drive + weapon**. The player names a design, saves it, and queues it at a Shipyard. Design stats are derived from the components and shown live in the designer.

The **Constructor** is the one fixed design. It has no weapon, it can build and repair, and it uses the `Colonizer` mesh. Players start with two.

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

**Weapons** set damage, rate of fire, range and projectile type.

| Weapon | Available | Character |
|---|---|---|
| Mass Driver | start | Short range, fast fire, cheap. Good against small hulls. |
| Lance | start | Long range, slow, heavy hit. Good against large hulls. |
| Missile Rack | research | Medium range with splash damage. Good against structures and clumps. |

The intent behind Q2: Small+Ion+Mass Driver swarms beat Lance ships that cannot track them, Large+Fusion+Lance beats swarms of mass drivers, and Missile Racks break defended positions and clumped swarms. Numbers are in §12 and **will** be retuned.

### Damage model (MVP)

Hit points and one **armour** value per ship: `damage taken = max(damage × 0.25, damage − armour)`. Warzone 2100 uses the same shape. It makes heavy weapons matter against heavy hulls without needing damage types.

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
- **The AI is a client.** It sees the snapshot its player is allowed to see and sends the same commands a human does. It cannot cheat by reading server state.
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
5. Rebuilds destroyed Mining Rigs.

A difficulty setting is out of scope. One AI tuned to "beatable by a careful player" is enough.

---

## 11. Art and presentation

- **Meshes:** the Terrakin set in `Art/Models/Race1` plus `Art/Models/Asteroids`. Hull meshes map to hull components, and the other meshes are placeholders (§6, §7).
- **The meshes do not share a scale or orientation.** Measured extents range from 1.8 units (`Mine`) to 921 units (`Small`). `Medium` is larger than `Large`. `Colonizer` points along z while the hulls point along x. Each model needs a **scale and a forward axis recorded in data**, applied when the mesh is loaded or baked. This is the first art-pipeline task.
- **Materials are missing.** The `.obj` files name `.mtl` libraries that are not in the tree, and there are no textures. The MVP shades with flat lighting and a **team colour**. Materials are post-MVP.
- **Effects:** the minimum needed to read combat — muzzle flash, projectile or beam, hit spark and explosion. These are placeholder sprites or simple geometry.
- **Audio:** placeholder weapon and explosion sounds at most. Audio is not part of any MVP question.
- **LODs:** the existing LOD meshes (Carrier, Drone, Fighter, Mine) are not needed for the MVP set.

---

## 12. Starting numbers (to be tuned)

These are first guesses, written down so that tuning has a baseline. They are data, not code constants, and will be loaded from a data file (engineering detail to follow).

| Item | Value |
|---|---|
| Simulation tick | 20 Hz |
| Starting Ore | 1,000 |
| Mining Rig income | 5 Ore/s |
| Command Station HP | 5,000 |

| Hull | HP | Armour | Speed (m/s) | Cost | Build (s) |
|---|---|---|---|---|---|
| Small | 200 | 2 | 60 | 50 | 10 |
| Medium | 500 | 6 | 40 | 120 | 20 |
| Large | 1,200 | 14 | 25 | 300 | 40 |

| Drive | Speed × | HP × | Cost + |
|---|---|---|---|
| Ion | 1.3 | 0.9 | 20 |
| Fusion | 0.8 | 1.4 | 80 |

| Weapon | Damage | Fire interval (s) | Range (m) | Cost + |
|---|---|---|---|---|
| Mass Driver | 12 | 0.4 | 120 | 30 |
| Lance | 90 | 3.0 | 280 | 90 |
| Missile Rack | 40 (splash 30 m) | 2.0 | 200 | 110 |

| Structure | HP | Cost | Build (constructor-seconds) |
|---|---|---|---|
| Shipyard | 2,500 | 300 | 40 |
| Research Lab | 1,500 | 200 | 30 |
| Mining Rig | 800 | 50 | 10 |
| Defence Platform | 1,500 | 150 | 20 |

Each research topic costs 100–250 Ore and 30–60 s.

---

## 13. Out of scope for the MVP

Campaign; multiplayer over a network; save and load; fog of war (first after the MVP); other races; carriers, fighters and VTOL-style rearming; commanders; sensors and artillery; transports and haulers; Huge and VeryLarge hulls; structure upgrades; damage types; unit veterancy; textures and materials; music; options, key rebinding and a real menu; AI difficulty levels.

Something from this list goes into the MVP only if an MVP question cannot be answered without it.

---

## 14. Milestones

Each milestone is playable or visible on screen, and each is **run**, not just built.

1. **A ship on screen.** The WinUI window with a D3D12 swap chain panel, a mesh loaded with its scale fixed, and the camera working.
2. **Ships that obey.** The in-process server ticking, selection, move commands, pathing around asteroids, and interpolated rendering. **This answers Q5 and the first half of Q4.**
3. **Ships that fight.** Weapons, damage and destruction. A 200-ship stress scene. **This answers Q4.**
4. **A base.** Constructors, structures, Ore, and the Shipyard queue.
5. **Designs and research.** The designer UI, components and the research tree.
6. **An opponent.** The AI player and the win/lose condition. **This answers Q1–Q3.**

---

## 15. Open questions

- Team colours and faction naming for the AI opponent.
- Should Constructors be buildable at the Shipyard, at the Command Station, or both?
- How far can the camera zoom out? Warzone 2100 limits it hard. Sins of a Solar Empire goes to a strategic view.
- Packaged (MSIX) or unpackaged app for day-to-day development. See ADR-001.

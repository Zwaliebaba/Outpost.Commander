# Index and authority map

HEAD `a053fe294d91dc885bbb061c2f34d76ca6897738` (2026-10-25T08:33:55+01:00). Routing table for every brief. Tags: rules, units, economy, orders/AI, world/persistence, UI/screens, look/rendering, engine-only. **Agents** = who receives it.

## Authority (AGENTS.md, "What is authoritative")

1. `AGENTS.md` — conformance only (naming, build). Not a source for game rules.
2. `Design/ADR/` — decisions as they stand now (edited in place, AGENTS.md §6).
3. Code and data at HEAD — ground truth for what the player sees and the numbers (`OutpostCommander/Assets/*.json`, `GameApp/`, `GameLogic/`, `Opponent/`).
4. Design documents: MVP → Phase 1 → 2 → 3 → 4 → 5; the later phase wins. They sit beside AGENTS.md, not above.
5. Plans are work queues, not authorities. A plan's "Not in this plan" and `todo` rows mark what is not built.

## Design documents and plans

| File | Subject | Status | In scope | Agents |
|---|---|---|---|---|
| `GameDesign/Archive/OutpostCommander-MVP.md` | MVP design: pitch, pillars, loop, units, base, AI | done | yes | A1 A2 A3 A4 A5 A7 |
| `GameDesign/Archive/OutpostCommander-Phase1.md` | Phase 1: research tiers, depletion, sectors, losing production, longer AI | done | yes | A1 A2 A3 A4 |
| `GameDesign/Archive/OutpostCommander-Phase2.md` | Phase 2: Territory, domination, modules, alerts, standing orders | done | yes | A1 A2 A3 A4 |
| `GameDesign/Archive/OutpostCommander-Phase3.md` | Phase 3: Structures that grow (levels L1–L5), AI difficulty | done | yes | A1 A2 A3 A4 |
| `GameDesign/Archive/OutpostCommander-Phase4.md` | Phase 4: Fleet cap, seeded placement, Pirates, salvage, repair and retreat | done | yes | A1 A2 A3 A4 |
| `GameDesign/OutpostCommander-Phase5.md` | Phase 5: A world that persists — saves, dedicated server, deputy, scheduled orders, world log, Away | accepted; milestones 35–39 merged; owner runs/week to come | yes | A1 A3 A4 |
| `GameDesign/Archive/ImplementationPlan.md` | MVP plan (closed) | closed | work queue only | A1–A7 (status, owner decisions) |
| `GameDesign/Archive/ImplementationPlan-Phase1.md` … `-Phase4.md` | Phase 1–4 plans (closed) | closed | work queue only | as their designs |
| `GameDesign/ImplementationPlan-Phase5.md` | Phase 5 plan, milestones 35–39 | all merged | work queue only | A1 A3 A4 |
| `GameDesign/ImplementationPlan-Interface.md` | First interface plan (owner's review of the screen) | all merged | yes | A4 A5 A7 |
| `GameDesign/ImplementationPlan-Interface2.md` | Second interface plan, UI tasks | all merged | yes | A4 A5 A7 |
| `GameDesign/ImplementationPlan-Look.md` | Lit vector look | LV1.1, LV1.2 merged; LV2.1 merged (PR #108); **LV2.2, LV2.3, LV3.1, LV4.1–4.3 todo — not built** | built tasks only | A5 A6 A7 |
| `GameDesign/OutpostCommander-Horizon.md` | Horizon | decides nothing | **EXCLUDED** | none |
| `GameDesign/ImplementationPlan-Horizon.md` | Horizon plan | — | **EXCLUDED** (cited only where Phase 5 adopts a part by name) | none |
| `GameDesign/ImplementationPlan-SelfPlay.md` | Self-play | side project | **EXCLUDED** | none |
| `Design/SelfPlayNetwork.md` | Self-play network blueprint | side project | **EXCLUDED** | none |
| `GameDesign/Mockups/ShipDesigner.png` | Designer mockup | evidence | yes (mockup, not ground truth) | A4 A5 |

Not built, therefore not drawn: paint slots `team`/`trim`/`light` (LV2.2), two-tone lines (LV2.3), beacons (LV3.1), fleets on the screen and plates over them (LV4.1–4.3), and the ground's sector plates (Look plan "Not in this plan"). `grep -i beacon` over `*.cpp/*.h` at HEAD finds nothing.

## Data files (ground truth)

| File | Holds | Agents |
|---|---|---|
| `OutpostCommander/Assets/Tuning.json` | hulls, drives, weapons, modules, structures and levels, research, costs, economy | A1 A2 A3 A4 |
| `OutpostCommander/Assets/Map.json` | sectors, nodes, adjacency, starts | A1 A7 |
| `OutpostCommander/Assets/Matchups.json` | battle matchups (ADR-083) | A2 |
| `OutpostCommander/Assets/Models.json` | model sets, lengths, side colors (Human, Tarkan, Pirate, Wreck) | A2 A5 A6 A7 |
| `OutpostCommander/Assets/Camera.json` | camera limits | A7 |
| `OutpostCommander/Assets/Opponent.json`, `OpponentEasy.json`, `OpponentHard.json` | AI settings per difficulty (Normal, Easy, Hard) | A3 |
| `OutpostCommander/Assets/Models/<Set>/*.nmf`, `Art/Models/<Set>/*.glb` | baked meshes and their sources | A6 |
| `OutpostCommander/Assets/Textures/*.dds`, `OutpostCommander/Assets/*.png` | particle/glow textures, app logos | A6 |

## ADRs

ADR-021 is a gap (deleted; number not reused). ADR-063 is self-play and excluded.

| ADR | Subject | Tag | Agents |
|---|---|---|---|
| 001 | Plain Win32 shell, MSIX | engine-only | — |
| 002 | Authoritative server, in-process for the MVP | engine-only | A3 (where the server runs) |
| 003 | x64 and ARM64 | engine-only | — |
| 004 | QUIC through MsQuic | engine-only | — |
| 005 | PIX markers | engine-only | — |
| 006 | Renderer shape: borderless full screen, windowed toggle, resolution, scaling | look/rendering | A5 A7 |
| 007 | Vendored d3dx12.h | engine-only | — |
| 008 | Tuning data is JSON | rules | A1 A2 |
| 009 | Deterministic core; tick is the clock | engine-only (tick rate visible) | A1 A3 |
| 010 | Movement and pathing; groups part without colliding | units | A2 |
| 011 | Meshes sized by data, flat-lit in team color | look/rendering | A6 A7 |
| 012 | RTS camera: zoom, pitch from zoom, cursor held | look/rendering | A7 |
| 013 | Client view (interpolated snapshots) and event-driven controls | UI/screens | A4 |
| 014 | Designs as derived stats; hp in hundredths; hits land at tick end | units | A2 |
| 015 | Interface: text and panels from one glyph atlas | UI/screens | A4 A5 |
| 016 | Base: Ore in hundredths, blocking structures, Constructors, queues pay at start | economy | A1 A2 |
| 017 | Research queue; designer derives stats from server numbers | units | A2 A4 |
| 018 | NMF meshes with hardpoints | units | A2 A6 |
| 019 | Exhaust glows in its drive's color | look/rendering | A7 |
| 020 | AI is a client with its own settings; match ends when a player is beaten; menu | orders/AI | A3 A4 |
| 022 | Natural starfield | look/rendering | A7 |
| 023 | Queue saves an unsaved design | units | A2 A4 |
| 024 | Fog of war | look/rendering (rule) | A7 A1 |
| 025 | Server thread | engine-only | — |
| 026 | Explosions and particles | look/rendering | A7 |
| 027 | Rock crease lines | look/rendering | A6 A7 |
| 028 | Vector grid and crosses; colors put fleets first | look/rendering | A7 A5 |
| 029 | Ship banking | look/rendering | A7 A2 |
| 030 | Typography and sprites | UI/screens | A5 A4 |
| 031 | Floating windows | UI/screens | A4 A5 |
| 032 | Order ticks (graph planning) | engine-only | — |
| 033 | Research tiers | units | A2 |
| 034 | Shot looks; Rail Cannon slug | look/rendering | A2 A7 |
| 035 | Ore depletion | economy | A1 |
| 036 | Map sectors, nodes, adjacency | rules | A1 |
| 037 | Losing all production | rules | A1 A3 |
| 038 | Match log (tooling) | engine-only | — |
| 039 | Ships turn in arcs | units | A2 |
| 040 | Lines over dark faces | look/rendering | A6 A7 A5 |
| 041 | AI plays a longer match | orders/AI | A3 |
| 042 | Faint footprint lines | look/rendering | A6 A7 |
| 043 | HUD in the windows' look; Ore written one way | UI/screens | A4 A5 |
| 044 | Rig stands on its legs | look/rendering | A6 A7 |
| 045 | Levels and spinning parts | look/rendering | A6 A2 |
| 046 | Second look at the screen | UI/screens | A4 A7 |
| 047 | A fight seen whole: health bars, no income warning | UI/screens | A4 A7 |
| 048 | Batched static uploads | engine-only | — |
| 049 | Startup in parallel (loading) | engine-only (loading screen) | A4 |
| 050 | Shader resolve | engine-only | — |
| 051 | One shader-visible heap | engine-only | — |
| 052 | Fog texture (ground and minimap) | look/rendering | A7 |
| 053 | Instanced meshes | engine-only | — |
| 054 | Obstacle grid | engine-only | — |
| 055 | Tick timer | engine-only | — |
| 056 | Territory | rules | A1 |
| 057 | Domination | rules | A1 |
| 058 | Modules; Sensor Array | units | A2 |
| 059 | Alerts and standing orders | orders/AI | A3 A4 |
| 060 | QUIC in process | engine-only | — |
| 061 | Measured text | UI/screens | A4 A5 |
| 062 | Type scale and contrast | UI/screens | A5 A4 |
| 063 | Self-play probe | **EXCLUDED** | — |
| 064 | Structure upgrades L1–L5 | units/economy | A2 A1 |
| 065 | AI difficulty | orders/AI | A3 A4 |
| 066 | Production status on the HUD | UI/screens | A4 |
| 067 | Selection ring at a fixed width | UI/screens | A4 A7 |
| 068 | Interface polish: headers, minimap marks, cards, Controls window | UI/screens | A4 |
| 069 | Starting design names | units | A2 |
| 070 | Interface scale | UI/screens | A4 A5 |
| 071 | Fleet cap | economy | A1 |
| 072 | Seeded placement | rules | A1 |
| 073 | Pirates | rules | A1 |
| 074 | Salvage | economy | A1 |
| 075 | Repair and retreat | rules | A1 |
| 076 | AI plays Phase 4 | orders/AI | A3 |
| 077 | World state on disk | world/persistence | A3 |
| 078 | Dedicated server | world/persistence | A3 |
| 079 | Seat controller and deputy | orders/AI | A3 |
| 080 | Events and scheduled orders | orders/AI | A3 |
| 081 | Territory and memory over the fog | look/rendering | A7 |
| 082 | World log and world run | world/persistence | A3 |
| 083 | Battle matchups | units | A2 |
| 084 | World without an end | rules | A1 |
| 085 | One meaning per color | UI/screens | A5 |
| 086 | Where the match runs; server gone quiet | UI/screens | A3 A4 |
| 087 | Fields of their own | look/rendering | A7 |
| 088 | Selection and commands | UI/screens | A3 A4 |
| 089 | Test DLL per library | engine-only | — |
| 090 | Warm key and cool fill | look/rendering | A5 A6 A7 |
| 091 | Phosphor glow; interface never glows | look/rendering | A5 A7 |
| 092 | Baked occlusion | look/rendering | A6 A7 |
| 093 | Failed assertion ends the process | engine-only | — |

# Phase 0 — Design ↔ Implementation Map (ground truth), Outpost Commander, HEAD 67a4ad8 (2026-09-23)

Built by the Lead Reviewer from the docs and the code, not from the designer's summary. Every claim below is
FACT (cited to a doc section or file:line, or measured by the harness in §5) unless marked INFERENCE.

## 0. Context the designer left empty, and what was used instead

The review template's context block was left as placeholders. What the docs say, used in its place:

- Genre / reference: multiplayer-only RTS on a tablet, "in the lineage of Warzone 2100" with Homeworld's feel (README, GameDesign §1).
- Pillars, as inferred from GameDesign §1 and §10 (INFERENCE — the designer did not state them):
  P1 Homeworld feel: "a fleet, not an army", scale, "ships that bank as they turn and read as silhouettes", an economy you must defend.
  P2 Warzone spine: a unit is a composition (hull + drive + slots), every stat derived (R24), research later.
  P3 Touch-only, one plane, one tap = one order (ADR-001, R21).
  P4 A deterministic, replayable, authoritative simulation; the client never simulates (R16, R19).
  P5 "Twenty matches in an evening": a two-player five-minute MVP that turns balance questions into answers (GameDesign §10, M3.11).
- Docs: Design/GameDesign.md, TechnicalDesign.md, Interface.md, OpenQuestions.md (57 questions), ADR-001..024 (no 014), Plan/M0..M4.
- Stack: C++23/MSVC v145, UWP client (D3D12, C++/WinRT), Win32 host, UDP; six libraries (NeuronCore/Client/Server, GameCore/Client/Logic), Server, Bot, OutpostCommander executables.
- Designer's own status (README, AGENTS.md, Plan/README): M0 complete; M1 built, two gates owed; M2 built through M2.12, two closing gates owed; M3 (combat, victory, restart, finite ore, stub AI) and M4 (AI, 4 players, Cruiser, research) not started.
- Target scale: 2 players × (50 ships + 1 station + 4 modules) = 110 entities for the MVP; 4 players = 220; a 100-player stress target set by the owner (ADR-024). 16,384-unit square. Multiplayer only. Platform: Surface Pro 11 (ARM64), Windows 11 22H2+.
- Time budget per week: NOT STATED anywhere. Git history covers 2026-09-22 10:42 to 2026-09-23 20:49 (81 commits, 34 hours) starting at "Close M0.21", so M0.1–M0.20 and all design docs (dated 2026-09-20/21) predate the history. Authors: Stefan Zwaal 47 commits, "Claude" 31, Zwaliebaba 3 merges. INFERENCE: code is written by an agent; the developer's scarce resource is hands-on-hardware verification time, not coding.
- Specific worries: none stated.

## 1. Where the designer's view and the code disagree — flagged first

D1  FACT. **Every hosted match gives player 1 a free Fighter at the map centre.** `Server/Server.cpp:146-149` (`RunHost`) creates `DesignId::Fighter` owned by player 1 at the origin and orders it to (4096, 2048) after `BeginMatch`. The comment calls it "the whole of the simulation at M0". It survived M1 and M2; the M1.15 hardware run recorded "both drew three entities" (Plan/M1-the-fleet.md:809) without noticing. `HostTests` asserts stations-only on `Host`, not on `Server`. R20 forbids game logic in `Server`. GameDesign §3's "every player's start is the same start" is false on the real host.

D2  FACT. **Ships never turn.** No code writes `Entity::heading` after `World::Create` (grep over GameLogic/GameCore: only Create/spawn set it). `Tick.cpp` moves position only. The client draws the wire heading as sent (`GameClient/Interpolation.cpp:90`, `OutpostCommander/App.cpp:1227`) and derives no facing from motion. GameDesign §6 "Turn rate derives the same way" names a stat that exists nowhere (no turn field in `Catalog.h`), §1's pillar "ships that bank as they turn and read as silhouettes" has no mechanism, and ADR-004/M3.2 "range and arc are checked at the fire tick" (Plan/M3-the-fight.md:89) needs a facing that never changes. No M3 or M4 step adds turning.

D3  FACT (measured, §5). **The economy's numbers are off by 2.5× and the bottleneck is inverted.** GameDesign §4: "a round trip to the home field is roughly thirty seconds, which puts one miner at about 2.5 credits per second"; Q47: build rate 20 cr/s "sits just above the income a running economy earns". With the shipped generator (home rocks 600–1,500 from the anchor, `Generator.h`) and the shipped loop, one Miner on the nearest rock of seed 20260922 earns **6.40 cr/s** (round trip ≈ 15.6 s); typical home rocks 4.0–4.7. Four to five miners saturate the 20 cr/s build slot; the opening simulation banks 30,165 unspent credits by minute five. The inner-radius 600 was added by Q26 on 2026-09-23 without re-deriving §4.

D4  FACT. **"Contested fields are richer" has no number behind it.** Contested clusters hold 6 rocks against a home field's 10 (`Generator.h`), no ore quantity is specified for either (Q26 recommends 200 ore per home rock only, "provisional"), and measured income from a contested rock is 0.67–1.33 cr/s against 4.0–6.4 at home. `Generator.h:54` says "richer ... is M3's to make true". GameDesign §3 calls it "the map's only real proposition".

D5  FACT. **"Ten rocks keeps [six miners] from queuing on one" (`Generator.h:38`, Q26) assumes a per-rock throughput limit that does not exist.** `MiningSystem.cpp` lets any number of miners extract from one rock concurrently; asteroids are inexhaustible until M3. Six miners on the best rock: 38.4 cr/s measured.

D6  FACT (measured on this Linux machine, g++ -O2; order of magnitude). **ADR-024's accumulator cost estimate ("on the order of 10⁷ integer operations a second, which is nothing") is wrong at the stress target.** `Accumulator::Fill` costs 51.5 ms per tick at 100 clients × 5,500 entities against a 50 ms tick (0.5 ms per client), because it builds every entity's record twice per client per tick (`Accumulator.cpp:286` and `:307`) and fully sorts all candidates (`:332`). At 2 and 4 players it is 16 µs and 75 µs — nothing.

D7  FACT. **The client forgets an entity after 3 sweeps = 3 ticks = 150 ms at the MVP** (`ReplicaStore.cpp:86-94`, `Update.h` `SweepTicks`, `FORGET_AFTER_SWEEPS`), while the link is only called lost after 1,000 ms of silence (`ClientFrame.h` `LINK_SILENCE_MILLISECONDS`). A silence between 200 ms and 1 s therefore drops and re-adds entities instead of holding them.

D8  FACT. `Attack` (type 2) is accepted, acknowledged and does nothing (`CommandIntake.cpp:158-163`); the client's tap table issues it on a hostile (`GameClient/Selection.cpp:83`). Documented as M3's.

D9  FACT. The endpoint timeout ADR-013 and TechnicalDesign §4 describe ("a heartbeat so the host can time it out") is not implemented; the host sends two updates a tick to a dead endpoint forever (`Host.cpp:SendUpdates`, ADR-013 "not implemented by this decision").

D10 FACT. Match restart (M3.8), victory, elimination, damage, weapons, point defense, miner flight, finite ore, the stub AI and the AI are all unwritten; `Host::BeginMatch` exists and nothing calls it after construction/start. Consistent with the designer's status.

D11 FACT. The determinism pin `0x18e094912655348f` has been computed under g++/clang only; CI (`Debug|x64`) has confirmed the previous pin, not this one; the four MSVC pairs are owed (README, ADR-002). Consistent with the designer's status.

## 2. The map

Legend: DI = designed & implemented as designed; DD = implemented differently from design; DN = designed, not implemented; IN = implemented, not designed; RN = referenced but never specified.

| System / mechanic | Class | Evidence |
|---|---|---|
| Tick at 20 Hz, integer/fixed-point simulation, PRNG streams | DI | `Tick.h`, `FixedPoint.h`, `Pcg32.h`; streams 1 tokens, 2 sky, 3 generator, 4 rock look |
| Tick order (commands → movement → mining → economy → build) | DI (partial) | `Host.cpp:RunOneTick`; TechnicalDesign §2 lists orders/AI/weapons/deaths/victory too — those are DN |
| Entity store, free list, generation, state hash over id/pos/heading/hull | DI | `World.cpp`, `StateHash.cpp` |
| Component catalog (5 hulls, 3 drives incl. None, 8 components incl. None), design table (7 rows), derived stats | DI | `Catalog.cpp`, `Design.cpp`, `DerivedStats.cpp` |
| Turn rate / turning / facing | RN + DN | GameDesign §6:331 names it; nothing derives or applies it; see D2 |
| Movement: straight line at max speed, arrive-and-snap, no acceleration, no collision, ships pass through everything | IN | `Tick.cpp:MoveEverything`; GameDesign §1 mentions "pathfinding, collision" only as being 2-D; no collision is designed |
| Ring slot assignment on move orders (hex rings, spacing = widest hull) | DI | `RingAssignment.cpp`; GameDesign §7 |
| Rally point / spawn stacking: new ships spawn at one fixed point in front of the station and stack there | DI (design accepts it) | `BuildSystem.cpp:SpawnPoint`; GameDesign §5 "no rally point" |
| Generator: symmetric field, 10 home + 2×6 contested rocks per region, exact rotations | DI | `Generator.cpp`, `Layout.cpp`; Q26 |
| Contested fields "richer" | RN | see D4 |
| Mining loop (ToOre/Extracting/ToUnload/Unloading), cargo in milli-ore, unload query over grid, derived capacity/rate/range | DI | `MiningSystem.cpp`, `UnloadTarget.cpp`; Q51, Q52 |
| Per-rock ore / exhaustion / husk / retarget | DN (M3.9) | GameDesign §4; `World::Field()` is a list of positions only |
| Economy: 1 ore = 1 credit, remainders carried, ore processor % exact | DI | `Economy.cpp`; Q56 |
| Build: single slot, cost deducted at start, full refund on cancel/replace, 20 cr/s, ticks round up, shipyard multiplies | DI | `BuildSystem.cpp`; Q35, Q47, Q56 |
| Modules: 4 designs, placement rule (400 radius, cap 4, clearances 155/90), in-place upgrade at the difference, best-of-kind effect | DI (stacking rule IN, recorded) | `ModuleSite.cpp`, `BuildSystem::StartModule/StartUpgrade`, `ModuleEffects.cpp`; ADR-015 "As built" |
| Command validation (ownership, selection bound, generation, clamp, sequence wrap) | DI | `CommandIntake.cpp`; Q24 |
| Attack order | DN (M3.2) | see D8 |
| Weapons, damage table, hit value curve, firing cadence | DN; cadence RN (F3/ADR-014 reserved) | GameDesign §7; Plan/M3.0 |
| Point defense, safe zone | DN (M3.5) | GameDesign §5 |
| Miner flight when fired upon | DN (M3.6) | GameDesign §7 |
| Death, removals carrying a death, wrecks | DN (M3.4); removal path built and empty | `Accumulator.cpp:272-292` |
| Elimination, victory, modules removed with ships | DN (M3.7, M3.8b) | GameDesign §2 |
| Match restart on victory; client detecting a reseed | DN (M3.8); ADR-013 names the detection gap as unsolved | ADR-013 Consequences |
| Stub AI (M3.10), AI (M4.5), AI taking an abandoned slot (M4.6) | DN; Q48 open | GameDesign §8 |
| Bot stress harness (players/churners/flooders), BotPolicy | DI (ADR-022), never run against a real host | `Bot/Bot.cpp`, `GameClient/BotPolicy.cpp` |
| Replication: 12-byte record, 21-byte header, priority accumulator, sweep, 2 updates/tick, removals ×10, fires ×3 | DI (ADR-024); measurements owed | `Update.h`, `Accumulator.cpp` |
| Client forget after 3 sweeps | DI, but see D7 | `ReplicaStore.cpp:86` |
| Join/session/token/rejoin, lowest free slot, MatchFull reply | DI (ADR-013) | `Sessions.cpp`, `Host.cpp:AnswerJoin` |
| Endpoint timeout | DN (deferred in ADR-013) | see D9 |
| Player count as runtime value, stress layout above 4, three-player layout admittedly unfair | DI (ADR-023) | `Layout.cpp`, `Host.h:PlayerCountAllowed` |
| Free Fighter for player 1 on the real host | IN | see D1 |
| Fog of war, formations, research, designer, Cruiser design, research module | DN (post-MVP / M4) | GameDesign §9, §10 |
| Client: interpolation 75 ms behind, three samples per entity, hold never extrapolate | DI | `ReplicaStore.cpp:Drawn` |
| Client: camera, selection (tap, double tap 192 px circle), order markers, four panels, credit flash, cargo chips, module placement preview | DI per plan; unverified on the device since M1.9 | `GameClient/*`, Plan/M2 "not drawn or touched" |
| Damage alert, hull bars in world, tracers | DN (M3.3, M3.3b) | ADR-020 |
| Income rate readout | decided out (Q36) | Interface §6 |
| Audio, minimap, chat, ping, pause, save | decided out | Interface §5, §7; GameDesign §2 |

## 3. Tuning table (from the code; the docs agree unless noted)

| Quantity | Value | Source |
|---|---|---|
| Tick | 20 Hz, 50 ms | `Tick.h` |
| Position unit | 1/256 world unit; square 16,384 units (±2,097,152) | `FixedPoint.h`, `Command.h` |
| Wire position step | 1/4 unit (int16 over the square) | `EntityRecord.h` |
| Anchor radius | 6,000 units; opposed stations 12,000 apart (85.7 s at 140 u/s); adjacent at 4 players 8,485 (60.6 s) | `Layout.h` |
| Home field | 10 rocks, 600–1,500 from the anchor; contested: 2 clusters × 6 rocks, centres 1,500–3,500 from the origin, 600 spread; 150 min spacing | `Generator.h` |
| Hulls (slots / HP / mass / cost / hit value / size units) | Scout 1/450/10/60/0/60; Frigate 2/600/20/100/0/90; Cruiser 4/3000/60/2080/0/150; Station 2/8000/0/0/300/220 acceptsOre; ModuleFrame 1/1500/0/0/300/90 | `Catalog.cpp` |
| Drives (mass / thrust / cost) | None 0/0/0; Ion 5/2000/40; Burn 10/5600/80 | `Catalog.cpp` |
| Components | MiningLaser mass 5, range 200, 20 ore/s, 100 capacity, cost 50; MassDriver mass 5, range 600, 25 dps, cost 60; PointDefense mass 5, range 400, 60 dps, station-only, no cost; ShipyardL1 400 cr ×1.50, L2 700 cr ×2.00; OreProcessorL1 350 cr ×1.25, L2 600 cr ×1.50 | `Catalog.cpp` |
| Designs (cost / speed) | Miner 150 cr, 100 u/s (5/tick); Fighter 300 cr, 140 u/s (7/tick); Station 0, 0; modules by component, not buildable (placed) | `Design.cpp`, `DerivedStats.cpp` |
| Upgrade cost | Shipyard L1→L2 300; OreProcessor L1→L2 250 | `ModuleSite.cpp` |
| Damage table (design, unimplemented) | MassDriver vs Small/Med/Large 70/60/25 %; PointDefense 120/90/30 %; structures: base×100/(100+hitValue), 300 → quarter | GameDesign §7 |
| Firing cadence | UNSPECIFIED (F3, ADR-014 reserved) | Plan/M3.0 |
| Raid arithmetic (design) | 1 fighter kills a miner 12.9 s; fighter v fighter 20 s; 3 fighters v 6 miners 25.7 s; PD kills a fighter <6 s; 1 fighter v module 120 s, v station 640 s; 10 fighters v station ~64 s | GameDesign §5, §7 (arithmetic reproduced by the reviewer) |
| Starting credits | 1,000 | `BuildSystem.h` |
| Build rate | 20 credits of cost per second; Miner 150 ticks, Fighter 300; ShipyardL1 ×1.5, L2 ×2.0; ticks round up | `BuildSystem.cpp` |
| Unload | 50 ore/s once touching (half sizes + 20 slack: 160 units Scout→Station) | `MiningSystem.h`, `UnloadTarget.h` |
| Cargo chips | 3 bits, 0–4, quarters rounded up | `EntityRecord.h` |
| Module rule | radius 400 (= PD range), cap 4, clearance 155 from station, 90 between modules | `ModuleSite.h` |
| Ring spacing | widest hull size in the selection; ring k holds 6k slots | `RingAssignment.cpp` |
| Update | 1,232 B payload, 21 B header, 12 B record, 100 records max, 65 floor, ≤48 removals, ≤40 fires, 2 updates/tick | `Update.h` |
| Accumulator weights | base 1, in view 4, changed 2, own 2 | `Accumulator.h` |
| Sweep / forget | ⌈live ÷ 130⌉ ticks (1 at 110, 43 at 5,500); forget after 3 sweeps | `Update.h` |
| Interpolation | 75 ms behind, 3 samples/entity | `ReplicaStore.h` |
| Link lost | 1,000 ms of silence; view report every 250 ms; join retry 250 ms | `ClientFrame.h`, ADR-013 |
| Removal / fire repeat | 10 / 3 ticks | `Update.h` |
| Command | 8 B fixed (+1 for PlaceModule), 3 B per identity, 12 B packet header incl. view | `Command.h` |
| Camera | 40° FOV, pitch floor 30°, near 50, far 50,000, zoom 1,400–22,500, opens at 2,400 | Interface §5 |
| Touch tiers | 48 / 64 / 96 authored px; pick radius 24; tap slop 16; double tap 300 ms; palm >78 px | Interface §1 |

## 4. Tick order as implemented (Host.cpp:RunOneTick) vs designed (TechnicalDesign §2)

Implemented: drain & apply commands → `Tick` (movement only) → `MiningSystem::Advance` (rebuild grid, run mine orders with in-pass phase changes) → `Economy::Credit` → `BuildSystem::Advance` → ++tick → fill & send updates.
Designed: drain → orders, AI, movement, weapons, mining, build queues, deaths, victory → state hash. AI, weapons, deaths, victory are M3/M4.

## 5. Figures measured in this session (harness: scratchpad/sim.cpp, GameCore + GameLogic compiled with g++ 13 -O2 on Linux against a stub windows.h; the simulation code is unmodified)

Seed 20260922, two players. Player 1's home rocks are 797–1,496 units from the anchor; contested rocks 4,509–5,858 (2,563–3,791 from the origin).

| Rock | Distance from anchor | One Miner, steady state | First delivery |
|---|---|---|---|
| nearest home (idx 0) | 797 | 6.40 cr/s | tick 289 (14.5 s) |
| typical home | 1,029–1,223 | 4.00–4.67 cr/s | 372–502 |
| farthest home (idx 8) | 1,496 | 3.33 cr/s | 560 |
| contested (idx 10–21) | 4,509–5,858 | 0.67–1.33 cr/s | 1,769–2,310 |

With OreProcessorL1 the nearest rock gives 8.00 cr/s per miner, L2 9.60. Six miners on the nearest rock: 38.4 cr/s (no throughput limit).

Opening simulation (build Miners back to back at 20 cr/s, each sent to the nearest rock, no shipyard): income passes the build rate before 60 s; at 300 s there are 40 miners, income 233 cr/s, 30,165 credits banked and unspendable.

Host cost on this machine (single thread): 2 players/110 entities: simulation 13 µs + accumulator 16 µs per tick. 4/220: 25 + 75 µs. 100/5,500: simulation 0.66 ms + accumulator 51.5 ms per tick (0.51 ms per client, 200 updates a tick).

Caveats: a cloud x64 VM with g++, not the Surface Pro with MSVC; all four contested/home figures are the shipped code's arithmetic on the shipped constants; nothing here was run on Windows.

## 6. Gates and measurements still owed (from Plan/README, ADRs, README)

Human/hardware: M0.5 LAN run and wireless loss/jitter (two machines); M0.23 two-machine tap-to-visible; M1.14b Bot against a real Server; M1.14c's four measurements (refresh interval, accumulator cost, 8-seat stress, tap-to-visible re-run); M1.15 two visible clients playing (owner chose two snapped windows); M1.16 seven confirmations (circle, text legibility, interface vs world pass cost, gesture constants, camera stick/orbit, sky frame time, link-silence threshold); ADR-021 clean-install check; M2.13 silhouettes (ships and modules at tactical zoom); M2.14 tick cost at 110; the four-pair determinism run for the current pin; "nothing M2 added to the screen has been looked at". Decisions: M3.0 firing cadence (ADR-014); Q34 (60/120 Hz); Q48 (AI shape); Q49 (weights).

## 7. Where to look

GameCore: Catalog, Design, DerivedStats, Entity, EntityRecord, Command, Update, Join, Layout, Generator, ModuleSite. GameLogic: World, Tick, StateHash, UniformGrid, UnloadTarget, MiningSystem, Economy, ModuleEffects, BuildSystem, CommandIntake, RingAssignment, Accumulator, Sessions, Host. Server/Server.cpp. Bot/Bot.cpp, GameClient/BotPolicy. GameClient: ReplicaStore, Interpolation, ClientFrame, Selection, TapOrder, ModulePlacement, Panels, HudLayout, Camera. Tests: Tests/GameLogicTests/*.cpp (DeterminismTests pins the hash), Tests/GameCoreTests/*.cpp. Docs: Design/*.md, Design/ADR/*.md, Design/Plan/*.md, AGENTS.md, .claude/skills/*/SKILL.md. Gates: `python3 Scripts/CheckDeterminism.py [--review]`, `CheckDesign.py`, `DatagramBudget.py` — all clean at HEAD.

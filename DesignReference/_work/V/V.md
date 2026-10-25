# V: independent verification of the Game Design Reference canvas

Checked against the repository at `a053fe294d91dc885bbb061c2f34d76ca6897738`, from the local copy in `DesignReference/canvas/project/` (90 artboards). Every value below was traced to code, data, ADRs or design documents. The makers' notes in `_work/A*`, `_work/R` and `compose/` were not used as evidence. I read them only twice, to see how two samples had been chosen: `A4/derive_layouts.py:1520`, where the pirate sectors are hard-coded, and the A6 notes.

Helpers, all in `DesignReference/_work/V/`:
- `totext.py` writes each artboard to `text/*.txt`.
- `frames.py` and `dump_frames.py` write each screen frame's elements to `frames/*.tsv`.
- `colors.py` converts every linear color constant to sRGB and writes `code_colors.json`.
- `screen_colors.py` maps every color the frames use to a token.
- `sample_numbers.py` picks random facts with seed 20261025 and writes `random_sample.tsv`.
- `render.cjs`, `fontprobe.cjs`, `fontshot.cjs`, `shot.cjs` and `probe.cjs` render the artboards with Playwright, into `render/`.

## Summary

| # | Check | Verdict | One line |
|---|---|---|---|
| 1 | Numbers (60 sampled) | **PARTIAL** | 56 of 60 are right, including all 24 random picks in the text chapters. The 4 wrong ones are two black color swatches and two screen figures: "4,320" maximum hit points and "of 25". |
| 2 | Screen elements (26 traced) | **PARTIAL** | Fixed geometry, strings, faces and colors from Hud.cpp are reproduced exactly. Several frames show impossible or missing content: a missing status panel, pirates in "between" sectors, no seeded asteroids, and contradictory sample states. |
| 3 | Screen-state coverage | **PASS** | Every state the code can show is drawn or described in the text. |
| 4 | Models coverage | **PASS** | All 21 meshes of Human, Tarkan and Pirate, every level from L1 to L5, the Relay and Repair Bay looks, and the 3 asteroids are imaged and tabled. Wreck shows only the 3 hulls; the reference says so and gives a correct reason. |
| 5 | No Horizon, self-play or unbuilt Look work | **PASS** | Every hit is an exclusion statement, a false positive, or a developer switch labelled as not part of play. |
| 6 | Colors on screens | **PARTIAL** | All 57 distinct colors map exactly to code tokens. But `#1F2739` stands for both CARD and CLOSE_BOX without a flag, translucent colors are blended in sRGB rather than linear light, and the minimap fog is a uniform 55%. |
| 7 | Sources present (20 statements) | **PASS** | 20 of 20 statements carry a source, and each source says what is claimed. Minor: some cross-references point at the makers' notes ("see A1", "as A2 traced it"). |
| 8 | Readability | **FAIL** | Four defects: a table that overflows its artboard, collided labels on the flow diagram, truncated step-card titles with an empty "()", and three black placeholder swatches. No missing images, markdown leakage or raw escapes. |

**17 FAIL items** are listed, ranked, at the end.

---

## Check 1: Numbers

Method: I took 24 numeric facts at random (seed 20261025, via `sample_numbers.py` over 2,211 table cells of the chapters; R3, R5, R6, R17, R26 and R28 were dropped as non-numeric), and added 36 targeted facts I judged most likely to be wrong. Each was checked at its primary source.

### Random picks

| ID | Artboard | Shown | Cited | Primary source (file:line) | Verdict |
|---|---|---|---|---|---|
| R1 | spec-orders-away | `orders.form.label` at 24, 247 | Hud.cpp | ORDERS_FORM_TOP = 36+12+22+7×22+20 = 244, label +3 (Hud.cpp:1939, 1958) | PASS |
| R2 | sides | Human (0.18, 0.42, 0.95) | Models.json:5 | Models.json:5 | PASS |
| R4 | economy-territory | CS L5 upgrade 900 | Tuning.json line 46 | Tuning.json:46 | PASS |
| R7 | spec-hud-1 | status row-0 bar 28, 102, 356, 4 | Hud.cpp:3241-3308 | 68+10+22+(8−4)/2 = 102; 380−24 = 356; STATUS_UNDER_BAR_UNITS 4 (Hud.cpp:145-146, 3290-3296) | PASS |
| R8 | sides | Wreck `#AAAAAD` | Models.json:40 | sRGB of (0.4, 0.4, 0.42) is `#AAAAAD` (Models.json:40) | PASS |
| R9 | economy-map | A2 z −3000…−1000 | Map.json | Map.json:109 | PASS |
| R10 | economy-map | center: 4 contested, r 45, 12,000 | Map.json:156 | Map.json:156 | PASS |
| R11 | battlefield-1 | starburst.dds 128×128 | ADR-022 | DDS header 128×128 (Assets/Textures/starburst.dds) | PASS |
| R12 | ships-parts | Pulse hitPointsFactor 0.75 | Tuning.json:23 | Tuning.json:23 | PASS |
| R13 | spec-windows | Production in slot 1 at (592, 178) | Hud.cpp:3643-3651 | 2×16+560 = 592; WINDOWS_TOP 178 (Hud.cpp:1635, 3648-3649) | PASS |
| R14 | spec-designer | picked name cut at 78 | Hud.cpp | 108−24−6 = 78 (Hud.cpp:926, 1436) | PASS |
| R15 | battlefield-2 | fields r 150 and 180, 80 of them | Map.json | Map.json:16-97 (80 entries) | PASS |
| R16 | ships-parts | Constructor: 0 guns | glb | Art/Models/{Human,Tarkan}/Constructor.glb: exhaust 1, no gun | PASS |
| R18 | visual-rules | BUTTON_INSET 12 | Hud.cpp:107 | Hud.cpp:107 | PASS |
| R19 | battlefield-1 | depth 32-bit float, cleared to 1 | Renderer.cpp:25-26 | Renderer.h:26 D32_FLOAT; Renderer.cpp:26 DEPTH_CLEAR 1 | PASS |
| R20 | battlefield-2 | ore asteroid face (0.052, 0.048, 0.045) | SceneShading | (0.35, 0.32, 0.30) × ROCK_FILL_SHADE 0.15 (Models.json:45; SceneShading.h) | PASS |
| R21 | battlefield-4 | glow: 5 levels, R11G11B10 | Bloom.h:27,30 | Bloom.h:27, 30 | PASS |
| R22 | ai | secondSlotTier 3 | Opponent.json | Opponent.json:53 | PASS |
| R23 | ships-damage | Medium+Ion+Missile Rack, 9.0 per 100 Ore vs Small | derived | 70.0 ÷ 7.80 = 8.97, with cost 330+60+390 (Tuning.json:17, 21, 28; DesignStats.h:88, 105-136) | PASS |
| R24 | spec-hud-2 | Back to menu button 712, 138, 496, 34 | Hud.cpp:3340-3350 | 700+12; 68+12+30+22+6 = 138; 520−24 (Hud.cpp:3341-3349) | PASS |
| R25 | visual-anatomy-2 | Rename label at (14, 16) | Hud.cpp | DESIGNER_INSET+14, footer+16 (Hud.cpp:1552) | PASS |
| R27 | visual-rules | status panel at line 3250 | Hud.cpp | Hud.cpp:3250 | PASS |
| R29 | ai-difficulty | Hard attackWithoutLeadShare 1.5 | OpponentHard.json:50 | OpponentHard.json:50 | PASS |
| R30 | ships-designs | Defence Autoloader +20% | Tuning.json:92-93 | Tuning.json:92-93 | PASS |

### Targeted picks

| ID | Artboard | Shown | Primary source | Verdict |
|---|---|---|---|---|
| T1 | economy-ore | rig income 3.5 / 4 / 5.5 / 6.5 Ore a second | Tuning.json:6-9 | PASS |
| T2 | economy-ore | home rig runs dry in 2,143 s (35:43); rich in 61:32 | 7,500 ÷ 3.5 and 24,000 ÷ 6.5 (Map.json:9, 157) | PASS |
| T3 | economy-map | 459,000 Ore on the map; 40 ore asteroids | Map.json:9-14, 150-157 | PASS |
| T4 | economy-map | start to start about 11.3 km | √2 × 8,000 (Map.json:5-6) | PASS |
| T5 | economy-territory | one-node lead lasts 50 min; a three-node lead about 17 min | 1,000 × 3 s; 333.3 × 3 s (Tuning.json:14) | PASS |
| T6 | economy-territory | first drain at the end of the 60th tick | `(m_tick+1) % 60` (Simulation.cpp:3786-3788) | PASS |
| T7 | structures | CS hit points at L5: 9,000, or 10,250 with +25% | `hp × (factor + 0.2(L−1))` (Simulation.cpp:3153-3156) | PASS |
| T8 | structures | Lab L4: 2,400 hit points, 2 slots | Tuning.json:54, 58 | PASS |
| T9 | research | tiers total 690 / 740 / 830 s, 2,260 s, 6,800 Ore | sum of Tuning.json:66-111 | PASS |
| T10 | ships-designs | turn radius 19.9 m (Small+Ion), 23.9 m (Large+Fusion) | 78 ÷ 3.927; 20 ÷ 0.838 (Tuning.json:16-23) | PASS |
| T11 | ships-designs | Lance with both topics: 43,200 milliticks | 2.7 ÷ 1.25 × 20,000 (Simulation.cpp:1573) | PASS |
| T12 | ships-designs | Swarm, Picket, Brawler, Lancer cost 261 / 411 / 495 / 645 | DesignStats.h:88 | PASS |
| T13 | ships-damage | Flak vs Small: 18 a hit × 5 ships = 180.0 | DesignStats.h:18-21, 105-136 | PASS |
| T14 | combat | matchup 3: 6,270 Ore, 22 command points, 4,950 hit points | 11 × (570, 2, 450) | PASS |
| T15 | combat | pirates: 10 structures, 24 ships, 7 outposts | Map.json:159-163; Tuning.json:123-127 | PASS |
| T16 | combat | stronghold platforms 140 m apart, ring 120 m | Simulation.cpp:82-83 | PASS |
| T17 | ai-difficulty | Easy differs from Normal in 12 fields, Hard in 6; 31 fields | diff of the three files | PASS |
| T18 | ai | home platforms 260 m, turns 0, ±0.5, ±1 rad, rings 70 m | AiPlayer.cpp:37-39 | PASS |
| T19 | ai | close in 500 m; pirate resend 10 s; salvage give-up 300 s; Bay 220 m | AiPlayer.cpp:33, 54, 60, 62 | PASS |
| T20 | world | default port 45000 | WorldSettings.h:6 | PASS |
| T21 | world | certificate valid 5 years | **NeuronCore/**ServerCertificate.cpp:28 (cited without its folder) | PASS |
| T22 | world | handshake 10 s; welcome 10 s | QuicChannel.h:31; QuicTransport.cpp:9 | PASS |
| T23 | world-state | save every 60 s, newest 3 kept | WorldFolder.h:15, 23 | PASS |
| T24 | world-state | restart after 3,600 s | Simulation.h:343 | PASS |
| T25 | world-state | battle window 10 s, gap 30 s, hour 3,600 s | WorldLog.h:55-57 | PASS |
| T26 | battlefield-1 | pitch at the default 500 m: 43.7° | lerp(40, 70, 350/2850) (Camera.json; Camera.cpp:160-165) | PASS |
| T27 | battlefield-1 | 50,000 stars, of which 25 crosses | Starfield.cpp:20-21 | PASS |
| T28 | visual-scale | steps 100 to 200%; 16:9 reaches 125% on the menu and 110% in a match; 2880×1920 reaches 125%; 4:3 about 150% | Hud.h:861; Hud.cpp:3035-3073 (recomputed) | PASS |
| T29 | visual-scale | Controls 700×762, Orders 640×560, Away 600×530, designer 818×913 | 31 bindings (KeyBindings.h:92-126); Hud.cpp:1939-1944, 1987, 1076-1090 | PASS |
| T30 | visual-type | face sizes 20/22/13/16/14/28/13 | Hud.h:46 | PASS |
| T31 | orders | pick 12 px, drag 6 px, double click 400 ms | Picking.h:26; PlayerControls.h:20, 22 | PASS |
| T32 | combat | stall: 0.1 m for 1 s, within 45° | Simulation.cpp:17-24 | PASS |
| T33 | ships-parts | CS L4/L5: 6 gun hardpoints; DefensePlatform: 6 | counted in Art/Models/*/*.glb | PASS |
| T34 | assets-table | Tarkan Large: 142 tris; Human CS_L4: 828 tris, 2,484 verts | NMF headers | PASS |
| T35 | visual-colors | faceLightLookingUp `#000000`; lineLightLookingUp `#000000` | FaceLight((0,1,0)) = (0.895, 0.877, 0.932), which is `#F3F1F7`; LineLight = 0.8846, which is `#F2F2F2` (SceneShading.cpp:7-25) | **FAIL** |
| T36 | screen-hud-busy-warships (and 3 more frames) | "Hit points 3,860 / **4,320**" for 6 Swarm, 4 Lancer, 2 Brawler | 6×198 + 6×450 = **3,888** (DesignStats.h:86, with Ion's ×0.9 at Tuning.json:21). With Hull Plating, which the Lab researching Large Hull implies, it is 4,472. 4,320 is the bare hull figure. | **FAIL** |

The 61 color-token hexes also all match, counted here as one further fact (see Check 6). The empire-fallen frame's "of 25" is counted under Check 2.

---

## Check 2: Screen elements

How the frames were read: `frames/*.tsv` lists every positioned element of each frame. Each element below was recomputed from Hud.cpp. Text widths were allowed as estimates.

| # | Frame | Element | Frame value | Hud.cpp value | Verdict |
|---|---|---|---|---|---|
| E1 | menu-default | menu panel | 740, 422, 440×236, `#181E2B` | MENU_WIDTH 440; 24+30+22+6+4×34+3×6 = 236; centred (Hud.cpp:2338-2344) | PASS |
| E2 | menu-default | title and subtitle | "Outpost Commander", GOLD, 22 px, 700, stretch 87.5% at 752, 434; "A skirmish against the AI", DIM, 16 px | Hud.cpp:2346-2347; Hud.h:46; Title face SemiCondensed (Hud.cpp:3011) | PASS |
| E3 | menu-default | button labels and faces | "Easy / Normal / Hard skirmish", "Quit"; CARD face, EDGE outline; label at +12, +6 | Hud.cpp:2329-2334, 1300-1318 | PASS |
| E4 | menu-connection-lost | notice | 2 lines in WARNING, plain text; panel 440×320 at y 380 | Hud.cpp:2336-2352; ADR-085 d1 | PASS |
| E5 | menu-scale-125 | panel | 685, 392.5, 550×295 | ×1.25 of 548, 314 in a 1536×864 reference | PASS (geometry) |
| E6 | menu-scale-125 / hud-scale-110 | font sizes | 27.5, 17.6, 15.4, 14.3, 24.2 px | Glyphs are rasterized at round(units × scale): 28, 18, 15, 14, 24 (UiPipeline.cpp:28-31) | **FAIL** (minor) |
| E7 | window-controls | window and 31 lines | 610, 159, 700×762; "CONTROLS" at +24, +11 in HEADER_LABEL; keys in ACCENT at x 634, actions in TEXT at x 874; every string verbatim | Hud.cpp:1888-1926; KeyBindings.h:92-126 | PASS |
| E8 | window-orders-* | window and rows | 1264, 178, 640×560; WAITING at y 229; form label at 425; rows every 30 from 444; strings "Enemies enter a sector", "At a time of day", "Over 8 enemy CP there", "Fires once, the first time it happens", "SELECT WARSHIPS" | Hud.cpp:1931-1980, 2230-2296; OrderForm.cpp:192, 238-264 | PASS |
| E9 | window-away | window and lines | 660, 429, 600×222; "WHILE YOU WERE AWAY"; "Away for 7:42:10"; "Lost 9 ships and no structures"; "Order held back: hold B2" | Hud.cpp:1983-2011, 2297-2320; Alerts.cpp:32-45 | PASS |
| E10 | window-designer-default | window rect | 1086, 16, 818×913 | DESIGNER_WIDTH 818; ExtentOf = 913 with Tuning.json (Hud.cpp:913-949, 1076-1090) | PASS |
| E11 | window-designer-default | header | "SHIPYARD 01" and "· DESIGNER" in HEADER_LABEL, tracked 2 px, y 27; "QUEUE · 0 / 5" at x 1588; Ore box | SLOTS_LEFT 502; Hud.cpp:1361-1381 | PASS |
| E12 | window-designer-default | part cards | Large card hatched, LOCKED; "RESEARCH · LARGE HULL" in AMBER; numbers "220 HP · ARM 2 · 60m/s" | Hud.cpp:622-635, 1442-1481 | PASS |
| E13 | window-designer-default | damage cards | 30.0 / 15.0 / 8.8; "11.5 / 100 ore"; title "DAMAGE / S AFTER ARMOR · VS A FORMATION" | DesignStats.h:18-21; Hud.cpp:781-806, 1510 | PASS |
| E14 | window-designer-default | retreat row | "25% 50% NEVER" in capitals, Label face; 25% lit | RETREAT_CHOICES and Capitals (Hud.cpp:461-475, 1286-1307) | PASS |
| E15 | window-designer-preview | preview | "Range 220 (+100)" in GOOD; "Cost 411 (+150)" in POOR; "+4.4 / +17.2 / +21.2"; "Preview: with Lance instead" | Hud.cpp:731-806 | PASS |
| E16 | window-designer-unsaved-editing | name | "Lancer with the long range sensor fit_" "**37/32**" in WARNING; PICKED_EDGE outline | Typing stops at 32: `typed.size() < DESIGN_NAME_LIMIT` (Designer.cpp:236). A refused name can only be empty or all spaces. | **FAIL** |
| E17 | window-production-none | hint and height | "Build a Shipyard to make warships." at y 298; window 478×238 | Hud.cpp:2837, 1771-1775, 1680-1684 | PASS |
| E18 | window-research-page1 | header and cards | "TOPICS · 1-10 OF 21"; "NEEDS · RESEARCH LAB L2" and "+ MASS DRIVER CALIBRATION"; "TIER 1 · 75 s"; window 560×694 at 510, 178 | Hud.cpp:1815-1880, 2969-2986 | PASS |
| E19 | hud-shipyard-selected | selection and buttons | lines "Shipyard 01 · L1", "Hit points 2,500 / 2,500", "Building Swarm · 62% · +2 queued", "L2: Medium hulls, 3,000 hit points"; caps P, D; "Upgrade to L2 · 0:30" with 150; panels 630, 930, 280×134 and 910, 926, 380×138 | Hud.cpp:2560-2690, 3357-3453 | PASS |
| E20 | hud-busy-warships | ship bars and retreat | 12 bars 17×10 every 21 px; the retreating one outlined in TEXT; "MIXED"; cells "25% 50% Never" in the Name face | Hud.cpp:85-90, 3398-3415, 3437-3452 | PASS |
| E21 | hud-end-victory | banner | 700, 68, 520×116; "Victory" in GOLD; "By domination. Match length 47:12" | Hud.cpp:3340-3350, 2199-2218 | PASS (layout); content see E24 |
| E22 | hud-server-silent | connection tag | "Server 203.0.113.5:4433"; chip "No word from the server · 5 s"; height 64 | Hud.cpp:3320-3337 | PASS |
| E23 | hud-placing-relay / relay-node-cap / derelict-hint | hint panel | 640, 16, 640×44; "Placing Relay: left-click to build, right-click to cancel"; "Relay: at the node cap; upgrade the Command Station to claim more"; "Derelict: 300 Ore, and part of Large Hull's research time · salvaged 40%" | Hud.cpp:2546-2552, 2135-2146, 3311-3316 | PASS |
| E24 | hud-end-victory | territory beside a domination win | enemy 731 tickets, player 642 and losing ("You -40 a minute") | A domination win requires the loser's tickets at 0 (Simulation.cpp:3796-3804) | **FAIL** |
| E25 | hud-end-empire-fallen | nodes note | "of 25" | With no station the cap is level 1's, 4 (NodeCapOf, Simulation.cpp:2912-2920; snapshot.nodeCap :1116), so it reads "cap 4 of 25" (Hud.cpp:3195) | **FAIL** |
| E26 | hud-lab-needs-research | selection lines | "Researching Fusion Drive · 35%" and "L2 needs Improved Extraction **and Hull Plating**" | Fusion Drive requires Hull Plating (Tuning.json:74), which must be done before it starts (Simulation.cpp:3181-3189). The line can only be "L2 needs Improved Extraction" (Hud.cpp:2678-2687). | **FAIL** |

Frame-level findings (detail in the ranked list):

- **Status panel missing** in designer-default, designer-preview, designer-unsaved-editing, production-shipyard and research-page1. Each shows a finished Shipyard 01 (BUILT · 4) or a working Lab (Hull Plating 45%). StatusLinesOf always adds lines for these (Hud.cpp:281-414), and the panel is drawn at 16, 68 (Hud.cpp:3241-3308). Compare hud-shipyard-selected, which has it.
- **Pirate-guarded sectors D2 and B4**, both of kind "between", are drawn in every in-match frame (27 of them). No seed can do this (Map.json:159-163, Map.cpp:420-440).
- **Minimap shows only the 6 home ore asteroids.** All 40 are always in the snapshot ("The map's asteroids and fields are always seen", Simulation.cpp:245-247) and all are drawn (Hud.cpp:3486-3528).

---

## Check 3: Screen-state coverage

States listed from the code: WinMain.cpp:220-520, GameClient.cpp:480-800, and Hud.cpp's Describe, Lay and window functions.

| State (code) | Frame or text |
|---|---|
| Startup: window hidden until loaded; message box on failure (WinMain.cpp:273-316, 95-101) | screen-startup-loading (described: nothing drawn) |
| Menu: default; with Join world; with a notice (join failed / connection lost); scaled | 4 menu frames, plus menu-scale-125 |
| `--join`, and `--measure`/`--load`/`--stress` skipping the menu | screens-flow (but see FAIL 15) |
| Match before the first snapshot (no HUD) | screen-match-awaiting-first-snapshot |
| HUD: start; one ship; many ships with an armed order; Constructors; placing; Relay at the node cap; structure selected; station upgrading; Lab whose next level needs research; derelict hint; silent server | 12 hud-* frames |
| Memory hint "… · last seen …", rig "Ore left / run out", sector earning lines, "Top level", "and n more designs" | spec-hud-1 / spec-hud-2 (text) |
| End banners: Victory and Empire fallen drawn; Defeat, Draw, "A fleet destroyed." and "Out of time." described | 2 frames, plus spec-hud-2 and screens-flow |
| Designer: default, preview, editing/refused, no Shipyard; "UPDATE", chips scrolling, Needs Shipyard L2 | 4 frames, plus spec-designer |
| Production: none, Command Station, Shipyard; no saved designs; "NEEDS L2"; WAITING FOR FLEET CAP | 3 frames, plus spec-production-research |
| Research: no Lab, page 1; "% BUILT"; "SALVAGED"; second slot | 2 frames, plus spec-production-research |
| Controls, Orders (time of day, event, disabled Give), Away | 4 frames |
| Three windows at their default places; interface scale 110% and 125% | 3 frames, plus visual-scale |

Nothing is missing. The only gap is that some states, such as the Defeat and Draw banners, appear in text alone; that is acceptable.

---

## Check 4: Models coverage

Models.json (Models.json:1-58) has:
- Human and Tarkan: Small, Medium, Large, Constructor, CommandStation ×5, Shipyard ×5, ResearchLab ×5, MiningRig, DefensePlatform. That is 21 each.
- Pirate, on Tarkan's meshes.
- Wreck, on Human's meshes.
- Asteroids: Small, Medium, Large.
- The Relay drawn with ResearchLab at tint 1.6, and the Repair Bay with Shipyard at tint 0.6.

| Set | Images | Tables (assets-table-1..3) | Verdict |
|---|---|---|---|
| Human | sheets/Human.png: all 21, plus Relay and RepairBay (viewed); CS, Shipyard and Lab level sheets | 21 rows, plus Relay and RepairBay rows | PASS |
| Tarkan | Tarkan.png and 3 level sheets | 21 rows, plus Relay and RepairBay | PASS |
| Pirate | Pirate.png and 3 level sheets | 21 rows | PASS |
| Wreck | Wreck.png: 3 hulls (viewed) | 3 rows | PASS. assets-textures says why the other 18 are absent: "only the three hull models are ever drawn as derelicts". That is right: GameClient.cpp:1628-1639 uses `ModelForHull` only. |
| Asteroids | Asteroids.png | 3 rows | PASS |

Spot checks against the NMF headers: Human Small 70 tris and 210 verts; Medium 160; CS_L4 828 and 2,484; Shipyard_L1 392; Tarkan Large 142; Constructor 266. All match. Hardpoint counts match the glTF sources.

---

## Check 5: Horizon, self-play and unbuilt Look tasks

The grep covered horizon, self-play, selfplay, ADR-063, galaxy, paint slot, two-tone, beacon, fleet plate, LV2.2, LV2.3, LV3.1, LV4, `--ai-matches` and `--quiet` over all 90 artboards. Each hit is one of:

- an exclusion statement (Main, assets-sheets, open-owner);
- battlefield-4's "Not built at HEAD" table, which marks LV2.2 to LV4.3 todo and matches ImplementationPlan-Look.md:48-53;
- a false positive: "40° below horizontal", the camera's horizon, or ADR-022's foreclosed "galaxy" band;
- developer switches listed as "not part of play" (game-loops:105);
- an ADR-065 measurement, which is the ADR's own.

Nothing out of scope is described as present. **PASS.**

---

## Check 6: Colors on screens

**How colors reach the screen.** The back buffer is R8G8B8A8_UNORM (Renderer.cpp:22), written through an `_SRGB` render-target view (Renderer.h:25, Renderer.cpp:658-661). UiPS.hlsl:4 says "The color is linear; the render target encodes it to sRGB". So a token's hex is the sRGB encoding of its linear constant.

**Constants.** I converted all 64 constants myself (`colors.py`): 51 in Hud.cpp, 8 in GameClient.cpp, 6 in CombatEffects.cpp and 3 in SceneShading.h. Every one equals the hex the reference prints.

**Screen colors.** `screen_colors.py` found 57 distinct (hex, alpha) pairs in the game layer of the frames. Every one maps to a code token:
- `#426391` is Human × 0.3, the entity-bar back (GameClient.cpp:98, 727). It is documented in sides.
- `#000000` is the cleared screen or the fog literal.
- `#FF2D95` appears only in the annotation layer, and in one annotation sentence. The frames say it is the reference's own. It is a saturated magenta that the game never draws, so it cannot be mistaken for a game color.

Each sampled use also matches the token's meaning: IDLE, +0/s and the newest alert as WARNING chips; enemy figures as plain ENEMY text; SAVED in QUEUE_EDGE; armed-order and picked faces in PICKED; locked cards in LOCKED with LOCKED_HATCH; and so on.

Problems:
1. `#1F2739` is both CARD_COLOR and CLOSE_BOX_COLOR (Hud.cpp:842, 988). These are two constants with identical values and are not declared aliases. On screen one hex therefore carries two meanings, and visual-colors and visual-rules list the two tokens without saying so. The other two shared hexes are declared aliases and are labelled as such: HOVER_EDGE = WINDOW_EDGE, and HEADER_LABEL = ROW_LABEL.
2. Translucent colors are written as CSS `rgba(sRGB hex, α)`, so the browser blends them in sRGB. The game blends in linear light (visual-colors itself says so; UiPipeline.cpp:110-116). On the minimap's ground, for example, the own-sector wash composites to about `#354B61` in the frame, but in linear light it is about `#445E76`. This affects the sector washes, outlines, hatching, the view outline and DISABLED_BUTTON_COLOR.
3. The minimap fog is drawn as a uniform `rgba(0,0,0,0.55)` over the whole map. The code shades each cell 0 when in sight, 0.55 when seen before, and 0.9 when never seen (FogOfWar.h:13-15). So the player's own home should be clear and the unexplored map darker.

The color page's three black swatches are counted under Check 8.

---

## Check 7: Sources present

| # | Artboard | Statement | Cited | Check | Verdict |
|---|---|---|---|---|---|
| S1 | economy-ore | paid when the job starts, nothing refunded, no cancel | MVP.md:150 | MVP.md:150 says exactly this | PASS |
| S2 | economy-ore | rig income formula, rounded to a hundredth | Simulation.cpp:2782-2793 | RigIncomeHundredthsPerSecond, 2784-2794 | PASS |
| S3 | research | topics 9 and 18 gone, ids not reused | ADR-033 §2 | ADR-033 decision 2: "identifiers are not reused"; names from Phase1.md:139, 148 | PASS |
| S4 | combat | pirates never leave their sector; placement throws | Simulation.cpp:828-831 | 828-831 throws | PASS |
| S5 | combat | stop point 40 m beyond the repairer's footprint | Simulation.cpp:86 | REPAIR_STANDOFF_METERS = 40, used at :2586 | PASS |
| S6 | ai | the AI plays player 2 as a client on the main thread | WinMain.cpp:425-437 | the loop feeds `match->ai->Update` | PASS |
| S7 | ai-difficulty | Normal is the default on the client and for `--ai` | GameClient.h:271; Main.cpp:125 | both say Normal | PASS |
| S8 | world | join file: host, port, certificate, player, token | JoinTicket.cpp:37-42 | ReadJoinTicket | PASS |
| S9 | world-state | header "OCWORLD\0" … FNV-1a | WorldState.cpp:132-140 | SAVE_KIND, FNV constants | PASS |
| S10 | world-state | newest 3 saves kept | WorldFolder.h:23 | SAVES_KEPT = 3 | PASS |
| S11 | orders | Shift+click on a selected ship takes it out | PlayerControls.cpp:267-286 | 281-282 | PASS |
| S12 | orders | Ctrl blocks A, H, T and S | PlayerControls.cpp:369-383 | `!_event.control` on each | PASS |
| S13 | economy-territory | tickets only on a map with territory, not in a world | Simulation.cpp:719-726 | `HasTerritory() && !m_worldRules` | PASS |
| S14 | ships-parts | no research raises armor or range | Tuning.cpp:364-376 | UPGRADE_PAIRS has no armor or range | PASS |
| S15 | sides | "linear and provisional until team colors are decided" | ModelCatalog.h:20-25 | ModelCatalog.h:21 | PASS |
| S16 | visual-colors | back buffer UNORM with an sRGB RTV | Renderer.cpp:22; Renderer.h:25 | both | PASS |
| S17 | visual-type | Consolas meets 600 with its bold | Hud.cpp:3005 | comment at 3005 | PASS |
| S18 | battlefield-1 | crosses 6–16 px | Starfield.cpp:38-39 | MIN/MAX_BURST_RADIUS_PIXELS | PASS |
| S19 | game-pitch | pitch and Phase 1 question quoted | MVP.md:9; Phase1.md:13 | verbatim | PASS |
| S20 | economy-ore | "bank sink or bound decided in Phase 6" | Phase5.md:137 | verbatim | PASS |

Notes. Some statements cross-refer to the makers' notes rather than the repository: "These are economy rules; see A1" (structures), "Research tree as A2 traced it" (research), "A1 match loop 4" (game-pitch). These sit beside primary citations. Derived tables cite `derive.py` and `designer_cards.py` together with the formulas' code lines. One path lacks its folder: ServerCertificate.cpp lives in NeuronCore/.

---

## Check 8: Readability

Method: every artboard was rendered with Playwright from disk, with `/_blob/` images mapped through assets.json. They were then rendered again with the pages' Google Fonts (Barlow, Barlow Semi Condensed, JetBrains Mono) loaded through the proxy.

- **Images.** All 24 `/_blob/` ids resolve to files, and every image loads.
- **Height.** No artboard's content is taller than its board.
- **Leakage.** No markdown or escapes leak (`**`, backticks, `&amp;`, `&lt;`, `](`, `undefined`, `NaN`); the one `**` hit is the name "xoshiro256**".

Defects:
1. **ships-parts.** The Hulls table is 1,590 px wide in a 1,440 px board, with web fonts loaded (1,739 px with fallbacks). Its "Unlocked by", "Mesh lengthMeters" and "Source" columns spill past the artboard edge (`render/ships-parts_0_700.png`).
2. **screens-flow.** Edge labels collide with the boxes and with each other (`render/F_screens-flow_60_300.png`):
   - "loaded (window shown)" reads "…d (window sh…";
   - "Easy / Normal / Hard skirmish" reads "/ Hard skirmi…";
   - "first snapshot" is cut;
   - "D P R F1 O, buttons" and "×, Esc, keys" are overprinted;
   - "no snapshot 2 s" is hidden behind a box.
3. **Step cards in game-loops and world.** Titles are cut mid-phrase and bodies begin with fragments (`render/F_world_steps.png`):
   - "Planned large orders of the last" and "Economy: (build";
   - bodies that begin ", then SeparateShips…";
   - "The owner hands the player the" and "The player joins again with the";
   - "The menu shows "Join world" ()." with empty parentheses.
4. **visual-colors, Scene light.** faceLightLookingUp, lineLightLookingUp and SCENE_BLOOM are drawn as black swatches labelled `#000000`. SCENE_BLOOM's meaning is empty, and it is a BloomDesc, not a color (GameClient.cpp:25) (`render/visual-colors-scene.png`).

---

## Ranked FAIL items

1. **Pirates in sectors no seed can give them.**
   - *Shown:* every in-match frame (27 of them, e.g. screen-hud-match-start.dc.html, screen-window-designer-default.dc.html) draws D2 and B4 as pirate-guarded (violet wash and outline). Both are of kind "between". The aside calls this "sample (seeded, ADR-072/073)", and the match-start minimap shows 2 guarded sectors.
   - *Right:* outposts go only on contested pairs (camps, 2 of the 4 pairs), on the rich pair E1/A5 and on the center C3 (strongholds). That makes 7 guarded sectors at the start of every match, never D2 or B4.
   - *Source:* Map.json:159-163; Map.cpp:420-440; ADR-072 decision 7.
2. **Status panel missing.**
   - *Shown:* no status panel in screen-window-designer-default, -designer-preview, -designer-unsaved-editing, -production-shipyard (Swarm at 62%) and -research-page1 (Hull Plating at 45%). Each shows a finished own Shipyard or Lab.
   - *Right:* a 380-wide panel at 16, 68 with the Lab line, or "Shipyards · BUILDING … · IDLE …" and the Fleet line.
   - *Source:* StatusLinesOf, Hud.cpp:281-414; drawn at Hud.cpp:3241-3308.
3. **Minimap missing 34 of the 40 ore asteroids.**
   - *Shown:* every in-match frame draws only the 6 home asteroids. The hovered derelict in hud-derelict-hint has no minimap mark either.
   - *Right:* all ore asteroids and fields are always in the snapshot and drawn, under the fog. That is 40 ore outlines, and any seen derelict too.
   - *Source:* Simulation.cpp:245-247; Hud.cpp:3486-3528.
4. **Wrong maximum hit points.**
   - *Shown:* screen-hud-busy-warships, -scale-110, window-orders-time-of-day and window-three-windows-default read "Hit points 3,860 / 4,320" for 6 Swarm, 4 Lancer and 2 Brawler.
   - *Right:* a maximum of 3,888 (6×198 + 6×450). With Hull Plating, which the Lab researching Large Hull implies, it is 4,472. 4,320 is the bare hull figure, without Ion's ×0.9.
   - *Source:* DesignStats.h:86; Tuning.json:16-17, 21; Hud.cpp:2769-2770.
5. **Victory by domination with the enemy holding 731 tickets.**
   - *Shown:* screen-hud-end-victory: "Victory", "By domination.", while the territory panel shows 642 : 731 and "You -40 a minute".
   - *Right:* a domination win means the enemy reached 0 tickets.
   - *Source:* Simulation.cpp:3796-3804; Hud.cpp:2205-2208.
6. **A design name that cannot be typed.**
   - *Shown:* screen-window-designer-unsaved-editing: "Lancer with the long range sensor fit_", "37/32", "too long for the server".
   - *Right:* typing stops at 32 characters, so the counter cannot pass 32/32. A name is refused only when it is empty or all spaces.
   - *Source:* Designer.cpp:236; Command.h:68-84.
7. **Research prerequisite contradiction.**
   - *Shown:* screen-hud-lab-needs-research: "Researching Fusion Drive · 35%" beside "L2 needs Improved Extraction and Hull Plating".
   - *Right:* Fusion Drive requires Hull Plating, so Hull Plating is already researched. The line reads "L2 needs Improved Extraction".
   - *Source:* Tuning.json:74; Simulation.cpp:3181-3189; Hud.cpp:2678-2687.
8. **Node cap note after the station falls.**
   - *Shown:* screen-hud-end-empire-fallen: "Nodes of 25".
   - *Right:* "Nodes cap 4 of 25". A player without a station keeps level 1's node cap.
   - *Source:* Simulation.cpp:1116, 2912-2920; Hud.cpp:3194-3196.
9. **Scene-light swatches broken.**
   - *Shown:* visual-colors.dc.html, "Scene light and effects": faceLightLookingUp `#000000`, lineLightLookingUp `#000000`, and SCENE_BLOOM `#000000` with an empty meaning.
   - *Right:* `#F3F1F7`, the light (0.895, 0.877, 0.932); `#F2F2F2`, the brightness 0.8846; and SCENE_BLOOM is not a color (threshold 0.35, knee 0.15, intensity 2, spread 0.6).
   - *Source:* SceneShading.cpp:7-25; GameClient.cpp:25.
10. **Hulls table overflows its artboard.**
    - *Shown:* ships-parts.dc.html: the Hulls table is about 1,590 px wide in a 1,440 px artboard, so its last three columns spill outside.
    - *Right:* the table should fit 1,440 px.
    - *Source:* rendered with the page's web fonts (`render/ships-parts_0_700.png`).
11. **Flow diagram labels collide.**
    - *Shown:* screens-flow.dc.html: edge labels are cut or overprinted ("…d (window sh…", "/ Hard skirmi…", "D P R F1 O, buttons" over "×, Esc, keys", "no snapshot 2 s" hidden).
    - *Right:* every label should be readable.
    - *Source:* `render/F_screens-flow_60_300.png`.
12. **Step-card titles truncated.**
    - *Shown:* game-loops.dc.html (tick loop) and world.dc.html (Join → play): "Planned large orders of the last", "Economy: (build", bodies opening with ", then …", "The owner hands the player the", "The player joins again with the", and "The menu shows "Join world" ()."
    - *Right:* titles should be whole phrases, with no empty "()".
    - *Source:* `render/F_world_steps.png`.
13. **Minimap blending and fog.**
    - *Shown:* every in-match frame blends translucent colors (sector wash, outline, hatch, view outline) in sRGB through CSS rgba, and draws the fog as a uniform 55% black.
    - *Right:* the game blends in linear light (own wash ≈ `#445E76`, against ≈ `#354B61` drawn). Fog is 0 in sight, 0.55 for cells seen before, and 0.9 for never seen.
    - *Source:* UiPipeline.cpp:110-116; Renderer.h:25; FogOfWar.h:13-15.
14. **Fractional font sizes in scaled frames.**
    - *Shown:* screen-hud-scale-110 and screen-menu-scale-125 use 17.6, 15.4, 14.3, 24.2 and 27.5 px.
    - *Right:* faces are rasterized at round(units × scale): 18, 15, 14, 24 and 28 px. visual-type states the rule itself.
    - *Source:* UiPipeline.cpp:28-31.
15. **Load and stress runs described as against the AI.**
    - *Shown:* screens-flow.dc.html: "--measure, --load, --stress → Match (skirmish against the Normal AI)".
    - *Right:* only `--measure` plays the Normal AI. Under `--load` the rival is a load driver, and `--stress` orders its own scene. No AI is made in either.
    - *Source:* WinMain.cpp:329-334 (`if (!load && !stress)`).
16. **One hex, two meanings, unflagged.**
    - *Shown:* visual-colors and visual-rules list CARD_COLOR and CLOSE_BOX_COLOR as two tokens with two meanings, both `#1F2739`, without noting that they share a value. They are not declared aliases.
    - *Right:* flag the shared value. On screen, a `#1F2739` face can mean either.
    - *Source:* Hud.cpp:842, 988.
17. **Building-site bar sample inconsistent** (low).
    - *Shown:* screen-hud-busy-warships and window-three-windows-default show a structure at 40% built with its health bar at 90%.
    - *Right:* a site starts at 1/10 of its hit points and gains the rest with the work, so it is at about 46% (amber) when 40% built, unless it was repaired.
    - *Source:* Simulation.cpp:71, 2449-2456.

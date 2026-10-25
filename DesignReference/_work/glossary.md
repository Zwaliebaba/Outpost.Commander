# Glossary

Agent **R** (reconciliation). HEAD `a053fe294d91dc885bbb061c2f34d76ca6897738` (2026-10-25T08:33:55+01:00).

**How terms are spelled.** Each term is written exactly as the game writes it on screen or in its data, with capitals as shown. A term that never appears on screen is in *italics* and gives its code or document name. "Variants" lists other spellings in the design documents, the code or the brief.

**Sources.** `Tuning.json`, `Map.json`, `Models.json` and `Opponent*.json` are under `OutpostCommander/Assets/`. Code paths are from the repository root. `MVP.md`, `Phase1.md` to `Phase4.md` are in `GameDesign/Archive/`, and `Phase5.md` is `GameDesign/OutpostCommander-Phase5.md`.

**Spelling rule.** The game's own nouns keep British spelling in data and on screen: **Defence**, **Armour**. Identifiers use the SDK's American spelling: `DefensePlatform`, `armor` (AGENTS.md R11). Where a screen label is an identifier-style word, it is American: the Designer's "Armor" bar (`Hud.cpp:767`).

---

| Term | Definition | Source | Variants |
|---|---|---|---|
| **25%**, **50%**, **Never** | The three retreat settings on the selection panel: retreat below a quarter or half of full hit points, or never. 25% is the default. In the Designer they read `25%`, `50%`, `NEVER`. | `Hud.cpp:470-476, 1557-1564`; `GameProtocol/RetreatThreshold.h:7-30` | code `Quarter`, `Half`, `Never` |
| **A Mining Rig is lost** | A scheduled order's trigger: a Mining Rig in the watched sector is destroyed. | `OrderForm.cpp:244`; `Simulation.cpp:3342-3343` | in words "when a Mining Rig in {sector} is lost" |
| **A Relay is threatened** | A scheduled order's trigger: the watched sector's Relay is suppressed or hit. | `OrderForm.cpp:242`; `Simulation.cpp:3340-3341` | |
| **A1** … **E5** | The 25 sectors' names: columns A–E west to east, rows 1–5 south to north. A1 is player 1's home and E5 player 2's. | `Map.json: sectors[].name` (lines 98-149) | |
| **Ablative Armour** | Tier 3 research topic 20: hull hit points +5%. Requires Composite Plating. | `Tuning.json:100-101` | code `armor` |
| *AI empire* | An AI that plays a world's seat on the server's thread, at Easy, Normal or Hard. | `OutpostServer/Main.cpp:195` ("Player {n} is an AI empire"); `ADR-079` decision 3 | |
| **Alerts** | The HUD's list of up to 4 events of the last 8 s, newest first; the newest is a warning chip. **Space** looks at the newest. | `Alerts.h:15-18`; `Hud.cpp:3215-3238`; `KeyBindings.h:115` | |
| **Alt, held** | Shows every health bar. | `KeyBindings.h:123` | |
| **ARM** | Armor, as the Designer's cards write it ("ARM 14"). | `Hud.cpp:651, 796` | |
| **Armor** | Flat damage taken off each hit, down to a quarter of the hit. Integer hundredths in the simulation. | `DesignStats.h:16-21`; Designer bar `Hud.cpp:767` | design docs "armour"; code `armor` |
| **Asteroid** | An ore asteroid in the Orders window's target list ("Asteroid, Ore {n}"). | `OrderForm.cpp:274-275` | *ore asteroid* |
| *asteroid field* | A cluster of rocks that blocks movement, radius 150 m or 180 m; 80 on the map. | `Map.json: asteroidFields` (lines 16-97); `ADR-087` | "field" |
| **At a time of day** | A scheduled order's trigger: fires at an hour and minute on the player's clock, once. | `OrderForm.cpp:238`; `InProcessServer.cpp:338-351` | in words "at {HH:MM}" |
| **Attack** | Order: close on one enemy ship or structure and fire until it dies. Right-click on an enemy. Also a scheduled-order action. | `Command.h:15-20`; `OrderForm.cpp:258` | "attack in {sector}" |
| **Attack-move** | Order: move, stopping at weapon range to fire at any enemy met. **A**, then click. | `KeyBindings.h:99`; `Hud.cpp:2798`; `Command.h:22-27` | "attack-move to {sector}" |
| **Automated Shipyards** | Tier 1 topic 8: Shipyard build speed +25%. Requires Improved Extraction. | `Tuning.json:80-81` | |
| **Away for** | The Away window's first line: how long the deputy held the seat. | `Hud.cpp:2297` | |
| *away report* | What happened while the deputy played the seat, handed over once on return: built, lost, sectors gained and lost, fired orders. | `GameLogic/AwayReports.cpp`; `Snapshot.h:454-502`; `ADR-080` decision 7 | |
| **Back to menu** | The end banner's button. It ends the match's server and shows the menu. | `Hud.cpp:3349`; `WinMain.cpp:491-504` | |
| *banner* | The end-of-match panel: Victory, Defeat, Draw or Empire fallen. | `Hud.cpp:2185-2219, 3340-3350` | |
| *battle matchup* | A developer setup: two fixed fleets with no bases (`--matchup`, `--matchups`). Not part of play. | `Matchups.json`; `ADR-083` | |
| *battle* (world log) | Two seats firing at each other in one sector within 10 s, ended by 30 s without such a shot. A `World.log` record. | `WorldLog.h:55-57`; `ADR-082` decision 2 | |
| *between* | Sector kind with 2 contested asteroids: D2 and B4. | `Map.json:155` | |
| **Brawler** | Starting design: Medium + Ion + Mass Driver. It is also the AI's default design. | `Tuning.json:116`; `Opponent.json:17` | |
| **BUILD** | The production window's label over its cards. | `Hud.cpp:1730` | |
| **Build** | The Designer's build-time bar, in s. | `Hud.cpp:779` | |
| **Build a Mining Rig** | Scheduled-order action given to Constructors: a rig on the picked asteroid. | `OrderForm.cpp:264` | "Mining Rig in {sector}" |
| **BUILT** | The Designer header's count of ships the targeted Shipyard has built ("BUILT · n"). | `Hud.cpp:1372`; `Snapshot.h:109-112` | |
| **By domination.** | Banner detail when the match ended by domination. | `Hud.cpp:2207` | |
| *camp* | A pirate outpost of 1 Defence Platform and 3 Small Ion Mass Driver ships, on 2 of the 4 contested pairs. Its wreck pays 600 Ore. | `Tuning.json:123-124`; `Map.json:160` | |
| **cap** | The node cap in the territory panel ("cap 4 of 25"). | `Hud.cpp:3195` | |
| *center* | Sector kind of C3: 4 contested asteroids and a pirate stronghold. | `Map.json:156, 162` | "centre" not used |
| **Coilgun Mass Drivers** | Tier 3 topic 21: Mass Driver fire rate +10%. Requires Mass Driver Calibration. | `Tuning.json:102-103` | |
| *command log* | A world's per-save log of every tick's commands, `Commands-<tick>.log`. | `WorldFolder.cpp:12-23`; `ADR-077` decision 7 | |
| *command points* | A warship's weight under the fleet cap: Small 1, Medium 2, Large 4. Written **CP** on screen. | `Tuning.json:16-18`; `ADR-071` | **CP** |
| **Command Station** | A player's starting base on its start. It cannot be built. It holds the home sector, builds Constructors, repairs ships and carries Defence guns. Five levels: node cap 4/7/10/13/16 and fleet cap 12/20/30/40/50. | `Tuning.json:40-46`; `ADR-064`; `ADR-071` | code `CommandStation`; "COMMAND STATION" (production subject, `Hud.cpp:2845`) |
| **Composite Plating** | Tier 2 topic 13: hull hit points +15%. Requires Hull Plating. | `Tuning.json:88-89` | |
| *condition* | A scheduled order's optional "unless": hold instead when the enemy command points seen in the action's sector exceed a number. | `Simulation.cpp:3382-3397`; `Phase5.md:116` | **UNLESS** |
| **Constructor** | The one fixed unarmed design. It builds, repairs and salvages. 60 Ore and 15 s at the Command Station; 2 at start. | `Tuning.json:35`; `Hud.cpp:164` | |
| *contested* | Sector kind with 1 contested asteroid (5.5 Ore/s, 12,000 Ore). Also that yield's name. | `Map.json:154`; `Tuning.json:8` | |
| *control group* | Ctrl+0–9 assigns a selection; 0–9 recalls it, and a second tap looks at it. | `KeyBindings.h:103-104` | "Make a control group" |
| **Controls** | The window listing every key and mouse action (**F1**), titled "CONTROLS". | `KeyBindings.h:93-125`; `Hud.cpp:1906` | "These controls" |
| **Cost** | The Designer's cost bar ("ore" unit). Elsewhere costs are the gem and a gold figure. | `Hud.cpp:778`; `ADR-043` decision 3 | |
| **CP** | Command points, in the Orders window ("Over {n} enemy CP there"). | `Hud.cpp:2273`; `OrderForm.cpp:313` | *command points* |
| **cut off** | A held sector with no path of held sectors to its holder's home. Its rigs earn half. | `Hud.cpp:2608`; `Simulation.cpp:2864-2891`; `Tuning.json:14` | code `cutOff` |
| **DAMAGE / S AFTER ARMOR · VS A FORMATION** | The Designer's damage section: one card per hull. | `Hud.cpp:1510` | mockup "VS HULL" |
| *dedicated server* | `OutpostServer`, the console program that runs a world from its folder. | `ADR-078` | |
| **Deep Core Survey** | Tier 2 topic 12, shown as "Asteroid ore reserves +30%". The researcher's rigs draw a 1.3rd less from a reserve. | `Tuning.json:86-87`; `Research.cpp:167-168`; `ADR-035` decision 3 | |
| **Defeat** | Banner title for the loser. | `Hud.cpp:2199` | |
| **Defence Autoloader** | Tier 2 topic 15: Defence gun fire rate +20%. | `Tuning.json:92-93` | |
| **Defence gun** | The structure weapon: 30 damage every 1.0 s at 250 m. The Command Station carries 1–3 and the Defence Platform 1. | `Tuning.json:37`; `Hud.cpp:2659` ("{n} Defence guns") | code `DefenseGun`; brief "Defense" |
| **Defence Platform** | A structure with one Defence gun: 450 Ore, 20 s, 1,500 hit points, armor 10. | `Tuning.json:61` | code `DefensePlatform`; brief and A6/A7 prose "Defense Platform" (C-065) |
| *deputy* | The keeper that plays a player's seat 60 s after the connection is lost, until the player returns. | `GameProtocol/Server.h:32`; `Opponent/Deputy.cpp`; `ADR-079`; `World.log` "deputy" | |
| **Derelict** | A wreck of the old fleet with no owner or hit points. Constructors salvage it for Ore, and some for research time. | `Hud.cpp:2137`; `ADR-074` | set **Wreck** |
| *design* | A saved hull + drive + weapon (+ module), named. Its stats are derived. | `ADR-014`; `ADR-017` | |
| **DESIGNER** | The ship designer window's title-bar label ("SHIPYARD 01 · DESIGNER"). | `Hud.cpp:1368` | **Ship designer**, **SHIP DESIGN** |
| **DO** | The Orders window's action row. | `Hud.cpp:2258` | |
| *domination* | A match's ticket race: each player loses 1 ticket every 3 s per node it is behind; at 0 it loses. Off in a world. | `Tuning.json:14`; `ADR-057`; `ADR-084` decision 2 | **By domination.** |
| **Draw** | Banner title when both players lose in one tick. | `Hud.cpp:2199` | |
| **DRIVE** | The Designer's drive slot: Ion, Fusion or Pulse. | `Hud.cpp:658`; `Tuning.json:21-23` | |
| **Drive Harmonics** | Tier 3 topic 23: ship speed +10%. | `Tuning.json:106-107` | |
| *dry* | An ore asteroid whose reserve is gone. Its rig earns a 20% trickle. Drawn in rust on the minimap. | `Tuning.json:10`; `Hud.cpp:34` | "Ore run out: it earns a trickle" |
| **Easy skirmish**, **Normal skirmish**, **Hard skirmish** | The menu's three matches against the AI at each difficulty. | `Hud.cpp:2329-2331`; `ADR-065` | |
| **Empire fallen** | A world's banner when the player has lost its production; the seat restarts an hour later. | `Hud.cpp:2193-2195`; `ADR-084` decision 7 | code `EmpireLost` |
| **Enemies enter a sector** | A scheduled order's trigger. | `OrderForm.cpp:240` | "when enemies enter {sector}" |
| **Enemy ships in** {sector} | Alert: an enemy warship entered a sector the player holds. | `Alerts.cpp:89` | |
| *exhaust* | A drive's glow at each `exhaust` hardpoint: Ion cyan, Fusion magenta, Pulse lime. | `Models.json:75-80`; `ADR-019` | |
| **field** ("the field") | The place an alert names when its event is in no sector. | `Alerts.cpp:73` | |
| **Flak Battery** | Weapon: 20 every 0.5 s, 200 m, splash 26 m. Unlocked by tier 2 topic 11 of the same name. | `Tuning.json:29, 84-85` | |
| *flank* | Sector kind beside a home, with 2 near asteroids: B1, A2, D5, E4. | `Map.json:152` | |
| **Fleet** | The status column's line of command points against the fleet cap ("29 / 30"). | `Hud.cpp:376-379` | |
| **Fleet Automation** | Tier 3 topic 24: Shipyard build speed +25%. Requires Automated Shipyards. | `Tuning.json:108-109` | |
| *fleet cap* | The most command points a player's warships, and their started jobs, may take: set by the Command Station's level. | `ADR-071`; `Hud.cpp:383, 2620` | "the fleet cap", "fleet {n}" |
| *fog of war* | What a player cannot see is hidden. Enemy structures are remembered as last seen. | `ADR-024`; `GameApp/FogOfWar.h` | |
| *footprint* | The circle a ship or structure keeps clear; structures are drawn across it. | `Tuning.json` `footprintRadiusMeters`; `GameClient.cpp:1658-1664` | |
| **Fusion** | Drive: speed ×0.8, hit points ×1.4, turn ×0.8. Unlocked by tier 1 topic 5, **Fusion Drive**. | `Tuning.json:22, 74-75` | |
| **Give to** {n} **ship(s)** | The Orders window's button that gives the form's order to the selection. | `Hud.cpp:2286` | "Give the order" |
| *guarded* | A sector with a pirate structure in it: it cannot be claimed and its node counts for no one. | `Simulation.cpp:2853-2860`; `ADR-073` decision 6 | |
| **held** | A sector whose node a player's finished Relay or Command Station holds. | `Hud.cpp:2604-2610` | "not held" |
| **Hit points** | Health; integer hundredths in the simulation, shown whole and rounded up. | `Hud.cpp:166-170, 764, 2587` | **HP** on cards |
| **Hold sector** | Standing order: hold the sector clicked in, attacking enemies seen there and returning to its node. **H**, then click. | `Hud.cpp:2800`; `KeyBindings.h:100` | "Hold the sector" (form), "Holding a sector" (panel), "hold {sector}" |
| *home* | Sector kind of each start, with 3 fixed asteroids (3.5 Ore/s, 7,500 Ore). Never suppressed. | `Map.json:9-14, 151`; `ADR-056` decision 6 | |
| **HP** | Hit points on a hull card ("1,200 HP"). | `Hud.cpp:651` | |
| **HULL** | The Designer's hull slot: Small, Medium or Large. | `Hud.cpp:639`; `Tuning.json:16-18` | |
| **Hull Plating** | Tier 1 topic 2: hull hit points +15%. | `Tuning.json:68-69` | |
| *Human* | Model set and side of player 1: blue (0.18, 0.42, 0.95), own meshes. | `Models.json:4-17, 54` | |
| **IDLE** | A warning chip: the Research Lab, or a Shipyard that could start a ship now, is doing nothing. | `Hud.cpp:297, 404` | |
| **Improved Extraction** | Tier 1 topic 1: Mining Rig income +25%. | `Tuning.json:66-67` | |
| *income* | Ore a second from the player's rigs, shown "+{n}/s"; a warning chip at nothing. | `Hud.cpp:3146-3156` | |
| **Ion** | The starting drive: speed ×1.3, hit points ×0.9, turn ×1.25. | `Tuning.json:21` | |
| *join file* | `Join-Player<n>.json`, written by the server: host, port, certificate, player, token. | `JoinTicket.cpp:37-42`; `ADR-078` decision 8 | |
| **Join world** | Menu button, shown when `Documents\Outpost Commander\Join.json` exists. It joins a dedicated server's world. | `Hud.cpp:2333`; `WinMain.cpp:56-57, 111-123` | `--join` |
| **L1** … **L5** | Structure levels ("· L2", "Upgrade to L3"). The Command Station goes to L5, the Shipyard to L3, the Research Lab to L4. | `Hud.cpp:2572, 2689`; `Tuning.json:41-59` | *level* |
| **Lance** | Weapon: 95 every 2.7 s, 220 m. It fires a beam in the side's color. | `Tuning.json:27`; `Models.json:82` | |
| **Lance Capacitors** | Tier 2 topic 16: Lance fire rate +10%. Requires Lance Focusing. | `Tuning.json:94-95` | |
| **Lance Focusing** | Tier 1 topic 4: Lance fire rate +15%. | `Tuning.json:72-73` | |
| **Lancer** | Starting design: Medium + Ion + Lance. | `Tuning.json:117` | |
| **Large** | Hull 3: 1,200 hit points, armor 14, 4 command points. Built at a Shipyard L3. Unlocked by tier 1 topic 6, **Large Hull**. | `Tuning.json:18, 76-77` | |
| **last seen** | How long ago a remembered structure or derelict was seen ("last seen 4:12 ago", "last seen earlier"). | `Hud.cpp:452-460` | |
| **Local skirmish** | The "where the match runs" tag in a match on the game's own server. | `Hud.cpp:3323`; `ADR-086` | |
| **Mass Driver** | The starting gun weapon: 14 every 0.4 s, 120 m. | `Tuning.json:26` | |
| **Mass Driver Calibration** | Tier 1 topic 3: Mass Driver fire rate +10%. | `Tuning.json:70-71` | |
| *match* | A skirmish against the AI. It ends by lost production or domination; the world runs on behind the banner. | `ADR-020` decision 9; `ADR-057` | **Match length** |
| **Medium** | Hull 2: 500 hit points, armor 8, 2 command points. Built at a Shipyard L2. | `Tuning.json:17, 51` | |
| *memory* | A structure or derelict the player saw and no longer sees, drawn as lines only over the fog. | `ADR-024` decision 5; `ADR-081` decision 4 | "when seen" |
| *minimap* | The HUD's 300-unit map at the bottom left. | `Hud.cpp:3456-3630` | |
| **Mining Rig** | A structure on an ore asteroid in a held sector that pays its Ore income: 50 Ore, 10 s. | `Tuning.json:60` | code `MiningRig`; docs "rig" |
| **Mining Rig lost** | Alert: one of the player's rigs was destroyed. | `Alerts.cpp:86` | |
| **Missile Guidance** | Tier 2 topic 17: Missile Rack fire rate +15%. Requires Missile Rack. | `Tuning.json:96-97` | |
| **Missile Rack** | Weapon: 30 every 2.0 s, 280 m, splash 30 m. Unlocked by tier 1 topic 7 of the same name. | `Tuning.json:28, 78-79` | |
| **MIXED** | The retreat row when selected ships differ. | `Hud.cpp:3449` | |
| **MODULE** | The Designer's optional fourth slot: None or Sensor Array. | `Hud.cpp:695`; `ADR-058` | |
| **Move** | Order: head for a point in formation. Right-click the ground. Also a scheduled-order action. | `Command.h:8-13`; `OrderForm.cpp:254` | "move to {sector}" |
| *near* | Sector kind with 1 near asteroid (4 Ore/s, 9,000 Ore). Also that yield's name. | `Map.json:153`; `Tuning.json:7` | |
| **NEEDS** | What a research topic lacks first ("NEEDS · RESEARCH LAB L2", "+ HULL PLATING"). | `Hud.cpp:1875, 2972` | "NEEDS RESEARCH" (upgrade), "Needs Shipyard L{n}" (Designer), "NEEDS L{n}" (production) |
| **No drain** | The territory panel when both sides hold as many nodes. | `Hud.cpp:2513` | |
| **No word from the server** | Warning chip after 2 s without a snapshot ("· {n} s"). | `Hud.cpp:3334`; `ADR-086` decision 2 | |
| *node* | The point at a sector's center that a Relay, or a Command Station, holds. | `Map.json: sectors[].node`; `ADR-036` decision 4 | |
| **NODE CAP** | A dim Relay button's note at the node cap. | `Hud.cpp:2825` | "node cap" |
| **Nodes** | The territory panel's row of nodes held, own against enemy. | `Hud.cpp:3196` | *node* |
| **On patrol** | Selection line for ships on a patrol. | `Hud.cpp:2787` | |
| **ONE PER PLAYER** | The Research Lab button's note: a player may have one. | `Hud.cpp:2824` | |
| **Order fired** / **Order held back** / **Order refused** | Alerts when a scheduled order fires, holds instead under its condition, or is refused. | `Alerts.cpp:38-44` | |
| **Ore** | The one resource, mined by rigs and paid when a job starts. Written as a gold gem and figure. | `MVP.md:146`; `ADR-043` decision 3; `Hud.cpp:3145` | "ore" (Cost bar unit, `Hud.cpp:778`) |
| *Ore box* | The Ore figure in the designer, production and research windows' headers. | `Hud.cpp:1244-1249` | |
| **Ore left** | A Mining Rig's remaining reserve as the player knows it. | `Hud.cpp:2594` | |
| *outpost* | A pirate camp or stronghold on a node; 7 on the map. | `Map.json:159-163`; `ADR-073` | |
| **Outpost Commander** | The game's title on the menu. In the lore, the player's role. | `Hud.cpp:2346`; `MVP.md:19` | |
| *OutpostServer* | The dedicated server program: `--new-world <folder>`, then `<folder>`. | `OutpostServer/Main.cpp:28-35` | |
| **Patrol** | Standing order: go between the ships' middle and the point clicked. **T**, then click. | `Hud.cpp:2801`; `KeyBindings.h:101` | "On patrol", "patrol to {sector}" |
| **PERFORMANCE** | The Designer's stat-bar section. | `Hud.cpp:1487` | |
| **Picket** | Starting design: Small + Ion + Lance. | `Tuning.json:115` | |
| **Pirate** | Neutral owner of outposts, every player's enemy. Model set violet (0.62, 0.22, 0.9) on Tarkan meshes. On screen: "Pirate ships in", "Pirates cleared", "Pirate {name}". | `Id.h:25`; `Models.json:33-37, 57`; `Alerts.cpp:89, 95`; `OrderForm.cpp:288` | code `PIRATES` |
| *placement ghost* | The structure's level-1 model and band ring under the cursor, green where it may stand and red where not. | `GameClient.cpp:122-123, 1807-1835` | |
| **Placing** | Hint while a structure placement is armed ("Placing {structure}: left-click to build, right-click to cancel"). | `Hud.cpp:2551` | *placement ghost* |
| **PRODUCTION** | The production window (**P**): one producer's queue and the cards to add to it. | `Hud.cpp:1725`; `KeyBindings.h:118` | **Production** button |
| **Proximity Fuses** | Tier 3 topic 22: Flak Battery fire rate +15%. Requires Flak Battery. | `Tuning.json:104-105` | |
| **Pulse** | Drive: speed ×1.6, hit points ×0.75, turn ×1.5. Unlocked by tier 2 topic 10, **Pulse Drive**. | `Tuning.json:23, 82-83` | |
| **QUEUE** | A producer's or Lab's queue, 5 jobs ("QUEUE · 3 / 5"). Also the Designer's commit button. | `Hud.cpp:827, 1045, 1677`; `Snapshot.h:42` | |
| **Quit** | Menu button: closes the game. | `Hud.cpp:2334` | |
| **Rail Cannon** | Weapon: 320 every 6.0 s, 240 m. It fires a white slug. Unlocked by tier 3 topic 19 of the same name. | `Tuning.json:30, 98-99`; `Models.json:83` | |
| **Range** | The Designer's weapon-range bar. Range is measured center to center. | `Hud.cpp:770`; `ADR-014` decision 7 | |
| **Rapid Construction** | Tier 3 topic 25: Constructor build and repair rate +25%. | `Tuning.json:110-111` | |
| **Reinforced Structures** | Tier 2 topic 14: structure hit points +25%. | `Tuning.json:90-91` | |
| **Relay** | A structure that claims its sector's node: 200 Ore, 40 s, 3,000 hit points. It is drawn with the Research Lab's model at tint 1.6. | `Tuning.json:62`; `Models.json:70`; `ADR-056` | |
| *Relay Archives*, *Precursor Vault* | Phase 1's gateway topics 9 and 18, removed; their IDs are not reused. | `ADR-033` decision 2 | not in the game |
| **Relay suppressed** / **Relay under attack** | Alerts about the player's Relay. | `Alerts.cpp:79, 82` | |
| **RENAME** / **UPDATE** / **SAVE** / **SAVED** | The Designer's buttons: save new picks, rename or update a saved design; SAVED when the picks are one. | `Hud.cpp:600-609` | |
| **Repair** | A Constructor's right-click on a damaged or unfinished own ship or structure. It costs nothing. | `KeyBindings.h:98`; `Command.h:43-49` | |
| **Repair Bay** | A structure that repairs up to 4 of its player's ships within 150 m at 3% a second: 300 Ore, 40 s. Drawn with the Shipyard model at tint 0.6. | `Tuning.json:63, 131`; `Models.json:71`; `ADR-075` | code `RepairBay` |
| **RESEARCH** | The research window (**R**): the Lab, its queue, and every open topic by tier. | `Hud.cpp:1825`; `KeyBindings.h:119` | |
| **Research Lab** | A structure that researches topics, one per player: 200 Ore, 30 s. Its levels open tiers 2 and 3 and, at L4, a second slot. | `Tuning.json:54-58` | code `ResearchLab`; "RESEARCH LAB" |
| *reserve* | The Ore an ore asteroid holds; a rig draws on it. It never refills. | `Map.json`; `ADR-035` | **Ore left** |
| *restart* | In a world, a lost seat's new Command Station and 2 Constructors on its start, an hour after the loss. | `Simulation.h:343`; `ADR-084` decision 4 | "It restarts at your start in …" |
| **RETREAT** | The retreat row: 25%, 50%, Never. | `Hud.cpp:1561, 3447` | |
| **Retreating to be repaired** | Selection line for a ship going back to its repairer. | `Hud.cpp:2789-2790` | **Ship retreating** (alert) |
| *rich* | Sector kind of E1 and A5: 2 rich asteroids (6.5 Ore/s, 24,000 Ore, r 60 m) and a stronghold. | `Map.json:157, 161`; `Tuning.json:9` | |
| *salvage* | A Constructor's 30 s work on a derelict: its Ore at once, and 50% of a named topic's time. | `Tuning.json:130`; `ADR-074` | |
| **SALVAGED** | A research card's tag when part of its time was recovered from a derelict. | `Hud.cpp:2979` | "salvaged {p}%" (hint) |
| *save* | A world's state, written every 60 s; the newest 3 are kept. | `WorldFolder.h:15, 23`; `ADR-077` | |
| *scheduled order* | An order given now that fires once on a trigger, with at most one condition. | `ADR-080`; `Phase5.md:107` | |
| **Scheduled orders** | The Orders window (**O**), titled "SCHEDULED ORDERS". | `KeyBindings.h:120`; `Hud.cpp:1952` | **Scheduled:** {order} (selection line) |
| *scout* | The AI's Small + Ion + Mass Driver + Sensor Array explorer. | `Opponent.json:44-45` | |
| *seat* | One of a world's player places. Taken with a token; played by the player or its deputy. | `ADR-078` decision 3; `SeatController.h` | "The server refused the seat" |
| *sector* | One of 25 squares of 2 km, held through its node. Kinds: home, flank, near, contested, between, center, rich. | `Map.json:98-157`; `ADR-036` | **Sectors gained**, **Sectors lost** |
| *selection ring* | The 3-px ring round each selected ship or structure: green, amber while an order is armed. | `GameClient.cpp:89-90`; `ADR-067` | |
| **Sensor Array** | The one module: sight 700 m, speed ×0.9, 120 Ore. Shown as the **Sensors** bar. | `Tuning.json:33`; `Hud.cpp:775` | "SA", "SIGHT" |
| **Server** {host}:{port} | The tag when joined to a dedicated server's world. | `Hud.cpp:3323`; `ADR-086` | |
| **Ship designer** | Button and Controls name of the Designer (**D**). | `Hud.cpp:2636`; `KeyBindings.h:117` | **DESIGNER**, **SHIP DESIGN** |
| **Ship retreating** | Alert: a ship of the player's started back to be repaired. | `Alerts.cpp:92` | |
| **Shipyard** | A structure that builds warships, in a held sector: 300 Ore, 40 s. L1 builds Small, L2 adds Medium, L3 adds Large. Numbered 01, 02 … | `Tuning.json:48-52`; `Hud.cpp:591, 2845` | "SHIPYARD 01" |
| **Shipyards** | The status column's line ("Shipyards · BUILDING b · WAITING w · IDLE i"). | `Hud.cpp:403-404` | |
| *skirmish* | A match against the AI on the game's own server. | `Hud.cpp:2329-2331, 2347` | |
| **Small** | Hull 1: 220 hit points, armor 2, 1 command point. Built at a Shipyard L1. | `Tuning.json:16` | |
| **Speed** | The Designer's speed bar, in m/s. | `Hud.cpp:769` | **SPD** (drive card) |
| **splash** | A weapon's area: every other enemy within the radius of the target takes the hit. Shown as "splash {r}m". | `Hud.cpp:688`; `ADR-014` decision 12 | |
| *standing order* | Hold sector or Patrol, kept by the server until the player gives a move, attack-move, attack or stop. | `ADR-059` | |
| **Stop** | Order: drop orders and hold. **S**. | `Hud.cpp:2802`; `KeyBindings.h:102` | |
| *stronghold* | A pirate outpost of 2 Defence Platforms and 4 Medium ships, on the rich pair and the center. Its wreck pays 1,200 Ore. | `Tuning.json:125-127`; `Map.json:161-162` | |
| **suppressed** | A Relay with an enemy warship within 400 m and none of its owner's. Its sector earns nothing. | `Hud.cpp:2606`; `Tuning.json:14`; `ADR-056` decision 6 | |
| **Swarm** | Starting design: Small + Ion + Mass Driver. | `Tuning.json:114` | |
| **TARGET** | The Orders window's row for an attack's target or a rig's asteroid. | `Hud.cpp:2265` | |
| *Tarkan* | Model set and side of player 2: orange-red (0.9, 0.25, 0.12), own meshes. In the lore, the Tarkan High Command. | `Models.json:19-31, 55`; `MVP.md:23` | |
| *Terrakin* | In the lore, the colonies on the far side of the dark jump relays. The premise is a proposal. | `MVP.md:15-17` | |
| *territory* | Sectors held through their nodes; it drives income and domination. | `ADR-056` | |
| **Tickets** | The territory panel's domination tickets, 1,000 each at the start of a match. | `Hud.cpp:3200`; `Tuning.json:14` | |
| **TIER** | A research topic's tier 1–3, opened by the Lab's level ("TIER 2 · 90 s"). | `Hud.cpp:2978` | "tier {n}" |
| **Top level** | A structure at its kind's last level. | `Hud.cpp:2699` | |
| *trickle* | An exhausted asteroid's rig earns 20% of its rate. | `Tuning.json:10` | "it earns a trickle" |
| **UNLESS** | The Orders window's condition row ("Over {n} enemy CP there", or "Never"). | `Hud.cpp:2272-2273` | |
| **Upgrade to L**{n} | A structure's button: order its next level, paid at once and built by itself. | `Hud.cpp:2689`; `ADR-064` | **UPGRADING** · {p}% |
| **Victory** | Banner title for the winner. | `Hud.cpp:2199` | |
| **WAITING FOR ORE** / **WAITING FOR FLEET CAP** | A queue's front job that cannot start. | `Hud.cpp:1689` | "waiting for Ore", "waiting for the fleet cap" |
| *warship* | Any ship but a Constructor. | `Simulation.cpp:2932-2949`; `OrderForm.cpp:192` ("SELECT WARSHIPS") | |
| **WATCHING** | The Orders window's row for an event trigger's sector. | `Hud.cpp:2257` | |
| **WEAPON** | The Designer's weapon slot. | `Hud.cpp:675` | |
| **WHEN** / **WHERE** | The Orders window's trigger and action-sector rows. | `Hud.cpp:2248, 2260` | |
| **WHILE YOU WERE AWAY** | The Away window: the report on return. | `Hud.cpp:1997` | *away report* |
| *world* | A match that does not end, run by a dedicated server from a folder, with saves, deputies and restarts. | `Phase5.md:13`; `ADR-077`; `ADR-084` | "Join world" |
| *World.json* / *World.log* | A world's settings (seed, seats, tokens) and its log of records. | `WorldSettings.cpp:36-100`; `WorldLog.h:6, 29-51` | |
| *Wreck* | The derelicts' model set: gray (0.4, 0.4, 0.42) on Human meshes, by hull. | `Models.json:38-42, 58` | ground rule 6 "grey"; `Phase4.md:305` "grey" |

---

**Count:** 201 rows in the table above. Rows that pair related labels (for example **25%**, **50%**, **Never**) count once.

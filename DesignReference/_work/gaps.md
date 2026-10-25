# Gaps

Agent **R** (reconciliation). HEAD `a053fe294d91dc885bbb061c2f34d76ca6897738` (2026-10-25T08:33:55+01:00).

Every "Not specified" and open item from `A1`–`A7`, deduplicated, plus the two cross-checks the brief asks for:

- **§B:** elements on screen (A4's inventory and UI strings) whose rule no rules agent (A1, A2, A3, A7) explains.
- **§C:** rules from A1, A2 and A3 with no visible surface on any screen A4 lists.

A gap is something the sources do not say. The reference prints **"Not specified"** in its place. When a gap is a disagreement it is in `conflicts.md`, and the entry here points there.

Each entry gives what is missing, where it shows, the agents, and the sources consulted.

---

## A. Not specified, or open

### World, map and economy

**G-001 — The map's name.**
- **Where:** the map as a whole, and the reference's map section.
- **Agents:** A1.
- **Sources:** `Map.json` (no name field); `Phase4.md:248` ("10 km system", unnamed); `MVP.md:15-19` (the lore's "Kessler Reach" is a proposal, not the map's name).

**G-002 — A cap on the Ore stockpile.**
- **Where:** the Ore panel.
- **Agents:** A1.
- **Sources:** `Tuning.json: rules`; `Simulation.cpp` (no cap found); `ADR-016`.

**G-003 — A sink or bound for a world's bank.**
- **Where:** a world's economy.
- **Agents:** A1, A3.
- **Sources:** `Phase5.md:137` ("decided in Phase 6"); gate H9 (`Phase5.md:54`); `ADR-082` Consequences (Blue's bank at 513,829 Ore at hour 23).

**G-004 — Placing a Mining Rig in a suppressed sector.**
- **Where:** placement.
- **Agents:** A1.
- **What the code does:** allows it, checking the holder only (`Simulation.cpp:2103-2108`). No document says either way.
- **Sources:** `ADR-056` decision 5; `Phase2.md:53`.

**G-005 — A held-sector rule for the Research Lab and the Defence Platform.**
- **Where:** placement.
- **Agents:** A1.
- **What the code does:** checks none (`Simulation.cpp:2113-2133`). No document states one. The Shipyard and the Repair Bay need a held sector (`Simulation.cpp:2118-2124`).

**G-006 — Hostile starts (gate H6).**
- **Where:** the world's restart. The banner reads "It restarts once your start is clear of the enemy".
- **Agents:** A1, A3.
- **What is open:** whether a start with enemy warships on it is free. The code counts it free (`Simulation.cpp:3750-3765`). Also open: whether a restarted base is spared for a while.
- **Sources:** `ADR-084` Consequences ("the owner's to decide"); `Plan5.md:247`.

**G-007 — A seat whose home sector the other player holds.**
- **Where:** the world's restart.
- **Agents:** A1.
- **What happens:** it waits with no limit. `Phase5.md:129` calls this "the weakest rule of Phase 5, and the week will show how often it binds".

**G-008 — The owner's pending runs.**
- **Where:** results throughout.
- **Agents:** A1, A3, A7.
- **What is pending:**
  - the Phase 5 week, W1 to W6 (`Phase5.md:30-37`): W1, W4 and W6 are the owner's, and W2's tick figure is the owner's Release run;
  - the 02:00 attack (W5);
  - task 37.3 (an AI empire left for an hour);
  - task 39.1 (the banner and replanning) (`Plan5.md:28-40, 156, 196, 206, 250`);
  - the look's "first values" (ADR-019, 026, 034, 090, 091);
  - interface plan 2's runs (`PlanUI2.md:64-81`).
- **Sources:** as listed. No result exists at HEAD.

**G-009 — Exact positions of seeded content.**
- **Where:** asteroids beyond the six home ones, pirate camps, derelicts, and derelict topics.
- **Agents:** A1, A4 (its layout JSON samples).
- **What is fixed:** these positions vary with the match's or world's seed (`ADR-072` Consequences). Only the counts per sector kind are fixed (`Map.json:150-170`).

**G-010 — Whose the shared contested asteroids are, in ADR-036 decision 4.**
- **Where:** the per-player asteroid listing.
- **Agents:** A1.
- **What is unclear:** the per-player listing counts B4, D2 and the center inside one player's list. The totals check: 16 contested on the map. `ADR-072` Consequences says which start these lie nearer varies by seed.

**G-011 — A match length target.**
- **Where:** match flow.
- **Agents:** A1.
- **Sources:** `Phase4.md:199` ("No match length target").

**G-012 — Pirate structures and the start-free rule.**
- **Where:** the world's restart.
- **Agents:** A1.
- **What is unstated:** `IsStartFree` blocks on any other owner's structure, `PIRATES` included (`Simulation.cpp:3762-3763`; `Id.h:25`). No outpost can stand in a home sector on this map (`Map.json:159-163`), so it never arises. No document covers it.

**G-013 — The premise and lore.**
- **Where:** the cover and the pitch.
- **Agents:** A1.
- **What is open:** the MVP marks the premise "proposal — edit freely": the Terrakin, the Kessler Reach, the Tarkan High Command (`MVP.md:15-23`). No later document confirms or amends it.

### Units, structures and combat

**G-014 — Shipyard L4–L5 and Research Lab L5: art with no content.**
- **Where:** structure levels.
- **Agents:** A2, lead.
- **Verdict:** a gap, not a conflict. The design ends the Shipyard at L3 and the Lab at L4 on purpose. Their L4–L5 and L5 art "waits for content that needs it" (`Phase3.md:82, 104, 162, 172`, gate K3; `Phase3.md:11` "five levels each … to be used later").
- **What exists:**
  - `Tuning.json:48-59` gives the Shipyard 2 levels above L1 and the Lab 3.
  - `Models.json:11-13, 26-28` give all three growing models 5 levels.
  - The loader caps levels at the art's last (`ADR-064:13`, `MAXIMUM_STRUCTURE_LEVEL`).
  - The client loads every level (`GameClient.cpp:360-376`) but draws only up to the structure's level (`DrawnLevel`, `ADR-064:28`). So Shipyard L4/L5 and Lab L5 are loaded and never drawn.
- **Not specified:** their cost, time and effect.

**G-015 — Hardpoints with no gun behind them.**
- **Where:** the meshes.
- **Agents:** A2.
- **What is unstated:** the Mining Rig mesh carries 1 `gun` hardpoint, though the rig has no weapon (`Tuning.json:60`). The Defence Platform carries 6 for its 1 Defence gun (R parsed the NMFs). `ADR-018` Consequences speaks only of "a gun on the two armed structures". No document says what the rig's hardpoint is for. Nothing fires from it.

**G-016 — The Defence gun's own cost.**
- **Where:** the structure-weapon table.
- **Agents:** A2.
- **Why there is none:** the gun is part of its structure (`Tuning.json:37`). The Command Station is not buildable, and the Defence Platform costs 450 (`Tuning.json:61`).

**G-017 — Damage against Constructors and structures.**
- **Where:** the Designer.
- **Agents:** A2.
- **What is missing:** the damage cards cover hulls only (`Hud.cpp:782-806`). Damage against a Constructor or a structure is shown nowhere. A2's table of those hits is derived from `DesignStats.h:18-21`, not displayed.

**G-018 — How many ships a splash reaches in a real fight.**
- **Where:** the Designer's "×5 ships".
- **Agents:** A2.
- **What is missing:** the card counts a formation at 3 footprint radii (`DesignStats.h:105-129`; `ADR-010` decision 4). Ships pressed together in a fight can stand closer (`MVP.md:354`). The real count is not specified.

**G-019 — Fire-rate upgrades hidden by rounding.**
- **Where:** the Designer's weapon card.
- **Agents:** A2.
- **What happens:** the interval is written to a tenth (`Hud.cpp:686-687`), so the Mass Driver's upgraded 0.3636 s still reads "0.4s". No document says whether that is intended.

**G-020 — "8.8" for 8.75 rests on rounding a tie.**
- **Where:** the Designer's damage card.
- **Agents:** A2.
- **What it depends on:** `std::format("{:.1f}")` on MSVC (`HudTests.cpp:1718`). No document fixes the rounding rule.

**G-021 — Code comments that point elsewhere.**
- **Where:** none on screen.
- **Agents:** A2.
- **Which:** `GameLogic/Matchup.h:5` cites "horizon §9"; the in-scope source is ADR-083. `Tools/BattleModel.py:86-88` puts `FORMATION_SPACING_RADII` in `Simulation.cpp`, but it lives in `GameProtocol/DesignStats.h:107`.

**G-022 — Defence guns at Command Station L2 and L4.**
- **Where:** the station's next-level panel line.
- **Agents:** A2.
- **What happens:** those levels give no `guns` value. They keep the level below's by the `max` in `StationGuns` (`Tuning.cpp:754-763`), and the panel's line names no guns for them (`Hud.cpp:2658-2659`). `ADR-064:41` states the rule. No document says what a player should read at those two levels.

### Orders, AI and the world

**G-023 — Can enemy ships be selected?**
- **Where:** the Controls window.
- **Agents:** A3.
- **What is unclear:** the window says "Click — Select a ship, or a structure of yours" (`KeyBindings.h:94`). The code picks only the player's own ships (`PlayerControls.cpp:251-253`). Whether "a ship" was meant to include an enemy's is not specified.

**G-024 — A limit on scheduled orders per seat.**
- **Where:** the Orders window.
- **Agents:** A3.
- **What exists:** no cap is in `Simulation.cpp:3274-3313`. The window lists the first 6, then "and n more" (`Hud.h:526`, `Hud.cpp:1964-1965`).

**G-025 — How the player learns of a refused command.**
- **Where:** everywhere.
- **Agents:** A3.
- **What exists:** nothing. "The protocol cannot tell the client yet" (`InProcessServer.cpp:482`). A scheduled order refused when given simply does not appear. Only a refusal when the order *fires* is told ("Order refused: …", `Alerts.cpp:39-40`).

**G-026 — The scout's own-half time counts the world's ticks.**
- **Where:** AI behavior in a world.
- **Agents:** A3.
- **What happens:** `ownHalf` compares the snapshot's tick with 600 s of ticks (`Opponent/AiPlayer.cpp:1340`). An AI empire made afresh late in a world, after a restart, goes straight to the flanks.
- **Not specified:** `ADR-076:14` says "for the first ten minutes", written for a match. Whether this is intended in a world is not specified. It may be a defect: R confirmed the code, and found no document.

**G-027 — Deputy defenders at hand-back.**
- **Where:** the Away flow.
- **Agents:** A3.
- **What happens:** once the player plays, the deputy only watches (`Deputy.cpp:128-138`). Warships it sent to defend are not moved back. `Phase5.md` §6 says only "What the deputy ordered stands".

**G-028 — An away report on the first join of a new world.**
- **Where:** the Away window.
- **Agents:** A3.
- **What happens:** a seat never taken is the deputy's from tick 0 (`SeatController.cpp:35-36`), so the first join carries a report (`AwayReports.cpp:49-50`). No document says whether this is intended.

**G-029 — The total delay before a vanished client's deputy takes over.**
- **Where:** the Away flow.
- **Agents:** A3.
- **What is known:** 60 s after the connection is seen gone (`Server.h:32`), plus MsQuic's default idle timeout of 30 s, which no code sets (`ADR-079` decision 4). The total is not stated anywhere.

**G-030 — A record of hand-overs in the client.**
- **Where:** the Away window.
- **Agents:** A3.
- **What exists:** the window shows the time away only. Hand-overs are in `World.log` `seat` records alone (`ADR-079` Consequences).

### Screens and visual language

**G-031 — Text widths.**
- **Where:** every width that depends on measured text.
- **Agents:** A4.
- **What is missing:** about 2,200 of A4's 7,677 layout elements are estimated (`"measured": false`). That includes the HUD column's ≈380 (`ADR-066` gives only the 380 floor), the selection panel's width, the tag's width and every right-aligned left edge. Exact values need the `GameAppTests` dump on Windows (`ADR-061`).

**G-032 — The picked weapon's name may be cut.**
- **Where:** the Designer's slot row.
- **Agents:** A4.
- **What is unverified:** "Mass Driver" in a 78-unit room, Name face (`Hud.cpp:1439-1440`), is borderline by estimate.

**G-033 — A locked module card cannot appear.**
- **Where:** the Designer.
- **Agents:** A4, A2.
- **Why:** `ViewOf(ModuleTuning)` always sets `available = true` (`ShipDesign.cpp:76-84`), so the `RESEARCH` lock path (`Hud.cpp:716`) is unreachable with the current data.

**G-034 — A loading screen, and a "connecting" indicator.**
- **Where:** startup, and Join world.
- **Agents:** A4.
- **What exists:** neither. The window is hidden until loaded (`WinMain.cpp:273-316`; `ADR-049` leaves a loading screen open). Join world blocks the frame loop up to 10 s (`QuicTransport.cpp:9, 26-35`).

**G-035 — The minimap's view outline.**
- **Where:** the minimap.
- **Agents:** A4.
- **What is missing:** A4 replayed it from `Camera.cpp`/`Camera.json` at the default zoom with an estimated focus. It is not measured.

**G-036 — Hover lighting.**
- **Where:** windows and the menu.
- **Agents:** A4.
- **What exists:** only the HUD's buttons, retreat cells, ship bars, status lines and the banner's "Back to menu" light under the pointer (`ADR-088` decision 1, `Hud.cpp:3129-3131`). The Orders window's Give button is drawn without a hovered flag (`Hud.cpp:1979`), and the menu has no hover. `ADR-088` Consequences says nothing in a window lights. Whether that is meant for Give is not specified.

**G-037 — The Body face.**
- **Where:** the type list.
- **Agents:** A4, A5.
- **What is unstated:** Segoe UI 600 at 20 units is rasterized (`Hud.cpp:3010`), but no drawn text uses it. Whether it is kept on purpose is not specified.

**G-038 — Interface-scale reach on a 4:3 screen.**
- **Where:** Interface scale.
- **Agents:** A4.
- **What exists:** A4 computes ≈150% in a match. No document states it. A5 did not recompute the reach, since `WindowsFit` now counts the Orders and Away windows (see C-022).

**G-039 — A pressed state.**
- **Where:** every button, card and cell.
- **Agents:** A5.
- **What exists:** nothing changes on mouse-down.

**G-040 — Front and back windows look the same.**
- **Where:** the floating windows.
- **Agents:** A5.
- **What exists:** the front window is only drawn last (`Hud.cpp:3632-3690`).

**G-041 — Hue families shared by more than one meaning.**
- **Where:** the HUD and the world.
- **Agents:** A5.
- **Which:**
  - **Green:** selection ring, healthy, better or Good, commit, valid ghost.
  - **Warm:** Ore gold, waiting amber, Fair, hurt, armed-order ring.
  - **Red:** enemy, worse or Poor, low health, invalid ghost.
  - **Blue:** own, accent, own progress, window edge, pick.
- **Sources:** beyond `ADR-085`'s three decisions, only `PlanUI1.md:285` addresses this ("each used consistently").

**G-042 — Ore written as a word.**
- **Where:** the Designer and the hints.
- **Agents:** A5.
- **Which:**
  - the Cost bar's unit "ore", lowercase, with the figure not grouped (`Hud.cpp:778`);
  - "{n} / 100 ore" (`Hud.cpp:802`);
  - "Derelict: 1,200 Ore" (`Hud.cpp:2137`);
  - "Ore left 1,234" (`Hud.cpp:2594`).
- **What is unstated:** `ADR-043` decision 3 does not list these. Whether they are meant exceptions is not specified.

**G-043 — Each face's line box in pixels.**
- **Where:** type.
- **Agents:** A5.
- **What it depends on:** the installed font's metrics, `ceil(ascent + descent + lineGap)` (`GlyphAtlas.cpp:258-259`).

**G-044 — Unsnapped 1-unit outlines.**
- **Where:** all panels at 1280×720.
- **Agents:** A5.
- **What is unstated:** a 1-unit outline there is 0.667 px (`UiPipeline.cpp:236-241`). No ADR says how it rasterizes.

**G-045 — The gem is scaled.**
- **Where:** the Ore gem.
- **Agents:** A5.
- **What happens:** it is rasterized at 12 units and drawn at 13 or 8 (`Hud.cpp:3022`; `UiPipeline.cpp:290-305`). Recorded, not judged.

**G-046 — Colors with no stated meaning.**
- **Where:** the minimap and the designer header.
- **Agents:** A5, R.
- **Which:**
  - `NEUTRAL_COLOR` is the minimap's mark for "anything else" (`Hud.cpp:31, 3514`). Which entities draw it at HEAD is not stated: every rock on the map is an ore asteroid or a field.
  - `SLOT_FILLED_COLOR` (`Hud.cpp:1013`) has no comment.
  - `AMBER_COLOR` (`Hud.cpp:1000`) has no comment. Its meaning is read from `Phase1.md:284`.

**G-047 — Dead parameters.**
- **Where:** none on screen.
- **Agents:** A5.
- **Which:** `CARD_COLOR` passed to `AddButton` is ignored (`Hud.cpp:1979, 2357, 3348, 3439`). `OpenWindow`'s title parameter is never drawn, since every caller passes "".

**G-048 — The Wreck set's meshes.**
- **Where:** derelicts.
- **Agents:** A6.
- **What happens:** `"meshes": "Human"` gives the Wreck set all 21 Human meshes, and the game loads them. Only the three hulls are ever drawn (`GameClient.cpp:1628-1639`).

**G-049 — The Pirate set's structures.**
- **Where:** pirates.
- **Agents:** A6.
- **What happens:** the Pirate set loads every Tarkan mesh, but pirates spawn only Defence Platforms (`Simulation.cpp:843`) and Small and Medium ships (`Simulation.cpp:868`; `Tuning.json:123-127`). Their Command Station, Shipyard, Research Lab, Mining Rig, Large hull and Constructor are never drawn.

**G-050 — No in-game image exists.**
- **Where:** the whole reference.
- **Agents:** A4, A6, A7.
- **What exists:**
  - `DesignReference/_work/screens/` is empty.
  - A6's renders approximate the look. They differ in line width, projection, antialiasing and the glow's scale. They also leave out a structure under construction, the rig on its rock and field rocks.
  - The app logos are placeholders (`Package.appxmanifest:20, 40-43`).
  - The game was not built or run.

**G-051 — A structure's size against `lengthMeters`.**
- **Where:** the world.
- **Agents:** A6.
- **What happens:** a structure is drawn across `2 × footprintRadiusMeters` (`GameClient.cpp:1658-1664`). For example, the Command Station is 90 m long against `lengthMeters` 60, and the Mining Rig 50 m against 4. `ADR-011` decision 2 calls `lengthMeters` "how big a model is", which holds for ships only. Not a conflict: the code is explicit.

**G-052 — Smaller overhangs of the footprint.**
- **Where:** the world.
- **Agents:** A6.
- **Which:** fitted along x and drawn across the footprint, the Human Mining Rig spans about 57 m against 50 m (z/x 1.15), and the Tarkan Defence Platform about 46 m against 40 m (z/x 1.15). Both are computed from the NMFs; R parsed both.
- **Not specified:** whether that is intended. The Human Command Station's far larger overhang is C-002.

**G-053 — Textures no code reads.**
- **Where:** the package.
- **Agents:** A6, A7.
- **Which:** `glow.dds`, `cloudyglow.dds` and `laser.dds` are packaged but read by no code. `starburst.dds` is read only by a test (`OutpostCommander.vcxproj:122-124, 171`; `ADR-022` decision 4; `ADR-028` Consequences).

**G-054 — Move and order markers.**
- **Where:** the world.
- **Agents:** A7.
- **What exists:** none is drawn (`GameClient.cpp:1125-1183`). `PlanUI2.md:520` mentions "where a move marker would stand". The only feedback is the selection ring's amber while an order is armed.

**G-055 — The drag box's width.**
- **Where:** the world.
- **Agents:** A7.
- **What exists:** 1.5 m on the ground (`GameClient.cpp:88`), from code only. At 3,000 m wide it is under a pixel. No ADR states it.

**G-056 — Which way the parts spin.**
- **Where:** Research Labs.
- **Agents:** A7.
- **What is known:** only that the Tarkan counter ring turns "the other way" (`ADR-045:12`). The others follow their baked axes, and no document states a sense.

**G-057 — A held and guarded sector.**
- **Where:** territory marks.
- **Agents:** A7.
- **What happens:** the code outlines it as held only (`TerritoryMarks.cpp:42-49`). `ADR-081` does not cover the case.

**G-058 — One pattern, two meanings.**
- **Where:** territory marks on the ground and on the minimap.
- **Agents:** R, from A4 and A7.
- **What happens:**
  - On the ground, a **suppressed** sector's outline is dashed and a cut-off one's dotted (`ADR-081:26`; `TerritoryMarks.cpp:45-47`).
  - On the minimap, a **cut-off** sector's outline is dashed (6 on, 4 off) and a suppressed one is hatched (`ADR-081:41`; `Hud.cpp:3573-3580`).
- **Not specified:** both are decided, but no document explains why the dashed pattern means different states on the two surfaces. `ADR-085` (one meaning per color) covers color, not pattern.

---

## B. On screen, with no rule explained by another agent

A4's element inventory and UI strings (`A4.md` §1–§4 and "UI strings" A and B) were checked against A1, A2, A3 and A7. Almost every element that carries a game rule is explained by a rules agent, or by an ADR A4 cites: status lines, territory and drain (`ADR-057` decision 7), alerts, hints, selection lines, buttons, designer, production, research, orders and away. These remain:

**G-059 — Fallback labels.**
- **Which:**
  - "Unknown design" (`Hud.cpp:191`, a design missing from the snapshot);
  - "Unknown topic" (`Hud.cpp:2930`);
  - "Structure" (`Hud.cpp:2163, 2568`; `OrderForm.cpp:281`);
  - "a structure" (`Hud.cpp:2551`);
  - "?" (`Hud.cpp:427, 2969`).
- **Not specified:** when a player can meet any of these. No document names them.
- **Agents:** R, from A4's string table.

**G-060 — "last seen earlier".**
- **Where:** the hint over a remembered structure or derelict.
- **What the code does:** shows "earlier" when the last-seen tick is 0 or later than the snapshot's (`Hud.cpp:452-460`). A4 states it only as "when it is only remembered".
- **Not specified:** when a memory can have no known tick in play, for example across a world's recovery. No document says.
- **Agents:** A4, R.

---

## C. Rules with no visible surface

These rules are applied by the simulation, but A4 lists no screen that shows their figure. The reference can state each rule, but no screenshot can show it. Sources were checked by `grep` over `GameApp/Hud.cpp` (for example, no `turnRate`, `oreYield`, `suppression` or `repairPercent` appears there).

**G-061 — Per-rig income and the asteroid's yield class.**
- **The rule:** home 3.5, near 4, contested 5.5, rich 6.5 Ore/s (`Tuning.json:6-9`).
- **What shows:** only the total "+n/s" (`Hud.cpp:3146-3151`). A rig's panel gives the Ore left and its sector's state, not its rate. The minimap gives no class: the rich asteroid differs only by its radius, 60 m against 45.
- **Agents:** A1, A4.

**G-062 — Command points per hull.**
- **The rule:** Small 1, Medium 2, Large 4 (`Tuning.json:16-18`).
- **What shows:** no Designer row or production card shows them. Only the totals show ("Fleet {cp} / {cap}", `Hud.cpp:375-379`) and the condition's "enemy CP" (`OrderForm.cpp:313`).
- **Agents:** A1, A2.

**G-063 — Turn rate and the drive's turn factor.**
- **The rule:** hull turn rate × drive factor (`DesignStats.h:83-84`).
- **What shows:** nothing. The drive card reads "SPD ×f · HP ×f" (`Hud.cpp:669`), and the Designer's bars are Hit points, Armor, Speed, Range, Sensors, Cost and Build.
- **Agents:** A2.

**G-064 — Sight ranges.**
- **The rule:** weapon range + 50 m, 200 m for an unarmed ship or structure, the shooter revealed for 3 s (`Tuning.json:13`).
- **What shows:** only the Sensors bar, for a module (`Hud.cpp:772-777`). The fog shows the result.
- **Agents:** A1, A2.

**G-065 — Suppression radius and the cut-off rule.**
- **The rule:** 400 m (`Tuning.json:14`), and a path of held sectors to home (`Simulation.cpp:2864-2891`).
- **What shows:** only each sector's state ("suppressed, it earns nothing", "cut off, it earns half") and its marks.
- **Agents:** A1.

**G-066 — Pirate guard and chase distances, and camp against stronghold.**
- **The rule:** 600 m and 900 m (`Tuning.json:120-121`).
- **What shows:** no ring, figure or label. Orders targets read "Pirate {name}" only (`OrderForm.cpp:288`).
- **Agents:** A1.

**G-067 — Repair rates and reach.**
- **The rule:** a Constructor repairs 2% a second each. A Repair Bay, the Command Station and a Shipyard repair 3% a second, up to 4 ships within 150 m (`Tuning.json:35, 131`).
- **What shows:** only the health bars move.
- **Agents:** A1, A2.

**G-068 — Where a retreat goes.**
- **The rule:** the nearest Repair Bay, then the Command Station, then the nearest Shipyard (`Simulation.cpp:2556-2577`).
- **What shows:** "Retreating to be repaired" and the "Ship retreating" alert. No destination is shown.
- **Agents:** A1.

**G-069 — Salvage's work time and recovery share.**
- **The rule:** 30 s of work and 50% recovery (`Tuning.json:130`).
- **What shows:** "salvaged {p}%", ", and part of {topic}'s research time" and "· SALVAGED" (`Hud.cpp:2137-2141, 2979`). The 50% is never written.
- **Agents:** A1.

**G-070 — Paid at the start, never refunded, no cancel.**
- **The rule:** `ADR-016` decisions 4 and 6.
- **What shows:** no cancel control and no text. A button is dim when the Ore is short.
- **Agents:** A1, A2.

**G-071 — The Constructor's helper share and the time with n Constructors.**
- **The rule:** 0.5 for each further Constructor (`Tuning.json:35`; `Simulation.cpp:2435-2437`).
- **What shows:** the build buttons show cost only, and a site shows its progress.
- **Agents:** A1, A2.

**G-072 — Pirates never retreat, never raid, never respawn, never leave their sector.**
- **The rule:** `ADR-073` decisions 4–5; `ADR-075` decision 4.
- **What shows:** nothing.
- **Agents:** A1.

**G-073 — Order within one tick.**
- **The rule:** a loss of production is decided before domination, so a player who loses both in one tick loses by production (`Simulation.cpp:946-951, 979`).
- **What shows:** nothing.
- **Agents:** A1.

**G-074 — The deputy's play while away.**
- **The rule:** research, production, Constructors, rigs, repairs and defense (`Deputy.cpp`).
- **What shows:** no live surface. Only the Away report afterwards, and `World.log`.
- **Agents:** A3.

**G-075 — Saves, recovery and the version pin.**
- **The rule:** every 60 s, the newest 3 kept, the refusal messages (`WorldFolder.h:15, 23`; `WorldState.cpp`).
- **What shows:** nothing in the client. These are server-console and folder matters.
- **Agents:** A3.

**G-076 — The world log's hourly records and battles.**
- **The rule:** `WorldLog.h:29-57`.
- **What shows:** `World.log` only.
- **Agents:** A1, A3.

**G-077 — The seed and the tick rate.**
- **The rule:** 20 Hz (`Tuning.json:3`). A match's seed goes to the debugger output, and a world's is printed once by `--new-world` (`OutpostServer/Main.cpp:171-174`).
- **What shows:** neither, in the client.
- **Agents:** A1, A3.

**G-078 — AI difficulty settings and the counter table.**
- **The rule:** `Opponent*.json`.
- **What shows:** only the menu's three names.
- **Agents:** A3.

**G-079 — Events that raise no alert.**
- **The rule:** `ShipBuilt`, `StructureBuilt`, `ShipLost`, `StructureLost` of anything but a rig, `SectorGained` and `SectorLost` (A3 §3.3).
- **What shows:** they are only counted or named in the Away report.
- **Agents:** A3.

---

## Counts

| Section | Entries |
|---|---|
| A. Not specified, or open | 58 (G-001 to G-058) |
| B. On screen, rule unexplained | 2 (G-059, G-060) |
| C. Rule with no surface | 19 (G-061 to G-079) |
| **All** | **79** |

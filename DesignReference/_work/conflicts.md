# Conflicts

Agent **R** (reconciliation). HEAD `a053fe294d91dc885bbb061c2f34d76ca6897738` (2026-10-25T08:33:55+01:00). Built from `A1`–`A7` and checked against the repository. Every entry was deduplicated across agents: "Reported by" credits each agent that raised it, and "lead" marks the lead's notes.

**How to read an entry.**
- **Sides** give file:line and the value each states.
- **Reference shows** is the code's or the data's value. That is what the reference prints (ground rule 3).
- **Class** is one of these:
  - `code-vs-intent`: the code differs from the latest decision. It may be a defect or an undocumented decision.
  - `stale-doc`: an ADR, a code comment or the brief should say what the code does.
  - `superseded`: an earlier document that a later phase or ADR already overrides. Informational only.
  - `agent-disagreement`: two agents describe the code differently.
- **OWNER?** is yes when the entry changes what a screen shows or a core rule, and is plausibly a code defect rather than a stale doc.
- **R checked** says what R verified itself in the repository.

**Order:** OWNER?=yes first (C-001 to C-005), then code-vs-intent (none without OWNER), then stale-doc (C-006 to C-036a), then superseded (C-037 to C-064), then agent-disagreement (C-065 to C-067).

Short names: `Phase1.md` and the others stand for `GameDesign/Archive/OutpostCommander-PhaseN.md`, `MVP.md` for `GameDesign/Archive/OutpostCommander-MVP.md`, and `Phase5.md` for `GameDesign/OutpostCommander-Phase5.md`. `Plan5.md` is `GameDesign/ImplementationPlan-Phase5.md`, `PlanUI1.md` is `GameDesign/ImplementationPlan-Interface.md`, `PlanUI2.md` is `GameDesign/ImplementationPlan-Interface2.md`, and `PlanLook.md` is `GameDesign/ImplementationPlan-Look.md`. ADRs are `Design/ADR/ADR-NNN-*.md`.

---

## OWNER? = yes

### C-001 — The HUD's side colors are code constants, not the data's side colors, and for seat 2 they contradict the world
- **Claim:** Ground rule 6 says colors come from data. ADR-011 decision 3 says the set's color is the team color. The minimap and the territory figures do not read `Models.json`.
- **Sides:**
  - **Data** (`OutpostCommander/Assets/Models.json:5, 20, 35, 40`; `players` at `:53-56`). The scene draws each side by player number (`GameApp/ModelCatalog.cpp:304-316`, `SetForPlayer`):
    - Human (0.18, 0.42, 0.95) `#76ADF9`, player 1
    - Tarkan (0.9, 0.25, 0.12) `#F38961`, player 2
    - Pirate (0.62, 0.22, 0.9) `#CE81F3`
    - Wreck (0.4, 0.4, 0.42) `#AAAAAD`
  - **HUD constants** (`GameApp/Hud.cpp:26-30`). They are relative to the viewer. The minimap marks use them at `Hud.cpp:3477, 3508-3511, 3602`, and the territory figures at `Hud.cpp:3191-3192`:
    - `OWN_COLOR` (0.35, 0.65, 1.0) `#A0D3FF`
    - `ENEMY_COLOR` (1.0, 0.38, 0.25) `#FFA689`
    - `PIRATE_COLOR` (0.78, 0.45, 1.0) `#E5B3FF`
    - `DERELICT_COLOR` (0.62, 0.62, 0.66) `#CECED4`
  - **Decision.** `ADR-043:36` (decision 7): "blue is the player's, red the enemy's". `ADR-011:15` (decision 3): "blue for the player's Human set, orange-red for the AI's Tarkan set". `GameApp/ModelCatalog.h:21` calls the color "provisional until team colors are decided".
  - **Where they clash.** In a world, seat 2 can be a human's: `OutpostServer --new-world` gives every map start a token unless `--ai` names it (`OutpostServer/Main.cpp:136-150`), and `Join-Player2.json` is written for it (A3 §10). That player sees its own ships and structures drawn Tarkan red-orange in the world, but blue (`OWN_COLOR`) on the minimap and in its node figure. Its enemy is drawn Human blue in the world but red (`ENEMY_COLOR`) on the minimap. In a skirmish the human is always player 1 (`OutpostCommander/WinMain.cpp:27-28`), so the hues agree there, but the values never match (`#A0D3FF` against `#76ADF9`).
- **Reference shows:** the HUD's constants on the HUD and minimap, and `Models.json` in the world. Both are listed, with the hex of each.
- **Reported by:** A5 (Open, "The HUD's side colors are not data"), A4 (minimap color names), A7 (side colors), lead.
- **Class:** code-vs-intent · **OWNER?** yes
- **R checked:** `Hud.cpp:19-30`, `ModelCatalog.cpp:304-316`, `Models.json:1-58`, `OutpostServer/Main.cpp:130-150`. Hex values recomputed (sRGB, IEC 61966-2-1).

### C-002 — The Human Command Station is drawn about 181 m across its 90 m footprint
- **Claim:** ADR-064 means every structure level to stand within its footprint. The Human Command Station, at every level, is drawn about twice as deep as its footprint is wide.
- **Sides:**
  - **Decision.**
    - `ADR-064:28` (decision 8): "Every level is drawn within level 1's ground … Level 1 is fitted to the model's length along its front."
    - `ADR-064:49` (Consequences): "Fitted by its length alone … a level whose depth grows more than its length overhangs its footprint … where level 1 is square."
    - `GameApp/GameClient.cpp:1658-1664`: "Drawn across its kind's footprint", scale = 2 × footprint radius ÷ `lengthMeters`.
  - **Code and art.**
    - Level 1 is fitted by its x length alone (`GameApp/ModelCatalog.cpp:382-385` → `NeuronClient/MeshData.cpp:219-225`, `FitMesh`).
    - `Human/CommandStation_L1.nmf` measures x 451.82 by z 908.00 mesh units, z/x = 2.01 (R parsed the NMF).
    - So L1 is 60 m along x and 120.6 m along z, drawn at ×1.5 (`Tuning.json:40`, footprint radius 45): **90 m by 181 m against a 90 m footprint**.
    - L2 to L5 take L1's widest (120.6 m), so they stand 181 m deep too (A6 per-model table).
  - **Why.** The owner replaced every mesh in commit `09638bc` ("New shaders added", 2026-10-04 16:55 +0200). That was after ADR-064's measurements of 2026-10-04 were written (commit `7d2b2fc`, 15:07 +0200). ADR-064's "level 1 is square" describes the replaced art.
- **Reference shows:** the code's fit: 90 m by about 181 m for the Human station at every level. Tarkan is 90 by 96 m at L1–L4 and 94 by 96 m at L5.
- **Reported by:** A6 (Open, "Some structures reach well past their footprint"), lead.
- **Class:** code-vs-intent · **OWNER?** yes
- **R checked:** the NMF extents of all ten Command Station levels (Human z/x 2.01, 1.82, 1.66, 1.11, 1.09; Tarkan 1.07 to 1.03); the fit code paths above; `git log` on the GLB and on the ADR. Smaller overhangs, within 15%, are gap G-052.

### C-003 — The designer's seventh stat row (Build) runs into its footer
- **Claim:** A Sensors bar was added, and "the window still fits". The layout reserves room for six bars but draws seven.
- **Sides:**
  - **Decision.** `ADR-058:17` (decision 7): "A Sensors bar shows the module's sight … The window still fits a screen of 1,080 lines."
  - **Reserved room.** `GameApp/Hud.cpp:1087` (`ExtentOf`): bars = `SECTION_LABEL_HEIGHT + 6 × BAR_ROW_HEIGHT` = 26 + 150 = 176. The footer starts at sections + max(176, 146) + 12 = S + 188 (`Hud.cpp:63, 939-948, 1085-1090`).
  - **Drawn rows.** `Hud.cpp:763-779`: Hit points, Armor, Speed, Range, Sensors (whenever `best.sensor > 0`), Cost, Build. `best` is taken over every design, so it includes the Sensor Array, whose sight is 700 m (`Tuning.json:33`). That makes seven rows in every match. Row *i* starts at S + 26 + 25*i* (`Hud.cpp:1491`), so Build runs from S + 176 to S + 201, past the footer's S + 188.
- **Reference shows:** the code's layout, with the Build row overlapping the top of the RENAME / RETREAT footer by about 13 units. This is derived from the code. Nobody has seen it on a screen.
- **Reported by:** A4 (Open 2).
- **Class:** code-vs-intent · **OWNER?** yes
- **R checked:** the constants and the arithmetic above.

### C-004 — The color of a lit footprint ring
- **Claim:** A lit ring (under the pointer, or while placing) should be "in the color of a structure's own lines".
- **Sides:**
  - **Decision.** `ADR-042:24` (decision 2): "in the color of a structure's own lines (`EdgeColor`, ADR-040)".
  - **Code.** `GameApp/GameClient.cpp:1376` uses `EdgeColor(side->color, EDGE_BRIGHTNESS, EDGE_WHITE_SHARE)`, which is the side's ship-line color. A structure's own lines are `EdgeColor(TowardGray(side, STRUCTURE_GRAY_SHARE 0.35))` (`GameClient.cpp:1577` with `:1561`; `ADR-040:20`, decision 4).
  - **Values (Human).** The ring is (0.473, 0.637, 1.0) `#B7D1FF`. The structure line is (0.572, 0.705, 1.0) `#C7DBFF`.
- **Reference shows:** the code's: the ship-line color.
- **Reported by:** A7 (Conflict 4).
- **Class:** code-vs-intent · **OWNER?** yes (minor: a shade of the same hue)
- **R checked:** `GameClient.cpp:1361-1397`, `ADR-042:20-30`, `ADR-040:17-20`.

### C-005 — The Large hull card's numbers line may be cut short at 1080p
- **Claim:** The part cards were widened "so a hull's numbers fit at the detail size".
- **Sides:**
  - **Decision.** `ADR-062:27` (decision 4): cards from 190 to 220 units "so a hull's numbers fit … like '1,400 HP · ARM 24 · 25m/s'".
  - **Code.**
    - The numbers are fitted to `CARD_WIDTH − 24` = 196 units (`GameApp/Hud.cpp:1459, 1467-1468`), in the Detail face: Cascadia Mono 400 at 13 units (`Hud.cpp:3016`).
    - Advances are rounded to whole pixels (`NeuronClient/GlyphAtlas.cpp:276`).
    - `1,200 HP · ARM 14 · 25m/s` is 25 characters. At an assumed Cascadia Mono advance of 1200/2048 em (7.6 px, rounded to 8), that is 200 px against 196, so `Fit` would cut it.
- **Reference shows:** the code's layout. Whether the line is cut is **Not determinable here**: the advance is quoted from memory, there is no font in the container, and no `HudTests` assertion covers this line (R grepped `GameAppTests/HudTests.cpp`). With Consolas, at 7 px, it fits.
- **Reported by:** A4 (Open 3, C17).
- **Class:** code-vs-intent (unproven) · **OWNER?** yes. This is a check on a Windows screen.
- **R checked:** the code path and constants. The font metric is unverified.

---

## code-vs-intent with OWNER? = no

None. Every code-vs-intent entry found is screen-visible, so all five are listed above.

---

## stale-doc

### C-006 — The Command Station's gun hardpoints
- **Sides:**
  - `Phase3.md:125` (gate K6): "the Human Command Station's 4 hardpoints at level 1 and 12 at level 5 and the Tarkan's 3 and 6".
  - `ADR-064:44` (decision 11): "the Human station's 4 to 12 and the Tarkan's 3 to 6".
  - Meshes: `OutpostCommander/Assets/Models/{Human,Tarkan}/CommandStation_L1..L5.nmf` carry 3, 3, 3, 6, 6 `gun` hardpoints in both sets, and the GLB sources agree (A2 `count_hardpoints.py`).
- **Reference shows:** 3, 3, 3, 6, 6 in both sets. The number of guns that fire, 1 / 1 / 2 / 2 / 3, is `Tuning.json:41, 44, 46` (`Tuning.cpp:754-763`).
- **Why stale:** the owner's own art replacement, `09638bc` (2026-10-04 16:55 +0200), came after these texts were written (`7d2b2fc`, 15:07 +0200). Hardpoints only change where a shot leaves from (`Hardpoints.cpp:205-220`).
- **Reported by:** A2 (Conflict 1), lead. **Class:** stale-doc · **OWNER?** no
- **R checked:** the NMF hardpoint counts, parsed. `git log` and `git show 7d2b2fc` on the ADR.

### C-007 — "Level 1 is square" in ADR-064's measurement
- **Sides:** `ADR-064:49` gives the Human Command Station's depth at 1.57× its length at L2–L3 and 1.13–1.14× at L4–L5, "where level 1 is square". The shipped NMFs give z/x of 2.01, 1.82, 1.66, 1.11 and 1.09 (R parse).
- **Reference shows:** the shipped meshes' extents (A6 table). The consequence is C-002.
- **Reported by:** R (found while checking the lead's note on A6). **Class:** stale-doc · **OWNER?** no

### C-008 — Protocol and world-state version numbers in ADRs
- **Sides:**
  - `ADR-064:26`: "The protocol's version is 5".
  - `ADR-071:16`: "The protocol's version is 7".
  - `ADR-081:49`: "`WorldStateTests.TheLayoutIsTheVersions` holds version 5's hash".
  - `ADR-084:33`: "`WorldStateTests` record the layout of version 6".
  - Against them: `AGENTS.md` R18 ("Until the game is live, the version stays 1", owner 2026-10-09); `ADR-060:17` and `ADR-077:24` (both stay 1); `Phase5.md:81`.
  - Code: `GameLogic/WorldState.h:9` `WORLD_STATE_VERSION = 1`, `GameProtocol/WireFormat.h:7` `PROTOCOL_VERSION = 1`, and `GameLogicTests/WorldStateTests.cpp:160-170` ("under version 1 until the game is live").
- **Reference shows:** 1 and 1.
- **Reported by:** A3 (Conflict 2), lead. R added ADR-064, ADR-071 and ADR-081.
- **Class:** stale-doc · **OWNER?** no. AGENTS.md is authority 1, and the code follows it.

### C-009 — Command Station next-level panel text in ADR-064
- **Sides:** `ADR-064:44` gives "L3: 10 nodes, 2 Defence guns, 7,000 hit points". `GameApp/Hud.cpp:2655-2677` adds the fleet cap (ADR-071): "L3: 10 nodes, 2 Defence guns, fleet 30, 7,000 hit points".
- **Reference shows:** the code's text. **Reported by:** A2 (Conflict 4). **Class:** stale-doc · **OWNER?** no

### C-010 — Who builds a structure's level (code comment)
- **Sides:** `GameProtocol/Snapshot.h:250-252` (comment on `StructureLevelView`): "how long one Constructor takes to build it". `ADR-064:23` (decision 3) and `GameLogic/Simulation.cpp:2650-2668`: a level builds itself, and no Constructor works on it.
- **Reference shows:** the level builds itself over its `buildSeconds`. **Reported by:** A2 (Conflict 3). **Class:** stale-doc (code comment) · **OWNER?** no

### C-011 — Flak Battery splash in ADR-034
- **Sides:**
  - `ADR-034:7` (Context): "a ring as wide as its 20 m".
  - `Tuning.json:29`: `splashRadiusMeters` 26. `ADR-014:22` and `Phase1.md:89` agree (tuned from 20 to 26).
  - The ring is drawn at the shot's splash (`CombatEffects.cpp:179-187`).
- **Reference shows:** 26 m. **Reported by:** A2 (Conflict 2), A7 (Conflict 7). **Class:** stale-doc · **OWNER?** no

### C-012 — Unupgraded income a tick in ADR-017
- **Sides:** `ADR-017:35`: "25 and 40 hundredths every tick" (5 and 8 Ore/s). `Tuning.json:6-9`: 3.5 / 4 / 5.5 / 6.5 Ore/s, which is 17.5 / 20 / 27.5 / 32.5 hundredths a tick at 20 Hz.
- **Reference shows:** `Tuning.json`. **Reported by:** A2 (Conflict 13). **Class:** stale-doc · **OWNER?** no

### C-013 — How long a home reserve lasts, in ADR-035
- **Sides:** `ADR-035:19` (Consequences): "Matches change only after their first 25 minutes, when the home asteroids, 7,500 Ore at 5 a second, run dry; on the 5 km map". `Tuning.json:6` gives 3.5 Ore/s, and `Map.json:9-14` 7,500 Ore. *Derived:* 7,500 ÷ 3.5 ≈ 2,143 s (35:43), on the 10 km map (`Map.json:2`).
- **Reference shows:** the derived 35:43. Near 37:30, contested ≈ 36:22, rich ≈ 61:32, with no research. **Reported by:** A1 (C3). **Class:** stale-doc (the ADR's figure; the Phase 1 half is C-040) · **OWNER?** no

### C-014 — Whom a base repairer repairs
- **Sides:**
  - `ADR-075:9` (Context): "repairs up to 4 friendly warships". `Phase4.md:324` says the same.
  - `ADR-075` decision 3: "its player's ships". `Phase4.md:333`: Constructors retreat and are repaired too.
  - `GameLogic/Simulation.cpp:2617-2621` repairs any damaged `EntityKind::Ship` of its player, Constructors included.
- **Reference shows:** any of its player's ships. **Reported by:** A1 (C10), A2 (Conflict 15). **Class:** stale-doc (ADR-075 Context) · **OWNER?** no

### C-015 — ADR cross-reference for a world's restart
- **Sides:** `ADR-037:28`: "a world's restart places one (ADR-081)". ADR-081 is territory and memory over the fog; the restart is `ADR-084` decision 4 (`Simulation.cpp:3715-3748`).
- **Reference shows:** ADR-084. **Reported by:** A3 (Conflict 9). **Class:** stale-doc · **OWNER?** no

### C-016 — Normal's attack group in ADR-065
- **Sides:**
  - `ADR-065:12` (decision 2): Easy "gathers 30 warships before its main attack not 20", which reads as Normal = 20.
  - `ADR-065:13` (decision 3) itself says task 25.2 "has since given Normal … an attack group of 25", which Hard's file does not share.
  - Data: `Opponent.json:2` 25, `OpponentEasy.json:2` 30, `OpponentHard.json:2` 20. `ADR-041:27` gives 25.
- **Reference shows:** Easy 30, Normal 25, Hard 20. **Reported by:** A3 (Conflict 1). **Class:** stale-doc · **OWNER?** no
- **R checked:** the three files and `ADR-065:10-14`.

### C-017 — Example text of a scheduled order's condition (code comment)
- **Sides:** `GameApp/OrderForm.h:114-115` (comment): "… unless over 8 CP". `GameApp/OrderForm.cpp:313`: ", unless over {n} enemy CP".
- **Reference shows:** ", unless over {n} enemy CP". **Reported by:** A3 (Conflict 11). **Class:** stale-doc (code comment) · **OWNER?** no

### C-018 — The tick a recovered world comes back at
- **Sides:** `Phase5.md:84`: "comes back at the last tick that applied a command". `ADR-077:32`: "the tick after the last command it logged". `GameLogic/InProcessServer.cpp:597-614` replays each logged tick, so the world stands at the tick after the last logged one.
- **Reference shows:** the ADR's wording, which matches the code. This may be wording only. **Reported by:** A3 (Conflict 13). **Class:** stale-doc · **OWNER?** no

### C-019 — Minimap metres per unit
- **Sides:**
  - `ADR-068:37`: "about 18 m a unit on the 5 km map". `PlanUI1.md:215` says the same.
  - `GameApp/Hud.cpp:112-115` (comment): "about 36 m a unit on the 10 km map". `Map.json:2` `sizeMeters` 10000. The map area is 284 units (`Hud.cpp:3462`).
- **Reference shows:** 284 units for 10,000 m = 35.2 m a unit. **Reported by:** A4 (C1), A5 (C4). **Class:** stale-doc · **OWNER?** no

### C-020 — The asteroid field's minimap color
- **Sides:** `ADR-046:34` (decision 5): (0.06, 0.06, 0.065), "down from (0.13, 0.13, 0.14)". `ADR-068:35` (decision 2): (0.13, 0.13, 0.14), "where it was (0.06…)". ADR-046 was not rewritten in place (AGENTS.md §6). Code: `ASTEROID_FIELD_COLOR` (0.13, 0.13, 0.14) (`Hud.cpp:38`).
- **Reference shows:** (0.13, 0.13, 0.14), `#656569`. **Reported by:** A4 (C2). **Class:** stale-doc · **OWNER?** no

### C-021 — Window body opacity
- **Sides:** `ADR-062:41` (Consequences): "A window's body is 97% opaque". `ADR-068:32` (decision 1): "opaque". `Hud.cpp:839-840`: `WINDOW_COLOR` alpha 1.0.
- **Reference shows:** opaque. **Reported by:** A4 (C3), A5 (C1). **Class:** stale-doc · **OWNER?** no

### C-022 — What `WindowsFit` checks
- **Sides:** `ADR-070:15-19` (decision 3): the designer and the Controls window, and the Controls window alone on the menu. `Hud.h:863-866` and `Hud.cpp:3044-3053`: in a match also the Orders window (640 × 560) and the Away window at 20 lines (600 × 530).
- **Reference shows:** in a match, Controls, Orders, Away and the designer; on the menu, Controls alone. **Reported by:** A4 (C4), A5 (C6). **Class:** stale-doc · **OWNER?** no

### C-023 — Draw order within a layer
- **Sides:** `ADR-031:14` (decision 4): "panels, then its sprites, then its texts". `Hud.h:714-715`, `GameClient.cpp:1300-1336` and `ADR-015` decision 4: panels, **lines**, sprites, texts.
- **Reference shows:** panels, lines, sprites, texts. **Reported by:** A4 (C5), A5 (C7). **Class:** stale-doc · **OWNER?** no

### C-024 — When the hint shows
- **Sides:** `ADR-015:22` (decision 4): "A hint at the top middle while a placement is armed". `GameClient.cpp:742-749` and `Hud.cpp:2135-2168` also show it for a derelict, or a remembered structure, under the pointer.
- **Reference shows:** the placement, derelict and memory hints. **Reported by:** A4 (C12). **Class:** stale-doc · **OWNER?** no

### C-025 — Edge scroll
- **Sides:** `ADR-013:45` ("What this forecloses"): "Edge scroll or orders on W and D". Against it: `ADR-012` decisions 5–6; `Camera.cpp:111-125`; `Camera.json` `edgeScrollMarginPixels` 4; `KeyBindings.h:109` "Screen edge — Pan, while the pointer is held in the window".
- **Reference shows:** edge scroll exists, while the cursor is held. **Reported by:** A4 (C13). **Class:** stale-doc (ambiguous wording) · **OWNER?** no

### C-026 — Ore's sprite in ADR-030
- **Sides:** `ADR-030:15` names "Ore's diamond" in the list of `Hud::Sprites`, although the same paragraph says `DrawSprite` draws a cut gem. `Hud.cpp:3022` (`SpriteShape::Gem`) and `ADR-085` decision 3.
- **Reference shows:** a cut gem. **Reported by:** A5 (C2). **Class:** stale-doc · **OWNER?** no

### C-027 — Who owns the build bar
- **Sides:** `ADR-085:33` (Consequences): "The build bar is `GameClient`'s … so it has no test". `ADR-085:24` (decision 2) and `Hud.cpp:1016, 3119`: the HUD's `BUILD_BAR_COLOR`. `ADR-088:50` names `HudTests.LaysTheBarsOverTheEntities`.
- **Reference shows:** the HUD's constant, laid out by `Hud::Lay`. **Reported by:** A5 (C3). **Class:** stale-doc · **OWNER?** no

### C-028 — The vector grid's extent
- **Sides:** `ADR-028:13` (decision 1): "across the whole map, 5,000 m a side". `GameClient.cpp:27-30` (`GRID_HALF_EXTENT_METERS` 5000, "covers the 10,000 m map"), `Map.json:2` and `ADR-036:15`.
- **Reference shows:** −5,000 to +5,000 m, 10,000 m a side. **Reported by:** A5 (C5), A7 (Conflict 1). **Class:** stale-doc · **OWNER?** no

### C-029 — Where a health bar stands
- **Sides:** `ADR-028:19` (decision 7): "just above its footprint". `ADR-088:31-33` (decision 5), `Camera.cpp:270-285` and `GameClient.cpp:731-732`: 4 units above the highest on-screen corner of the model's bounds.
- **Reference shows:** above the model's bounds. **Reported by:** A7 (Conflict 5). **Class:** stale-doc · **OWNER?** no

### C-030 — The widest camera view in ADR-046
- **Sides:** `ADR-046:29`: "At the widest 1,600 m view". `Camera.json:4` `maximumViewWidthMeters` 3000; `ADR-012:13, 17`; `Phase4.md:400` (gate L6).
- **Reference shows:** 3,000 m. (The MVP half is C-061.) **Reported by:** A7 (Conflict 2). **Class:** stale-doc · **OWNER?** no

### C-031 — When the selection ring is amber
- **Sides:** `ADR-013:33` and `ADR-067:13`: amber "while attack-move waits". `GameClient.cpp:1440-1441`: amber while attack-move, Hold sector or Patrol is armed (the arming keys are ADR-059's).
- **Reference shows:** amber for any of the three armed orders. **Reported by:** A7 (Conflict 6). **Class:** stale-doc · **OWNER?** no

### C-032 — Bahnschrift's width for labels and names
- **Sides:**
  - `Phase1.md:290`: "Bahnschrift, condensed and semibold, for headings, labels and names". `GameApp/Hud.h:29-30` (comment): "condensed Bahnschrift for titles, labels and names".
  - `ADR-030:7` (Context) describes the mockup: titles condensed bold, names semibold.
  - `Hud.cpp:3011-3013`: Title is 700 SemiCondensed, while Label and Name are 600 at Normal width.
- **Reference shows:** the code's faces. **Reported by:** A5 (C8). **Class:** stale-doc (design wording and code comment) · **OWNER?** no

### C-033 — Look plan finding on materials
- **Sides:** `PlanLook.md:32` (finding 5): "Every source today has exactly one material, `team`". `Art/Models/Asteroids/{Small,Medium,Large}.glb` have no material. The other 42 have `team`.
- **Reference shows:** the files. Materials are not baked, so no player sees this (`Tools/BakeMeshes.py` docstring). **Reported by:** A6 (C1). **Class:** stale-doc · **OWNER?** no

### C-034 — Crease counts in ADR-027
- **Sides:** `ADR-027:19`: "86 to 277 creases a ship and 249 to 736 a structure". A replay of `BuildCreaseLines` at 10° on HEAD's NMFs gives ships 74–259 and structures 108–1,198 (A6 per-model table).
- **Reference shows:** A6's counts, marked as a replay. **Reported by:** A6 (C2). **Class:** stale-doc (a measurement of older meshes) · **OWNER?** no

### C-035 — Tarkan Medium against Large length in ADR-011
- **Sides:** `ADR-011:9` (Context): "The Tarkan `Medium` is longer than the Tarkan `Large`." The NMF x extents are Medium 35.0 and Large 60.0 mesh units, and `Models.json:23-24` gives 35 m and 60 m.
- **Reference shows:** 35 m and 60 m. **Reported by:** A6 (C3). **Class:** stale-doc (Context) · **OWNER?** no

### C-036 — Look plan status of LV2.1
- **Sides:** `PlanLook.md:47`: LV2.1 "built; awaiting CI and the owner's run". `git log`: `ff8f4ba` "Merge pull request #108 … lit-vector-occlusion". `Tools/BakeMeshes.py:76` and `NeuronClient/MeshData.cpp:19` set NMF version 3, and `--check` passes (A6).
- **Reference shows:** LV2.1 built (baked occlusion is in). **Reported by:** A6 (C4), A7 (Open 2), lead. **Class:** stale-doc (plan status only) · **OWNER?** no

### C-036a — The brief's vocabulary and interface steps
- **Sides:**
  - The brief's ground rule 5 spells "Defense Platform". The screen and the data spell **Defence Platform**, **Defence gun** and **Defence Autoloader** (`Tuning.json:37, 61, 92`; `Hud.cpp:2659` "{} Defence guns, "). Code identifiers spell `DefensePlatform` (AGENTS.md R11).
  - The brief names interface steps of 100/110/125%. `Hud.h:861` and `ADR-070` decision 2 give 100, 110, 125, 150, 175 and 200%. A 16:9 screen reaches 110% in a match and 125% on the menu (A4 computed reach).
- **Reference shows:** "Defence" on screen and in data, and six steps. **Reported by:** A4 (C16, Interface scale), A5 (Open). **Class:** stale-doc (brief) · **OWNER?** no

---

## superseded

### C-037 — Protocol and world-state versions in the Phase 5 plan
- **Sides:** `Plan5.md:239`: "The protocol is at version 15 and a world's state at version 6". `Plan5.md:97`: `PROTOCOL_VERSION` 11. `Plan5.md:147`: 12 and `WORLD_STATE_VERSION` 2. Superseded by the owner's 2026-10-09 decision in `AGENTS.md` R18, `ADR-060:17`, `ADR-077:24` and `Phase5.md:81`.
- **Reference shows:** 1 and 1 (`WorldState.h:9`, `WireFormat.h:7`).
- **Reported by:** A3 (Conflict 2), lead. **Class:** superseded. A plan is a work queue, and AGENTS.md is authority 1. · **OWNER?** no

### C-038 — Mining Rig income rates
- **Sides:** `MVP.md:147` gives home 5 and contested 8. `Phase1.md:216-219` gives 5, 6, 8, 10. `Phase2.md:15` gives 5, 6, 8, 10. `Phase4.md:216` gives 3.5, 4, 5.5, 6.5, and `ADR-036` decision 3 agrees.
- **Reference shows:** `Tuning.json:6-9`: 3.5 / 4 / 5.5 / 6.5 Ore/s. **Reported by:** A1 (C1). **Class:** superseded · **OWNER?** no

### C-039 — A rig's payback at home
- **Sides:** `MVP.md:149`: "pays for itself in 10 s at home". *Derived* from the code: 50 ÷ 3.5 ≈ 14.3 s (`Tuning.json:6, 60`).
- **Reference shows:** ≈14.3 s (derived). **Reported by:** A1 (C2). **Class:** superseded · **OWNER?** no

### C-040 — Reserve durations in Phase 1
- **Sides:** `Phase1.md:216-219` gives home, near and contested 25 min and rich 40 min. `Phase1.md:206`: "home rigs run dry around minute 25–30". `Phase4.md:216` lowered the rates without restating these.
- **Reference shows:** derived 35:43 / 37:30 / 36:22 / 61:32 (see C-013). **Reported by:** A1 (C3). **Class:** superseded · **OWNER?** no

### C-041 — Domination's drain
- **Sides:** `Phase2.md:110, 166` (gate J4) and `ImplementationPlan-Phase2.md:131`: every 10 s, 30 × the difference ÷ the map's nodes. `Phase4.md:259, 377` and `ADR-057` decisions 1–2: 3 s, 1 ticket a node.
- **Reference shows:** `Tuning.json:14` `drainIntervalSeconds` 3, `drainTicketsPerNodeDifference` 1. **Reported by:** A1 (C4). **Class:** superseded · **OWNER?** no

### C-042 — Node caps by Command Station level
- **Sides:** `Phase3.md:114-118, 122, 173` (gate K4): 3, 4, 5, 6, 7, and 3 without a station. `Phase4.md:231-235, 259` (gate L4) and `ADR-064:41`: 4, 7, 10, 13, 16.
- **Reference shows:** `Tuning.json:41-46`: 4 / 7 / 10 / 13 / 16, and 4 without a station (`Simulation.cpp:2912-2920`). **Reported by:** A1 (C5), A2 (Conflict 6). **Class:** superseded · **OWNER?** no

### C-043 — The map's size, sectors and asteroids
- **Sides:** `MVP.md:135`: about 2,000 × 2,000 m and about 12 asteroids. `Phase1.md:212-221`: 5 km and 24 asteroids, 285,000 Ore. `Phase2.md:65`: nine sectors. `Phase4.md:248` and `ADR-036` decision 4: 10 km and 25 sectors.
- **Reference shows:** `Map.json`: 10,000 m, 25 sectors, 40 ore asteroids, 459,000 Ore (derived). **Reported by:** A1 (C6). **Class:** superseded · **OWNER?** no

### C-044 — Derelict Ore range
- **Sides:** `Phase4.md:307, 402` (gate L8) and `ADR-074:10` (Context): 300 to 900. `Phase4.md:311` (as built) and ADR-074 (owner, 2026-10-07): 300 / 450 / 600 / 750.
- **Reference shows:** `Map.json:165-168`: 300 / 450 / 600 / 750. Outpost wrecks give 600 and 1,200 (`Tuning.json:124, 127`). **Reported by:** A1 (C7). **Class:** superseded · **OWNER?** no

### C-045 — Where a cleared outpost's derelict lies
- **Sides:** `Phase4.md:289`: "where its largest structure stood". `Phase4.md:296` and `ADR-074:48` (decision 6): where its last Defence Platform fell, the first in identifier order if the last fall together.
- **Reference shows:** `Simulation.cpp:1738-1764`, which matches ADR-074: the first destroyed pirate structure of the sector in the tick its last structure fell. **Reported by:** A1 (C8). **Class:** superseded · **OWNER?** no
- **R checked:** `Simulation.cpp:1738-1764` and its comment.

### C-046 — Deep Core Survey
- **Sides:**
  - `Phase1.md:142` ("Every asteroid's ore reserve +30%") and `Phase1.md:204` ("adds 30% to every reserve … including what is left").
  - `ADR-035:13` (decision 3): the reserve is not raised. The researcher's rigs draw a 1.3rd less for the same Ore. `ADR-035:7` (Context) restates the design.
  - On screen, the research card's effect reads "Asteroid ore reserves +30%" (`GameLogic/Research.cpp:167-168`).
- **Reference shows:** the reserve figure is not raised, and the draw is divided by 1.3 (`Simulation.cpp:3023-3024`). A player's "Ore left" does not jump by 30%; it drains more slowly. The effect text is shown verbatim.
- **Reported by:** A1 (C9). **Class:** superseded (the ADR chose the mechanism) · **OWNER?** no
- **R checked:** `ADR-035:7, 13`, `Simulation.cpp:3015-3027`, `Research.cpp:164-170`.

### C-047 — The MVP's win condition
- **Sides:** `MVP.md:159, 498`: losing the Command Station loses. `Phase1.md:70` and `ADR-037` decision 1: neither a Command Station nor a finished Shipyard.
- **Reference shows:** `Simulation.cpp:3683-3710`. **Reported by:** A1 (C11). **Class:** superseded · **OWNER?** no

### C-048 — Depletion in the MVP
- **Sides:** `MVP.md:146, 439`: no depletion. `Phase1.md:202-206`: reserves.
- **Reference shows:** `Map.json` reserves and `Simulation.cpp:3015-3027`. **Reported by:** A1 (C12). **Class:** superseded · **OWNER?** no

### C-049 — Ore a home-only player earns
- **Sides:** `Phase4.md:15` ("Three home rigs pay 15 Ore/s") describes the game before Phase 4's change. `Phase4.md:216` gives home 3.5.
- **Reference shows:** 3 × 3.5 = 10.5 Ore/s before research (derived). **Reported by:** A1 (C13). **Class:** superseded · **OWNER?** no

### C-050 — Does the designer show shots to kill?
- **Sides:** `MVP.md:224` lists "damage per second after armour, shots to kill, range and cost". `Phase1.md:272-281` lists the window's contents without shots to kill. `Hud.cpp:585-807` has no such figure.
- **Reference shows:** no shots-to-kill figure. **Reported by:** A2 (Conflict 5). **Class:** superseded · **OWNER?** no

### C-051 — Component costs
- **Sides:**
  - `MVP.md:346-355, 337`: Small 32, Medium 110, Mass Driver 35, Lance 85, Missile Rack 130; Large 300.
  - `Phase1.md:89, 90, 107`: Flak Battery 60, Rail Cannon 150, Pulse Drive 40.
  - `Phase2.md:132`: Sensor Array 40.
  - Phase 4 §4 tripled them, and `ADR-058:37` says "three times Phase 2's 40".
- **Reference shows:** `Tuning.json:16-33`: hulls 96 / 330 / 900; drives 60 / 240 / 120; weapons 105 / 255 / 390 / 180 / 450; Sensor Array 120. **Reported by:** A2 (Conflicts 7, 8, 9). **Class:** superseded · **OWNER?** no

### C-052 — The research tree's size and totals
- **Sides:** `Phase1.md:119, 139, 148, 157`: 25 topics with gateways 9 and 18, 2,590 s. `Phase1.md:51, 319`: the AI researches about 25 topics. `MVP.md:246`: the whole tree takes 11½ min (690 s). Against them: `Phase3.md:153`, `ADR-033:12`, `ADR-082` Consequences ("all 23 topics") and `Opponent.json:16` (23 topics).
- **Reference shows:** `Tuning.json:65-112`: 23 topics, 2,260 s, 6,800 Ore. **Reported by:** A2 (Conflicts 10, 11), A3 (Conflict 8). **Class:** superseded · **OWNER?** no

### C-053 — Armor on unarmed structures
- **Sides:** `MVP.md:170`: "Every other structure has no armour". `Tuning.json:62-63`: Relay 10, Repair Bay 5.
- **Reference shows:** 10 and 5. The Shipyard, Research Lab and Mining Rig have 0. **Reported by:** A2 (Conflict 12). **Class:** superseded · **OWNER?** no

### C-054 — How later structure levels are fitted
- **Sides:** `Phase3.md:65`: each level's mesh "is fitted to the kind's length as level 1's is". `ADR-064:28` (decision 8): `FitMeshAcross` to level 1's widest. `ModelCatalog.cpp:382-385`.
- **Reference shows:** `FitMeshAcross`. **Reported by:** A2 (Conflict 14), A6 (Size row). **Class:** superseded · **OWNER?** no

### C-055 — When the AI's first scout sets out
- **Sides:** `Phase2.md:151`: "in the first two minutes". `ADR-020:49`: "in the first three minutes, at about 2:30" (a measured outcome).
- **Reference shows:** no time rule in code. The scout is queued ahead of warships and of a structure waiting for Ore (`AiPlayer.cpp:1071-1101`). **Reported by:** A3 (Conflict 3). **Class:** superseded · **OWNER?** no

### C-056 — The AI's attack group, fall-back and regroup
- **Sides:** `MVP.md:300` (gate G9): 12 ships. `Phase1.md:324-325`: 20 ships, a 30% fall-back, 120 s. `ADR-041` decisions 1–2.
- **Reference shows:** `Opponent.json:2-5`: 25 + 12 per tier, 0.15, 240 s (Normal). **Reported by:** A3 (Conflicts 4, 5). **Class:** superseded · **OWNER?** no

### C-057 — AI difficulty out of scope, and the menu's contents
- **Sides:** `MVP.md:304, 403`: no difficulty setting. `MVP.md:289`: "Menu: Start skirmish, Quit. Nothing else." Against them: `ADR-065` decision 4, `ADR-078` decision 9, and `Hud.cpp:2329-2334`.
- **Reference shows:** "Easy skirmish", "Normal skirmish", "Hard skirmish", "Join world" (with a join file) and "Quit". **Reported by:** A3 (Conflicts 6, 7), A4 (C9). **Class:** superseded · **OWNER?** no

### C-058 — The AI's salvage crew
- **Sides:** `Phase4.md:353`: "its first Constructors on salvage trips" (plural). `ADR-076:12, 30`: one Constructor at a time.
- **Reference shows:** one at a time (`AiPlayer.cpp:1835-1891`). **Reported by:** A3 (Conflict 10). **Class:** superseded · **OWNER?** no

### C-059 — Number of actions in the Controls window
- **Sides:** `PlanUI1.md:240`: "26 actions under three headings". `KeyBindings.h:93-125`: 28 actions (12 + 8 + 8). O and Ctrl+= / Ctrl+- came later.
- **Reference shows:** 28 actions, 3 headings, 762 units tall. **Reported by:** A3 (Conflict 12), A4 (C14). **Class:** superseded · **OWNER?** no

### C-060 — Interface details superseded by later ADRs and plans
- **Sides:**
  - **Designer size.** `Phase1.md:310`: about 38% × 65% of the screen. `ADR-062:39`: 818 × 913 (43% × 85%).
  - **Designer header.** `Phase1.md:268` and the mockup: "SHIPYARD 01 · DESIGNER" and "QUEUE · 0 BUILT". `ADR-068` decision 1 and `Hud.cpp:1365-1373`: "< SHIPYARD 01 > · DESIGNER", "QUEUE · 0 / 5", "BUILT · 0".
  - **Damage heading.** The mockup: "DAMAGE / S AFTER ARMOR · VS HULL". `Hud.cpp:1510`: "… · VS A FORMATION".
  - **Income of nothing.** `Phase1.md:307`: in the warning's color. `ADR-085` decision 1 and `Hud.cpp:3152-3156`: a chip.
  - **Idle status line.** `PlanUI1.md:134`: in the warning's color. `PlanUI2.md` UI2.1 and `Hud.cpp:3277-3280`: a chip.
  - **Health bar size.** `Phase1.md:307`: "a bar keeps a least size". `ADR-088` decision 5 and `Hud.cpp:3096-3100`: a fixed 32 × 5 at the back buffer's scale.
  - **Status line and window top.** `PlanUI1.md:143` (task 15.1): "Shipyards: 2 building, 1 waiting for Ore, 1 IDLE", windows at 148. `ADR-066` decisions 1 and 3 and `Hud.cpp:403-410, 1635`: "Shipyards · BUILDING 2 · WAITING 1 · IDLE 1", windows at 178.
- **Reference shows:** the code's in every case. **Reported by:** A4 (C6, C7, C8, C10, C11, C15), A5 (C9, Mockup row 12). **Class:** superseded · **OWNER?** no

### C-061 — The MVP's camera and sky
- **Sides:**
  - `MVP.md:139, 510`: widest view 1,600 m. `Camera.json:4` gives 3,000 m, and `Phase4.md:400` (gate L6) set it.
  - `MVP.md:324`: "the brightest few have the spikes of a starburst". `ADR-028:14-15`, `GameClient.cpp:466` (`Shape::Cross`) and `StarCrossPS.hlsl`: one-pixel crosses. `starburst.dds` is not drawn.
  - `MVP.md:324`: "a barely visible gray grid". `GameClient.cpp:32` (0.012, 0.013, 0.019), and ADR-028 calls it "dim blue-gray".
- **Reference shows:** 3,000 m; crosses; (0.012, 0.013, 0.019). **Reported by:** A7 (Conflicts 2, 3, 8). **Class:** superseded · **OWNER?** no

### C-062 — Ore's gold against the mockup
- **Sides:** The mockup (`GameDesign/Mockups/ShipDesigner.png`, sampled by A5) has gold `#F7D364`, linear (0.93, 0.651, 0.127). `Hud.cpp:986` has `GOLD_COLOR` (0.93, 0.50, 0.045) `#F7BC3C`, under the comment at `Hud.cpp:976` ("The mockup's colors … sampled from it"). `ImplementationPlan-Phase1.md:210`: "Ore stays the HUD's gold outside the windows". `ADR-062` decision 3: "The gold stays the gold of every other cost."
- **Reference shows:** `#F7BC3C`. The mockup is evidence, not ground truth. **Reported by:** A5 (C10, Mockup row 2). **Class:** superseded (the code comment is stale) · **OWNER?** no

### C-063 — Other mockup differences with no recorded decision
- **Sides:** The mockup shows a title-bar ground `#222A3C` with stripes `#2A3243`, gradient fills on the picked card and stat bars, and "SAVED" in about `#74D580`. The code has `WINDOW_COLOR` with `TITLE_HATCH_COLOR` stripes, solid fills only (`Hud.h:630-637`), and `QUEUE_EDGE_COLOR` `#559361`.
- **Reference shows:** the code's. **Reported by:** A5 (Mockup rows 5, 10, 11, 14). **Class:** superseded (the mockup is evidence) · **OWNER?** no

### C-064 — Scope of difficulty in the Phase 3 design
- **Sides:** The Phase 3 design does not mention difficulty. It came from plan task 26.1 (`ImplementationPlan-Phase3.md:255-262`) and is recorded in `ADR-065`.
- **Reference shows:** three difficulties (`OpponentEasy.json`, `Opponent.json`, `OpponentHard.json`). **Reported by:** A3 (Conflict 6, note). **Class:** superseded (informational) · **OWNER?** no

---

## agent-disagreement

### C-065 — "Defence Platform" against "Defense Platform" in the agents' prose
- **Sides:** A1, A2 and A4 write **Defence Platform** and **Defence gun**, as the data does (`Tuning.json:37, 61`). A6 (`A6.md:54, 260`) and A7 (`A7.md:291, 455`) write "Defense Platform". The code kind is `DefensePlatform` (AGENTS.md R11), and the brief writes "Defense Platform".
- **Which is right:** the screen shows **Defence Platform** (build button `Hud.cpp:2821` with the type's `nameUtf8` from `Tuning.json:61`). The reference spells it so, and uses `DefensePlatform` only for the code identifier.
- **Reported by:** A4 (C16). R found it in A6 and A7. **Class:** agent-disagreement · **OWNER?** no

### C-066 — How a research topic card looks when its tier is not open
- **Sides:**
  - **A2** (Research tree, rules): "A topic not yet open is dim, and its first need reads 'RESEARCH LAB L2' … Each card reads 'TIER n · s'".
  - **A4** (§4.3): a blocked topic is drawn on `LOCKED_COLOR` with `LOCKED_HATCH_COLOR` stripes. Its needs read `NEEDS · RESEARCH LAB L2`, then `+ {Y}`. The `TIER {t} · {s} s` line shows only when nothing is needed.
- **Which is right:** **A4.** `Hud.cpp:1849-1855` gives a blocked card the hatched face. `Hud.cpp:1872-1881` writes "NEEDS·" and draws the time line only `if (!blocked)`. `Hud.cpp:2971-2972` inserts `RESEARCH LAB L{n}` first among the needs.
- **Reported by:** R (cross-check). **Class:** agent-disagreement · **OWNER?** no

### C-067 — Do derelicts carry bars?
- **Sides:** A7 (Open 5): "whether derelicts carry hit points in the snapshot is Not specified in the sources read". A1 and A2: a derelict has no hit points (`ADR-074:27`, decision 1).
- **Which is right:** **A1 and A2.** `Hud::BarOver` (`Hud.cpp:2176`) shows a health bar only when `maxHitPointsHundredths > 0`. `EntityView::builtPermille` defaults to `PERMILLE` (`Snapshot.h:102`), so no build bar either. A derelict shows no bar.
- **Reported by:** R (cross-check). **Class:** agent-disagreement · **OWNER?** no

---

## Counts

| Class | Entries | OWNER? = yes |
|---|---|---|
| code-vs-intent | 5 (C-001 to C-005) | 5 |
| stale-doc | 32 (C-006 to C-036a) | 0 |
| superseded | 28 (C-037 to C-064) | 0 |
| agent-disagreement | 3 (C-065 to C-067) | 0 |
| **All** | **68** | **5** |

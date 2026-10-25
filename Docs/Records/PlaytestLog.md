# Outpost Commander — Playtest log

Every owner run, and every other run a plan, a design, an ADR or a budget names, done and to come. Its entries are dated and appended, and an entry is changed afterwards only to correct a fact. Its Outstanding table is kept current: it lists every run still to come, and is the home of each one's status until its entry is appended.

## Outstanding

Each row is a run that no entry below records. When it is made, its entry is appended under [Entries](#entries), its figures go to [BalanceLog.md](BalanceLog.md) and the latest of each to its row of [Budgets.md](../Budgets.md), the plan's board takes its status where a plan holds it, and its row leaves this table. A plan cited here is the open plan of that name in `Docs/Plans/`. A closed plan or design, which the tree no longer holds, is cited with the commit that wrote what is cited, or else as it stood at commit `8366820`, the last on `main` before the closed documents were deleted.

### The Phase 5 plan

From [ImplementationPlan-Phase5.md](../Plans/ImplementationPlan-Phase5.md) and the open part of [OutpostCommander-Phase5.md](../Plans/OutpostCommander-Phase5.md). The plan names no configuration for these runs unless a row says so.

| Run | Build and configuration | What to look at | Acceptance | Source |
|---|---|---|---|---|
| A world made with `OutpostServer --new-world <folder>` and run with `OutpostServer <folder>` on the owner's machine, then stopped with Ctrl+C | `OutpostServer` on the owner's PC, or a Windows Server 2022 VM chosen at the run (gate H1) | `World.json` (address, port, host, seed, and a seat with a token for each start); the saves and the command log written to the folder; the kept certificate; the join files; the server running until Ctrl+C or its console is closed | The world runs, and is left saved when it stops. CI and the container already pass `WorldSettingsTests` and `--new-world`. | Task 36.1 |
| The game on a second machine joins the owner's world with its join file, plays, loses its connection when the server stops, and joins again when it runs | The packaged game on a second machine, against task 36.1's world | "Join world" on the menu when the player's documents hold `Outpost Commander\Join.json`, or `--join <file>`; play on the remote seat; the return to the menu with the reason in the warning's color when the server stops; joining again | It joins, plays, is told why its connection was lost, and joins again. CI passes `HudTests.OffersAWorldAndSaysWhyTheLastGameEnded` and every `QuicTransportTests` case. | Task 36.3 |
| A world with an AI empire, left for an hour, and the deputy's play judged on the player's return | A world made with `OutpostServer --new-world --ai <player>[:<difficulty>]`, and the game | What the deputy kept running while the owner was away (Game §ai.deputy): research and production queues, standing orders, retreat thresholds, rigs rebuilt, Constructors kept, the defense of held sectors and the defenders' return, and no attacks; whether its defense, which draws every idle warship to any enemy warship in a held sector, is good enough (ADR-079); the hand-over 60 s after the connection drops, and back on return; the away panel of task 38.3 | The owner judges the deputy's play on return. W3's figure is the container's, and done (BalanceLog.md, 2026-10-08). | Task 37.3 |
| W5's live half: a 02:00 attack given from the orders window, and fired, in play | The game against an `OutpostServer` world | The orders window (O): a time of day on the player's own clock, the action and the condition; the selection panel's pending order; the order firing at its tick, and its alert; the panel of what happened while away when the seat is taken again | The owner judges a 02:00 attack in play. W5's other half, each trigger, action and condition firing on its tick, passes in `ScheduledOrderTests` in the container and CI. | Task 38.3; the Phase 5 design's W5 |
| A seat that loses in a world, its banner, and its restart, in play | The game against an `OutpostServer` world | The banner saying when the seat restarts, or that it waits for its start; the seat restarting on the hour with a Command Station, the starting Constructors and its Ore raised to the starting Ore; an AI empire planning afresh round its new Command Station | Seen in play. In the container's day of world Red restarted 11 times, never waiting, and 7 of its bases fell 12–14.5 s after their restart (BalanceLog.md, 2026-10-08). Once task 39.5 is built, a start held by enemy warships is not free, and the seat waits (owner, 2026-10-25, gate H11). | Task 39.1; milestone 39's As built |
| W4's first half: the battle matchups played live against the AI | The game, `OutpostCommander --matchup <n>` | Each matchup of `Matchups.json`, fought live on the 10 km map without bases, against the AI's battle behavior | The owner's win rate above the AI-against-AI baseline is what presence buys. The baseline, 40 seeds a matchup with side 1 first, is in BalanceLog.md (2026-10-08). | Task 39.3; the Phase 5 design's W4 |
| W1, the owner's week: the owner's world runs for 7 days on the dedicated server, through at least one planned restart and one killed process | `OutpostServer` on the owner's PC or a Windows Server 2022 VM (gate H1) | The world log: each save and how long it took, each recovery, each hand-over, the scheduled orders fired, losses and restarts, the hourly banks, income, fleets, nodes and research, and battles with presence; the command log across the restart and the killed process | The world survives the week and loses no command the log had written. | Task 39.4; the Phase 5 design's W1 |
| W2's tick in Release: the server's tick with the deputies and the saves in | Release on the development machine, with the deputies in; the plan names no switch for it | The 99th percentile tick, the save's time on the server's thread and the save's size, on a late world | U7's 99th percentile tick, at most 5 ms, holds with the deputies and the saves in; a late world's save takes under 50 ms of the server's thread; the save's size is recorded. The container's figures are in BalanceLog.md (2026-10-08). | Tasks 35.2 and 39.4; the Phase 5 design's W2 |
| W4's second half: over the week, the owner's income, fleet against its cap and research done per hour, against its deputy's | The week's world (W1) and its world log | The world log's hourly records for each seat, split by who controlled it | Recorded and compared; the design gives no threshold. A deputy that out-builds its player makes absence the better strategy. | Task 39.4; the Phase 5 design's W4 |
| W6: over the week, whether three check-ins a day are worth making | The week's world (W1) | The rhythm of play over the week, and the week's log | The owner's judgement. With the week's log it decides gate H10: a galaxy or friends next. | Task 39.4; the Phase 5 design's W6 and gate H10 |

### Phase 4's runs

Phase 4 closed on 2026-10-08 with U7, U8 and the owner's own AI-against-AI figures left to the owner ([PostMortems.md](PostMortems.md), Phase 4). No open plan holds them.

| Run | Build and configuration | What to look at | Acceptance | Source |
|---|---|---|---|---|
| U7, the engine at 10 km: `--measure` on an AI-against-AI match for the 99th percentile tick, and a frame capture zoomed out over the largest fight at 1920×1080 | Release on the development machine | The tick timings of a whole AI-against-AI match; the frame times zoomed out over the largest fight at 1920×1080 | The 99th percentile tick at most 5 ms in an AI-against-AI match, and 99% of frames at most 16.7 ms at 1920×1080 over the largest fight. In the container the median match's 99th percentile tick is 1.4 ms and the worst match's 3.3 ms (BalanceLog.md, 2026-10-07). If U7 misses, pathing by sector is the next step, and is decided in an ADR (owner, 2026-10-07). The same run gives how late ticks start, which ADR-055 leaves to it, and the development machine's figures that ADR-060 has none of: a snapshot's encoding and decoding under MSVC, and a match's key, certificate and handshakes. | The Phase 4 design's U7; the Phase 4 plan, tasks 28.4 and 34.2 |
| U8: a match against the AI on the 10 km map | The game | Whether the opening feels like exploring (scouting, salvage, pirate camps), and whether a 10 km map can be followed with the camera, the minimap and the alerts | The owner's judgement in play. A strategic view is designed only if this run asks for it. It is also the owner's look at what the AI does against a human, which ADR-020, ADR-041 and ADR-065 leave to the owner's runs; the owner found Easy beatable on 2026-10-04. | The Phase 4 design's U8; the Phase 4 plan, task 34.2 |
| The owner's own AI-against-AI figures: `OutpostCommander.exe --ai-matches --matches 40 --limit-minutes 180`, then `python Tools/MatchLog.py --ai-matches` | The MSVC build; the configuration is not named. MSVC may play the seeds differently from clang (ADR-009) | U1 to U5 as `Tools/MatchLog.py` scores them, beside the container's (BalanceLog.md, 2026-10-07). It is also the first run of `--ai-matches` on Windows (ADR-038). | The figures recorded. | The Phase 4 plan, task 34.2 |

### The Interface plan

From [ImplementationPlan-Interface.md](../Plans/ImplementationPlan-Interface.md). The owner confirmed on 2026-10-25 that no owner run of this plan, or of the second interface plan, has been done. Its rule 5 has every task run and looked at, at 1920×1080 and at 2880×1920, and accepted by the owner's run; its rule 6 has no text outside its panel or over another. Every run here is of the game, in any configuration, at those two sizes.

| Run | Build and configuration | What to look at | Acceptance | Source |
|---|---|---|---|---|
| Run and owner run: text measured with its fonts and kept inside its card | The game, 1920×1080 and 2880×1920 | Every text inside its panel: Relay Archives' prerequisites one to a line ("NEEDS · IMPROVED EXTRACTION", then "+ HULL PLATING"); a part card's splash note and lock line on lines of their own; names cut to their card's width; the territory panel and the alerts | No text outside its panel or over another, on the screen as in `KeepsEveryTextInsideItsPanel` and `OverlapsNoTwoTexts`; the owner's run accepts it. | Task 14.1 |
| A check: `KeepsEveryTextInsideItsPanel` fails at Relay Archives on the layout before the fix | A build with the game's fonts, such as CI's Debug\|x64, of the test run against the HUD's layout as it was before #65 | The test's result | It fails at Relay Archives, as task 14.1's acceptance asks. ADR-061 recorded that this had not been shown. | Task 14.1; ADR-061's Consequences as they stood at `8366820` (commit `4bbbdcd`, #65) |
| Run, with the designer beside the mockup, and owner run: a type scale that reads | As above | `Hud::FaceUnits`' sizes (Label 13, Figure 14 at weight 600, Detail 13); the locked text at (0.20, 0.26, 0.35); the part cards 220 wide and the designer 818; production 478 and research 560; the designer beside `Mockups/ShipDesigner.png` | Every text at 4.5:1 against its panel; the owner's run closes gate K1's sizes. | Task 14.2; gate K1 |
| Run and owner run: what production and research are doing, on the HUD | As above | The status panel under the Ore (the Lab's line, the Shipyards' line, IDLE); a click on a line opening its window; the selection panel's line for a producer or the Lab | The owner's run accepts it. The second interface plan's UI3.1 and UI3.2 rewrite this panel, and their owner run closes this task with them. | Task 15.1 |
| Run and owner run: keys on the buttons | As above | A key cap at the right end of each window button: P on Production, D on Ship designer, R on Research; none on a Build button | The owner's run accepts it. | Task 15.2 |
| Run and owner run: the designer's preview in figures | As above | While a part is hovered, each bar reads the previewed figure and its signed change, such as "242 (+44)" and "1,680 (+1,482)"; a damage card's change on its large figure's line; a number that does not change reads alone | The owner's run accepts it. | Task 15.3 |
| Run: windows open where there is room | As above | Research alone opens at the first slot under the Ore; production then research, and research then production, side by side in the order opened; a moved window kept where it was left; both clear of the designer | A run of the game; the board names no owner run for this task. | Task 15.4 |
| Run, near and far, and owner run: the selection ring at a fixed width | As above, at the nearest and the farthest zoom | The selection's green ring and attack-move's amber one as one-pixel lines a pixel apart, about 3 px at 1080p at any zoom, still the strongest mark on the ground | The owner's run accepts it (gate K3). | Task 15.5 |
| Run and owner run: the windows' headers and bodies | As above | "BUILT · 4" left of the slots; "< SHIPYARD 05 > · DESIGNER" with 24-unit arrows; research's "TOPICS · 1-10 OF 21"; an opaque window body | The owner's run accepts it. | Task 16.1 |
| Run and owner run: the minimap's marks and size | As above | Ore asteroids as outlined squares, dry asteroids outlined; fields at 2.1:1 and dry asteroids at 3.1:1 against the map; the minimap 300 units square | The owner's run accepts it. The second interface plan's UI1.4 rewrites the minimap, and its owner run closes this task with it. | Task 16.2 |
| Run and owner run: what a production card says | As above | A Shipyard's card with its design's build time and the S, M and L bars of three, two or one segments; the Constructor's card with its build time; a card 74 units tall | The owner's run accepts it. | Task 16.3 |
| Run and owner run: a Controls window | As above | F1 opening and closing the Controls window in the middle of the screen, its actions under three headings; the main menu unchanged (owner, 2026-10-05, gate K4) | The owner's run accepts it. | Task 16.4; gate K4 |
| Run: short names for the starting designs | As above | Swarm, Picket, Brawler and Lancer (owner, 2026-10-05, gate K6) shown whole in the chips and on the cards; the AI's designs keeping their components' names | A run of the game; the board names no owner run for this task. | Task 16.5 |
| Run at each step, and owner run: an interface scale the player sets | As above, at each step | Ctrl+= and Ctrl+- stepping through 100%, 110%, 125%, 150%, 175% and 200%, taken only while the designer and the Controls window fit (110% in a match on 16:9, 125% on 2880×1920); a moved window staying on the screen; the factor taken down on a new screen size or a new match; the HUD, the windows and the menu scaled, and not the world's marks | The owner's run accepts it. | Task 17.1 |
| Owner run: motion and flashes, which stills cannot show | The game | A battle at the peak ship counts, watched for flashing; the beam's 0.25 s, the blasts and the camera's feel | What it finds becomes a task. | Both interface plans, "Not in this plan" |

### The second interface plan

From [ImplementationPlan-Interface2.md](../Plans/ImplementationPlan-Interface2.md), whose rules 5 and 6 are the Interface plan's, with `SetsEveryTextAtFourAndAHalfToOne` beside its two text tests; its rule 7 has the owner's run of a rewrite close the first plan's task with it. No owner run of this plan has been done (owner, 2026-10-25).

| Run | Build and configuration | What to look at | Acceptance | Source |
|---|---|---|---|---|
| Run, at the default zoom and the widest, and owner run: sectors and nodes in the world | The game, 1920×1080 and 2880×1920 | Every sector's border as one-pixel lines: a held one in its holder's color inset 4 m, two holders' borders side by side; a neutral one brighter than the grid; a guarded one in the pirates' color; suppressed and cut-off borders in dashes and dots; all drawn over the fog. The nodes the player could claim marked only while it places a Relay (gate V2, revised 2026-10-09). | The owner's run accepts it, and with it the lattice's and outlines' strengths, the 4 m inset and the dashes and dots (ADR-081). | Task UI1.1; gate V2 |
| Run and owner run: memories that look like memories | As above | A remembered structure or derelict drawn after the fog as lines without faces or bars; "Shipyard · last seen 4:12 ago, 34% built" on hover and on its selection panel; a memory on the minimap as a cross over the fog | The owner's run accepts it, and with it the memory's shade (ADR-081). A click opens the remembered structure's panel since task RF2.4 of [the findings plan](../Plans/ImplementationPlan-Findings.md), whose run is under that plan. | Task UI1.2 |
| Run, measuring the grid's brightness in and out of sight as the review measured it, and owner run: sight that shows | As above | The ground's seen-before shade through the knee from 0.55 to 0.8, never seen kept at 0.9; the minimap still at 0.55. The measure: the median of each grid line's peak along a row, the Rec. 709 luma of the sRGB pixels, in sight and where seen before. | Outside sight the grid falls from 27 of 255 to about 7, computed (gate V2); the owner's run accepts it. | Task UI1.3 |
| Run and owner run: the minimap's territory | As above | The lattice at (0.11, 0.13, 0.17), 2.06:1 against the map; no mark on a node; while placing a Relay, each sector whose node the player could claim outlined 3 units inside its border in blue; a cut-off sector outlined in dashes of 6 with gaps of 4 | The owner's run accepts it, and closes the Interface plan's 16.2 with it. | Task UI1.4 |
| Run and owner run: a warning is a chip | As above | The chip, dark words on a filled tag of `WARNING_COLOR`, on a status line's IDLE ("2 IDLE"), an income of "+0/s" and the newest alert; the enemy's figures plain; the main menu's notice plain | The owner's run accepts it. | Task UI2.1; gate V4 |
| Run: build progress in a neutral color | The game | `BUILD_BAR_COLOR`, (0.5, 0.52, 0.55) linear, on an enemy structure under construction (the review's shot 5) and on an own one | The Verify line names a run; the board waits for the owner's run, and ADR-085 has the owner's run see it on an enemy's site and an own one. | Task UI2.2 |
| Run and owner run: the blast and Ore's glyph | The game, 1920×1080 and 2880×1920 | Ore written with the cut gem everywhere Ore is written, in `GOLD_COLOR`, beside a blast's amber diamond particles | The owner's run accepts it; the task may be dropped. | Task UI2.3 |
| Run and owner run: what holds the Shipyards back, and what frees them | As above | "Shipyards at the fleet cap · Station L4: +10", "upgrading, 62%", "Fleet 50 / 50, at the cap"; IDLE a chip only when an idle Shipyard could start a ship; the Lab's "every open topic done"; a click at the cap selecting the Command Station and moving the camera to it | The owner's run accepts it, and closes the Interface plan's 15.1 with it. | Task UI3.1 |
| Run and owner run: one column, and state in bars | As above | The Ore, status, territory and alerts panels at one fixed width; research's name over its bar with "+3"; "BUILDING 0 · WAITING 2 · IDLE 3"; the fleet as a bar with "29 / 30"; the windows opening 178 units from the top | The owner's run accepts it, and closes the Interface plan's 15.1 with it. | Task UI3.2 |
| Run and owner run: nodes and tickets that read | As above | The nodes as "3 : 2" with "cap 10 of 25"; the drain row "Enemy -20 a minute · out in 32:06", "You -60 a minute · out in 12:09" as a warning chip, or "No drain"; no ticket rows in a world | The owner's run accepts it. | Task UI3.3 |
| Run, turning the camera, and owner run: bars on the screen | As above, turning the camera with Q and E | Health and build bars 32 × 5 reference units above each entity's model, filling left to right however the camera has turned, under every panel and window; the ground bars' green, amber and red; no bar on a memory; and the selection rings chaining in a tight formation, which the plan looks at again at this run | The owner's run accepts it, seeing the bars as the camera turns (ADR-088). | Task UI4.1 |
| Run and owner run: commands beside the selection | As above | The command panel against the selection panel's right edge, the pair centered at the bottom (right of the minimap on screens narrower than about 3:2); Attack-move, Hold sector, Patrol and Stop with their key caps for a warship selection, the armed one lit; a button arming on the press | The owner's run accepts it. | Task UI4.2; gate V6 |
| Run and owner run: retreat as three choices | As above | The row "25%", "50%" and "Never" under RETREAT, the selection's setting lit, MIXED for a mixed selection; the designer's same row in the stepper's 206 × 48 units | The owner's run accepts it. | Task UI4.3 |
| Run: controls that answer the pointer | As above | The hovered button's edge lit, and the row of a clickable status line; a dim button naming the action under the pointer staying unlit; text that cannot be clicked unchanged | A run of the game; the board names no owner run for this task. | Task UI4.4 |
| Run and owner run: one count, one bar per ship, a short name | As above | "7 × Lancer+SA" titled once; a bar per ship for two to 24 ships, a retreating ship's outlined, a click selecting the ship and centering the camera; past 24, the single bar | The owner's run accepts it. | Task UI4.5 |
| Run at the default zoom, with the review's measure, and owner run: rocks that stand back | As above, at the default zoom | Rock faces at `ROCK_FILL_SHADE` 0.15 against the ships' 0.3, their lines unchanged. The measure: a field's lit pixels (luma above 35, in a box round the field) and its brightness against a group of ships. | The review's measure is repeated at the owner's run; the plan gives no target. Against the review's shot 1, the fields had 15,400 and 22,465 lit pixels against the six ships' 851 (BalanceLog.md, 2026-10-08). | Task UI5.1 |
| Run, owner run and the owner's `--measure` run: fields of their own | As above; `--measure` for the GPU time | Each field's 14 to 18 rocks laid out from its identifier, no gap along its ring wider than a Small hull's 16 m; 1,254 rocks on the 10 km map against 560 | The owner's run accepts it; the added rocks' GPU time is the owner's `--measure` run's (ADR-087). | Task UI5.2 |
| Run, turning the camera: the minimap's view as its four corners | As above, turning the camera | The view drawn on the minimap as its four ground corners joined, showing the near and the far side and turning with the camera, cut at the minimap's edge | A run of the game; the board names no owner run for this task, and ADR-015 has the owner's run see the outline turn. | Task UI5.3 |

### The Look plan

From [ImplementationPlan-Look.md](../Plans/ImplementationPlan-Look.md), whose rule 5 has every task run and looked at, and accepted by the owner's run.

| Run | Build and configuration | What to look at | Acceptance | Source |
|---|---|---|---|---|
| Owner run: occlusion baked into every vertex, in the game | The game; the plan names no configuration | Faces darkened in their recesses, and lines never; the Shipyards' bays and docks, the Command Station's crown, the ground round cockpits, fins and wing roots; the Tarkan Shipyard's frame, near black at `OCCLUSION_STRENGTH` 1 | The owner's run accepts it; the owner may want `OCCLUSION_STRENGTH` lower (ADR-092). The look has been judged only in an offline render of seven models. | Task LV2.1 (merged in #108) |
| Runs and owner runs of the trim and light paints, two-tone lines, beacons, the fleets' plates, and the health bars where the plates are not, once each is built | The game | Each task's look | Rule 5: each is run and looked at, and the owner's run accepts it. The tasks are todo, and LV4 waits for gate A7. | Tasks LV2.2, LV2.3, LV3.1 and LV4.1 to LV4.3 |

### The findings plan

From [ImplementationPlan-Findings.md](../Plans/ImplementationPlan-Findings.md), whose tasks that touch rendering, input or presentation are run, not only built, and accepted by the owner's run.

| Run | Build and configuration | What to look at | Acceptance | Source |
|---|---|---|---|---|
| Run and owner run: the designer's seventh bar | The game, 1920×1080 and 2880×1920, from #116 | The designer with the Sensor Array in the match: the Sensors bar and Build's row both above the footer; the window at 110% on 16:9 | No bar row meets the footer; the owner's run accepts it. | Task RF2.1; gate F3 |
| Run: what the Controls window says a click selects | The game, from #116 | F1: "Click · Select a ship of yours, or any structure", inside the window | A run of the game; the board names no owner run for this task. | Task RF2.2; gates F9 and F16 |
| Run: Give lights under the pointer | The game, from #116 | The orders window (O) with warships selected: Give's edge lit under the pointer, as a HUD button's is; dim Give unlit | A run of the game; the board names no owner run for this task. | Task RF2.3; gate F13 |
| Run and owner run: another player's structure selected | The game against the AI, from #116 | A click on an enemy structure in sight, and on one remembered under the fog: its panel alone, "Last seen 4:12 ago" for a memory, no buttons; a right-click and S give nothing; the selection gone once the memory is | The owner's run accepts it, and with it the memory's panel that task UI1.2 of the second interface plan laid out. | Task RF2.4; gate F16 |
| Owner run: the Human Command Station on a square plan | The game against the AI, the player's station upgraded to L5 | Each level 90 m across both ways on its 90 m footprint ring, the booms reaching the ring and nothing past it; the hull 45 m across and the station 61 m tall, half of L1's drawn size before the rework; the radiator fins standing on the upper hull, where the model sheet drew them 11 to 15 units off it; shots leaving from the three hull pads, and from L4 the three ring pads too | The owner's run accepts it. The bake is checked; nobody has seen it in the game. | Owner's track RF-O1; gate F2 |

### The self-play plan

From [ImplementationPlan-SelfPlay.md](../Plans/ImplementationPlan-SelfPlay.md) and [SelfPlayNetwork.md](../Plans/SelfPlayNetwork.md).

| Run | Build and configuration | What to look at | Acceptance | Source |
|---|---|---|---|---|
| Owner run: the first search, and what it found. It holds SP1.1's Windows run of the switch with `--quiet` and a bad option, showing the message on standard error. | Release\|x64 on the owner's x64 laptop | One match timed with `--ai-matches --matches 1`; `Tools\SelfPlay.py` run with `--generations` set to what the night allows; `report.txt`. On Windows also `CommandLineToArgvW`, standard error and the exit code, and whether the Release\|x64 build puts the game, with its Assets beside it, where the script looks first, `x64\Release\OutpostCommander\AppX\OutpostCommander.exe` (`--exe` names it otherwise) | The headroom figure, and what the champion does that the packaged AI does not, recorded in the plan, each finding sorted as a rule to change, a weakness of the scripted AI, or neither; the owner decides what follows (gate S1). It waits for SP1.5: `python3 Tools/SelfPlay.py --self-test` fails on `main` because `Opponent.json`'s `attackCapShare`, `pirateMargin` and `scoutOwnHalfSeconds` have no knob (entry of 2026-10-25). | Tasks SP1.3, SP1.5 and SP1.1; ADR-063's Consequences as they stood at `8366820` (commit `c2ecf4f`, #64) |
| Owner run of the trainer's long run, once SP2.2 is built | Release\|x64 on the owner's x64 laptop | The trainer's warm start from the scripted AI's choices, then PPO against it | The warmed-up network wins about half its matches against the packaged settings. The task is todo. | Task SP2.3 |
| Owner run: the league, and what the network found | Release\|x64 on the owner's x64 laptop | The blueprint's league trained for the owner's budget, and its report | The report: win rates against the packaged settings and SP1's champion, what the network does differently, which decision carries its advantage, how soon a lead decides a match, and what better decisions are worth over the scripted AI. The task is todo, after SP2.3. | Task SP2.4; the blueprint's league |

### The budgets' measurements

From [Budgets.md](../Budgets.md): each row is a measurement a budget's row still wants and no row above holds. Each figure goes to BalanceLog.md with its method, and the latest to its row of Budgets.md.

| Run | Build and configuration | What to look at | Acceptance | Source |
|---|---|---|---|---|
| The stress scene on the development machine: `--measure --stress` | Release on the development machine, on a build with ADR-092's occlusion | The stress scene's ticks, at the 99th percentile and the slowest; a frame's CPU work, which the second machine's run of 2026-10-11 does not give, and its GPU work; the light's and the glow's GPU work together and each apart, which needs a build without the other; the end of startup's device stage (ADR-049) | The figures recorded, each against its budget's target in Budgets.md. | Budgets.md: the stress scene's tick, its frame, the light and the glow, and startup |
| The parts of a frame, each apart | The game on the development machine, broken into regions as Budgets.md's method says (ADR-005) | The resolve's shader (ADR-050); four samples a pixel (ADR-040 decision 3); the sky's two draws of its stars (ADR-022); the asteroid fields' rocks (ADR-087), which the second interface plan's run of fields of their own also measures; the models' lines (ADR-027); the exhausts' glows (ADR-019); the explosions' shards (ADR-026); the Mining Rigs' rings, which look up their rock's surface every frame (ADR-042 decision 4); the Mining Rigs' stance on their rocks, fitted each frame from the rock's surface under each foot (ADR-044 decision 1); and the CPU work the instanced mesh draws save (ADR-053) | Each part's cost recorded. | Budgets.md: what each part of a frame costs |
| The interface's quads at their most | The game, any configuration | A frame's count of interface quads, with every window open and Alt held | The count recorded, against `UiPipeline::MAX_QUADS` (ADR-015 decision 2). | Budgets.md: a frame's interface quads |
| The container's figures on the development machine | Debug and Release on the development machine | A path graph's build, reduction and extension (ADR-054); the client's fog of war, an update from a snapshot (ADR-052) | The figures recorded. | Budgets.md: a path graph's build, and the client's fog of war |
| The tick's parts on the 10 km map | The Linux container or the development machine | What fog of war adds to a tick on the 10 km map (ADR-024); the pass over the entities that reveals the finished Shipyards of a player that has lost its Command Station, made each tick for each player only while some player has lost its station (ADR-037 decision 3); the whole seeded placement, outposts and derelicts included, and whether every seed still places (ADR-072) | The figures recorded. | Budgets.md: what fog of war adds, the reveal, and seeded placement |
| Order to response over QUIC | The game, Release, on the development machine, by Budgets.md's method | From an order to the ship visibly responding over QUIC on the loopback, as a skirmish runs; with a large order planned over two ticks, which sets off a tick later (ADR-032); and over a real network, a world joined from another machine (ADR-078) | The figures recorded against the target in Budgets.md. Over a real network the figure decides whether play needs an acknowledgement or a prediction (ADR-002). | Budgets.md: from an order to the ship visibly responding |
| A snapshot's size on the 10 km map | The Linux container or the development machine | Player 1's snapshot late in a match and in a world, and the bandwidth a player takes at a snapshot a tick; what the static part each snapshot repeats, the catalog of components and topics among it, adds (ADR-016, ADR-017) | The figures recorded. | Budgets.md: a snapshot's size, and the bandwidth a player takes |

### Runs no open plan holds

| Run | Build and configuration | What to look at | Acceptance | Source |
|---|---|---|---|---|
| Owner run: the HUD's tag saying where the match runs, and its warning when the server has gone quiet | The game, in a skirmish and against a dedicated server | The tag in the top-right corner naming the server's address, in a skirmish and on a dedicated server; the warning once the snapshots stop when the server is stopped | Seen in play. The owner asked for it on 2026-10-09 (#102); no plan task holds it. | ADR-086; Game §interface.hud.tag |
| A check: whether Windows Defender Firewall asks about a match's listener the first time a packaged build listens | The packaged game, its first match on a machine | Any firewall prompt as the first match starts. The listener binds 127.0.0.1 only, so it is not reachable from another machine. | Recorded. It has not been checked. | ADR-060 |
| Owner playtest of raids against the base: whether the Defence Platform's and the Command Station's thin margins against a raid hold in play | The game, against the AI | A raid on a lone Defence Platform and on the Command Station | The owner's judgement. The owner left the margins for the owner's playtests on 2026-10-01: in the hand checks the platform kept 28% of its hit points against five raiders, and the station's minute was 2.4 s from failing (BalanceLog.md, 2026-10-01). Whether they hold is this playtest's to judge, and the numbers stand until it does (owner, 2026-10-25). | The MVP design §12; [PostMortems.md](PostMortems.md), the MVP and the reconciliation's gate R10 |
| A check: the executable's imports, now that the renderer calls `check_hresult` | Debug and Release, x64 and ARM64 | `dumpbin /imports` of `OutpostCommander.exe`: what it takes from `ole32`, `oleaut32` and `runtimeobject` | Recorded. ADR-001 names what `<winrt/base.h>` and `check_hresult` reference. The imports were read on 2026-09-30, before anything called `check_hresult`, and not since (entry of 2026-09-30). | ADR-001's Consequences as they stood at `8366820` (commit `4323f42`, #34) |
| Owner run: whether the server's own thread removed the hitch at a big order | Release on the development machine | Both 100-ship fleets ordered at once, as `--measure --load` orders them, watched for a hitch, with the frame times and the order ticks' times | No hitch at the order. ADR-025 left it to be seen on the development machine. The order ticks' times are recorded against their target in Budgets.md. | ADR-025's Consequences as they stood at `8366820` (commit `842bbfb`, #48) |
| Run: the interface drawn over the resolved scene at one sample a pixel | The game, 1920×1080 and 2880×1920 | Panel edges that fall between pixels, each now drawn or not where four samples blended it; text and sprites, laid on whole pixels, unchanged | Seen on screen: ADR-050 called it a visible change to check. The resolve's GPU time is in Docs/Budgets.md. | ADR-050's Consequences as they stood at `8366820` (commit `f0d7ada`, #53) |
| A check: a failed assertion on Windows | Debug and Release, on x64 and ARM64 (ADR-093 decision 3); the game and `OutpostServer` | An assertion made to fail, as the container's `Main.cpp` did: its file, line and expression in the debugger's output, and on the dedicated server's console; the process ending with the fast-fail's code | As in the container (entry of 2026-10-11): the message written, and the process ended at once, in both configurations. | ADR-093's Consequences as they stood at `8366820` (commit `d647385`, #108) |
| Owner run: an order folds the Constructors' build buttons into one Build button | The game, from #110 | A selection with Constructors given an order, such as a placement, a move, an attack or a stop: the structures fold into Build, which opens them; a placement with Shift keeps them; a click, a box or a group recalled opens them again; the selection, the warships' orders and the retreat stay | Seen on screen; the owner's run accepts it. The tests ran in the Linux container, and CI is their first MSVC build. | ADR-088 decision 2 (owner, 2026-10-25); ADR-088's Consequences as they stood at `8366820` (commit `e2492bd`, #110) |
| A check: whether side 1 moving first in a tick is worth a first strike in the matchups | `OutpostServer --matchups`; the build is not named | The mirror's 114 to 86 for side 1 over seeds 1 to 200, 57%, a two-sided binomial p of about 0.06 (BalanceLog.md, 2026-10-08); side 1's ships are made first and may act first in a tick | Looked into, and what it finds recorded; no document gives a criterion. | ADR-083's Consequences as they stood at `8366820` (commit `b6d343b`, #98) |
| Owner run of the MVP plan's task D.1: dark faces under multisampled lines, and structures behind the ships | The game | Zoomed in on a structure, and zoomed out over a fight | The owner's run. The closed board left D.1 "in review; not yet built or run", though #50 merged it on 2026-10-03 (entry of 2026-10-03). The Look plan's runs of 2026-10-11 looked at the same scene under ADR-090's light, and no record ties them to D.1. The multisampling's GPU time is in Docs/Budgets.md. | The MVP plan's task D.1, as it stood at `8366820` (commit `11c3802`, #50) |

## Adding an entry

Append an entry under the last one, dated the day of the run, and take its row out of the Outstanding table. Its figures go to BalanceLog.md with their method, the latest of each to its row of Budgets.md, and a change of status goes to the plan's board where a plan holds the run.

```
### YYYY-MM-DD · What was run

- **Run:** what was run and looked at, and the task, gate or question it answers.
- **Build:** the commit or pull request, the machine, and the configuration.
- **Verdict:** what the owner found: accepted; not accepted, and what follows; or what was measured, with a pointer to BalanceLog.md.
- **Source:** where else the verdict is written: the plan's task, the pull request, or the commit.
```

## Entries

### 2026-09-30 · The checkers, run locally (MVP tasks 0.1 to 0.3)

- **Run:** `Build/CheckFormat.py` with clang-format 18 on Linux, and `Build/CheckProjectFiles.py` on Linux.
- **Build:** Linux, clang-format 18.1.3.
- **Verdict:** Run. Each exits 0 on the tree, and each was then gated in CI.
- **Source:** the MVP plan's tasks 0.1 to 0.3 and its board (commit `ca221af`, #26).

### 2026-09-30 · The window opens and closes (MVP task 1.1)

- **Run:** the owner's run of the packaged app: launched with F5, it opens a window and closes with exit code 0.
- **Build:** the packaged app; the configuration is not stated.
- **Verdict:** Run by the owner; done.
- **Source:** the MVP plan's task 1.1 and its board (commit `4c57c07`, #28, "Record G4 and the owner run of 1.1").

### 2026-09-30 · The device and the swap chain (MVP task 1.2)

- **Run:** the owner's run of the D3D12 device and the flip-model swap chain on x64 and ARM64: full screen, Alt+Enter to a 1280×720 window and back, resizing, a change of display resolution, minimize and restore, no debug-layer errors, and a message box on failure; and a PIX capture of the Debug build showing the named regions.
- **Build:** x64 and ARM64; the PIX capture of the Debug build.
- **Verdict:** Run by the owner; done. No detail of the run or of the PIX capture is recorded.
- **Source:** the MVP plan's task 1.2 and its board (commit `9e0ba0d`, #29).

### 2026-09-30 · Milestone 1 (MVP tasks 1.3 to 1.6)

- **Run:** the owner's runs of a Human and a Tarkan hull on screen, each facing its forward axis (1.3); a hull reading in two team colors (1.4); the RTS camera (1.5); and the milestone's review, the app on x64 and ARM64 in Debug and Release (1.6).
- **Build:** x64 and ARM64, Debug and Release for 1.6.
- **Verdict:** Run by the owner. For 1.3 the run confirmed which end of each Tarkan hull is the front. The owner closed milestone 1 on 2026-09-30 after running it.
- **Source:** the MVP plan's tasks 1.3 to 1.6 and its board (commits `23f0c1f` and `fdf40db`, #35).

### 2026-09-30 · Milestone 2 (MVP tasks 2.3, 2.5 and 2.6)

- **Run:** the owner's runs of the map once 2.5 renders it (2.3); smooth motion at 60 fps from a 20 Hz tick (2.5); and selection, orders and control groups (2.6).
- **Build:** not stated.
- **Verdict:** Run by the owner; 2.3's layout confirmed by the owner.
- **Source:** the MVP plan's tasks 2.3, 2.5 and 2.6 and its board (commits `ec4d980`, #34, and `fdf40db`, #35, "Mark milestones 1 and 2 done after the owner's run").

### 2026-09-30 · Q5 and the tick under load (MVP task 2.7)

- **Run:** the owner's measurement of Q5, the order-to-response delay at most 150 ms and the server boundary failing to compile, and of Q4's tick half, at most 5 ms at 200 ships and 40 structures, with `--measure` and `--measure --load`. Made on 2026-09-30 on the second machine and on 2026-10-01 on the development machine.
- **Build:** Release|x64 on the second machine, and Release|ARM64 on the development machine, each driven by a script (BalanceLog.md describes both machines).
- **Verdict:** Run. Q5 met on both machines, and the boundary fails with C1083. Q4's tick half met in steady play and missed on the ticks that order both 100-ship fleets: on 2 of 6 such ticks on the second machine, and on 7 of 8 on the development machine. The owner closed the task on 2026-09-30, and decided on 2026-10-01 to record Q4 with these ticks and fix them after the MVP. The second machine's scripted runs were not repeated by hand. The figures are in BalanceLog.md (2026-09-30 and 2026-10-01).
- **Source:** the MVP plan's task 2.7 (commit `0d90731`); the MVP design §3 (commit `6861fcc`).

### 2026-09-30 · The battle model reads the tuning data (MVP task 3.1)

- **Run:** `python Tools/BattleModel.py`, to give the same verdicts once it reads `Tuning.json`.
- **Build:** local.
- **Verdict:** Not recorded. The board says done (#31, merged 2026-09-30); the plan records neither the run nor its verdicts.
- **Source:** the MVP plan's task 3.1 and its board (commit `0d4c166`, #32).

### 2026-09-30 · The packages and the COM header, checked on both platforms (ADR-001, ADR-003, ADR-004 and ADR-005)

- **Run:** the checks the ADRs name for what the build stands on. `<winrt/base.h>` compiled under `/std:c++latest /permissive- /W4 /WX` in Debug and Release on both platforms; a test program and a temporary call in the executable caught `check_hresult(E_INVALIDARG)` as `0x80070057`; the executable's imports were read before anything called `check_hresult`; both installed SDKs carried C++/WinRT 2.0.250303.5. On the development machine no `msquic*.h` was under either installed SDK, 10.0.26100.0 and 10.0.28000.0, and `System32` had no `msquic.dll`. A temporary `MsQuicOpen2` call in a `NeuronCore` `.cpp` compiled clean under the project's settings, the executable imported `msquic.dll`, and the call succeeded run from the package layout. A temporary PIX CPU event made the Debug executable import `WinPixEventRuntime.dll` and left the Release one without it; `dumpbin /symbols` found `PIXEventsReplaceBlock` and `PIXGetThreadInfo` in the Debug object and neither in the Release one; `pix3.h` built for x64 in Debug and Release under the project's settings. `dumpbin /headers` read `8664` and `AA64` for each DLL the Debug|x64 and Debug|ARM64 builds copied.
- **Build:** Debug and Release, x64 and ARM64; the SDK and `System32` checks on the development machine.
- **Verdict:** Run, and each check passed; each temporary call and event was then removed. The executable's imports have not been read since the renderer calls `check_hresult` (Outstanding). The size of MsQuic's x64 DLL is in BalanceLog.md (2026-09-30).
- **Source:** ADR-001, ADR-003, ADR-004 and ADR-005, Context and Consequences, as they stood at `8366820` (commit `c0b4e76`, #26; ADR-001's imports, commit `4323f42`, #34).

### 2026-10-01 · The tuning data loads in the package (MVP task 2.2)

- **Run:** the owner's run of the executable loading `Assets\Tuning.json` from the package at startup, and reporting a failure in a message box.
- **Build:** the packaged app; the configuration is not stated.
- **Verdict:** Not recorded. The board says done (#32) and records no owner run; the MVP plan closed on 2026-10-01.
- **Source:** the MVP plan's task 2.2 and its board (commit `ec4d980`, #34).

### 2026-10-01 · Combat effects and the first HUD (MVP tasks 3.5 and 3.6)

- **Run:** the owner's runs of the combat effects, a muzzle flash, a tracer and a beam, a hit spark and an explosion (3.5); and the UI drawn over the scene, with the first HUD (3.6).
- **Build:** not stated.
- **Verdict:** Run by the owner. The effects read, and the text is sharp at the native resolution.
- **Source:** the MVP plan's tasks 3.5 and 3.6 and its board (commit `6861fcc`).

### 2026-10-01 · Q4 under stress (MVP task 3.7)

- **Run:** the owner's measurement of Q4 with `--measure --stress`: 200 ships and 40 structures in combat with the HUD, zoomed out to the 1,600 m view over the fight, summarized by `Tools/FrameTimes.py`.
- **Build:** Release|ARM64 on the development machine, at 1920×1080 and at 2880×1920.
- **Verdict:** Run. Frames met at 1920×1080. At 2880×1920 every frame missed a vsync. Ticks over 5 ms on 4 and 5 ticks a run, which the owner left for after the MVP. The Adreno's GPU timestamps miss work, so the frames presented are counted too. The figures are in BalanceLog.md (2026-10-01).
- **Source:** the MVP plan's task 3.7 and its board; the MVP design §3 (commit `6861fcc`).

### 2026-10-01 · The placement ghost, the build menu, the full HUD and the minimap (MVP tasks 4.2 and 4.5)

- **Run:** the owner's runs of the placement ghost and the build menu (4.2), and of the full HUD and the minimap (4.5).
- **Build:** not stated.
- **Verdict:** Run by the owner; both read.
- **Source:** the MVP plan's tasks 4.2 and 4.5 and its board (commit `6861fcc`).

### 2026-10-01 · The ship designer panel (MVP task 5.2)

- **Run:** the owner's run of the ship designer panel.
- **Build:** not stated.
- **Verdict:** Run by the owner, as the board records; no verdict beyond that.
- **Source:** the MVP plan's task 5.2 and its board (commit `6923496`, #40).

### 2026-10-01 · The Missile Rack (MVP task 5.3)

- **Run:** the battle model run locally with the Missile Rack in, and the owner's run of the splash ring.
- **Build:** local for the model; not stated for the owner's run.
- **Verdict:** Run. The model fails only (b), and the simulation fails all four criteria; both were recorded untuned. The owner's run found that the ring reads. The figures are in BalanceLog.md (2026-10-01).
- **Source:** the MVP plan's task 5.3 and its board (commits `6923496`, #40, and `6861fcc`).

### 2026-10-01 · The models as Blender shows them, and the exhaust (MVP tasks A.1 and A.2)

- **Run:** the owner's runs of every model drawn as Blender shows it, with the Tarkan Station and Medium changing visibly (A.1); and of the exhaust in its drive's color, growing with speed, and shots leaving from the guns, read from the RTS camera (A.2).
- **Build:** not stated.
- **Verdict:** Run by the owner later the same day (#41, merged 2026-10-01). No verdict beyond that is recorded.
- **Source:** the MVP plan's tasks A.1 and A.2 and its board (commits `c530880`, #41, and `6861fcc`).

### 2026-10-01 · Q4 with the glow pass (MVP task A.2)

- **Run:** task 3.7's `--measure --stress` again, with the glow pass in.
- **Build:** Release, on the development machine.
- **Verdict:** Not recorded. The plan said the owner's Q4 run measures the glow pass too, and ADR-019 recorded its cost as not yet measured. The owner's `--measure --stress` of `main` on 2026-10-11 is of a build that has the glows (BalanceLog.md, 2026-10-11).
- **Source:** the MVP plan's task A.2 (commit `c530880`, #41); ADR-019's Consequences (commit `cedc905`, #41).

### 2026-10-01 · The AI, and win, lose and the menu (MVP tasks 6.1 and 6.2)

- **Run:** the owner's runs of the AI player (6.1), and of winning, losing and the menu (6.2).
- **Build:** not stated.
- **Verdict:** Run by the owner. For 6.2: the menu, the Victory banner, and going back to the menu and on. For 6.1 no verdict is recorded.
- **Source:** the MVP plan's tasks 6.1 and 6.2 and its board (commit `fe50a7a`).

### 2026-10-01 · Q1 and Q3: the owner's first two matches (MVP task 6.3)

- **Run:** the owner's playtests for Q1, whether a full match against the AI lasts 15 to 25 minutes and is worth playing again, and Q3, whether research visibly changes what is built in the mid-game; summarized by `Tools/MatchLog.py`.
- **Build:** the MVP after milestone 6 (#42); the configuration is not stated.
- **Verdict:** Run. Q1 yes and Q3 yes (owner, 2026-10-01), from two matches: seed 17908775201362716, left at 24:51 with the AI at 443 warships to the player's 141; and seed 17908808925176743, won at 14:31 with 55 to the AI's 261. After the win the owner decided that the AI can be beaten as it is.
- **Source:** the MVP plan's task 6.3 and its board, and the MVP design §3 (commit `fe50a7a`, "Answer Q1 "yes" after the owner's win, and close the plan").

### 2026-10-01 · The full Q2 check in Release (MVP task B.1)

- **Run:** the owner's run of `Q2CheckTests.TheFullCheck` in Release, which CI never runs.
- **Build:** Release|ARM64 on the development machine, ten cores.
- **Verdict:** Run. The check of record passes all four criteria, in 1 min 51 s; its robustness sweep made 2,304 counter checks against the Linux container's 2,226 (BalanceLog.md, 2026-10-01).
- **Source:** the MVP plan's task B.1 and its board, and the MVP design §12 (commit `6861fcc`).

### 2026-10-02 · A destruction, from the RTS camera (MVP task C.1)

- **Run:** the owner's run of whether a destruction reads from the RTS camera: shards, a fireball, debris and smoke.
- **Build:** not stated.
- **Verdict:** Run. A full battle with every model lined showed three faults: the rocks' lines the brightest on the screen, the fireball's red, and every beam the Humans' pale blue. The next run showed three more: beige blasts, faint beams, and the health bars the most saturated marks. The owner asked for each to be fixed, and kept the rocks' lines (2026-10-02). C.1 merged as #46 on 2026-10-02, though the closed board still listed it as in review and not yet built or run.
- **Source:** the MVP plan's task C.1 (commit `9439b33`, #46); ADR-028's Context (commit `234922d`, #46).

### 2026-10-02 · Q4 with the explosions' shards (MVP task C.1)

- **Run:** `--measure --stress` with the explosions' shards.
- **Build:** Release, on the development machine.
- **Verdict:** Not recorded. ADR-026 called the shards' cost computed, not measured, and named the owner's `--measure --stress` run as the measurement. The owner's run of `main` on 2026-10-11 is of a build that has them (BalanceLog.md, 2026-10-11).
- **Source:** the MVP plan's task C.1 (commit `9439b33`, #46); ADR-026's Consequences (commit `1baea31`, #46).

### 2026-10-02 · The owner's first match, looked at again (ADR-023)

- **Run:** the owner's report on the first match of 2026-10-01 (MVP task 6.3), whose player researched all eight topics and never built the Large hull, the Fusion Drive or the Missile Rack.
- **Build:** the MVP after milestone 6 (#42); the configuration is not stated.
- **Verdict:** A fault found in play: the Large hull could be picked, but Queue stayed dim until the design was saved, and nothing on the panel said so, though research and the server had unlocked it. The panel stopped the player, which weakens what the first match says about Q2 and Q3. ADR-023 had Queue save the design (#48).
- **Source:** ADR-023's Context as it stood at `8366820` (commit `2f41b20`, #48).

### 2026-10-02 · The server's own thread under ThreadSanitizer (ADR-025)

- **Run:** `GameLogicTests`' server tests under ThreadSanitizer, and `InProcessServerTests.RunsItsTicksOnItsOwnThread` again on a build with the lock taken out of `LoopbackTransport::Send`.
- **Build:** the Linux container, g++ 13 with `-fsanitize=thread`.
- **Verdict:** Run. No report on the tree, and 27 races with the lock taken out, so the test can catch a missing lock. CI runs the test under MSVC without a sanitizer.
- **Source:** ADR-025's Consequences as they stood at `8366820` (commit `842bbfb`, #48).

### 2026-10-02 · Container runs of Phase 1 (to 2026-10-03)

- **Run:** the runs Phase 1's Verify lines name in the container: 7.2's balance check with arcs; 8.2's measurement and full balance check before and after; 9.7's and 12.2's AI-against-AI matches; 10.3's check of record; 13.1's `--ai-matches`; 13.2's AI-against-AI run; and 7.3's seeds 1 to 40 again, which moved to Phase 2's 14.4.
- **Build:** the Linux container, clang 18, against stand-ins.
- **Verdict:** Run, but for 7.3. 7.2: (a) to (c) pass at every stage, and (d) is unsure at tier 1; 8.2: the order ticks timed and counted, and the balance check passes before and after; 9.7, 12.2 and 13.1: the match lengths; 10.3: all four criteria at every stage. 7.3's run of seeds 1 to 40 is not recorded: Phase 2's 14.4 was verified by CI only, and its pull request (#55) records seeds 1 to 10 for 14.5. The figures are in BalanceLog.md (2026-10-03).
- **Source:** the Phase 1 plan's tasks 7.2, 7.3, 8.2, 9.7, 10.3, 12.2, 13.1 and 13.2 (commits `5a7d99d` and `8197633`); the Phase 2 plan's tasks 14.4 and 14.5 (commit `0d3a678`, #55).

### 2026-10-03 · The lines carry the shape (MVP task D.1)

- **Run:** the owner's run of dark faces under multisampled lines, with structures drawn behind the ships, zoomed in on a structure and zoomed out over a fight; and Q4 measured again with `--measure --stress` for the multisampling.
- **Build:** Release, on the development machine.
- **Verdict:** Not recorded. D.1 merged as #50 on 2026-10-03, though the closed board still listed it as in review and not yet built or run. ADR-040 asked for the multisampling to be measured again before it was taken as settled. The owner's `--measure --stress` of `main` on 2026-10-11, at 1920×1080 on a 165 Hz display, includes the multisampling (BalanceLog.md, 2026-10-11); no document ties it to D.1.
- **Source:** the MVP plan's task D.1 (commit `11c3802`, #50); ADR-040's Consequences (commit `85f5f58`, #54).

### 2026-10-03 · Replays unchanged by the order and path work (ADR-032 and ADR-054)

- **Run:** the container's checks that two changes left the simulation alone where they should. Before and after ADR-032's large orders over two ticks: a run of 2,400 ticks with every order of 32 ships or fewer, with the graphs built after setup or not, and the same load split into large orders with and without its decisions 2 and 4. Before and after ADR-054's obstacle grid and extended graphs: eight two-AI matches of 72,000 ticks each, hashing every snapshot and, every 6,000 ticks, the whole world; the measurement load and the stress scene; and four `--ai-matches` matches of 15 minutes.
- **Build:** the Linux container, each build made the same way before and after.
- **Verdict:** Run. Each ended in exactly the same state, bit for bit, as before; the matches gave the same snapshots and states, and the four matches wrote the same log.
- **Source:** ADR-032's Consequences (commit `9782b1c`, #49) and ADR-054's Consequences (commit `c381f32`, #53), as they stood at `8366820`.

### 2026-10-03 · P5: the designer, the floating windows and the banking (Phase 1 task 13.2)

- **Run:** the owner judging the designer against the mockup, the floating windows and the banking, in play. It is the only recorded run of what the owner runs of 7.1 (the bank from the RTS camera, the exhaust on the hull), 7.2 (ships turning in arcs), 9.1 (every panel reads, sharp text at 1920×1080 and 2880×1920), 9.2 (dragging, Esc and closing by hand) and 9.4 (research and production as windows) were to look at.
- **Build:** not stated.
- **Verdict:** Run. P5 yes (owner, 2026-10-03): the designer, the floating windows and the banking read in play. Each task's row says it was accepted as built (owner, 2026-10-03). The particular looks the tasks name, such as whether the arcs and their banking look natural, which ADR-039 left to the owner's run, are not recorded apart.
- **Source:** the Phase 1 plan's tasks 7.1, 7.2, 9.1, 9.2, 9.4 and 13.2, its board, and the Phase 1 design §2 (commit `8197633`).

### 2026-10-03 · The designer beside the mockup (Phase 1 task 9.3)

- **Run:** the owner's run of the designer window side by side with `Mockups/ShipDesigner.png`, to close gate H6 of the Phase 1 design.
- **Build:** not stated.
- **Verdict:** Run. Gate H6 decided on 2026-10-03: the designer's colors and sizes as built.
- **Source:** the Phase 1 plan's task 9.3 and its gates (commit `8197633`).

### 2026-10-03 · Two looks at the screen (Phase 1 tasks 9.5 and 9.6)

- **Run:** the owner's reviews of the screen after tasks 9.5 and 9.6, the only recorded looks at what their owner runs name: for 9.5, the HUD in the windows' look, faint rings and the rig on its legs; for 9.6, rings by zoom, white crosses and the HUD.
- **Build:** not stated.
- **Verdict:** Run. The review of a base's screen after 9.5 gave task 9.6: seven recommendations, and two faults the owner found playing (a rig could claim an asteroid never explored; `--stress` stood rigs on open ground). The review of a big fight after 9.6 gave task 9.7: columns, health bars that did not read, and "+0/s". Both tasks were accepted as built (owner, 2026-10-03).
- **Source:** the Phase 1 plan's tasks 9.5, 9.6 and 9.7 (commits `da9a81b`, #51, and `8197633`).

### 2026-10-03 · The rest of Phase 1's client tasks (tasks 9.7, 10.2, 11.1, 12.1 and 12.2)

- **Run:** the owner's runs of the health bars and the Alt key (9.7); the Pulse Drive's exhaust and the Flak Battery's and the Rail Cannon's shots, with the Pulse exhaust's color to be picked at the run (10.2); the ore-left panel and the minimap's dry asteroid (11.1); losing all production, and the revealed Shipyards (12.1); and the AI's longer match, against a human (12.2).
- **Build:** not stated.
- **Verdict:** Accepted as built, with no run recorded: each row says "done; accepted as built (owner, 2026-10-03)". These were the runs ADR-034, ADR-037, ADR-041 and ADR-047 named as where their looks and play would be judged; ADR-034 called the Pulse Drive's lime green provisional until the owner picked it at 10.2's run.
- **Source:** the Phase 1 plan's tasks 9.7, 10.2, 11.1, 12.1 and 12.2 and its board (commit `8197633`).

### 2026-10-03 · The 5 km map (Phase 1 task 11.2)

- **Run:** the owner's run confirming the 5 km map's layout, its first matches deciding gate H8 of the Phase 1 design, and a `--measure --load` run on the new map.
- **Build:** not stated; `--measure --load` in Release on the development machine.
- **Verdict:** Partly made. H8 was decided on 2026-10-03: G3's 1,600 m zoom limit stayed. The layout's confirmation is not recorded; ADR-036 called the layout unconfirmed until this run. The `--measure --load` run was not made (P4). Phase 4's 10 km map then replaced the 5 km one (task 28.2).
- **Source:** the Phase 1 plan's task 11.2 and its gates, and the Phase 1 design §2 (commit `8197633`); ADR-036's Consequences (commit `85f5f58`, #54).

### 2026-10-03 · Phase 1's runs on the development machine (tasks 8.1, 8.2, 10.3 and 13.2)

- **Run:** 8.1's and 8.2's `--measure --load` and Q5 again; 10.3's balance check of record with its time; and 13.2's Q4 measurement on the 5 km map at a 60-minute match's peak ship count (P4).
- **Build:** Release|ARM64 on the development machine.
- **Verdict:** Not made. Phase 1 closed on 2026-10-03 without them: P3's run in Release|ARM64 on the development machine was not made, and P4 was not answered, with neither the development machine's `--measure --load` nor the frame and tick times on the 5 km map at a 60-minute match's peak ship count measured. Phase 4's U7 asks the engine's question again, and is outstanding.
- **Source:** the Phase 1 plan's tasks 8.1, 8.2, 10.3 and 13.2, and the Phase 1 design §2 (commit `8197633`).

### 2026-10-03 · P1 and P2 for the owner (Phase 1 task 13.2)

- **Run:** the owner's matches against the AI without rushing it (P1), and the match log's check of what each side builds after a tier's gateway (P2).
- **Build:** not stated.
- **Verdict:** Not made. P1 was answered for two AIs only, a median of 47.5 minutes over seeds 1 to 40 and 51.1 over 41 to 80; the owner's matches were not measured. P2 was not answered: 2 of 80 AI matches reached tier 3, and the match log was not checked. Phase 4 later set no target for a match's length.
- **Source:** the Phase 1 plan's task 13.2 and the Phase 1 design §2 (commit `8197633`); the Phase 4 design §3 (commit `6311164`, #81).

### 2026-10-03 · Container runs of Phase 2 (to 2026-10-04)

- **Run:** the runs Phase 2's Verify lines name in the container: 14.5's AI-against-AI matches, 18.1's, and 19.2's AI-against-AI run tuned against S1 to S4.
- **Build:** the Linux container, against stand-ins for the Windows headers and the test framework.
- **Verdict:** Run. 14.5: seeds 1 to 10 all end. 18.1: the AI's matches shorter with territory's play. 19.2: S1 to S3 met by two AIs, and S4 met over seeds 1 to 40 and 10 seconds short over 1 to 10, after the fall-back was tuned to 10% and 240 s. The figures are in BalanceLog.md (2026-10-03 and 2026-10-04).
- **Source:** the Phase 2 plan's tasks 14.5, 18.1 and 19.2 (commits `0d3a678`, #55, and `8c6d2c9`, #61); the Phase 2 design §2 (commit `32793ac`, #67).

### 2026-10-04 · Phase 2 in play, and S5 (Phase 2 tasks 14.3 to 19.2)

- **Run:** the owner's runs of Phase 2's client tasks: territory on the screen, with the Relay's build button and ghost, the Relay's model, the minimap tinted by holder and hatched where suppressed, and the nodes line (14.3); the tickets, and the banner naming a domination (15.2); the designer's module row (16.2); alerts, and Space to the newest (17.1); holding a sector on H and patrolling on T (17.2); and S5, the owner judging the alerts and the standing orders in play (19.2).
- **Build:** not stated.
- **Verdict:** Run. "Phase 2 works fine" (owner, 2026-10-04), and S5 yes. The run met gate K7 of the Phase 3 design, and the owner then accepted Phase 3. It is the owner's run that ADR-056, ADR-057 and ADR-058 left their panels, the minimap, the banner and the module row to.
- **Source:** the Phase 2 plan's tasks 14.3, 15.2, 16.2, 17.1, 17.2 and 19.2 and its board, and the Phase 2 design §2 (commit `32793ac`, #67, "Accept Phase 3 after the owner's run of Phase 2").

### 2026-10-04 · S1, S2 and S4 for the owner (Phase 2 task 19.2)

- **Run:** the owner's match measured with the match log, for S1 (the first shot by minute 5), S2 (five engagements in three sectors before minute 20) and S4 (45 to 60 minutes).
- **Build:** not stated.
- **Verdict:** Not made. That match was not measured with the match log, so S1, S2 and S4 rest on the two AIs' figures.
- **Source:** the Phase 2 plan's task 19.2 and the Phase 2 design §2 (commit `32793ac`, #67).

### 2026-10-04 · Phase 3's client tasks (tasks 20.2, 21.2, 22.3 and 23.2)

- **Run:** the owner's runs of the levels and the Upgrade button (20.2); what a Shipyard cannot build (21.2); the research window by the Lab's level (22.3); and the node cap, with the station's guns spread over its hardpoints (23.2).
- **Build:** not stated.
- **Verdict:** Run, and accepted (owner, 2026-10-04): "20.2, 21.2, 22.3 and 23.2 were in review for the owner's run, which accepted them on 2026-10-04."
- **Source:** the Phase 3 plan's board (commit `27ba3d2`, "Close Phase 3's client tasks after the owner's run").

### 2026-10-04 · T5: the levels read (Phase 3 task 25.2)

- **Run:** the owner judging in play whether a structure's level is visible at a glance, and whether what the next level gives is clear before buying it.
- **Build:** not stated.
- **Verdict:** Run. Yes: a structure's level reads at a glance, and what the next level gives is clear before it is bought.
- **Source:** the Phase 3 plan's task 25.2 and its board, and the Phase 3 design §2 (commit `0c0b03c`, #74).

### 2026-10-04 · Difficulties on the menu (Phase 3 task 26.1)

- **Run:** the owner's run of Easy, Normal and Hard on the menu: is Easy beatable?
- **Build:** not stated.
- **Verdict:** Run. Easy is beatable (owner, 2026-10-04). What the difficulties do against a human beyond that is not measured.
- **Source:** the Phase 3 plan's task 26.1 and its status line (commit `0c0b03c`, #74, "Close Phase 3: T5 reads, Easy is beatable, and every task is done").

### 2026-10-04 · Container runs of Phase 3

- **Run:** the AI-against-AI matches Phase 3's Verify lines name, for 21.1, 22.1, 23.1, 24.1, 25.2 (seeds 1 to 40) and 26.1 (each difficulty against Normal).
- **Build:** the Linux container's harness, against stand-ins.
- **Verdict:** Run. T1 and T4 met; T2 missed at 9:28 and T3 missed on the Lab, in 11 of 40, both kept as misses (owner, 2026-10-04); Normal beats Easy and Hard beats Normal in all 20 of seeds 1 to 20. The figures are in BalanceLog.md (2026-10-04).
- **Source:** the Phase 3 plan's tasks 21.1 to 26.1 (commit `0c0b03c`, #74); the Phase 3 design §2 (commit `363f137`, #73).

### 2026-10-04 · The QUIC transport's tests on Linux (ADR-060)

- **Run:** `WireFormatTests`, `QuicTransportTests` and `InProcessServerTests` three times in a row; `QuicTransportTests` and `InProcessServerTests` once more under ThreadSanitizer; and the certificate test with the client's pinning disabled.
- **Build:** the Linux container, against MsQuic 2.6.1's OpenSSL build, with an OpenSSL certificate standing in for the CNG one.
- **Verdict:** Run. No failure in the three runs. The one ThreadSanitizer report lay between two of MsQuic's own threads, inside the library. With the pinning disabled the certificate test fails, as it should. The Schannel path, the CNG certificate and the MSVC build are checked by CI's Debug|x64 run alone.
- **Source:** ADR-060's Consequences as they stood at `8366820` (commit `1ec4476`, #62).

### 2026-10-04 · The self-play switch and search in the container (self-play tasks SP1.1 and SP1.2)

- **Run:** `--ai-matches` given every kind of malformed option, and `Tools/SelfPlay.py`'s search through a build against stand-ins: 4 generations straight through, and stopped after 2 and resumed to 4; a search interrupted in its first generation; and a game that failed.
- **Build:** the Linux container, against stand-ins for the Windows headers.
- **Verdict:** Run. Every malformed option failed with its own message: unknown, given twice, missing a value, a number out of range or not a number, a missing or invalid settings file, a log that cannot be written; under `--quiet` the message went to standard error. The search of 4 generations played 66 matches, promoted 4 champions with the thresholds set to promote every time, and wrote its report; stopped after 2 and resumed, it gave the same history and the same champions. Interrupted in its first generation, it stopped once the matches under way had finished, and `--resume` completed it. A game that failed stopped the search with the game's own message. Not checked: the MSVC build, and on Windows `CommandLineToArgvW`, standard error, the exit code and the script's default path to the game (Outstanding, SP1.3).
- **Source:** ADR-063's Consequences as they stood at `8366820` (commit `c2ecf4f`, #64).

### 2026-10-04 · Self-play's search, and the AI at each `secondSlotTier` (SP1.2 and SP1.4, to 2026-10-05)

- **Run:** `Tools/SelfPlay.py --self-test`, and a short search in the container, stopped and resumed; SP1.4 also plays the AI at each `secondSlotTier`.
- **Build:** the Linux container; the AI built with clang 18 against stand-ins.
- **Verdict:** Run. SP1.4: a search of 6 generations against a stand-in played its matches and wrote its report; stopped after 2 and resumed to 4, it gave the same history and report as a run straight to 4; a state saved by version 1 was refused. The AI over seeds 1 to 4 logged the same bytes at `secondSlotTier` 1, 2 and 3 (BalanceLog.md, 2026-10-05). SP1.2's run is not described beyond "done, #64".
- **Source:** the self-play plan's tasks SP1.2 and SP1.4 and its board (commits `6346981`, #64, and `8fac442`, #80).

### 2026-10-05 · The sky's crosses (Interface plan task 16.6)

- **Run:** none: gate K5 of the Interface plan left the choice among ADR-028's crosses, shorter crosses or dots to the owner's run.
- **Build:** none.
- **Verdict:** Decided without a run. The owner chose on 2026-10-05 to keep ADR-028's crosses; nothing changed, ADR-068 records it, and no run is needed.
- **Source:** the Interface plan's task 16.6, its board and gate K5 (commit `2754910`, #78).

### 2026-10-06 · Container runs of Phase 4 (to 2026-10-08)

- **Run:** the runs Phase 4's Verify lines name in the container: `BalanceCheckTests` at tripled budgets (27.1); AI-against-AI matches for 27.4, 27.5, 27.6, 28.0, 28.4, 29.1, 30.1, 31.1, 32.1 and 33.1 over seeds 1 to 40; four AI matches logged through `MatchLog` (34.1); and U1 to U5 in the harness, with U6's full balance check (34.2).
- **Build:** the Phase 4 harness in the Linux container: the server and the AI built with clang 18 against stand-ins.
- **Verdict:** Run. U1 met; U2 met at its peak and missed at minute 20; U3 met in number and short in share (accepted, owner, 2026-10-07); U4 met; U5 every match ends, by domination only (recorded, not tuned); U6 met. In the container the median match's 99th percentile tick is 1.4 ms and the worst 3.3 ms. The figures are in BalanceLog.md (2026-10-07 and 2026-10-08).
- **Source:** the Phase 4 plan's tasks 27.1 to 34.2 (commits `a17151f`, #81, to `cec3c94`, #91); the Phase 4 design §2 (commit `b19efdf`, #81, and later).

### 2026-10-07 · The 10 km map in a Debug build (Phase 4 task 28.1)

- **Run:** the owner's play of a Debug build on the 10 km map, after task 28.1.
- **Build:** Debug.
- **Verdict:** Run. The ships stuttered and the controls lagged; the owner chose to make the fog's update incremental, which became task 29.2. Task 28.1's own owner run, at the widest view over the 10 km map, is not recorded apart.
- **Source:** the Phase 4 plan's tasks 28.1 and 29.2 (commit `465eb84`, #82).

### 2026-10-07 · A pirate camp, seen and fought (Phase 4 task 30.2)

- **Run:** the owner's run of a pirate camp seen and fought, on the screen and on the minimap.
- **Build:** not stated.
- **Verdict:** Run, and accepted (owner, 2026-10-07). The owner kept the camp's numbers: the design's first fleet of 4 to 6 Small ships is one of Lance ships.
- **Source:** the Phase 4 plan's task 30.2 and its board (commit `3471534`, #88).

### 2026-10-07 · A derelict, salvaged (Phase 4 task 31.2)

- **Run:** the owner's run of a derelict found, hovered and salvaged, and a camp's wreck salvaged before its Relay.
- **Build:** not stated.
- **Verdict:** Run, and accepted (owner, 2026-10-07).
- **Source:** the Phase 4 plan's task 31.2 and its board (commit `3471534`, #88).

### 2026-10-07 · The Repair Bay and retreat (Phase 4 task 32.2)

- **Run:** the owner's run of a Repair Bay built at the front, a ship hit below a quarter going to it and coming back whole, the selection's retreat stepped, and a design saved with a retreat.
- **Build:** not stated.
- **Verdict:** Run, and accepted (owner, 2026-10-07).
- **Source:** the Phase 4 plan's task 32.2 and its board (commit `3471534`, #88).

### 2026-10-08 · The fleet cap on the screen, and the incremental fog (Phase 4 tasks 27.3 and 29.2)

- **Run:** the owner's runs of the fleet against its cap on the Shipyards' line, with a Shipyard waiting for the cap (27.3); and, in Debug on the 10 km map, the incremental fog (29.2).
- **Build:** not stated for 27.3; Debug for 29.2.
- **Verdict:** Not recorded. Both rows say done with no run, and Phase 4 closed on 2026-10-08; 27.3's As built calls the owner's run its first look.
- **Source:** the Phase 4 plan's tasks 27.3 and 29.2 and its board (commits `9214426`, #85, and `cec3c94`, #91).

### 2026-10-08 · Container runs of Phase 5 (to 2026-10-09)

- **Run:** the runs the Phase 5 plan's Verify lines and As built name in the container: 35.1's worlds saved and loaded; 35.2's W2 save; 36.1's `OutpostServer --new-world` and its refusals; 37.2's and 37.3's W2 and W3 with hosted players; 39.2's `--world-run` of a day, killed at random; 39.3's matchup baseline over 40 seeds; and 39.4's W2 and W3 again.
- **Build:** the Linux container: the server built by clang 18 at `-O2` against stand-ins for the Windows headers and MsQuic.
- **Verdict:** Run. 35.1: worlds saved at minutes 6, 12 and 18 and run on equal the running world at minute 24, to the bit. 39.2: a day of world killed 10 times ended equal to the world run straight through. The W2 and W3 figures, the day of world and the matchups' baseline are in BalanceLog.md (2026-10-08).
- **Source:** the Phase 5 plan's tasks 35.1 to 39.4 and milestone 39's As built (commits `e0b0108`, #89, `e68f305`, #92, `70f480c`, #95, and `a234270`, #98).

### 2026-10-09 · The node marks, a first look (second interface plan task UI1.1)

- **Run:** the owner's first look at the node marks.
- **Build:** not stated.
- **Verdict:** A look, not the owner run. A mark on every node made the screen busy, and gate V2 was revised on 2026-10-09: the nodes the player could claim are marked only while it places a Relay, in the world and on the minimap (#100). UI1.1's and UI1.4's owner runs are outstanding.
- **Source:** the second interface plan's gate V2 and the As built of UI1.1 and UI1.4 (commit `4b486d4`, #100).

### 2026-10-09 · Ore's glyph, picked (second interface plan task UI2.3)

- **Run:** the owner's pick of Ore's glyph from three drawn at 8, 13 and 20 pixels.
- **Build:** drawings, not the game.
- **Verdict:** A choice, not a run: a cut gem, over a hexagon and a tall crystal. The glyph's run in the game and the owner run are outstanding.
- **Source:** the second interface plan's task UI2.3 (commit `92cfc97`, #99, and later).

### 2026-10-11 · A warm key, a cool fill, and the glow (Look plan tasks LV1.1 and LV1.2)

- **Run:** the owner's runs of a warm key, a cool fill and lines that keep their hue, at the default zoom and close in (LV1.1); and of the phosphor glow (LV1.2).
- **Build:** Release.
- **Verdict:** Run, and accepted, zoomed in and zoomed out over a fight: "The owner merged #107 and accepted the light and the glow on 2026-10-11, zoomed in and zoomed out over a fight." Whether the glow's clipping toward white shows, which the plan left to LV1.2's run since there is no tone map, is not recorded.
- **Source:** the Look plan's tasks LV1.1 and LV1.2 and its board (commit `4462303`, #108); ADR-090 and ADR-091 (commit `07cd7a9`, #107).

### 2026-10-11 · The light and the glow on the GPU (Look plan task LV1.2)

- **Run:** the owner's `--measure --stress` of this change against `main`, summarized by `Tools/FrameTimes.py`.
- **Build:** Release, at 1920×1080 on a 165 Hz display.
- **Verdict:** Run. The glow and the light together cost 0.30 ms of GPU work a frame on average, 5% of a 165 Hz frame, not measured apart. Q4 is met. The figures are in BalanceLog.md (2026-10-11).
- **Source:** the Look plan's task LV1.2 (commit `602f1b6`, #107); ADR-091's Consequences (commit `07cd7a9`, #107).

### 2026-10-11 · Container runs of the Look plan

- **Run:** the runs the Look plan names in the container: LV1.1's `SceneShadingTests`; LV1.2's `BloomTests`; and LV2.1's bake of every source, timed, the baker's self-test, `MeshDataTests` and `SceneShadingTests`, and an offline render of seven models.
- **Build:** the Linux container: gcc and clang against stand-ins; CPython 3.13 on 4 cores.
- **Verdict:** Run. LV1.1: the least light on a face the camera sees is 0.315 in luminance, and a face looking up is within 7% of gray. LV2.1: the bake's times are in BalanceLog.md (2026-10-11); the offline render showed recesses darkening softly, and the Tarkan Shipyard's frame near black at `OCCLUSION_STRENGTH` 1.
- **Source:** the Look plan's tasks LV1.1, LV1.2 and LV2.1 (commits `fdcc5f5`, #107, and `7aa6e4d`, #108); ADR-092's Consequences (commit `7aa6e4d`, #108).

### 2026-10-11 · A failed assertion, in the container (ADR-093)

- **Run:** an assertion's failing path, `Fatal` and `FailFast`, with stand-ins for the four Win32 calls.
- **Build:** the Linux container, gcc 13 and clang 18, in both configurations.
- **Verdict:** Run. It wrote `Main.cpp(19): name != nullptr && "a name"` and exited with the fast-fail's code, in both configurations. It has not been run on Windows (Outstanding).
- **Source:** ADR-093's Consequences as they stood at `8366820` (commit `d647385`, #108).

### 2026-10-25 · The self-play search's self-test on `main`

- **Run:** `python3 Tools/SelfPlay.py --self-test`, at the start of the run that wrote this log.
- **Build:** `main` at commit `8366820`, in the Linux container.
- **Verdict:** Fails: "Opponent.json's numbers and the knobs differ: ['attackCapShare', 'pirateMargin', 'scoutOwnHalfSeconds']". Phase 4's AI added the three numbers to `Opponent.json` without knobs in `Tools/SelfPlay.py`. Task SP1.5 of the self-play plan takes it up, and SP1.3 waits for it.
- **Source:** the self-play plan's task SP1.5.

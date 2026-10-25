# Outpost Commander — Balance log

Every measurement of the game's balance, its performance and its screen, dated. Entries are appended, each section in date order, and an entry is changed afterwards only to correct a factual error. [Design/Budgets.md](../../Design/Budgets.md) keeps each budget's target and its latest measurement; this log keeps every measurement, old and new.

**An entry** is one run: the date it was measured, what was measured, the figures, and under them the build (the code's state, where it ran and in what configuration), the seeds, the method and the source. A source is the commit that wrote the figure down, with the pull request that merged it where there was one; `git show <commit>` gives the document as it stood. Where two documents give one measurement differently, the entry gives each as stated, with its source. Figures worked out by arithmetic from the tuning data or the map, and estimates, are not measurements and are not here.

**Where the runs were made:**

- **The development machine** is the owner's laptop (ADR-003): a Qualcomm Snapdragon X X1P64100 with 10 cores at 3.4 GHz, 16 GB of memory, its integrated Adreno X1-85 GPU and its built-in 2880×1920 panel at 60 Hz, on Windows 11 Home build 26200.
- **The second machine** is a laptop with an Intel Core i7-12700H, 16 GB of memory and an NVIDIA GeForce RTX 3070 Ti Laptop GPU, whose 1920×1080 panel at 165 Hz is driven by the integrated Intel Iris Xe, on Windows 11 Pro build 26200. Its runs were driven by a script.
- **The Linux container** is an agent's cloud container, with no Windows, no MSVC and no GPU. The server, the AI and the tests are built there against stand-ins for the Windows headers, the test framework and MsQuic, with the compiler an entry names. Floats replay only on the same build (ADR-009), so MSVC's build may play the same seeds differently.
- **The container's harness** builds the server, the AI and ADR-038's match log in the Linux container and plays AI-against-AI matches on them, as `--ai-matches` does; where an entry does not name the compiler, the documents did not.
- **The Phase 4 harness** is the container's own driver of Phase 4: the server and the AI built with clang 18, two Normal AIs played on the in-process server as `--ai-matches` does, and each side's warships counted from its snapshots. Its matches run to three hours unless an entry says otherwise.
- **CI** is the repository's Windows job, which builds Debug|x64 with MSVC and runs the suites.

**Labels.** Q1 to Q5, P1 to P5, S1 to S5, T1 to T5 and U1 to U8 are the questions the closed phases asked, which [PostMortems.md](PostMortems.md) states; W1 to W6 are Phase 5's, in [OutpostCommander-Phase5.md](../OutpostCommander-Phase5.md). The Q2 check became the balance check in Phase 1. Tasks, milestones and gates are those of the plan the entry names.

## Balance

Win rates, match lengths and endings, fleets, banks, idle shares, matchups, and how ships move.

### 2026-09-30 · A review of the battle model's first Q2 check

| Quantity | Value |
|---|---|
| Model review of the first Q2 check | the model's win rates were measuring where each budget cut a design's ship count, and 60 unstratified battles could not tell 53% from 48% |

- **Build:** `Tools/BattleModel.py` at the second tuning pass of 2026-09-30
- **Seeds:** not stated
- **Method:** A review of the model's win rates. The model was then changed to spread its budgets evenly, field the leftover Ore as a fractional ship and take its verdicts on 95% confidence intervals.
- **Source:** the MVP design, §12 (commit `fbd7448`, #31).

### 2026-09-30 · The tuning pass: what the model reports when one number goes back

| Quantity | Value |
|---|---|
| Small hull hit points at 200 (tuned to 220) | Medium+Ion+Lance has no counter that wins four battles in five at 2,000-3,000 Ore (25-48%) |
| Small hull cost at 35 and at 45 (tuned to 32) | at 35, one-sided Lance Focusing leaves the upgraded Medium+Ion+Lance without an answer that wins half its battles (25-32%); at 45, Medium+Ion+Lance has no counter at 2,000-3,000 Ore under spread fire (25-27%) |
| Medium hull armor at 6 (tuned to 8) | at 6, Medium+Ion+Mass Driver was the only starting design worth building; going back to 6 leaves the swarm and the brawler without a counter in the starting stage (0-68%) |
| Medium hull cost at 120 (tuned to 110) | one-sided Hull Plating or Mass Driver Calibration leaves the upgraded Small+Ion+Mass Driver without an answer (0-8% under focus fire) |
| Mass Driver damage at 13 (tuned to 14) | one-sided Lance Focusing leaves the upgraded Medium+Ion+Lance without an answer (17-18%) |
| Mass Driver cost at 30 (tuned to 35) | one-sided Mass Driver Calibration leaves the upgraded Medium+Ion+Mass Driver without an answer (2-15% under focus fire) |
| Lance damage at 90 (tuned to 95) | one-sided Hull Plating leaves the upgraded Medium+Ion+Mass Driver without an answer (12-42% under focus fire) |
| Lance cost at 90 (tuned to 85) | one-sided Mass Driver Calibration leaves the upgraded Medium+Ion+Mass Driver without an answer (8-41% under focus fire) |

- **Build:** `Tools/BattleModel.py` at the second tuning pass of 2026-09-30; where it ran is not stated
- **Seeds:** not stated
- **Method:** The Q2 check with every other number as tuned and the one named number put back: the MVP design §12's "Tuned on" table, which ADR-008 names.
- **Source:** the MVP design, §12 (commit `c429317`, #22).

### 2026-09-30 · The model's picture of the counters

| Quantity | Value |
|---|---|
| With the starting components, at 2,000-4,500 Ore under both targetings | in every battle the model runs at 2,000-4,500 Ore under both targetings: the swarm (S+I+MD) beats the line (M+I+La), the line beats the brawler (M+I+MD), the brawler beats the swarm, every time; all three are worth building, each at about a third of the equilibrium mix; Small+Ion+Lance is not worth building; Small+Ion+Mass Driver beats Small+Ion+Lance in every modelled battle |
| With every component | Large+Fusion+Lance beats the brawler every time, and the swarm under spread fire but not focus fire; Large+Fusion+Mass Driver beats the swarm every time; both fall to the Small+Ion+Lance picket every time; the picket falls to the swarm; at every budget of the full set the designs worth building are the three starting designs, the picket and one heavy (L+F+Lance under spread fire, L+F+MD under focus fire), each between 11% and 26% of the mix |

- **Build:** `Tools/BattleModel.py` at the numbers of the time; where it ran is not stated
- **Seeds:** not stated
- **Method:** The model's Q2 check.
- **Source:** the MVP design, §7 (commits `ba769b1` and `c429317`, #22).
- **Note:** The date is that of the commit that wrote the figures down; the run's own date is not stated.

### 2026-10-01 · The Q2 check against the simulation, at milestone 3

| Quantity | Value |
|---|---|
| Q2 check against the simulation at milestone 3 | failed against the simulation where it passed in the model |

- **Build:** The MVP at milestone 3 (#36), in the Linux container
- **Seeds:** not stated
- **Method:** `GameLogicTests`' Q2 check: headless battles, whole ships at the center of each slice of the ±15% window, Ore left over not fielded, the second side turned half a turn; `Simulation::SetTargetRule` forces spread or focus fire.
- **Source:** the MVP plan, task 3.4 (commit `3e915e3`, #43).

### 2026-10-01 · The Q2 check with the Missile Rack in

| Quantity | Value |
|---|---|
| Q2 with the Missile Rack in (task 5.3), before B.1 | the model fails only (b): the Missile Rack is never worth building under spread fire; the simulation fails all four criteria; the Missile Rack, taken by one side with Mass Driver Calibration, leaves the other side's starting designs without a single win at 3,000-4,500 Ore |

- **Build:** The MVP at milestone 5 (#40): the simulation, and `Tools/BattleModel.py`; where each ran is not stated
- **Seeds:** not stated
- **Method:** Both checks field all eighteen designs; the robustness sweep moves the splash radius too; the model's clumps stand in a grid three footprint radii apart.
- **Source:** the MVP plan, task 5.3 (commit `6923496`, #40).

### 2026-10-01 · Retuning against the simulation (task B.1)

| Quantity | Value |
|---|---|
| B.1: candidates tried in retuning §12 against the simulation | about twenty candidates, each run through the simulation's check |
| B.1: whether any candidate made a heavy hull worth building at 2,000 Ore and a Medium one at 12,000 Ore at once | no candidate did, whatever moved; so (b) became a judgement over each stage's budgets |
| Mass Driver Calibration back at +15% fire rate (tuned to +10%) | the simulation's check (d) leaves one side's upgraded brawler and swarm without a starting answer that wins half their battles under spread fire (10-44%) |
| The Lance's fire interval back at 3.0 s (tuned to 2.7 s) | the line beats the brawler in only 53% of battles at 2,000 Ore under spread fire, and eight single 5% changes flip it; a Lance hit of 100 instead fails (c) everywhere |
| The Missile Rack's damage at 40 and at 25 (tuned to 30) | at 40, taken by one side, no starting design wins a single battle against it at 3,000-4,500 Ore; at 25, no design worth building uses it at 2,000-3,000 Ore; ships under fire stand close enough for a 30 m splash to reach eight Small or four Medium neighbours |
| The Missile Rack's cost back at 110 with 30 damage (tuned to 130) | one side's upgraded Medium+Ion+Missile Rack still has no starting answer that wins more than 27% at 4,500 Ore under focus fire |

- **Build:** The MVP's task B.1 candidates; the simulation's Q2 check in the Linux container
- **Seeds:** not stated
- **Method:** The simulation's Q2 check over each candidate; the last four rows are what it reports when that one number goes back, with everything else as tuned.
- **Source:** the MVP design, §12 (commit `f28eb1d`, #43).
- **Note:** The MVP design calls "the last four rows" of its table the pass of 2026-10-01, but the four numbers B.1 moved are Mass Driver Calibration, the Lance's interval and the Missile Rack's damage and cost, and Mass Driver Calibration's row sits in the middle of that table.

### 2026-10-01 · The Q2 check of the simulation after B.1

| Quantity | Value |
|---|---|
| Criterion (a) | passes: every design's best counter wins at least 80% of its battles at every budget, in both stages and modes, nearly all of them 100% |
| Criterion (b) | passes over each stage's budgets, in both modes: the starting stage plays its triangle at every budget; with every component, small and medium hulls with the Mass Driver and the Lance carry the 2,000 Ore fights; the heavies join from 2,000 Ore under focus fire and 3,000 under spread fire, and the Missile Rack from 3,000; at 12,000 Ore the mix is Small hulls and heavies; reported, not failed: the Large hull, the Fusion drive and the Missile Rack at 2,000 Ore, the Medium hull at 12,000, and the Mass Driver under focus fire from 4,500 Ore up and under spread fire at 6,000-9,000 |
| Criterion (c) | passes: none of 2,226 counters flips when any single number moves 5% |
| Criterion (d) | passes: the weakest answer to a one-sided topic is the brawler against the upgraded swarm, 55% with Hull Plating and 59% with Mass Driver Calibration, alone or with the Missile Rack; Lance Focusing leaves an answer at 87% |

- **Build:** The MVP after task B.1 (#43), in the Linux container
- **Seeds:** not stated
- **Method:** `Q2CheckTests.TheFullCheck`: every component at 2,000-12,000 Ore and the starting components at 2,000-4,500; spread fire picks a random enemy in range, focus fire the weakest; a threshold counts as met only when the 95% confidence interval of the win rate clears it, and (b) takes the designs the equilibrium mix of the win-rate matrix gives 5% or more.
- **Source:** the MVP design, §12 (commit `f28eb1d`, #43).

### 2026-10-01 · The Q2 check of record, on the development machine

| Quantity | Value |
|---|---|
| The check of record on the development machine | passes all four criteria with the same verdicts, the 55%, 59% and 87% of (d) included; its robustness sweep made 2,304 counter checks where the Linux container's counted 2,226, so the two builds do not play every battle the same |

- **Build:** The MVP after task B.1 (#43); Release|ARM64 on the development machine
- **Seeds:** not stated
- **Method:** `Q2CheckTests.TheFullCheck` with `OUTPOST_Q2_FULL` set.
- **Source:** the MVP design, §12 (commit `6861fcc`).

### 2026-10-01 · The model's Q2 check after B.1

| Quantity | Value |
|---|---|
| The model's Q2 check after B.1 (python Tools/BattleModel.py) | passes (a) and fails the rest: (b) no design worth building uses the Missile Rack at any budget; (c) the swarm stops beating the line at every budget under spread fire when Mass Driver damage drops 5%; (d) one side's Lance Focusing leaves its line without an answer that wins more than 13-25% |

- **Build:** `Tools/BattleModel.py` after task B.1; where it ran is not stated
- **Seeds:** not stated
- **Method:** The model: clumps at formation spacing, every ship in range of every enemy, fractional ships for the leftover Ore.
- **Source:** the MVP design, §12 (commit `f28eb1d`, #43).

### 2026-10-01 · The base's hand checks

| Quantity | Value |
|---|---|
| A lone Defence Platform against five Small+Ion+Mass Driver ships | met: the raid is destroyed in 41.7 s; the platform keeps 420 of 1,500 hit points (28%) |
| The armed Command Station against seven Small+Ion+Mass Driver ships | met: its gun destroys all seven in 57.6 s, keeping 2,952 of 5,000 hit points; 2.4 s from failing the one-minute intent |
| A Command Station without its gun or armor against seven | it falls in 24.9 s, approach included (intent: about 20 s; §6 says about 25 s, the seven ships about 610 Ore, ready around 1:30) |

- **Build:** The MVP at milestone 4 (#38), in the Linux container; the simulation is deterministic, so the figures are the game's
- **Seeds:** not stated
- **Method:** `HandCheckTests`: the raid gathers 400 m off, out of the gun's reach, and attacks; the run ends when one side is gone.
- **Source:** the MVP design, §12 (commit `393636c`, #38).
- **Note:** The owner left these margins for the owner's playtests to judge (2026-10-01); the playtest log carries that run, and gate R10 of ImplementationPlan-Reconcile.md asks about it.

### 2026-10-01 · The AI against a player who does nothing

| Quantity | Value |
|---|---|
| The AI's economy in AiPlayerTests (task 6.1) | with a second Shipyard at 30 Ore/s, the AI earned three times what its two Shipyards spent; with a Shipyard for each 10 Ore/s it has 4 by 2:15, and wins against a passive player at 5:24 |
| When it wins, and how its base goes up | It beats a player who does nothing at 5:24 (tick 6,483, seed 3). It places its base plan by 1:45 and its fourth Shipyard by 2:15, has all four built by 3:15, and sends its first attack group by 3:30. |
| What its Shipyards spend | From about 2:15 its income is 48.75 Ore/s (3 home rigs, 3 contested rigs and Improved Extraction), so it has 4 Shipyards. Building brawlers they spend about 33 Ore/s, and it holds about 3,300 Ore when the passive player's station falls, still gaining about 12 Ore/s. |

- **Build:** The MVP at milestone 6 (#42): `GameLogicTests` on Linux, clang at `-O2`, with the repository's data and the real server
- **Seeds:** seed 3 for the win; otherwise not stated
- **Method:** `AiPlayerTests` on the real server, against a player who does nothing.
- **Source:** the MVP plan, task 6.1 (commit `486c796`, #42); ADR-020, Consequences (commit `3cfc0fb`, #42).

### 2026-10-01 · Two AIs on the real server

| Quantity | Value |
|---|---|
| Match length of two AIs on the real server (task 6.3's first look) | a match ends in 8 to 15 minutes with §12 as retuned; the designs they build shift with each review and with the heavies from 6:35 |
| When the heavies come | Large Hull lands at 6:35. In 4 of the 5 matches one side or both built Large+Fusion+Lance or Large+Fusion+Mass Driver from then on, and what each side built changed from one 5-minute window to the next. |
| Match lengths | 12:09, 11:49, 11:24, 14:54 and 7:51; player 1 won 2 of the 5 |

- **Build:** The MVP after milestone 6 and task B.1's retune: `GameLogicTests` on Linux, clang at `-O2`, with the repository's data and the real server
- **Seeds:** 1 to 5
- **Method:** AI against AI on the real server, read from the match log.
- **Source:** the MVP plan, task 6.3 (commit `f28eb1d`, #43); ADR-020, Consequences (commits `486c796` (#42) and `f28eb1d` (#43)).

### 2026-10-01 · Q1 and Q3: the owner's first two matches against the AI

| Quantity | Value |
|---|---|
| The first match: length and warships built | the owner left at 24:51, before it ended, finding the AI too tough; the AI had built 443 warships to the player's 141, and 26 to 1 in the first five minutes |
| The second match | the owner won at 14:31, with 55 warships to the AI's 261 (just under Q1's 15-25 minutes) |
| Research in the first match | the AI moved to Large+Fusion designs after its Large Hull at 6:42, and its first heavies came between 10:00 and 15:00; the player had researched all eight topics by 13:04 and built Small and Medium hulls with the Mass Driver and the Lance, never the Large hull, the Fusion Drive or the Missile Rack |

- **Build:** The MVP after milestone 6 (#42), on the owner's machine; the configuration is not stated
- **Seeds:** 17908775201362716 (the first match) and 17908808925176743 (the second)
- **Method:** The owner's play, summarized by `Tools/MatchLog.py` from the match log.
- **Source:** the MVP design, §3 (commit `fe50a7a`).

### 2026-10-02 · The Q2 check under fog of war

| Quantity | Value |
|---|---|
| The Q2 check under fog of war | the full check gives a report byte-identical to the one without fog: all four criteria pass with the same figures, 2,226 counter checks included |

- **Build:** The MVP with fog of war (ADR-024), in the Linux container
- **Seeds:** not stated
- **Method:** The full check run with and without its fog option; `Q2CheckTests.FogChangesNoBattle` keeps a sample in CI.
- **Source:** the MVP design, §12 (commit `f10f836`, #48).

### 2026-10-02 · The AI under fog of war

| Quantity | Value |
|---|---|
| A player who does nothing | beaten at tick 6,516 (5:26), against 6,483 without fog |
| A player who holds the middle | beaten at tick 7,360 |

- **Build:** The MVP with fog of war, in the Linux container; the simulation is deterministic, so the figures are the game's (ADR-009)
- **Seeds:** not stated
- **Method:** `AiPlayerTests`.
- **Source:** ADR-024, Consequences (commit `f0a9916`, #48).

### 2026-10-02 · How long a match against the AI lasts, in the owner's play

| Quantity | Value |
|---|---|
| Match length against the AI before Phase 1 (the owner's play) | about five minutes when the owner rushes the AI, and 15-25 minutes when he does not |

- **Build:** The MVP as finished, on the owner's machine
- **Seeds:** not stated
- **Method:** The owner's matches; no log is cited.
- **Source:** the Phase 1 design, §1 (commit `169fbf7`, #49).

### 2026-10-03 · Two AIs with the match log (task 13.1)

| Quantity | Value |
|---|---|
| Two AIs with task 13.1's match log (--ai-matches) | matches end in 22 minutes, the median of 40 seeds, from 19:37 to 32:18, none in 45-60; tier 2 opens at 15:04 and tier 3 is never reached; player 1 wins 30 of the 40 with the same AI in both seats (cause not known) |
| Seeds 1 to 10 | all ten end: a median of 21:50, from 19:47 to 32:18 |
| Seeds 11 to 40 | a median of 22:44, from 19:37 to 29:14; all 40 together 22:11 |
| Tier 2 opened | by every AI at 15:04 but one, at 15:09; none reaches tier 3 |
| A side's peak | a median of 80 warships, from 62 to 116 |
| Seat bias | player 1 won 30 of the 40; connecting player 2 first gave the same 30 to 10, so it is not the order in which the server applies the players' commands |

- **Build:** Phase 1 at task 13.1: `--ai-matches`' code built by clang 18 at `-O2` in the Linux container against the real server and data
- **Seeds:** 1 to 40
- **Method:** `--ai-matches`, read by `Tools/MatchLog.py --ai-matches`.
- **Source:** the Phase 1 plan, task 13.1 (commit `882c112`, #49); ADR-038, Consequences (commit `882c112`, #49).

### 2026-10-03 · Ships turn in arcs: two AIs (task 7.2)

| Quantity | Value |
|---|---|
| Two AIs' match length after ships turn in arcs (task 7.2) | a median of 23:32 over 140 seeds, against 22:11 before |
| Seat bias | player 2 won 89 of the 140, where player 1 had won 30 of 40 |

- **Build:** Phase 1 at task 7.2, in the Linux container
- **Seeds:** 140 seeds; which ones is not stated
- **Method:** AI against AI.
- **Source:** the Phase 1 plan, task 7.2 (commit `8c6acd2`, #49); ADR-039, Consequences (commit `5a7d99d`, #49).

### 2026-10-03 · Ships turn in arcs: the balance check (task 7.2)

| Quantity | Value |
|---|---|
| Balance check after ships turn in arcs (task 7.2) | (a), (b) and (c) pass at every stage; (d) is UNSURE at tier 1: with Hull Plating, the plated swarm's best answer at 2,000 Ore under spread fire wins 53% of 480 battles, where it won 55% before; nothing retuned until the owner decides |

- **Build:** Phase 1 at task 7.2, in the Linux container
- **Seeds:** not stated
- **Method:** The full balance check run again with arcs.
- **Source:** the Phase 1 plan, task 7.2 (commit `8c6acd2`, #49); ADR-039, Consequences (commit `8c6acd2`, #49).
- **Note:** Phase 1's check of record later passed (d); whether that settles it is gate R9 of ImplementationPlan-Reconcile.md.

### 2026-10-03 · Large orders over two ticks: the balance check before and after (task 8.2)

| Quantity | Value |
|---|---|
| Balance check before and after task 8.2's order-tick fix | passes all four criteria both times; large armies set off a tick later, so the shares of what is worth building move by a few points (ADR-032 lists the two that move more); tier-1 verdicts are the MVP's |
| The two shares that moved most | At 12,000 Ore under spread fire the Medium hull, not worth building before, is, in M+I+La at 21%; at 4,500 Ore under spread fire the starting stage is an even three-way of S+I+MD, M+I+MD and M+I+La, where it was M+I+MD and S+I+La with S+I+MD at 7%. |
| The recorded counters | every one holds at 2,000 Ore, where the armies are under 32 ships (`BalanceCheckTests.TheRecordedCountersHold`) |

- **Build:** Phase 1 at task 8.2, in the Linux container; the check's battles are played by groups of up to 137 ships
- **Seeds:** not stated
- **Method:** The full balance check before and after the change.
- **Source:** the Phase 1 plan, task 8.2 (commit `573190e`, #49); ADR-032, Consequences (commit `9782b1c`, #49).
- **Note:** Whether the shift matters is gate R8 of ImplementationPlan-Reconcile.md.

### 2026-10-03 · The staged balance check, first runs (task 10.3)

| Quantity | Value |
|---|---|
| Criterion (a) at tiers 2 and 3, with task 10.2's numbers | holds: every design has a counter |
| The Flak Battery as first written (9 a hit, 140 m, a 20 m splash, 70 Ore) | (b) fails on it at every budget of tiers 2 and 3: it beats every Small design 20-0 and loses 0-20 to every Medium and Large one; its 20 m splash reaches nobody (the formation stands Small hulls 24 m apart); a 26 m splash, or a hit of 12 or 14 at 60 Ore, still loses 0-20 to every Medium design |
| The Mass Driver in (b) at tiers 2 and 3 | fails at tier 3 in both fire modes and at tier 2 under focus fire; with the MVP's numbers no Mass Driver design was worth building under focus fire at 4,500 Ore or more, and under spread fire only S+I+MD at 7% at 12,000 |
| Ablative Armour at +15%, criterion (d) | M+I+La with +45% hit points against the tier 2 designs with +30% has no answer better than 37% at 6,000 Ore under focus fire |

- **Build:** Phase 1 at task 10.3, first runs, in the Linux container
- **Seeds:** not stated
- **Method:** 20 battles a pairing, without (c).
- **Source:** the Phase 1 plan, task 10.3 (commit `b372cb2`, #49).
- **Note:** For the plated Medium Lance line at 6,000 Ore under focus fire with Ablative Armour at +15%, the Phase 1 plan gives no answer better than 37% and the Phase 1 design gives the picket 33% (the next entry); both are as recorded.

### 2026-10-03 · Tuning the Flak Battery and Ablative Armour (task 10.3)

| Quantity | Value |
|---|---|
| The picket against the fully plated Medium+Ion+Lance line, by Ablative Armour's share | at +15% the picket wins 33% under focus fire at 6,000 Ore, at +10% 45%; at +5% it wins 95-100% at every tier 3 budget (585 hit points, 6.7 of the picket's 87-point hits, still seven hits) |
| The tuned Flak Battery (20 a hit, 200 m, a 26 m splash, 60 Ore) | beats the swarm, the picket, the brawler and the Medium Missile Rack, trades evenly with the Lance line under spread fire, and loses to it under focus fire and to the heavy Lance and missile lines |
| The check after the tuning, at 12 battles a pairing without (c) | (a) and (b) hold at tiers 2 and 3, the Flak Battery and the Rail Cannon each worth building at every one of their budgets |

- **Build:** Phase 1 at task 10.3, in the Linux container
- **Seeds:** not stated
- **Method:** The balance check per tier.
- **Source:** the Phase 1 design, §6 and §5 (commits `573190e` and `ac86d73`, #49); the Phase 1 plan, task 10.3 (commit `ac86d73`, #49).

### 2026-10-03 · P3: Phase 1's balance check of record, every tier

| Quantity | Value |
|---|---|
| P3: Phase 1's balance check of record, every tier | passes all four criteria at every stage; tier 1's verdicts are the MVP's; at tier 2 the Flak Battery is worth building at every budget, in Small, Medium and Large designs; at tier 3 the Rail Cannon is, and the Medium Lance line and the Missile Rack stay in the mix; no design worth building uses the Pulse Drive; the weakest answer to a researched design is 93% (the picket against the Medium Lance line with Composite Plating), 97% with Ablative Armour as well |
| The Rail Cannon against the heavies and the picket | Large+Fusion+Rail beats Large+Fusion+Lance; it does not answer the Lance picket, which still beats every heavy, since ships stand at their own range rather than kite |

- **Build:** Phase 1 after task 10.3, in the Linux container; the run of record on the development machine was not made
- **Seeds:** not stated
- **Method:** `BalanceCheckTests.TheFullCheck`: 60 battles a pairing, up to 480 when uncertain, and 3,604 counter checks under one-number changes of ±5%.
- **Source:** the Phase 1 design, §7 and §5 (commits `ad27324` and `ec5c0c7`, #49).

### 2026-10-03 · The AI in `AiPlayerTests`, Phase 1

| Quantity | Value |
|---|---|
| Tier 3 against a player who does nothing (`ReachesTierThree`) | the AI opens tier 3 at tick 37,200, 31 minutes in |
| The AI follows the ore (`FollowsTheOre` and `ReachesTierThree`) | with its home reserves at 200 Ore, after eight minutes the AI keeps three dry rigs and six on ore, each away from home with a platform; without the change the AI's income fell to the trickle at minute 21 and its Shipyards took every Ore the lab waited for |
| A player who does nothing, on the 5 km map | it beats the player at 7.8 minutes, where it took 5.4 on the 2 km map |
| After losing all production became the rule (task 12.1) | it still beats a player who does nothing at 7.8 minutes, and one who holds the middle at 9.0 |
| The same, in ticks | a player who does nothing at tick 9,390, one who holds the middle at tick 10,811 |

- **Build:** Phase 1, tasks 10.4 to 12.1; the Linux container where stated
- **Seeds:** not stated
- **Method:** `AiPlayerTests`.
- **Source:** the Phase 1 plan, tasks 10.4, 11.3, 11.2 and 12.1 (commits `ec5c0c7`, `4d5426b`, `a67fd29` and `8b4f7fa`, #49); ADR-037, Consequences (commit `8b4f7fa`, #49).
- **Note:** The runs' own dates are not stated; they were recorded on 2026-10-02 and 2026-10-03.

### 2026-10-03 · Two AIs before the longer match (task 12.2)

| Quantity | Value |
|---|---|
| Two AIs' match length before task 12.2 | a median of 24 minutes, decided by one battle at minute 12, before tier 2 |

- **Build:** Phase 1 before task 12.2; where it ran is not stated
- **Seeds:** not stated
- **Method:** AI against AI.
- **Source:** the Phase 1 plan, task 12.2 (commit `86b3bc2`, #51).

### 2026-10-03 · The AI plays a longer match (task 12.2)

| Quantity | Value |
|---|---|
| Two AIs after task 12.2 (an attack group of 20 and 12 a tier, a fall-back at 30% of losses then 150 s, 2 platforms a Shipyard) | median 53.9 minutes for seeds 1-40, the middle half 43.0-74.0; 50.8 minutes for seeds 41-80, of which 3 had not ended at 150 minutes; 56.3 minutes for the switch's seeds 1-10; of all 80, 23 end within 45-60 minutes; the seat bias is gone, 39 wins against 38; only 2 of the 80 reach tier 3 |
| Over all 80 seeds | a median of 52 minutes over 80 seeds; only 23 of the 80 within 45-60, and 3 had not ended at 150 minutes |
| The spread | of the 80 matches, 23 end within 45-60 minutes, 25 before and 32 after; the 3 not ended at 150 minutes are all of seeds 41 to 80 |
| P2: the tiers reached | 78 of 80 matches reach tier 2, and only 2 reach tier 3 (an AI at war seldom affords the 600-Ore gateway) |
| Candidates tried and dropped | falling back from a stronger fleet gave 32-34 minutes; leaving the enemy's base alone until a later tier gave 26 minutes at tier 2 and stalled 36 of 40 matches at tier 3 |
| The same candidates, as ADR-041 gives them | falling back from a fleet stronger than the group, by hit points times damage a second: a median of 32.2 to 33.5 minutes; leaving the enemy's base alone until tier 2 or 3: a median of 25.7 at tier 2, and at tier 3 alone 36 of 40 matches still going at 150 minutes |

- **Build:** Phase 1 at task 12.2: `--ai-matches`' code built by clang 18 at `-O2` in the Linux container against the real server and data
- **Seeds:** 1 to 40, 41 to 80 (run as a check after the numbers were chosen) and the switch's own 1 to 10
- **Method:** `--ai-matches`; a match not ended at 150 minutes counts as the longest.
- **Source:** the Phase 1 plan, task 12.2 (commit `86b3bc2`, #51); the Phase 1 design, §13 and §2 (commits `86b3bc2` (#51) and `8197633`); ADR-041, Consequences (commit `86b3bc2`, #51).

### 2026-10-03 · Groups keep their lanes, and the AI regroups for 120 s (task 9.7)

| Quantity | Value |
|---|---|
| P1: two AIs' match length after task 9.7 (AI regroups for 120 s instead of 150 s, lanes round obstacles) | a median of 47.5 minutes over seeds 1-40, and 51.1 over seeds 41-80; without the change, lanes made it 66.4 and 60.5 |
| Median length by setting, seeds 1-40 / 41-80 | before lanes 53.9, all ended; lanes with the AI unchanged 66.4, all ended / 60.5, 2 not ended; lanes with a 120 s regroup 47.5, 2 not ended / 51.1, all ended |
| Within 45-60 minutes, with the 120 s regroup | 11 of seeds 1-40 and 15 of seeds 41-80; each side wins about half, 20-18 and 18-22 |
| Settings tried on seeds 1-40 and dropped | attack groups of 16: 50.0, 2 not ended, one side winning 28 of 38; groups of 18: 42.6, one side winning 31 of 40; falling back at 40% losses: 49.6, 5 not ended; one platform per Shipyard: 60.7, one side winning 30 of 40; with groups of 18: 41.8; with a 135 s regroup: 41.0 |
| The spread | match lengths run from about 30 to over 100 minutes, and the medians move by up to ten minutes between neighboring settings |
| A match that does not end | one AI holds about 200 warships against about 50 for the last hour and never finishes the match |

- **Build:** Phase 1 after task 9.7, in the Linux container, measured as ADR-041 measures
- **Seeds:** 1 to 40 and 41 to 80
- **Method:** AI against AI with the match log; a match not ended at 150 minutes counts as the longest.
- **Source:** the Phase 1 plan, task 9.7 (commit `ced8fc8`, #52); ADR-047, Consequences (commit `ced8fc8`, #52).

### 2026-10-03 · Territory: the AI claims its flanks (task 14.5)

| Quantity | Value |
|---|---|
| Two AIs, seeds 1 to 10 | all end, at a median of 45:48, from 31:03 to 1:26:51 |
| The same seeds before the change | a median of 1:06:29, from 29:37 to 1:39:58 |

- **Build:** Phase 2 at milestone 14, in the Linux container, clang 18 at `-O2` (ADR-038)
- **Seeds:** 1 to 10
- **Method:** AI against AI.
- **Source:** the Phase 2 plan, task 14.5 (commit `eda8819`); ADR-056, Consequences (commit `28a34f7`, #55).

### 2026-10-03 · Domination (task 15.1)

| Quantity | Value |
|---|---|
| Two AIs, seeds 1 to 10 | all end, at a median of 41:50, from 31:03 to 58:38; three end by domination |
| The three endings by domination | at 42:00, 43:00 and 51:20, each won by player 2; they ran 50 to 87 minutes before |
| Seed 2, followed with a probe | both sides hold their home and flanks until about 25 minutes; then player 2's home rigs run dry and it builds Relays in the center and the northwest; player 1 holds three nodes to its five from then on and loses by domination at 51:20 |

- **Build:** Phase 2 at milestone 15, in the Linux container as ADR-056 measured; the AI does not yet play for nodes
- **Seeds:** 1 to 10
- **Method:** AI against AI; the probe printed the territory every five minutes.
- **Source:** the Phase 2 plan, task 15.1 (commit `92eb257`, #56); ADR-057, Consequences (commit `3d63d8e`, #56).

### 2026-10-03 · Territory's play (task 18.1)

| Quantity | Value |
|---|---|
| Before task 18.1, Relays only where the ore takes it | seeds 1 to 10 end at a median of 41:50 |
| Task 18.1's first cut: every free sector next to its territory, raids of 4 every 90 s | seeds 1 to 10 end at a median of 23:36; followed with a probe, seed 1's winner held 7 nodes, earned 150 Ore/s and had 25,000 Ore banked by 16:00 |
| The starting values: one claim, raids of 2 every 180 s | a median of 20:23, from 11:11 to 35:49, none within 45-60 minutes, every match ended and all by production; every match has a shot by minute 5 and five engagements in three sectors before minute 20; player 2 won 14 |
| Before ties were mirrored, when player 1 won 16 of the 20 with the starting values | without claims or raids 40:34; one claim and no raids 26:32; raids and no claim 28:44; waiting for a lead of 2 nodes or twice the group 11:31, every match by production |

- **Build:** Phase 2 after milestone 18, in the container's harness
- **Seeds:** 1 to 20 unless a row says otherwise
- **Method:** AI against AI on the repository's map with territory, with ADR-038's match log.
- **Source:** the Phase 2 design, §2 (commit `fcf7060`, #60); ADR-041, Consequences (commit `0352511`, #59); ADR-020, Decision (commit `0352511`, #59).

### 2026-10-03 · Ships turn in arcs: a single ship's move (task 7.2)

| Quantity | Value |
|---|---|
| What a turn costs | a U-turn takes about 8% longer: a Large+Fusion ship sent 100 m behind itself arrives in 8.55 s against 7.90 s; a point 15 m abeam, which the old rule circled for seconds and gave up 2 m short of, is reached exactly in 0.75-3.6 s, depending on the hull |
| A U-turn's loop | a loop about 20-25 m across |

- **Build:** Phase 1 at task 7.2, in the Linux container
- **Seeds:** not applicable
- **Method:** Movement tests in the container.
- **Source:** the Phase 1 plan, task 7.2 (commit `8c6acd2`, #49); the Phase 1 design, §9 (commit `5a7d99d`, #49).

### 2026-10-04 · S1 to S4: the fall-back tuned (task 19.2)

| Quantity | Value |
|---|---|
| Falls back after 30% of losses, regroups for 120 s | seeds 1-20: 20:23, none of 20 within 45-60 |
| 10% and 180 s | seeds 1-40: 45:05, 17 within 45-60; seeds 1-10: 42:34 |
| 15% and 240 s | seeds 1-40: 44:30, 19 within; seeds 1-10: 45:15 |
| 10% and 240 s, chosen (the owner kept the 240 s, 2026-10-04) | seeds 1-40: 47:55, 25 within; seeds 1-10: 44:50 |
| S1: first shot by minute 5 | all 10 of seeds 1-10 have a shot by minute 5, and all 40 of seeds 1-40 |
| S2: engagements before minute 20 | seeds 1-10 have a median of 16.5 engagements in 6 sectors; every one of the 40 has at least five in three sectors |
| S3: how matches end | of seeds 1-10, 6 end by domination and 4 by production; of seeds 1-40, 27 and 13 |
| S4: match length | seeds 1-40 end at a median of 47:55, 25 of them within 45-60 minutes, every one ended; seeds 1-10 alone give 44:50, 10 seconds short |
| The chosen numbers over seeds 1-40 | every match ends, from 25:48 to 1:23:00, with a median of 16 engagements in 6 sectors before minute 20; player 1 wins 16 and player 2 24; a side's peak is a median of 175.5 warships, from 72 to 347 |

- **Build:** Phase 2 after task 19.2, in the container's harness. Floats replay only on the same build (ADR-009)
- **Seeds:** 1 to 10 and 1 to 40
- **Method:** AI against AI with ADR-038's match log; an engagement is both sides firing in one sector within 10 s of each other, another counting after 30 s without a shot.
- **Source:** the Phase 2 design, §2 (commits `2b4fe06` and `fcf7060`, #60); ADR-041, Consequences (commit `2b4fe06`, #60).
- **Note:** The owner's match was not measured with the match log, so S1, S2 and S4 rest on these figures.

### 2026-10-04 · Shipyard levels (task 21.1)

| Quantity | Value |
|---|---|
| Two AIs after task 21.1 (Shipyard levels), against main's Phase 2 on the same harness | every match ends, at a median of 49:30 against 44:50; every first shot by minute 5, as before; 11.5 engagements in 4 sectors before minute 20 against 16.5 in 6, every match still meeting S2; 7 dominations to 3 against 6 to 4; the harness on main reproduces task 19.2's figures exactly |
| Seed 1's first Large ship | 1:46 after its Large Hull research (the level 3 upgrade and the hull's build time) |

- **Build:** Phase 3 at milestone 21, in the container's harness
- **Seeds:** 1 to 10
- **Method:** AI against AI with the match log, against `main`'s Phase 2 on the same harness.
- **Source:** the Phase 3 plan, task 21.1 (commit `5d0bdda`, #68).

### 2026-10-04 · The Lab's levels open the tiers (task 22.1)

| Quantity | Value |
|---|---|
| Tier 3 opened after the gateways became Lab levels (task 22.1) | without the AI holding back production for an upgrade it cannot pay for, no AI of seeds 1-10 opened tier 3; with it, 5 of the 20 seats do, as many as with the gateway on main |
| Two AIs after task 22.1 (Lab levels), against main | every match ends, at a median of 48:45 against 44:50; S1 10 of 10; S2 a median of 11 engagements in 4 sectors, every match meeting it; 7 dominations to 3; tier 3 opened in 5 of 20 seats, as on main, against Phase 1's 2 of 80 matches |

- **Build:** Phase 3 at milestone 22, in the container's harness
- **Seeds:** 1 to 10
- **Method:** AI against AI.
- **Source:** the Phase 3 plan, task 22.1 (commit `0d9070e`, #69).

### 2026-10-04 · The Command Station's levels cap the nodes (task 23.1)

| Quantity | Value |
|---|---|
| Two AIs after task 23.1 (Command Station levels), against milestone 22 | all 10 end; median 52:25 against 48:45, from 36:56 to 1:17:40; 6 within 45-60 minutes against 7; S1 10 of 10; S2 a median of 11 engagements in 4 sectors, all 10 meeting it; S3 8 by domination and 2 by production, against 7 and 3; tier 3 opened in 8 of the 20 seats, against 5 |
| `AiPlayerTests.FollowsTheOre` after the node cap | the test needs 9 minutes rather than 6, since the AI's third claim waits for the station's upgrade |

- **Build:** Phase 3 at milestone 23, in the container's harness
- **Seeds:** 1 to 10
- **Method:** AI against AI; `AiPlayerTests`.
- **Source:** the Phase 3 plan, task 23.1 (commits `68fe4db` and `7d2b2fc`, #70).

### 2026-10-04 · The AI plays the levels (task 24.1)

| Quantity | Value |
|---|---|
| The station's upgrade in `AiPlayerTests.UpgradesItsStationBeforeItsFirstClaim` | it upgrades to level 2 at about 3:30 and holds four nodes by 6:00, and goes no higher with no claim left |
| Two AIs after task 24.1 (the AI plays upgrades), against task 23.1's tree | a median of 30:07 against 52:25; 1 within 45-60 minutes against 6; 2 by domination and 8 by production against 8 and 2; S2 met in 5 of 10 against 10; tier 3 in 1 of 20 seats against 8; without the claim fix it is 52:50 and 8 by domination; without the levels in what it attacks nothing moves |
| The spread, and the claim alone taken out | from 22:02 to 48:50; with the claim alone taken out 52:50, 8 by domination and tier 3 in 7 seats; with the levels left out of what it attacks the figures do not move |

- **Build:** Phase 3 at milestone 24, in the container's harness
- **Seeds:** 1 to 10
- **Method:** AI against AI, against task 23.1's tree.
- **Source:** the Phase 3 plan, task 24.1 (commit `9664a27`, #71); ADR-041, Consequences (commit `9664a27`, #71).
- **Note:** The owner kept the claim and left the retuning to task 25.2 (2026-10-04).

### 2026-10-04 · T1 to T4: the AI tuned again (task 25.2)

| Quantity | Value |
|---|---|
| From task 24.1's numbers, seeds 1-40 | a median of 29:48, 1 within 45-60 minutes, 36 by production, T3 in none and tier 3 in 3 of 80 seats |
| Candidates, seeds 1-20: median; within 45-60; domination / production; T2; T3 (all three at level 3); T4 median | none changed: 29:48; 1; 3 / 17; 3:52; 0; 6. Lead 2: 29:16; 2; 2 / 18; 3:52; 0; 6. 2 claims: 1:00:25; 5; 15 / 5; 8:48; 8; 0. 2 claims, lead 2: 51:50; 9; 18 / 2; 8:48; 9; 0. 2 claims, group 25: 55:10; 13; 16 / 4; 9:25; 7; 0. The same falling back at 20%: 45:48; 9; 13 / 7; 9:25; 4; 2.5. At 30%: 41:15; 6; 9 / 11; 9:25; 4; 4. At 15%, chosen: 50:56; 13; 13 / 7; 9:25; 5; 4.5 |
| T1: Phase 2's S1 to S4, seeds 1-40 | all 40 end, at a median of 52:45, 28 of them within 45-60 minutes (Phase 2 had 47:55 and 25); S1 met in all 40, S2 in 39; S3 sees both endings, 29 by domination and 11 by production; player 1 wins 28 of the 40 |
| T2: both players at their caps with equal nodes | a median of 9:28 a match, from 6:14 to 10:33, against the 5 minutes asked; both reach level 1's three nodes at about 2:25 and wait for level 2, then sit at four each until one reaches level 3; missed, and kept (owner, 2026-10-04) |
| T2's alternatives, measured and dropped | a level 2 of 150 Ore and 30 s gave 8:40; a cap one node higher at every level gave 5:10 but a median match of 25:08 that never needed level 3 |
| T3: structure levels reached, and tier 3 | each side reaches level 3 of the Command Station and of a Shipyard in all 40, and of the Research Lab in 11, so all three in 11 of the 40; tier 3 is opened in 33 of the 40 matches and by 44 of the 80 players, against Phase 1's 2 in 80; missed on the Lab, and kept |
| T4: upgraded structures attacked | a median of 4 structures above level 1 attacked a match, from 0 to 12 |
| Two AI changes tried and dropped, seeds 1-20 | upgrading the station whenever it is at its cap with a claim left changed nothing; doing so ahead of its plan started level 2 at 1:33 and gave a median of 41:30, T3 in none and tier 3 in 3 of 40 seats, for a stall of 6:09 |

- **Build:** Phase 3 after task 25.2 (2 claims beyond its rigs, an attack group of 25, a fall-back after losing 15%), in the container's harness. Floats replay only on the same build (ADR-009)
- **Seeds:** 1 to 20 for each candidate, 1 to 40 for the chosen one
- **Method:** AI against AI with ADR-038's match log.
- **Source:** the Phase 3 design, §2 (commit `363f137`, #73); ADR-041, Consequences (commit `363f137`, #73).
- **Note:** T2 and T3 were missed, and kept (owner, 2026-10-04).

### 2026-10-04 · The AI's difficulties (task 26.1)

| Quantity | Value |
|---|---|
| The AI's difficulties against each other (task 26.1) | on seeds 1-20, each difficulty in both seats, Normal beats Easy in all 20 and Hard beats Normal in all 20, before task 25.2's tuning and after it; a larger economy alone made Hard weaker |
| Hard's candidates against Normal | a larger economy alone (5 Constructors, 4 contested rigs, a Shipyard for every 8 Ore a second, 3 jobs queued, a review every 45 s) won 5 of 20, and 6 of 20 when it also attacked with 16 warships; with raids and claims added to it 11 of 20; raids and claims alone 14 of 20; raids, claims, 5 Constructors and the 45 s review, the numbers chosen, 20 of 20; a third claim changed nothing measured |
| Before task 25.2 | Normal wins all 20 against Easy, 10 from each seat, at a median of 31:40, 17 by domination; Hard wins all 20 against Normal, 10 from each seat, at a median of 19:06, 19 by production |
| After task 25.2 | Normal wins all 20 against Easy at a median of 23:56, 18 by domination; Hard wins all 20 against Normal at a median of 29:10, 12 by domination; Hard with 3 claims plays every one of the 20 the same |

- **Build:** Phase 3: the Linux harness's build of `--ai-matches` (ADR-063)
- **Seeds:** 1 to 20, the difficulty in seat 1 for seeds 1-10 and in seat 2 for 11-20
- **Method:** AI against AI.
- **Source:** the Phase 3 plan, task 26.1 (commit `782275d`, #74); ADR-065, Decision and Consequences (commits `10b7eda` (#73) and `0c0b03c` (#74)).

### 2026-10-04 · A group attacking past an obstacle (task 14.4)

| Quantity | Value |
|---|---|
| The group's shape | 25 ships attacking a target moving away beyond a 200 m field: the group is at most 2.5 times as long as wide, 1.5 while it passes the field; with the old pathing grafted back in it was 6.2 |

- **Build:** Phase 2 at task 14.4, in the Linux container's stand-in harness
- **Seeds:** not applicable
- **Method:** `MovementTests.AnAttackingGroupPassesAnObstacleSideBySide`.
- **Source:** the Phase 2 plan, task 14.4 (commit `0d3a678`, #55).

### 2026-10-05 · The Lab's second slot, by `secondSlotTier` (SP1.4)

| Quantity | Value |
|---|---|
| Effect of secondSlotTier on AI-against-AI matches (SP1.4) | at secondSlotTier 1, 2 and 3 every seed logged the same bytes; at 4 no Lab reached level 4, and on seeds 1 and 4 (where player 1's Lab had reached it at 3) the match played on differently: seed 4's winner then won by production at 50:45 rather than by domination at 57:00; seeds 2 and 3 unchanged |

- **Build:** The Linux container: the server and the AI built with clang 18 against a stand-in for the Windows headers. Floats replay only on one build
- **Seeds:** 1 to 4, both players at each `secondSlotTier` from 1 to 4
- **Method:** `--ai-matches` over the seeds at each setting, comparing the match logs.
- **Source:** the self-play plan, SP1.4's As built (commit `d9887e3`, #80).

### 2026-10-07 · Before milestone 27: Phase 3's rules on the 5 km map

| Quantity | Value |
|---|---|
| Most warships a side has at once: median of the 80 sides (largest) | 193 (565) |
| Warships a side has at minute 20, median; sides with 20 or more | 123.5 |
| Warships a side builds in a match, median | 400.5 |
| First shot of the match, median | 1:58 |
| Match length, median (shortest-longest) | 41:27 (24:24-62:48) |
| Endings | 16 domination, 24 production |
| Ore a side holds at minute 20, median; sides over 2,000 | 136 |

- **Build:** Phase 4's `main` before milestone 27, on the 5 km map, in the Phase 4 harness
- **Seeds:** 1 to 40; the Normal AI in both seats
- **Method:** AI against AI, scored by the harness's counts and `Tools/MatchLog.py`.
- **Source:** the Phase 4 design, §2 (commit `b19efdf`, #81).

### 2026-10-07 · Milestone 27: tripled costs and the fleet cap, at Phase 3's income

| Quantity | Value |
|---|---|
| Most warships a side has at once: median of the 80 sides (largest) | 24.5 (50) |
| Sides over 40 warships at once | 14 of 80 |
| Warships a side has at minute 20, median; sides with 20 or more | 15.5; 36 of 80 |
| Warships a side builds in a match, median | 87 |
| First shot of the match, median | 2:47 |
| Match length, median (shortest-longest) | 53:50 (44:48-1:29:18) |
| Endings | 40 domination |
| Ore a side holds at minute 20, median; sides over 2,000 | 8,823; 50 of 80 |

- **Build:** Milestone 27 at Phase 3's income, on the 5 km map, in the Phase 4 harness
- **Seeds:** 1 to 40; the Normal AI in both seats
- **Method:** AI against AI, scored by the harness's counts and `Tools/MatchLog.py`.
- **Source:** the Phase 4 design, §2 (commit `b19efdf`, #81).

### 2026-10-07 · Milestone 27 with the rigs' income lowered by about a third

| Quantity | Value |
|---|---|
| Most warships a side has at once: median of the 80 sides (largest) | 21 (38) |
| Sides over 40 warships at once | 0 of 80 |
| Warships a side has at minute 20, median; sides with 20 or more | 15; 8 of 80 |
| Warships a side builds in a match, median | 66 |
| First shot of the match, median | 3:14 |
| Match length, median (shortest-longest) | 1:01:05 (46:10-1:31:10) |
| Endings | 40 domination (no AI took the other's base; the siege ended 24 of 40 before) |
| Ore a side holds at minute 20, median; sides over 2,000 | 733; 19 of 80 (a quarter of the sides still over 2,000, mostly the side ahead) |

- **Build:** Milestone 27 with the income that was built, on the 5 km map, in the Phase 4 harness
- **Seeds:** 1 to 40; the Normal AI in both seats
- **Method:** AI against AI, scored by the harness's counts and `Tools/MatchLog.py`.
- **Source:** the Phase 4 design, §2 (commit `b19efdf`, #81).
- **Note:** The owner lowered the rigs' income on 2026-10-07, and chose to measure the siege again on the 10 km map rather than tune it on the 5 km one.

### 2026-10-07 · Milestone 27 with the income halved instead (measured, not built)

| Quantity | Value |
|---|---|
| Milestone 27 with the income halved instead (measured, not built) | the median side peaks at 21 warships and builds 47.5; the first shot comes at 4:03; three of the 40 end by production; the median side holds 421 Ore at minute 20 |

- **Build:** Milestone 27 with the income halved, on the 5 km map, in the Phase 4 harness
- **Seeds:** 1 to 40; the Normal AI in both seats
- **Method:** AI against AI, scored by the harness's counts and `Tools/MatchLog.py`.
- **Source:** the Phase 4 design, §2 (commit `b19efdf`, #81).

### 2026-10-07 · The AI's economy after the income cut

| Quantity | Value |
|---|---|
| The AI's economy in `AiPlayerTests` after the cut | its scout sets out at about 2:30, inside three minutes rather than two; it has 2 Shipyards at 28.14 Ore/s rather than 4 at 41.25; it has six rigs with ore at fourteen minutes rather than twelve |
| The scout, as ADR-020 gives it | it sets out in the first three minutes, at about 2:30 on Phase 4's income |

- **Build:** Phase 4 at milestone 27, in the Linux container; the configuration is not stated
- **Seeds:** not stated
- **Method:** `AiPlayerTests`' economy tests, timed again.
- **Source:** the Phase 4 plan, task 27.6 (commit `b19efdf`, #81); ADR-020, Decision (commit `01a20da`, #90).
- **Note:** The Phase 4 plan gives the scout's 2:30 after task 27.6, which lowered the rigs' income; ADR-020 gives it at task 27.5, before that cut. Both are as recorded.

### 2026-10-07 · Milestone 28: the 10 km map, still empty

| Quantity | Value |
|---|---|
| Most warships a side has at once: median of the 80 sides (largest) | 25.5 (50) |
| Sides over 40 warships at once | 14 of 80, all late in a long match, filling the cap of 50 points with Small hulls |
| Warships a side has at minute 20, median; sides with 20 or more | 13; 0 of 80 |
| Warships a side builds in a match, median | 129.5 |
| First shot of the match, median | 4:42 in every match (4:42-4:43), the scouts meeting at the enemy's flank |
| Match length, median (shortest-longest) | 2:08:55 (1:40:30-3:04:20) |
| Endings | 40 domination; won by player 1: 22 |
| Ore a side holds at minute 20, median; sides over 2,000 | 2,625; 50 of 80 |
| Ore a side holds at minute 30, median | 18,236 |
| Command Station level and fleet cap by minute 20 on the 10 km map (after milestone 28) | every side's Command Station is at level 5, its cap 50 points, by minute 20, and every side fills that cap at some point in its match |

- **Build:** After milestone 28, on the 10 km map without pirates or derelicts, matches up to four hours, in the Phase 4 harness
- **Seeds:** 1 to 40; the Normal AI in both seats
- **Method:** AI against AI, scored by the harness's counts and `Tools/MatchLog.py`.
- **Source:** the Phase 4 design, §2 (commits `d96bc37` and `cfc275f`, #82).
- **Note:** The owner then had domination's drain stop dividing by the map's nodes, and the seed place each sector's asteroids (2026-10-07).

### 2026-10-07 · The AI's economy on the 10 km map

| Quantity | Value |
|---|---|
| The AI's economy in AiPlayerTests on the 10 km map (task 28.2) | two Shipyards by nine minutes rather than seven, and six rigs with ore by sixteen minutes rather than fourteen |

- **Build:** Phase 4 at milestone 28, in the Linux container; the configuration is not stated
- **Seeds:** not stated
- **Method:** `AiPlayerTests`' economy tests, timed again.
- **Source:** the Phase 4 plan, task 28.2 (commit `465eb84`, #82).

### 2026-10-07 · Milestone 29: placement from the seed

| Quantity | Value |
|---|---|
| Most warships a side has at once: median of the 80 sides (largest) | 21 (46) |
| Sides over 40 warships at once | 4 of 80 |
| Warships a side has at minute 20, median; sides with 20 or more | 14; 12 of 80 |
| Warships a side builds in a match, median | 82 |
| First shot of the match, median (earliest-latest) | 4:42 (4:35-5:21); it varies by up to 46 seconds |
| Match length, median (shortest-longest) | 1:37:06 (54:09-2:30:39); half the matches last 1:23 to 1:50 |
| Endings; won by player 1 | 40 domination; 20 |
| Ore a side holds at minute 20, median; sides over 2,000 | 1,777; 32 of 80 |
| Ore a side holds at minute 30, median | 17,135 |

- **Build:** After milestone 29, on seeded 10 km maps, in the Phase 4 harness
- **Seeds:** 1 to 40; the Normal AI in both seats
- **Method:** AI against AI, scored by the harness's counts and `Tools/MatchLog.py`.
- **Source:** the Phase 4 design, §2 (commit `371692c`, #83).

### 2026-10-07 · Milestone 30: pirates

| Quantity | Value |
|---|---|
| Most warships a side has at once: median of the 80 sides (largest) | 20 (47) |
| Sides over 40 warships at once | 7 of 80 |
| Warships a side has at minute 20, median; sides with 20 or more | 13; 9 of 80 |
| Warships a side builds in a match, median | 64 |
| First shot of the match, median (earliest-latest), any shot | 4:02 (4:01-5:21), 40 seconds sooner, the scouts meeting a camp on the way |
| Match length, median of those that end (shortest-longest) | 1:31:42 (43:27-2:33:42) |
| Endings; won by player 1 | 34 domination, 3 production, 3 not ended at 3 hours; 20 (three AIs took the other's base, the first on the 10 km map) |
| Ore a side holds at minute 20, median; sides over 2,000 | 2,474; 44 of 80 |
| Ore a side holds at minute 30, median | 18,754 |
| Pirate structures and ships left at the end, median | 7.5 of 10; 14 of 24 (most outposts outlast the match) |
| The AI and the guarded sectors, seed 3, 25 minutes with eight claims allowed | an AI that tried the guarded sectors ordered 145 Relays there and held 7 sectors; this one orders none and holds 9 |

- **Build:** After milestone 30, pirates on seven nodes and an AI that leaves them alone, matches up to three hours, in the Phase 4 harness
- **Seeds:** 1 to 40; the Normal AI in both seats, and seed 3 for the guarded sectors
- **Method:** AI against AI, scored by the harness's counts and `Tools/MatchLog.py`.
- **Source:** the Phase 4 design, §2 (commit `9214426`, #85); ADR-073, Consequences (commit `9214426`, #85).

### 2026-10-07 · Clearing a pirate outpost

| Quantity | Value |
|---|---|
| A first fleet against a camp (one Defence Platform and 3 Small Mass Driver ships) | a first fleet of four Pickets (Small Lance ships) clears a camp and loses one; Swarms (Small Mass Driver ships) need eight, since a Mass Driver does a Defence Platform's armor little harm |
| Swarm (Small Mass Driver): camp; stronghold | 6 lose; 8 clear it, 3 lost. Stronghold: 10 lose |
| Picket (Small Lance): camp; stronghold | 3 lose; 4 clear it, 1 lost; 6, none lost. Stronghold: 6 lose; 8 clear it, 2 lost; 10, none lost |
| Brawler (Medium Mass Driver): camp; stronghold | 3 lose; 4 clear it, 2 lost. Stronghold: 8 lose; 10 clear it, 5 lost |
| Lancer (Medium Lance): camp; stronghold | 3 clear it, none lost. Stronghold: 3 lose; 4 clear it, 3 lost |

- **Build:** Phase 4 at milestone 30, before retreat existed, headless in the Linux container on the repository map
- **Seeds:** seed 3
- **Method:** Blue's ships of one design attack-move on an outpost's node from 850 m, sent again every 5 seconds, for up to 4 minutes; `PirateTests` keeps the Picket result, with its ships set never to retreat.
- **Source:** the Phase 4 plan, task 30.1 (commit `9214426`, #85); ADR-073, Consequences (commit `9214426`, #85).
- **Note:** The owner kept the numbers and reworded the design's first fleet to fit the Pickets (2026-10-07).

### 2026-10-07 · Milestone 31: derelicts

| Quantity | Value |
|---|---|
| Matches after milestone 31 (12 derelicts a map, a wreck for each outpost cleared; the AI does not salvage) | 37 end, 36 by domination and 1 by production, at a median of 1:32:57; the same three are level at three hours; each player wins about half |
| Derelicts left at the end of the median match (after milestone 31) | 14: the 12 placed and the wrecks of the outposts the fleets cleared on their way; nothing salvaged |
| Match length, median of those that end (shortest-longest) | 1:32:57 (43:27-2:47:15) |
| Endings; won by player 1 | 36 domination, 1 production, 3 not ended at 3 hours; 21 |
| Warships a side loses in a match, median | 52.5 |
| Warships a side builds in a match, median | 69.5 |
| Most warships a side has at once, median (largest) | 21 (50) |
| Share of the match a side's fleet has no room for one more ship, median | 61% |
| Ore a side holds at minute 20, median | 2,474 |

- **Build:** After milestone 31: 12 derelicts a map and a wreck for each outpost cleared, and an AI that does not salvage, in the Phase 4 harness
- **Seeds:** 1 to 40; the Normal AI in both seats
- **Method:** AI against AI, scored by the harness's counts and `Tools/MatchLog.py`.
- **Source:** the Phase 4 design, §2 (commits `c0ec4e6` (#86), `217ba80` (#87) and `9b7dc6b` (#87)).

### 2026-10-07 · Milestone 32: retreat and repair

| Quantity | Value |
|---|---|
| Match length, median of those that end (shortest-longest) | 1:35:08 (34:25-2:46:42) |
| Endings; won by player 1 | 35 domination, 1 production, 4 not ended at 3 hours; 16 |
| Warships a side loses in a match, median | 20.5 (well under half as many as after milestone 31) |
| Warships a side builds in a match, median | 35.5 |
| Most warships a side has at once, median (largest) | 16 (43) |
| Share of the match a side's fleet has no room for one more ship, median | 64% |
| Ore a side holds at minute 20, median | 4,636 |
| Ships a side turns for home, median; of them repaired whole | 55.5; 40% (six in ten turned for home are lost on the way, or the match ends first) |
| Retreating ships kept in their attack group (a candidate for task 32.1, dropped) | 17 of the 40 matches were level at three hours; the owner kept a retreat counting toward the fall-back (2026-10-07) |
| A retreating Constructor taken as idle | it was given builds the AI then dropped, which held its Ore and production back; with that fixed, the median match is 15 minutes longer and fleets are rebuilt |

- **Build:** After milestone 32: every ship retreats at a quarter to a Repair Bay, the Command Station or a Shipyard, and the AI builds no Repair Bay; matches up to three hours, in the Phase 4 harness
- **Seeds:** 1 to 40; the Normal AI in both seats
- **Method:** AI against AI, scored by the harness's counts and `Tools/MatchLog.py`.
- **Source:** the Phase 4 design, §2 (commits `217ba80` and `9b7dc6b`, #87); the Phase 4 plan, task 32.1 (commit `679fd3e`, #87); ADR-075, Consequences (commit `217ba80`, #87).
- **Note:** The owner kept a retreat counting toward the fall-back (2026-10-07).

### 2026-10-07 · Milestone 33: the AI plays Phase 4 (U1 to U5)

| Quantity | Value |
|---|---|
| Match length, median (shortest-longest) | 1:39:42 (52:00-2:30:18) |
| Endings; won by player 1 | 40 domination; 22 |
| The players' first shot at each other, median (earliest-latest); matches with it between minute 8 and 20 | 11:38 (10:20-21:11); 39 of 40 |
| Derelicts a side salvages before the players' first shot, median; in the match | 3; 8 (in all 40 matches both sides have salvaged 2 or more first) |
| Pirate outposts a side fights before the players' first shot, median; detachments it sends in the match | 2; 4 (the first detachment at a median of 8:48) |
| Most warships a side has at once, median (largest); sides over 40 | 21 (44); 1 of 80 |
| Warships a side has at minute 20, median; sides with 20 or more | 20; 42 of 80 (U2 missed at minute 20) |
| Warships a side builds in a match, median; loses | 41.5; 23.5 |
| Ore a side holds at minute 20, median | 8,159 |
| Ships a side turns for home, median; of them repaired whole | 75; 55% (against 40% before the Repair Bays) |
| Warships a side turns for home, each counted once, median; repaired; fight again | 33.5; 19.5; 17 (45% fight again, where U3 asks half) |
| Engagements before minute 40, median, in a median of sectors | 7, in 4.5 |
| Pirate structures and ships left at the end, median; derelicts | 4 of 10, 8 of 24; 0 |
| U1 without the scout exploring its own half (milestone 33's first version) | the players' first shot came at a median of 6:55, before any salvage |
| Fleet at its cap after milestone 33 | the median side's fleet is full for 69% of the match (the salvaged Ore fills the 50-point cap sooner) |
| U3 spread over the sides after milestone 33 | 79 of the 80 sides send 10 or more warships back, and 28 see half of them fight again |
| U4 spread over the matches after milestone 33 | 33 of the 40 matches meet five engagements in three sectors before minute 40 |
| A second version set aside | it explored its own half but skipped the guarded sectors, and in `PlaysPhaseFoursOpening`'s match sent no detachment in half an hour; not measured over the 40 seeds |

- **Build:** After milestone 33: the AI salvages, clears pirates by detachment, builds a Repair Bay and scouts its own half for ten minutes; matches up to three hours, in the Phase 4 harness
- **Seeds:** 1 to 40; the Normal AI in both seats
- **Method:** AI against AI, scored by the harness's counts and `Tools/MatchLog.py`.
- **Source:** the Phase 4 design, §2 (commits `40db13a` and `131442d`, #90); ADR-076, Consequences (commit `40db13a`, #90).
- **Note:** U2 at minute 20 and U5's single ending were recorded rather than tuned (owner, 2026-10-07 and 2026-10-08); U3's share was accepted as measured (owner, 2026-10-07).

### 2026-10-07 · The match log against the harness

| Quantity | Value |
|---|---|
| Four AI-against-AI matches logged through `MatchLog` | the same derelicts salvaged, before the players' first shot and in all, the same pirate outposts fought before it, and the same warships fighting again, side for side, as the harness counted from the server |

- **Build:** Phase 4 at milestone 34, in the Linux container
- **Seeds:** not stated
- **Method:** The match log's records compared with the harness's counts; U3 counts each warship once, as the log does, where the harness had counted a retreat each time a ship went.
- **Source:** ADR-038, Consequences (commit `0b62014`, #91).

### 2026-10-08 · U6: the balance check at Phase 4's budgets

| Quantity | Value |
|---|---|
| U6: the balance check of MVP §3 at Phase 4's tripled budgets | passes all four criteria at budgets of 6,000 to 36,000 Ore; the Missile Rack is among the designs worth building at most budgets from 9,000 Ore |

- **Build:** Phase 4 after milestone 34, in the Linux container, built by clang at `-O2`
- **Seeds:** each battle's seed hashes its budget
- **Method:** `BalanceCheckTests.TheFullCheck` with `OUTPOST_BALANCE_FULL` set. A single factor on every component's cost moves the budgets with it, so each battle buys the same ships.
- **Source:** the Phase 4 design, §2 (commit `54a15e2`, #91).
- **Note:** The run's own date is not stated; it was recorded by 2026-10-08.

### 2026-10-08 · W3: the deputy keeps an empire running (milestone 37)

| Quantity | Value |
|---|---|
| Idle with Ore to spend, minutes 10 to 20 | deputy's Shipyards 0.8-1.4%, its Lab 0.2%; the AI's Shipyards 0.0-5.4%, its Lab 0.0-0.2% |
| The deputy's first version, which queued only once the Ore and the cap allowed | Shipyards idle 50.6% of the time |

- **Build:** Milestone 37: `GameLogicTests` in the Linux container against stand-ins for the Windows headers, the test framework and MsQuic; the compiler is not stated
- **Seeds:** 1 to 4; the committed test runs seed 2
- **Method:** `DeputyPlayTests.KeepsAnEmpireRunningAsTheAiKeepsItsOwn`: two Normal AIs for 10 minutes, then Blue's seat its deputy's for 10 more. "Idle with Ore to spend" is a finished Shipyard with an empty queue while its player could pay for a ship the cap had room for, or a Lab with nothing to research while a topic it could take was paid for.
- **Source:** the Phase 5 plan, milestone 37's As built (commit `70f480c`, #95); ADR-079, Consequences (commit `12a86e6`, #95).

### 2026-10-08 · W3 in CI (milestone 37)

| Quantity | Value |
|---|---|
| W3 in CI: deputy's Shipyards idle with Ore to spend, against the AI's | deputy's Shipyards 0.8%, the AI's 0.0% ('the same there as in the container') |

- **Build:** CI, Debug|x64 on Windows (MSVC)
- **Seeds:** the test's seed, 2
- **Method:** `DeputyPlayTests.KeepsAnEmpireRunningAsTheAiKeepsItsOwn` in CI's run of `GameLogicTests`.
- **Source:** the Phase 5 plan, milestone 37's As built (commit `d908a03`, #95).

### 2026-10-08 · An hour of world (task 39.2)

| Quantity | Value |
|---|---|
| Banks at hour 1 | the two seats banked 122,715 and 63,703 Ore, each fleet at its cap of 50 |
| Recovery | killed after ticks 19,465, 40,664 and 52,866, it came back at 19,462, 40,404 and 52,800, and ended equal |

- **Build:** Milestone 39: `OutpostServer --world-run` built by clang 18 at `-O2` in the Linux container
- **Seeds:** seed 1; the Normal AI in both seats
- **Method:** `OutpostServer --world-run` for an hour, killed three times, against the same world run straight through; the world log's hourly seat records.
- **Source:** ADR-082, Consequences (commit `919e09c`, #98).

### 2026-10-08 · A day of world (task 39.2)

| Quantity | Value |
|---|---|
| Research, fleets and the bank over the day | both seats researched every topic by hour 1 and kept their fleets at the cap all day; Blue's bank reached 513,829 Ore at hour 23, on an income that fell from 40 Ore a second to 10 |
| Nodes and battles | Blue held 9 nodes throughout; the straight world's log holds 405 battles |
| Losses and restarts of a seat | Red lost its base 11 times and restarted 11 times, each on the hour, never waiting; seven of those bases fell 12-14.5 s after their restart |
| The restarts in more detail | Red lost its base first at hour 7; of the bases that did not fall at once, three stood 27, 27 and 74 minutes, and the last to the end of the day |
| Recovery | killed after ticks 148,264, 157,123, 189,738, 591,824, 752,200, 754,665, 862,814, 989,411, 1,455,187 and 1,613,540, it came back at 148,242, 157,104, 189,726, 591,600, 751,200, 754,142, 862,800, 988,800, 1,454,400 and 1,612,800, and ended equal: back as far as 1,000 ticks, a save interval |

- **Build:** Milestone 39: `OutpostServer --world-run` built by clang 18 at `-O2` in the Linux container
- **Seeds:** seed 1; the Normal AI in both seats; 24 hours; the default 10 kills
- **Method:** `OutpostServer --world-run`, read from the world log's seat, loss, restart and save records, against the same world run straight through.
- **Source:** the Phase 5 plan, milestone 39's As built (commits `a234270` and `f62d3b6`, #98); ADR-082, Consequences (commit `a234270`, #98); ADR-084, Consequences (commit `a234270`, #98).
- **Note:** Server code changed after this run: the first interface milestone's remembered structures' last-seen ticks were merged into its branch on 2026-10-09, and snapshot fields were added; whether play differs is not known. Whether a start held by enemy warships is free is gate H11 of the Phase 5 design.

### 2026-10-08 · W3 again, with milestone 39's AI

| Quantity | Value |
|---|---|
| W3 again with milestone 39's AI: idle with Ore to spend, minutes 10 to 20 | deputy's Shipyards 0.8%, its Lab 0.2%; the AI's 0.0% and 0.2%, 'as at milestone 37' |

- **Build:** Milestone 39, in the Linux container; the compiler is not stated
- **Seeds:** not stated; the committed test runs seed 2
- **Method:** `DeputyPlayTests.KeepsAnEmpireRunningAsTheAiKeepsItsOwn`.
- **Source:** the Phase 5 plan, milestone 39's As built (commit `a234270`, #98).

### 2026-10-08 · The battle matchups' baseline (task 39.3)

| Quantity | Value |
|---|---|
| Mirror: side 1 won; side 2 won; drawn; median length | 25; 15; 0; 21 s |
| Swarm against line | 0; 40; 0; 31 s |
| Flak against swarm | 40; 0; 0; 9 s |
| Heavy against line | 19; 21; 0; 41 s |
| Missiles against line | 0; 40; 0; 22 s |
| Over seeds 1 to 200 | the mirror 114 to 86 for side 1, 57%, a two-sided binomial p of about 0.06; matchups 2, 3 and 5 stayed 200 to 0; the heavy line went 90 to 109 with one draw |

- **Build:** Milestone 39: `OutpostServer --matchups` built by clang 18 at `-O2` in the Linux container
- **Seeds:** 1 to 40 for each matchup, side 1 first; then 1 to 200
- **Method:** `OutpostServer --matchups` over `Matchups.json`'s five fleets: no bases, no fog, no retreat, and the AI's battle behavior on both sides.
- **Source:** the Phase 5 plan (commit `a234270`, #98); ADR-083, Consequences (commit `b6d343b`, #98).

## Performance

Ticks, frames, order-to-response delay, saves, snapshot and file sizes, GPU time, and how long the checks take to run.

### 2026-09-30 · Q5 on the second machine

| Quantity | Value |
|---|---|
| From the injected click to the first frame that shows the response | mean 43 ms, median 44 ms, 95th percentile 66 ms, worst 67 ms; target ≤ 150 ms met |
| From when the game read the click | mean 38 ms, worst 62 ms |

- **Build:** The MVP after milestone 2 (task 2.7); Release|x64 on the second machine
- **Seeds:** not stated
- **Method:** `--measure`. A script injected 30 move orders to ships at rest, spread by a random 150-400 ms pause. Start: the script's `QueryPerformanceCounter` reading just before it injects the click, so the click's wait in the queue is included; or, for the second row, when the game reads the click, on `std::chrono::steady_clock`. End: after `Present` returns for the first frame in which an ordered ship's center or nose has moved at least one pixel at the default 500 m view. Scanout and the display's latency are not included. The owner did not repeat the scripted runs by hand.
- **Source:** the MVP design, §3 (commit `0d90731`).

### 2026-09-30 · Ticks under load on the second machine

| Quantity | Value |
|---|---|
| Steady ticks, both fleets kept moving (`--measure --load`) | mean 0.18 ms, 99th percentile 0.37 ms, under 1 ms on every tick but the order ticks; 1,181 ticks over 60 s |
| The ticks that order both 100-ship fleets at once, every 10 s | 3.0, 3.7, 2.7, 5.0, 6.8 and 8.3 ms, rising as the fleets spread; 5 ms missed on 2 of 6 |

- **Build:** The MVP after milestone 2 (task 2.7), before group pathing and before the load's structures blocked movement; Release|x64 on the second machine
- **Seeds:** not stated
- **Method:** The server times each tick on its own clock: its commands, its simulation step and every snapshot (`Server::TakeTickDurations`). `--load` places 200 ships and 40 structures and orders both players' ships across the map every 200 ticks.
- **Source:** the MVP design, §3 (commit `0d90731`).

### 2026-10-01 · Q5 on the development machine

| Quantity | Value |
|---|---|
| From the injected click to the first frame that shows the response | mean 48 ms, median 49 ms, 95th percentile 66 ms, worst 70 ms; target ≤ 150 ms met |
| From when the game read the click | mean 38 ms, worst 55 ms |
| The server boundary | `#include "GameLogic.h"` added to `GameApp` and to `Opponent` fails to compile in both with C1083 |

- **Build:** The MVP of 2026-10-01; Release|ARM64 on the development machine, a script launching the game, moving the camera and injecting the clicks
- **Seeds:** not stated
- **Method:** `--measure`. A script injected 30 move orders to ships at rest, spread by a random 150-400 ms pause. Start: the script's `QueryPerformanceCounter` reading just before it injects the click, so the click's wait in the queue is included; or, for the second row, when the game reads the click, on `std::chrono::steady_clock`. End: after `Present` returns for the first frame in which an ordered ship's center or nose has moved at least one pixel at the default 500 m view. Scanout and the display's latency are not included.
- **Source:** the MVP design, §3 (commit `6861fcc`).

### 2026-10-01 · Ticks under load on the development machine

| Quantity | Value |
|---|---|
| Ticks between orders (`--measure --load`) | mean 0.48 ms, 99th percentile 0.90 ms, one tick at 8.6 ms; 1,464 ticks |
| The ticks that order both 100-ship fleets | 20.7 ms on the first, then 2.1, 5.7, 8.2, 5.7, 8.7, 6.0 and 6.9 ms; 5 ms missed on 7 of 8 |

- **Build:** The MVP of 2026-10-01, with group pathing, and the load's structures blocking movement since milestone 4; Release|ARM64 on the development machine
- **Seeds:** not stated
- **Method:** The server times each tick on its own clock: its commands, its simulation step and every snapshot (`Server::TakeTickDurations`). `--load` places 200 ships and 40 structures and orders both players' ships across the map every 200 ticks. Summarized by `Tools/FrameTimes.py`.
- **Source:** the MVP design, §3 (commit `6861fcc`).

### 2026-10-01 · Q4: frames and ticks in combat on the development machine

| Quantity | Value |
|---|---|
| Combat at 1920×1080 with the HUD, zoomed out to the 1,600 m view over the fight, 5,654 frames | presented 59.9 frames a second; CPU work mean 2.57 ms, 99th percentile 4.64, worst 13.06; GPU work mean 2.25, 99th percentile 2.43, worst 3.52; 99% of frames ≤ 16.7 ms met |
| The same at the panel's 2880×1920, 2,778 frames | presented 30.0 frames a second (every frame missed a vsync); CPU work mean 3.43 ms, 99th percentile 5.31; GPU work mean 2.94, 99th percentile 3.16; not Q4's resolution |
| GPU timestamps against frames presented at 2880×1920 | logged work about 3 ms of CPU and 3 ms of GPU a frame, yet every frame took two vsyncs; the same view at 1920×1080 held 60 frames a second, and so did every view at 2880×1920 with less of the fight on screen |
| The ticks of the two runs, 1,930 and 1,928 | mean 0.88 ms; 99th percentile 1.57 and 1.74 ms; over 5 ms on 4 and 5 ticks, the first tick included; worst 9.4 and 9.9 ms; target ≤ 5 ms missed on 4-5 ticks a run |

- **Build:** The MVP of 2026-10-01, with the server's ticks on the frame thread; Release|ARM64 on the development machine, the display set to 1920×1080 at 60 Hz for the first run only
- **Seeds:** not applicable: the `--stress` scene
- **Method:** `--measure --stress`: each player keeps 100 ships of its four starting designs and 20 structures fighting, both fleets attack-moving on each other's rally and lost ships coming back. CPU work runs from when the swap chain lets the frame start to the return from `Present`, the server's ticks included; GPU work is between two timestamps at the start and end of the frame's command list. `Tools/FrameTimes.py` summarizes the log, leaving out the first 120 frames; frames presented a second are the frames logged over the run's length in ticks.
- **Source:** the MVP design, §3 (commit `6861fcc`).
- **Note:** The owner decided on 2026-10-01 to record Q4 with the slow ticks and fix them after the MVP. On this machine the frames presented, not the GPU work, say whether a frame was on time.

### 2026-10-01 · A group's order paths once for the group

| Quantity | Value |
|---|---|
| The 200-ship order tick, as the MVP design gives it | about 3.4 ms before, about 1 ms after |
| One search per ship | a mean of 3.1-4.1 ms per order tick over three runs |
| The group's route, with the lazy search | 0.8-1.3 ms, about a quarter |
| Ships that still search for themselves | about one in ten, the load's fleets being scattered over half the map |
| Arrival | in both ways all 200 ships arrived within 60 s of the last order, and no two overlapped |

- **Build:** The MVP at milestone 3, before the load's structures blocked movement; the Linux container, clang 18 at `-O2`
- **Seeds:** not stated
- **Method:** Task 2.7's load, both 100-ship fleets ordered across the map in one tick, 20 times; read as a ratio, since the container is not the development machine.
- **Source:** the MVP design, §3 (commit `6861fcc`); ADR-010, Consequences (commit `670c25b`, #36).

### 2026-10-01 · Building the path graphs

| Quantity | Value |
|---|---|
| One radius, on the map with the two Command Stations | 0.25-0.4 ms |
| A graph rebuilt when a structure appears or dies | 1.4-2.8 ms with only tangent edges kept, rather than 14-17 ms before |
| The same, as ADR-010 gives it | on the stress scene's 53 obstacles, 14-17 ms a graph when every pair of corners was tested, which put a 34 ms tick into the scene each time a structure died; 1.4-2.8 ms keeping only the lines that touch both polygons and eight corners round small circles, with the same paths |

- **Build:** The MVP at milestone 4 (#38); the Linux container, clang 18 at `-O2`
- **Seeds:** not stated
- **Method:** `Pathfinder::Prepare` for each radius.
- **Source:** the MVP plan, task 4.2 (commit `393636c`, #38); ADR-010, Consequences (commit `822b4b2`, #38).

### 2026-10-01 · Stress-scene ticks in `StressLoadTests`

| Quantity | Value |
|---|---|
| Stress-scene ticks in StressLoadTests (60 simulated seconds) | median 0.11 ms, 99th percentile 0.45 ms, worst 1.7 ms |

- **Build:** The MVP at milestone 3 (#36), in the Linux container; the compiler and configuration are not stated
- **Seeds:** not applicable
- **Method:** `StressLoadTests` runs the scene for 60 simulated seconds, each side staying above 80 ships, with more than 1,000 shots and more than 20 destroyed.
- **Source:** the MVP plan, task 3.7 (commit `6861fcc`).

### 2026-10-01 · The research catalog in every snapshot

| Quantity | Value |
|---|---|
| What the catalog adds to each snapshot | 1,624 bytes of records and 357 bytes of text |
| The stress test's median tick | 0.135-0.170 ms over four runs, against 0.142 ms before, within the noise between runs |

- **Build:** The MVP at milestone 5 (#39): libstdc++ on x64 Linux, in the Linux container
- **Seeds:** not applicable
- **Method:** The snapshot's size measured; task 3.7's stress test.
- **Source:** ADR-017, Consequences (commit `ad41574`, #39).

### 2026-10-01 · The placeholder meshes' indices

| Quantity | Value |
|---|---|
| The largest of the 29 placeholder models | 3,972 vertices |
| What 16-bit indices would have saved | 135 KB across all 29 |

- **Build:** The placeholder meshes of 2026-10-01
- **Seeds:** not applicable
- **Method:** Counted from the baked meshes.
- **Source:** ADR-018, Decision (commit `cedc905`, #41).

### 2026-10-01 · How long the Q2 check takes

| Quantity | Value |
|---|---|
| The full check at milestone 3, before task B.1 | about ten minutes on four threads |
| The full check after task B.1 | 278 s on four cores (the same section also says 'about five minutes on four cores') |
| The check of record | 1 min 51 s on ten cores |
| `python Tools/BattleModel.py` | 80 s |

- **Build:** The MVP: the Linux container for the first two rows; Release|ARM64 on the development machine for the check of record; where the model ran is not stated
- **Seeds:** not stated
- **Method:** Wall time, with `OUTPOST_Q2_FULL` set; CI never runs the full check.
- **Source:** the MVP plan, task 3.4 (commit `3e915e3`, #43); the MVP design, §12 (commits `f28eb1d` (#43) and `6861fcc`).
- **Note:** The MVP design gives the full check after B.1 both as 278 s and as "about five minutes on four cores"; both are as recorded.

### 2026-10-02 · What fog of war costs a tick

| Quantity | Value |
|---|---|
| With fog | a median tick of 0.19-0.20 ms, and 0.29-0.47 ms at the 99th percentile |
| Without | a median of 0.16-0.18 ms, and 0.24-0.44 ms |
| The difference | about 0.03 ms a tick |
| What the runs did | both fired the same 2,020 shots and destroyed the same 340 ships and structures |

- **Build:** The MVP with fog of war, in the Linux container, g++ 13 at `-O2`
- **Seeds:** not applicable: the stress scene
- **Method:** `StressLoadTests`, 1,200 ticks of the stress scene three times with fog and three times without.
- **Source:** ADR-024, Consequences (commit `f10f836`, #48).
- **Note:** The development machine did not run it.

### 2026-10-02 · The text atlas

| Quantity | Value |
|---|---|
| The atlas for two fonts | 512×128 texels |

- **Build:** The code of 2026-10-02
- **Seeds:** not applicable
- **Method:** Measured in a test, which the ADR does not name.
- **Source:** ADR-030, Consequences (commit `f0c5071`, #49).

### 2026-10-03 · Where an order tick's time goes, and the fix (task 8.2)

| Quantity | Value |
|---|---|
| Where the time goes | the 20.7 ms first order tick was building the four path graphs, which match setup dropped by placing the bases after building them; the later order ticks were the ships' paths, about 60 full searches each, mostly line tests against every obstacle; parting ships cost 0.07-0.2 ms a tick; the container runs the MVP's order ticks in about a third of the development machine's time, on both the first and later ticks |
| The first order tick | 6.5-10 ms, of which graph builds are 5.2-7.8 ms |
| Later order ticks | 1.5-2.6 ms, the ships' paths: about one ship in three still makes a full search, some 60 a tick, and an order tick makes 6,000-11,000 line tests against 54 obstacles |
| By instructions | line tests 44% (a ship trying the routes of the ships before it 19%, graph builds 14%, searches 10%), the searches' priority queue 15%, and a graph build's own pass over pairs of corners 17% |
| The container against the development machine | the MVP's figures are about 3.3 times these, on both the first and the later order ticks |
| The worst order tick, before and after the fix | 2.58 ms before, 1.24 ms after |
| The first order tick after the fix | 0.5-1.0 ms, against 6.5-10 ms |
| Instructions an order's planning takes, over seven pairs of orders | 51.9 million against 64.7 million: the split costs about 7% more in all, sorting in batches saves 10%, and passing over hopeless routes 13% |
| Tried and not kept | a uniform grid of obstacles for every line test cost 6% more than testing every obstacle's box; the obstacles' grown radii kept per clearance, 7% more |

- **Build:** Phase 1 at task 8.2: the Linux container, clang 18 at `-O2`
- **Seeds:** not applicable: task 2.7's load
- **Method:** Task 2.7's load (200 ships and 40 structures, both fleets of 100 ordered across the map every 200 ticks, 12 orders over 2,400 ticks), timed by task 8.1's parts of a tick and counted with callgrind; each tick's least time over five runs, since single runs in the container spike.
- **Source:** the Phase 1 design, §10 (commit `9782b1c`, #49); ADR-032, Context and Consequences (commit `9782b1c`, #49).
- **Note:** The owner's `--measure --load` run on the development machine, which was to record the fix there, was not made.

### 2026-10-03 · A whole graph build on the 2.3 km map

| Quantity | Value |
|---|---|
| In the container | about 1.3 ms |
| On the development machine | about 4 ms |

- **Build:** Phase 1, after task 8.2
- **Seeds:** not applicable
- **Method:** Not stated.
- **Source:** the Phase 2 design, §7 (commit `15a8670`); ADR-032, Consequences (commit `9782b1c`, #49).
- **Note:** Neither document says whether the development machine's figure was measured there or scaled from the container by the ratio of the entry above.

### 2026-10-03 · Ships turn in arcs: the stress scene's ticks (task 7.2)

| Quantity | Value |
|---|---|
| Median tick | 0.24 ms |
| Slowest tick, over three runs | 1.6-2.4 ms, against 1.5 ms before |

- **Build:** Phase 1 at task 7.2, in the Linux container
- **Seeds:** not applicable: the stress scene
- **Method:** The stress scene's ticks.
- **Source:** ADR-039, Consequences (commit `5a7d99d`, #49).

### 2026-10-03 · Ten AI matches headlessly (task 13.1)

| Quantity | Value |
|---|---|
| Ten matches | 9 seconds |

- **Build:** Phase 1 at task 13.1: `AiMatches.cpp` built by clang 18 at `-O2` in the Linux container against the real server and data, on four threads
- **Seeds:** 1 to 10
- **Method:** Wall time of `--ai-matches`.
- **Source:** ADR-038, Consequences (commit `882c112`, #49).

### 2026-10-03 · Groups keep their lanes: the order ticks (task 9.7)

| Quantity | Value |
|---|---|
| The worst order tick | 1.26 ms, against 1.02 ms |
| The mean order tick | 0.85 ms, against 0.65 ms |
| The ships' paths in an order tick, on average | 0.74 ms, against 0.50 ms: all of the difference |

- **Build:** Phase 1 at task 9.7, in the Linux container
- **Seeds:** not applicable: ADR-032's measurement load
- **Method:** As ADR-032 measured: each order tick's least time over seven runs, 24 order ticks over 2,400 ticks.
- **Source:** ADR-047, Consequences (commit `ced8fc8`, #52).

### 2026-10-03 · How long the balance check takes, Phase 1

| Quantity | Value |
|---|---|
| The full check before and after task 8.2 | 6 minutes each, with four threads |
| The check with arcs (task 7.2) | 88 minutes on four threads that it shared with other runs |
| Phase 1's check of record, every tier | 69 minutes on four threads; 60 battles a pairing, up to 480 when uncertain; 3,604 counter checks under one-number changes of ±5% |
| `AiPlayerTests.ReachesTierThree` in CI | about four minutes in Debug; the longest test in CI, kept there (owner, 2026-10-03) |

- **Build:** Phase 1, in the Linux container; the last row is CI's Debug|x64
- **Seeds:** not stated
- **Method:** Wall time; the full check runs with `OUTPOST_BALANCE_FULL` set.
- **Source:** the Phase 1 design, §7 (commit `ad27324`, #49); the Phase 1 plan, task 10.4 (commit `ec5c0c7`, #49); ADR-032, Consequences (commit `9782b1c`, #49); ADR-039, Consequences (commit `8c6acd2`, #49).

### 2026-10-03 · The obstacle grid and extended graphs, in instructions

| Quantity | Value |
|---|---|
| Every line through a grid of 200 m cells, seed 1's first 12,000 ticks | 8.40 G instructions against 7.41 G before, 13% more; every line longer than 3 cells taken to every obstacle, with 100 m cells, was the cheapest setting tried |
| Seed 1's match, first 12,000 ticks: before / grid / grid and extended graphs | ticks 7.41 G / 7.08 G / 5.00 G; graph builds 2.71 G / 2.43 G / 0.36 G |
| The measurement load | ticks 3.83 G throughout; graph builds 170 M / 108 M / 109 M |
| The stress scene | ticks 5.00 G throughout; graph builds 173 M / 110 M / 112 M |
| In time, seeds 1 to 4 played for 72,000 ticks each | the slowest tick from 52-71 ms to 15-34 ms, and ticks over 5 ms from 413-558 a match to 262-399 |

- **Build:** The owner's startup and frame performance work of 2026-10-03 (#53), on the 5 km map: the Linux container, clang 18 at `-O2`
- **Seeds:** seed 1; seeds 1 to 4 for the times
- **Method:** callgrind, and the tick's times.
- **Source:** ADR-054, Consequences (commits `c381f32` and `2a0042b`, #53).

### 2026-10-04 · A snapshot's size, and what encoding it costs

| Quantity | Value |
|---|---|
| Player 1's snapshot at the start of a match (49 entities) | 8,065 bytes |
| Under task 2.7's measurement load (173 entities in its fogged view) | 18,785 bytes |
| Encoding and decoding it under the load | 76 µs together, averaged over 200 runs |

- **Build:** The code of 2026-10-04 on the 5 km map: g++ 13 on Linux, the libraries built `-O1`
- **Seeds:** not applicable
- **Method:** The 20th tick's snapshot encoded from the repository's tuning and map. Neither has been measured under MSVC.
- **Source:** ADR-060, Context and Consequences (commit `1ec4476`, #62).
- **Note:** The horizon plan restates the sizes as 8-19 KB, and works out 1.3-3 Mbit/s for each player at 20 Hz from them.

### 2026-10-04 · What an AI-against-AI match costs (self-play's basis)

| Quantity | Value |
|---|---|
| Processor time of the AI against itself | 60 s of processor time for 2 hours 56 minutes of play: about 15 s a match of 45 minutes, and per minute of play twice what Phase 1's smaller fleets cost |

- **Build:** Phase 3's map and AI: the Linux container, clang 18 at `-O2` against a stand-in for the Windows headers
- **Seeds:** 1 to 4, the AI against itself
- **Method:** Processor time of `--ai-matches` over the seeds.
- **Source:** the self-play network blueprint, §12 (commit `0f786f2`, #64); ADR-063, Consequences (commit `c2ecf4f`, #64).

### 2026-10-05 · An AI-against-AI match in SP1.4's runs

| Quantity | Value |
|---|---|
| CPU time of one AI-against-AI match, and a tick's mean cost, in SP1.4's runs | 11 to 29 CPU seconds a match, about 0.42 ms a tick |

- **Build:** Phase 3's map and AI: the Linux container, clang 18 against a stand-in for the Windows headers
- **Seeds:** 1 to 4, both players at each `secondSlotTier` from 1 to 4
- **Method:** Processor time of `--ai-matches`.
- **Source:** the self-play plan, SP1.4's As built (commit `d9887e3`, #80).

### 2026-10-07 · The container's driver: how long 40 matches take

| Quantity | Value |
|---|---|
| 40 matches before and at milestone 27 | 4 minutes for main's 40 matches (before milestone 27) and 2 minutes for milestone 27's, on four threads |
| 40 matches on the 10 km map (task 28.4) | 16 minutes of processor time, 4:45 on four threads |

- **Build:** Phase 4, in the Phase 4 harness
- **Seeds:** 1 to 40
- **Method:** Wall time, and processor time where given.
- **Source:** the Phase 4 plan, tasks 27.5 and 28.4 (commits `273acf1` (#81) and `cfc275f` (#82)).

### 2026-10-07 · Path graphs on the 10 km map (task 28.0)

| Quantity | Value |
|---|---|
| The 99th percentile tick on a first 10 km map, before the change | about 20 ms, most of it whole builds of the path graphs a destroyed structure dropped |
| A whole build late in a match | with some 180 obstacles and 2,000 corners a graph, 22-30 ms |
| Per match: built whole on a removal / reduced | the 99th percentile tick 19.2-22.3 ms / 1.9-2.0 ms; the slowest tick 88-140 ms / 10-16 ms; ticks over 5 ms 1,607-1,690 / 55-91 |
| The same, as the Phase 4 plan gives it | 99th percentile tick from 19-22 ms to about 2 ms; slowest tick from 88-140 ms to 10-16 ms; every match plays exactly as before |
| A graph alone, over 120 obstacles and 60 structures placed at random, about 2,060 corners | built whole in 22-23 ms, reduced by one structure in 1.4-1.8 ms, extended by one in 0.9-1.0 ms; a first version that paired every corner near the removed obstacle took 34-51 ms |
| The same, as the Phase 4 plan gives it | under 2 ms for a graph of about 2,060 corners, against 22 ms to build it whole |

- **Build:** Phase 4 at milestone 28, on the 10 km map with its node caps, in the Phase 4 harness
- **Seeds:** 1 to 4, two Normal AIs for up to 120 minutes each
- **Method:** The same matches played before and after the change; every match played exactly as before, with the same ending, winner, length, fleets, structures built and Ore at minutes 10, 20 and 30.
- **Source:** the Phase 4 plan, task 28.0 (commit `f36deff`, #82); ADR-054, Context and Consequences (commit `f36deff`, #82).

### 2026-10-07 · Keeping ships out of obstacles: the push

| Quantity | Value |
|---|---|
| The push's part of the tick, over four AI matches of 20 minutes | 4.8-5.2 s in all, against 13.1-14.2 s: about 0.35 ms a tick of some 21 |
| Cells of entities for targets and of observers for vision, tried with it | about 24 ms a tick against 21, so not kept |

- **Build:** Phase 4: a server built unoptimized with checked iterators in the Linux container, standing in for a Debug build
- **Seeds:** four matches; seeds 1 to 40, built optimized, give the same outcomes line for line
- **Method:** The part of the tick timed alone, since the whole tick's mean moved by more between two runs of the same build than this saves.
- **Source:** ADR-054, Consequences (commit `14bc460`, #84).

### 2026-10-07 · The client's fog at 10 km (task 29.2)

| Quantity | Value |
|---|---|
| The whole grid worked out every snapshot, before the change | about 40 ms a snapshot unoptimized, 2 ms optimized, for the whole 250,000-cell grid; every changed fog texture converted and copied whole |
| Worked out only where something changed, on 120 entities, half of them moving | 2.5 ms unoptimized and 0.4 ms optimized, against 40 ms and 1.1 ms for the whole update |

- **Build:** Phase 4 at milestone 29, in the Linux container, unoptimized (with checked iterators) and optimized
- **Seeds:** not applicable
- **Method:** Fifteen held sectors and 120 entities, half of them moving a few meters every update, with sectors changing hands; checked against the earlier code cell for cell over 1,200 random updates.
- **Source:** the Phase 4 plan, task 29.2 (commit `496d8cc`, #83); ADR-052, Context and Consequences (commit `496d8cc`, #83).
- **Note:** For the whole update optimized, the Phase 4 plan and ADR-052 give 2 ms in one place and 1.1 ms in another; both are as recorded. The owner, playing a Debug build on the 10 km map, had found the ships stuttering and the controls lagging (2026-10-07).

### 2026-10-07 · Placing the map from the seed (task 29.1)

| Quantity | Value |
|---|---|
| Seeded placement of the map (task 29.1) | about 10 µs per seed; seeds 1 to 2,000 all place the map |

- **Build:** Phase 4 at milestone 29, when only the asteroids were placed, in the Linux container
- **Seeds:** 1 to 2,000
- **Method:** Not stated.
- **Source:** the Phase 4 plan, task 29.1 (commit `114b4bd`, #91).

### 2026-10-07 · Ticks in AI-against-AI matches on the 10 km map, milestone by milestone

| Quantity | Value |
|---|---|
| After milestone 28, the map empty, matches up to four hours | median match's 99th percentile tick 2.5 ms, worst match's 4.2 ms; slowest single tick 20 ms in the median match and 39 ms at worst |
| After milestone 29, seeded placement | median match's 99th percentile tick 2.1 ms, worst match's 4.2 ms; one match's slowest tick 72 ms, the most yet; the median match's slowest 23 ms |
| After milestone 30, pirates, matches up to three hours | median match's 99th percentile tick 1.7 ms, worst match's 2.7 ms |
| After milestone 31, derelicts in every player's vision pass | median match's 99th percentile tick 1.9 ms (against 1.7 ms after milestone 30), worst match's 2.9 ms; every derelict is in each player's vision pass |
| After milestone 32, retreat and repair | median match's 99th percentile tick 1.5 ms, worst match's 2.9 ms; one match's slowest tick 76 ms; the median match's slowest 16 ms |
| After milestone 33, the AI plays Phase 4 | median match's 99th percentile tick 1.4 ms, worst match's 3.3 ms; one match's slowest tick 68 ms; the median match's slowest 14 ms; under U7's 5 ms |

- **Build:** Phase 4, in the Phase 4 harness. The runs after milestones 31 and 32 were on an idle machine; after milestone 33 each match ran alone on one core while the other three ran the balance check
- **Seeds:** 1 to 40; the Normal AI in both seats
- **Method:** The server's tick timings over the 40 matches: the 99th percentile of each match, then the median match and the worst. The AI's own work is not in the figures.
- **Source:** the Phase 4 design, §2 (commits `d96bc37` (#82), `371692c` (#83), `9214426` (#85), `c0ec4e6` (#86), `217ba80` (#87) and `ad75858` (#90)); ADR-074, Consequences (commit `c0ec4e6`, #86); ADR-075, Consequences (commit `217ba80`, #87); ADR-076, Consequences (commit `ad75858`, #90).

### 2026-10-08 · How long U6's balance check takes

| Quantity | Value |
|---|---|
| Run time of the balance check at Phase 4's budgets (U6, BalanceCheckTests.TheFullCheck with OUTPOST_BALANCE_FULL) | 1 hour 43 minutes |

- **Build:** Phase 4 after milestone 34, in the Linux container, built by clang at `-O2`, beside other work
- **Seeds:** not stated
- **Method:** Wall time of `BalanceCheckTests.TheFullCheck` with `OUTPOST_BALANCE_FULL` set.
- **Source:** the Phase 4 plan, task 34.2 (commit `54a15e2`, #91).
- **Note:** The run's own date is not stated; it was recorded by 2026-10-08.

### 2026-10-08 · W2: a late world's save (milestone 35)

| Quantity | Value |
|---|---|
| Its size | 274-293 entities in 75-78 KB |
| Its encoding on the server's thread | 0.36-0.38 ms |

- **Build:** Milestone 35: the Linux container, clang 18 at `-O2` against a stand-in for the Windows headers
- **Seeds:** 1 to 3
- **Method:** `WorldFolderTests.MeasuresALateSave` with `OUTPOST_WORLD_MEASURE` set: a world at minute 60 of two Normal AIs on the 10 km map, averaged over 20 encodings for each seed.
- **Source:** the Phase 5 design, §2 (commit `e68f305`, #92); ADR-077, Consequences (commit `e68f305`, #92).

### 2026-10-08 · W2: two AI empires on the server's thread (milestone 37)

| Quantity | Value |
|---|---|
| Over the three seeds | a tick's 99th percentile 0.25-0.40 ms; the hosted players' 99th percentile 0.08-0.09 ms, their longest at most 3.20 ms |
| Seed 1: the tick's 99th percentile and longest; the hosted part's | 0.35 ms and 7.08 ms; 0.09 ms and 0.61 ms |
| Seed 2 | 0.40 ms and 8.64 ms; 0.09 ms and 0.36 ms |
| Seed 3 | 0.25 ms and 6.08 ms; 0.08 ms and 3.20 ms |

- **Build:** Milestone 37: the Linux container, clang 18 at `-O2` against a stand-in for the Windows headers
- **Seeds:** 1 to 3
- **Method:** `InProcessServerTests.MeasuresTheHostedPlayersTicks` with `OUTPOST_WORLD_MEASURE` set: two `AiEmpire` seats, an hour of world stepped, the 99th percentile of the tick and of its hosted part.
- **Source:** the Phase 5 plan, milestone 37's As built (commit `70f480c`, #95); ADR-079, Consequences (commit `70f480c`, #95).

### 2026-10-08 · An hour of world (task 39.2)

| Quantity | Value |
|---|---|
| An hour of world | 18 s for each of the two worlds |
| Its 64 saves | 82-83 KB, encoded in 0.21-0.50 ms, median 0.32 ms |

- **Build:** Milestone 39: `OutpostServer --world-run` built by clang 18 at `-O2` in the Linux container
- **Seeds:** seed 1; the Normal AI in both seats
- **Method:** Processor time of the run, and the world log's save records.
- **Source:** ADR-082, Consequences (commit `919e09c`, #98).

### 2026-10-08 · A day of world: speed, saves and ticks (milestone 39)

| Quantity | Value |
|---|---|
| A day of world, killed 10 times, against the same world run straight through | 1,728,000 ticks twice in 615 s, ending equal |
| W2: the day's saves | 1,451 saves of 46-81 KB, a median 0.30 ms of the server's thread, the longest 6.03 ms |
| W2: a deputy in Blue's seat and the Normal AI in Red's, for an hour | a tick's 99th percentile 0.22-0.25 ms and the hosted players' 0.05-0.06 ms, against U7's 5 ms; the longest tick 6.30-17.07 ms, of which the hosted players took at most 2.53 ms |

- **Build:** Milestone 39 in the Linux container: `OutpostServer --world-run` built by clang 18 at `-O2`; the deputy's hour on a build whose compiler is not stated
- **Seeds:** seed 1 for the day, with the Normal AI in both seats and 10 kills; 1 to 3 for the deputy's hour
- **Method:** Processor time and the world log's save records; the deputy's hour is `InProcessServerTests.MeasuresTheHostedPlayersTicks` run with a deputy in Blue's seat, a locally changed test, since the committed one hosts two `AiEmpire` seats.
- **Source:** the Phase 5 plan, milestone 39's As built (commit `a234270`, #98); ADR-082, Consequences (commit `a234270`, #98).

### 2026-10-10 · Rocks drawn on the 10 km map (UI5.2)

| Quantity | Value |
|---|---|
| Rocks a field, and over the map | 14 to 18 a field, 15.7 on average, 1,254 over its 80 fields against 560 at seven a field |

- **Build:** UI5.2's field layout, in the Linux container
- **Seeds:** not applicable: the field identifiers 1 to 80
- **Method:** Counted from `Map.json`'s field radii, at a reach of 0.60 and a gap of 16 m. In a match a field's identifier is its entity's, so this is a sample of the layout, not a match's count.
- **Source:** the second interface plan, UI5.2's As built (commit `f19fe26`, #103); ADR-087, Consequences (commit `1ef27b5`, #103).
- **Note:** The second interface plan calls the count computed, ADR-087 measured. What the added rocks cost on the GPU is not measured.

### 2026-10-11 · The light and the glow on the GPU (LV1.2)

| Quantity | Value |
|---|---|
| With the light and the glow, 7,415 frames: a whole frame's GPU work | mean 0.67 ms, median 0.66 ms, 99th percentile 1.19 ms, worst 1.76 ms |
| `main` the same day, 10,281 frames | mean 0.37 ms, median 0.32 ms, 99th percentile 0.81 ms, worst 1.17 ms |
| The difference | the glow and the light together 0.30 ms a frame on average, 5% of a 165 Hz frame's 6.06 ms; the median up 0.34 ms and the 99th percentile 0.38 ms; the two not measured apart |
| The server's ticks, which draw nothing | a mean of 0.77 ms on the change's run against 0.62 ms on `main`'s |
| Startup | the server's stage 311 ms against 247 ms; the device stage finished at 2,220 ms from the start of `wWinMain` against 1,906 ms |

- **Build:** LV1's build, before ADR-092's occlusion, and `main` of the same day; the owner's Release build, at 1920×1080 on a 165 Hz display
- **Seeds:** not applicable: the `--stress` scene
- **Method:** `--measure --stress`, one run of each, summarized by `Tools/FrameTimes.py`. One run each does not separate the differences in the ticks and startup from the machine's state.
- **Source:** the Look plan, LV1.2's As built (commit `602f1b6`, #107); ADR-091, Consequences (commits `07cd7a9` and `602f1b6`, #107).
- **Note:** Q4 is met. The light alone, and LV2.1's occlusion, are not measured.

### 2026-10-11 · Baking the meshes with occlusion (LV2.1)

| Quantity | Value |
|---|---|
| A bake of every source | 117 s one source after another and 33 s one a core; the largest, the Human `Shipyard_L5` of 1,236 triangles, 19.5 s alone |
| `--check` | 33 s |
| The 45 `.nmf` files | grew by 4 bytes a vertex, 195,792 bytes in all, from 1,375,904 to 1,571,696 |

- **Build:** LV2.1: `Tools/BakeMeshes.py` with CPython 3.13 on 4 cores in the Linux container
- **Seeds:** not applicable
- **Method:** Wall time of the bake, serially and one source a core; the files' sizes.
- **Source:** the Look plan, LV2.1's As built (commit `7aa6e4d`, #108); ADR-092, Consequences (commit `7aa6e4d`, #108).
- **Note:** CI's mesh job has 4 cores and a 10-minute limit.

## Screen

The figures of the reviews of the screen, and of the meshes as drawn: sizes, contrast, brightness and hues.

### 2026-10-02 · The owner's low-poly models

| Quantity | Value |
|---|---|
| Hull lengths in model units | Small, Medium and Large are 45, 112 and 330 units long in the Human set and 46, 110 and 466 in the Tarkan set |
| Hull length to width, seen from above | the Human Small, Medium and Large are 1.0, 2.3 and 3.5 times longer than they are wide, and the Tarkan ones 1.05, 1.1 and 1.55 times |

- **Build:** The baked `.nmf` meshes of the owner's models, 2026-10-02
- **Seeds:** not applicable
- **Method:** Measured from the baked meshes.
- **Source:** the MVP design, §11 (commit `f0cbb94`, #46).

### 2026-10-02 · The rocks' meshes (task C.1)

| Quantity | Value |
|---|---|
| Triangles in the Small, Medium and Large rocks | Small, Medium and Large rocks have 32, 54 and 78 triangles, in place of the old 514-triangle Asteroid |

- **Build:** The baked rock meshes of task C.1
- **Seeds:** not applicable
- **Method:** Counted from the meshes.
- **Source:** the MVP plan, task C.1 (commit `8197633`).

### 2026-10-03 · Ships turn in arcs, and bank (task 7.2)

| Quantity | Value |
|---|---|
| A Fusion ship's bank with the first "full at" values | a Fusion ship reached only 40-70% of its limit (so each 'full at' became the hull's Fusion design's sideways acceleration in a sharp turn at half speed: Small 60 m/s2, Medium 26, Large 8, the Constructor 55) |

- **Build:** Phase 1 at task 7.2; where it ran is not stated
- **Seeds:** not applicable
- **Method:** Not stated.
- **Source:** the Phase 1 plan, task 7.2 (commit `b484918`, #49).

### 2026-10-03 · A Mining Rig on its rock (task 9.5)

| Quantity | Value |
|---|---|
| Mining Rig stance on its rock (task 9.5) | the three-legged Tarkan rig stands on all three legs, within 0.6 m; the four-legged Human rig rests on two, and its others hang up to 6.2 m; on a 45 m rock the crown goes up to 10 m into the Human rig's body |

- **Build:** Phase 1 at task 9.5: `StandOnFeet` and `StandPoint` compiled in the Linux container with the shipped meshes
- **Seeds:** not applicable
- **Method:** The tests' cases, on the shipped meshes.
- **Source:** the Phase 1 plan, task 9.5 (commit `1d8b4f3`, #51).

### 2026-10-03 · The first interface review

| Quantity | Value |
|---|---|
| The type a player decides by, and a locked card's text | 11-13 px at 1080p, where Xbox Accessibility Guideline 101 asks 18 px on PC; a locked card's text 3.6-4.0:1 against its card, under 4.5:1 |
| The selection ring's band | a band 15% of its radius wide: about 15 px of saturated green under a Shipyard |
| The minimap's marks and size | fields 1.3:1 against the map and dry asteroids 2.5:1; the minimap 260 units for a 5 km map |
| The bank in the screenshots, with the queues empty | 4,292-5,136 Ore over the last three screenshots, while the queues they show, research's and Shipyard 05's, stand at 0 / 5 |
| The designer's size before and after the proposed type scale (gate K1) | from 728×790 units with five weapons, 38% of a 1080p screen's width and 73% of its height, to about 816×840, 43% and 78% (Phase 1 §12 gives 38% and 65% at the mockup's three weapons) |
| The proposed locked-text color and the labels (task 14.2) | locked text at about (0.20, 0.26, 0.35) linear: 4.6:1 on a locked card's hatching and 5.1:1 on its body, under a third the luminance of live text; the labels 4.8-5.3:1 |
| Texts the contrast test was expected to find under 4.5:1 | the name field's count '21/32' at 3.5:1; the hatched header's labels, such as 'SHIPYARD 05 · DESIGNER', at 3.6:1 on its stripes |

- **Build:** The game as of 2026-10-03: `Hud::Typefaces`, `Hud.cpp`'s colors and the layout, and six screenshots of a match; the machine and the build are not stated
- **Seeds:** not stated
- **Method:** Sizes are `Hud::Typefaces`' sizes in reference units, pixels at 1080p; contrast is WCAG's ratio of the relative luminances of the linear colors, encoded to sRGB, as the contrast test computes it; computed, not measured on a screen; the ring's band and the bank are read off the screenshots; the designer's size is computed from the layout.
- **Source:** the Interface plan, what the review found, its gates and task 14.2 (commits `59300ed` and `0763dc7` (#65)).

### 2026-10-04 · A type scale that reads (task 14.2)

| Quantity | Value |
|---|---|
| The designer's measured size and card heights at the new type scale (14.2 As built) | part cards 220 wide and the designer 818; a card stays 80 units tall, since measured the larger faces need only 2 units more in a row with notes, not about 12; the designer 818×810 with three weapons and 818×913 with five and the module row; topic lines 18 units apart |

- **Build:** Task 14.2: `HudTests` with the real fonts, in CI's Debug|x64
- **Seeds:** not applicable
- **Method:** The layout measured with the fonts (ADR-061).
- **Source:** the Interface plan, 14.2's As built (commit `ea7a47a`, #65).

### 2026-10-04 · Structure levels' meshes

| Quantity | Value |
|---|---|
| Higher levels at level 1's scale | the baked meshes would grow up to 2.6 times in their longest dimension, and the Human Shipyard 5 times in depth |
| The Human Command Station fitted by length alone | it would stand 57% deeper than its footprint at levels 2 and 3 |

- **Build:** The baked `.nmf` files of 2026-10-04
- **Seeds:** not applicable
- **Method:** Measured from the baked files.
- **Source:** the Phase 3 design, §4 (commit `6a596f9`, #63); the Phase 3 plan, task 20.2 (commit `5e258bc`, #67).

### 2026-10-05 · The minimap's marks (task 16.2)

| Quantity | Value |
|---|---|
| Minimap marks' contrast after 16.2 | a field 2.1:1 and a dry asteroid 3.1:1 against the map, both computed |

- **Build:** Task 16.2's colors
- **Seeds:** not applicable
- **Method:** Computed as task 14.2's contrast test computes it.
- **Source:** the Interface plan, 16.2's As built (commit `2754910`, #78).

### 2026-10-05 · The interface scale's largest step (task 17.1)

| Quantity | Value |
|---|---|
| The interface scale's largest step that fits (17.1) | 110% in a match on any 16:9 screen, since the designer with five weapons and the module row is 913 units tall; 125% on 2880×1920 |

- **Build:** Task 17.1's layout
- **Seeds:** not applicable
- **Method:** Computed from the layout: the designer and the Controls window between the screen's margins.
- **Source:** the Interface plan, 17.1's As built (commit `5c5b5a6`, #79).

### 2026-10-08 · The second interface review

| Quantity | Value |
|---|---|
| A grid line's brightness in sight and where seen before | a grid line peaks at 27 of 255 in sight and 15 where seen before (27.3 in shots 1-3, 15.4 in shot 5); ADR-024's seen-before shade of 0.55 applied in linear light to 27.3 gives 15.6 |
| Outside sight at the proposed seen-before shade of about 0.8 (gate V2) | from 15 of 255 to about 7, against 27 in sight and about 4 where never seen |
| The warning's and the enemy's hues | WARNING_COLOR at an sRGB hue of 17.5 degrees and ENEMY_COLOR at 14.6 degrees (3 degrees apart) |
| The build bar's hue against the player's | the build bar at 202.5 degrees against the player's 207.8 degrees |
| The status panel's width | 515 px in shot 1 and 322 px in shot 5, between two panels 260 units wide |
| The ticket drain and the time to run out | review: at 3 nodes to 2 the enemy loses 1 ticket every 3 s and is out in 32:06; in shot 5, at 3 to 1, it loses 2 every 3 s and is out in 12:09. As built: 'Enemy -20 a minute · out in 32:06' and 'You -60 a minute · out in 12:09', the 12:09 case three nodes behind with 729 tickets, or 728 |
| A bar laid on the ground, at the camera's pitch | the bar's thickness foreshortened to 0.64 at 40 degrees |
| The retreat button's distance from the selection it acts on | 420 px |
| Rock fields against a group of ships (shot 1) | the two rock fields cover 15,400 and 22,465 lit pixels, against 851 for the group of six ships; the ships are twice as bright a pixel, the rocks 18-26 times larger |
| The fleet, the Shipyards and the bank in the screenshots | shots 1-4: the fleet at its cap, 29 / 30 with a Medium hull taking 2; shot 5: four Shipyards idle with room for 11 points and 25,586 Ore banked |
| The bank across the five screenshots | 15,387-25,586 Ore |

- **Build:** The game as of 2026-10-08: five 1917×1078 PNG screenshots of a match against the AI on the 10 km map, on a 1080p screen; the machine and the build are not stated
- **Seeds:** not stated
- **Method:** Brightness is the Rec. 709 luma of the sRGB pixels: the median of each grid line's peak along a row; lit pixels are those above 35 in a box round each field and round the ships; widths are the distance between a panel's brackets; hues are the linear colors encoded to sRGB. The shade at 0.8, the drain and the bar's foreshortening are computed.
- **Source:** the second interface plan, what the review found, its gates and what is not in the plan (commits `92cfc97` (#99) and `4b486d4` (#100)).
- **Note:** For the drain, the review's fifth screenshot is two nodes behind (3 to 1) and UI3.3's test three behind with 729 tickets; the screenshot's own figures were not recorded.

### 2026-10-08 · The minimap's territory (UI1.4)

| Quantity | Value |
|---|---|
| Contrast of the minimap's sector lattice (UI1.4 As built) | (0.11, 0.13, 0.17) linear, 2.06:1 against the map |

- **Build:** UI1.4's colors
- **Seeds:** not applicable
- **Method:** WCAG's ratio of the relative luminances of the linear colors, encoded to sRGB, as the contrast test computes it; computed, not measured on a screen.
- **Source:** the second interface plan, UI1.4's As built (commit `5821933`, #99).

### 2026-10-09 · A warning is a chip (UI2.1)

| Quantity | Value |
|---|---|
| Contrast of a warning chip's words on its tag (UI2.1 As built) | 10.3:1, computed |

- **Build:** UI2.1's colors
- **Seeds:** not applicable
- **Method:** WCAG's ratio of the relative luminances of the linear colors, encoded to sRGB, as the contrast test computes it; computed, not measured on a screen.
- **Source:** the second interface plan, UI2.1's As built (commit `f9e117b`, #101).

### 2026-10-10 · Bars on the screen, and commands beside the selection (UI4.1, UI4.2)

| Quantity | Value |
|---|---|
| Where the scope's rule would have put a Command Station's bar | 173 pixels over its rim at the default view and 576 at the nearest, often off the screen |
| A bar's length at the default view, before and after | 32 pixels, where a Large hull's was 184 and the Command Station's 346 |
| The health fills' red on a side's back at 0.3 | the HUD's poor red would stand at 1.67:1, against the ground bars' red at 2.36:1 |
| The screen shape below which the command and selection panels move right of the minimap | a screen narrower than about 3:2, such as 4:3, with both panels at their widest |

- **Build:** UI4.1 and UI4.2 as built
- **Seeds:** not applicable
- **Method:** The red's contrast is WCAG's ratio of the relative luminances of the linear colors, encoded to sRGB, as the contrast test computes it; computed, not measured on a screen; the screen shape is computed from the layout; how the bar's place and length were found is not stated.
- **Source:** the second interface plan, UI4.1's As built and UI4.2's As built (commits `c5e3058`, `87e90b4` and `891e07e`, #104).
- **Note:** The owner chose on 2026-10-10 not to build the scope's rule for the bar.

### 2026-10-10 · Fields of their own (UI5.2)

| Quantity | Value |
|---|---|
| Rock meshes' least reach and the old field ring's widest gap (UI5.2 As built) | the rock meshes reach as little as 0.60 of their radius across, measured from their files, so the old ring could leave about 46 m between two rocks; gaps are now closed to a Small hull's 16 m footprint |

- **Build:** UI5.2: the committed rock meshes
- **Seeds:** not applicable
- **Method:** Measured from the mesh files.
- **Source:** the second interface plan, UI5.2's As built (commit `f19fe26`, #103).

### 2026-10-11 · The Look review, and a warm key and a cool fill (LV1.1)

| Quantity | Value |
|---|---|
| A Human ship's lines against its faces, before LV1 | a Human ship's lines stand 2.75:1 above its faces on the darkest face the camera sees and 3.94:1 on the brightest; a face twice as bright brings the darkest under 2:1 |
| A warm key and a cool fill | a warm key and a cool fill, no darker than today's 0.3 ambient where the camera looks, keep every model within 0.1 of its ratio |
| The least light on a face the camera sees, after LV1.1 | the least light 0.315 in luminance; the light on a face that looks up within 7% of gray |

- **Build:** The code and data of 2026-10-11, before LV1.1 for the first two rows; `SceneShadingTests`, in the Linux container (gcc and clang against stand-ins) and in CI, for the last
- **Seeds:** not applicable
- **Method:** A Python script replaying `MeshPS.hlsl`'s light, `GameClient.cpp`'s face and line colors and `Models.json`'s side colors over every unit normal in 3° steps that the camera can see at its lowest pitch, 40°; WCAG's ratio of the linear luminances, as ADR-040 computed it. Computed, not measured on a screen.
- **Source:** the Look plan, what the review found and LV1.1 (commit `fdcc5f5`, #107).

# Outpost Commander — Self-Play Plan

Status: **open** · Started 2026-10-04, when the owner opened self-play as a side project and answered its first questions · Revised that day for Phase 3's draft and the horizon · Derived from [ADR-063](../ADR/ADR-063-self-play-probe.md) and [the network's blueprint](SelfPlayNetwork.md)

Self-play is a side project beside the game's phases. AIs fight each other, and a search keeps what wins. It is a probe of the match's rules, and it changes nothing a player sees. The game's phases keep plans of their own. This plan says **in what order** the side project is built, as a queue of tasks. It is a work queue, not an authority: where it disagrees with a design, AGENTS.md or an ADR, those win and this plan gets fixed. [The horizon](../Horizon.md), the owner's long-term direction, amends nothing here; the blueprint's §16 says what it changes for the network.

---

## The owner's answers, 2026-10-04

- **What the network decides:** macro decisions and fleet tactics. Macro decisions are research, designs, and when to attack, hold, raid or claim. Fleet tactics are which target a group goes for and when it falls back. The scripted AI keeps placement, the Constructors and pathing.
- **What it is for:** a probe of the rules. The opponent the game ships stays the scripted one (Game §ai, ADR-041).
- **Where it trains:** an x64 laptop, on the Release|x64 build.
- **What comes first:** a search over `Opponent.json`'s numbers, with no network. It is the baseline a network has to beat.
- **For Phase 3 and the horizon,** later that day: the blueprint and this plan target Phase 3's rules, and take from the horizon only what costs nothing on today's map. The network stays the probe, and becomes the benchmark for the horizon's deputy. Whether a learned policy should ever command a seat in a season was gate N6, decided on 2026-10-25: no.

## How an agent uses this plan

1. **Read AGENTS.md, ADR-063 and ADR-020, then the blueprint for milestone SP2, and [the living design](../OutpostCommander.md) for the rules as built.** SP2 targets Phase 3's rules, and [the post-mortems](../Records/PostMortems.md) say what each phase since has changed.
2. **Take the lowest-numbered task whose status is `todo`, whose dependencies are `done` and whose gate is decided.** One PR per milestone, in milestone order.
3. **Tasks are numbered SP\<milestone\>.\<task\>,** apart from the game's milestones, so that the phases keep their numbers. The blueprint's questions are gates N1 to N6, all decided, and this plan's own is gate S1.
4. **Know what you cannot verify.** An agent in a cloud container has no Windows, no MSBuild and no GPU. The server, the AI and the switch build and run there against a stand-in for the Windows headers, as the phases' did. A search on the real build is the owner's run.

---

## Task board

| Task | Title | Depends on | Gate | Status |
|---|---|---|---|---|
| SP1.1 | `--ai-matches` plays any two settings | — | — | done, [#64](https://github.com/Zwaliebaba/Outpost.Commander/pull/64) |
| SP1.2 | The search over the AI's numbers | SP1.1 | — | done, [#64](https://github.com/Zwaliebaba/Outpost.Commander/pull/64) |
| SP1.3 | The first search, and what it found | SP1.2, SP1.5 | — | todo: owner run |
| SP1.4 | The search follows Phase 3 | SP1.2, Phase 3's AI | — | done, [#80](https://github.com/Zwaliebaba/Outpost.Commander/pull/80) |
| SP1.5 | Knobs for Phase 4's AI numbers | SP1.4 | — | done, [#150](https://github.com/Zwaliebaba/Outpost.Commander/pull/150) |
| SP2.1 | The network's shape, as an ADR | SP1.3 | S1, N1–N6 | todo |
| SP2.2 | The network plays | SP2.1, Phase 3 built | — | todo |
| SP2.3 | Trajectories, the warm start and the trainer | SP2.2 | — | todo |
| SP2.4 | The league, and what the network found | SP2.3 | — | todo: owner run |

### Milestone order

SP1, then SP2. The network is built only once the search over the numbers has a result, because that result is its baseline. SP1 answers what fixed numbers can do. SP2 answers what decisions that adapt to the match can do on top of them. SP1.5 comes before SP1.3, since the search's self-test fails until the knobs follow `Opponent.json` again.

SP2 targets Phase 3's rules, which were built and run by 2026-10-04, so SP2.2 waits only for SP2.1. SP1.4 brought the search up to Phase 3's AI ([#80](https://github.com/Zwaliebaba/Outpost.Commander/pull/80)). Nothing in this plan builds for the horizon's galaxy, its seasons or more than two players; the blueprint's §16 lists what waits for a phase design that takes them up.

---

## Gates

The network's own questions are the blueprint's gates N1 to N6, which SP2.1 needs decided. This plan has one more.

| Gate | Decision | Proposed in | Blocks | Status |
|---|---|---|---|---|
| S1 | **What follows the first search.** Once SP1.3 has sorted what the champion does into a rule that wants changing, a weakness of the scripted AI, or neither, the owner decides what follows: a rules change, an AI change, SP2, or nothing. | SP1.3 | SP2.1 | open |

---

## Milestone SP1 — The search over the AI's numbers

### SP1.1 — `--ai-matches` plays any two settings

- **Goal:** a script plays any two settings of the AI against each other, on the seeds it names, into the log it names, with no message box.
- **Scope:** options for each AI's settings file, the first seed, the number of matches, the time limit and the log. `--quiet` reports a failure on standard error and by the exit code. `PlayAiMatches` takes each player's settings. With no options, the switch plays ADR-038's ten matches as before.
- **ADR:** ADR-063 decisions 1 and 2. ADR-038 decision 3, edited in place.
- **Verify:** CI builds it. In the container, against the stand-in: the same seeds log the same bytes as before, per-player settings take effect, and every malformed option fails with its own message. On Windows, one run of the switch with `--quiet` and a bad option shows the message on standard error; that is the owner's run in SP1.3.

### SP1.2 — The search over the AI's numbers

- **Goal:** ADR-063 decisions 3 to 8.
- **Scope:** `Tools/SelfPlay.py`: a separable CMA-ES over the numbers of `Opponent.json` and its research order, against a champion and a hall of fame, from both seats on shared seeds; a champion confirmed on fresh seeds; state saved every generation, and `--resume`; the report against the packaged settings on seeds 1 to 40; `--self-test`.
- **Verify:** `--self-test`, and a short search against the stand-in in the container, stopped and resumed.

### SP1.3 — The first search, and what it found

- **Goal:** the headroom figure, and what the champion does that the packaged AI does not.
- **Scope:**
  - Build Release|x64.
  - Time one match: `--ai-matches --matches 1` prints its seconds.
  - Run `Tools\SelfPlay.py`, with `--generations` set to what the night allows.
  - Read `report.txt`.
  - Record here what the champion does, and sort each finding: a rule that wants changing, a weakness of the scripted AI, or neither.
  - The owner decides what follows: a rules change, an AI change, SP2, or nothing (gate S1).
- **Verify:** **owner run.**

### SP1.4 — The search follows Phase 3

- **Goal:** the search moves Phase 3's numbers too, once Phase 3's AI has them in `Opponent.json` (Game §ai.levels).
- **Scope:** `Tools/SelfPlay.py`'s table of knobs follows `Opponent.json` as Phase 3 leaves it. Its upgrade numbers are added, each with a range that holds its packaged value, and any number Phase 3 removes is taken out. The research order follows the packaged file by itself: 23 topics once the gateways go.
- **Verify:** `--self-test`, which checks that the packaged settings lie in every range and decode to themselves, and a short search in the container.
- **As built:** [ADR-063](../ADR/ADR-063-self-play-probe.md) decisions 3 and 6, and ADR-020 decision 14, edited in place.
  - **Phase 3's two numbers were already knobs.** Phase 3's AI added `secondSlotTier` and `attackLevelMeters` to the table with the AI itself, and the research order already held the 23 topics, so this task found the table complete: 24 knobs.
  - **`secondSlotTier` was a dimension of noise.** The Lab's level with a second slot comes after the level that opens tier 3, so 1, 2 and 3 play alike. It is now searched at 3, take the slot, and 4, never take it; the self-test holds that range to `Tuning.json`'s Lab levels. The saved state's version moves to 2, so a search saved under the old range is refused rather than read wrongly.
  - **The self-test fails when the search falls behind**: a number of `Opponent.json` without a knob, a knob not in the file, or a topic of the tree missing from the research order. Each was broken on purpose once, and the self-test failed each time.
  - **The report** gives each side's tier 3 beside its tier 2, and when it first finished each level of its Command Station, Shipyards and Lab.
  - **Run in the container:** a search of 6 generations against a stand-in for the game, which writes match logs from the settings it is given, played its matches, promoted its champions and wrote the report with each side's levels. Stopped after 2 generations and resumed to 4, it gave the same history and report as a run straight to 4, and a state saved by version 1 was refused. The stand-in decides nothing as the game does, so this checks the script, not the AI.
  - **The AI, run in the container** on the server and AI built with clang 18 against a stand-in for the Windows headers: seeds 1 to 4, both players at each `secondSlotTier` from 1 to 4. At 1, 2 and 3 every seed logged the same bytes. At 4 no Lab reached level 4, and the two seeds where player 1's Lab had reached it at 3, seeds 1 and 4, played on differently; seed 4's winner then won by production at 50:45 rather than by domination at 57:00. In seeds 2 and 3 no Lab came within reach of level 4, so 4 changed nothing there. A match took 11 to 29 CPU seconds, about 0.42 ms a tick. Floats replay only on one build (ADR-009), so MSVC's Release build may play these seeds differently.

### SP1.5 — Knobs for Phase 4's AI numbers

- **Goal:** the search follows `Opponent.json` again, so that its self-test passes and SP1.3 searches every number the packaged AI plays by.
- **Why:** Phase 4's AI added three numbers to `Opponent.json` that the table of knobs in `Tools/SelfPlay.py` lacks: `attackCapShare`, the share of the room under the fleet cap that the attack group takes (ADR-041 decision 1); `pirateMargin`, how far a detachment outweighs a pirate outpost in Ore (ADR-076 decisions 2 and 3); and `scoutOwnHalfSeconds`, how long the scout explores its own half (ADR-076 decision 4). The self-test checks that every number of the file has a knob, so `python3 Tools/SelfPlay.py --self-test` fails on `main`: "Opponent.json's numbers and the knobs differ: ['attackCapShare', 'pirateMargin', 'scoutOwnHalfSeconds']".
- **Scope:**
  - The three join the table, each with a range that holds its packaged value in `Opponent.json` and keeps to what `AiSettings` accepts: a share above 0 and at most 1, a margin above 0, and seconds not below 0. The table's 24 knobs become 27, and the script's own account of them says so.
  - The saved state's version moves on, as SP1.4's did, so a search saved with fewer knobs is refused rather than read wrongly.
- **Verify:** `--self-test`, and a short search in the container, stopped and resumed.
- **As built:** the three are searched from 0.3 to 1, 0.5 to 4 and 0 to 1,800 s, each around its packaged 0.8, 1.5 and 600 s. `attackCapShare` and `pirateMargin` join the loader's positive numbers in the script's check, and `attackCapShare` its shares of at most 1. The saved state's version moves to 3. CMA-ES's default population stays 15 with 50 dimensions, so a generation still plays 360 matches (ADR-063).
  - **Run in the container:** `--self-test` passes, and failed on `main` as this task says. A search against a stand-in for the game, which scores each side by its settings' distance from a fixed point that names the three numbers, ran 4 generations of 2 seeds; stopped after 2 and resumed to 4, it gave the same history and report as a run straight to 4, and its champion moved all three toward that point. A state saved by version 2 was refused. The stand-in decides nothing as the game does, so this checks the script, not the AI.
  - **Each check was broken on purpose:** an `attackCapShare` range reaching 1.2 failed the self-test on the loader's share of at most 1, and a missing knob failed it as `main` does.

---

## Milestone SP2 — The network

[The blueprint](SelfPlayNetwork.md) describes the network in full: what it sees, what it decides, how it is trained, and what it costs. It targets Phase 3's rules, and takes from the horizon what costs nothing on today's map (blueprint §16). It is a proposal. SP2.1 turns it into decisions.

### SP2.1 — The network's shape, as an ADR

- **Goal:** an ADR written from the blueprint as gates N1 to N6 decided it, against Phase 3's rules as built.
- **Scope:** the ADR only. The blueprint stays as the explanation, edited to what was decided.
- **Verify:** the owner's review.

### SP2.2 — The network plays

- **Goal:** a seat of `--ai-matches` plays with a network, deciding what the blueprint's heads decide, and the scripted body carries the decisions out.
- **Scope:**
  - In `Opponent`, the observation of the match and of each sector, the sector encoder, the heads with their pointers and masks, Phase 3's levels in the investment head, the weights file and the forward pass.
  - The AI's own draws, each seeded from the match's seed, the seat, the tick and the head.
  - The switch's options that give a seat a network.
- **Acceptance:** `GameLogicTests`:
  - the observation of a scripted snapshot;
  - the order of the sectors changes no probability but theirs, and mirrored seats give mirrored sectors the same probabilities;
  - masks forbid what the server would refuse;
  - a weights file reads back as written, and a wrong size or version is refused;
  - a forward pass matches a reference;
  - a decision reproduces from its seed, seat, tick and head alone;
  - a network with random weights plays a whole match on the real server;
  - the same seed and weights log the same match twice.
- **Verify:** CI; the container.

### SP2.3 — Trajectories, the warm start and the trainer

- **Goal:** the training loop runs, and its first network plays as well as the scripted AI it learned from.
- **Scope:**
  - The switch writes a trajectory for each seat that plays a network, and the scripted AI's own choices for a seat that does not.
  - A trainer under `Tools/` learns the scripted AI's choices first, then improves on them with PPO against the scripted AI.
  - An episode cut short is bootstrapped, so a segment of a longer run trains as a match does.
- **Acceptance:** the trainer's self-test learns a toy problem and bootstraps an episode cut short; the warmed-up network wins about half its matches against the packaged settings.
- **Verify:** the container, on a short run; the owner's laptop for the rest.

### SP2.4 — The league, and what the network found

- **Goal:** the blueprint's league trained for the owner's budget, and its report: win rates against the packaged settings and SP1's champion, what the network does differently, which decision carries its advantage, how soon a lead decides a match, and what better decisions are worth over the scripted AI, the horizon's deputy.
- **Verify:** **owner run.**

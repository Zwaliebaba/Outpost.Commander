# ADR-063 — Self-play searches the AI's numbers as a probe of the rules, and --ai-matches plays any two settings

Status: **accepted** · 2026-10-04

## Context

On 2026-10-04 the owner asked for two neural networks to fight each other and discover the best combination of actions, as a side project. That day the owner answered four questions:

- **What the network decides:** macro decisions, such as research, designs and when to attack, hold, raid or claim, and fleet tactics, such as which target a group goes for and when it falls back. The scripted AI keeps placement, the Constructors and pathing.
- **What it is for:** a probe of the rules. A strategy that wins too easily is a defect in the rules, as the balance check finds for designs (Phase 1 design §7). The opponent the game ships stays the scripted one of design §10, tuned by ADR-041.
- **Where it trains:** an x64 laptop.
- **What comes first:** a search over `Opponent.json`'s numbers with no network. It measures how much the scripted AI leaves on the table, and it is the baseline a network has to beat.

Two things shape the search:

- **No design is best.** The balance check requires every design to have a counter, so pure self-play chases itself round the triangle and never settles. Self-play in real-time strategy games meets the same cycle, and AlphaStar's league is the best-known answer: a population, with past winners kept and played again.
- **A match is expensive.** Measured in the Linux container with clang 18 at `-O2`, against a stand-in for the Windows headers, the AI against itself on seeds 1 to 4 took 60 seconds of CPU for 2 hours 56 minutes of play. That is about 15 seconds a match of 45 minutes, and per minute of play twice what Phase 1's smaller fleets cost (ADR-038). The search is sized by that figure.

The `--ai-matches` switch of [ADR-038](ADR-038-phase-one-match-log.md) gave both players the packaged settings, played seeds 1 to 10 into one log in the temporary folder, and ended with a message box. A script could name none of those.

## Decision

1. **`--ai-matches` takes options** (`AiMatchesOptions` in `OutpostCommander/AiMatches.h`).
   - `--ai1` and `--ai2` name a settings file for player 1's AI and player 2's. A file is read as strictly as the packaged `Opponent.json`, and a problem names the file.
   - `--first-seed`, `--matches` and `--limit-minutes` say which matches to play, and `--log` where to log them.
   - Each option takes a value and is given at most once. Anything else fails, naming the argument, so that a mistyped option cannot run the packaged AI in its place.
   - With no options it plays ADR-038's ten matches, as before.
   - The command line is split by `CommandLineToArgvW`, so a quoted path with spaces is one argument.
2. **`--quiet` shows no message box.** The exit code says how the run went, and a failure's message goes to standard error, where the script that ran the game reads it. It covers a failure in reading the options too.
3. **`Tools/SelfPlay.py` searches the AI's numbers with a separable CMA-ES.**
   - It moves the 24 numbers of `Opponent.json`, each within a range that holds its packaged value, whole numbers kept whole. It moves the research order as one key per topic, researched in the order of the keys. The AI researches the first topic in its order whose prerequisites are done, so every order is a valid one.
   - `secondSlotTier` is searched at 3, which takes the Lab's second slot, and 4, which never does. Every value up to 3 plays alike, since the slot's level comes after the level that opens tier 3 (ADR-020 decision 14), so a range below 3 would be a dimension of noise. The self-test holds the range to `Tuning.json`'s Lab levels.
   - The search follows `Opponent.json` and the research tree: the self-test fails when a number of `Opponent.json` has no knob, when a knob is not in the file, or when the research order does not hold every topic of `Tuning.json`. A number the AI gains is then searched, not silently held at its packaged value.
   - The counters, the default design and the scout's design stay the packaged ones.
   - The diagonal variant, after Ros and Hansen (2008), learns a step for each number but not how numbers move together. With 47 dimensions and about a hundred generations, a full covariance matrix would still be learning.
   - Like every other tool in `Tools/`, it uses the Python standard library and nothing else.
4. **A candidate's fitness is its mean score against the champion and one member of the hall of fame.**
   - The champion starts as the packaged settings. The hall holds past champions, the packaged settings among them once one is beaten, and each generation draws one member at random.
   - Each pairing is played from both seats. Every candidate of a generation plays the same seeds, so two candidates are compared on the same matches. A match reproduces from its seed and the two settings on one build (ADR-009). The seeds move on each generation, so the search is not tuned to a few maps' worth of luck.
   - A win scores 1, a loss -1 and a draw 0. A match still going at the time limit scores half a point to the side ahead on tickets. Tickets are how domination keeps score (ADR-057), so that side is the one winning on territory. Without the half point, a search among AIs that never attack would see no difference between them.
5. **A champion is confirmed on fresh seeds.** The generation's best, if it scored 0.2 or more against the champion, plays the champion again on 10 seeds it has never played, from both seats. If it scores 0.2 or more there too, it becomes the champion and the old one joins the hall. Every threshold is an option of the script.
6. **The report measures the champion against the packaged settings** on seeds 1 to 40, from both seats. The search never plays those seeds. It gives the champion's win share with its 95% interval, which is how much the scripted AI's numbers leave on the table. It also lists both sides' numbers, their research orders, and what each side did: when it opened tiers 2 and 3, when it first finished each level of its Command Station, Shipyards and Research Lab, its peak fleet, what it built most, and how its matches ended.
7. **The search runs on the Release|x64 build.** Players get that binary, and a match reproduces on one binary only (ADR-009), so a search is read on the build it ran on. The script runs one match a process, as many at once as the machine has cores, at below-normal priority, so that the laptop stays usable. It saves its state after every generation, and `--resume` goes on from there.
8. **The search changes nothing in the game.** `Opponent.json` changes only by the owner's decision, as ADR-020 has it. A champion that wins too easily is reported as a question for the rules, not shipped as the AI.
9. **The network comes after, and only if the knobs leave headroom.** It decides macro and fleet tactics on top of the scripted AI's body, as the owner asked. Its inputs, outputs, training loop and weights file are decisions for a new ADR. The side plan, [ImplementationPlan-SelfPlay.md](../../GameDesign/ImplementationPlan-SelfPlay.md), orders the work.

## Consequences

- **The default search is about a night's work.** With 15 candidates, 6 seeds and two opponents, a generation plays 360 matches, plus 20 for each confirmation. At the container's 15 CPU seconds a match, that is 90 CPU minutes: about 11 minutes on 8 cores and 6 on 16. A hundred generations take 19 hours on 8 cores. The report adds 80 matches. MSVC's Release build has not been timed, and the script prints its own estimate after each generation.
- **Checked in the Linux container** with clang 18, against the stand-in:
  - The ten matches of ADR-038 are unchanged. Seeds 1 to 4 logged the same bytes through the new options, and again with the packaged file named in both seats, as before this change.
  - A changed settings file for one player changed its matches, and so did swapping the seats.
  - Every malformed option failed with its own message: unknown, given twice, missing a value, a number out of range or not a number, a missing or invalid settings file, a log that cannot be written. Under `--quiet` the message went to standard error.
  - The script's `--self-test` checks the optimizer on test functions, the encoding, the loader's rules over 3,000 random points, and the scoring. It caught each of eight deliberate breaks in the script.
  - A search of 4 generations played 66 matches through the stand-in build, promoted 4 champions with the thresholds set to promote every time, and wrote its report. Stopped after 2 generations and resumed to 4, it gave the same history and the same champions as the run that went straight through.
  - Interrupted in its first generation, it stopped once the matches under way had finished, and `--resume` completed it. A game that failed stopped the search with the game's own message.
- **Not checked here:** the MSVC build, which CI's Debug|x64 is the first to compile; `CommandLineToArgvW`, standard error and the exit code on Windows; and whether the Release build's package layout puts the game at the script's default path, `x64\Release\OutpostCommander\AppX\OutpostCommander.exe`, with its Assets beside it. `--exe` names it otherwise. These are the owner's first run.
- **The probe finds only what the scripted AI can do.** A rush or a turtle is a few numbers away. A strategy the AI's code cannot express is not, such as kiting, a base walled shut or a raid on a target the code does not choose. Some of what it finds will be weaknesses in the AI rather than in the rules, and the owner sorts them.
- **The switch still has no test in CI**, as ADR-038 says: no test project may include both the AI and the log. Its option reading is in `AiMatches.cpp`, beside the matches, and was run in the container only.

## What this forecloses

- A search that changes `Opponent.json`, `Tuning.json` or the rules by itself.
- A learned AI in the game the players get, without a design amendment and a new ADR.
- A run of `--ai-matches` that takes options it does not know, or the same option twice.

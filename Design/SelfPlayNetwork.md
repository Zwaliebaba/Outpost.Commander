# The Self-Play Network — Blueprint

Status: **proposal** · 2026-10-04, revised that day for [Phase 3's draft](../GameDesign/Archive/OutpostCommander-Phase3.md) and [the horizon](../GameDesign/OutpostCommander-Horizon.md) · The blueprint of milestone SP2 of [the self-play plan](../GameDesign/ImplementationPlan-SelfPlay.md). Nothing here is decided until SP2.1's ADR. Six questions are the owner's, gates N1 to N6 (§15).

This document describes how the network that plays the AI's macro game would work: what it sees, what it decides, how it learns, where it sits in the code, what it costs, and what it tells the owner. [ADR-063](ADR/ADR-063-self-play-probe.md) records the first milestone, the search over the AI's numbers, and why it comes first. The network targets Phase 3's rules, because it will be built after them, and §16 records what the horizon would change.

---

## 1. What it is for

The owner asked on 2026-10-04 for networks that fight each other to discover the best combination of actions, as a probe of the match's rules. The opponent the game ships stays the scripted one of design §10. The owner also decided what the network decides: macro decisions and fleet tactics, with placement, the Constructors and pathing left to the scripted AI.

The first milestone answers what fixed numbers can do. The scripted AI applies the same rules whatever happens in the match: it researches in one order, answers a design with the counter its table names, and attacks once its reserve reaches a size. The network decides from the state of the match: what the enemy fields, where the territory stands, how the tickets run. If deciding from the state beats fixed rules by a wide margin, that margin and how it is won are the probe's findings. A strategy that wins too easily is a defect in the rules, or a weakness in how the scripted AI plays them, and the owner sorts which.

The network never plays in the game the players get. It plays only in the headless `--ai-matches` switch, and ADR-063 forecloses a learned AI in the shipped game without a design amendment and a new ADR.

**The horizon gives the AI another job.** The owner's horizon proposes a world that keeps running. In it, an AI called the deputy commands an absent player's seat around the clock, and the AI empires are the same code (horizon §4). A deputy much weaker than its player lets presence decide a season. The network does not become the deputy here: it stays the probe, and it becomes the deputy's benchmark (§13). Whether a learned policy should ever command a seat is gate N6 (§15).

## 2. The shape of it in one paragraph

The scripted `AiPlayer` stays the AI's body. Every 10 seconds of match time, the body builds an observation from its own snapshot and memory: 126 numbers about the whole match, and 18 about each sector. A small network encodes every sector with the same weights, pools them, and turns the result into six heads of choices: what to research, which design to build, what to build or upgrade, which sector to claim, where the main fleet goes, and whether to raid. A choice of a sector points at that sector's encoding, so the network reads a map of any number of sectors. The body carries the choices out as it carries out its own rules today. The network learns first by imitating the scripted AI, so that it starts as strong as the AI it replaces. It then improves with PPO, first against the scripted AI and then in a league of its own past versions. Training runs as a Python tool on the owner's laptop, and the game only plays matches and writes down what happened.

## 3. Where it sits

```
                     ┌──────────────────────────── Opponent (AiPlayer) ─────────────────────────────┐
  server ─snapshot─▶ │ every tick     Watch: shots on its structures, enemy warships sighted          │
                     │ every second   the body: placement, Constructors, repair, Shipyard queues,     │
                     │                design saving, the scout, the defence reflex, attack-moves       │
                     │ every 10 s     the decision step:                                               │
                     │                  snapshot + memory ─▶ observation ─▶ network ─▶ six heads      │
                     │                  masks ─▶ one choice per head ─▶ the body's standing orders     │
                     │                  (a trajectory record, when the switch asks for one)            │
                     └────────────────────────────────────────────────────────────────────────┬─────┘
                                                                                     commands ─▶ server
```

- **The AI stays a client** (ADR-002, ADR-020). `Opponent` includes `GameProtocol` and nothing of the server. The network sees what the scripted AI sees: its own player's snapshot under fog of war, and what the AI remembers. Nothing is given to it by the server that a player would not have.
- **Without a network, `AiPlayer` is the scripted AI, unchanged.** The game never gives it one. A seat of `--ai-matches` gets one with an option, `--policy1` or `--policy2` and a weights file.
- **The network replaces rules, not the body.** Each head takes over one rule the scripted AI decides with today (§6). Everything else, including every numeric setting the body still uses, comes from that seat's `Opponent.json` settings, as today.
- **The trainer is a tool.** It lives under `Tools/`, outside the build, and R14 binds what the executable is built from, not what a tool needs. The game links no learning library: its only learned part is the forward pass, written out in plain C++.

The types are proposals, named by AGENTS.md's rules, all in `Outpost` in `Opponent`:

| Type | What it holds |
|---|---|
| `MacroObservation` | the observation of one decision step: the match's numbers, and each sector's |
| `ChoiceMasks` | for each head, which choices the body can carry out now |
| `MacroChoice` | one choice per head consulted, as the body reads them |
| `PolicyNetwork` | the weights, the normalization, and the forward pass from an observation and masks to each head's probabilities |
| `TrajectoryWriter` | the records of a seat's decision steps, for the trainer |

## 4. The decision step

The network decides every 10 seconds of match time: every 200 ticks at the tuning data's 20 Hz. A match of 50 minutes is 300 decisions for each seat. Between decisions the last choice stands, and the body keeps acting on it every second as it acts on its own rules today.

Ten seconds is long enough for an order to show what it does, and short enough that the network can answer an attack or a lost battle within a few steps. Fewer steps make each decision's credit easier to find: at 300 a match, the discount below still reaches across most of a match. Gate N2 asks whether 10 seconds is right. A season of the horizon would be far longer, and §9 and §12 say what that changes. The step itself would not change.

The body's reflexes stay faster than the network. The defence reflex answers a shot on a structure within the second, as ADR-020 decision 8 has it, and closing in on a target and on a Relay stay the body's, every second.

## 5. What it sees

The observation has two parts: numbers about the whole match, and the same 18 numbers about each sector. On today's map of 9 sectors that is 288 numbers.

**The match**, 126 numbers:

| Group | Features | Count |
|---|---|---|
| Clock | minutes played ÷ 60; which stage of the match, one-hot: before 5 minutes, 5 to 15, 15 to 30, after 30 | 5 |
| Economy | Ore, on a log scale; income; rigs working; rigs on dry asteroids; Constructors; Shipyards built; Shipyards planned and not built; asteroids it knows still hold ore | 8 |
| Research | each of Phase 3's 23 topics researched; each of the Lab's two slots busy, and its progress; the tier opened, one-hot of 3 | 30 |
| Levels | its Command Station's level, and the nodes its cap allows; its Shipyards at each level (3); its Lab's level; upgrades under way; the highest level it has seen of the enemy's Command Station, Shipyards and Lab (3) | 10 |
| Its fleet | warships by hull (3), by drive (3) and by weapon (5); how many are in the reserve, the attack group and a raid; their hit points as a share of full; scouts; warships queued | 17 |
| The enemy's fleet | warships seen in the last minute by hull, drive and weapon (11); the same since the match began, fading with a half-life of 3 minutes (11); seconds since one was last seen | 23 |
| Structures | its own by kind (6); the enemy's seen or remembered by kind (6) | 12 |
| Domination | its tickets and the enemy's, as shares of the starting tickets; the nodes each holds | 4 |
| Its last choice | its fleet held at home, held at the front, or attacking (3); its production design's hull, drive and weapon (11) | 14 |
| Pressure | shots on its structures in the last 10 seconds; its warships and the enemy's lost in the last minute | 3 |

**Each sector**, 18 numbers:

| Features | Count |
|---|---|
| held by it, by the enemy, or free | 3 |
| suppressed; cut off | 2 |
| next to its territory; next to the enemy's | 2 |
| steps from its home, and from the enemy's, along the sectors' adjacency (ADR-036) | 2 |
| its warships there; enemy warships seen there in the last minute | 2 |
| its structures there; the enemy's seen or remembered there | 2 |
| the highest level of an enemy structure there | 1 |
| the ore its asteroids still hold, as far as it knows | 1 |
| its fleet's target, its raid's target, or its claim under way | 3 |

**Each seat sees the map from where it stands.** A sector carries no fixed number. It is described by what is in it, and by how many steps it lies from each home. One set of weights so plays both seats without mirroring the map, and a seat bias (ADR-038) has nowhere to hide in it. A map of another shape is read the same way, and so would a galaxy's systems be (§16).

**Counts are scaled, then standardized.** A count is divided by a fixed scale that puts it near 0 to 1, such as warships ÷ 100. The trainer then measures each feature's mean and spread over the warm start's data (§9), and those are frozen into the weights file, so that the game standardizes exactly as training did.

**The network has no memory of its own.** What a player would remember is in the observation: the enemy's fleet seen over the last minute and fading since, its remembered structures, its last choice. An AI started in the middle of a run, after a restart or at the start of a segment (§9), rebuilds this from the snapshots it then sees, within the few minutes its fading counts span. Nothing has to be saved. A recurrent layer is the next step only if the measurements show the network losing for want of memory.

## 6. What it decides

Six heads, 109 choices on today's map. A head is consulted only when its decision is open, and a mask removes the choices the body cannot carry out:

| Head | Choices | Consulted | Masked out | The scripted rule it replaces |
|---|---|---|---|---|
| Research | the 23 topics | when one of the Lab's slots is free | researched; prerequisites not done; a tier the Lab's level has not opened (Phase 3 §6) | the research order (ADR-020) |
| Production | 45 designs: 3 hulls × 3 drives × 5 weapons | every step | a component not unlocked; a hull above its best Shipyard's level (Phase 3 §5) | the counters, reviewed every 60 s (ADR-020 decision 8) |
| Investment | nothing; a Shipyard; a Defence Platform at home; one by its front Relay; the next level of its Command Station, of a Shipyard or of the Lab | every step | one of the same kind still waiting; no place for it; a structure at its last level | a Shipyard per income share; platforms per Shipyard and per front Relay (ADR-020, ADR-041); Phase 3's upgrade rules (Phase 3 §8) |
| Territory | no claim, or a sector | every step | a sector not free or not next to its territory; a claim still waiting for its Relay; the nodes its cap allows already held (Phase 3 §7) | the nearest free sector, one at a time (ADR-020 decision 13) |
| Posture | hold home; hold the front; attack a sector | every step | a sector where it has seen or remembers no enemy structure, apart from the enemy's home | the attack group's size and lead, the fall-back and the regroup (ADR-041) |
| Raid | no raid, or a sector; then 2, 4 or 8 ships | when no raid is under way | a sector the enemy does not hold; the enemy's home; more ships than half the reserve | the raid rule (ADR-020 decision 13) |

**How the body carries the choices out:**

- **Research** queues the topic in a free slot of the Lab, as the order's walk does today.
- **Production** becomes the design the Shipyards keep queued. The body saves the design when the player does not have it, as it does today. A Shipyard whose level is below the design's hull keeps the last design it could build.
- **Investment** adds a slot to the base plan, placed as the scripted AI places one today. A Shipyard is planned whatever the income. A level is ordered as Phase 3 §4 has it: the Ore paid when ordered and Constructors assigned as for a build. The Shipyard upgraded is the one of lowest level, nearest the Command Station.
- **Territory** plans the Relay and the rigs of the chosen sector, as the claim rule plans the nearest.
- **Posture.** *Attack sector s* sends the reserve to join the attack group. It attack-moves the group on the best structure it knows in that sector, production first (ADR-037) and the highest level first (Phase 3 §8), or on the sector's node when it knows none. *Hold the front* and *hold home* send the reserve to hold that sector with a standing order (ADR-059). A hold chosen while an attack is under way calls the attack group back to the rally, so a fall-back is a hold.
- **Raid** sends that many of the reserve's smallest hulls on the sector, and the body runs the raid as it runs one today, closing on the Relay.

**What stays the body's:** where every structure goes, the Constructors and repair, keeping the Shipyards' queues full, the scout and its tour, the defence reflex, the pathing and formation of every order, which structure an attack goes for within its sector, and closing in.

**What of `Opponent.json` still matters.** The body still reads `constructors`, `homeAsteroids`, `contestedAsteroids`, `shipyardQueueJobs`, `structureGapMeters`, `rallyDistanceMeters`, `defenseHoldSeconds`, `raidLossShare`, `scouts` and `scoutDesign`. The rest are the rules the heads replace, and so are the upgrade rules whose numbers Phase 3 adds (Phase 3 §8). A network seat plays on its seat's settings for the former, so it can play on SP1's champion's body.

**The heads are drawn independently** given the layers they share, and a step's probability is the product of the consulted heads' probabilities. The raid's size is drawn only once its sector is. Choices that depend on each other, such as a raid and an attack on the same sector, are left for the body to reconcile: a raid takes ships from the reserve before the posture moves the rest.

## 7. The network

| Part | Shape | Parameters |
|---|---|---|
| Sector encoder | each sector's 18 numbers → 64 → 64, tanh; the same weights for every sector | 5,376 |
| Pooling | the mean and the maximum of the sector encodings | none |
| Hidden 1 | the match's 126 numbers and the pooled 128 → 256, tanh | 65,280 |
| Hidden 2 | 256 → 256, tanh | 65,792 |
| Fixed choices | 256 → 82 logits: research 23, production 45, investment 7, the posture's two holds, no claim, no raid, the raid's three sizes | 21,074 |
| Pointer heads | territory, the posture's attack and the raid's sector: each a query 256 → 64, scored against each sector's encoding | 49,344 |
| Value | 256 → 1 | 257 |
| | | **207,123** |

About 200,000 parameters, 0.8 MB as floats. The sector encoder treats the sectors as a set, after Deep Sets (Zaheer et al., 2017). Pooling by mean and maximum gives the same result in any order of the sectors and for any number of them. A choice of a sector is a pointer, after Pointer Networks (Vinyals, Fortunato and Jaitly, 2015). The torso makes a query, each sector's logit is the dot product of the query with that sector's encoding, and the fixed choices of the same head, such as no claim, join them before the softmax. AlphaStar chose units the same way (Vinyals et al., 2019).

The initialization and the activations follow the defaults that the large study of on-policy methods by Andrychowicz et al. (2021) found to work: orthogonal weights, tanh, and the last layer of each head scaled down to 0.01 so that the first choices are near uniform.

**The forward pass in the game** is plain C++ over arrays of floats, in a fixed order: about 250,000 multiply-adds on today's map, done once every 10 seconds for each seat that has a network. The game is compiled `/fp:precise` with no contraction (R16), so the same binary gives the same probabilities for the same observation, which is what a replay needs (ADR-009). A masked choice's logit becomes minus infinity before the softmax, so it has probability zero. Masking the invalid choices, rather than letting the network pick them and fail, is what Huang and Ontañón (2022) found to scale.

## 8. How a choice is drawn

**In training and in the probe's report, each head's choice is drawn from its probabilities.** Each draw is computed from the match's seed, the seat, the tick and the head, not taken from a running generator. A fresh generator is seeded from the four, combined so that no two draws share a seed, and gives one number. The switch hands the seed to the AI, since the server does not; the AI learns nothing about the match from it.

A match with a network so reproduces from its seed, the two settings files and the two weights files, on one binary. And any single decision reproduces from the snapshot and the command log alone, with no generator state to save, which a world that restarts needs (horizon §3).

`Neuron::Random`, the generator ADR-009 pins, lives in `NeuronServer`, which the AI may not include (ADR-002). Gate N4 asks whether it moves to `NeuronCore`, where both may use it, or the AI keeps a copy.

**A greedy run takes each head's likeliest choice instead.** It shows what the network prefers, and the report gives both.

## 9. How it learns

### The reward

- **A match's outcome is the reward:** 1 for a win, -1 for a loss, 0 for a draw, and at the time limit half a point to the side ahead on tickets, as ADR-063 decision 4 scores a match.
- **Each step adds a shaping term** from the tickets: *r* = γΦ(*s′*) − Φ(*s*), with Φ = 0.5 × (its tickets − the enemy's) ÷ the starting tickets. Shaping of this form leaves the best policy unchanged (Ng, Harada and Russell, 1999), and it gives a signal every step of a long match.
- **Nothing else is rewarded.** Rewarding kills, Ore or nodes would teach the network what the scripted AI already believes. A probe of the rules is told only who wins.
- **The discount is 0.995 a step,** a half-life of 138 steps or 23 minutes, and the advantages are generalized advantage estimates with λ = 0.95 (Schulman et al., 2016).

### Matches now, segments later

- **Today an episode is a whole match.**
- **An episode that stops without an outcome is cut short, not lost.** The trainer adds the value it estimates for the last state to the returns, and does not treat the stop as an ending. The learning code so works unchanged on pieces of a longer game.
- **In a season there is no match to finish** (§12). An episode is then a segment of an hour or two, started from a state of a long run.
- **The cheapest start state needs no new code.** The same binary replays a recorded run from its seed and commands to any tick (ADR-009). The AIs replay with it, since their draws are seeded too (§8). Forking a running server would save the replay's time, but the server cannot copy itself today.
- **In a season the reward is position:** what the season's design makes winning, which the horizon leaves open (horizon §7). Until then the stand-in is the nodes held, and the capacity they allow once fleets are bounded (horizon §6.2), as a potential, as the tickets are today.

### Stage 1: the warm start

A network with random weights researches at random and attacks at random. It would lose every match to the scripted AI, and a reward that is always -1 teaches nothing. So the network first learns to do what the scripted AI does.

- The switch records, for a scripted seat, the observation at each decision step and what the scripted rules chose in the 10 seconds before it, translated into the heads' terms. For example, the topic the order's walk queued, the design the review chose, the level the upgrade rules ordered, or the sector the attack went for.
- About 2,000 matches between scripted AIs supply them, on the packaged settings and on SP1's champions, so that the network sees more than one way of playing.
- The heads learn by cross-entropy, and the value head learns the outcome.
- The warm start is done when the network, playing alone, wins about half its matches against the packaged settings. AlphaStar began the same way, from human replays (Vinyals et al., 2019).

### Stage 2: PPO against the scripted AI

The network then improves with proximal policy optimization (Schulman et al., 2017) against opponents that do not change: the packaged settings and SP1's champions, from both seats. It goes on until it beats them clearly. A fixed opponent first makes the learning curve readable, before the league makes it move.

| PPO setting | Value |
|---|---|
| Matches an iteration | 64 |
| Clip | 0.2 |
| Optimizer | Adam, learning rate 3 × 10⁻⁴, decaying linearly to 0 |
| Epochs an iteration | 4 |
| Minibatch | 2,048 steps |
| Entropy bonus | 0.01 for each head |
| Value loss weight | 0.5 |
| Gradient norm clip | 0.5 |
| Advantages | normalized in each minibatch |

These are the published defaults, and the self-test of SP2.3 checks the trainer on toy problems before it trains on the game.

### Stage 3: the league

Self-play against only its latest self cycles, as ADR-063 explains for the search, so the network trains in a league, after AlphaStar's:

- **Four matches in ten are against itself,** and both seats learn.
- **Four in ten are against a past version,** a checkpoint drawn by prioritized fictitious self-play: a checkpoint the network beats with probability *p* is drawn with weight (1 − *p*)². The versions it still loses to are the ones it plays.
- **Two in ten are against the scripted anchors,** the packaged settings and SP1's champions, so that it does not forget how to beat them.
- **A checkpoint joins the league every 10 iterations.** Every match is played from both seats in turn, and the league keeps an Elo rating for each checkpoint from all its matches.

The league is one against one. Its sampling and its ratings assume two players and one winner, and §16 says what more players would need.

## 10. The loop and its files

An iteration of the trainer:

1. It writes the network's weights file.
2. It plays the iteration's matches through `--ai-matches`, one match a process, as many at once as the machine has cores, as `Tools/SelfPlay.py` does. Each match is told which settings and which weights each seat plays, and where to write each network seat's trajectory.
3. It reads the trajectories, computes the returns and advantages, and updates the network.

**The weights file** is little-endian. It holds:

- a tag and a version;
- the sizes of the match's numbers, of a sector's numbers, of the encoder, the hidden layers and the heads;
- the observation's means and spreads;
- each layer's weights, row by row, and biases, as 32-bit floats.

`PolicyNetwork` reads it as strictly as `LoadAiSettings` reads its file. A size or a version that does not match what the game's code builds is refused, and the error names the file, so a network trained for another observation cannot play by mistake. Weights are the products of a run, kept in its output folder. None is committed, since no build of the game reads one.

**The trajectory file** holds one record for each decision step of one seat:

- its tick, the match's numbers and each sector's;
- for each head, whether it was consulted, its mask, the choice and the choice's log-probability;
- the value the network estimated, and both sides' tickets for the shaping term.

A last record holds the outcome, or says that the episode was cut short. A scripted seat's records hold the scripted rules' choices with no probabilities, for the warm start.

## 11. How it is checked

- **In `GameLogicTests`, in CI:**
  - the observation of a scripted snapshot;
  - the order of the sectors changes no probability but theirs;
  - the two seats of the point-symmetric map, in mirrored states, give the mirrored sectors the same probabilities;
  - every mask removes what the server would refuse;
  - a weights file reads back as written, and a wrong size or version is refused;
  - a forward pass matches values computed outside the game for a small network written into the test;
  - a decision reproduces from its seed, seat, tick and head alone;
  - a network with random weights plays a whole match on the real server;
  - the same seed and weights log the same match twice.
- **In the trainer's `--self-test`:**
  - PPO learns a contextual bandit and a short chain of states;
  - an episode cut short adds its last value to the returns;
  - the warm start fits a known policy;
  - a trajectory written by the game reads back.
- **In the container,** against the stand-in, as for SP1: a short training run, end to end.

## 12. What it costs

The only measured figure is the match: about 15 CPU seconds for a match of 45 minutes between scripted AIs, in the Linux container with clang at `-O2` (ADR-063). The network adds two forward passes every 10 seconds, about 150 million multiply-adds a match, which is milliseconds. Everything below is an estimate from that figure:

| Stage | Matches | CPU hours | On 8 cores |
|---|---|---|---|
| Warm start, scripted matches recorded | 2,000 | 8 | 1 hour |
| PPO against the scripted AI, 200 iterations | 12,800 | 53 | 7 hours |
| The league, 800 iterations | 51,200 | 213 | 27 hours |
| The report, 80 matches against each of three opponents | 240 | 1 | 8 minutes |
| **In all** | **66,000** | **275** | **35 hours** |

An iteration of the league yields about 27,000 decision steps, since a match against itself teaches from both seats. A thousand iterations are about 27 million steps. That is far fewer than AlphaStar or OpenAI Five used, for a far smaller problem: the observation is engineered, there are about a hundred choices, and the scripted body does all the work below the decisions.

The update itself is small. Four passes over an iteration's steps through 200,000 parameters is about 10¹¹ floating-point operations, seconds on a laptop's CPU. A GPU buys nothing at this size.

Gate N5 sets the hours. An earlier limit, 45 minutes rather than 120, in the first iterations shortens the matches that stall, with the tickets deciding at the limit.

**In a season, at today's cost per minute of play:**

| | Decisions per seat | CPU to simulate once |
|---|---|---|
| A 45-minute match | 270 | 15 seconds |
| One system, a 4-week season | 240,000 | 3.7 hours |
| 30 systems, a 4-week season | | 110 hours |

Training on whole seasons is out of reach, and segments are the answer (§9). A galaxy's training would also outgrow a laptop. The trainer plays one match a process, as `Tools/SelfPlay.py` does, so it spreads over more machines unchanged, provided they run the same binary (ADR-009).

## 13. What the probe reports

- **How much it wins.** The final network against the packaged settings and against SP1's champion, on seeds 1 to 40 from both seats, with 95% intervals, sampled and greedy.
- **What it does differently.** From the match logs: research timings, designs by 5-minute window, peak fleets and endings, through `Tools/MatchLog.py`. From its trajectories: how often it chose each research topic, design, level, posture, claim and raid at each stage of the match, beside what the scripted rules chose in the same matches.
- **Which decision carries its advantage.** Each head in turn is handed back to the scripted rule and the win rate measured again. The head whose return costs the most is where the advantage lies, and it points at the rule being exploited: a design that wins too much, an attack timing the defences cannot answer, a claim the tickets reward too much, a level bought too early to answer.
- **Whether it is brittle.** A fresh network, an exploiter, trains for 100 iterations against the frozen final network alone, as AlphaStar's exploiters did. If the exploiter wins easily, the final network's strategy is narrow, and its findings are read with that in mind.
- **How soon a lead decides.** The value head estimates the outcome from any state. Read over the report's matches, it says how soon a lead in nodes, levels, Ore or ships becomes decisive. That is the horizon's question of whether an early lead decides a season (horizon §7), asked of a match.
- **What better decisions are worth.** The network's margin over the scripted AI estimates what stronger macro decisions buy over the AI that would command an absent player's seat (horizon §4). The horizon's first measurement asks the same of battles (horizon §9), where the network has no say.

Each finding is sorted by the owner, as in SP1.3: a rule that wants changing, a weakness of the scripted AI, or neither.

## 14. What could go wrong

- **It cycles,** chasing its own latest strategy round the triangle. *Answer:* the league, prioritized fictitious self-play, and the scripted anchors (§9).
- **It finds the body's bugs rather than the rules' defects.** An attack the pathing cannot defend, say. *Answer:* the ablations point at the decision, and the owner sorts the finding. A weakness of the scripted AI is still worth knowing.
- **It cannot tell which decision won a 50-minute match.** *Answer:* 10-second steps, generalized advantage estimates, the tickets' shaping, and the warm start's head start.
- **A run outgrows the laptop.** *Answer:* the costs above, the warm start, an earlier time limit early on, and `--resume` after every iteration, as `Tools/SelfPlay.py` saves after every generation.
- **Results differ between builds.** Floats are only promised to replay on one binary (ADR-009). *Answer:* train and measure on Release|x64 (ADR-063 decision 7). Weights move between builds; outcomes do not reproduce bit for bit across them.
- **It fits the one map.** The encoding reads any map's sectors, but the weights learn this map. *Answer:* the probe answers for the repository's map, and another map is measured again before its findings count.
- **It is built for a galaxy that is not planned.** The horizon is not accepted, and a network shaped for systems and seasons before a phase design decides them would be shaped wrong. *Answer:* only what costs nothing on today's map is taken now (§16).
- **Phase 3 changes before it is built.** Its draft is not accepted, and its numbers are starting values. *Answer:* the heads and the observation follow its rules as built, and SP2.1's ADR is written against them.

## 15. The owner's questions

| Gate | Question | Recommendation |
|---|---|---|
| N1 | The trainer's library: PyTorch on the CPU, or NumPy with the gradients written by hand. Either is a tool's dependency that the game never links (R14). | PyTorch. Its gradients and its optimizer are used and tested far more widely than code written for this tool would be. |
| N2 | The decision step: 5, 10 or 30 seconds. | 10 seconds (§4). |
| N3 | The heads: the six of §6, with Phase 3's levels in the investment head and the defence reflex, the scout and placement left to the body; or the defence as a seventh head. | The six. The defence reflex is a reaction within a second, not a decision. |
| N4 | The AI's draws, each seeded from the seed, the seat, the tick and the head: `Neuron::Random` moved from `NeuronServer` to `NeuronCore`, or a copy in `Opponent`. | The move. One generator, already pinned by `RandomTests`. |
| N5 | The budget: how many hours of the laptop a run gets. | 35 hours for the plan of §12, run over several nights with `--resume`. |
| N6 | Whether a learned policy should ever command a seat in a season: the horizon's deputy, or an AI empire (horizon §4). | No. A player must be able to predict the deputy left in charge, and see why it lost a fleet overnight. A scripted deputy that follows its player's directives can be read that way, and a learned policy cannot. The network stays the probe, and the deputy's benchmark (§13). |

## 16. What the horizon changes

[The horizon](../GameDesign/OutpostCommander-Horizon.md) is not a plan, and it amends nothing here. Of what it would change, this blueprint takes now only what costs nothing on today's map:

- **The sectors are encoded one by one, with shared weights, and chosen by pointing at them** (§5, §7). The network so reads any number of sectors and needs no mirrored map.
- **A draw reproduces from the seed, the seat, the tick and the head** (§8). Nothing but the snapshot and the command log has to survive a restart.
- **The network has no memory of its own** (§5). After a restart, its observation is rebuilt from the snapshots the AI sees next. The scripted body's own state, such as its base plan and where it sent its Constructors, is not, and that is for the deputy's design to solve rather than the network's.
- **An episode cut short is bootstrapped** (§9). A segment of a season needs no change to the learning code.

What waits for a phase design that takes the horizon up:

- **A galaxy.** Systems become a second level of the same encoding, with travel between systems a new head and the commands that go with it.
- **More than two players.** Prioritized fictitious self-play and Elo assume two players and one winner. A free-for-all among friends and AI empires has kingmaking and alliances (horizon O6), and needs a league of its own.
- **Seasons of months.** The weights file then has to be read by later builds, as the world's save format must be (horizon §7). Today's file carries a version, and a build refuses one it does not know.
- **Directives, if gate N6 ever says yes.** A player's directives would fix the heads they cover: a research queue fixes the research head, and an order to hold fixes the posture. The network would decide the rest, so training would have to cover any directives a player could give. Its cost would be no obstacle: a decision is about 250,000 multiply-adds, so a server running dozens of seats would not notice them.
- **A deputy that plays like its player.** The warm start could learn from a player's own recorded play instead of the scripted AI's (horizon O4), if such a deputy is ever wanted.

## 17. The work, in order

The self-play plan's milestone SP2 carries it out:

- **SP2.1, the network's shape as an ADR.** The gates are decided, and this blueprint becomes decisions.
- **SP2.2, the network plays,** once Phase 3 is built. The observation, the sector encoder, the masks, the heads and their pointers, the weights file and the forward pass in `Opponent`; the AI's own draws; the switch's options for a seat's network. Checked as §11 says, in CI.
- **SP2.3, trajectories, the warm start and the trainer.** The trajectory writer, the scripted seat's records, and the trainer under `Tools/` with its self-test. Done when the warm-started network wins about half its matches against the packaged settings.
- **SP2.4, the league and its report.** The owner's run.

## References

- Andrychowicz et al., 2021. *What Matters for On-Policy Deep Actor-Critic Methods? A Large-Scale Study.* ICLR.
- Huang and Ontañón, 2022. *A Closer Look at Invalid Action Masking in Policy Gradient Algorithms.* FLAIRS.
- Ng, Harada and Russell, 1999. *Policy Invariance Under Reward Transformations: Theory and Application to Reward Shaping.* ICML.
- Schulman et al., 2016. *High-Dimensional Continuous Control Using Generalized Advantage Estimation.* ICLR.
- Schulman et al., 2017. *Proximal Policy Optimization Algorithms.* arXiv:1707.06347.
- Vinyals, Fortunato and Jaitly, 2015. *Pointer Networks.* NeurIPS.
- Vinyals et al., 2019. *Grandmaster Level in StarCraft II Using Multi-Agent Reinforcement Learning.* Nature 575.
- Zaheer et al., 2017. *Deep Sets.* NeurIPS.

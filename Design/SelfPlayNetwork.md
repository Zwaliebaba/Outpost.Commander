# The Self-Play Network — Blueprint

Status: **proposal** · 2026-10-04 · The blueprint of milestone SP2 of [the self-play plan](../GameDesign/ImplementationPlan-SelfPlay.md). Nothing here is decided until SP2.1's ADR. Five questions are the owner's, gates N1 to N5 (§15).

This document describes how the network that plays the AI's macro game would work: what it sees, what it decides, how it learns, where it sits in the code, what it costs, and what it tells the owner. [ADR-061](ADR/ADR-061-self-play-probe.md) records the first milestone, the search over the AI's numbers, and why it comes first.

---

## 1. What it is for

The owner asked on 2026-10-04 for networks that fight each other to discover the best combination of actions, as a probe of the match's rules. The opponent the game ships stays the scripted one of design §10. The owner also decided what the network decides: macro decisions and fleet tactics, with placement, the Constructors and pathing left to the scripted AI.

The first milestone answers what fixed numbers can do. The scripted AI applies the same rules whatever happens in the match: it researches in one order, answers a design with the counter its table names, and attacks once its reserve reaches a size. The network decides from the state of the match: what the enemy fields, where the territory stands, how the tickets run. If deciding from the state beats fixed rules by a wide margin, that margin and how it is won are the probe's findings. A strategy that wins too easily is a defect in the rules, or a weakness in how the scripted AI plays them, and the owner sorts which.

The network never plays in the game the players get. It plays only in the headless `--ai-matches` switch, and ADR-061 forecloses a learned AI in the shipped game without a design amendment and a new ADR.

## 2. The shape of it in one paragraph

The scripted `AiPlayer` stays the AI's body. Every 10 seconds of match time, the body builds an observation of about 200 numbers from its own snapshot and memory. A small network with two hidden layers turns the observation into six heads of choices: what to research, which design to build, whether to add a Shipyard or a Defence Platform, which sector to claim, where the main fleet goes, and whether to raid. The body carries the choices out as it carries out its own rules today. The network learns first by imitating the scripted AI, so that it starts as strong as the AI it replaces. It then improves with PPO, first against the scripted AI and then in a league of its own past versions. Training runs as a Python tool on the owner's laptop, and the game only plays matches and writes down what happened.

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
| `MacroObservation` | the observation of one decision step, a fixed array of floats |
| `ChoiceMasks` | for each head, which choices the body can carry out now |
| `MacroChoice` | one choice per head consulted, as the body reads them |
| `PolicyNetwork` | the weights, the normalization, and the forward pass from an observation and masks to each head's probabilities |
| `TrajectoryWriter` | the records of a seat's decision steps, for the trainer |

## 4. The decision step

The network decides every 10 seconds of match time: every 200 ticks at the tuning data's 20 Hz. A match of 50 minutes is 300 decisions for each seat. Between decisions the last choice stands, and the body keeps acting on it every second as it acts on its own rules today.

Ten seconds is long enough for an order to show what it does, and short enough that the network can answer an attack or a lost battle within a few steps. Fewer steps make each decision's credit easier to find: at 300 a match, the discount below still reaches across most of a match. Gate N2 asks whether 10 seconds is right.

The body's reflexes stay faster than the network. The defence reflex answers a shot on a structure within the second, as ADR-020 decision 8 has it, and closing in on a target and on a Relay stay the body's, every second.

## 5. What it sees

The observation is about 200 numbers, built at each decision step from the snapshot and from what the AI remembers:

| Group | Features | Count |
|---|---|---|
| Clock | minutes played ÷ 60; which stage of the match, one-hot: before 5 minutes, 5 to 15, 15 to 30, after 30 | 5 |
| Economy | Ore, on a log scale; income; rigs working; rigs on dry asteroids; Constructors; Shipyards built; Shipyards planned and not built; asteroids it knows still hold ore | 8 |
| Research | each of the 25 topics researched; the Lab busy; its progress; the tier opened, one-hot of 3 | 30 |
| Its fleet | warships by hull (3), by drive (3) and by weapon (5); how many are in the reserve, the attack group and a raid; their hit points as a share of full; scouts; warships queued | 17 |
| The enemy's fleet | warships seen in the last minute by hull, drive and weapon (11); the same since the match began, fading with a half-life of 3 minutes (11); seconds since one was last seen | 23 |
| Structures | its own by kind (6); the enemy's seen or remembered by kind (6) | 12 |
| Territory | for each of the 9 sectors: held by it, by the enemy, or free (3); suppressed; cut off; next to its territory; its warships there; enemy warships seen there in the last minute | 72 |
| Domination | its tickets and the enemy's, as shares of the starting tickets; the nodes each holds | 4 |
| Its last choice | its fleet's posture, one-hot of 11; its production design's hull, drive and weapon | 22 |
| Pressure | shots on its structures in the last 10 seconds; its warships and the enemy's lost in the last minute | 3 |
| | | **196** |

**Each seat sees the map from its own corner.** The repository's map is point-symmetric, with the two homes in opposite corners. The sectors are numbered as seen from the AI's home, so player 2's Northeast is what player 1 calls its Southwest: the sector in row *r* and column *c* of one seat's 3×3 grid is in row 2 − *r* and column 2 − *c* of the other's. Positions are never inputs; sectors are. One set of weights so plays both seats, and a seat bias (ADR-038) has nowhere to hide in it. A map without that symmetry needs its own frame of reference, and the network would have to be trained again.

**Counts are scaled, then standardized.** A count is divided by a fixed scale that puts it near 0 to 1, such as warships ÷ 100. The trainer then measures each feature's mean and spread over the warm start's data (§9), and those are frozen into the weights file, so that the game standardizes exactly as training did.

**The network has no memory of its own.** What a player would remember is in the observation: the enemy's fleet seen over the last minute and fading since, its remembered structures, its last choice. A recurrent layer is the next step only if the measurements show the network losing for want of memory.

## 6. What it decides

Six heads, 123 choices in all. A head is consulted only when its decision is open, and a mask removes the choices the body cannot carry out:

| Head | Choices | Consulted | Masked out | The scripted rule it replaces |
|---|---|---|---|---|
| Research | the 25 topics | when the Lab stands and is idle | researched, or prerequisites not done | the research order (ADR-020) |
| Production | 45 designs: 3 hulls × 3 drives × 5 weapons | every step | a component not unlocked | the counters, reviewed every 60 s (ADR-020 decision 8) |
| Investment | nothing; a Shipyard; a Defence Platform at home; one by its front Relay | every step | one of the same kind still waiting to be built, or no place for it | a Shipyard per income share; platforms per Shipyard and per front Relay (ADR-020, ADR-041) |
| Territory | no claim, or one of the 9 sectors | every step | a sector not free or not next to its territory, or a claim still waiting for its Relay | the nearest free sector, one at a time (ADR-020 decision 13) |
| Posture | hold home; hold the front; attack one of the 9 sectors | every step | a sector where it has seen or remembers no enemy structure, apart from the enemy's home | the attack group's size and lead, the fall-back and the regroup (ADR-041) |
| Raid | no raid, or one of the 9 sectors with 2, 4 or 8 ships | when no raid is under way | a sector the enemy does not hold, the enemy's home, or more ships than half the reserve | the raid rule (ADR-020 decision 13) |

**How the body carries the choices out:**

- **Research** queues the topic at the Lab, as the order's walk does today.
- **Production** becomes the design the Shipyards keep queued. The body saves the design when the player does not have it, as it does today.
- **Investment** adds a slot to the base plan, placed as the scripted AI places one today. A Shipyard is planned whatever the income.
- **Territory** plans the Relay and the rigs of the chosen sector, as the claim rule plans the nearest.
- **Posture.** *Attack sector s* sends the reserve to join the attack group and attack-moves the group on the best structure it knows in that sector, production first (ADR-037), or on the sector's node when it knows none. *Hold the front* and *hold home* send the reserve to hold that sector with a standing order (ADR-059). A hold chosen while an attack is under way calls the attack group back to the rally, so a fall-back is a hold.
- **Raid** sends that many of the reserve's smallest hulls on the sector, and the body runs the raid as it runs one today, closing on the Relay.

**What stays the body's:** where every structure goes, the Constructors and repair, keeping the Shipyards' queues full, the scout and its tour, the defence reflex, the pathing and formation of every order, which structure an attack goes for within its sector, and closing in.

**What of `Opponent.json` still matters.** The body still reads `constructors`, `homeAsteroids`, `contestedAsteroids`, `shipyardQueueJobs`, `structureGapMeters`, `rallyDistanceMeters`, `defenseHoldSeconds`, `raidLossShare`, `scouts` and `scoutDesign`. The rest are the rules the heads replace. A network seat plays on its seat's settings for the former, so it can play on SP1's champion's body.

**The heads are drawn independently** given the layers they share, and a step's probability is the product of the consulted heads' probabilities. Choices that depend on each other, such as a raid and an attack on the same sector, are left for the body to reconcile: a raid takes ships from the reserve before the posture moves the rest.

## 7. The network

| Layer | Shape | Parameters |
|---|---|---|
| Input | 196, standardized | — |
| Hidden 1 | 196 → 256, tanh | 50,432 |
| Hidden 2 | 256 → 256, tanh | 65,792 |
| Heads | 256 → 25, 45, 4, 10, 11, 28, one linear layer each | 31,611 |
| Value | 256 → 1 | 257 |
| | | **148,092** |

About 150,000 parameters, 0.6 MB as floats. The initialization and the activations follow the defaults that the large study of on-policy methods by Andrychowicz et al. (2021) found to work: orthogonal weights, tanh, and the last layer of each head scaled down to 0.01 so that the first choices are near uniform.

**The forward pass in the game** is plain C++ over arrays of floats, in a fixed order: about 150,000 multiply-adds, done once every 10 seconds for each seat that has a network. The game is compiled `/fp:precise` with no contraction (R16), so the same binary gives the same probabilities for the same observation, which is what a replay needs (ADR-009). A masked choice's logit becomes minus infinity before the softmax, so it has probability zero. Masking the invalid choices, rather than letting the network pick them and fail, is what Huang and Ontañón (2022) found to scale.

## 8. How a choice is drawn

**In training and in the probe's report, each head's choice is drawn from its probabilities.** The draws come from a generator of the AI's own, seeded from the match's seed and the seat. The switch hands the seed to the AI, since the server does not; the AI learns nothing about the match from it. A match with a network so reproduces from its seed, the two settings files and the two weights files, on one binary.

`Neuron::Random`, the generator ADR-009 pins, lives in `NeuronServer`, which the AI may not include (ADR-002). Gate N4 asks whether it moves to `NeuronCore`, where both may use it, or the AI keeps a copy.

**A greedy run takes each head's likeliest choice instead.** It shows what the network prefers, and the report gives both.

## 9. How it learns

### The reward

- **A match's outcome is the reward:** 1 for a win, -1 for a loss, 0 for a draw, and at the time limit half a point to the side ahead on tickets, as ADR-061 decision 4 scores a match.
- **Each step adds a shaping term** from the tickets: *r* = γΦ(*s′*) − Φ(*s*), with Φ = 0.5 × (its tickets − the enemy's) ÷ the starting tickets. Shaping of this form leaves the best policy unchanged (Ng, Harada and Russell, 1999), and it gives a signal every step of a long match.
- **Nothing else is rewarded.** Rewarding kills, Ore or nodes would teach the network what the scripted AI already believes. A probe of the rules is told only who wins.
- **The discount is 0.995 a step,** a half-life of 138 steps or 23 minutes, and the advantages are generalized advantage estimates with λ = 0.95 (Schulman et al., 2016).

### Stage 1: the warm start

A network with random weights researches at random and attacks at random. It would lose every match to the scripted AI, and a reward that is always -1 teaches nothing. So the network first learns to do what the scripted AI does.

- The switch records, for a scripted seat, the observation at each decision step and what the scripted rules chose in the 10 seconds before it, translated into the heads' terms. For example, the topic the order's walk queued, the design the review chose, or the sector the attack went for.
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

Self-play against only its latest self cycles, as ADR-061 explains for the search, so the network trains in a league, after AlphaStar's:

- **Four matches in ten are against itself,** and both seats learn.
- **Four in ten are against a past version,** a checkpoint drawn by prioritized fictitious self-play: a checkpoint the network beats with probability *p* is drawn with weight (1 − *p*)². The versions it still loses to are the ones it plays.
- **Two in ten are against the scripted anchors,** the packaged settings and SP1's champions, so that it does not forget how to beat them.
- **A checkpoint joins the league every 10 iterations.** Every match is played from both seats in turn, and the league keeps an Elo rating for each checkpoint from all its matches.

## 10. The loop and its files

An iteration of the trainer:

1. It writes the network's weights file.
2. It plays the iteration's matches through `--ai-matches`, one match a process, as many at once as the machine has cores, as `Tools/SelfPlay.py` does. Each match is told which settings and which weights each seat plays, and where to write each network seat's trajectory.
3. It reads the trajectories, computes the returns and advantages, and updates the network.

**The weights file** is little-endian. It holds:

- a tag and a version;
- the observation's size, the heads' sizes and the hidden layers' sizes;
- the observation's means and spreads;
- each layer's weights, row by row, and biases, as 32-bit floats.

`PolicyNetwork` reads it as strictly as `LoadAiSettings` reads its file. A size that does not match what the game's code builds is refused, and the error names the file, so a network trained for another observation cannot play by mistake. Weights are the products of a run, kept in its output folder. None is committed, since no build of the game reads one.

**The trajectory file** holds one record for each decision step of one seat:

- its tick and its observation;
- for each head, whether it was consulted, its mask, the choice and the choice's log-probability;
- the value the network estimated, and both sides' tickets for the shaping term.

A last record holds the outcome. A scripted seat's records hold the scripted rules' choices with no probabilities, for the warm start.

## 11. How it is checked

- **In `GameLogicTests`, in CI:**
  - the observation of a scripted snapshot;
  - mirrored seats see mirrored observations;
  - every mask removes what the server would refuse;
  - a weights file reads back as written, and a wrong size is refused;
  - a forward pass matches values computed outside the game for a small network written into the test;
  - a network with random weights plays a whole match on the real server;
  - the same seed and weights log the same match twice.
- **In the trainer's `--self-test`:**
  - PPO learns a contextual bandit and a short chain of states;
  - the warm start fits a known policy;
  - a trajectory written by the game reads back.
- **In the container,** against the stand-in, as for SP1: a short training run, end to end.

## 12. What it costs

The only measured figure is the match: about 15 CPU seconds for a match of 45 minutes between scripted AIs, in the Linux container with clang at `-O2` (ADR-061). The network adds two forward passes every 10 seconds, about 90 million multiply-adds a match, which is milliseconds. Everything below is an estimate from that figure:

| Stage | Matches | CPU hours | On 8 cores |
|---|---|---|---|
| Warm start, scripted matches recorded | 2,000 | 8 | 1 hour |
| PPO against the scripted AI, 200 iterations | 12,800 | 53 | 7 hours |
| The league, 800 iterations | 51,200 | 213 | 27 hours |
| The report, 80 matches against each of three opponents | 240 | 1 | 8 minutes |
| **In all** | **66,000** | **275** | **35 hours** |

An iteration of the league yields about 27,000 decision steps, since a match against itself teaches from both seats. A thousand iterations are about 27 million steps. That is far fewer than AlphaStar or OpenAI Five used, for a far smaller problem: the observation is engineered, the action space has 123 choices, and the scripted body does all the work below the decisions.

The update itself is small. Four passes over an iteration's steps through 150,000 parameters is about 10¹¹ floating-point operations, seconds on a laptop's CPU. A GPU buys nothing at this size.

Gate N5 sets the hours. An earlier limit, 45 minutes rather than 120, in the first iterations shortens the matches that stall, with the tickets deciding at the limit.

## 13. What the probe reports

- **How much it wins.** The final network against the packaged settings and against SP1's champion, on seeds 1 to 40 from both seats, with 95% intervals, sampled and greedy.
- **What it does differently.** From the match logs: research timings, designs by 5-minute window, peak fleets and endings, through `Tools/MatchLog.py`. From its trajectories: how often it chose each research order, design, posture, claim and raid at each stage of the match, beside what the scripted rules chose in the same matches.
- **Which decision carries its advantage.** Each head in turn is handed back to the scripted rule and the win rate measured again. The head whose return costs the most is where the advantage lies, and it points at the rule being exploited: a design that wins too much, an attack timing the defences cannot answer, a claim the tickets reward too much.
- **Whether it is brittle.** A fresh network, an exploiter, trains for 100 iterations against the frozen final network alone, as AlphaStar's exploiters did. If the exploiter wins easily, the final network's strategy is narrow, and its findings are read with that in mind.

Each finding is sorted by the owner, as in SP1.3: a rule that wants changing, a weakness of the scripted AI, or neither.

## 14. What could go wrong

- **It cycles,** chasing its own latest strategy round the triangle. *Answer:* the league, prioritized fictitious self-play, and the scripted anchors (§9).
- **It finds the body's bugs rather than the rules' defects.** An attack the pathing cannot defend, say. *Answer:* the ablations point at the decision, and the owner sorts the finding. A weakness of the scripted AI is still worth knowing.
- **It cannot tell which decision won a 50-minute match.** *Answer:* 10-second steps, generalized advantage estimates, the tickets' shaping, and the warm start's head start.
- **A run outgrows the laptop.** *Answer:* the costs above, the warm start, an earlier time limit early on, and `--resume` after every iteration, as `Tools/SelfPlay.py` saves after every generation.
- **Results differ between builds.** Floats are only promised to replay on one binary (ADR-009). *Answer:* train and measure on Release|x64 (ADR-061 decision 7). Weights move between builds; outcomes do not reproduce bit for bit across them.
- **It fits the one map.** *Answer:* none is wanted. The probe answers for the repository's map, and another map is another probe.

## 15. The owner's questions

| Gate | Question | Recommendation |
|---|---|---|
| N1 | The trainer's library: PyTorch on the CPU, or NumPy with the gradients written by hand. Either is a tool's dependency that the game never links (R14). | PyTorch. Its gradients and its optimizer are used and tested far more widely than code written for this tool would be. |
| N2 | The decision step: 5, 10 or 30 seconds. | 10 seconds (§4). |
| N3 | The heads: the six of §6, with the defence reflex, the scout and placement left to the body; or the defence as a seventh head. | The six. The defence reflex is a reaction within a second, not a decision. |
| N4 | The AI's random draws: `Neuron::Random` moved from `NeuronServer` to `NeuronCore`, or a copy in `Opponent`. | The move. One generator, already pinned by `RandomTests`. |
| N5 | The budget: how many hours of the laptop a run gets. | 35 hours for the plan of §12, run over several nights with `--resume`. |

## 16. The work, in order

The self-play plan's milestone SP2 carries it out:

- **SP2.1, the network's shape as an ADR.** The gates are decided, and this blueprint becomes decisions.
- **SP2.2, the network plays.** The observation, the masks, the heads, the weights file and the forward pass in `Opponent`; the AI's own draws; the switch's options for a seat's network. Checked as §11 says, in CI.
- **SP2.3, trajectories, the warm start and the trainer.** The trajectory writer, the scripted seat's records, and the trainer under `Tools/` with its self-test. Done when the warm-started network wins about half its matches against the packaged settings.
- **SP2.4, the league and its report.** The owner's run.

## References

- Andrychowicz et al., 2021. *What Matters for On-Policy Deep Actor-Critic Methods? A Large-Scale Study.* ICLR.
- Huang and Ontañón, 2022. *A Closer Look at Invalid Action Masking in Policy Gradient Algorithms.* FLAIRS.
- Ng, Harada and Russell, 1999. *Policy Invariance Under Reward Transformations: Theory and Application to Reward Shaping.* ICML.
- Schulman et al., 2016. *High-Dimensional Continuous Control Using Generalized Advantage Estimation.* ICLR.
- Schulman et al., 2017. *Proximal Policy Optimization Algorithms.* arXiv:1707.06347.
- Vinyals et al., 2019. *Grandmaster Level in StarCraft II Using Multi-Agent Reinforcement Learning.* Nature 575.

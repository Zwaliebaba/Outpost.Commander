#!/usr/bin/env python3
"""Self-play over the AI's settings: a probe of the match's rules (ADR-063).

Two AIs fight, and a search keeps whichever settings of OutpostCommander/Assets/Opponent.json win. What it finds is a
question for the owner: a strategy that wins too easily is a degenerate one in the rules, or a weakness in how the
scripted AI plays them. The packaged settings are not changed by this; that is the owner's decision.

The AI's body stays the scripted one: where it builds, how its Constructors work, how its fleet moves. The search
moves only its numbers, the knobs below, which are what the scripted AI decides with:

  - the 24 numbers of Opponent.json, each in a range of its own (KNOBS), whole numbers kept whole;
  - its research order, as one key per topic: the topics are researched in the order of their keys.

The counters, the default design and the scout's design stay the packaged ones.

The search is a separable CMA-ES (Ros and Hansen, 2008), which learns a step size for each knob but not how knobs
move together: with about fifty dimensions and a hundred generations it adapts where a full covariance matrix would
still be learning. Each knob is searched in [0, 1], reflected at its bounds, so that the search runs unbounded.

Each generation, every candidate plays the champion, which starts as the packaged settings, and one member of the hall
of fame, a past champion drawn at random once there is one. Both seats, on the same seeds for every candidate, so that
two candidates are compared on the same matches: a match reproduces from its seed and the two settings on one build
(ADR-009). The seeds move on each generation, so that nothing is tuned to a few maps' worth of luck. A win scores 1, a
loss -1 and a draw 0; a match still going at the limit scores half a point to the side ahead on tickets. A candidate's
fitness is its mean score.

The generation's best candidate, if it scored at least --challenge against the champion, plays the champion again on
fresh seeds, both seats; at --promote or more there, it becomes the champion and the old one joins the hall.

At the end the champion plays the packaged settings on seeds 1 to --report-seeds, both seats, which no search match
uses: its win share there is how much the scripted AI's numbers leave on the table. The report lists the champion's
numbers beside the packaged ones and what each side did in those matches.

The matches are played by the game's --ai-matches switch, one match a process, as many at once as --workers says. The
default executable is the Release|x64 build's layout, which holds the Assets folder the game reads.

Usage:
  python Tools/SelfPlay.py                       search, from scratch, into the output folder
  python Tools/SelfPlay.py --resume              go on from the last finished generation, with the search's own options;
                                                 only --generations, --workers, --exe and --keep-logs apply
  python Tools/SelfPlay.py --report              play and write the report for the champion the search has
  python Tools/SelfPlay.py --self-test           check the search itself, without the game

The output folder, OutpostCommander-selfplay in the temporary folder by default, holds state.pickle to resume from,
history.csv with a line for each generation, champion-<n>.json for each champion, and report.txt. Ctrl+C stops it
after the matches under way; --resume repeats the generation it stopped in.
"""

import argparse
import collections
import concurrent.futures
import copy
import csv
import dataclasses
import json
import math
import os
import pickle
import random
import statistics
import subprocess
import sys
import tempfile
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from MatchLog import clock, read_matches, summarize  # noqa: E402

REPO_ROOT = Path(__file__).resolve().parents[1]
PACKAGED_SETTINGS = REPO_ROOT / "OutpostCommander" / "Assets" / "Opponent.json"
PACKAGED_TUNING = REPO_ROOT / "OutpostCommander" / "Assets" / "Tuning.json"
# Where the Release|x64 build puts the game: the package layout first, which holds the Assets folder beside the
# executable, and the bare output folder in case the layout lives there.
EXE_CANDIDATES = (REPO_ROOT / "x64" / "Release" / "OutpostCommander" / "AppX" / "OutpostCommander.exe",
                  REPO_ROOT / "x64" / "Release" / "OutpostCommander" / "OutpostCommander.exe")
OUTPUT_NAME = "OutpostCommander-selfplay"
STATE_NAME = "state.pickle"
# 2: secondSlotTier's range moved, so a point saved by version 1 decodes to other settings.
STATE_VERSION = 2

# Seeds: each generation takes the next block from SEARCH_SEEDS, each confirmation from CONFIRM_SEEDS. The report plays
# seeds 1 to --report-seeds, the ones the ADRs measure on, which the search never plays.
SEARCH_SEEDS = 1_000
CONFIRM_SEEDS = 1_000_000
CONFIDENCE_Z = 1.96


@dataclasses.dataclass(frozen=True)
class Knob:
  """One number of Opponent.json the search moves, in a range that holds the packaged value. The loader's own bounds
  (Opponent/AiSettings.cpp) are wider: these keep the search to values a player would try."""
  name: str
  low: float
  high: float
  whole: bool

  def value(self, position):
    """The knob's value at a position in [0, 1]. A whole knob gives each of its numbers an equal share."""
    if self.whole:
      count = int(self.high - self.low) + 1
      return int(self.low) + min(int(position * count), count - 1)
    return round(self.low + position * (self.high - self.low), 3)

  def position(self, value):
    """Where in [0, 1] a value lies: a whole number at the middle of its share."""
    if self.whole:
      return (value - self.low + 0.5) / (self.high - self.low + 1)
    return (value - self.low) / (self.high - self.low)

  def step(self):
    """A whole knob's share of [0, 1], and 0 for one that is not whole."""
    return 1 / (self.high - self.low + 1) if self.whole else 0.0


KNOBS = (
  Knob("attackGroupShips", 4, 80, True),
  Knob("attackGroupGrowthPerTier", 0, 40, True),
  Knob("retreatLossShare", 0.0, 0.9, False),
  Knob("regroupSeconds", 0.0, 600.0, False),
  Knob("reviewIntervalSeconds", 10.0, 240.0, False),
  Knob("constructors", 1, 10, True),
  Knob("incomePerShipyardOrePerSecond", 4.0, 40.0, False),
  Knob("homeAsteroids", 1, 5, True),
  Knob("contestedAsteroids", 0, 8, True),
  Knob("homePlatformsPerShipyard", 0, 5, True),
  Knob("shipyardQueueJobs", 1, 5, True),
  Knob("defenseHoldSeconds", 2.0, 60.0, False),
  Knob("structureGapMeters", 20.0, 120.0, False),
  Knob("rallyDistanceMeters", 60.0, 600.0, False),
  Knob("scouts", 0, 3, True),
  Knob("raidShips", 0, 12, True),
  Knob("raidLossShare", 0.0, 0.9, False),
  Knob("raidIntervalSeconds", 30.0, 600.0, False),
  Knob("attackNodeLead", 0, 4, True),
  Knob("attackWithoutLeadShare", 1.0, 3.0, False),
  Knob("frontPlatforms", 0, 3, True),
  Knob("claimSectors", 0, 4, True),
  # The Lab's level with a second slot comes after the level that opens tier 3 (Tuning.json), so tier 3 is open whenever
  # that level is next, and every tier up to 3 plays alike: 3 takes the second slot and 4 never does. The self-test
  # holds this range to the tuning data.
  Knob("secondSlotTier", 3, 4, True),
  Knob("attackLevelMeters", 0.0, 1500.0, False),
)
# The research keys start this much closer together than the knobs, relative to the step size: a key one step away
# moves a topic several places in the order.
RESEARCH_KEY_SCALE = 0.5
# A whole knob's step never falls below this share of one number, so that its neighbors are still tried; and no
# knob's step grows past half its range, beyond which a sample says nothing about where it came from.
WHOLE_STEP_FLOOR = 0.25
STEP_CEILING = 0.5


def fail(message):
  print(f"SelfPlay: {message}", file=sys.stderr)
  sys.exit(2)


# ---- The search ------------------------------------------------------------------------------------------------------

class SepCmaEs:
  """Separable CMA-ES (Ros and Hansen, 2008, "A Simple Modification in CMA-ES Achieving Linear Time and Space
  Complexity"): CMA-ES with a diagonal covariance matrix, whose learning rates are raised by (n + 2) / 3 as the paper
  has it. It minimizes. The rest of the parameters are Hansen's defaults ("The CMA Evolution Strategy: A Tutorial").

  min_steps and max_steps bound each coordinate's step, sigma times the square root of its variance."""

  def __init__(self, mean, sigma, scales, seed, population=None, min_steps=None, max_steps=None):
    n = len(mean)
    self.n = n
    self.mean = list(mean)
    self.sigma = sigma
    self.variances = [scale * scale for scale in scales]
    self.population = population or 4 + int(3 * math.log(n))
    self.min_steps = list(min_steps) if min_steps else [0.0] * n
    self.max_steps = list(max_steps) if max_steps else [math.inf] * n
    parents = self.population // 2
    raw = [math.log(parents + 0.5) - math.log(rank + 1) for rank in range(parents)]
    self.weights = [weight / sum(raw) for weight in raw]
    self.mu_eff = 1 / sum(weight * weight for weight in self.weights)
    self.c_sigma = (self.mu_eff + 2) / (n + self.mu_eff + 5)
    self.d_sigma = 1 + 2 * max(0.0, math.sqrt((self.mu_eff - 1) / (n + 1)) - 1) + self.c_sigma
    self.c_c = (4 + self.mu_eff / n) / (n + 4 + 2 * self.mu_eff / n)
    c_1 = 2 / ((n + 1.3) ** 2 + self.mu_eff)
    c_mu = min(1 - c_1, 2 * (self.mu_eff - 2 + 1 / self.mu_eff) / ((n + 2) ** 2 + self.mu_eff))
    self.c_1 = min(1.0, c_1 * (n + 2) / 3)
    self.c_mu = min(1 - self.c_1, c_mu * (n + 2) / 3)
    self.chi_n = math.sqrt(n) * (1 - 1 / (4 * n) + 1 / (21 * n * n))
    self.path_sigma = [0.0] * n
    self.path_c = [0.0] * n
    self.generation = 0
    self.random = random.Random(seed)
    self._bound_steps()

  def ask(self):
    """The next generation's candidates."""
    steps = [self.sigma * math.sqrt(variance) for variance in self.variances]
    return [[mean + step * self.random.gauss(0.0, 1.0) for mean, step in zip(self.mean, steps)]
            for _ in range(self.population)]

  def tell(self, candidates, losses):
    """Moves the search toward the candidates with the smallest losses."""
    n = self.n
    order = sorted(range(len(candidates)), key=lambda index: (losses[index], index))
    best = [[(value - mean) / self.sigma for value, mean in zip(candidates[index], self.mean)]
            for index in order[:len(self.weights)]]
    shift = [sum(weight * y[i] for weight, y in zip(self.weights, best)) for i in range(n)]
    self.mean = [mean + self.sigma * step for mean, step in zip(self.mean, shift)]
    whitened = [step / math.sqrt(variance) for step, variance in zip(shift, self.variances)]
    rate = math.sqrt(self.c_sigma * (2 - self.c_sigma) * self.mu_eff)
    self.path_sigma = [(1 - self.c_sigma) * path + rate * step for path, step in zip(self.path_sigma, whitened)]
    length = math.sqrt(sum(path * path for path in self.path_sigma))
    self.generation += 1
    # The tutorial's h_sigma: while the step size's path is long, the covariance's path holds still, so that the axes do
    # not grow along with the step.
    holding = length / math.sqrt(1 - (1 - self.c_sigma) ** (2 * self.generation)) >= (1.4 + 2 / (n + 1)) * self.chi_n
    rate = 0.0 if holding else math.sqrt(self.c_c * (2 - self.c_c) * self.mu_eff)
    self.path_c = [(1 - self.c_c) * path + rate * step for path, step in zip(self.path_c, shift)]
    keep = 1 - self.c_1 - self.c_mu + (self.c_1 * self.c_c * (2 - self.c_c) if holding else 0.0)
    for i in range(n):
      rank_mu = sum(weight * y[i] * y[i] for weight, y in zip(self.weights, best))
      self.variances[i] = keep * self.variances[i] + self.c_1 * self.path_c[i] ** 2 + self.c_mu * rank_mu
    self.sigma *= math.exp((self.c_sigma / self.d_sigma) * (length / self.chi_n - 1))
    self._bound_steps()

  def steps(self):
    return [self.sigma * math.sqrt(variance) for variance in self.variances]

  def _bound_steps(self):
    for i, step in enumerate(self.steps()):
      bounded = min(max(step, self.min_steps[i]), self.max_steps[i])
      if bounded != step:
        self.variances[i] = (bounded / self.sigma) ** 2


def reflect(coordinate):
  """Folds a coordinate of the unbounded search into [0, 1], reflecting it at 0 and 1."""
  folded = math.fmod(coordinate, 2.0)
  if folded < 0.0:
    folded += 2.0
  return 2.0 - folded if folded > 1.0 else folded


def dimensions(base):
  return len(KNOBS) + len(base["researchOrder"])


def decode(point, base):
  """The settings at a point of the search: base's, with every knob and the research order taken from the point."""
  positions = [reflect(coordinate) for coordinate in point]
  settings = copy.deepcopy(base)
  for knob, position in zip(KNOBS, positions):
    settings[knob.name] = knob.value(position)
  topics = base["researchOrder"]
  keys = positions[len(KNOBS):]
  settings["researchOrder"] = [topic for _, _, topic in sorted(zip(keys, range(len(topics)), topics))]
  return settings


def encode(settings, base):
  """The point whose settings are these, for settings whose research order holds base's topics."""
  point = [knob.position(settings[knob.name]) for knob in KNOBS]
  places = {topic: place for place, topic in enumerate(settings["researchOrder"])}
  topics = base["researchOrder"]
  return point + [(places[topic] + 0.5) / len(topics) for topic in topics]


def check_settings(settings, base):
  """The loader's rules (Opponent/AiSettings.cpp) for what the search writes, and its own ranges: a list of problems."""
  problems = []
  for knob in KNOBS:
    value = settings[knob.name]
    if knob.whole and not isinstance(value, int):
      problems.append(f"{knob.name} is {value!r}, not a whole number")
    if not knob.low <= value <= knob.high:
      problems.append(f"{knob.name} is {value}, outside {knob.low} to {knob.high}")
  for share in ("retreatLossShare", "raidLossShare"):
    if not 0 <= settings[share] < 1:
      problems.append(f"{share} is {settings[share]}, not a share below 1")
  for positive in ("reviewIntervalSeconds", "incomePerShipyardOrePerSecond", "defenseHoldSeconds", "rallyDistanceMeters",
                   "attackWithoutLeadShare"):
    if settings[positive] <= 0:
      problems.append(f"{positive} is {settings[positive]}, not positive")
  if not 1 <= settings["shipyardQueueJobs"] <= 5:
    problems.append(f"shipyardQueueJobs is {settings['shipyardQueueJobs']}, not 1 to 5")
  if sorted(settings["researchOrder"]) != sorted(base["researchOrder"]):
    problems.append("researchOrder is not the packaged topics in another order")
  return problems


def new_search(base, sigma, population, seed):
  scales = [1.0] * len(KNOBS) + [RESEARCH_KEY_SCALE] * len(base["researchOrder"])
  min_steps = [WHOLE_STEP_FLOOR * knob.step() for knob in KNOBS] + [0.0] * len(base["researchOrder"])
  return SepCmaEs(encode(base, base), sigma, scales, seed, population, min_steps, [STEP_CEILING] * dimensions(base))


# ---- Playing matches -------------------------------------------------------------------------------------------------

@dataclasses.dataclass(frozen=True)
class Job:
  """One match: player 1's settings file, player 2's, and the seed."""
  first: Path
  second: Path
  seed: int


class Runner:
  """Plays matches with the game's --ai-matches switch, one match a process, --workers at once."""

  def __init__(self, exe, workers, limit_minutes, folder, keep_logs=False):
    self.exe = exe
    self.workers = workers
    self.limit_minutes = limit_minutes
    self.folder = folder
    self.keep_logs = keep_logs
    self.played = 0

  def play(self, jobs):
    """The matches, in the order of the jobs, as MatchLog.py reads them."""
    self.folder.mkdir(parents=True, exist_ok=True)
    pool = concurrent.futures.ThreadPoolExecutor(max_workers=self.workers)
    try:
      futures = [pool.submit(self._play, job, index) for index, job in enumerate(jobs)]
      matches = [future.result() for future in futures]
    except BaseException:
      # Ctrl+C or a failed match: start no more. The processes under way finish their one match each.
      pool.shutdown(wait=False, cancel_futures=True)
      raise
    pool.shutdown()
    self.played += len(jobs)
    return matches

  def _play(self, job, index):
    log = self.folder / f"match-{index}.log"
    command = [str(self.exe), "--ai-matches", "--quiet", "--ai1", str(job.first), "--ai2", str(job.second),
               "--first-seed", str(job.seed), "--matches", "1", "--limit-minutes", str(self.limit_minutes), "--log", str(log)]
    # Below normal priority on Windows, so that the machine stays usable through a long search.
    flags = subprocess.BELOW_NORMAL_PRIORITY_CLASS if os.name == "nt" else 0
    result = subprocess.run(command, capture_output=True, text=True, creationflags=flags)
    if result.returncode != 0:
      detail = result.stderr.strip() or f"exit code {result.returncode}"
      raise RuntimeError(f"the match of seed {job.seed} between {job.first.name} and {job.second.name} failed: {detail}")
    matches = read_matches(log)
    if len(matches) != 1 or matches[0].seed != job.seed:
      raise RuntimeError(f"{log} does not hold the one match of seed {job.seed}")
    if not self.keep_logs:
      log.unlink()
    return matches[0]


def score(match, player):
  """The match from one player's side: 1 a win, -1 a loss, 0 a draw; still going at the limit, half a point to the side
  ahead on tickets, and 0 when neither is or the map has none."""
  if match.winner is not None:
    return 0.0 if match.winner == 0 else (1.0 if match.winner == player else -1.0)
  latest = {}
  for _, owner, tickets in match.tickets:
    latest[owner] = tickets
  mine, theirs = latest.get(player), latest.get(3 - player)
  if mine is None or theirs is None or mine == theirs:
    return 0.0
  return 0.5 if mine > theirs else -0.5


def both_seats(candidate, opponent, seeds):
  """The jobs of one pairing, and the candidate's player in each: every seed, with the candidate first and second."""
  jobs = []
  for seed in seeds:
    jobs.append((Job(candidate, opponent, seed), 1))
    jobs.append((Job(opponent, candidate, seed), 2))
  return jobs


def write_settings(path, settings):
  path.parent.mkdir(parents=True, exist_ok=True)
  path.write_text(json.dumps(settings, indent=2) + "\n", encoding="utf-8")
  return path


def find_exe(named):
  """The game, which must have its Assets folder beside it, as the package layout does."""
  candidates = [Path(named)] if named else list(EXE_CANDIDATES)
  for exe in candidates:
    if exe.is_file() and (exe.parent / "Assets" / "Tuning.json").is_file():
      return exe.resolve()
  looked = "\n  ".join(str(exe) for exe in candidates)
  fail(f"no game with an Assets folder beside it at:\n  {looked}\nBuild Release|x64 first, or name the game with --exe.")


# ---- The run ---------------------------------------------------------------------------------------------------------

def new_state(arguments, base):
  return {
    "version": STATE_VERSION,
    "options": {"seeds": arguments.seeds, "population": arguments.population, "sigma": arguments.sigma,
                "limit_minutes": arguments.limit_minutes, "search_seed": arguments.search_seed,
                "challenge": arguments.challenge, "promote": arguments.promote, "confirm_seeds": arguments.confirm_seeds},
    "search": new_search(base, arguments.sigma, arguments.population, arguments.search_seed),
    "random": random.Random(arguments.search_seed + 1),
    "generation": 0,
    "confirmations": 0,
    "base": base,
    "champion": base,
    "champion_number": 0,
    "hall": [],
    "promotions": [],
  }


def save_state(folder, state):
  temporary = folder / (STATE_NAME + ".tmp")
  with open(temporary, "wb") as file:
    pickle.dump(state, file)
  os.replace(temporary, folder / STATE_NAME)


def load_state(folder):
  path = folder / STATE_NAME
  if not path.is_file():
    fail(f"no search to go on with in {folder}: {STATE_NAME} is missing.")
  with open(path, "rb") as file:
    state = pickle.load(file)
  if state.get("version") != STATE_VERSION:
    fail(f"{path} was written by another version of this script.")
  return state


def append_history(folder, row):
  path = folder / "history.csv"
  new = not path.is_file()
  with open(path, "a", newline="", encoding="utf-8") as file:
    writer = csv.DictWriter(file, fieldnames=list(row))
    if new:
      writer.writeheader()
    writer.writerow(row)


def preflight(runner, folder, base):
  """One minute of one match, so that a game that cannot run says so before the search starts."""
  packaged = write_settings(folder / "work" / "packaged.json", base)
  quick = Runner(runner.exe, 1, 1, folder / "work")
  quick.play([Job(packaged, packaged, 1)])


def run_generation(state, runner, folder):
  options = state["options"]
  search = state["search"]
  generation = state["generation"]
  work = folder / "work"
  points = search.ask()
  candidates = [decode(point, state["base"]) for point in points]
  for candidate in candidates:
    problems = check_settings(candidate, state["base"])
    if problems:
      raise RuntimeError("a candidate breaks the loader's rules: " + "; ".join(problems))
  files = [write_settings(work / f"candidate-{index}.json", candidate) for index, candidate in enumerate(candidates)]
  opponents = [("champion", write_settings(work / "champion.json", state["champion"]))]
  hall_member = None
  if state["hall"]:
    hall_member = state["random"].randrange(len(state["hall"]))
    opponents.append(("hall", write_settings(work / "hall.json", state["hall"][hall_member])))
  first_seed = SEARCH_SEEDS + generation * options["seeds"]
  seeds = range(first_seed, first_seed + options["seeds"])

  jobs = []
  for index, file in enumerate(files):
    for role, opponent in opponents:
      jobs += [(index, role, job, player) for job, player in both_seats(file, opponent, seeds)]
  matches = runner.play([job for _, _, job, _ in jobs])
  totals = collections.defaultdict(list)
  for (index, role, _, player), match in zip(jobs, matches):
    totals[index].append(score(match, player))
    totals[(index, role)].append(score(match, player))
  fitness = [statistics.fmean(totals[index]) for index in range(len(candidates))]
  versus_champion = [statistics.fmean(totals[(index, "champion")]) for index in range(len(candidates))]
  search.tell(points, [-value for value in fitness])

  best = max(range(len(candidates)), key=lambda index: (fitness[index], -index))
  promoted = ""
  confirmed = ""
  if versus_champion[best] >= options["challenge"]:
    first_seed = CONFIRM_SEEDS + state["confirmations"] * options["confirm_seeds"]
    state["confirmations"] += 1
    pairs = both_seats(files[best], opponents[0][1], range(first_seed, first_seed + options["confirm_seeds"]))
    played = runner.play([job for job, _ in pairs])
    confirmation = statistics.fmean(score(match, player) for match, (_, player) in zip(played, pairs))
    confirmed = f"{confirmation:.3f}"
    if confirmation >= options["promote"]:
      state["hall"].append(state["champion"])
      state["champion"] = candidates[best]
      state["champion_number"] += 1
      number = state["champion_number"]
      write_settings(folder / f"champion-{number}.json", candidates[best])
      state["promotions"].append({"generation": generation, "champion": number, "score": confirmation})
      promoted = str(number)

  state["generation"] += 1
  steps = search.steps()
  append_history(folder, {
    "generation": generation, "first_seed": seeds[0], "hall_member": "" if hall_member is None else hall_member,
    "best_fitness": f"{fitness[best]:.3f}", "mean_fitness": f"{statistics.fmean(fitness):.3f}",
    "best_versus_champion": f"{versus_champion[best]:.3f}", "confirmation": confirmed, "promoted": promoted,
    "sigma": f"{search.sigma:.4f}", "median_step": f"{statistics.median(steps):.4f}"})
  return fitness[best], versus_champion[best], confirmed, promoted


def search(arguments):
  folder = Path(arguments.out)
  base = json.loads(PACKAGED_SETTINGS.read_text(encoding="utf-8"))
  if arguments.resume:
    state = load_state(folder)
    if state["base"] != base:
      print("Note: the packaged Opponent.json has changed since this search began; the search goes on from the one it "
            "began with.")
  else:
    if (folder / STATE_NAME).is_file():
      fail(f"{folder} holds a search already: --resume it, or name another folder with --out.")
    folder.mkdir(parents=True, exist_ok=True)
    state = new_state(arguments, base)
    # Saved before any match, so that a search stopped in its first generation can be resumed too.
    save_state(folder, state)
  if state["generation"] >= arguments.generations:
    print(f"The search has run {state['generation']} generations, and --generations asks for {arguments.generations} in "
          f"all. --report plays its report again.")
    return
  runner = Runner(find_exe(arguments.exe), arguments.workers, state["options"]["limit_minutes"], folder / "work",
                  arguments.keep_logs)
  preflight(runner, folder, state["base"])
  print(f"{runner.exe}\n{runner.workers} matches at once; {dimensions(base)} dimensions, "
        f"{state['search'].population} candidates a generation, {state['options']['seeds']} seeds each, both seats.")

  started = time.monotonic()
  first = state["generation"]
  while state["generation"] < arguments.generations:
    generation = state["generation"]
    began = time.monotonic()
    best, versus, confirmed, promoted = run_generation(state, runner, folder)
    save_state(folder, state)
    taken = time.monotonic() - began
    left = (time.monotonic() - started) / (state["generation"] - first) * (arguments.generations - state["generation"])
    line = (f"Generation {generation + 1}/{arguments.generations}: best {best:+.3f}, {versus:+.3f} against the champion")
    if confirmed:
      line += f", {confirmed} on fresh seeds" + (f": champion {promoted}" if promoted else "")
    print(f"{line}. {clock(taken)}, about {clock(left)} left.", flush=True)
  print(f"Played {runner.played} matches in {clock(time.monotonic() - started)}.")
  report(state, runner, folder, arguments.report_seeds)


# ---- The report ------------------------------------------------------------------------------------------------------

def wilson(successes, trials):
  """The 95% Wilson interval of a share."""
  if trials == 0:
    return 0.0, 1.0
  share = successes / trials
  denominator = 1 + CONFIDENCE_Z ** 2 / trials
  center = (share + CONFIDENCE_Z ** 2 / (2 * trials)) / denominator
  half = CONFIDENCE_Z * math.sqrt(share * (1 - share) / trials + CONFIDENCE_Z ** 2 / (4 * trials * trials)) / denominator
  return center - half, center + half


def endings(counts):
  return ", ".join(f"{count} by {ending}" for ending, count in sorted(counts.items())) or "none"


def side_summary(name, matches, players):
  """What one side did over the report's matches: its tiers, its peak fleet and what it built most."""
  tier_two = []
  peaks = []
  built = collections.Counter()
  for match, player in zip(matches, players):
    opened = [tick for tick, owner, tier in match.tiers if owner == player and tier == 2]
    if opened:
      tier_two.append(match.seconds(min(opened)))
    if player in match.peaks:
      peaks.append(match.peaks[player][1])
    built.update(design for _, owner, design in match.built if owner == player)
  lines = [f"  {name}"]
  if tier_two:
    lines.append(f"    Tier 2 opened in {len(tier_two)} of {len(matches)}, at a median {clock(statistics.median(tier_two))}")
  else:
    lines.append(f"    Tier 2 opened in none of {len(matches)}")
  if peaks:
    lines.append(f"    Peak warships: median {statistics.median(peaks):g}, from {min(peaks)} to {max(peaks)}")
  if built:
    lines.append("    Built most: " + ", ".join(f"{design} {count}" for design, count in built.most_common(4)))
  return lines


def report(state, runner, folder, report_seeds):
  base = state["base"]
  champion = state["champion"]
  lines = [f"Self-play over Opponent.json: {state['generation']} generations, {len(state['promotions'])} champions."]
  for promotion in state["promotions"]:
    lines.append(f"  Champion {promotion['champion']} in generation {promotion['generation'] + 1}, scoring "
                 f"{promotion['score']:+.3f} against the one before on fresh seeds")
  if not state["promotions"]:
    lines.append("No candidate beat the packaged settings on fresh seeds, so there is no champion to measure.")
    write_report(folder, lines)
    return

  work = folder / "work"
  champion_file = write_settings(work / "champion.json", champion)
  packaged_file = write_settings(work / "packaged.json", base)
  pairs = both_seats(champion_file, packaged_file, range(1, report_seeds + 1))
  matches = runner.play([job for job, _ in pairs])
  players = [player for _, player in pairs]
  scores = [score(match, player) for match, player in zip(matches, players)]
  wins = sum(1 for match, player in zip(matches, players) if match.winner == player)
  losses = sum(1 for match, player in zip(matches, players) if match.winner not in (None, 0, player))
  draws = sum(1 for match in matches if match.winner == 0)
  left = sum(1 for match in matches if match.winner is None)
  low, high = wilson(wins, wins + losses)
  lines += ["", f"Champion {state['champion_number']} against the packaged settings, seeds 1 to {report_seeds}, both seats:",
            f"  {wins} won, {losses} lost, {draws} drawn, {left} still going at {runner.limit_minutes} minutes; "
            f"mean score {statistics.fmean(scores):+.3f}",
            f"  Won {wins} of the {wins + losses} decided, 95% interval {low:.0%} to {high:.0%}"]
  won = collections.Counter(match.ending for match, player in zip(matches, players) if match.winner == player)
  lost = collections.Counter(match.ending for match, player in zip(matches, players) if match.winner == 3 - player)
  lines.append(f"  The champion won {endings(won)}, and lost {endings(lost)}")
  lines += [""] + side_summary("The champion", matches, players)
  lines += side_summary("The packaged settings", matches, [3 - player for player in players])
  lines += ["", summarize(matches)]

  lines += ["", "The champion's numbers:", f"  {'knob':32} {'packaged':>10} {'champion':>10}   range"]
  for knob in KNOBS:
    marker = "" if base[knob.name] == champion[knob.name] else "  *"
    lines.append(f"  {knob.name:32} {base[knob.name]:>10g} {champion[knob.name]:>10g}   {knob.low:g} to {knob.high:g}{marker}")
  lines += ["", "Research order, packaged:  " + " ".join(str(topic) for topic in base["researchOrder"]),
            "Research order, champion:  " + " ".join(str(topic) for topic in champion["researchOrder"])]
  write_report(folder, lines)


def write_report(folder, lines):
  text = "\n".join(lines) + "\n"
  (folder / "report.txt").write_text(text, encoding="utf-8")
  print("\n" + text)


# ---- The self-test ---------------------------------------------------------------------------------------------------

def self_test():
  """Checks the search without the game: the optimizer, the encoding, the loader's rules and the scoring."""
  failures = []

  def check(condition, message):
    if not condition:
      failures.append(message)

  def ellipsoid(point):
    return sum(10 ** (4 * i / (len(point) - 1)) * value * value for i, value in enumerate(point))

  # Measured when this was written, with these seeds: 223 generations to 1e-10, 454 without the step size's adaptation.
  optimizer = SepCmaEs([3.0] * 10, 1.0, [1.0] * 10, seed=1)
  for _ in range(400):
    points = optimizer.ask()
    optimizer.tell(points, [ellipsoid(point) for point in points])
  check(ellipsoid(optimizer.mean) < 1e-10, f"the optimizer left a separable ellipsoid at {ellipsoid(optimizer.mean):.3g}")

  # A step far too small grows, before it shrinks again near the optimum: past 0.1 in 36 to 47 generations over seeds 1
  # to 5, and never without the adaptation.
  cramped = SepCmaEs([3.0] * 10, 1e-4, [1.0] * 10, seed=1)
  largest = 0.0
  for _ in range(100):
    points = cramped.ask()
    cramped.tell(points, [sum(value * value for value in point) for point in points])
    largest = max(largest, *cramped.steps())
  check(largest > 0.1, f"a step of 1e-4 grew only to {largest:.3g} in 100 generations")

  # Down a slope the covariance's path stretches the search along it: in 30 generations its variance there was 20 times
  # the others' with this seed, and 5.5 times without the path.
  sloped = SepCmaEs([0.0] * 10, 1.0, [1.0] * 10, seed=1)
  for _ in range(30):
    points = sloped.ask()
    sloped.tell(points, [point[0] for point in points])
  stretch = sloped.variances[0] / statistics.fmean(sloped.variances[1:])
  check(stretch > 10, f"down a slope the search stretched only {stretch:.3g} times along it")

  floored = SepCmaEs([0.5] * 4, 0.3, [1.0] * 4, seed=2, min_steps=[0.05] * 4, max_steps=[0.5] * 4)
  for _ in range(200):
    points = floored.ask()
    floored.tell(points, [sum(value * value for value in point) for point in points])
  check(min(floored.steps()) >= 0.05 - 1e-12, "a step fell below its floor")

  restored = pickle.loads(pickle.dumps(optimizer))
  check(restored.ask() == optimizer.ask(), "a search restored from its pickle asks something else")

  base = json.loads(PACKAGED_SETTINGS.read_text(encoding="utf-8"))
  check(decode(encode(base, base), base) == base, "the packaged settings do not decode to themselves")
  check(not check_settings(base, base), "the packaged settings break the search's ranges: "
        + "; ".join(check_settings(base, base)))
  draw = random.Random(3)
  for _ in range(3000):
    point = [draw.uniform(-4.0, 5.0) for _ in range(dimensions(base))]
    problems = check_settings(decode(point, base), base)
    if problems:
      failures.append("a point decodes to settings the loader refuses: " + "; ".join(problems))
      break
  for knob in KNOBS:
    if knob.whole:
      seen = {knob.value(i / 999) for i in range(1000)}
      check(seen == set(range(int(knob.low), int(knob.high) + 1)), f"{knob.name} does not reach every whole number")
  # secondSlotTier's range starts at the tier the Lab has open when its level with a second slot is next, and ends one
  # past it, at never.
  tuning = json.loads(PACKAGED_TUNING.read_text(encoding="utf-8"))
  lab = next(structure for structure in tuning["structures"] if structure.get("kind") == "ResearchLab")
  slot = next(index for index, level in enumerate(lab["levels"]) if level.get("researchSlots", 1) > 1)
  open_tier = max([1] + [level.get("opensTier", 1) for level in lab["levels"][:slot]])
  second_slot = next(knob for knob in KNOBS if knob.name == "secondSlotTier")
  check((second_slot.low, second_slot.high) == (open_tier, open_tier + 1),
        f"secondSlotTier searches {second_slot.low:g} to {second_slot.high:g}, but the Lab has tier {open_tier} open when "
        f"its second slot is next")
  check(reflect(-0.25) == 0.25 and reflect(1.25) == 0.75 and reflect(2.5) == 0.5, "reflection is wrong")

  match = collections.namedtuple("Match", "winner tickets")
  check(score(match(1, []), 1) == 1.0 and score(match(1, []), 2) == -1.0 and score(match(0, []), 1) == 0.0,
        "a decided match scores wrongly")
  check(score(match(None, [(10, 1, 900), (10, 2, 950)]), 2) == 0.5, "the side ahead on tickets at the limit scores wrongly")
  check(score(match(None, [(10, 1, 900), (10, 2, 950), (20, 1, 980), (20, 2, 940)]), 2) == -0.5,
        "the latest tickets are not the ones that count")
  check(score(match(None, []), 1) == 0.0, "a match at the limit without tickets scores wrongly")
  check(len(both_seats(Path("a"), Path("b"), range(3))) == 6, "a pairing is not played from both seats")

  for message in failures:
    print(f"FAILED: {message}")
  print("Self-test " + ("failed." if failures else "passed."))
  return 1 if failures else 0


def main():
  parser = argparse.ArgumentParser(description="Self-play over the AI's settings, a probe of the match's rules.")
  parser.add_argument("--exe", help="the game, with its Assets folder beside it; by default the Release|x64 build's")
  parser.add_argument("--out", default=os.path.join(tempfile.gettempdir(), OUTPUT_NAME), help="the output folder")
  parser.add_argument("--resume", action="store_true", help="go on from the last finished generation")
  parser.add_argument("--report", action="store_true", help="only play and write the report for the champion")
  parser.add_argument("--self-test", action="store_true", help="check the search itself, without the game")
  parser.add_argument("--generations", type=int, default=100, help="generations in all, counting those done")
  parser.add_argument("--seeds", type=int, default=6, help="seeds a generation, each played from both seats")
  parser.add_argument("--population", type=int, default=None, help="candidates a generation; CMA-ES's default by default")
  parser.add_argument("--sigma", type=float, default=0.15, help="the first step, as a share of each knob's range")
  parser.add_argument("--challenge", type=float, default=0.2,
                      help="the score against the champion that earns a confirmation on fresh seeds")
  parser.add_argument("--promote", type=float, default=0.2, help="the score on fresh seeds that makes a champion")
  parser.add_argument("--confirm-seeds", type=int, default=10, help="fresh seeds a confirmation plays, from both seats")
  parser.add_argument("--report-seeds", type=int, default=40, help="the report plays seeds 1 to this, from both seats")
  parser.add_argument("--limit-minutes", type=int, default=120, help="a match still going this long is left")
  parser.add_argument("--workers", type=int, default=os.cpu_count() or 1, help="matches played at once")
  parser.add_argument("--search-seed", type=int, default=1, help="the seed of the search's own random draws")
  parser.add_argument("--keep-logs", action="store_true", help="keep each match's log in the output's work folder")
  arguments = parser.parse_args()

  if arguments.self_test:
    return self_test()
  for name in ("generations", "seeds", "confirm_seeds", "report_seeds", "limit_minutes", "workers"):
    if getattr(arguments, name) < 1:
      parser.error(f"--{name.replace('_', '-')} must be at least 1")
  if arguments.population is not None and arguments.population < 4:
    parser.error("--population must be at least 4")
  if not 0 < arguments.sigma <= STEP_CEILING:
    parser.error(f"--sigma must be above 0 and at most {STEP_CEILING}")
  for name in ("challenge", "promote"):
    if not -1 <= getattr(arguments, name) <= 1:
      parser.error(f"--{name} must be a score from -1 to 1")
  try:
    if arguments.report:
      folder = Path(arguments.out)
      state = load_state(folder)
      runner = Runner(find_exe(arguments.exe), arguments.workers, state["options"]["limit_minutes"], folder / "work",
                      arguments.keep_logs)
      report(state, runner, folder, arguments.report_seeds)
    else:
      search(arguments)
  except KeyboardInterrupt:
    print("\nStopped. --resume goes on from the last finished generation.", file=sys.stderr)
    return 130
  except RuntimeError as error:
    print(f"SelfPlay: {error}", file=sys.stderr)
    return 1
  return 0


if __name__ == "__main__":
  sys.exit(main())

#!/usr/bin/env python3
"""Equal-Ore battle model for the balance check of the MVP design.

The balance check asks whether ship design matters (GameDesign/Archive/OutpostCommander-MVP.md §3), the MVP's question Q2,
after which the MVP's documents and the ADRs call it the Q2 check. This model answers the first-order version of that
question: a fast first guess at what a change does. The scripted headless battles in GameLogicTests decide, and the
tuning numbers are tuned against them (§12). The model is not taught the simulation's geometry, so it
can disagree with them (owner, 2026-10-01).

It reads the hulls, drives, weapons and research topics from OutpostCommander/Assets/Tuning.json, the same file the game
loads (ADR-008), so the model and the game cannot disagree: change a number or an effect there and run this again.

The model is deliberately small, and each simplification is one the design document also makes or leaves open:

  - Hits are instant (§7). There is no accuracy, tracking or projectile flight.
  - Damage taken is max(damage x 0.25, damage - armour) (§7).
  - Each side is one clump of identical ships bought with the same Ore. Every ship in range of the enemy clump can
    shoot any ship in it. There is no pathing or terrain.
  - A clump stands in a square grid, as a group forms up in the game: its ships are FORMATION_SPACING_RADII footprint
    radii apart (ADR-010), from the footprints the hulls' sizes give (gate G5). Only splash needs the grid.
  - Each battle's Ore is drawn from within 15% of the nominal budget, the same for both sides. Armies in a match are
    never bought at an exact figure. The draws are stratified: battle k of n takes its Ore from the k-th of n equal
    slices of that window, so n battles cover the window evenly rather than wherever the random draws fall.
  - Ore that does not buy a whole ship buys a fractional one, with its hit points and damage scaled by the fraction.
    Without it, the Ore left over after the last whole ship (up to one ship's cost, a fifth of a small budget) would be
    thrown away, and the result would turn on where the budget cuts each design's ship count.
  - Both sides attack-move. Each closes on the other until its own weapon reaches, then stops and fires. Nobody kites
    or retreats.
  - Each ship's first shot comes at a random point within its fire interval, so volleys are not synchronised.
  - Targets are picked at random (spread fire) or as the weakest enemy (focus fire). Shots land together at the end
    of each tick, so focus fire overkills.
  - A splash weapon's hit also hits every other ship of the target's clump whose center is within its splash radius of
    the target's, each after its armour (§7, ADR-014). Ships do not close up as others die.
  - Structures are not modelled.
  - Research is applied only in check (d), to one side, and comes free: that side spends no Ore on it.

Every verdict is taken with a 95% confidence interval (Wilson). A win rate whose interval straddles its threshold is
run again with --max-seeds battles, and if it still straddles it, the verdict is UNSURE, which fails the check.

The balance check (§3) runs at the stages of Phase 1 design §7: the starting components (those no research topic unlocks), at
2,000-4,500 Ore; tier 1's, the MVP's every component, at 2,000-12,000; tier 2's at 4,500-12,000; and tier 3's at
6,000-12,000. It passes when, at every budget of every stage and in both fire modes:
  (a) every design has a counter that beats it at least 80% of the time,
  (b) the designs worth building, the support of the equilibrium mix, use every hull, drive and weapon of the stage at
      one budget of the stage or more, in each fire mode (owner, 2026-10-01): a heavy hull need not pay at the
      smallest budget, nor a medium one at the largest,
  (c) none of the counters in (a) against those designs stops winning when any single tuning number moves by 5%, and
  (d) no research topic, taken with its prerequisites by one side only, gives that side a design that none of the
      other side's designs beats at least half the time. A topic of tier 1 is played at the starting budgets against
      the starting designs; one of a later tier at its tier's budgets against every design and upgrade of the tier
      before. (b) at a later tier asks only the components the tier adds (owner, 2026-10-03, gate H9), and does not ask the
      Pulse Drive to be worth building: its case is speed, which no battle here sees.

Usage:
  python Tools/BattleModel.py                        the balance check against OutpostCommander/Assets/Tuning.json
  python Tools/BattleModel.py --detail               also print every win-rate matrix and the shots-to-kill table
  python Tools/BattleModel.py --quick                skip the robustness sweep in (c)
  python Tools/BattleModel.py --set Small.hp=220     try a number without editing the data (repeatable)
"""

import argparse
import concurrent.futures
import dataclasses
import itertools
import json
import math
import random
import re
import sys
from pathlib import Path

TUNING_DEFAULT = Path(__file__).resolve().parents[1] / "OutpostCommander" / "Assets" / "Tuning.json"

ARMOUR_FLOOR = 0.25  # §7: damage taken = max(damage x 0.25, damage - armour)
START_GAP_M = 400.0  # both clumps start out of range of every weapon
BUDGET_SPREAD = 0.15  # each battle's Ore is drawn from within 15% of the nominal budget
TIME_LIMIT_S = 600.0
RANGE_EPSILON_M = 1e-6

COUNTER_WIN_RATE = 0.8  # (a): a counter wins at least four battles in five
ROBUST_WIN_RATE = 0.5  # (c): a counter still wins after a 5% change; (d): an answer to a researched design
PERTURBATION = 0.05
WORTH_BUILDING = 0.05  # a design is worth building when the equilibrium mix gives it at least 5%
CONFIDENCE_Z = 1.96  # verdicts are taken on a 95% interval
FRACTION_MIN = 0.01  # a fractional ship smaller than this is not fielded
# The game's formation spacing, in footprint radii: FORMATION_SPACING_RADII in GameLogic/Simulation.cpp. It is code, not
# tuning data, so this is a second copy; change both together.
FORMATION_SPACING_RADII = 3.0


@dataclasses.dataclass(frozen=True)
class Hull:
  name: str
  hp: float
  armour: float
  speed: float
  cost: float
  footprint: float  # the footprint's radius in metres (gate G5)


@dataclasses.dataclass(frozen=True)
class Drive:
  name: str
  speed_factor: float
  hp_factor: float
  cost: float


@dataclasses.dataclass(frozen=True)
class Weapon:
  name: str
  damage: float
  interval: float
  range: float
  cost: float
  splash: float


@dataclasses.dataclass(frozen=True)
class Design:
  code: str
  hull: str
  drive: str
  weapon: str
  hp: float
  armour: float
  speed: float
  cost: float
  damage: float
  interval: float
  range: float
  splash: float
  footprint: float


@dataclasses.dataclass(frozen=True)
class Topic:
  number: int
  name: str
  requires: tuple
  unlocks: str  # the component the topic unlocks, or ""
  target: str  # what an upgrade changes: "All hulls", a weapon, or a target no battle feels, such as "Mining Rig"
  stat: str  # the stat an upgrade changes, or ""
  factor: float  # 1 + the upgrade's percentage
  tier: int = 1  # its research tier (Phase 1 design §6); a gateway has no target, stat or unlock


UPGRADES = {("All hulls", "HP"): "hp", ("weapon", "fire rate"): "interval", ("weapon", "damage"): "damage",
            ("weapon", "range"): "range"}
# Upgrades with no effect on a battle between two clumps: the economy, structures, speed, Constructors and ore. The model
# reads them, names them, and leaves them out.
# Drives whose case is speed, which a battle between two clumps cannot see: (b) does not ask them to be worth building
# (Phase 1 design §5, §7; owner, 2026-10-02, gate H4).
SPEED_DRIVES = ("Pulse",)
ECONOMY = {("Mining Rig", "income"), ("Shipyard", "build speed"), ("All structures", "HP"), ("Structure weapon", "fire rate"),
           ("All ships", "speed"), ("Constructors", "build rate"), ("Asteroids", "ore reserve")}


# ---- Reading the tuning data -----------------------------------------------------------------------------------------

# How the tuning data names an upgrade's target and stat, and how the model names them.
UPGRADE_TARGETS = {"allHulls": "All hulls", "miningRig": "Mining Rig", "shipyards": "Shipyard",
                   "allStructures": "All structures", "structureWeapon": "Structure weapon", "allShips": "All ships",
                   "constructors": "Constructors", "asteroids": "Asteroids"}
UPGRADE_STATS = {"hitPoints": "HP", "fireRate": "fire rate", "damage": "damage", "range": "range", "income": "income",
                 "buildSpeed": "build speed", "speed": "speed", "buildRate": "build rate", "oreReserve": "ore reserve"}


def field(entry, name, where):
  if not isinstance(entry, dict) or name not in entry:
    sys.exit(f"{where} has no '{name}'.")
  return entry[name]


def number(entry, name, where):
  value = field(entry, name, where)
  if isinstance(value, bool) or not isinstance(value, (int, float)):
    sys.exit(f"{where}: '{name}' is not a number.")
  return float(value)


def entries(data, name, path):
  value = field(data, name, path.name)
  if not isinstance(value, list):
    sys.exit(f"{path.name}: '{name}' is not a list.")
  return value


def read_tuning(path):
  """The tick rate, the hulls, drives and weapons, and the raw research entries from the tuning data."""
  try:
    data = json.loads(path.read_text(encoding="utf-8"))
  except (OSError, json.JSONDecodeError) as error:
    sys.exit(f"{path}: {error}")

  tick_hz = number(field(data, "rules", path.name), "tickHz", "rules")
  hulls = [Hull(field(h, "name", "a hull"), *(number(h, n, f"hull {h['name']}") for n in
                                               ("hitPoints", "armor", "speedMetersPerSecond", "cost",
                                                "footprintRadiusMeters")))
           for h in entries(data, "hulls", path)]
  drives = [Drive(field(d, "name", "a drive"), *(number(d, n, f"drive {d['name']}") for n in
                                                 ("speedFactor", "hitPointsFactor", "cost")))
            for d in entries(data, "drives", path)]
  weapons = [Weapon(field(w, "name", "a weapon"), *(number(w, n, f"weapon {w['name']}") for n in
                                                    ("damage", "fireIntervalSeconds", "rangeMeters", "cost",
                                                     "splashRadiusMeters")))
             for w in entries(data, "weapons", path)]
  ids = {kind: {int(number(e, "id", f"a {kind[:-1]}")): e["name"] for e in entries(data, kind, path)}
         for kind in ("hulls", "drives", "weapons")}
  return tick_hz, (hulls, drives, weapons), entries(data, "research", path), ids


def read_research(research, ids):
  """The research topics, with each effect read as an unlock or an upgrade."""
  topics = []
  for entry in research:
    number_ = int(number(entry, "id", "a research topic"))
    where = f"research topic {number_}"
    requires = field(entry, "requires", where)
    if not isinstance(requires, list) or not all(isinstance(r, int) for r in requires):
      sys.exit(f"{where}: 'requires' must be a list of topic ids.")
    effect = field(entry, "effect", where)
    tier = int(entry.get("tier", 1))
    if "opensTier" in effect:
      topics.append(Topic(number_, field(entry, "name", where), tuple(requires), "", "", "", 1.0, tier))
      continue
    unlocks = [(kind, effect[f"unlock{kind[:-1].capitalize()}"]) for kind in ("hulls", "drives", "weapons")
               if f"unlock{kind[:-1].capitalize()}" in effect]
    if unlocks:
      kind, component = unlocks[0]
      if len(unlocks) != 1 or component not in ids[kind]:
        sys.exit(f"{where}: an unlock names exactly one hull, drive or weapon by its id.")
      topics.append(Topic(number_, field(entry, "name", where), tuple(requires), ids[kind][component], "", "", 1.0, tier))
      continue
    target = field(effect, "upgrade", where)
    stat = UPGRADE_STATS.get(field(effect, "stat", where))
    percent = number(effect, "percent", where)
    if target == "weapon":
      weapon = int(number(effect, "weapon", where))
      if weapon not in ids["weapons"]:
        sys.exit(f"{where}: upgrades weapon {weapon}, which does not exist.")
      target, kind = ids["weapons"][weapon], "weapon"
    else:
      target = UPGRADE_TARGETS.get(target)
      kind = target
    if stat is None or target is None or ((kind, stat) not in UPGRADES and (target, stat) not in ECONOMY):
      sys.exit(f"{where}: the model cannot apply this upgrade. It knows all hulls' HP, a weapon's fire rate, damage "
               f"or range, and leaves out what no battle feels: {', '.join(sorted(t for t, _ in ECONOMY))}.")
    topics.append(Topic(number_, field(entry, "name", where), tuple(requires), "", target, stat, 1.0 + percent / 100.0,
                        tier))
  numbers = {t.number for t in topics}
  for t in topics:
    if set(t.requires) - numbers:
      sys.exit(f"research topic {t.number} requires a topic that does not exist.")
  return topics


def with_prerequisites(topic, topics):
  """The topic and every topic it needs, prerequisites first."""
  by_number = {t.number: t for t in topics}
  chain = []

  def visit(t):
    for r in t.requires:
      visit(by_number[r])
    if t not in chain:
      chain.append(t)

  visit(topic)
  return chain


def available(parts, topics, researched):
  """The hulls, drives and weapons a player can build once `researched` is done."""
  locked = {t.unlocks for t in topics if t.unlocks} - {t.unlocks for t in researched if t.unlocks}
  return tuple([p for p in group if p.name not in locked] for group in parts)


def through_tier(parts, topics, tier):
  """The hulls, drives and weapons that no topic unlocks, or that a topic of `tier` or an earlier one unlocks."""
  later = {t.unlocks for t in topics if t.unlocks and t.tier > tier}
  return tuple([p for p in group if p.name not in later] for group in parts)


def upgraded(parts, researched):
  """The parts with every upgrade in `researched` applied. Upgrades of one stat add their percentages (ADR-033)."""
  hulls, drives, weapons = (list(group) for group in parts)
  added = {}
  for t in researched:
    if t.stat and (t.target, t.stat) not in ECONOMY:
      added[(t.target, t.stat)] = added.get((t.target, t.stat), 0.0) + (t.factor - 1.0)
  for (target, stat), share in added.items():
    factor = 1.0 + share
    if target == "All hulls":
      hulls = [dataclasses.replace(h, hp=h.hp * factor) for h in hulls]
    else:
      field = UPGRADES[("weapon", stat)]
      change = (lambda v: v / factor) if field == "interval" else (lambda v: v * factor)
      weapons = [dataclasses.replace(w, **{field: change(getattr(w, field))}) if w.name == target else w
                 for w in weapons]
  return hulls, drives, weapons


def apply_overrides(parts, overrides):
  """Applies --set NAME.FIELD=VALUE to the hulls, drives and weapons read from the tuning data."""
  hulls, drives, weapons = (list(p) for p in parts)
  for override in overrides:
    match = re.fullmatch(r"(.+)\.(\w+)=([0-9.]+)", override)
    if not match:
      sys.exit(f"--set {override}: expected NAME.FIELD=VALUE, such as Small.hp=220.")
    name, field, value = match.group(1), match.group(2), float(match.group(3))
    for group in (hulls, drives, weapons):
      for i, part in enumerate(group):
        if part.name == name:
          if field not in {f.name for f in dataclasses.fields(part)} - {"name"}:
            sys.exit(f"--set {override}: {name} has no field '{field}'.")
          group[i] = dataclasses.replace(part, **{field: value})
          break
      else:
        continue
      break
    else:
      sys.exit(f"--set {override}: no hull, drive or weapon is called '{name}'.")
  return hulls, drives, weapons


# ---- Designs ---------------------------------------------------------------------------------------------------------


def short(name, initials):
  words = name.split()
  if initials and len(words) > 1:
    return "".join(word[0] for word in words)
  return name[0] if not initials else name[:2]


def designs_from(hulls, drives, weapons):
  designs = []
  for hull, drive, weapon in itertools.product(hulls, drives, weapons):
    designs.append(Design(code=f"{short(hull.name, False)}+{short(drive.name, False)}+{short(weapon.name, True)}",
                          hull=hull.name, drive=drive.name, weapon=weapon.name,
                          hp=hull.hp * drive.hp_factor, armour=hull.armour,
                          speed=hull.speed * drive.speed_factor, cost=hull.cost + drive.cost + weapon.cost,
                          damage=weapon.damage, interval=weapon.interval, range=weapon.range, splash=weapon.splash,
                          footprint=hull.footprint))
  if len({d.code for d in designs}) != len(designs):
    sys.exit("Two designs share a short code; give the hulls, drives or weapons distinct initials.")
  return designs


def hit(damage, armour):
  return max(damage * ARMOUR_FLOOR, damage - armour)


def splash_neighbours(count, footprint, splash):
  """For each ship of a clump of `count`, the other ships whose centers are within `splash` metres of its center.

  The clump is a square grid, ceil(sqrt(count)) columns wide, FORMATION_SPACING_RADII footprint radii apart.
  """
  if splash <= 0.0:
    return None
  spacing = FORMATION_SPACING_RADII * footprint
  columns = math.ceil(math.sqrt(count))
  places = [((i % columns) * spacing, (i // columns) * spacing) for i in range(count)]
  reach = splash * splash + 1e-9
  return [[j for j, (x, z) in enumerate(places) if j != i and (x - xi) ** 2 + (z - zi) ** 2 <= reach]
          for i, (xi, zi) in enumerate(places)]


# ---- One battle ------------------------------------------------------------------------------------------------------


def fight(a, b, budget, focus, seed, seeds, dt):
  """Battle `seed` of `seeds` between clumps of `a` and `b` bought with `budget` Ore each, give or take the spread.

  +1: a wins, -1: b wins, 0: draw.
  """
  rng = random.Random(seed)
  uniform, rand = rng.uniform, rng.random
  budget *= 1.0 - BUDGET_SPREAD + 2.0 * BUDGET_SPREAD * (seed % seeds + rand()) / seeds
  side = (a, b)
  hp, scale = [], []
  for d in side:
    whole, fraction = divmod(budget / d.cost, 1.0)
    if not whole:
      raise ValueError(f"{budget:.0f} Ore does not buy one {d.code}.")
    scales = [1.0] * int(whole) + ([fraction] if fraction >= FRACTION_MIN else [])
    hp.append([d.hp * f for f in scales])
    scale.append(scales)
  n = (len(hp[0]), len(hp[1]))
  alive = [list(range(n[0])), list(range(n[1]))]
  # Who a splash from side s also hits, around each ship of the other side.
  splashed = (splash_neighbours(n[1], b.footprint, a.splash), splash_neighbours(n[0], a.footprint, b.splash))
  per_hit = (hit(a.damage, b.armour), hit(b.damage, a.armour))
  interval = (a.interval, b.interval)
  reach = (a.range + RANGE_EPSILON_M, b.range + RANGE_EPSILON_M)
  step = (a.speed * dt, b.speed * dt)
  when = ([0.0] * n[0], [0.0] * n[1])
  engaged = [False, False]
  queue = {}  # tick -> [(side, ship)]
  gap = START_GAP_M
  ceil = math.ceil
  tick, ticks = 0, int(TIME_LIMIT_S / dt)

  while tick < ticks:
    if not (engaged[0] and engaged[1]):
      moving = [s for s in (0, 1) if gap > reach[s]]
      if moving:
        closed = sum(min(step[s], gap - side[s].range) for s in moving)
        gap = max(gap - closed, min(side[s].range for s in moving))
      for s in (0, 1):
        if not engaged[s] and gap <= reach[s]:
          engaged[s] = True
          for ship in range(n[s]):
            when[s][ship] = time = tick * dt + uniform(0.0, interval[s])
            queue.setdefault(ceil(time / dt - 1e-9), []).append((s, ship))

    shots = queue.pop(tick, None)
    if shots is None:
      # Once both sides stand and fire, nothing happens until the next shot.
      tick = min(queue) if engaged[0] and engaged[1] and queue else tick + 1
      continue
    if focus:
      weakest = (min(alive[1], key=hp[1].__getitem__), min(alive[0], key=hp[0].__getitem__))
    landed = []
    for s, ship in shots:
      if hp[s][ship] <= 0.0:
        continue
      enemy = 1 - s
      if focus:
        target = weakest[s]
      else:
        targets = alive[enemy]
        target = targets[int(rand() * len(targets))]
      landed.append((enemy, target, per_hit[s] * scale[s][ship]))
      if splashed[s] is not None:
        landed.extend((enemy, other, per_hit[s] * scale[s][ship]) for other in splashed[s][target] if hp[enemy][other] > 0.0)
      when[s][ship] = time = when[s][ship] + interval[s]
      queue.setdefault(ceil(time / dt - 1e-9), []).append((s, ship))
    died = False
    for enemy, target, damage in landed:
      before = hp[enemy][target]
      hp[enemy][target] = before - damage
      died |= before > 0.0 >= before - damage
    if died:
      alive[0] = [ship for ship in alive[0] if hp[0][ship] > 0.0]
      alive[1] = [ship for ship in alive[1] if hp[1][ship] > 0.0]
      if not alive[0] or not alive[1]:
        break
    tick += 1
  return (1 if alive[0] else 0) - (1 if alive[1] else 0)


def pair_record(task):
  """Wins and losses of `a` against `b` over the seeds, for one budget and fire mode."""
  a, b, budget, focus, seeds, dt = task
  outcomes = [fight(a, b, budget, focus, seed, seeds, dt) for seed in range(seeds)]
  return outcomes.count(1), outcomes.count(-1)


def wilson(wins, n):
  """The 95% confidence interval of a win rate of wins/n."""
  z = CONFIDENCE_Z
  p, d = wins / n, 1.0 + z * z / n
  centre = (p + z * z / (2 * n)) / d
  half = z * math.sqrt(p * (1.0 - p) / n + z * z / (4.0 * n * n)) / d
  return centre - half, centre + half


def verdict(wins, n, threshold):
  low, high = wilson(wins, n)
  return "pass" if low >= threshold else "fail" if high < threshold else "unsure"


def settle(pool, contests, threshold, seeds, max_seeds, dt):
  """The best win rate and its verdict for each contest, re-running the uncertain ones with more battles.

  A contest is a list of (a, b, budget, focus, wins, n) candidates for the same job: a counter to one design, an
  answer to one researched design, or one counter under a perturbation. It passes if any candidate passes.
  """
  results = []
  rerun = []
  for c, candidates in enumerate(contests):
    best = max(candidates, key=lambda x: x[4] / x[5])
    verdicts = [verdict(x[4], x[5], threshold) for x in candidates]
    if "pass" in verdicts or "unsure" not in verdicts:
      results.append((best, "pass" if "pass" in verdicts else "fail"))
    else:
      results.append(None)
      rerun.extend((c, x) for x, v in zip(candidates, verdicts) if v == "unsure")
  if rerun and max_seeds > seeds:
    tasks = [(x[0], x[1], x[2], x[3], max_seeds, dt) for _, x in rerun]
    again = {}
    for (c, x), (wins, _) in zip(rerun, pool.map(pair_record, tasks, chunksize=1)):
      again.setdefault(c, []).append((*x[:4], wins, max_seeds))
    for c, candidates in again.items():
      best = max(candidates, key=lambda x: x[4] / x[5])
      verdicts = [verdict(x[4], x[5], threshold) for x in candidates]
      results[c] = (best, "pass" if "pass" in verdicts else "unsure" if "unsure" in verdicts else "fail")
  for c, result in enumerate(results):
    if result is None:  # nothing re-run: max_seeds is not above seeds
      best = max(contests[c], key=lambda x: x[4] / x[5])
      results[c] = (best, "unsure")
  return results


# ---- Matrices, the equilibrium mix and the balance check -------------------------------------------------------------


def win_matrix(pool, designs, budget, focus, seeds, dt):
  pairs = list(itertools.combinations(range(len(designs)), 2))
  tasks = [(designs[i], designs[j], budget, focus, seeds, dt) for i, j in pairs]
  matrix = [[0.5] * len(designs) for _ in designs]
  for (i, j), (wins, losses) in zip(pairs, pool.map(pair_record, tasks, chunksize=4)):
    matrix[i][j] = wins / seeds
    matrix[j][i] = losses / seeds
  return matrix


def equilibrium(matrix, iterations=40000):
  """The mix of designs neither side can improve on, found by fictitious play on the win-minus-loss payoff."""
  n = len(matrix)
  payoff_of = [[matrix[i][j] - matrix[j][i] for j in range(n)] for i in range(n)]
  counts = [0] * n
  payoff = [0.0] * n
  best = 0
  for _ in range(iterations):
    counts[best] += 1
    for i in range(n):
      payoff[i] += payoff_of[i][best]
    best = max(range(n), key=payoff.__getitem__)
  return [count / iterations for count in counts]


def perturbed_parts(parts):
  """Every (label, hulls, drives, weapons) with one modelled number moved by +/-5%."""
  hulls, drives, weapons = parts
  groups = (hulls, drives, weapons)
  for g, group in enumerate(groups):
    for i, part in enumerate(group):
      # The footprint is a size, not a combat number, and the simulation's check does not move it either.
      for field in (f.name for f in dataclasses.fields(part) if f.name not in ("name", "footprint")):
        if field == "splash" and not part.splash:
          continue
        for sign in (-1, 1):
          changed = dataclasses.replace(part, **{field: getattr(part, field) * (1 + sign * PERTURBATION)})
          new = [list(hulls), list(drives), list(weapons)]
          index = new[g].index(part)
          new[g][index] = changed
          yield f"{part.name}.{field} {'-' if sign < 0 else '+'}5%", new[0], new[1], new[2]


def component_names(parts):
  hulls, drives, weapons = parts
  return [h.name for h in hulls], [d.name for d in drives], [w.name for w in weapons]


def print_designs(designs, hulls):
  print(f"{'design':10} {'Ore':>5} {'HP':>6} {'armour':>6} {'m/s':>5} {'range':>5}   damage per second after armour")
  for d in designs:
    after = "  ".join(f"vs {h.name} {hit(d.damage, h.armour) / d.interval:5.1f}" for h in hulls)
    print(f"{d.code:10} {d.cost:5.0f} {d.hp:6.0f} {d.armour:6.1f} {d.speed:5.1f} {d.range:5.0f}   {after}")


def print_shots_to_kill(designs):
  weapons = {d.weapon: d for d in designs}.values()
  print("\nShots to kill, and how far the target's HP sits above the next breakpoint (one shot fewer):")
  print(f"{'weapon':14}" + "".join(f"{d.code:>16}" for d in designs))
  for w in weapons:
    row = []
    for d in designs:
      per_hit = hit(w.damage, d.armour)
      shots = math.ceil(d.hp / per_hit - 1e-9)
      margin = (d.hp - (shots - 1) * per_hit) / d.hp
      row.append(f"{shots:>9} ({margin:4.0%})")
    print(f"{w.weapon:14}" + "".join(row))


def print_matrix(designs, matrix):
  print(" " * 10 + "".join(f"{d.code:>9}" for d in designs))
  for i, d in enumerate(designs):
    print(f"{d.code:>10}" + "".join(f"{'-':>9}" if i == j else f"{matrix[i][j]:>9.0%}" for j in range(len(designs))))


def stage(pool, label, parts, budgets, args, dt, unmodelled, failures, legs, speed_drives=(), judged=None):
  """Runs (a) and (b) for one stage of a match, adding the counters it finds to `legs` for (c).

  (b) judges the components of `judged`, by default every one of the stage: a later tier's stage asks only the
  components the tier adds (owner, 2026-10-03, gate H9). It does not ask a drive in `speed_drives` to be worth building,
  since a battle cannot see speed; that is still reported.
  """
  judged = parts if judged is None else judged
  designs = designs_from(*parts)
  print(f"\n==== {label}: {len(designs)} designs ====")
  # (b) is judged over the stage, in each fire mode: the components used at any of its budgets.
  used_in_stage = {False: [set(), set(), set()], True: [set(), set(), set()]}
  for budget in budgets:
    for focus in (False, True):
      mode = "focus" if focus else "spread"
      where = f"{label}, {budget:,} Ore {mode}"
      matrix = win_matrix(pool, designs, budget, focus, args.seeds, dt)
      mix = equilibrium(matrix)
      built = [i for i, weight in enumerate(mix) if weight >= WORTH_BUILDING]
      print(f"\n{budget:,} Ore, {mode} fire. Worth building: " +
            ", ".join(f"{designs[i].code} {mix[i]:.0%}" for i in sorted(built, key=lambda i: -mix[i])))
      if args.detail:
        print_matrix(designs, matrix)
      contests = [[(designs[i], target, budget, focus, round(matrix[i][j] * args.seeds), args.seeds)
                   for i in range(len(designs)) if i != j] for j, target in enumerate(designs)]
      for j, ((counter, target, _, _, wins, n), status) in enumerate(
          settle(pool, contests, COUNTER_WIN_RATE, args.seeds, args.max_seeds, dt)):
        if status != "pass":
          failures["a" if status == "fail" else "a?"].append(
            f"{where}: the best counter to {target.code} is {counter.code} at {wins / n:.0%} of {n}")
        if j in built:
          legs.add((counter.code, target.code, budget, focus))
          print(f"  {target.code:>9} is countered by {counter.code} ({wins / n:.0%} of {n})")
      used = [{getattr(designs[i], attribute) for i in built} for attribute in ("hull", "drive", "weapon")]
      for in_stage, in_use in zip(used_in_stage[focus], used):
        in_stage |= in_use
      unused = unused_components(parts, used, unmodelled)
      if unused:
        print(f"  Not worth building at this budget: {unused}")
  for focus in (False, True):
    unused = unused_components(judged, used_in_stage[focus], unmodelled + list(speed_drives))
    if unused:
      failures["b"].append(f"{label}, {'focus' if focus else 'spread'} fire: no design worth building at any budget uses the {unused}")
    exempt = [d for d in speed_drives if d in component_names(judged)[1] and d not in used_in_stage[focus][1]]
    if exempt:
      print(f"\n{label}, {'focus' if focus else 'spread'} fire: no design worth building at any budget uses the "
            f"{', '.join(exempt)} drive, which (b) does not ask of it")


def unused_components(parts, used, unmodelled):
  """The hulls, drives and weapons, modelled, that no design in `used` has, as "Large hull, Fusion drive"."""
  return ", ".join(f"{name} {kind}" for names, in_use, kind in zip(component_names(parts), used, ("hull", "drive", "weapon"))
                   for name in names if name not in in_use and name not in unmodelled)


def research_check(pool, parts, topics, tier, budgets, args, dt, failures):
  """(d): each topic of `tier`, with its prerequisites, researched by one side only.

  The other side has every topic of the tiers before: none for tier 1, as the MVP's (d), and every design and upgrade
  of the tier before for a later tier (Phase 1 design §7). Only the designs the topic adds or changes are tested.
  """
  before = [t for t in topics if t.tier < tier]
  answers = designs_from(*upgraded(available(parts, topics, before), before))
  by_code = {d.code: d for d in answers}
  print(f"\n==== One-sided research (d), tier {tier}: each topic against the {len(answers)} designs of "
        f"{'the start' if tier == 1 else f'tier {tier - 1}'} ====")
  for topic in topics:
    if topic.tier != tier:
      continue
    if not (topic.unlocks or topic.stat):
      print(f"  {topic.name}: no effect on a battle")
      continue
    if (topic.target, topic.stat) in ECONOMY:
      print(f"  {topic.name}: no effect on a battle")
      continue
    chain = [t for t in with_prerequisites(topic, topics) if t not in before]
    researched = before + chain
    side = upgraded(available(parts, topics, researched), researched)
    designs = designs_from(*side)
    if topic.unlocks and not any(topic.unlocks in (d.hull, d.drive, d.weapon) for d in designs):
      print(f"  {topic.name}: not modelled")
      continue
    designs = [d for d in designs if by_code.get(d.code) != d]
    if not designs:
      print(f"  {topic.name}: changes no design")
      continue
    keys = [(budget, focus, x) for budget in budgets for focus in (False, True) for x in designs]
    tasks = [(y, x, budget, focus, args.seeds, dt) for budget, focus, x in keys for y in answers]
    records = iter(pool.map(pair_record, tasks, chunksize=4))
    contests = [[(y, x, budget, focus, next(records)[0], args.seeds) for y in answers] for budget, focus, x in keys]
    settled = settle(pool, contests, ROBUST_WIN_RATE, args.seeds, args.max_seeds, dt)
    chain = " + ".join(t.name for t in chain)
    (y, x, budget, focus, wins, n), _ = min(settled, key=lambda r: r[0][4] / r[0][5])
    print(f"  {chain}: weakest answer is {y.code} to {x.code}* at {wins / n:.0%} of {n} "
          f"({budget:,} Ore {'focus' if focus else 'spread'})")
    for (y, x, budget, focus, wins, n), status in settled:
      if status != "pass":
        failures["d" if status == "fail" else "d?"].append(
          f"{chain}, {budget:,} Ore {'focus' if focus else 'spread'}: the best answer to {x.code}* is {y.code} "
          f"at {wins / n:.0%} of {n}")


def run(args):
  tick_hz, parts, research, ids = read_tuning(args.tuning)
  parts = apply_overrides(parts, args.set)
  topics = read_research(research, ids)
  every = parts
  last_tier = max(1, min(args.last_tier, max(t.tier for t in topics)))
  parts = through_tier(every, topics, last_tier)
  hulls, drives, weapons = parts
  dt = 1.0 / tick_hz
  designs = designs_from(hulls, drives, weapons)
  budgets = [int(b) for b in args.budgets.split(",")]
  early = [int(b) for b in args.early_budgets.split(",")]
  tier_budgets = [budgets] + [[int(b) for b in later.split(",")] for later in (args.tier_two_budgets, args.tier_three_budgets)]
  research_budgets = [early] + tier_budgets[1:]
  unmodelled = []  # every hull, drive and weapon is modelled
  start = available(parts, topics, [])

  print(f"Numbers from {args.tuning.name}" + (f", with {', '.join(args.set)}" if args.set else "") +
        f". {len(designs)} designs through tier {last_tier}, {args.seeds} battles per pairing (up to {args.max_seeds} when a verdict is "
        f"uncertain), a {tick_hz:g} Hz tick.")
  print("Starting components: " + ", ".join(p.name for group in start for p in group) + ".")
  print()
  print_designs(designs, hulls)
  if args.detail:
    print_shots_to_kill(designs)

  failures = {key: [] for key in ("a", "a?", "b", "c", "c?", "d", "d?")}
  legs = set()
  with concurrent.futures.ProcessPoolExecutor(max_workers=args.jobs) as pool:
    # The stages of Phase 1 design §7: each tier's components at the budgets that fit when they arrive, the starting
    # components, and each tier's research against the tier before.
    for tier in range(1, last_tier + 1):
      now = through_tier(every, topics, tier)
      before = through_tier(every, topics, tier - 1) if tier > 1 else ([], [], [])
      added = tuple([p for p in group if p not in old] for group, old in zip(now, before))
      stage(pool, f"Tier {tier}", now, tier_budgets[tier - 1], args, dt, unmodelled, failures, legs, SPEED_DRIVES, added)
      if tier == 1:
        stage(pool, "Starting components", start, early, args, dt, unmodelled, failures, legs)
      research_check(pool, every, topics, tier, research_budgets[tier - 1], args, dt, failures)

    if not args.quick:
      by_code = {d.code: d for d in designs}
      keys, tasks = [], []
      for label, new_hulls, new_drives, new_weapons in perturbed_parts(parts):
        changed = {d.code: d for d in designs_from(new_hulls, new_drives, new_weapons)}
        for counter, target, budget, focus in sorted(legs):
          if changed[counter] == by_code[counter] and changed[target] == by_code[target]:
            continue
          tasks.append((changed[counter], changed[target], budget, focus, args.robust_seeds, dt))
          keys.append(label)
      records = pool.map(pair_record, tasks, chunksize=2)
      contests = [[(*task[:4], wins, args.robust_seeds)] for task, (wins, _) in zip(tasks, records)]
      for label, ((counter, target, budget, focus, wins, n), status) in zip(
          keys, settle(pool, contests, ROBUST_WIN_RATE, args.robust_seeds, args.max_seeds, dt)):
        if status != "pass":
          failures["c" if status == "fail" else "c?"].append(
            f"{label}: {counter.code} no longer beats {target.code} at {budget:,} Ore "
            f"{'focus' if focus else 'spread'} fire ({wins / n:.0%} of {n})")
      print(f"\nRobustness: {len(tasks)} counter checks under one-number changes of +/-5%.")

  print("\nBalance check (§3):")
  verdicts = {
    "a": "every design has a counter that wins at least 80%",
    "b": "the designs worth building use every hull, drive and weapon over each stage's budgets",
    "c": "no counter stops winning when one number moves 5%",
    "d": "no research topic leaves a design without an answer that wins half the time",
  }
  failed = False
  for key, text in verdicts.items():
    if key == "c" and args.quick:
      print(f"  ({key}) {text}: not run (--quick)")
      continue
    unsure = failures.get(key + "?", [])
    if failures[key]:
      status = "FAIL"
    elif unsure:
      status = "UNSURE"
    else:
      status = "INCOMPLETE" if key == "b" and unmodelled else "PASS"
    failed |= status in ("FAIL", "UNSURE")
    suffix = f" ({', '.join(unmodelled)} not modelled)" if status == "INCOMPLETE" else ""
    print(f"  ({key}) {text}: {status}{suffix}")
    for line in failures[key]:
      print(f"      {line}")
    for line in unsure:
      print(f"      unsure: {line}")
  return 1 if failed else 0


def main():
  parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
  parser.add_argument("--tuning", type=Path, default=TUNING_DEFAULT, help="the tuning data to read (ADR-008)")
  parser.add_argument("--budgets", default="2000,3000,4500,6000,9000,12000",
                      help="Ore per side for tier 1's components, the MVP's every component, comma-separated")
  parser.add_argument("--early-budgets", default="2000,3000,4500",
                      help="Ore per side for the starting components and tier 1's research check (d)")
  parser.add_argument("--tier-two-budgets", default="4500,6000,9000,12000",
                      help="Ore per side for tier 2's components and research (Phase 1 design §7)")
  parser.add_argument("--tier-three-budgets", default="6000,9000,12000",
                      help="Ore per side for tier 3's components and research (Phase 1 design §7)")
  parser.add_argument("--last-tier", type=int, default=3, help="the last research tier to check; 1 is the MVP's check")
  parser.add_argument("--seeds", type=int, default=60, help="battles per pairing for the matrices")
  parser.add_argument("--robust-seeds", type=int, default=30, help="battles per pairing for the robustness sweep")
  parser.add_argument("--max-seeds", type=int, default=480,
                      help="battles for a verdict whose confidence interval straddles its threshold")
  parser.add_argument("--jobs", type=int, default=None, help="worker processes (default: one per core)")
  parser.add_argument("--set", action="append", default=[], metavar="NAME.FIELD=VALUE",
                      help="override one tuning number, such as Small.hp=220 or Ion.hp_factor=1.0")
  parser.add_argument("--detail", action="store_true", help="print every matrix and the shots-to-kill table")
  parser.add_argument("--quick", action="store_true", help="skip the robustness sweep")
  return run(parser.parse_args())


if __name__ == "__main__":
  sys.exit(main())

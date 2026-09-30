#!/usr/bin/env python3
"""Equal-Ore battle model for Q2 of the MVP design.

Q2 asks whether ship design matters (GameDesign/OutpostCommander-MVP.md §3). This model answers the first-order
version of that question before any C++ exists. §12's numbers are tuned against it until the scripted headless
battles in SimulationTests take over at milestone 3 (§14).

It reads the hull, drive and weapon tables from §12 and the research table from §8 of the design document itself, so
the model and the document cannot disagree: change a number in §12 or an effect in §8 and run this again.

The model is deliberately small, and each simplification is one the design document also makes or leaves open:

  - Hits are instant (§7). There is no accuracy, tracking or projectile flight.
  - Damage taken is max(damage x 0.25, damage - armour) (§7).
  - Each side is one clump of identical ships bought with the same Ore. Every ship in range of the enemy clump can
    shoot any ship in it. There is no formation, pathing or terrain.
  - Each battle's Ore is drawn from within 15% of the nominal budget, the same for both sides. Armies in a match are
    never bought at an exact figure, and without the spread a budget that happens to cut one design's ship count
    would decide the result rather than the designs.
  - Both sides attack-move. Each closes on the other until its own weapon reaches, then stops and fires. Nobody kites
    or retreats.
  - Each ship's first shot comes at a random point within its fire interval, so volleys are not synchronised.
  - Targets are picked at random (spread fire) or as the weakest enemy (focus fire). Shots land together at the end
    of each tick, so focus fire overkills.
  - The Missile Rack is not modelled: its splash needs ship spacing, and ship sizes are not set yet (§15).
  - Structures are not modelled.
  - Research is applied only in check (d), to one side, and comes free: that side spends no Ore on it.

The Q2 check (§3) runs at two stages: every component, at 2,000-12,000 Ore, and the starting components (those no
research topic unlocks), at 2,000-4,500 Ore. It passes when, at every budget of both stages and in both fire modes:
  (a) every design has a counter that beats it at least 80% of the time,
  (b) the designs worth building, the support of the equilibrium mix, use every hull, drive and weapon of the stage,
  (c) none of the counters in (a) against those designs stops winning when any single §12 number moves by 5%, and
  (d) at the starting budgets, no research topic, taken with its prerequisites by one side only, gives that side a
      design that none of the other side's starting designs beats at least half the time.

Usage:
  python Tools/BattleModel.py                        the Q2 check against §12
  python Tools/BattleModel.py --detail               also print every win-rate matrix and the shots-to-kill table
  python Tools/BattleModel.py --quick                skip the robustness sweep in (c)
  python Tools/BattleModel.py --set Small.hp=220     try a number without editing §12 (repeatable)
"""

import argparse
import concurrent.futures
import dataclasses
import itertools
import math
import random
import re
import sys
from pathlib import Path

DOC_DEFAULT = Path(__file__).resolve().parents[1] / "GameDesign" / "OutpostCommander-MVP.md"

ARMOUR_FLOOR = 0.25  # §7: damage taken = max(damage x 0.25, damage - armour)
START_GAP_M = 400.0  # both clumps start out of range of every weapon
BUDGET_SPREAD = 0.15  # each battle's Ore is drawn from within 15% of the nominal budget
TIME_LIMIT_S = 600.0
RANGE_EPSILON_M = 1e-6

COUNTER_WIN_RATE = 0.8  # (a): a counter wins at least four battles in five
ROBUST_WIN_RATE = 0.5  # (c): a counter still wins after a 5% change; (d): an answer to a researched design
PERTURBATION = 0.05
WORTH_BUILDING = 0.05  # a design is worth building when the equilibrium mix gives it at least 5%


@dataclasses.dataclass(frozen=True)
class Hull:
  name: str
  hp: float
  armour: float
  speed: float
  cost: float


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


@dataclasses.dataclass(frozen=True)
class Topic:
  number: int
  name: str
  requires: tuple
  unlocks: str  # the component the topic unlocks, or ""
  target: str  # what an upgrade changes: "All hulls", a weapon, or an economy target such as "Mining Rig"
  stat: str  # the §8 stat an upgrade changes, or ""
  factor: float  # 1 + the upgrade's percentage


UPGRADES = {("All hulls", "HP"): "hp", ("weapon", "fire rate"): "interval", ("weapon", "damage"): "damage",
            ("weapon", "range"): "range"}
ECONOMY = {("Mining Rig", "income"), ("Shipyard", "build speed")}  # upgrades with no effect on a battle


# ---- Reading §12 -----------------------------------------------------------------------------------------------------


def section_lines(text, number):
  lines = text.splitlines()
  start = next((i for i, line in enumerate(lines) if line.startswith(f"## {number}.")), None)
  if start is None:
    sys.exit(f"The design document has no '## {number}.' section.")
  end = next((i for i in range(start + 1, len(lines)) if lines[i].startswith("## ")), len(lines))
  return lines[start + 1:end]


def cells(line):
  return [cell.strip() for cell in line.strip().strip("|").split("|")]


def tables_in(lines):
  """Every Markdown table in `lines`, keyed by its first header cell, as a list of rows keyed by header."""
  tables = {}
  i = 0
  while i < len(lines):
    is_header = lines[i].startswith("|") and i + 1 < len(lines) and "-" in lines[i + 1]
    if is_header and set(lines[i + 1].replace("|", "").strip()) <= set("-: "):
      header = cells(lines[i])
      rows = []
      i += 2
      while i < len(lines) and lines[i].startswith("|"):
        rows.append(dict(zip(header, cells(lines[i]))))
        i += 1
      tables[header[0]] = rows
    else:
      i += 1
  return tables


def number(text, where):
  match = re.match(r"\s*([0-9][0-9,]*(?:\.[0-9]+)?)", text)
  if not match:
    sys.exit(f"§12 {where}: '{text}' is not a number.")
  return float(match.group(1).replace(",", ""))


def column(row, name, table):
  if name not in row:
    sys.exit(f"The {table} table has no '{name}' column. It has: {', '.join(row)}.")
  return row[name]


def read_section_12(text):
  tables = tables_in(section_lines(text, 12))
  for name in ("Item", "Hull", "Drive", "Weapon"):
    if name not in tables:
      sys.exit(f"§12 has no table whose first column is '{name}'.")

  items = {row["Item"]: row["Value"] for row in tables["Item"]}
  if "Simulation tick" not in items:
    sys.exit("§12 has no 'Simulation tick' row.")
  tick_hz = number(items["Simulation tick"], "Simulation tick")

  hulls = []
  for row in tables["Hull"]:
    name = row["Hull"]
    hulls.append(Hull(name, *(number(column(row, c, "§12 Hull"), f"{name} {c}")
                              for c in ("HP", "Armour", "Speed (m/s)", "Cost"))))
  drives = []
  for row in tables["Drive"]:
    name = row["Drive"]
    drives.append(Drive(name, *(number(column(row, c, "§12 Drive"), f"{name} {c}") for c in ("Speed ×", "HP ×", "Cost +"))))
  weapons = []
  for row in tables["Weapon"]:
    name = row["Weapon"]
    damage_cell = column(row, "Damage", "§12 Weapon")
    splash = re.search(r"splash\s+([0-9.]+)\s*m", damage_cell)
    weapons.append(Weapon(name, number(damage_cell, f"{name} Damage"),
                          number(column(row, "Fire interval (s)", "§12 Weapon"), f"{name} Fire interval"),
                          number(column(row, "Range (m)", "§12 Weapon"), f"{name} Range"),
                          number(column(row, "Cost +", "§12 Weapon"), f"{name} Cost"),
                          float(splash.group(1)) if splash else 0.0))
  return tick_hz, hulls, drives, weapons


def read_section_8(text, parts):
  """The research topics in §8's table, with each effect read as an unlock or an upgrade."""
  tables = tables_in(section_lines(text, 8))
  if "#" not in tables:
    sys.exit("§8 has no table whose first column is '#'.")
  hulls, drives, weapons = parts
  components = [p.name for group in parts for p in group]
  weapon_names = {w.name for w in weapons}
  topics = []
  for row in tables["#"]:
    where = f"§8 topic {row.get('#', '?')}"
    cells_ = [row.get("#", "")] + [r for r in re.split(r"\s*,\s*", column(row, "Requires", "§8 research").strip())
                                   if r not in ("—", "-", "")]
    if not all(c.isdigit() for c in cells_):
      sys.exit(f"{where}: the topic number and its requirements must be topic numbers.")
    number_, requires = int(cells_[0]), tuple(int(c) for c in cells_[1:])
    effect = column(row, "Effect", "§8 research")
    unlock = re.fullmatch(r"Unlocks the (.+)", effect)
    upgrade = re.fullmatch(r"(.+?):?\s+(HP|fire rate|damage|range|income|build speed)\s+\+([0-9.]+)%", effect)
    if unlock:
      named = [c for c in components if re.search(rf"\b{re.escape(c)}\b", unlock.group(1))]
      if len(named) != 1:
        sys.exit(f"{where}: '{effect}' should name exactly one hull, drive or weapon from §12.")
      topics.append(Topic(number_, row["Topic"], requires, named[0], "", "", 1.0))
    elif upgrade:
      target, stat, percent = upgrade.group(1), upgrade.group(2), float(upgrade.group(3))
      kind = "weapon" if target in weapon_names else target
      if (kind, stat) not in UPGRADES and (target, stat) not in ECONOMY:
        sys.exit(f"{where}: the model cannot apply '{effect}'. It knows All hulls HP, a weapon's fire rate, damage "
                 f"or range, Mining Rig income and Shipyard build speed.")
      topics.append(Topic(number_, row["Topic"], requires, "", target, stat, 1.0 + percent / 100.0))
    else:
      sys.exit(f"{where}: '{effect}' is neither 'Unlocks the <component>' nor '<target> <stat> +N%'.")
  numbers = {t.number for t in topics}
  for t in topics:
    if set(t.requires) - numbers:
      sys.exit(f"§8 topic {t.number} requires a topic that does not exist.")
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


def upgraded(parts, researched):
  """The parts with every upgrade in `researched` applied."""
  hulls, drives, weapons = (list(group) for group in parts)
  for t in researched:
    if t.target == "All hulls":
      hulls = [dataclasses.replace(h, hp=h.hp * t.factor) for h in hulls]
    elif (t.target, t.stat) not in ECONOMY and t.stat:
      field = UPGRADES[("weapon", t.stat)]
      change = (lambda v: v / t.factor) if field == "interval" else (lambda v: v * t.factor)
      weapons = [dataclasses.replace(w, **{field: change(getattr(w, field))}) if w.name == t.target else w
                 for w in weapons]
  return hulls, drives, weapons


def apply_overrides(parts, overrides):
  """Applies --set NAME.FIELD=VALUE to the hulls, drives and weapons read from §12."""
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
    if weapon.splash:
      continue
    designs.append(Design(code=f"{short(hull.name, False)}+{short(drive.name, False)}+{short(weapon.name, True)}",
                          hull=hull.name, drive=drive.name, weapon=weapon.name,
                          hp=hull.hp * drive.hp_factor, armour=hull.armour,
                          speed=hull.speed * drive.speed_factor, cost=hull.cost + drive.cost + weapon.cost,
                          damage=weapon.damage, interval=weapon.interval, range=weapon.range))
  if len({d.code for d in designs}) != len(designs):
    sys.exit("Two designs share a short code; give the hulls, drives or weapons distinct initials.")
  return designs


def hit(damage, armour):
  return max(damage * ARMOUR_FLOOR, damage - armour)


# ---- One battle ------------------------------------------------------------------------------------------------------


def fight(a, b, budget, focus, seed, dt):
  """One battle between clumps of `a` and `b` bought with `budget` Ore each. +1: a wins, -1: b wins, 0: draw."""
  rng = random.Random(seed)
  budget *= rng.uniform(1.0 - BUDGET_SPREAD, 1.0 + BUDGET_SPREAD)
  side = (a, b)
  hp = [[d.hp] * int(budget // d.cost) for d in side]
  if not hp[0] or not hp[1]:
    raise ValueError(f"{budget:.0f} Ore does not buy one {a.code if not hp[0] else b.code}.")
  alive = [list(range(len(hp[0]))), list(range(len(hp[1])))]
  per_hit = (hit(a.damage, b.armour), hit(b.damage, a.armour))
  when = [[0.0] * len(hp[0]), [0.0] * len(hp[1])]
  engaged = [False, False]
  queue = {}  # tick -> [(side, ship)]
  gap = START_GAP_M

  def schedule(s, ship, time):
    when[s][ship] = time
    queue.setdefault(math.ceil(time / dt - 1e-9), []).append((s, ship))

  for tick in range(int(TIME_LIMIT_S / dt)):
    moving = [s for s in (0, 1) if gap > side[s].range + RANGE_EPSILON_M]
    if moving:
      closed = sum(min(side[s].speed * dt, gap - side[s].range) for s in moving)
      gap = max(gap - closed, min(side[s].range for s in moving))
    for s in (0, 1):
      if not engaged[s] and gap <= side[s].range + RANGE_EPSILON_M:
        engaged[s] = True
        for ship in range(len(hp[s])):
          schedule(s, ship, tick * dt + rng.uniform(0.0, side[s].interval))

    shots = queue.pop(tick, ())
    if not shots:
      continue
    weakest = [min(alive[1], key=hp[1].__getitem__), min(alive[0], key=hp[0].__getitem__)] if focus else None
    landed = []
    for s, ship in shots:
      if hp[s][ship] <= 0.0:
        continue
      enemy = 1 - s
      target = weakest[s] if focus else alive[enemy][rng.randrange(len(alive[enemy]))]
      landed.append((enemy, target, per_hit[s]))
      schedule(s, ship, when[s][ship] + side[s].interval)
    for enemy, target, damage in landed:
      hp[enemy][target] -= damage
    for s in (0, 1):
      alive[s] = [ship for ship in alive[s] if hp[s][ship] > 0.0]
    if not alive[0] or not alive[1]:
      break
  return (1 if alive[0] else 0) - (1 if alive[1] else 0)


def pair_record(task):
  """Wins and losses of `a` against `b` over the seeds, for one budget and fire mode."""
  a, b, budget, focus, seeds, dt = task
  outcomes = [fight(a, b, budget, focus, seed, dt) for seed in range(seeds)]
  return outcomes.count(1), outcomes.count(-1)


# ---- Matrices, the equilibrium mix and the Q2 check ------------------------------------------------------------------


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
  groups = (hulls, drives, [w for w in weapons if not w.splash])
  for g, group in enumerate(groups):
    for i, part in enumerate(group):
      for field in (f.name for f in dataclasses.fields(part) if f.name not in ("name", "splash")):
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


def stage(pool, label, parts, budgets, args, dt, unmodelled, failures, legs):
  """Runs (a) and (b) for one stage of a match, adding the counters it finds to `legs` for (c)."""
  designs = designs_from(*parts)
  print(f"\n==== {label}: {len(designs)} designs ====")
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
      for j, target in enumerate(designs):
        best = max((i for i in range(len(designs)) if i != j), key=lambda i: matrix[i][j])
        if matrix[best][j] < COUNTER_WIN_RATE:
          failures["a"].append(f"{where}: the best counter to {target.code} is {designs[best].code} "
                               f"at {matrix[best][j]:.0%}")
        if j in built:
          legs.add((designs[best].code, target.code, budget, focus))
          print(f"  {target.code:>9} is countered by {designs[best].code} ({matrix[best][j]:.0%})")
      used = [{getattr(designs[i], attribute) for i in built} for attribute in ("hull", "drive", "weapon")]
      for names, in_use, kind in zip(component_names(parts), used, ("hull", "drive", "weapon")):
        for name in names:
          if name not in in_use and name not in unmodelled:
            failures["b"].append(f"{where}: no design worth building uses the {name} {kind}")


def research_check(pool, parts, topics, budgets, args, dt, failures):
  """(d): each topic, with its prerequisites, researched by one side only, against the other side's starting designs."""
  answers = designs_from(*available(parts, topics, []))
  print(f"\n==== One-sided research (d): each topic against the {len(answers)} starting designs ====")
  for topic in topics:
    if (topic.target, topic.stat) in ECONOMY:
      print(f"  {topic.name}: no effect on a battle")
      continue
    researched = with_prerequisites(topic, topics)
    side = upgraded(available(parts, topics, researched), researched)
    designs = designs_from(*side)
    if topic.unlocks and not any(topic.unlocks in (d.hull, d.drive, d.weapon) for d in designs):
      print(f"  {topic.name}: not modelled")
      continue
    tasks, keys = [], []
    for budget in budgets:
      for focus in (False, True):
        for x in designs:
          for y in answers:
            tasks.append((y, x, budget, focus, args.seeds, dt))
            keys.append((budget, focus, x.code, y.code))
    best = {}
    for (budget, focus, x, y), (wins, _) in zip(keys, pool.map(pair_record, tasks, chunksize=4)):
      rate = wins / args.seeds
      if rate > best.get((budget, focus, x), ("", -1.0))[1]:
        best[(budget, focus, x)] = (y, rate)
    worst = min(best.items(), key=lambda item: item[1][1])
    (budget, focus, x), (y, rate) = worst
    chain = " + ".join(t.name for t in researched)
    print(f"  {chain}: weakest answer is {y} to {x}* at {rate:.0%} ({budget:,} Ore {'focus' if focus else 'spread'})")
    for (budget, focus, x), (y, rate) in sorted(best.items()):
      if rate < ROBUST_WIN_RATE:
        failures["d"].append(f"{chain}, {budget:,} Ore {'focus' if focus else 'spread'}: the best answer to "
                             f"{x}* is {y} at {rate:.0%}")


def run(args):
  text = args.doc.read_text(encoding="utf-8")
  tick_hz, hulls, drives, weapons = read_section_12(text)
  parts = apply_overrides((hulls, drives, weapons), args.set)
  topics = read_section_8(text, parts)
  hulls, drives, weapons = parts
  dt = 1.0 / tick_hz
  designs = designs_from(hulls, drives, weapons)
  budgets = [int(b) for b in args.budgets.split(",")]
  early = [int(b) for b in args.early_budgets.split(",")]
  unmodelled = [w.name for w in weapons if w.splash]
  start = available(parts, topics, [])

  print(f"Numbers from {args.doc.name} §12" + (f", with {', '.join(args.set)}" if args.set else "") +
        f". {len(designs)} designs, {args.seeds} battles per pairing, a {tick_hz:g} Hz tick.")
  print("Starting components: " + ", ".join(p.name for group in start for p in group) + ".")
  if unmodelled:
    print(f"Not modelled: {', '.join(unmodelled)} (splash needs ship sizes, §15).")
  print()
  print_designs(designs, hulls)
  if args.detail:
    print_shots_to_kill(designs)

  failures = {"a": [], "b": [], "c": [], "d": []}
  legs = set()
  with concurrent.futures.ProcessPoolExecutor(max_workers=args.jobs) as pool:
    stage(pool, "Every component", parts, budgets, args, dt, unmodelled, failures, legs)
    stage(pool, "Starting components", start, early, args, dt, unmodelled, failures, legs)
    research_check(pool, parts, topics, early, args, dt, failures)

    if not args.quick:
      by_code = {d.code: d for d in designs}
      checks = 0
      for label, new_hulls, new_drives, new_weapons in perturbed_parts(parts):
        changed = {d.code: d for d in designs_from(new_hulls, new_drives, new_weapons)}
        tasks, keys = [], []
        for counter, target, budget, focus in sorted(legs):
          if changed[counter] == by_code[counter] and changed[target] == by_code[target]:
            continue
          tasks.append((changed[counter], changed[target], budget, focus, args.robust_seeds, dt))
          keys.append((counter, target, budget, focus))
        for (counter, target, budget, focus), (wins, _) in zip(keys, pool.map(pair_record, tasks, chunksize=2)):
          checks += 1
          if wins / args.robust_seeds < ROBUST_WIN_RATE:
            failures["c"].append(f"{label}: {counter} no longer beats {target} at {budget:,} Ore "
                                 f"{'focus' if focus else 'spread'} fire ({wins / args.robust_seeds:.0%})")
      print(f"\nRobustness: {checks} counter checks under one-number changes of +/-5%.")

  print("\nQ2 check (§3):")
  verdicts = {
    "a": "every design has a counter that wins at least 80%",
    "b": "the designs worth building use every hull, drive and weapon",
    "c": "no counter stops winning when one number moves 5%",
    "d": "no research topic leaves a design without an answer that wins half the time",
  }
  failed = False
  for key, text in verdicts.items():
    if key == "c" and args.quick:
      print(f"  ({key}) {text}: not run (--quick)")
      continue
    status = "FAIL" if failures[key] else ("INCOMPLETE" if key == "b" and unmodelled else "PASS")
    failed |= bool(failures[key])
    suffix = f" ({', '.join(unmodelled)} not modelled)" if status == "INCOMPLETE" else ""
    print(f"  ({key}) {text}: {status}{suffix}")
    for line in failures[key]:
      print(f"      {line}")
  return 1 if failed else 0


def main():
  parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
  parser.add_argument("--doc", type=Path, default=DOC_DEFAULT, help="the design document to read §12 from")
  parser.add_argument("--budgets", default="2000,3000,4500,6000,9000,12000",
                      help="Ore per side for every component, comma-separated")
  parser.add_argument("--early-budgets", default="2000,3000,4500",
                      help="Ore per side for the starting components and the research check (d)")
  parser.add_argument("--seeds", type=int, default=60, help="battles per pairing for the matrices")
  parser.add_argument("--robust-seeds", type=int, default=30, help="battles per pairing for the robustness sweep")
  parser.add_argument("--jobs", type=int, default=None, help="worker processes (default: one per core)")
  parser.add_argument("--set", action="append", default=[], metavar="NAME.FIELD=VALUE",
                      help="override one §12 number, such as Small.hp=220 or Ion.hp_factor=1.0")
  parser.add_argument("--detail", action="store_true", help="print every matrix and the shots-to-kill table")
  parser.add_argument("--quick", action="store_true", help="skip the robustness sweep")
  return run(parser.parse_args())


if __name__ == "__main__":
  sys.exit(main())

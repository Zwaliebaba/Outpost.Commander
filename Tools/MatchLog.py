"""Summarizes the matches the game logged, for the owner's playtests: the MVP's Q1 and Q3 (plan task 6.3, design section
3), Phase 1's P1, P2 and P4 (Phase 1 plan task 13.1, Phase 1 design sections 2 and 10), Phase 2's S1 to S4 (Phase 2
plan task 19.1, Phase 2 design section 2), Phase 3's T1 to T4 (Phase 3 plan task 25.1, Phase 3 design section 2) and
Phase 4's U1 to U5 (Phase 4 plan task 34.1, Phase 4 design section 2).

The game adds every match against the AI to OutpostCommander-matches.log in the temporary folder, and its --ai-matches
switch writes ten seeded AI-against-AI matches to OutpostCommander-ai-matches.log there. Each line is one record,
every time in ticks:

  match seed <seed> ticks_per_second <rate>                       a match starts
  research <tick> player <player> topic <id> <name>               a player finished a research topic
  tier <tick> player <player> tier <tier>                         a player's Research Lab opened a tier (Phase 3 design §6)
  built <tick> player <player> hull <id> drive <id> weapon <id> <name>   a warship first appeared; a module ends its name
  fleet <tick> player <player> warships <count>                   a player's warships, every 30 seconds
  dry <tick> asteroid <id>                                        an ore asteroid ran dry
  peak <tick> player <player> warships <count>                    the most warships a player had at once
  contact <tick> sector <id>                                      the match's first shot
  engagement <tick> sector <id>                                   both sides fired in one sector
  sector <tick> sector <id> holder <player or 0>                  a sector's holder changed
  tickets <tick> player <player> tickets <count>                  a player's tickets, every 30 seconds
  upgrade <tick> player <player> structure <id> <kind> level <level> <started, finished or lost>
                                                                  a structure's level, from its owner's snapshots
  attacked <tick> player <owner> structure <id> <kind> level <level>   a structure above level 1 first shot at
  stall <tick> ticks <count>                                      the ticks both players sat at their caps with equal nodes
  salvaged <tick> player <player> derelict <id> ore <ore>         a player's Constructors salvaged a derelict
  pirates <tick> player <player> sector <id>                      a player first fought the pirates of a sector
  cleared <tick> sector <id>                                      the pirates no longer guard a sector
  retreat <tick> player <player> ship <id>                        a player's warship turned for home to be repaired
  repaired <tick> player <player> ship <id>                       a warship that turned for home stopped, whole
  again <tick> player <player> ship <id>                          a repaired warship fired, the first time since its repair
  ending <tick> <production or domination>                        how the match ended
  end <tick> winner <player, or 0 for a draw>                     the match ended
  left <tick>                                                     the match was left before it ended

For each match this prints its length and outcome (P1 and S4 ask for 45 to 60 minutes) and how it ended (S3), its first
shot (S1 asks for one by minute 5), its engagements before minute 20 and the sectors they were in (S2 asks for at least
five, in at least three sectors), the asteroids that ran dry, and for each player its research with the time it
finished, the times it opened each tier, its peak warship count (P4), its warships by design in each tier (P2 asks
whether each tier changes what gets built), and its warships by design in windows of the match. On a map with territory it
adds Phase 3's: how long the caps stalled the nodes (T2), each player's highest level of the Command Station, a Shipyard
and the Research Lab (T3), and the structures above level 1 attacked (T4). With Phase 4's records it adds the players'
first shot at each other and what each side salvaged and fought of the pirates before it (U1), each side's warships at
minute 20 (U2), the warships each side turned for home, had repaired and saw fight again (U3), and the engagements before
minute 40 (U4). With more than one match it ends with their lengths' median and spread, P1's and S4's repeatable figure,
S1's to S3's, T2's to T4's, and U1's to U5's.

Usage: python Tools/MatchLog.py [log] [--all] [--ai-matches] [--window-minutes N]

The log defaults to the matches against the AI in the temporary folder; --ai-matches reads the AI-against-AI log
instead, names both players AI, and shows every match. Only the last match is shown unless --all is given. The windows
are 5 minutes long by default.
"""

import argparse
import os
import statistics
import sys
import tempfile

LOG_NAME = "OutpostCommander-matches.log"
AI_MATCHES_LOG_NAME = "OutpostCommander-ai-matches.log"
PLAYER_NAMES = {1: "Player", 2: "AI"}
# The pirates' owner, as the log writes it (ADR-073).
PIRATES = 0xFFFFFFFF
AI_MATCHES_PLAYER_NAMES = {1: "AI 1", 2: "AI 2"}
P1_MINUTES = (45, 60)
# Phase 2 design section 2: contact by minute 5 (S1), and before minute 20 at least five engagements in at least three
# sectors (S2).
S1_MINUTES = 5
S2_MINUTES = 20
S2_ENGAGEMENTS = 5
S2_SECTORS = 3
# Phase 3 design section 2: the caps stall the nodes for under 5 minutes in the median match (T2); each side reaches level 3
# of the Command Station, a Shipyard and the Research Lab, and tier 3 is opened, in at least half the matches (T3); and in
# the median match at least one structure above level 1 is attacked (T4).
T2_MINUTES = 5
T3_LEVEL = 3
T3_KINDS = ("station", "shipyard", "lab")
T4_ATTACKS = 1
# Phase 4 design section 2: in the median match each side salvages at least 2 derelicts and fights at least 1 pirate
# outpost before the players first fire on each other, and that first shot comes between minute 8 and minute 20 in at
# least 30 of 40 matches (U1); neither side ever has more than 40 warships, and at minute 20 each has fewer than 20 (U2);
# each side sends at least 10 warships back to be repaired, and at least half of them fight again (U3); before minute 40
# there are at least five engagements, in at least three sectors (U4); and every match ends, by both endings (U5).
U1_SALVAGED = 2
U1_PIRATES = 1
U1_MINUTES = (8, 20)
U1_SHARE = 30 / 40
U2_MOST = 40
U2_MINUTE = 20
U2_AT_MINUTE = 20
U3_RETREATS = 10
U3_AGAIN_SHARE = 0.5
U4_MINUTES = 40
U4_ENGAGEMENTS = 5
U4_SECTORS = 3


class Match:
  def __init__(self, seed, ticks_per_second):
    self.seed = seed
    self.ticks_per_second = ticks_per_second
    self.research = []
    self.tiers = []
    self.built = []
    self.fleets = []
    self.dry = []
    self.peaks = {}
    self.contact = None
    self.engagements = []
    self.holders = []
    self.tickets = []
    self.upgrades = []
    self.attacked = []
    self.stall_ticks = None
    self.salvaged = []
    self.pirates = []
    self.cleared = []
    self.retreats = []
    self.repaired = []
    self.again = []
    self.ending = None
    self.end_tick = None
    self.winner = None
    self.left_tick = None

  def seconds(self, tick):
    return tick / self.ticks_per_second

  def last_tick(self):
    ticks = [record[0] for records in (self.research, self.tiers, self.built, self.fleets, self.dry, self.engagements, self.holders)
             for record in records]
    for tick in (self.end_tick, self.left_tick):
      if tick is not None:
        ticks.append(tick)
    return max(ticks, default=0)

  def tier_at(self, player, tick):
    """The highest tier the player had opened by the tick: 1 until its Research Lab opened the next."""
    return max((tier for at, owner, tier in self.tiers if owner == player and at <= tick), default=1)

  def early_engagements(self):
    """The engagements before S2's minute 20, and the sectors they were in."""
    early = [sector for tick, sector in self.engagements if self.seconds(tick) < S2_MINUTES * 60]
    return len(early), len(set(early))

  def s1(self):
    return self.contact is not None and self.seconds(self.contact[0]) <= S1_MINUTES * 60

  def s2(self):
    count, sectors = self.early_engagements()
    return count >= S2_ENGAGEMENTS and sectors >= S2_SECTORS

  def players(self):
    """The players, not the pirates, whose warships are written as anyone's are (Phase 4 design section 8)."""
    return sorted(({player for _, player, _ in self.research} | {player for _, player, _ in self.built} |
                   {player for _, player, _ in self.fleets}) - {PIRATES})

  def top_level(self, player, kind):
    """The highest level the player finished of a structure of the kind; 1 when it finished none."""
    return max((level for _, owner, _, what, level, how in self.upgrades if owner == player and what == kind and how == "finished"),
               default=1)

  def t3(self):
    """Each side reached level 3 of the Command Station, a Shipyard and the Research Lab."""
    players = self.players()
    return len(players) >= 2 and all(self.top_level(player, kind) >= T3_LEVEL for player in players for kind in T3_KINDS)

  def tier_three(self):
    """The players that opened tier 3."""
    return sorted({owner for _, owner, tier in self.tiers if tier >= 3})

  def phase_four(self):
    """Whether the log has Phase 4's records: a match on a map with derelicts and pirates leaves some."""
    return bool(self.salvaged or self.pirates or self.retreats)

  def before_contact(self, records, player):
    """The player's records before the players' first shot at each other, or all of them without one."""
    first = self.contact[0] if self.contact is not None else None
    return [record for record in records if record[1] == player and (first is None or record[0] < first)]

  def u1_side(self, player):
    """The derelicts the player salvaged and the pirate outposts it fought before the players' first shot."""
    return len(self.before_contact(self.salvaged, player)), len(self.before_contact(self.pirates, player))

  def u1_contact(self):
    low, high = U1_MINUTES
    return self.contact is not None and low * 60 <= self.seconds(self.contact[0]) <= high * 60

  def warships_at(self, player, minute):
    """The player's warships at the last 30-second count by the minute; None without one."""
    counts = [(tick, count) for tick, owner, count in self.fleets if owner == player and self.seconds(tick) <= minute * 60]
    return max(counts)[1] if counts else None

  def u3_side(self, player):
    """The warships the player turned for home, had repaired, and saw fight again, each counted once."""
    def ships(records):
      return {ship for _, owner, ship in records if owner == player}
    return len(ships(self.retreats)), len(ships(self.repaired)), len(ships(self.again))

  def u4(self):
    """The engagements before U4's minute 40, and the sectors they were in."""
    early = [sector for tick, sector in self.engagements if self.seconds(tick) < U4_MINUTES * 60]
    return len(early), len(set(early))


def clock(seconds):
  """A length as minutes and seconds, 6:13, as the game's banner writes it."""
  whole = int(seconds)
  hours, rest = divmod(whole, 3600)
  minutes, secs = divmod(rest, 60)
  return f"{hours}:{minutes:02}:{secs:02}" if hours else f"{minutes}:{secs:02}"


def read_matches(path):
  matches = []
  with open(path, encoding="utf-8") as log:
    for number, line in enumerate(log, start=1):
      words = line.split()
      if not words:
        continue
      try:
        if words[0] == "match":
          matches.append(Match(int(words[2]), int(words[4])))
          continue
        if not matches:
          raise ValueError("a record before the first match")
        match = matches[-1]
        if words[0] == "research":
          match.research.append((int(words[1]), int(words[3]), " ".join(words[6:])))
        elif words[0] == "tier":
          match.tiers.append((int(words[1]), int(words[3]), int(words[5])))
        elif words[0] == "built":
          match.built.append((int(words[1]), int(words[3]), " ".join(words[10:])))
        elif words[0] == "fleet":
          match.fleets.append((int(words[1]), int(words[3]), int(words[5])))
        elif words[0] == "dry":
          match.dry.append((int(words[1]), int(words[3])))
        elif words[0] == "peak":
          match.peaks[int(words[3])] = (int(words[1]), int(words[5]))
        elif words[0] == "contact":
          match.contact = (int(words[1]), int(words[3]))
        elif words[0] == "engagement":
          match.engagements.append((int(words[1]), int(words[3])))
        elif words[0] == "sector":
          match.holders.append((int(words[1]), int(words[3]), int(words[5])))
        elif words[0] == "tickets":
          match.tickets.append((int(words[1]), int(words[3]), int(words[5])))
        elif words[0] == "upgrade":
          match.upgrades.append((int(words[1]), int(words[3]), int(words[5]), words[6], int(words[8]), words[9]))
        elif words[0] == "attacked":
          match.attacked.append((int(words[1]), int(words[3]), int(words[5]), words[6], int(words[8])))
        elif words[0] == "stall":
          match.stall_ticks = int(words[3])
        elif words[0] == "salvaged":
          match.salvaged.append((int(words[1]), int(words[3]), int(words[5]), int(words[7])))
        elif words[0] == "pirates":
          match.pirates.append((int(words[1]), int(words[3]), int(words[5])))
        elif words[0] == "cleared":
          match.cleared.append((int(words[1]), int(words[3])))
        elif words[0] in ("retreat", "repaired", "again"):
          getattr(match, {"retreat": "retreats", "repaired": "repaired", "again": "again"}[words[0]]).append(
            (int(words[1]), int(words[3]), int(words[5])))
        elif words[0] == "ending":
          match.ending = words[2]
        elif words[0] == "end":
          match.end_tick = int(words[1])
          match.winner = int(words[3])
        elif words[0] == "left":
          match.left_tick = int(words[1])
        else:
          raise ValueError(f"unknown record {words[0]!r}")
      except (IndexError, ValueError) as error:
        raise SystemExit(f"{path}:{number}: {error}") from None
  return matches


def peak_of(match, player):
  """The player's peak warship count and its tick: the peak record, or the largest 30-second count without one."""
  if player in match.peaks:
    tick, count = match.peaks[player]
    return count, tick
  counts = [(count, tick) for tick, owner, count in match.fleets if owner == player]
  if not counts:
    return None
  return max(counts, key=lambda item: (item[0], -item[1]))


def counted(designs):
  counts = {}
  for design in designs:
    counts[design] = counts.get(design, 0) + 1
  return ", ".join(f"{count} {design}" for design, count in sorted(counts.items(), key=lambda item: (-item[1], item[0])))


def describe(match, window_minutes, names):
  lines = [f"Match with seed {match.seed}"]
  if match.end_tick is not None:
    length = match.seconds(match.end_tick)
    outcome = "a draw" if match.winner == 0 else f"won by {names.get(match.winner, f'player {match.winner}')}"
    low, high = P1_MINUTES
    within = "within" if low * 60 <= length <= high * 60 else "outside"
    how = f" by {match.ending}" if match.ending is not None else ""
    lines.append(f"  Ended at {clock(length)}, {outcome}{how}; {within} P1's and S4's {low} to {high} minutes")
  elif match.left_tick is not None:
    lines.append(f"  Left at {clock(match.seconds(match.left_tick))}, before it ended")
  else:
    lines.append(f"  Still running, or the game stopped, at {clock(match.seconds(match.last_tick()))}")
  if match.contact is not None:
    verdict = "within" if match.s1() else "after"
    lines.append(f"  First shot at {clock(match.seconds(match.contact[0]))} in sector {match.contact[1]}; {verdict} S1's "
                 f"{S1_MINUTES} minutes")
  elif match.holders or match.engagements:
    lines.append("  First shot: none")
  if match.holders or match.engagements:
    count, sectors = match.early_engagements()
    verdict = "meets" if match.s2() else "short of"
    lines.append(f"  Engagements before {S2_MINUTES}:00: {count}, in {sectors} sectors; {verdict} S2's {S2_ENGAGEMENTS} in "
                 f"{S2_SECTORS}. In all: {len(match.engagements)}")
    held = {}
    for tick, sector, holder in match.holders:
      held[sector] = holder
    nodes = {}
    for holder in held.values():
      if holder:
        nodes[holder] = nodes.get(holder, 0) + 1
    if nodes:
      lines.append("  Nodes held at the end: " + ", ".join(f"player {player} {count}" for player, count in sorted(nodes.items())))
  if match.dry:
    times = ", ".join(clock(match.seconds(tick)) for tick, _ in sorted(match.dry))
    lines.append(f"  Asteroids run dry: {len(match.dry)}, at {times}")
  else:
    lines.append("  Asteroids run dry: none")

  if match.stall_ticks is not None:
    stall = match.seconds(match.stall_ticks)
    verdict = "under" if stall < T2_MINUTES * 60 else "not under"
    lines.append(f"  Both at their caps with equal nodes: {clock(stall)}; {verdict} T2's {T2_MINUTES} minutes")
  if match.upgrades or match.attacked:
    attacked = ", ".join(f"{what} L{level} of player {owner} at {clock(match.seconds(tick))}"
                         for tick, owner, _, what, level in match.attacked)
    lines.append(f"  Structures above level 1 attacked: {len(match.attacked)}" + (f": {attacked}" if attacked else ""))

  if match.phase_four():
    if match.contact is not None:
      low, high = U1_MINUTES
      verdict = "within" if match.u1_contact() else "outside"
      lines.append(f"  The players' first shot at each other at {clock(match.seconds(match.contact[0]))}; {verdict} U1's minute "
                   f"{low} to {high}")
    if match.cleared:
      lines.append("  Pirate outposts cleared: " + ", ".join(f"sector {sector} at {clock(match.seconds(tick))}"
                                                             for tick, sector in match.cleared))
    count, sectors = match.u4()
    verdict = "meets" if count >= U4_ENGAGEMENTS and sectors >= U4_SECTORS else "short of"
    lines.append(f"  Engagements before {U4_MINUTES}:00: {count}, in {sectors} sectors; {verdict} U4's {U4_ENGAGEMENTS} in "
                 f"{U4_SECTORS}")

  players = match.players()
  window_seconds = window_minutes * 60
  for player in players:
    name = names.get(player, f"Player {player}")
    lines.append(f"  {name}")
    research = [(tick, topic) for tick, owner, topic in match.research if owner == player]
    if research:
      lines.append("    Research: " + ", ".join(f"{topic} {clock(match.seconds(tick))}" for tick, topic in research))
    else:
      lines.append("    Research: none")
    tiers = sorted((tier, tick) for tick, owner, tier in match.tiers if owner == player)
    if tiers:
      lines.append("    Tiers opened: " + ", ".join(f"{tier} at {clock(match.seconds(tick))}" for tier, tick in tiers))
    else:
      lines.append("    Tiers opened: none past tier 1")
    peak = peak_of(match, player)
    if peak is not None:
      lines.append(f"    Peak warships: {peak[0]}, at {clock(match.seconds(peak[1]))}")
    finished = [(tick, what, level) for tick, owner, _, what, level, how in match.upgrades if owner == player and how == "finished"]
    if finished:
      lines.append("    Levels: " + ", ".join(f"{what} L{level} at {clock(match.seconds(tick))}" for tick, what, level in finished))
    lost = sum(1 for _, owner, _, _, _, how in match.upgrades if owner == player and how == "lost")
    if lost:
      lines.append(f"    Upgrades lost with their structure: {lost}")
    if match.phase_four():
      salvaged, fought = match.u1_side(player)
      ore = sum(ore for _, owner, _, ore in match.salvaged if owner == player)
      lines.append(f"    Before the first shot: {salvaged} derelicts salvaged, {fought} pirate outposts fought (U1 asks "
                   f"{U1_SALVAGED} and {U1_PIRATES}); in all {len([1 for r in match.salvaged if r[1] == player])} salvaged for "
                   f"{ore} Ore")
      at_minute = match.warships_at(player, U2_MINUTE)
      if at_minute is not None:
        lines.append(f"    Warships at {U2_MINUTE}:00: {at_minute} (U2 asks under {U2_AT_MINUTE})")
      retreated, repaired, again = match.u3_side(player)
      lines.append(f"    Warships turned for home: {retreated}, repaired {repaired}, fought again {again} (U3 asks {U3_RETREATS}, "
                   f"and half of them again)")

    built = [(tick, design) for tick, owner, design in match.built if owner == player]
    if not built:
      lines.append("    Warships built: none")
      continue
    lines.append(f"    Warships built, {len(built)} in all, by the tier it had opened:")
    by_tier = {}
    for tick, design in built:
      by_tier.setdefault(match.tier_at(player, tick), []).append(design)
    for tier in sorted(by_tier):
      lines.append(f"      Tier {tier}: {counted(by_tier[tier])}")
    lines.append(f"    and by {window_minutes}-minute window:")
    windows = {}
    for tick, design in built:
      windows.setdefault(int(match.seconds(tick) // window_seconds), []).append(design)
    for window in sorted(windows):
      start = clock(window * window_seconds)
      stop = clock((window + 1) * window_seconds)
      lines.append(f"      {start}-{stop}: {counted(windows[window])}")
  return "\n".join(lines)


def summarize(matches):
  """P1's repeatable figure: the median length of the matches that ended, with the spread."""
  lengths = sorted(match.seconds(match.end_tick) for match in matches if match.end_tick is not None)
  unfinished = len(matches) - len(lengths)
  lines = [f"{len(matches)} matches, {len(lengths)} ended"]
  if lengths:
    low, high = P1_MINUTES
    within = sum(1 for length in lengths if low * 60 <= length <= high * 60)
    lines.append(f"  Length: median {clock(statistics.median(lengths))}, from {clock(lengths[0])} to {clock(lengths[-1])}; "
                 f"{within} within P1's {low} to {high} minutes")
  if unfinished:
    stops = ", ".join(clock(match.seconds(match.last_tick())) for match in matches if match.end_tick is None)
    lines.append(f"  Not ended: {unfinished}, stopped at {stops}")
  peaks = []
  for match in matches:
    for player in sorted(set(match.peaks) | {owner for _, owner, _ in match.fleets}):
      peak = peak_of(match, player)
      if peak is not None:
        peaks.append(peak[0])
  if peaks:
    lines.append(f"  Peak warships of a side: median {statistics.median(peaks):g}, from {min(peaks)} to {max(peaks)}")
  territorial = [match for match in matches if match.holders or match.engagements]
  if territorial:
    contacts = sum(1 for match in territorial if match.s1())
    lines.append(f"  S1, a shot by minute {S1_MINUTES}: {contacts} of {len(territorial)}")
    early = sorted(match.early_engagements()[0] for match in territorial)
    sectors = sorted(match.early_engagements()[1] for match in territorial)
    met = sum(1 for match in territorial if match.s2())
    lines.append(f"  S2, engagements before {S2_MINUTES}:00: median {statistics.median(early):g} in a median "
                 f"{statistics.median(sectors):g} sectors; {met} of {len(territorial)} meet {S2_ENGAGEMENTS} in {S2_SECTORS}")
    endings = {}
    for match in territorial:
      if match.ending is not None:
        endings[match.ending] = endings.get(match.ending, 0) + 1
    if endings:
      lines.append("  S3, endings: " + ", ".join(f"{count} by {ending}" for ending, count in sorted(endings.items())))
  stalls = sorted(match.seconds(match.stall_ticks) for match in matches if match.stall_ticks is not None)
  if stalls:
    verdict = "meets" if statistics.median(stalls) < T2_MINUTES * 60 else "misses"
    lines.append(f"  T2, both at their caps with equal nodes: median {clock(statistics.median(stalls))}, from {clock(stalls[0])} "
                 f"to {clock(stalls[-1])}; {verdict} T2's under {T2_MINUTES} minutes")
  graded = [match for match in matches if match.upgrades or match.stall_ticks is not None]
  if graded:
    half = len(graded) / 2
    levels = sum(1 for match in graded if match.t3())
    either = sum(1 for match in graded if match.tier_three())
    both = sum(1 for match in graded if len(match.tier_three()) >= 2)
    seats = sum(len(match.tier_three()) for match in graded)
    per_kind = ", ".join(
      f"{kind} {sum(1 for match in graded if all(match.top_level(p, kind) >= T3_LEVEL for p in match.players()))}"
      for kind in T3_KINDS)
    lines.append(f"  T3, each side at level {T3_LEVEL} of all three: {levels} of {len(graded)} ({per_kind}); tier 3 opened in "
                 f"{either}, by both sides in {both}, {seats} of {2 * len(graded)} seats; "
                 f"{'meets' if levels >= half and either >= half else 'misses'} T3's half")
    attacks = sorted(len(match.attacked) for match in graded)
    verdict = "meets" if statistics.median(attacks) >= T4_ATTACKS else "misses"
    lines.append(f"  T4, structures above level 1 attacked: median {statistics.median(attacks):g}, from {attacks[0]} to "
                 f"{attacks[-1]}; {verdict} T4's {T4_ATTACKS}")
  lines.extend(summarize_phase_four([match for match in matches if match.phase_four()]))
  return "\n".join(lines)


def summarize_phase_four(matches):
  """U1 to U5 over the matches with Phase 4's records, each side's figure taken at the median."""
  if not matches:
    return []
  lines = []
  sides = [(match, player) for match in matches for player in match.players()]
  salvaged = statistics.median(match.u1_side(player)[0] for match, player in sides)
  fought = statistics.median(match.u1_side(player)[1] for match, player in sides)
  timely = sum(1 for match in matches if match.u1_contact())
  low, high = U1_MINUTES
  met = salvaged >= U1_SALVAGED and fought >= U1_PIRATES and timely >= U1_SHARE * len(matches)
  lines.append(f"  U1, before the players' first shot a side salvages a median {salvaged:g} and fights a median {fought:g} pirate "
               f"outposts; the shot comes between minute {low} and {high} in {timely} of {len(matches)}; "
               f"{'meets' if met else 'misses'} U1")
  most = statistics.median(max((peak_of(match, player) or (0, 0))[0] for player in match.players()) for match in matches)
  at_minute = statistics.median(max(match.warships_at(player, U2_MINUTE) or 0 for player in match.players()) for match in matches)
  met = most <= U2_MOST and at_minute < U2_AT_MINUTE
  lines.append(f"  U2, the larger side's peak: median {most:g}; its warships at {U2_MINUTE}:00: median {at_minute:g}; "
               f"{'meets' if met else 'misses'} U2's {U2_MOST} and under {U2_AT_MINUTE}")
  retreated = statistics.median(match.u3_side(player)[0] for match, player in sides)
  again = statistics.median(match.u3_side(player)[2] for match, player in sides)
  repaired = statistics.median(match.u3_side(player)[1] for match, player in sides)
  met = retreated >= U3_RETREATS and again >= U3_AGAIN_SHARE * retreated
  lines.append(f"  U3, warships a side turns for home: median {retreated:g}, repaired {repaired:g}, fight again {again:g}; "
               f"{'meets' if met else 'misses'} U3's {U3_RETREATS} and half again")
  engagements = statistics.median(match.u4()[0] for match in matches)
  sectors = statistics.median(match.u4()[1] for match in matches)
  met = engagements >= U4_ENGAGEMENTS and sectors >= U4_SECTORS
  lines.append(f"  U4, engagements before {U4_MINUTES}:00: median {engagements:g} in a median {sectors:g} sectors; "
               f"{'meets' if met else 'misses'} U4's {U4_ENGAGEMENTS} in {U4_SECTORS}")
  endings = {}
  for match in matches:
    if match.ending is not None:
      endings[match.ending] = endings.get(match.ending, 0) + 1
  ended = sum(endings.values())
  met = ended == len(matches) and len(endings) >= 2
  how = ", ".join(f"{count} by {ending}" for ending, count in sorted(endings.items())) or "none"
  lines.append(f"  U5, {ended} of {len(matches)} ended ({how}); {'meets' if met else 'misses'} U5's every match, by both endings")
  return lines


def main():
  parser = argparse.ArgumentParser(description="Summarizes the matches the game logged.")
  parser.add_argument("log", nargs="?", help="the log to read; by default the game's, in the temporary folder")
  parser.add_argument("--all", action="store_true", help="every match in the log, not only the last")
  parser.add_argument("--ai-matches", action="store_true", help="the AI-against-AI matches of the --ai-matches switch")
  parser.add_argument("--window-minutes", type=int, default=5)
  arguments = parser.parse_args()
  if arguments.window_minutes <= 0:
    parser.error("--window-minutes must be at least 1")

  default_name = AI_MATCHES_LOG_NAME if arguments.ai_matches else LOG_NAME
  path = arguments.log or os.path.join(tempfile.gettempdir(), default_name)
  if not os.path.isfile(path):
    what = "run the game with --ai-matches" if arguments.ai_matches else "play a match against the AI"
    print(f"No match log at {path}: {what} first.", file=sys.stderr)
    return 1
  matches = read_matches(path)
  if not matches:
    print(f"{path} holds no match.", file=sys.stderr)
    return 1
  names = AI_MATCHES_PLAYER_NAMES if arguments.ai_matches else PLAYER_NAMES
  shown = matches if arguments.all or arguments.ai_matches else matches[-1:]
  print("\n\n".join(describe(match, arguments.window_minutes, names) for match in shown))
  if len(shown) > 1:
    print("\n" + summarize(shown))
  return 0


if __name__ == "__main__":
  sys.exit(main())

"""Summarizes the matches the game logged, for the owner's playtests: the MVP's Q1 and Q3 (plan task 6.3, design section
3), Phase 1's P1, P2 and P4 (Phase 1 plan task 13.1, Phase 1 design sections 2 and 10) and Phase 2's S1 to S4 (Phase 2
plan task 19.1, Phase 2 design section 2).

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
  ending <tick> <production or domination>                        how the match ended
  end <tick> winner <player, or 0 for a draw>                     the match ended
  left <tick>                                                     the match was left before it ended

For each match this prints its length and outcome (P1 and S4 ask for 45 to 60 minutes) and how it ended (S3), its first
shot (S1 asks for one by minute 5), its engagements before minute 20 and the sectors they were in (S2 asks for at least
five, in at least three sectors), the asteroids that ran dry, and for each player its research with the time it
finished, the times it opened each tier, its peak warship count (P4), its warships by design in each tier (P2 asks
whether each tier changes what gets built), and its warships by design in windows of the match. With more than one match
it ends with their lengths' median and spread, P1's and S4's repeatable figure, and S1's to S3's.

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
AI_MATCHES_PLAYER_NAMES = {1: "AI 1", 2: "AI 2"}
P1_MINUTES = (45, 60)
# Phase 2 design section 2: contact by minute 5 (S1), and before minute 20 at least five engagements in at least three
# sectors (S2).
S1_MINUTES = 5
S2_MINUTES = 20
S2_ENGAGEMENTS = 5
S2_SECTORS = 3


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

  players = sorted({player for _, player, _ in match.research} | {player for _, player, _ in match.built} |
                   {player for _, player, _ in match.fleets})
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
  return "\n".join(lines)


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

"""Summarizes the matches the game logged against the AI, for the Q1 and Q3 playtests (plan task 6.3, design section 3).

The game adds every match against the AI to OutpostCommander-matches.log in the temporary folder. Each line is one
record, every time in ticks:

  match seed <seed> ticks_per_second <rate>                       a match starts
  research <tick> player <player> topic <id> <name>               a player finished a research topic
  built <tick> player <player> hull <id> drive <id> weapon <id> <name>   a warship first appeared
  end <tick> winner <player, or 0 for a draw>                     the match ended
  left <tick>                                                     the match was left before it ended

Player 1 is the human and player 2 the AI. For each match this prints its length and outcome (Q1 asks for 15 to 25
minutes), each player's research with the time it finished, and each player's warships by design in windows of the
match (Q3 asks whether research choices change what gets built in the mid-game).

Usage: python Tools/MatchLog.py [log] [--all] [--window-minutes N]

The log defaults to the one in the temporary folder. Only the last match is shown unless --all is given. The windows
are 5 minutes long by default.
"""

import argparse
import os
import sys
import tempfile

LOG_NAME = "OutpostCommander-matches.log"
PLAYER_NAMES = {1: "Player", 2: "AI"}
Q1_MINUTES = (15, 25)


class Match:
  def __init__(self, seed, ticks_per_second):
    self.seed = seed
    self.ticks_per_second = ticks_per_second
    self.research = []
    self.built = []
    self.end_tick = None
    self.winner = None
    self.left_tick = None

  def seconds(self, tick):
    return tick / self.ticks_per_second

  def last_tick(self):
    ticks = [tick for tick, _, _ in self.research] + [tick for tick, _, _ in self.built]
    for tick in (self.end_tick, self.left_tick):
      if tick is not None:
        ticks.append(tick)
    return max(ticks, default=0)


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
        elif words[0] == "built":
          match.built.append((int(words[1]), int(words[3]), " ".join(words[10:])))
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


def describe(match, window_minutes):
  lines = [f"Match with seed {match.seed}"]
  if match.end_tick is not None:
    length = match.seconds(match.end_tick)
    outcome = "a draw" if match.winner == 0 else f"won by {PLAYER_NAMES.get(match.winner, f'player {match.winner}')}"
    low, high = Q1_MINUTES
    within = "within" if low * 60 <= length <= high * 60 else "outside"
    lines.append(f"  Ended at {clock(length)}, {outcome}; {within} Q1's {low} to {high} minutes")
  elif match.left_tick is not None:
    lines.append(f"  Left at {clock(match.seconds(match.left_tick))}, before it ended")
  else:
    lines.append(f"  Still running, or the game stopped, at {clock(match.seconds(match.last_tick()))}")

  players = sorted({player for _, player, _ in match.research} | {player for _, player, _ in match.built})
  window_seconds = window_minutes * 60
  for player in players:
    name = PLAYER_NAMES.get(player, f"Player {player}")
    lines.append(f"  {name}")
    research = [(tick, topic) for tick, owner, topic in match.research if owner == player]
    if research:
      lines.append("    Research: " + ", ".join(f"{topic} {clock(match.seconds(tick))}" for tick, topic in research))
    else:
      lines.append("    Research: none")

    built = [(tick, design) for tick, owner, design in match.built if owner == player]
    if not built:
      lines.append("    Warships built: none")
      continue
    lines.append(f"    Warships built, {len(built)} in all, by {window_minutes}-minute window:")
    windows = {}
    for tick, design in built:
      window = int(match.seconds(tick) // window_seconds)
      windows.setdefault(window, {})
      windows[window][design] = windows[window].get(design, 0) + 1
    for window in sorted(windows):
      start = clock(window * window_seconds)
      stop = clock((window + 1) * window_seconds)
      counts = ", ".join(f"{count} {design}" for design, count in sorted(windows[window].items(), key=lambda item: (-item[1], item[0])))
      lines.append(f"      {start}-{stop}: {counts}")
  return "\n".join(lines)


def main():
  parser = argparse.ArgumentParser(description="Summarizes the matches the game logged against the AI.")
  parser.add_argument("log", nargs="?", default=os.path.join(tempfile.gettempdir(), LOG_NAME))
  parser.add_argument("--all", action="store_true", help="every match in the log, not only the last")
  parser.add_argument("--window-minutes", type=int, default=5)
  arguments = parser.parse_args()
  if arguments.window_minutes <= 0:
    parser.error("--window-minutes must be at least 1")

  if not os.path.isfile(arguments.log):
    print(f"No match log at {arguments.log}: play a match against the AI first.", file=sys.stderr)
    return 1
  matches = read_matches(arguments.log)
  if not matches:
    print(f"{arguments.log} holds no match.", file=sys.stderr)
    return 1
  shown = matches if arguments.all else matches[-1:]
  print("\n\n".join(describe(match, arguments.window_minutes) for match in shown))
  return 0


if __name__ == "__main__":
  sys.exit(main())

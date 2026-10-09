"""Summarizes a world's log, for the owner's week (Phase 5 design sections 2 and 10, ADR-082): W1's starts, recoveries
and saves, and W4's half that the world log can answer, each seat's income, fleet and research in the hours its player
played it against the hours its deputy did. It also counts each seat's battles by whether its player was present, and
the losses, waits and restarts of a world without an end (ADR-084).

The dedicated server writes World.log in the world's folder, and --world-run one in each of its two worlds' folders. Each
line is one record, every time in ticks; WorldLog.h lists them.

  python Tools/WorldLog.py <world folder or World.log>
"""

import argparse
import os
import statistics
import sys

TICKS_PER_SECOND = 20
HOUR_SECONDS = 3600


def read(path):
  """Each record of the log as its words."""
  with open(path, encoding="utf-8") as log:
    return [line.split() for line in log if line.strip()]


def summarize(records):
  lines = []
  starts = [words for words in records if words[0] == "start"]
  recovered = [words for words in starts if words[2] == "recovered"]
  saves = [words for words in records if words[0] == "save"]
  last = max((int(words[1]) for words in records if words[1].isdigit()), default=0)
  lines.append(f"{last / TICKS_PER_SECOND / HOUR_SECONDS:.1f} hours of world, {len(starts)} starts, {len(recovered)} of them recoveries")
  for words in recovered:
    lines.append(f"  came back at tick {words[1]} from the save of tick {words[4]} and {words[6]} commands logged after it")
  if saves:
    encode = sorted(int(words[5]) for words in saves)
    size = sorted(int(words[3]) for words in saves)
    lines.append(f"W2, {len(saves)} saves: encoding median {statistics.median(encode) / 1000:.2f} ms, at most {encode[-1] / 1000:.2f} ms; "
                 f"{size[0] // 1000}-{size[-1] // 1000} KB")

  hours = [words for words in records if words[0] == "hour"]
  for player in sorted({words[3] for words in hours}):
    seat = [words for words in hours if words[3] == player]
    present = [words for words in seat if int(words[17]) * 2 >= HOUR_SECONDS]
    deputy = [words for words in seat if int(words[19]) * 2 >= HOUR_SECONDS]
    lines.append(f"Player {player}: {len(seat)} hours, {len(present)} mostly its player's, {len(deputy)} mostly its deputy's")

    def rates(group, label):
      if not group:
        return
      income = statistics.mean(int(words[7]) / 100 for words in group)
      fleet = statistics.mean(int(words[9]) / max(int(words[11]), 1) for words in group)
      gained = []
      for words in group:
        earlier = [other for other in seat if int(other[1]) < int(words[1])]
        if earlier:
          gained.append(int(words[15]) - int(earlier[-1][15]))
      research = f", {statistics.mean(gained):.1f} topics an hour" if gained else ""
      lines.append(f"  W4, {label}: income {income:.1f} Ore a second, fleet {fleet:.0%} of its cap{research}; bank at the last "
                   f"{int(group[-1][5]):,} Ore")

    rates(present, "its player's hours")
    rates(deputy, "its deputy's hours")
    battles = [words for words in records if words[0] == "battle" and player in words[6::3]]
    fought = {}
    for words in battles:
      play = words[words.index(player, 6) + 1]
      fought[play] = fought.get(play, 0) + 1
    if battles:
      lines.append("  battles: " + ", ".join(f"{count} {play}" for play, count in sorted(fought.items())))
    for kind in ("lost", "waiting", "restart"):
      count = sum(1 for words in records if words[0] == kind and words[3] == player)
      if count:
        lines.append(f"  {kind}: {count}")
  return lines


def main():
  parser = argparse.ArgumentParser(description="Summarizes a world's log.")
  parser.add_argument("world", help="the world's folder, or its World.log")
  arguments = parser.parse_args()
  path = os.path.join(arguments.world, "World.log") if os.path.isdir(arguments.world) else arguments.world
  if not os.path.isfile(path):
    print(f"No world log at {path}.", file=sys.stderr)
    return 1
  print("\n".join(summarize(read(path))))
  return 0


if __name__ == "__main__":
  sys.exit(main())

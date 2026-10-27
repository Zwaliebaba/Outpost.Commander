"""Summarizes a universe's log, for the owner's week (ADR-082, Game §universe.log): its starts, recoveries and saves, and
what the log can tell of what a player's presence buys, each seat's income, fleet and research in the hours its player
played it against the hours its deputy did. It also counts each seat's battles by whether its player was present, and
the losses, waits and restarts of a universe without an end (ADR-084). What it says of the server itself: its saves'
writes, its minutes with a long tick and the ticks it dropped, every join by its outcome and where it came from, each
seat's orders, refusals and bytes an hour, and the overseer's comings and goings.

The dedicated server writes Universe.log in the universe's folder, and --universe-run one in each of its two universes' folders. Each
line is one record, every time in ticks; UniverseLog.h lists them.

  python Tools/UniverseLog.py <universe folder or Universe.log>
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
  elapsed_hours = last / TICKS_PER_SECOND / HOUR_SECONDS
  lines.append(f"{elapsed_hours:.1f} hours of the universe, {len(starts)} starts, {len(recovered)} of them recoveries")
  for words in recovered:
    lines.append(f"  came back at tick {words[1]} from the save of tick {words[4]} and {words[6]} commands logged after it")
  if saves:
    encode = sorted(int(words[5]) for words in saves)
    size = sorted(int(words[3]) for words in saves)
    lines.append(f"W2, {len(saves)} saves: encoding median {statistics.median(encode) / 1000:.2f} ms, at most {encode[-1] / 1000:.2f} ms; "
                 f"{size[0] // 1000}-{size[-1] // 1000} KB")
  written = sorted(int(words[3]) for words in records if words[0] == "written")
  if written:
    lines.append(f"  written by the folder's thread: median {statistics.median(written) / 1000:.2f} ms, at most {written[-1] / 1000:.2f} ms")
  lines.extend(server_lines(records))

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
    # The commands applied for the seat and refused it, and the bytes sent its player, over the hours that have them.
    def field(words, name):
      return int(words[words.index(name) + 1])
    counted = [words for words in seat if "orders" in words]
    if counted:
      orders = sum(field(words, "orders") for words in counted)
      refused = sum(field(words, "refused") for words in counted)
      lines.append(f"  orders: {orders / len(counted):.0f} an hour, {refused / max(orders, 1):.1%} of them refused")
    sent = [words for words in seat if "sent_bytes" in words]
    if sent:
      lines.append(f"  sent: {statistics.mean(field(words, 'sent_bytes') for words in sent) / 1e6:.1f} MB an hour its player was connected")
    # The player's connection, over the hours it was sampled in (ADR-004).
    sampled = [words for words in seat if "rtt_median_us" in words]
    if sampled:
      def field(words, name):
        return int(words[words.index(name) + 1])
      sent = sum(field(words, "sent") for words in sampled)
      lost = sum(field(words, "lost") for words in sampled)
      lines.append(f"  connection, {len(sampled)} hours: round trip median of the hours {statistics.median(field(words, 'rtt_median_us') for words in sampled) / 1000:.1f} ms, "
                   f"the worst hour's 95th percentile {max(field(words, 'rtt_p95_us') for words in sampled) / 1000:.1f} ms; "
                   f"{lost / sent if sent else 0:.2%} of {sent} packets lost; largest backlog "
                   f"{max(field(words, 'backlog_most') for words in sampled) / 1024:.1f} KiB")
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
    # How long past its hour each restart waited for its start to be free.
    waits = sorted(int(words[5]) / TICKS_PER_SECOND for words in records if words[0] == "restart" and words[3] == player and len(words) > 5)
    if waits:
      lines.append(f"  restarts waited: {sum(1 for wait in waits if wait > 0)} of {len(waits)}, median {statistics.median(waits):.1f} s, "
                   f"at most {waits[-1]:.1f} s")
  return lines


def server_lines(records):
  """The server's own records: its long minutes and dropped ticks, and every join by its outcome and address."""
  lines = []
  long_minutes = [words for words in records if words[0] == "tick"]
  if long_minutes:
    longest = max(long_minutes, key=lambda words: int(words[3]))
    # The part that took most of each minute's longest tick, where one took a quarter of it or more.
    leading = {}
    for words in long_minutes:
      if len(words) > 6:
        leading[words[6]] = leading.get(words[6], 0) + 1
    parts = ", ".join(f"{part} {count}" for part, count in sorted(leading.items(), key=lambda item: -item[1]))
    lines.append(f"{len(long_minutes)} minutes with a tick of 5 ms or more, {sum(int(words[5]) for words in long_minutes)} such ticks; "
                 f"the longest {int(longest[3]) / 1000:.1f} ms at tick {longest[1]}" + (f"; most of it: {parts}" if parts else ""))
  dropped = [words for words in records if words[0] == "dropped"]
  if dropped:
    ticks = sum(int(words[3]) for words in dropped)
    lines.append(f"{ticks} ticks dropped after stalls, {ticks / TICKS_PER_SECOND:.1f} s of the universe, in {len(dropped)} minutes")
  joins = [words for words in records if words[0] == "join"]
  if joins:
    outcomes = {}
    sources = {}
    for words in joins:
      # welcomed, refused <why> or gone <how>, and where it came from, the record's last word.
      outcome = words[2] if words[2] == "welcomed" else f"{words[2]} {words[3] if words[2] == 'refused' else words[5]}"
      outcomes[outcome] = outcomes.get(outcome, 0) + 1
      if words[2] == "refused":
        sources.setdefault(outcome, set()).add(words[-1])
    lines.append("joins: " + ", ".join(f"{count} {outcome}" for outcome, count in sorted(outcomes.items())))
    for outcome, addresses in sorted(sources.items()):
      lines.append(f"  {outcome} from " + ", ".join(sorted(addresses)))
  lines += overseer_lines(records)
  return lines


def overseer_lines(records):
  """The overseer's comings and goings (ADR-101): how often its channel opened, how long it was open, and every refusal by its
  reason and address."""
  if not any(words[0] == "overseer" for words in records):
    return []
  openings = 0
  held = 0
  since = 0
  open_ticks = 0
  refusals = {}
  sources = {}
  for words in records:
    tick = int(words[1])
    # A server made afresh holds no channel, and the time of one open as it went is not known.
    if words[0] == "start":
      held = 0
    if words[0] != "overseer":
      continue
    if words[2] == "opened":
      openings += 1
      if held == 0:
        since = tick
      held += 1
    elif words[2] == "closed" and held > 0:
      held -= 1
      if held == 0:
        open_ticks += tick - since
    elif words[2] == "refused":
      refusals[words[3]] = refusals.get(words[3], 0) + 1
      sources.setdefault(words[3], set()).add(words[-1])
  if held > 0:
    open_ticks += int(records[-1][1]) - since
  refused = ", ".join(f"{count} {why}" for why, count in sorted(refusals.items()))
  lines = [f"overseer: opened {openings} times, open {open_ticks / TICKS_PER_SECOND / HOUR_SECONDS:.1f} h of universe time; refused "
           + (refused or "none")]
  for why, addresses in sorted(sources.items()):
    lines.append(f"  refused {why} from " + ", ".join(sorted(addresses)))
  return lines


def main():
  parser = argparse.ArgumentParser(description="Summarizes a universe's log.")
  parser.add_argument("universe", help="the universe's folder, or its Universe.log")
  arguments = parser.parse_args()
  path = os.path.join(arguments.universe, "Universe.log") if os.path.isdir(arguments.universe) else arguments.universe
  if not os.path.isfile(path):
    print(f"No universe log at {path}.", file=sys.stderr)
    return 1
  print("\n".join(summarize(read(path))))
  return 0


if __name__ == "__main__":
  sys.exit(main())

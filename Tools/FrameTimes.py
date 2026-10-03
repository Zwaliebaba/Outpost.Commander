"""Summarizes a measurement log the game wrote with --measure (task 2.7 and task 3.7, design section 3).

The game writes OutpostCommander-measure.log to the temporary folder. Each line is one record:

  seed <seed> load <0|1> stress <0|1>         the run's switches
  display <width> <height> refresh_hz <hz>    the back buffer's size and the display's refresh rate, when they change
  frame_cpu_ns <ns>                           one frame's CPU work, from the swap chain's wait to Present's return
  frame_gpu_ns <ns>                           one frame's GPU work, between timestamps at its command list's ends
  tick_ns <ns>                                one server tick
  tick_part_names <name> ...                  the parts of a tick, in the order tick_parts_ns gives them (task 8.1)
  tick_parts_ns <ns> ...                      the tick just logged, part by part; the parts nest (GameProtocol/Server.h)
  response_ns <ns> input_read_ns ... presented_ns ...   Q5: a move order to its first visible response
  startup_ns <stage> <ns>                     when a stage of startup was done, from the start of wWinMain (ADR-049)

Usage: python Tools/FrameTimes.py [log] [--skip-frames N]

The log defaults to the one in the temporary folder. The first N frames, 120 by default, are left out, since they hold
loading and the switch to full screen; ticks and responses are all kept.
"""

import argparse
import math
import os
import sys
import tempfile

LOG_NAME = "OutpostCommander-measure.log"
FRAME_TARGET_MS = 1000.0 / 60.0
FRAME_SHARE = 0.99
TICK_TARGET_MS = 5.0
RESPONSE_TARGET_MS = 150.0


def percentile(values_sorted, share):
  """The nearest-rank percentile of a sorted list."""
  rank = max(1, math.ceil(share * len(values_sorted)))
  return values_sorted[rank - 1]


def describe(name, values_ms, target_ms):
  if not values_ms:
    return f"{name}: none recorded"
  values = sorted(values_ms)
  mean = sum(values) / len(values)
  within = sum(1 for value in values if value <= target_ms)
  return (f"{name}: {len(values)} recorded; mean {mean:.2f} ms, median {percentile(values, 0.5):.2f}, "
          f"95th percentile {percentile(values, 0.95):.2f}, 99th {percentile(values, 0.99):.2f}, "
          f"worst {values[-1]:.2f}; {within / len(values):.1%} within {target_ms:.1f} ms")


def describe_parts(names, ticks, title):
  """Each part's mean and worst over the given ticks, the heaviest mean first."""
  if not ticks:
    return [f"{title}: none"]
  lines = [f"{title}, {len(ticks)} ticks (parts nest: commands holds group_route and ship_paths, and graph_build is "
           f"counted inside whichever part built the graph):"]
  rows = []
  for index, name in enumerate(names):
    values = [tick[index] for tick in ticks]
    rows.append((sum(values) / len(values), max(values), name))
  for mean, worst, name in sorted(rows, reverse=True):
    lines.append(f"  {name:<12} mean {mean:7.3f} ms, worst {worst:7.3f} ms")
  return lines


def main():
  parser = argparse.ArgumentParser(description="Summarizes the game's measurement log.")
  parser.add_argument("log", nargs="?", default=os.path.join(tempfile.gettempdir(), LOG_NAME))
  parser.add_argument("--skip-frames", type=int, default=120)
  arguments = parser.parse_args()

  header = ""
  displays = []
  cpu_ms = []
  gpu_ms = []
  tick_ms = []
  part_names = []
  # Each tick's parts in ms, paired with its total.
  tick_parts = []
  response_ms = []
  # Startup's stages in the order they were logged, each with its time from the start of wWinMain.
  startup = []
  with open(arguments.log, encoding="utf-8") as log:
    for line in log:
      fields = line.split()
      if not fields:
        continue
      if fields[0] == "seed":
        header = line.strip()
      elif fields[0] == "display":
        displays.append(f"{fields[1]}x{fields[2]} at {fields[4]} Hz")
      elif fields[0] == "frame_cpu_ns":
        cpu_ms.append(int(fields[1]) / 1e6)
      elif fields[0] == "frame_gpu_ns":
        gpu_ms.append(int(fields[1]) / 1e6)
      elif fields[0] == "tick_ns":
        tick_ms.append(int(fields[1]) / 1e6)
      elif fields[0] == "tick_part_names":
        part_names = fields[1:]
      elif fields[0] == "tick_parts_ns" and tick_ms:
        tick_parts.append((tick_ms[-1], [int(value) / 1e6 for value in fields[1:]]))
      elif fields[0] == "response_ns":
        response_ms.append(int(fields[1]) / 1e6)
      elif fields[0] == "startup_ns":
        startup.append((fields[1], int(fields[2]) / 1e6))

  cpu_ms = cpu_ms[arguments.skip_frames:]
  gpu_ms = gpu_ms[arguments.skip_frames:]
  print(f"{arguments.log}: {header}")
  print(f"Back buffer and display: {', '.join(displays) if displays else 'not recorded'}")
  if startup:
    # The server, the models and the interface are made on threads of their own, so their times overlap the window's and
    # the device's; each is when that stage was done, not how long it took.
    print("Startup, each stage when it was done: " + ", ".join(f"{stage} {at_ms:.1f} ms" for stage, at_ms in startup))
  print(describe("Frame CPU work", cpu_ms, FRAME_TARGET_MS))
  print(describe("Frame GPU work", gpu_ms, FRAME_TARGET_MS))
  print(describe("Tick", tick_ms, TICK_TARGET_MS))
  if response_ms:
    print(describe("Order to response", response_ms, RESPONSE_TARGET_MS))
  if part_names and tick_parts:
    # Where the time goes over the whole run, and in the ticks over the target, which are the ones to fix (task 8.1).
    every = [parts for _, parts in tick_parts]
    slow = [parts for total, parts in tick_parts if total > TICK_TARGET_MS]
    for line in describe_parts(part_names, every, "Tick parts, every tick"):
      print(line)
    for line in describe_parts(part_names, slow, f"Tick parts, ticks over {TICK_TARGET_MS:.0f} ms"):
      print(line)

  # Q4: 99% of frames within 16.7 ms, on the CPU and on the GPU, and every tick within 5 ms.
  if cpu_ms and gpu_ms and tick_ms:
    frames_met = all(percentile(sorted(values), FRAME_SHARE) <= FRAME_TARGET_MS for values in (cpu_ms, gpu_ms))
    slow_ticks = sum(1 for value in tick_ms if value > TICK_TARGET_MS)
    print(f"Q4 frames: {'met' if frames_met else 'missed'}; ticks over {TICK_TARGET_MS:.0f} ms: {slow_ticks} of {len(tick_ms)}")
  return 0


if __name__ == "__main__":
  sys.exit(main())

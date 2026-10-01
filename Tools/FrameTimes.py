"""Summarizes a measurement log the game wrote with --measure (task 2.7 and task 3.7, design section 3).

The game writes OutpostCommander-measure.log to the temporary folder. Each line is one record:

  seed <seed> load <0|1> stress <0|1>         the run's switches
  display <width> <height> refresh_hz <hz>    the back buffer's size and the display's refresh rate, when they change
  frame_cpu_ns <ns>                           one frame's CPU work, from the swap chain's wait to Present's return
  frame_gpu_ns <ns>                           one frame's GPU work, between timestamps at its command list's ends
  tick_ns <ns>                                one server tick
  response_ns <ns> input_read_ns ... presented_ns ...   Q5: a move order to its first visible response

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
  response_ms = []
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
      elif fields[0] == "response_ns":
        response_ms.append(int(fields[1]) / 1e6)

  cpu_ms = cpu_ms[arguments.skip_frames:]
  gpu_ms = gpu_ms[arguments.skip_frames:]
  print(f"{arguments.log}: {header}")
  print(f"Back buffer and display: {', '.join(displays) if displays else 'not recorded'}")
  print(describe("Frame CPU work", cpu_ms, FRAME_TARGET_MS))
  print(describe("Frame GPU work", gpu_ms, FRAME_TARGET_MS))
  print(describe("Tick", tick_ms, TICK_TARGET_MS))
  if response_ms:
    print(describe("Order to response", response_ms, RESPONSE_TARGET_MS))

  # Q4: 99% of frames within 16.7 ms, on the CPU and on the GPU, and every tick within 5 ms.
  if cpu_ms and gpu_ms and tick_ms:
    frames_met = all(percentile(sorted(values), FRAME_SHARE) <= FRAME_TARGET_MS for values in (cpu_ms, gpu_ms))
    slow_ticks = sum(1 for value in tick_ms if value > TICK_TARGET_MS)
    print(f"Q4 frames: {'met' if frames_met else 'missed'}; ticks over {TICK_TARGET_MS:.0f} ms: {slow_ticks} of {len(tick_ms)}")
  return 0


if __name__ == "__main__":
  sys.exit(main())

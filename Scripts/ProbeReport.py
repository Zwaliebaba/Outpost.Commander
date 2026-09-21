#!/usr/bin/env python3
"""Loss and jitter, out of the M0.5 probe's client log.

A CALCULATOR, NOT A GATE (AGENTS.md section on Scripts/): it answers a question and has no
verdict. There is no threshold in here and no pass or fail, because nobody yet knows what an
acceptable figure is -- finding that out is what M0.5 exists for.

The client writes one line per arrival and computes nothing (OutpostCommander/App.cpp), which is
what keeps the arithmetic out of an executable no suite can reach (AGENTS.md R20). This is where
it lives instead.

  python Scripts/ProbeReport.py <probe-log.txt>

The log comes off the device at:

  %LOCALAPPDATA%\\Packages\\<PackageFamilyName>\\LocalState\\probe-log.txt

M0.5 SCAFFOLDING. It goes when ProbePacket goes.

Two things it is careful about, because both would make the link look worse than it is:

1. THE SEQUENCE IS SIXTEEN BITS and wraps every 65536 packets -- at 20 Hz, every 54 minutes and
   thirty seconds. It is unwrapped here rather than read as a catastrophic reordering.

2. JITTER IS NOT THE SPREAD OF ARRIVAL TIMES. The two clocks are never synchronized and the
   absolute one-way delay is therefore unknowable; what is knowable is how much the transit
   CHANGED between consecutive packets, because the constant offset between the clocks cancels in
   the difference. That is RFC 3550's D(i-1,i), and its smoothed mean is RFC 3550's J.
"""

import argparse
import re
import statistics
import sys

RX = re.compile(r"^RX seq=(\d+) host_ms=(\d+) local_ms=(\d+)\s*$")
SEQUENCE_MODULUS = 65536


def parse(path):
    """Every RX line, as (sequence, host_ms, local_ms), in the order the client logged them."""
    samples = []
    malformed = 0
    with open(path, "r", encoding="utf-8", errors="replace") as handle:
        for line in handle:
            line = line.strip()
            if not line.startswith("RX "):
                continue
            match = RX.match(line)
            if match is None:
                malformed += 1
                continue
            samples.append(tuple(int(group) for group in match.groups()))
    return samples, malformed


def unwrap(sequences):
    """Sixteen-bit sequences to a monotonic count, tolerating reordering at the wrap."""
    unwrapped = []
    epoch = 0
    previous = None
    for sequence in sequences:
        if previous is not None and sequence < previous and (previous - sequence) > SEQUENCE_MODULUS // 2:
            epoch += SEQUENCE_MODULUS
        previous = sequence
        unwrapped.append(epoch + sequence)
    return unwrapped


def report(path):
    samples, malformed = parse(path)
    if len(samples) < 2:
        print(f"{path}: {len(samples)} RX line(s) -- not enough to report anything.")
        return 1

    absolute = unwrap([sample[0] for sample in samples])
    first, last = min(absolute), max(absolute)
    expected = (last - first) + 1
    received = len(set(absolute))
    duplicates = len(absolute) - received
    lost = expected - received

    # Out of order means a packet arrived after one the host sent later. UDP is allowed to, and it
    # is worth separating from loss: a reorder is a packet that came, and loss is one that did not.
    reordered = sum(1 for a, b in zip(absolute, absolute[1:]) if b < a)

    print(f"log                  {path}")
    print(f"packets sent         {expected}   (sequence {first} through {last})")
    print(f"packets received     {received}")
    print(f"packets lost         {lost}   ({(lost / expected) * 100.0:.2f}%)")
    print(f"duplicated           {duplicates}")
    print(f"out of order         {reordered}")
    if malformed:
        print(f"unreadable RX lines  {malformed}")

    # RFC 3550: D(i-1,i) = (Rj - Ri) - (Sj - Si), over consecutive packets AS SENT, so the pairs
    # are taken in sequence order rather than arrival order.
    in_order = sorted(samples, key=lambda sample: sample[0])
    transits = []
    jitter = 0.0
    for (_, host_a, local_a), (_, host_b, local_b) in zip(in_order, in_order[1:]):
        difference = (local_b - local_a) - (host_b - host_a)
        transits.append(abs(difference))
        jitter += (abs(difference) - jitter) / 16.0

    if transits:
        print()
        print(f"jitter (RFC 3550 J)  {jitter:.2f} ms")
        print(f"  |D| mean           {statistics.fmean(transits):.2f} ms")
        print(f"  |D| median         {statistics.median(transits):.2f} ms")
        print(f"  |D| worst          {max(transits):.2f} ms")
        # A percentile rather than a comparison against the tick: this file states no design
        # figure, so there is no fourth copy of one here to go stale (AGENTS.md section 6).
        ordered = sorted(transits)
        print(f"  |D| 95th pct       {ordered[min(len(ordered) - 1, int(len(ordered) * 0.95))]:.2f} ms")

    # The run's own length, so that "a run of some minutes" can be confirmed rather than assumed.
    duration_ms = in_order[-1][2] - in_order[0][2]
    print()
    print(f"run length           {duration_ms / 1000.0:.1f} s")
    return 0


def main():
    parser = argparse.ArgumentParser(description="Loss and jitter from an M0.5 probe log.")
    parser.add_argument("log", help="the client's probe-log.txt")
    arguments = parser.parse_args()
    return report(arguments.log)


if __name__ == "__main__":
    sys.exit(main())

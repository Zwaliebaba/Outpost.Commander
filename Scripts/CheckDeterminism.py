#!/usr/bin/env python3
"""Sweep the simulation for the constructs R16 forbids.

R16 is a rule the build cannot check and review reads past. A float in GameCore, an
unordered container whose iteration order reaches an outcome, a QueryPerformanceCounter
inside the tick -- each compiles, passes every test, and works perfectly on one machine.
It surfaces as an x64-against-ARM64 desync, or a replay that will not reproduce from its
seed, which is the class of defect that cannot be debugged from a report of what happened.

    python3 Scripts/CheckDeterminism.py
    python3 Scripts/CheckDeterminism.py --also NeuronCore
    python3 Scripts/CheckDeterminism.py --review   # include the judgement calls

Comments and string literals are stripped before matching, so the output is signal. What it
cannot see is in SKILL.md and is not optional -- a clean sweep is half the audit.
"""
import argparse
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
SIMULATION = ["GameCore", "GameLogic"]

# A violation: nothing in the simulation may hold one of these, and no argument makes it fine.
VIOLATIONS = [
    (r"\b(float|double)\b", "R16: the simulation holds no floats",
     "hold the fraction as an integer and put the scale in the name (R6)"),
    (r"\blong\s+double\b", "R16: the simulation holds no floats", "as above"),
    (r"\b\d+\.\d*([eE][-+]?\d+)?[fF]?\b", "R16: a floating-point literal",
     "an integer count of the unit the name states"),
    (r"\bstd::unordered_(map|set|multimap|multiset)\b",
     "R16: iteration order is unspecified and differs between builds",
     "std::map or a vector kept in entity-identity order"),
    (r"\b(std::chrono|GetTickCount64|GetTickCount|QueryPerformanceCounter|GetSystemTime\w*|"
     r"timeGetTime)\b", "R16: no wall-clock time -- the tick is the clock",
     "take the tick; wall time maps to ticks at the seam and only there"),
    (r"\bstd::random_device\b", "R16: randomness is a pinned PRNG seeded from the match",
     "the match's seeded PRNG"),
    (r"\b(srand|rand)\s*\(", "R16: rand() is implementation-defined and globally stateful",
     "the match's seeded PRNG"),
    (r"\bstd::default_random_engine\b",
     "R16: default_random_engine is an implementation-defined typedef",
     "name the engine explicitly, seeded from the match"),
    (r"\bstd::(sqrt|sin|cos|tan|atan2|atan|asin|acos|pow|exp|log|log2|hypot|fma)\b",
     "R16: CRT transcendentals are not correctly-rounded and are not specified bit-identical",
     "an integer routine, or the Q1.15 table ADR-002 pins"),
    (r"\b(sqrtf|sinf|cosf|tanf|atan2f|powf|expf|logf|fmaf|fabsf)\b",
     "R16: as above, and the f-suffixed CRT forms are the same problem",
     "an integer routine, or the Q1.15 table ADR-002 pins"),
    (r"\bDirectX::XM\w+", "R16: DirectXMath is float SIMD; it belongs in the renderer",
     "integer arithmetic in the simulation, DirectXMath on the client side of the seam"),
]

# A judgement call: legal, but each is a way a determinism defect has historically arrived.
REVIEW = [
    (r"\bstd::(sort|stable_sort|nth_element|partial_sort|make_heap|push_heap|pop_heap)\b",
     "does the comparator impose a TOTAL order?",
     "std::sort is not stable, so two elements comparing equal may come out either way -- "
     "tie-break on entity identity"),
    (r"\bstd::(shuffle|sample)\b", "seeded from the match's PRNG, and only that?",
     "the engine must be the match's, never a local one"),
    (r"\bstd::uniform_(int|real)_distribution\b|\bstd::(normal|bernoulli|poisson)_distribution\b",
     "a distribution is NOT specified bit-identical between standard libraries",
     "derive the value from the raw engine output with your own arithmetic"),
    (r"\bstd::(map|set)\s*<\s*[\w:]*\s*\*", "a container keyed by POINTER orders by address",
     "key on entity identity, which is stable across runs and machines"),
    (r"reinterpret_cast\s*<\s*(std::)?(u?int(ptr_t|\d+_t)|size_t)", "an address used as a number",
     "an address differs every run; derive from entity identity"),
    (r"\bstatic\b(?![^;{]*\bconst(expr)?\b)", "mutable static state in the simulation",
     "sm_ requires documented thread-safety (R3); prefer none at all in the tick"),
]

LINE_COMMENT = re.compile(r"//[^\n]*")
BLOCK_COMMENT = re.compile(r"/\*.*?\*/", re.S)
RAW_STRING = re.compile(r'R"([^(]*)\(.*?\)\1"', re.S)
STRING = re.compile(r'"(\\.|[^"\\])*"')
CHAR = re.compile(r"'(\\.|[^'\\])*'")
INCLUDE = re.compile(r"^\s*#\s*include[^\n]*", re.M)


def blank(text):
    """Replace comments and literals with spaces, keeping every newline so lines still line up."""
    def spaces(match):
        return re.sub(r"[^\n]", " ", match.group(0))
    for pattern in (RAW_STRING, BLOCK_COMMENT, LINE_COMMENT, STRING, CHAR, INCLUDE):
        text = pattern.sub(spaces, text)
    return text


def sweep(path, rules):
    source = blank(path.read_text(encoding="utf-8", errors="replace"))
    lines = source.splitlines()
    for number, line in enumerate(lines, 1):
        for pattern, why, instead in rules:
            match = re.search(pattern, line)
            if match:
                yield number, match.group(0).strip(), why, instead, line.strip()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=pathlib.Path, default=ROOT)
    parser.add_argument("--also", action="append", default=[],
                        help="another directory to treat as simulation, e.g. NeuronCore")
    parser.add_argument("--review", action="store_true",
                        help="also report the judgement calls, which are legal but load-bearing")
    a = parser.parse_args()

    directories = [a.root / d for d in SIMULATION + a.also]
    missing = [d for d in directories if not d.is_dir()]
    if missing:
        print(f"  Not found: {', '.join(str(d) for d in missing)}")
        return 1

    files = sorted(f for d in directories for f in list(d.rglob("*.cpp")) + list(d.rglob("*.h")))
    violations = review = 0
    for path in files:
        relative = path.relative_to(a.root)
        for number, found, why, instead, line in sweep(path, VIOLATIONS):
            print(f"  {relative}:{number}: {found!r} -- {why}")
            print(f"      {line[:96]}")
            print(f"      instead: {instead}")
            violations += 1
        if a.review:
            for number, found, why, instead, line in sweep(path, REVIEW):
                print(f"  [review] {relative}:{number}: {found!r} -- {why}")
                print(f"      {instead}")
                review += 1

    print(f"\n{violations} violation(s)"
          + (f", {review} judgement call(s)" if a.review else "")
          + f" across {len(files)} file(s) in {', '.join(d.name for d in directories)}.")
    if not a.review:
        print("  Run with --review for the judgement calls: sort comparators, distributions,")
        print("  pointer-keyed containers and mutable statics are all legal and all load-bearing.")
    print("  A clean sweep is HALF the audit. SKILL.md lists what no sweep can see.")
    return 1 if violations else 0


if __name__ == "__main__":
    sys.exit(main())

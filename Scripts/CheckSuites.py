#!/usr/bin/env python3
"""Assert every suite actually ran a test, by reading the .trx vstest wrote.

vstest reports "no tests found" as a PASS. An empty suite is therefore worse than no suite:
it is a green check mark over a library nobody exercised, and CI's existing step only proves
the DLL was BUILT (AGENTS.md section 3). This closes the gap between built and exercised.

    python3 Scripts/CheckSuites.py TestResults          # a directory, or a .trx directly

Exit 0 when every *Tests.vcxproj in the tree ran at least one test, 1 otherwise.
"""
import argparse
import collections
import pathlib
import sys
import xml.etree.ElementTree as ET

NS = "{http://microsoft.com/schemas/VisualStudio/TeamTest/2010}"
ROOT = pathlib.Path(__file__).resolve().parent.parent


def counts(trx):
    """Executed tests per suite DLL stem, and how many of them did not pass."""
    root = ET.parse(trx).getroot()
    storage = {}
    for unit in root.iter(NS + "UnitTest"):
        path = (unit.get("storage") or "").replace("\\", "/")
        storage[unit.get("id")] = pathlib.PurePosixPath(path).stem
    ran, failed = collections.Counter(), collections.Counter()
    for result in root.iter(NS + "UnitTestResult"):
        suite = storage.get(result.get("testId"))
        if suite is None:
            continue
        ran[suite] += 1
        if (result.get("outcome") or "") != "Passed":
            failed[suite] += 1
    return ran, failed


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("results", type=pathlib.Path, help="a .trx, or a directory holding them")
    parser.add_argument("--root", type=pathlib.Path, default=ROOT, help="tree to take suites from")
    a = parser.parse_args()

    files = sorted(a.results.rglob("*.trx")) if a.results.is_dir() else [a.results]
    if not files:
        print(f"  No .trx under {a.results}. vstest writes one with /Logger:trx.")
        return 1

    ran, failed = collections.Counter(), collections.Counter()
    for trx in files:
        r, f = counts(trx)
        ran.update(r)
        failed.update(f)

    suites = sorted(p.stem for p in a.root.rglob("*Tests.vcxproj"))
    if not suites:
        print("  No *Tests.vcxproj in the tree. Every library has a suite (AGENTS.md section 2).")
        return 1

    faults = 0
    for suite in suites:
        if not ran[suite]:
            print(f"  {suite}: ran NO tests. vstest scores that as a pass; it is a green check "
                  f"mark over a library nobody exercised.")
            faults += 1
        else:
            note = f", {failed[suite]} not passed" if failed[suite] else ""
            print(f"  {suite}: {ran[suite]} test(s){note}")

    unknown = sorted(set(ran) - set(suites))
    for suite in unknown:
        print(f"  {suite}: ran {ran[suite]} test(s) but has no *Tests.vcxproj -- a stale DLL "
              f"from an earlier build is being run")
        faults += 1

    print(f"\n{faults} fault(s) across {len(suites)} suites, from {len(files)} .trx file(s).")
    return 1 if faults else 0


if __name__ == "__main__":
    sys.exit(main())

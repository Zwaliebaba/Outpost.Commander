#!/usr/bin/env python3
"""Check Design/ for figures that disagree, citations that dangle and links that miss.

A figure in this tree is usually stated three times -- in TechnicalDesign, in the ADR that
ruled it, and in the OpenQuestions row that asked. When one moves and the others do not, both
copies read as authoritative and there is nothing to tell them apart. That has happened here
repeatedly, which is why this exists rather than a reminder to be careful.

    python3 Scripts/CheckDesign.py
    python3 Scripts/CheckDesign.py --root .

Prose is normalised before matching: a figure wraps across a line break, and a naive grep for
'96 bytes' misses '96\\nbytes'. An earlier sweep let exactly that through.

THE DATAGRAM FIGURES ARE NOT LISTED HERE. They are imported from the datagram-budget skill's
budget.py and recomputed, so this checker cannot itself go stale against them. Everything else
is a manifest below, and when one of those moves it moves here too -- that is the cost of a
figure being canonical, and it is smaller than the cost of two of them.
"""
import argparse
import importlib.util
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]

# A figure that has moved. `gone` must not appear; `here` must appear in each named file.
#
# EVERY `gone` PATTERN MUST BE CLAIM-SHAPED, NOT VALUE-SHAPED, and this is the whole difficulty.
# This tree deliberately records why a figure changed -- "tap-to-visible is 152 ms where 10 Hz made
# it 252 ms" is correct prose that a bare /10 Hz/ would flag. So match the sentence that ASSERTS the
# old value ("snapshots go out at 10 Hz"), never the number on its own. A checker that cries wolf on
# good prose gets switched off, and then it catches nothing at all.
#
# A figure earns a row by having actually drifted. Version pins are NOT checked here -- prose about a
# version is usually a cautionary tale; Scripts/CheckProjectFiles.py reads the real project files.
MANIFEST = [
    ("snapshot rate", [r"[Ss]napshots go out at 10 Hz"], r"[Ss]napshots go out at\*{0,2} 20 Hz",
     ["Design/TechnicalDesign.md"]),
    ("interpolation delay", [r"client renders \*{0,2}150 milliseconds\*{0,2} behind"],
     r"client renders \*{0,2}75 milliseconds\*{0,2} behind", ["Design/TechnicalDesign.md"]),
    ("MVP entity count", [r"\b102 entities in the reduced MVP"], r"\b110 entities in th\w* reduced MVP",
     ["Design/OpenQuestions.md"]),
    # ADR-016 moved this across ten documents at once, which is exactly the drift this exists for.
    ("world resolution", [r"[Tt]he world is authored at 1440 . 960",
                          r"scene target at the authored 1440 . 960"],
     r"default(ing to|s to| is) 1:1", ["Design/TechnicalDesign.md", "Design/Interface.md"]),
    # ADR-017 moved the verb across five documents; the posture correction moved one premise.
    ("group selection's verb", [r"selection by tap and by hold", r"[Aa] hold on one of your ships"],
     r"double tap", ["Design/Interface.md", "Design/GameDesign.md", "Design/Plan/M1-the-fleet.md"]),
    ("the posture", [r"held at its sides and the thumbs reach"],
     r"kickstand on a desk", ["Design/Interface.md"]),
    ("the interface's own transform", [r"carried through \*{0,2}the same fit transform"],
     r"interface fit", ["Design/Interface.md", "Design/TechnicalDesign.md"]),
]

# Prose that names an ADR which must exist, and a question which must be on the register.
ADR_REFERENCE = re.compile(r"\bADR-(\d{3})\b")
QUESTION_REFERENCE = re.compile(r"\*\*Q(\d+)\*\*|\bQ(\d+)\b")

faults = []


FENCE = re.compile(r"```.*?```", re.S)
CODE_SPAN = re.compile(r"`[^`\n]+`")


def flat(text, prose_only=False):
    """One line, for figures that wrap. prose_only drops code, which QUOTES rather than claims:
    a skill document naming a pattern, or a shell block showing a flag, is not the design speaking."""
    if prose_only:
        text = CODE_SPAN.sub(" ", FENCE.sub(" ", text))
    return re.sub(r"\s+", " ", text)


def documents(root):
    return sorted(set(list(root.glob("Design/**/*.md")) + list(root.glob(".claude/**/*.md"))
                      + [root / "AGENTS.md"]))


def check_figures(root, files):
    for name, gone, here, expect in MANIFEST:
        for path in files:
            body = flat(path.read_text(), prose_only=True)
            for pattern in gone:
                for match in re.finditer(pattern, body):
                    faults.append(f"{path.relative_to(root)}: {name} still reads "
                                  f"{match.group(0)!r}; it is now {here}")
        for target in expect:
            path = root / target
            if not path.exists():
                faults.append(f"{target}: expected to state {name} but the file is missing")
            elif not re.search(here, flat(path.read_text(), prose_only=True)):
                faults.append(f"{target}: never states {name} ({here}), which it is expected to")


def check_datagram(root, files):
    """Recompute the budget rather than restating it, so this cannot drift against the design."""
    script = root / "Scripts/DatagramBudget.py"
    if not script.exists():
        faults.append("Scripts/DatagramBudget.py not found; the wire figures were not checked")
        return
    sys.dont_write_bytecode = True          # no __pycache__ beside a checked-in script
    spec = importlib.util.spec_from_file_location("budget", script)
    budget = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(budget)

    entities, record, header, total = budget.budget(2, 50, 4, 3, 0, 0)
    pinned = budget.PINNED
    headroom = pinned - total
    derived = [(f"the snapshot ({total} B)", rf"\b{total:,}\b".replace(",", ",?")),
               (f"the pin ({pinned} B)", rf"\b{pinned:,}\b".replace(",", ",?")),
               (f"the headroom ({headroom} B)", rf"\b{headroom}\b")]
    for name, pattern in derived:
        where = [p for p in files if re.search(pattern, flat(p.read_text()))]
        if not where:
            faults.append(f"Design/: budget.py computes {name} and no document states it")

    # Any four-digit byte figure near the word "payload" that is not the pin is a stale pin.
    for path in files:
        body = flat(path.read_text())
        for match in re.finditer(r"(\d,?\d{3})-byte (payload|command packet|figure)", body):
            value = int(match.group(1).replace(",", ""))
            if value != pinned and "ADR-003" not in path.name:
                faults.append(f"{path.relative_to(root)}: {match.group(0)!r} -- the pin is "
                              f"{pinned}; only ADR-003 may discuss the fallback")


def check_citations(root, files):
    adrs = {p.name[4:7]: p for p in (root / "Design/ADR").glob("ADR-*.md")}
    readme = root / "Design/ADR/README.md"
    # An ADR number may be RESERVED before its file exists -- the plan forward-references decisions
    # it has not taken yet. The README is where that is declared, so read it rather than guess.
    reserved = set()
    if readme.exists():
        for sentence in re.findall(r"[^.]*\breserved\b[^.]*\.", readme.read_text()):
            reserved |= set(ADR_REFERENCE.findall(sentence))

    register = (root / "Design/OpenQuestions.md")
    questions = set()
    if register.exists():
        body = register.read_text()
        # A question is a table row OR a heading; Q26 is a heading and an earlier sweep cried wolf.
        questions = set(re.findall(r"\|\s*\*\*Q(\d+)\*\*", body)) \
            | set(re.findall(r"^#+\s*Q(\d+)\b", body, re.M))
    for path in files:
        body = path.read_text()
        for number in set(ADR_REFERENCE.findall(body)):
            if number not in adrs and number not in reserved:
                faults.append(f"{path.relative_to(root)}: cites ADR-{number}, which neither exists "
                              f"nor is reserved in Design/ADR/README.md")
        if questions and path != register:
            for a, b in set(QUESTION_REFERENCE.findall(body)):
                number = a or b
                if number and number not in questions:
                    faults.append(f"{path.relative_to(root)}: cites Q{number}, which is not on "
                                  f"the register")
    # Every ADR is reachable from its README, or nobody finds it.
    readme = root / "Design/ADR/README.md"
    if readme.exists():
        listed = set(ADR_REFERENCE.findall(readme.read_text()))
        for number, path in sorted(adrs.items()):  # noqa: every ADR must be reachable from here
            if number not in listed:
                faults.append(f"Design/ADR/README.md: does not list {path.name}")


def check_shape(root, files):
    for path in files:
        raw = path.read_text()
        for number, line in enumerate(raw.splitlines(), 1):
            stripped = line.strip()
            # An escaped pipe is content, not a column separator -- `Debug\|x64` is not a table.
            if stripped.startswith("|") and stripped.endswith("|") \
                    and re.sub(r"\\\|", "", stripped).count("|") < 3:
                faults.append(f"{path.relative_to(root)}:{number}: a table row with too few "
                              f"columns: {stripped[:60]}")
        for match in re.finditer(r"\]\((?!http)([^)#]+)", raw):
            if not (path.parent / match.group(1)).resolve().exists():
                faults.append(f"{path.relative_to(root)}: link to {match.group(1)} does not resolve")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=pathlib.Path, default=ROOT)
    root = parser.parse_args().root.resolve()
    files = [p for p in documents(root) if p.exists()]
    if not files:
        print(f"  No documents under {root}.")
        return 1

    check_figures(root, files)
    check_datagram(root, files)
    check_citations(root, files)
    check_shape(root, files)

    for line in faults:
        print(f"  {line}")
    print(f"\n{len(faults)} issue(s) across {len(files)} documents.")
    return 1 if faults else 0


if __name__ == "__main__":
    sys.exit(main())

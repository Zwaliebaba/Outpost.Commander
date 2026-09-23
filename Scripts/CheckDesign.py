#!/usr/bin/env python3
"""Check Design/ for figures that disagree, citations that dangle and links that miss.

A figure in this tree is usually stated three times -- in TechnicalDesign, in the ADR that
ruled it, and in the OpenQuestions row that asked. When one moves and the others do not, both
copies read as authoritative and there is nothing to tell them apart. That has happened here
repeatedly, which is why this exists rather than a reminder to be careful.

    python3 Scripts/CheckDesign.py
    python3 Scripts/CheckDesign.py --root .

Prose is normalized before matching: a figure wraps across a line break, and a naive grep for
'96 bytes' misses '96\\nbytes'. An earlier sweep let exactly that through.

THE DATAGRAM FIGURES ARE NOT LISTED HERE. They are imported from the datagram-budget skill's
budget.py and recomputed, so this checker cannot itself go stale against them. Everything else
is a manifest below, and when one of those moves it moves here too -- that is the cost of a
figure being canonical, and it is smaller than the cost of two of them.

THE MILESTONE STEP COUNTS ARE NOT LISTED EITHER, for the same reason: check_plan_counts below
recomputes them from the step headings and compares both places that state them.
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
    ("update rate", [r"[Ss]napshots go out at 10 Hz"], r"(?:[Ss]napshots|[Uu]pdates) go out at\*{0,2} 20 Hz",
     ["Design/TechnicalDesign.md"]),
    # ADR-024 replaced the full snapshot; a document still asserting it as the design is stale.
    ("the replication unit", [r"[Rr]eplication is full self-contained snapshots\*\*\s*\(",
                              r"^\*\*Every snapshot is self-contained\.\*\*"],
     r"priority accumulator", ["Design/TechnicalDesign.md", "Design/README.md"]),
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
    # ADR-018: the camera's control model was absent rather than wrong, so this row is a
    # presence check with nothing to retire -- `gone` may be empty and often should be.
    ("the camera's control model", [], r"anchor solve",
     ["Design/Interface.md", "Design/Plan/M0-the-wire.md"]),
    ("the spent Holding", [r"[Tt]wo verbs are banked rather than one",
                           r"\`Holding\` means nothing anywhere"],
     r"recenters", ["Design/Interface.md"]),
    # ADR-019 says it out loud: "a backdrop that nobody pinned gets brighter one commit at a time."
    ("the sky's luminance ceiling", [], r"12% of full white",
     ["Design/ADR/ADR-005-a-mesh-is-a-cmo-file.md",
      "Design/ADR/ADR-019-the-sky-is-generated-from-the-seed.md",
      "Design/Plan/M1-the-fleet.md"]),
    # The magnitude tiers were arithmetic nobody had run: 1:3:9:27:81:243 sums to 364, which divides no
    # round number of stars evenly, and two documents plus the suite and two comments all stated an
    # "exact" division that was neither exact nor what the code computed. A six-number list is
    # value-shaped rather than claim-shaped, which this file warns against -- it is safe here only
    # because the whole list appears nowhere except as an assertion of these counts.
    #
    # The list moved again when the shipped count went from 3,000 to 8,000 (ADR-019's second look), and
    # the 3,000-star list is retired by the same reasoning: nothing writes it out except to assert it.
    ("the star field's magnitude tiers", [r"8, 25, 74, 222, 667 and 2,004", r"8, 25, 74, 223, 668 and 2,002"],
     r"22, 66, 198, 593, 1,780 and 5,341",
     ["Design/ADR/ADR-019-the-sky-is-generated-from-the-seed.md", "Design/Plan/M1-the-fleet.md"]),
    # Both figures moved because the sky was looked at and did not read as one: 1.5-pixel sprites did
    # not rasterize, and 20% saturation over a narrow temperature range left every star the same
    # off-white. Claim-shaped rather than bare numbers -- "1.5" alone appears in unrelated prose, and
    # the history of both figures is legitimately written down in ADR-019.
    # It moved again, 2.4 to 3.0, when a screenshot showed 2.4 delivering 16 of 255 to a pixel.
    ("the star sprite's size range", [r"down to 1\.5 for the faintest", r"8 scene-target pixels down to 1\.5",
                                      r"pixels down to 2\.4", r"down to 2\.4 at the faint"],
     r"down to 3\.0", ["Design/ADR/ADR-019-the-sky-is-generated-from-the-seed.md", "Design/Plan/M1-the-fleet.md"]),
    # ADR-019 withdrew the galaxy band after the second look: it read as a painting. The `gone` patterns
    # are the sentences that ASSERT a band is drawn; the band's history stays in ADR-019 as prose.
    ("the sky is stars only", [r"[Tt]he galaxy bakes once into", r"[Tt]he galaxy band is a 512",
                               r"[Tt]he sky is two more draws and a bake"],
     r"stars and nothing else",
     ["Design/ADR/ADR-019-the-sky-is-generated-from-the-seed.md", "Design/TechnicalDesign.md",
      "Design/Plan/M1-the-fleet.md"]),
    ("the star field's saturation", [r"desaturate to roughly 20%", r"desaturated to about 20%"],
     r"(desaturate to about|desaturated to about) 38%",
     ["Design/ADR/ADR-019-the-sky-is-generated-from-the-seed.md", "Design/Plan/M1-the-fleet.md"]),
    # ADR-005 was REPLACED rather than amended -- a mesh was a function and is now a CMO file -- which
    # moved one rule stated in seven documents at once. The `gone` patterns are the sentences that ASSERT
    # the old rule; the old TITLE is quoted as history by ADR-005's own status line and by ADR-021, and a
    # pattern matching that would fire on correct prose, which is how a checker gets switched off.
    ("what a mesh is", [r"[Mm]eshes are functions rather than files",
                        r"A hull is a function that emits a few dozen triangles",
                        r"[Mm]eshes are generated in code\.", r"[Mm]eshes are still generated in code"],
     r"\bCMO\b", ["Design/ADR/ADR-005-a-mesh-is-a-cmo-file.md", "Design/TechnicalDesign.md",
                   "Design/Plan/M1-the-fleet.md", "Design/Plan/M2-the-field.md"]),
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
            body = flat(path.read_text(encoding="utf-8"), prose_only=True)
            for pattern in gone:
                for match in re.finditer(pattern, body):
                    faults.append(f"{path.relative_to(root)}: {name} still reads "
                                  f"{match.group(0)!r}; it is now {here}")
        for target in expect:
            path = root / target
            if not path.exists():
                faults.append(f"{target}: expected to state {name} but the file is missing")
            elif not re.search(here, flat(path.read_text(encoding="utf-8"), prose_only=True)):
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

    b = budget.budget(2, 50, 4, 3, 2, 0, 0)
    pinned = budget.PINNED
    per_client = pinned * 20 / 1000
    # ADR-024: the update's figures. The record and header widths are stated in prose as words or
    # digits; the per-datagram count and the per-client rate are digits wherever they appear.
    derived = [(f"the record ({b['record']} B)", r"\btwelve bytes\b|\b12 B\b|\brecord 12\b"),
               (f"the header ({b['header']} B)", r"\b[Tt]wenty-one bytes\b|\b21 B\b|\bheader 21\b"),
               (f"records per datagram ({b['records per datagram']})",
                rf"\b{b['records per datagram']} records\b"),
               (f"the pin ({pinned} B)", rf"\b{pinned:,}\b".replace(",", ",?")),
               (f"the per-client rate ({per_client:.1f} KB/s)", rf"\b{per_client:.1f} KB/s")]
    for name, pattern in derived:
        where = [p for p in files if re.search(pattern, flat(p.read_text(encoding="utf-8")))]
        if not where:
            faults.append(f"Design/: DatagramBudget.py computes {name} and no document states it")

    # Any four-digit byte figure near the word "payload" that is not the pin is a stale pin.
    for path in files:
        body = flat(path.read_text(encoding="utf-8"))
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
        for sentence in re.findall(r"[^.]*\breserved\b[^.]*\.", readme.read_text(encoding="utf-8")):
            reserved |= set(ADR_REFERENCE.findall(sentence))

    register = (root / "Design/OpenQuestions.md")
    questions = set()
    if register.exists():
        body = register.read_text(encoding="utf-8")
        # A question is a table row OR a heading; Q26 is a heading and an earlier sweep cried wolf.
        questions = set(re.findall(r"\|\s*\*\*Q(\d+)\*\*", body)) \
            | set(re.findall(r"^#+\s*Q(\d+)\b", body, re.M))
    for path in files:
        body = path.read_text(encoding="utf-8")
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
        listed = set(ADR_REFERENCE.findall(readme.read_text(encoding="utf-8")))
        for number, path in sorted(adrs.items()):  # noqa: every ADR must be reachable from here
            if number not in listed:
                faults.append(f"Design/ADR/README.md: does not list {path.name}")


def check_shape(root, files):
    for path in files:
        raw = path.read_text(encoding="utf-8")
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


# Number words a plan's prose actually uses. Spelled out rather than digits, which is the house
# style in Design/ -- "Twenty-three steps and three gates", not "23 steps and 3 gates".
WORDS = {w: n for n, w in enumerate(
    "zero one two three four five six seven eight nine ten eleven twelve thirteen fourteen fifteen "
    "sixteen seventeen eighteen nineteen twenty".split())}
WORDS.update({f"twenty-{w}": 20 + n for w, n in list(WORDS.items())[1:10]})
WORDS.update({f"thirty-{w}": 30 + n for w, n in list(WORDS.items())[1:10]})
WORDS["thirty"] = 30
WORDS["forty"] = 40


def check_plan_counts(root):
    """Recompute each milestone's step and gate counts from its own headings.

    A milestone states its size in two places -- its own opening line and the table in
    Design/Plan/README.md -- and both are prose beside a list that is the actual answer. M2 said
    "eleven steps" against fifteen while the README said fifteen, and neither the manifest above nor
    anything else noticed, because a step count is not a figure anyone thought to pin.

    So it is computed here rather than listed, for the same reason the datagram figures are: a
    checker that restates the number it is policing can go stale against it.

    THE COUNTING RULE, because it is not the obvious one. A milestone's steps are its DISTINCT step
    NUMBERS, with gates among them rather than beside them -- M0's twenty-three steps include its
    three gates -- and a b-suffixed step folds into its parent, so M1.9 and M1.9b are one step. Both
    fall out of the README's own table, which every row already satisfies.
    """
    readme = root / "Design/Plan/README.md"
    table = flat(readme.read_text(encoding="utf-8")) if readme.exists() else ""

    for path in sorted((root / "Design/Plan").glob("M?-*.md")):
        milestone = path.name.split("-")[0]
        body = path.read_text(encoding="utf-8")
        steps = len(set(re.findall(rf"^### ({milestone}\.\d+)", body, re.M)))
        gates = len(re.findall(rf"^### {milestone}\.\d+b? — GATE", body, re.M))
        if not steps:
            continue

        claim = re.search(r"([A-Za-z]+(?:-[a-z]+)?) steps(?:,| and) ([a-z]+(?:-[a-z]+)?) gates",
                          flat(body))
        if not claim:
            faults.append(f"{path.relative_to(root)}: no line states how many steps and gates "
                          f"{milestone} has; it has {steps} and {gates}")
        else:
            said_steps = WORDS.get(claim.group(1).lower())
            said_gates = WORDS.get(claim.group(2).lower())
            if said_steps != steps or said_gates != gates:
                faults.append(f"{path.relative_to(root)}: says {claim.group(0)!r}, but "
                              f"{milestone} has {steps} steps and {gates} gates")

        row = re.search(rf"\| \[`{milestone}`\]\([^)]+\)[^|]*\|[^|]*\|[^|]*\| (\d+) \| (\d+) \|",
                        table)
        if not row:
            faults.append(f"Design/Plan/README.md: no milestone row for {milestone}")
        elif (int(row.group(1)), int(row.group(2))) != (steps, gates):
            faults.append(f"Design/Plan/README.md: {milestone} is listed as {row.group(1)} steps "
                          f"and {row.group(2)} gates, but it has {steps} and {gates}")


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
    check_plan_counts(root)
    check_citations(root, files)
    check_shape(root, files)

    for line in faults:
        print(f"  {line}")
    print(f"\n{len(faults)} issue(s) across {len(files)} documents.")
    return 1 if faults else 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""Assert US spelling across the tree, in identifiers and in prose (AGENTS.md R11).

R11 picks US spelling because the Windows SDK does -- `D3D12_CLEAR_VALUE::Color` settles it --
and the defect it prevents is a tree where a reader has to know which half they are in and a
grep for one finds half the uses. Until 2026-09-21 the rule covered identifiers only and
nothing enforced either half. Exempting prose failed here for a reason specific to this tree:
the design documents quote identifiers constantly, so both spellings ended up inside single
sentences -- Interface.md said "the recogniser emits `Tapped`" about `GestureRecognizer`.

    python3 Scripts/CheckSpelling.py
    python3 Scripts/CheckSpelling.py --fix      # rewrite in place, then read the diff

Exit 0 when the tree is US throughout, 1 otherwise, naming file, line and replacement.
"""
import argparse
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]

# EXPLICIT PAIRS ONLY. A blanket /ise$/ rule is wrong and would fire on correct US English:
# surprise, precise, concise, promise, exercise, advertise, compromise, franchise, supervise,
# revise, devise, arise, otherwise, likewise, wise. Add inflections when one bites; a missing
# inflection is how "optimisation" survived the first pass of this list.
WORDS = {}
for stem, us in [("optimis", "optimiz"), ("standardis", "standardiz"), ("normalis", "normaliz"),
                 ("recognis", "recogniz"), ("organis", "organiz"), ("minimis", "minimiz"),
                 ("maximis", "maximiz"), ("synchronis", "synchroniz"), ("initialis", "initializ"),
                 ("serialis", "serializ"), ("realis", "realiz"), ("summaris", "summariz"),
                 ("prioritis", "prioritiz"), ("utilis", "utiliz"), ("emphasis", "emphasiz"),
                 ("quantis", "quantiz"), ("visualis", "visualiz"), ("penalis", "penaliz"),
                 ("analys", "analyz")]:
    for suffix in ("e", "es", "ed", "ing", "ation", "ations", "er", "ers"):
        WORDS[stem + suffix] = us + suffix
WORDS.update({
    "colour": "color", "colours": "colors", "coloured": "colored", "colouring": "coloring",
    "behaviour": "behavior", "behaviours": "behaviors",
    "favour": "favor", "favours": "favors", "favoured": "favored", "favourite": "favorite",
    "neighbour": "neighbor", "neighbours": "neighbors", "neighbouring": "neighboring",
    "centre": "center", "centres": "centers", "centred": "centered", "centring": "centering",
    "recentre": "recenter", "recentres": "recenters", "recentred": "recentered",
    "grey": "gray", "greyed": "grayed", "greys": "grays",
    "cancelled": "canceled", "cancelling": "canceling",
    "modelling": "modeling", "modelled": "modeled", "travelling": "traveling",
    "travelled": "traveled", "labelled": "labeled", "labelling": "labeling",
    "signalled": "signaled", "signalling": "signaling",
    "defence": "defense", "defences": "defenses", "offence": "offense", "offences": "offenses",
    "licence": "license", "licences": "licenses",
    "catalogue": "catalog", "catalogues": "catalogs", "dialogue": "dialog", "programme": "program",
    "metre": "meter", "metres": "meters", "fibre": "fiber", "litre": "liter",
    "judgement": "judgment", "judgements": "judgments",
    "acknowledgement": "acknowledgment", "acknowledgements": "acknowledgments",
    "practise": "practice", "practised": "practiced", "sceptical": "skeptical",
    "manoeuvre": "maneuver", "manoeuvres": "maneuvers",
    "artefact": "artifact", "artefacts": "artifacts",
    "whilst": "while", "amongst": "among", "learnt": "learned", "spelt": "spelled",
    "burnt": "burned", "aluminium": "aluminum",
})

# Fragments inside CamelCase identifiers, where \b never falls: PointDefence, SceneColour.
# `PointDefence` was a real catalog identity in this tree and R11 had banned it from the start.
FRAGMENTS = {"Defence": "Defense", "Colour": "Color", "Behaviour": "Behavior", "Centre": "Center",
             "Grey": "Gray", "Normalise": "Normalize", "Serialise": "Serialize",
             "Initialise": "Initialize", "Optimise": "Optimize", "Neighbour": "Neighbor",
             "Catalogue": "Catalog", "Licence": "License", "Analyse": "Analyze"}

SUFFIXES = {".md", ".py", ".ps1", ".yml", ".yaml", ".cpp", ".h", ".hlsl", ".json"}
NAMES = {".clang-tidy", ".clang-format", ".editorconfig", ".gitattributes"}
SKIP = {".git", "x64", "ARM64", "packages", ".vs", "AppPackages", "__pycache__"}


def targets(root):
    for path in sorted(root.rglob("*")):
        if not path.is_file() or any(part in SKIP for part in path.parts):
            continue
        # This file is the only one exempt, because its word lists ARE the UK spellings. Its own
        # prose therefore goes unchecked, which is a small hole and a deliberate one.
        if path.name == "CheckSpelling.py":
            continue
        if path.suffix in SUFFIXES or path.name in NAMES:
            yield path


def cased(us, found):
    if found.isupper():
        return us.upper()
    if found[0].isupper():
        return us[0].upper() + us[1:]
    return us


def scan(text):
    """(line number, what was found, what it should be) for every hit."""
    for number, line in enumerate(text.splitlines(), 1):
        for uk, us in WORDS.items():
            for match in re.finditer(rf"\b{uk}\b", line, re.I):
                yield number, match.group(0), cased(us, match.group(0))
        for uk, us in FRAGMENTS.items():
            for match in re.finditer(rf"[A-Za-z]{uk}|{uk}[A-Za-z]", line):
                yield number, match.group(0), match.group(0).replace(uk, us)


def rewrite(text):
    for uk, us in WORDS.items():
        text = re.sub(rf"\b{uk}\b", lambda m: cased(us, m.group(0)), text, flags=re.I)
    for uk, us in FRAGMENTS.items():
        text = re.sub(rf"(?<=[A-Za-z]){uk}|{uk}(?=[A-Za-z])", us, text)
    return text


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=pathlib.Path, default=ROOT)
    parser.add_argument("--fix", action="store_true", help="rewrite in place; read the diff after")
    a = parser.parse_args()
    root = a.root.resolve()

    faults, files = 0, 0
    for path in targets(root):
        text = path.read_text(encoding="utf-8", errors="replace")
        hits = list(scan(text))
        files += 1
        if not hits:
            continue
        if a.fix:
            path.write_text(rewrite(text))
            print(f"  fixed {path.relative_to(root)}: {len(hits)} occurrence(s)")
            faults += len(hits)
            continue
        for number, found, want in hits:
            print(f"  {path.relative_to(root)}:{number}: {found!r} -> {want!r}")
            faults += 1

    print(f"\n{faults} occurrence(s) across {files} file(s) checked, against AGENTS.md R11.")
    if faults and not a.fix:
        print("  US spelling is the rule in prose and in identifiers alike. `--fix` rewrites them.")
    return 1 if faults and not a.fix else 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""Check the HUD handoff's geometry against its own rules, and against Interface.md section 1.

    python3 Scripts/CheckHudGeometry.py
    python3 Scripts/CheckHudGeometry.py --root .

Design/design_handoff_hud/geometry.json states every rectangle of the interface pass in integer
authored coordinates. Two different things can go wrong with it and neither is visible by reading:

FIRST, the layout can violate the rules it was authored to meet. Interface.md section 1 derives a
48/64/96 tier for every interactive target and 16 authored pixels of clear space between adjacent
ones, and the handoff's own acceptance criterion is that a test asserts both rather than a reviewer
measuring by eye. The margins are not comfortable -- twelve distinct pairs sit exactly on the 16px
floor -- so a button that grows by two pixels breaks the rule immediately and silently.

SECOND, and worse, the handoff RESTATES figures it does not own. The frame, the tiers and the
clearance are Interface.md's, copied into geometry.json for the emitter's convenience. Scripts/
CheckDesign.py polices figures inside Design/*.md and does not reach into this JSON, so a tier that
moves in Interface.md leaves a copy here that still reads as authoritative. That is the exact defect
the design-consistency skill exists for, one directory further out.

WHAT THIS DOES NOT DO YET: compare against GameClient/HudLayout.h. That file does not exist until
M1.14, and the plan's arrangement is that the C++ suite asserts over its constants while this gate
compares those constants to the JSON -- Python reads the file and C++ reads none, because R14 closes
the dependency list and there is no JSON parser in this tree. Until HudLayout.h lands, the layout
half below is skipped rather than failed, and says so.
"""
import argparse
import itertools
import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
GEOMETRY = "Design/design_handoff_hud/geometry.json"
INTERFACE = "Design/Interface.md"
LAYOUT_HEADER = "GameClient/HudLayout.h"

faults = []


def box_of(rect):
    """The hit box if one is given, else the drawn rect. Both are [x, y, w, h]."""
    return rect["hitRect"] or [rect["x"], rect["y"], rect["w"], rect["h"]]


def is_placed(box):
    """False for a rect whose along-edge coordinate is solved at runtime, like an alert."""
    return all(isinstance(value, int) for value in box)


def surface_of(name):
    """Rects that can be on screen together share a prefix -- build, sel, system, alert."""
    return name.split(".")[0]


def separation(first, second):
    """Clear space between two boxes: -1 if they overlap, None if they are only diagonal."""
    first_right, first_bottom = first[0] + first[2], first[1] + first[3]
    second_right, second_bottom = second[0] + second[2], second[1] + second[3]
    overlap_x = min(first_right, second_right) - max(first[0], second[0])
    overlap_y = min(first_bottom, second_bottom) - max(first[1], second[1])
    if overlap_x > 0 and overlap_y > 0:
        return -1
    if overlap_x > 0:
        return max(first[1], second[1]) - min(first_bottom, second_bottom)
    if overlap_y > 0:
        return max(first[0], second[0]) - min(first_right, second_right)
    return None


def check_against_interface(geometry, interface_text):
    """The figures geometry.json copies from Interface.md section 1 must still match it."""
    prose = re.sub(r"\s+", " ", interface_text)

    frame = geometry["frame"]
    if not re.search(rf"\*\*{frame['width']} ?. ?{frame['height']}\*\*", prose):
        faults.append(f"frame {frame['width']} x {frame['height']} is not stated in "
                      f"{INTERFACE} -- one of the two moved")

    tiers = geometry["tiers"]
    for key, label in (("floor", "Floor"), ("combat", "Combat"), ("underFire", "Under fire")):
        width, height = tiers[key]
        if not re.search(rf"\*\*{label}\*\* \| {width} ?. ?{height}", prose):
            faults.append(f"tier '{key}' is {width} x {height} in {GEOMETRY} but "
                          f"{INTERFACE} section 1's table does not say so")

    clearance = tiers["minClearance"]
    if not re.search(rf"\*\*{clearance} pixels of clear space\*\*", prose):
        faults.append(f"clearance floor is {clearance} in {GEOMETRY} but {INTERFACE} "
                      f"section 1 does not derive that number")


def check_rects(geometry):
    """Tier compliance, the mirror formula, frame bounds, overlap and clear space."""
    frame_width, frame_height = geometry["frame"]["width"], geometry["frame"]["height"]
    tiers = geometry["tiers"]
    clearance = tiers["minClearance"]
    rects = [dict(zip(geometry["rectFields"], row)) for row in geometry["rects"]]

    for rect in rects:
        drawn = [rect["x"], rect["y"], rect["w"], rect["h"]]
        if is_placed(drawn) and (drawn[0] < 0 or drawn[1] < 0
                                 or drawn[0] + drawn[2] > frame_width
                                 or drawn[1] + drawn[3] > frame_height):
            faults.append(f"{rect['name']} at {tuple(drawn)} falls outside the "
                          f"{frame_width} x {frame_height} frame")

        if rect["mirrorX"] is not None and isinstance(rect["x"], int):
            expected = frame_width - rect["x"] - rect["w"]
            if rect["mirrorX"] != expected:
                faults.append(f"{rect['name']} has mirrorX {rect['mirrorX']}, but "
                              f"x' = {frame_width} - x - w gives {expected}")

    interactive = [rect for rect in rects if rect["tier"]]
    for rect in interactive:
        box = box_of(rect)
        least_width, least_height = tiers[rect["tier"]]
        if box[2] < least_width or box[3] < least_height:
            faults.append(f"{rect['name']} is {box[2]} x {box[3]} against the "
                          f"'{rect['tier']}' tier's {least_width} x {least_height}")

        # Interface.md section 1 allows a target to be drawn smaller than it is hit, and nothing
        # else. A hit rect that does not contain what is drawn means a control whose edge does
        # nothing, or one that answers to a tap landing outside it -- and because the tier and
        # clearance rules above read the HIT rect, both go unnoticed when only the drawn rect moves.
        drawn = [rect["x"], rect["y"], rect["w"], rect["h"]]
        if rect["hitRect"] and is_placed(drawn) and is_placed(rect["hitRect"]):
            hit = rect["hitRect"]
            if (hit[0] > drawn[0] or hit[1] > drawn[1]
                    or hit[0] + hit[2] < drawn[0] + drawn[2]
                    or hit[1] + hit[3] < drawn[1] + drawn[3]):
                faults.append(f"{rect['name']} is drawn at {tuple(drawn)} but hit at "
                              f"{tuple(hit)}, which does not contain it")

    # Placement is static for everything but the alert, whose along-edge coordinate comes from a
    # bearing at runtime. Its size is checked above; its clearance is a runtime clamp.
    placed = [rect for rect in interactive if is_placed(box_of(rect))]
    for mirrored in (False, True):
        state = "mirrored" if mirrored else "primary"
        for surface in sorted({surface_of(rect["name"]) for rect in placed}):
            boxes = []
            for rect in (r for r in placed if surface_of(r["name"]) == surface):
                box = list(box_of(rect))
                if mirrored and rect["mirrorX"] is not None:
                    box[0] = rect["mirrorX"]
                boxes.append((rect["name"], box))
            for (left, first), (right, second) in itertools.combinations(boxes, 2):
                gap = separation(first, second)
                if gap is None:
                    continue
                if gap < 0:
                    faults.append(f"{left} and {right} overlap ({state} layout)")
                elif gap < clearance:
                    faults.append(f"{left} and {right} are {gap}px apart, under the "
                                  f"{clearance}px floor ({state} layout)")
    return len(rects), len(interactive), len(placed)


def check_layout_header(root):
    """Compare GameClient/HudLayout.h's constants to the JSON. Skipped until M1.14 writes it."""
    header = root / LAYOUT_HEADER
    if not header.exists():
        return False
    # M1.14 writes the header and extends this to read it. Failing here before that step exists
    # would gate the tree on a file the plan has not reached yet.
    return True


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=pathlib.Path, default=ROOT)
    root = parser.parse_args().root.resolve()

    geometry_path, interface_path = root / GEOMETRY, root / INTERFACE
    for path in (geometry_path, interface_path):
        if not path.exists():
            print(f"  {path.relative_to(root)} is missing.")
            return 1

    geometry = json.loads(geometry_path.read_text(encoding="utf-8"))
    check_against_interface(geometry, interface_path.read_text(encoding="utf-8"))
    total, interactive, placed = check_rects(geometry)
    compared_header = check_layout_header(root)

    for line in faults:
        print(f"  {line}")
    print(f"\n{len(faults)} issue(s) over {total} rects, {interactive} of them interactive "
          f"({placed} statically placed), in both handedness states.")
    if not compared_header:
        print(f"  {LAYOUT_HEADER} does not exist yet, so nothing compared the drawn layout to "
              f"this one. M1.14 writes it.")
    return 1 if faults else 0


if __name__ == "__main__":
    sys.exit(main())

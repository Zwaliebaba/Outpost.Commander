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

THIRD, the client can draw something other than what was designed. M1.14 wrote GameClient/HudLayout.h,
and the plan's arrangement is that the C++ suite asserts over its constants while this gate compares
those constants to the JSON -- Python reads the file and C++ reads none, because R14 closes the
dependency list and there is no JSON parser in this tree. Every constant that copies a row carries a
`// geometry: <name>` tag, and each tagged constant is compared field by field: a JSON field that is
null (a text box's width) is skipped, and a cell-relative "+16" compares as 16. A JSON row with a fixed
position that no constant claims is a fault too, because a rect the client never copied is a rect it
cannot be drawing where the handoff says.
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


LAYOUT_LINE = re.compile(r"inline constexpr HudRect (\w+)\{(-?\d+), (-?\d+), (-?\d+), (-?\d+)\};\s*// geometry: (\S+)")

# Rows this client cannot claim yet, because their position is solved at runtime (the alert's
# along-edge coordinate, the world-anchored hull bar) or because only a later milestone draws them
# (the armed and unavailable module states are M2's, and so is cargo). Listed so the exemption is
# visible rather than silent.
UNCLAIMABLE = ("alert.", "world.", "build.btn.armed.", "build.btn.hatch", "sel.group.cargo.")


def as_number(value):
    """A JSON field as a number to compare: an int, a cell-relative "+16", or None when not comparable."""
    if isinstance(value, int):
        return value
    if isinstance(value, str) and re.fullmatch(r"\+\d+", value):
        return int(value[1:])
    return None


def check_layout_header(root, geometry):
    """Compare GameClient/HudLayout.h's tagged constants to the JSON, rect for rect."""
    header = root / LAYOUT_HEADER
    if not header.exists():
        faults.append(f"{LAYOUT_HEADER} is missing -- M1.14 wrote it, and nothing now compares the drawn "
                      f"layout to the designed one")
        return 0

    rects = {row[0]: dict(zip(geometry["rectFields"], row)) for row in geometry["rects"]}
    claimed = set()
    compared = 0
    for number, line in enumerate(header.read_text(encoding="utf-8").splitlines(), start=1):
        match = LAYOUT_LINE.search(line)
        if not match:
            if line.lstrip().startswith("inline constexpr") and "// geometry:" in line:
                faults.append(f"{LAYOUT_HEADER}:{number} carries a geometry tag this gate cannot read")
            continue
        constant, name = match.group(1), match.group(6)
        values = [int(match.group(index)) for index in range(2, 6)]
        if name not in rects:
            faults.append(f"{LAYOUT_HEADER}:{number} {constant} names '{name}', which {GEOMETRY} does not have")
            continue
        claimed.add(name)
        compared += 1
        for field, value in zip(("x", "y", "w", "h"), values):
            expected = as_number(rects[name][field])
            if expected is not None and expected != value:
                faults.append(f"{constant} has {field} = {value}, but {GEOMETRY} says {name}.{field} = "
                              f"{rects[name][field]}")

    for name in rects:
        if name not in claimed and not name.startswith(UNCLAIMABLE):
            faults.append(f"{GEOMETRY} row '{name}' is claimed by no constant in {LAYOUT_HEADER}")
    return compared


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
    compared = check_layout_header(root, geometry)

    for line in faults:
        print(f"  {line}")
    print(f"\n{len(faults)} issue(s) over {total} rects, {interactive} of them interactive "
          f"({placed} statically placed), in both handedness states; {compared} constants in "
          f"{LAYOUT_HEADER} compared against them.")
    return 1 if faults else 0


if __name__ == "__main__":
    sys.exit(main())

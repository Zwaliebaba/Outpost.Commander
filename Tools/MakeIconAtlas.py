#!/usr/bin/env python3
"""Generate GameData/Textures/Icons.dds as the 32x32 grid of masks Design/Interface.md 3 asks for.

    python3 Tools/MakeIconAtlas.py [--out GameData/Textures]
    python3 Tools/MakeIconAtlas.py --describe          # print every cell as text, draw nothing

WHY THIS EXISTS. Interface.md 11 row 7 is "the icon list: six cursors and one icon per structure,
module, order and stance, with Icons.dds laid out as a grid of 32x32 cells", and it is marked
**blocks K4**. What ships today is the eight 64x64 Species order banners C4 imported unkeyed,
opaque and coloured (ADR-010) - a sheet of the right SIZE by coincidence and of the wrong content
and the wrong kind. Interface.md 3 is explicit that "an icon is a single-channel mask tinted at
draw time, never a coloured bitmap", and the banners are not masks.

SO THESE ARE PLACEHOLDERS AND SAY SO, exactly as Tools/MakePlaceholderModels.py's boxes are.
Species has no art for this game's structures, orders or stances, and inventing thirty-two drawn
icons is the owner's work rather than a generator's. What a generator CAN do honestly is give each
icon a DISTINCT SIMPLE SHAPE, so that a panel of buttons is readable as a panel of different
buttons and the one that is wired wrong is visible at a glance - which is the whole of what the
panels need from the sheet until the art exists.

THE OUTPUT IS A MASK IN EVERY CHANNEL. Interface.md 3 tints an icon by "colour.rgb with the atlas's
RED as the mask", so red is what carries coverage; green, blue and alpha carry the same value, so
that a reader opening the file sees the shape rather than a red smear and so that a pass that
sampled another channel by mistake still draws something.

Like every generator under Tools, this never ships (AGENTS.md R14), is run by hand, and its output
is committed. It needs no third-party module.
"""
from __future__ import annotations

import argparse
import math
import struct
import sys
from pathlib import Path

CELL = 32
COLUMNS = 8
ROWS = 4

DDS_MAGIC = 0x20534444
DDSD_CAPS, DDSD_HEIGHT, DDSD_WIDTH, DDSD_PIXELFORMAT = 0x1, 0x2, 0x4, 0x1000
DDSD_PITCH = 0x8
DDPF_FOURCC = 0x4
DDSCAPS_TEXTURE = 0x1000
DXGI_FORMAT_B8G8R8A8_UNORM = 87
D3D10_RESOURCE_DIMENSION_TEXTURE2D = 3

# The thirty-two icons Interface.md needs, IN CELL ORDER: the cell index is the row times eight plus
# the column, and GameData/Interface.json's "icons" table is what names each one for the panels.
# The order is the document's - six cursors, six structures, four modules, six orders, ten stances -
# so that a reader with Interface.md open finds a cell where they expect it.
ICONS: list[tuple[str, str]] = [
    # (name, shape)
    ("CursorArrow", "arrow"),
    ("CursorSelect", "bracket"),
    ("CursorMove", "chevron"),
    ("CursorAttack", "crosshair"),
    ("CursorBuild", "frame"),
    ("CursorRefuse", "slash"),
    ("StructureCommandPost", "tower"),
    ("StructureExtractor", "diamond"),
    ("StructureGenerator", "bolt"),
    ("StructureFactory", "house"),
    ("StructureResearchLab", "flask"),
    ("StructureTower", "turret"),
    ("ModuleMachineGun", "bars2"),
    ("ModuleCannon", "bars1"),
    ("ModuleMortar", "arc"),
    ("ModuleBuilder", "wrench"),
    ("OrderMove", "chevron"),
    ("OrderAttackMove", "chevroncross"),
    ("OrderPatrol", "loop"),
    ("OrderGuard", "shield"),
    ("OrderStop", "square"),
    ("OrderReturnToRepair", "cross"),
    ("StanceFireAtWill", "bars3"),
    ("StanceReturnFire", "bars2"),
    ("StanceHoldFire", "bars1"),
    ("StanceRangeOptimal", "ringsmall"),
    ("StanceRangeLong", "ringlarge"),
    ("StanceRetreat50", "half"),
    ("StanceRetreat25", "quarter"),
    ("StanceRetreatNever", "slash"),
    ("StancePursue", "chevron"),
    ("StanceHoldPosition", "anchor"),
]


def blank() -> list[list[int]]:
    return [[0] * CELL for _ in range(CELL)]


def plot(cell: list[list[int]], x: int, y: int, value: int = 255) -> None:
    if 0 <= x < CELL and 0 <= y < CELL:
        cell[y][x] = max(cell[y][x], value)


def line(cell: list[list[int]], x0: int, y0: int, x1: int, y1: int, width: int = 2) -> None:
    """A thick line by Bresenham, because every shape below is lines and a filled run."""
    steps = max(abs(x1 - x0), abs(y1 - y0), 1)
    for step in range(steps + 1):
        x = x0 + ((x1 - x0) * step) // steps
        y = y0 + ((y1 - y0) * step) // steps
        for dy in range(width):
            for dx in range(width):
                plot(cell, x + dx - width // 2, y + dy - width // 2)


def rectangle(cell: list[list[int]], x0: int, y0: int, x1: int, y1: int, fill: bool = False) -> None:
    if fill:
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                plot(cell, x, y)
        return
    line(cell, x0, y0, x1, y0)
    line(cell, x1, y0, x1, y1)
    line(cell, x1, y1, x0, y1)
    line(cell, x0, y1, x0, y0)


def ring(cell: list[list[int]], radius: int, width: int = 2) -> None:
    middle = CELL // 2
    for degree in range(0, 360 * 4):
        angle = math.radians(degree / 4.0)
        x = middle + int(round(math.cos(angle) * radius))
        y = middle + int(round(math.sin(angle) * radius))
        for dy in range(width):
            for dx in range(width):
                plot(cell, x + dx - width // 2, y + dy - width // 2)


def wedge(cell: list[list[int]], fraction: float) -> None:
    """A filled pie from twelve o'clock, clockwise, for the two retreat thresholds."""
    middle = CELL // 2
    radius = 12
    for y in range(CELL):
        for x in range(CELL):
            dx = x - middle
            dy = y - middle
            if (dx * dx) + (dy * dy) > radius * radius:
                continue
            angle = math.atan2(dx, -dy)
            if angle < 0.0:
                angle += 2.0 * math.pi
            if angle <= fraction * 2.0 * math.pi:
                plot(cell, x, y)


def shape(name: str) -> list[list[int]]:
    cell = blank()
    if name == "arrow":
        for step in range(18):
            line(cell, 8, 6 + step, 8 + (step // 2), 6 + step, 1)
        line(cell, 8, 6, 20, 18)
        line(cell, 8, 6, 8, 24)
        line(cell, 8, 24, 20, 18)
    elif name == "bracket":
        for corner in ((6, 6, 1, 1), (25, 6, -1, 1), (6, 25, 1, -1), (25, 25, -1, -1)):
            x, y, stepX, stepY = corner
            line(cell, x, y, x + (7 * stepX), y)
            line(cell, x, y, x, y + (7 * stepY))
    elif name in ("chevron", "chevroncross"):
        line(cell, 8, 20, 16, 10)
        line(cell, 16, 10, 24, 20)
        if name == "chevroncross":
            line(cell, 10, 24, 22, 24)
    elif name == "crosshair":
        ring(cell, 10)
        line(cell, 16, 2, 16, 9)
        line(cell, 16, 23, 16, 30)
        line(cell, 2, 16, 9, 16)
        line(cell, 23, 16, 30, 16)
    elif name == "frame":
        rectangle(cell, 6, 6, 25, 25)
        rectangle(cell, 12, 12, 19, 19, fill=True)
    elif name == "slash":
        ring(cell, 12)
        line(cell, 8, 8, 24, 24, 3)
    elif name == "tower":
        rectangle(cell, 11, 8, 20, 26, fill=True)
        line(cell, 16, 2, 16, 8, 3)
        line(cell, 8, 4, 24, 4)
    elif name == "diamond":
        for y in range(CELL):
            span = 12 - abs(y - 16)
            if span <= 0:
                continue
            for x in range(16 - span, 17 + span):
                plot(cell, x, y)
    elif name == "bolt":
        line(cell, 19, 4, 11, 17, 3)
        line(cell, 11, 17, 18, 17, 3)
        line(cell, 18, 17, 12, 28, 3)
    elif name == "house":
        rectangle(cell, 8, 15, 24, 26, fill=True)
        line(cell, 6, 15, 16, 6, 2)
        line(cell, 16, 6, 26, 15, 2)
    elif name == "flask":
        line(cell, 12, 5, 12, 14)
        line(cell, 20, 5, 20, 14)
        line(cell, 10, 5, 22, 5)
        for y in range(14, 27):
            span = 2 + ((y - 14) * 8) // 13
            for x in range(16 - span, 17 + span):
                plot(cell, x, y)
    elif name == "turret":
        rectangle(cell, 10, 18, 22, 27, fill=True)
        ring(cell, 6, 3)
        line(cell, 16, 10, 28, 6, 2)
    elif name in ("bars1", "bars2", "bars3"):
        count = int(name[-1])
        for index in range(count):
            rectangle(cell, 8, 20 - (index * 7), 24, 24 - (index * 7), fill=True)
    elif name == "arc":
        for degree in range(200, 341):
            angle = math.radians(degree)
            plot(cell, 16 + int(round(math.cos(angle) * 12)), 24 + int(round(math.sin(angle) * 12)))
            plot(cell, 16 + int(round(math.cos(angle) * 11)), 24 + int(round(math.sin(angle) * 11)))
        line(cell, 16, 24, 16, 28, 3)
    elif name == "wrench":
        line(cell, 8, 24, 22, 10, 3)
        ring(cell, 5, 3)
        rectangle(cell, 5, 21, 11, 27)
    elif name == "loop":
        ring(cell, 10, 2)
        line(cell, 22, 10, 26, 16, 2)
        line(cell, 22, 10, 18, 8, 2)
    elif name == "shield":
        for y in range(6, 27):
            span = 10 - max(0, (y - 16) * 10 // 10)
            if span <= 0:
                continue
            for x in range(16 - span, 17 + span):
                plot(cell, x, y, 255 if (y < 9 or y > 23 or span < 3 or x in (16 - span, 16 + span)) else 0)
        rectangle(cell, 6, 6, 26, 9)
    elif name == "square":
        rectangle(cell, 9, 9, 22, 22, fill=True)
    elif name == "cross":
        rectangle(cell, 13, 6, 18, 25, fill=True)
        rectangle(cell, 6, 13, 25, 18, fill=True)
    elif name == "ringsmall":
        ring(cell, 7, 3)
    elif name == "ringlarge":
        ring(cell, 13, 3)
    elif name == "half":
        wedge(cell, 0.5)
        ring(cell, 12, 2)
    elif name == "quarter":
        wedge(cell, 0.25)
        ring(cell, 12, 2)
    elif name == "anchor":
        line(cell, 16, 6, 16, 26, 3)
        line(cell, 8, 20, 16, 27, 3)
        line(cell, 24, 20, 16, 27, 3)
        line(cell, 10, 11, 22, 11, 3)
    else:
        raise AssertionError(f"no shape called {name}")
    return cell


def atlas() -> list[list[int]]:
    """Every cell laid into one COLUMNS x ROWS sheet, top row first."""
    width = COLUMNS * CELL
    height = ROWS * CELL
    sheet = [[0] * width for _ in range(height)]
    for index, (name, form) in enumerate(ICONS):
        cell = shape(form)
        originX = (index % COLUMNS) * CELL
        originY = (index // COLUMNS) * CELL
        for y in range(CELL):
            for x in range(CELL):
                sheet[originY + y][originX + x] = cell[y][x]
        assert name  # the name is the table's, not the picture's
    return sheet


def write_dds(path: Path, sheet: list[list[int]]) -> None:
    """The same header ImportTextures.py writes: top-down B8G8R8A8_UNORM, one level, no mips."""
    height = len(sheet)
    width = len(sheet[0])
    header = (
        struct.pack("<I", DDS_MAGIC)
        + struct.pack("<7I", 124, DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT | DDSD_PITCH, height, width, width * 4, 0, 1)
        + b"\0" * 44
        + struct.pack("<8I", 32, DDPF_FOURCC, 0x30315844, 0, 0, 0, 0, 0)
        + struct.pack("<5I", DDSCAPS_TEXTURE, 0, 0, 0, 0)
        + struct.pack("<5I", DXGI_FORMAT_B8G8R8A8_UNORM, D3D10_RESOURCE_DIMENSION_TEXTURE2D, 0, 1, 0)
    )
    payload = bytearray()
    for row in sheet:
        for value in row:
            payload += bytes((value, value, value, value))
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(header + bytes(payload))


def describe(sheet: list[list[int]]) -> None:
    """Every cell as text, so that a session with no way to open a DDS can still read the sheet."""
    ramp = " .:-=+*#%@"
    for index, (name, form) in enumerate(ICONS):
        originX = (index % COLUMNS) * CELL
        originY = (index // COLUMNS) * CELL
        covered = sum(1 for y in range(CELL) for x in range(CELL) if sheet[originY + y][originX + x] > 0)
        print(f"cell {index:2d}  {name:<24} {form:<12} {100.0 * covered / (CELL * CELL):5.1f}% covered")
        for y in range(0, CELL, 2):
            row = "".join(ramp[min(9, sheet[originY + y][originX + x] * 10 // 256)] for x in range(0, CELL, 1))
            print(f"    |{row}|")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--out", default="GameData/Textures", help="where Icons.dds is written")
    parser.add_argument("--describe", action="store_true", help="print every cell as text and write nothing")
    arguments = parser.parse_args()
    assert len(ICONS) == COLUMNS * ROWS, f"{len(ICONS)} icons in a sheet of {COLUMNS * ROWS} cells"
    names = [name for name, _ in ICONS]
    assert len(set(names)) == len(names), "an icon name is used twice"
    sheet = atlas()
    if arguments.describe:
        describe(sheet)
        return 0
    path = Path(arguments.out) / "Icons.dds"
    write_dds(path, sheet)
    covered = sum(1 for row in sheet for value in row if value > 0)
    print(f"{path}: {len(sheet[0])}x{len(sheet)}, {len(ICONS)} cells of {CELL}, {100.0 * covered / (len(sheet) * len(sheet[0])):.1f}% covered")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

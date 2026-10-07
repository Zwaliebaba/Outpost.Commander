#!/usr/bin/env python3
"""Writes the repository's map, OutpostCommander/Assets/Map.json (ADR-036 decision 4, Phase 4 design §6).

The map is 10 km a side: 25 sectors of 2 km on a 5 x 5 grid, named by column A to E from west to east and row 1 to 5 from
south to north, the homes in the corners A1 and E5, point-symmetric about the center. Player 1's asteroids are listed by
sector, and each is mirrored through the center for player 2. Two asteroid fields stand on every border between two
sectors, leaving a passage at its middle and one at each corner.

  python Tools/MakeMap.py           rewrites the map
  python Tools/MakeMap.py --check   fails if the committed map is not what this writes
"""

import json
import pathlib
import sys

SIZE_METERS = 10000
SECTOR_METERS = 2000
EDGES = [-SIZE_METERS // 2 + SECTOR_METERS * k for k in range(6)]
CENTERS = [edge + SECTOR_METERS // 2 for edge in EDGES[:5]]
COLUMNS = "ABCDE"
# How far in from each end of a border its two fields stand.
FIELD_INSET_METERS = 470
# A field's radius, picked by its place so that a field and its mirror have the same.
FIELD_RADII = [170, 150, 190, 160, 180]

# Player 1's ore, by sector (column, row from 0), as offsets from the sector's center.
PLAYER_ONE_ORE = {
    (0, 0): [(0, 260, "home"), (260, 0, "home"), (190, 190, "home")],
    (1, 0): [(-350, 520, "near"), (420, -380, "near")],
    (0, 1): [(520, -350, "near"), (-380, 420, "near")],
    (2, 0): [(-450, 480, "near")],
    (1, 1): [(480, 450, "near")],
    (0, 2): [(480, -450, "near")],
    (3, 0): [(-520, 420, "contested")],
    (2, 1): [(450, 520, "contested")],
    (1, 2): [(-520, -450, "contested")],
    (0, 3): [(420, -520, "contested")],
    (0, 4): [(380, -520, "rich"), (-480, 400, "rich")],
    # B4, whose mirror is D2: one asteroid nearer each start.
    (1, 3): [(-500, -380, "contested"), (520, 420, "contested")],
    # The center, its own mirror.
    (2, 2): [(480, -300, "contested"), (-150, 560, "contested")],
}
RADIUS_METERS = {"home": 45, "near": 45, "contested": 45, "rich": 60}
RESERVE_ORE = {"home": 7500, "near": 9000, "contested": 12000, "rich": 24000}


def Asteroid(_x, _z, _yield):
    return {"xMeters": _x, "zMeters": _z, "radiusMeters": RADIUS_METERS[_yield], "yield": _yield, "reserve": RESERVE_ORE[_yield]}


def Asteroids():
    asteroids = []
    for (column, row), ores in PLAYER_ONE_ORE.items():
        for dx, dz, ore in ores:
            x, z = CENTERS[column] + dx, CENTERS[row] + dz
            asteroids.append(Asteroid(x, z, ore))
            asteroids.append(Asteroid(-x, -z, ore))
    return asteroids


def Field(_x, _z):
    radius = FIELD_RADII[(abs(_x) // 10 * 7 + abs(_z) // 10 * 3) % len(FIELD_RADII)]
    return {"xMeters": _x, "zMeters": _z, "radiusMeters": radius}


def Fields():
    fields = []
    for border in EDGES[1:5]:
        for start in EDGES[:5]:
            near, far = start + FIELD_INSET_METERS, start + SECTOR_METERS - FIELD_INSET_METERS
            fields += [Field(border, near), Field(border, far), Field(near, border), Field(far, border)]
    return fields


def Sectors():
    sectors = []
    for row in range(5):
        for column in range(5):
            adjacent = [(row + dz) * 5 + column + dx + 1 for dx, dz in ((0, -1), (-1, 0), (1, 0), (0, 1))
                        if 0 <= column + dx < 5 and 0 <= row + dz < 5]
            sectors.append({"id": row * 5 + column + 1, "name": f"{COLUMNS[column]}{row + 1}",
                            "minXMeters": EDGES[column], "maxXMeters": EDGES[column + 1],
                            "minZMeters": EDGES[row], "maxZMeters": EDGES[row + 1],
                            "node": {"xMeters": CENTERS[column], "zMeters": CENTERS[row]}, "adjacent": sorted(adjacent)})
    return sectors


def Inline(_object):
    return "{ " + ", ".join(f"{json.dumps(key)}: {json.dumps(value)}" for key, value in _object.items()) + " }"


def Listed(_name, _items, _last=False):
    lines = [f"  \"{_name}\": ["]
    lines += [f"    {item}" + ("," if index + 1 < len(_items) else "") for index, item in enumerate(_items)]
    lines.append("  ]" if _last else "  ],")
    return lines


def MapText():
    starts = [Inline({"xMeters": CENTERS[0], "zMeters": CENTERS[0]}), Inline({"xMeters": CENTERS[4], "zMeters": CENTERS[4]})]
    sectors = []
    for sector in Sectors():
        head = {key: sector[key] for key in ("id", "name", "minXMeters", "maxXMeters", "minZMeters", "maxZMeters")}
        tail = f"\"node\": {Inline(sector['node'])}, \"adjacent\": {json.dumps(sector['adjacent'])} }}"
        sectors.append(f"{Inline(head)[:-2]},\r\n      {tail}")
    lines = ["{", f"  \"sizeMeters\": {SIZE_METERS},", "  \"minimumGapMeters\": 60,"]
    lines += Listed("starts", starts)
    lines += Listed("oreAsteroids", [Inline(asteroid) for asteroid in Asteroids()])
    lines += Listed("asteroidFields", [Inline(field) for field in Fields()])
    lines += Listed("sectors", sectors, _last=True)
    lines.append("}")
    # JSON in the repository is CRLF (.editorconfig).
    return "\r\n".join(lines) + "\r\n"


def Main():
    path = pathlib.Path(__file__).resolve().parent.parent / "OutpostCommander" / "Assets" / "Map.json"
    text = MapText().encode("utf-8")
    if "--check" in sys.argv[1:]:
        if path.read_bytes() != text:
            print(f"{path} is not what Tools/MakeMap.py writes.")
            return 1
        print("Map.json: as written.")
        return 0
    path.write_bytes(text)
    print(f"Wrote {path}.")
    return 0


if __name__ == "__main__":
    sys.exit(Main())

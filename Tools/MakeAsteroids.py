#!/usr/bin/env python3
"""Generates the asteroid meshes' glTF sources, Art/Models/Asteroids/<Size>.glb (owner, 2026-10-02).

The rocks are low-poly on purpose: big flat facets with clear ridges between them, the eighties vector look the crease
lines draw (GameClient's DrawRock). Each is the convex hull of points spread evenly over an ellipsoid and jittered, with
a few of its corners pushed in so that it is not a gem, flat-shaded: each triangle has its own three corners and its
face normal. The ellipsoid's long axis is glTF's +z, which is the game's +x, the axis a mesh is fitted to its length by
(ADR-018), so a rock never pokes out of its radius.

Everything comes from a fixed seed through SplitMix64 written out here, so a run writes the same bytes every time and
on every machine. The files are written with BakeMeshes.write_glb and then baked like any other source: run
python Tools/BakeMeshes.py after this, and commit the .glb and .nmf files together.

Usage:
  python Tools/MakeAsteroids.py           write the three sources
"""

import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from BakeMeshes import write_glb  # noqa: E402  (a sibling script, not a package)

OUTPUT_DIRECTORY = Path(__file__).resolve().parent.parent / "Art" / "Models" / "Asteroids"

# Each size: its point count, which sets how many facets it has (about twice as many triangles), the ellipsoid's axes
# along glTF's x, y and z with z the longest, how far each point may stray from the ellipsoid as a share of it, how many
# corners are pushed in and by how much, and its seed.
SIZES = [
  {"name": "Small", "points": 18, "axes": (0.80, 0.70, 1.00), "jitter": 0.16, "dents": 2, "dent": 0.16, "seed": 0x5EED_0001},
  {"name": "Medium", "points": 30, "axes": (0.82, 0.72, 1.00), "jitter": 0.14, "dents": 4, "dent": 0.13, "seed": 0x5EED_0002},
  {"name": "Large", "points": 46, "axes": (0.85, 0.75, 1.00), "jitter": 0.12, "dents": 6, "dent": 0.11, "seed": 0x5EED_0003},
]

EPSILON = 1e-9
# How far the tips of the long axis reach, as a share of the farthest other corner's distance from the center.
TIP_REACH = 1.03


class SplitMix64:
  def __init__(self, seed):
    self.state = seed & 0xFFFF_FFFF_FFFF_FFFF

  def next(self):
    self.state = (self.state + 0x9E37_79B9_7F4A_7C15) & 0xFFFF_FFFF_FFFF_FFFF
    mixed = self.state
    mixed = ((mixed ^ (mixed >> 30)) * 0xBF58_476D_1CE4_E5B9) & 0xFFFF_FFFF_FFFF_FFFF
    mixed = ((mixed ^ (mixed >> 27)) * 0x94D0_49BB_1331_11EB) & 0xFFFF_FFFF_FFFF_FFFF
    return mixed ^ (mixed >> 31)

  def unit(self):
    """A number in [0, 1) from the top 53 bits of one draw."""
    return (self.next() >> 11) / float(1 << 53)

  def signed(self, extent):
    return (2.0 * self.unit() - 1.0) * extent


def sub(a, b):
  return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def cross(a, b):
  return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def dot(a, b):
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def normalized(a):
  length = math.sqrt(dot(a, a))
  return (a[0] / length, a[1] / length, a[2] / length)


def scatter(size):
  """Points spread evenly over the unit sphere by a Fibonacci spiral, jittered, then stretched onto the ellipsoid."""
  random = SplitMix64(size["seed"])
  count = size["points"]
  golden = math.pi * (3.0 - math.sqrt(5.0))
  points = []
  for i in range(count):
    y = 1.0 - 2.0 * (i + 0.5) / count
    ring = math.sqrt(max(0.0, 1.0 - y * y))
    angle = golden * i + random.signed(0.35)
    direction = normalized((ring * math.cos(angle) + random.signed(0.12), y + random.signed(0.12),
                            ring * math.sin(angle) + random.signed(0.12)))
    reach = 1.0 + random.signed(size["jitter"])
    axes = size["axes"]
    points.append((direction[0] * axes[0] * reach, direction[1] * axes[1] * reach, direction[2] * axes[2] * reach))
  return points, random


def convex_hull(points):
  """The hull's triangles as corner indices, wound counterclockwise seen from outside, glTF's front. Incremental: start
  from a tetrahedron, then add each point outside the hull and replace the faces it sees with a fan to their horizon."""
  def plane(face):
    a, b, c = (points[index] for index in face)
    return normalized(cross(sub(b, a), sub(c, a))), a

  def outside(face, point):
    normal, origin = plane(face)
    return dot(normal, sub(point, origin)) > EPSILON

  first = (0, 1, 2)
  fourth = next(i for i in range(3, len(points))
                if abs(dot(cross(sub(points[1], points[0]), sub(points[2], points[0])), sub(points[i], points[0]))) > 1e-6)
  center = tuple(sum(points[i][axis] for i in (0, 1, 2, fourth)) / 4.0 for axis in range(3))
  faces = []
  for face in (first, (0, 1, fourth), (1, 2, fourth), (2, 0, fourth)):
    normal, origin = plane(face)
    faces.append(face if dot(normal, sub(origin, center)) > 0.0 else (face[0], face[2], face[1]))

  for index in range(3, len(points)):
    if index == fourth:
      continue
    seen = [face for face in faces if outside(face, points[index])]
    if not seen:
      continue
    # The horizon is every edge of a seen face whose reverse is not also an edge of a seen face.
    edges = {(face[k], face[(k + 1) % 3]) for face in seen for k in range(3)}
    horizon = [edge for edge in edges if (edge[1], edge[0]) not in edges]
    faces = [face for face in faces if face not in seen]
    faces.extend((edge[0], edge[1], index) for edge in horizon)
  return faces


def make(size):
  points, random = scatter(size)

  # The game fits a mesh to its length along its long axis, centered on its bounds, and blocks a circle of that half
  # length (ADR-018), so no corner may reach farther from the bounds' center than the long half-axis. The two points at
  # the ends of the long axis become its tips: set on the axis through the center, a little beyond the farthest other
  # point, they make the long half-axis the rock's reach. They are placed before the hull is built round them.
  top = max(range(len(points)), key=lambda index: points[index][2])
  bottom = min(range(len(points)), key=lambda index: points[index][2])
  others = [point for index, point in enumerate(points) if index not in (top, bottom)]
  low = [min(point[axis] for point in others) for axis in range(3)]
  high = [max(point[axis] for point in others) for axis in range(3)]
  center = tuple((low[axis] + high[axis]) / 2.0 for axis in range(3))
  far = max(math.sqrt(dot(sub(point, center), sub(point, center))) for point in others)
  points[top] = (center[0], center[1], center[2] + far * TIP_REACH)
  points[bottom] = (center[0], center[1], center[2] - far * TIP_REACH)

  faces = convex_hull(points)
  # Push a few corners in, toward the center, so that the rock has hollows as well as ridges. Not the tips, which hold
  # its reach, and none twice.
  corners = sorted({index for face in faces for index in face} - {top, bottom})
  for _ in range(size["dents"]):
    corner = corners.pop(int(random.unit() * len(corners)))
    keep = 1.0 - size["dent"] * (0.6 + 0.4 * random.unit())
    points[corner] = tuple(center[axis] + (points[corner][axis] - center[axis]) * keep for axis in range(3))

  positions, normals, indices = [], [], []
  for face in faces:
    a, b, c = (points[index] for index in face)
    normal = normalized(cross(sub(b, a), sub(c, a)))
    for corner in (a, b, c):
      indices.append(len(positions))
      positions.append(corner)
      normals.append(normal)
  return {"name": size["name"], "positions": positions, "normals": normals, "indices": indices}, faces, points


def main():
  for size in SIZES:
    mesh, faces, points = make(size)
    target = OUTPUT_DIRECTORY / f"{size['name']}.glb"
    target.write_bytes(write_glb([mesh]))
    print(f"wrote {target.relative_to(OUTPUT_DIRECTORY.parent.parent.parent)}: {len(faces)} triangles")
  return 0


if __name__ == "__main__":
  sys.exit(main())

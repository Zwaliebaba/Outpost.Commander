#!/usr/bin/env python3
"""Bakes the meshes' glTF sources into the NMF files the game loads (ADR-018, ADR-045).

Each model's source is a binary glTF, Art/Models/<Set>/<Model>.glb, edited with Blender's own glTF import and export.
This turns each into OutpostCommander/Assets/Models/<Set>/<Model>.nmf: one triangle list of positions and normals, the
model's hardpoints and its spinning parts, in the game's frame. A model that grows has one source a level,
<Model>_L1.glb to <Model>_L5.glb, and each is baked as a model of its own. The .nmf files are committed, and CI runs
--check to prove each one is exactly what its source bakes to. CPython's floats are IEEE doubles with no fused
operations, so a bake is the same bytes on every machine.

The frames. glTF is right-handed with +y up, +z forward and -x to the right; Blender's exporter turns its own z-up
frame into that one. The game is left-handed with +y up and +x forward (ADR-011, ADR-012). A point (x, y, z) of the
source is (z, y, x) in the game, and every triangle's corners are reversed: that maps front to front, up to up and
right to right, so the game shows the model as Blender does and not its mirror image, and the triangles still wind
clockwise seen from their front, Direct3D's default.

A hardpoint is a node named hp_<tag>, with Blender's own .001 suffixes allowed: an empty in Blender. Its tag is
lowercase letters and digits, and the game decides what it means. Its frame is the empty's: forward is the empty's +z,
the axis Blender's Single Arrow display draws, and up is its +y. Its size is the empty's scale, which must be uniform.
Everything is baked in world space, so it does not matter whether an empty is parented to the model or a transform is
applied.

A spinning part is a node whose rotation an animation turns: a radar on its mast, say. The animation must be a steady
spin, which is what Blender's keys for one make: linear keys, each less than half a turn from the last, all about one
axis through the node's origin, turning at one rate, and ending a whole number of turns from the first. The part is the
node and everything under it, baked as its first key stands, and the game turns it about the same axis at the same
rate. A part may not hold a hardpoint or another part. The node's extras, such as a spin_period_s, are not read: the
animation is what Blender plays.

What a source may hold is narrow, and anything else is refused with the file's name rather than baked into something
the game would draw wrongly: triangles only, a position and a normal on every vertex, no skin or morph target, no
animation but a part's spin, no extension the file requires (Draco or quantized meshes, say), and no sparse accessor.
Materials, texture coordinates, cameras and lights are left in the source and not baked.

The NMF layout, little-endian, version 2:

  char magic[4] = "NMF\\0"; u32 version; u32 vertexCount; u32 indexCount; u32 hardpointCount; u32 partCount; u32 flags = 0
  vertexCount    x { f32 position[3]; f32 normal[3]; }
  indexCount     x u32, a multiple of 3, each below vertexCount
  hardpointCount x { u8 tagLength; char tag[tagLength]; f32 position[3]; f32 forward[3]; f32 up[3]; f32 size; }
  partCount      x { u32 firstIndex; u32 indexCount; f32 pivot[3]; f32 axis[3]; f32 periodSeconds; }

The fixed triangles come first, then each part's, so the parts' index runs follow each other to the end. A part turns a
whole turn each period about its pivot, the way DirectXMath's XMMatrixRotationAxis(axis, angle) turns a point as the
angle grows: clockwise, looking along the axis toward the origin, in the game's left-handed frame.

Usage:
  python Tools/BakeMeshes.py                 bake every source
  python Tools/BakeMeshes.py --check         fail when an .nmf is not what its source bakes to, or has no source
  python Tools/BakeMeshes.py --list <glb>    print a source's hardpoints as the game will see them
  python Tools/BakeMeshes.py --self-test     bake small sources built here and check what comes out and what is refused

Exit status: 0 when all is well, 1 when a check fails or a source is refused.
"""

import argparse
import json
import math
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SOURCE_DIRECTORY = ROOT / "Art" / "Models"
OUTPUT_DIRECTORY = ROOT / "OutpostCommander" / "Assets" / "Models"

NMF_MAGIC = b"NMF\0"
NMF_VERSION = 2
NMF_HEADER = struct.Struct("<4s6I")
NMF_VERTEX = struct.Struct("<6f")
NMF_HARDPOINT_FLOATS = struct.Struct("<10f")
NMF_PART = struct.Struct("<2I7f")
MAXIMUM_TAG_LENGTH = 31

GLB_MAGIC = b"glTF"
GLB_VERSION = 2
GLB_JSON_CHUNK = 0x4E4F534A
GLB_BIN_CHUNK = 0x004E4942
TRIANGLES = 4
FLOAT = 5126
INDEX_COMPONENTS = {5121: ("B", 1), 5123: ("H", 2), 5125: ("I", 4)}

HARDPOINT_NAME = re.compile(r"hp_([a-z][a-z0-9]*)(\.[0-9]+)?")
# A hardpoint's scale is uniform when its axes' lengths agree to this share, which Blender's float32 easily meets.
UNIFORM_SCALE_TOLERANCE = 1e-4
# A spin's keys are about one axis when what lies off it is below this, and turn at one rate when each key's angle is
# within this many radians of where the rate puts it; Blender's float32 keys meet both easily.
SPIN_AXIS_TOLERANCE = 1e-4
SPIN_ANGLE_TOLERANCE = 1e-3
FULL_TURN = 2.0 * math.pi


class BakeError(Exception):
  """A source the game cannot take, with the reason."""


# ── Small matrix helpers: 4x4 as rows, acting on column vectors, as glTF writes them ────────────────────────────────


def identity():
  return [[1.0 if row == column else 0.0 for column in range(4)] for row in range(4)]


def multiply(a, b):
  return [[sum(a[row][k] * b[k][column] for k in range(4)) for column in range(4)] for row in range(4)]


def node_matrix(node):
  """A node's local transform: its matrix, or its translation, rotation and scale."""
  if "matrix" in node:
    values = node["matrix"]
    if len(values) != 16:
      raise BakeError(f"node '{node.get('name', '')}' has a matrix that is not 16 numbers")
    # glTF stores the matrix column by column.
    return [[float(values[column * 4 + row]) for column in range(4)] for row in range(4)]
  tx, ty, tz = (float(value) for value in node.get("translation", [0.0, 0.0, 0.0]))
  qx, qy, qz, qw = (float(value) for value in node.get("rotation", [0.0, 0.0, 0.0, 1.0]))
  sx, sy, sz = (float(value) for value in node.get("scale", [1.0, 1.0, 1.0]))
  rotation = [
    [1 - 2 * (qy * qy + qz * qz), 2 * (qx * qy - qz * qw), 2 * (qx * qz + qy * qw)],
    [2 * (qx * qy + qz * qw), 1 - 2 * (qx * qx + qz * qz), 2 * (qy * qz - qx * qw)],
    [2 * (qx * qz - qy * qw), 2 * (qy * qz + qx * qw), 1 - 2 * (qx * qx + qy * qy)],
  ]
  scale = (sx, sy, sz)
  matrix = [[rotation[row][column] * scale[column] for column in range(3)] + [t] for row, t in enumerate((tx, ty, tz))]
  return matrix + [[0.0, 0.0, 0.0, 1.0]]


def transform_point(matrix, point):
  return tuple(matrix[row][0] * point[0] + matrix[row][1] * point[1] + matrix[row][2] * point[2] + matrix[row][3] for row in range(3))


def transform_direction(matrix, direction):
  return tuple(matrix[row][0] * direction[0] + matrix[row][1] * direction[1] + matrix[row][2] * direction[2] for row in range(3))


def determinant3(matrix):
  (a, b, c), (d, e, f), (g, h, i) = (row[:3] for row in matrix[:3])
  return a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g)


def normal_matrix(matrix):
  """The inverse transpose of the upper 3x3, which carries normals; its scale does not matter, they are renormalized."""
  (a, b, c), (d, e, f), (g, h, i) = (row[:3] for row in matrix[:3])
  # The cofactor matrix is the inverse transpose times the determinant.
  return [[e * i - f * h, f * g - d * i, d * h - e * g], [c * h - b * i, a * i - c * g, b * g - a * h],
          [b * f - c * e, c * d - a * f, a * e - b * d]]


def length(vector):
  return math.sqrt(sum(component * component for component in vector))


def normalized(vector, what):
  size = length(vector)
  if not math.isfinite(size) or size <= 0.0:
    raise BakeError(f"{what} has no direction")
  return tuple(component / size for component in vector)


def to_game(vector):
  """The game's coordinates of a source point or direction: glTF's +z front becomes +x, and right stays right."""
  return (vector[2], vector[1], vector[0])


def dot(a, b):
  return sum(x * y for x, y in zip(a, b))


def quaternion_product(a, b):
  """The rotation b then a, as glTF writes quaternions: (x, y, z, w)."""
  ax, ay, az, aw = a
  bx, by, bz, bw = b
  return (aw * bx + ax * bw + ay * bz - az * by, aw * by - ax * bz + ay * bw + az * bx,
          aw * bz + ax * by - ay * bx + az * bw, aw * bw - ax * bx - ay * by - az * bz)


# ── Reading a .glb ──────────────────────────────────────────────────────────────────────────────────────────────────


def read_glb(data):
  """The JSON and the binary chunk of a .glb's bytes."""
  if len(data) < 12:
    raise BakeError("it is too short to be a binary glTF")
  magic, version, total = struct.unpack_from("<4sII", data, 0)
  if magic != GLB_MAGIC or version != GLB_VERSION:
    raise BakeError("it is not a version 2 binary glTF")
  if total != len(data):
    raise BakeError(f"its header says {total} bytes, and it has {len(data)}")
  offset = 12
  document = None
  binary = b""
  while offset < total:
    if offset + 8 > total:
      raise BakeError("a chunk header runs past the end")
    chunk_length, chunk_type = struct.unpack_from("<II", data, offset)
    offset += 8
    if offset + chunk_length > total:
      raise BakeError("a chunk runs past the end")
    chunk = data[offset:offset + chunk_length]
    offset += chunk_length
    if chunk_type == GLB_JSON_CHUNK and document is None:
      document = json.loads(chunk.decode("utf-8"))
    elif chunk_type == GLB_BIN_CHUNK and not binary:
      binary = chunk
  if document is None:
    raise BakeError("it has no JSON chunk")
  return document, binary


def read_accessor(document, binary, index, component_types, item_type):
  """An accessor's items as tuples, checked against the component types and item type the caller takes."""
  accessors = document.get("accessors", [])
  if not 0 <= index < len(accessors):
    raise BakeError(f"accessor {index} does not exist")
  accessor = accessors[index]
  if "sparse" in accessor:
    raise BakeError(f"accessor {index} is sparse")
  if accessor.get("normalized", False):
    raise BakeError(f"accessor {index} is normalized")
  component_type = accessor.get("componentType")
  if component_type not in component_types:
    raise BakeError(f"accessor {index} has component type {component_type}, which is not one the game reads here")
  if accessor.get("type") != item_type:
    raise BakeError(f"accessor {index} is a {accessor.get('type')}, not a {item_type}")
  count = accessor["count"]
  width = {"SCALAR": 1, "VEC3": 3, "VEC4": 4}[item_type]
  code, size = (("f", 4) if component_type == FLOAT else INDEX_COMPONENTS[component_type])
  item_bytes = size * width
  if "bufferView" not in accessor:
    raise BakeError(f"accessor {index} has no buffer view")
  view = document["bufferViews"][accessor["bufferView"]]
  if view.get("buffer", 0) != 0 or "uri" in document["buffers"][view.get("buffer", 0)]:
    raise BakeError(f"accessor {index} reads a buffer outside the file")
  stride = view.get("byteStride", item_bytes)
  start = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
  end = start + (stride * (count - 1)) + item_bytes if count > 0 else start
  if end > view.get("byteOffset", 0) + view["byteLength"] or end > len(binary):
    raise BakeError(f"accessor {index} runs past its buffer")
  item = struct.Struct("<" + code * width)
  return [item.unpack_from(binary, start + i * stride) for i in range(count)]


def read_spins(document, binary):
  """Each spinning part's node index and its spin: the rotation it is baked at, its axis in its parent's frame, and the
  seconds a turn takes."""
  nodes = document.get("nodes", [])
  spins = {}
  for animation in document.get("animations", []):
    samplers = animation.get("samplers", [])
    for channel in animation.get("channels", []):
      target = channel.get("target", {})
      node_index = target.get("node")
      if not isinstance(node_index, int) or not 0 <= node_index < len(nodes):
        raise BakeError("an animation turns a node that does not exist")
      name = nodes[node_index].get("name", str(node_index))
      if target.get("path") != "rotation":
        raise BakeError(f"node '{name}' is animated in its {target.get('path')}, and the game only spins parts")
      if node_index in spins:
        raise BakeError(f"node '{name}' is turned by more than one animation")
      if "matrix" in nodes[node_index]:
        raise BakeError(f"node '{name}' is animated and has a matrix")
      sampler = samplers[channel["sampler"]]
      if sampler.get("interpolation", "LINEAR") != "LINEAR":
        raise BakeError(f"node '{name}' is animated with {sampler['interpolation']} keys, and a spin's are linear")
      times = [item[0] for item in read_accessor(document, binary, sampler["input"], {FLOAT}, "SCALAR")]
      keys = read_accessor(document, binary, sampler["output"], {FLOAT}, "VEC4")
      spins[node_index] = read_spin(f"the animation of node '{name}'", times, keys)
  return spins


def read_spin(what, times, keys):
  """The spin a rotation channel's keys make, or a refusal when they are not a steady spin."""
  if len(times) < 2 or len(times) != len(keys):
    raise BakeError(f"{what} has fewer than two keys, or not one rotation for each time")
  if not all(math.isfinite(value) for value in times) or any(later <= earlier for earlier, later in zip(times, times[1:])):
    raise BakeError(f"{what} has times that do not rise")
  rotations = [normalized(key, f"a key of {what}") for key in keys]
  first = rotations[0]
  undo_first = (-first[0], -first[1], -first[2], first[3])
  # Each key as a turn from the first, in the node's parent's frame: the rotation that takes the first key to it.
  turns = [quaternion_product(rotation, undo_first) for rotation in rotations]
  # The axis turns the second key forward from the first by less than half a turn.
  second = turns[1] if turns[1][3] >= 0.0 else tuple(-value for value in turns[1])
  if length(second[:3]) <= SPIN_AXIS_TOLERANCE:
    raise BakeError(f"{what} does not turn from its first key to its second")
  axis = normalized(second[:3], what)
  angles = [0.0]
  for turn in turns[1:]:
    along = dot(turn[:3], axis)
    if length(tuple(turn[i] - along * axis[i] for i in range(3))) > SPIN_AXIS_TOLERANCE:
      raise BakeError(f"{what} does not turn about one axis")
    # The same turn a whole turn on is the same rotation; it is the one within half a turn of the key before.
    angle = 2.0 * math.atan2(along, turn[3])
    angle += FULL_TURN * round((angles[-1] - angle) / FULL_TURN)
    step = angle - angles[-1]
    if step <= 0.0 or step >= math.pi - SPIN_ANGLE_TOLERANCE:
      raise BakeError(f"{what} has keys that turn back, stand still or are half a turn or more apart")
    angles.append(angle)
  whole_turns = round(angles[-1] / FULL_TURN)
  if whole_turns < 1 or abs(angles[-1] - whole_turns * FULL_TURN) > SPIN_ANGLE_TOLERANCE:
    raise BakeError(f"{what} does not end a whole number of turns from where it starts")
  span = times[-1] - times[0]
  rate = whole_turns * FULL_TURN / span
  if any(abs(angle - rate * (time - times[0])) > SPIN_ANGLE_TOLERANCE for angle, time in zip(angles, times)):
    raise BakeError(f"{what} does not turn at one rate")
  return {"rotation": first, "axis": axis, "period": span / whole_turns}


def walk(document, node_index, parent, depth, visit, part):
  """Visits a node and everything under it. visit takes the node, its parent's and its own world matrix and the part it
  is in, and gives the part its children are in."""
  if depth > len(document.get("nodes", [])):
    raise BakeError("its nodes form a cycle")
  node = document["nodes"][node_index]
  world = multiply(parent, node_matrix(node))
  part = visit(node_index, node, parent, world, part)
  for child in node.get("children", []):
    walk(document, child, world, depth + 1, visit, part)


def bake(data):
  """The .nmf bytes of a .glb's bytes."""
  document, binary = read_glb(data)
  if document.get("asset", {}).get("version") != "2.0":
    raise BakeError("it is not glTF 2.0")
  if document.get("extensionsRequired"):
    raise BakeError(f"it requires the extensions {', '.join(document['extensionsRequired'])}")
  scenes = document.get("scenes", [])
  if not scenes:
    raise BakeError("it has no scene")
  scene = scenes[document.get("scene", 0)]
  spins = read_spins(document, binary)
  # A spinning part is baked as its first key stands, which is how glTF draws it when its animation starts.
  for node_index, spin in spins.items():
    document["nodes"][node_index] = dict(document["nodes"][node_index], rotation=list(spin["rotation"]))

  vertices = []
  # The triangles that stand still, as indices into vertices; and each part, by its node's name, with its own triangles.
  fixed = []
  parts = []
  hardpoints = []

  def visit(node_index, node, parent, world, part):
    name = node.get("name", "")
    if "skin" in node:
      raise BakeError(f"node '{name}' has a skin, and the game draws rigid meshes only")
    if node_index in spins:
      if part is not None:
        raise BakeError(f"the spinning part '{name}' is in the spinning part '{part['name']}'")
      part = add_part(node_index, name, parent, world)
    if name.startswith("hp_"):
      if part is not None:
        raise BakeError(f"the hardpoint '{name}' is on the spinning part '{part['name']}'")
      add_hardpoint(node, name, world)
    if "mesh" in node:
      add_mesh(node, world, fixed if part is None else part["indices"])
    return part

  def add_part(node_index, name, parent, world):
    # The axis is in the parent's frame, which must not stretch it, or the turn would not be a rotation.
    axes = [length(transform_direction(parent, axis)) for axis in ((1.0, 0.0, 0.0), (0.0, 1.0, 0.0), (0.0, 0.0, 1.0))]
    if axes[0] <= 0.0 or any(abs(axis - axes[0]) > UNIFORM_SCALE_TOLERANCE * axes[0] for axis in axes):
      raise BakeError(f"the spinning part '{name}' is under a node that is not scaled uniformly")
    axis = normalized(transform_direction(parent, spins[node_index]["axis"]), f"the axis of the spinning part '{name}'")
    # A mirroring parent turns the spin the other way about its axis.
    if determinant3(parent) < 0.0:
      axis = tuple(-value for value in axis)
    # The change of frame is a mirror too, so the game's axis is the source's, changed to the game's frame and reversed.
    part = {"name": name, "indices": [], "pivot": to_game(transform_point(world, (0.0, 0.0, 0.0))),
            "axis": tuple(-value for value in to_game(axis)), "period": spins[node_index]["period"]}
    parts.append(part)
    return part

  def add_hardpoint(node, name, world):
    match = HARDPOINT_NAME.fullmatch(name)
    if match is None:
      raise BakeError(f"the hardpoint '{name}' is not named hp_<tag> with a tag of lowercase letters and digits")
    tag = match.group(1)
    if len(tag) > MAXIMUM_TAG_LENGTH:
      raise BakeError(f"the hardpoint '{name}' has a tag longer than {MAXIMUM_TAG_LENGTH} characters")
    if "mesh" in node:
      raise BakeError(f"the hardpoint '{name}' has a mesh; a hardpoint is an empty")
    axes = [length(transform_direction(world, axis)) for axis in ((1.0, 0.0, 0.0), (0.0, 1.0, 0.0), (0.0, 0.0, 1.0))]
    size = axes[0]
    if size <= 0.0 or any(abs(axis - size) > UNIFORM_SCALE_TOLERANCE * size for axis in axes):
      raise BakeError(f"the hardpoint '{name}' is not scaled uniformly")
    # Blender's +z is glTF's +y in the node's own frame, and Blender's +y is glTF's -z.
    position = transform_point(world, (0.0, 0.0, 0.0))
    forward = normalized(transform_direction(world, (0.0, 1.0, 0.0)), f"the hardpoint '{name}'")
    up = normalized(transform_direction(world, (0.0, 0.0, -1.0)), f"the hardpoint '{name}'")
    hardpoints.append((name, tag, to_game(position), to_game(forward), to_game(up), size))

  def add_mesh(node, world, indices):
    meshes = document.get("meshes", [])
    if not 0 <= node["mesh"] < len(meshes):
      raise BakeError(f"node '{node.get('name', '')}' names a mesh that does not exist")
    normals_by = normal_matrix(world)
    # A mirroring transform turns the source's own winding over; the change of frame below turns it over again.
    reverse = determinant3(world) > 0.0
    for primitive_index, primitive in enumerate(meshes[node["mesh"]].get("primitives", [])):
      where = f"mesh '{meshes[node['mesh']].get('name', node['mesh'])}' primitive {primitive_index}"
      if primitive.get("mode", TRIANGLES) != TRIANGLES:
        raise BakeError(f"{where} is not a triangle list")
      if primitive.get("targets"):
        raise BakeError(f"{where} has morph targets, and the game draws rigid meshes only")
      attributes = primitive.get("attributes", {})
      for required in ("POSITION", "NORMAL"):
        if required not in attributes:
          raise BakeError(f"{where} has no {required}")
      positions = read_accessor(document, binary, attributes["POSITION"], {FLOAT}, "VEC3")
      normals = read_accessor(document, binary, attributes["NORMAL"], {FLOAT}, "VEC3")
      if len(positions) != len(normals):
        raise BakeError(f"{where} has {len(positions)} positions and {len(normals)} normals")
      if "indices" in primitive:
        corners = [item[0] for item in read_accessor(document, binary, primitive["indices"], set(INDEX_COMPONENTS), "SCALAR")]
      else:
        corners = list(range(len(positions)))
      if len(corners) % 3 != 0:
        raise BakeError(f"{where} is not a whole number of triangles")
      first = len(vertices)
      for position, normal in zip(positions, normals):
        if not all(math.isfinite(value) for value in position + normal):
          raise BakeError(f"{where} has a vertex that is not a finite number")
        world_position = transform_point(world, position)
        world_normal = normalized(tuple(sum(normals_by[row][k] * normal[k] for k in range(3)) for row in range(3)),
                                  f"a normal of {where}")
        vertices.append(to_game(world_position) + to_game(world_normal))
      for triangle in range(0, len(corners), 3):
        a, b, c = corners[triangle:triangle + 3]
        if max(a, b, c) >= len(positions):
          raise BakeError(f"{where} has an index past its vertices")
        indices.extend((first + a, first + c, first + b) if reverse else (first + a, first + b, first + c))

  for root in scene.get("nodes", []):
    walk(document, root, identity(), 0, visit, None)

  if not fixed:
    raise BakeError("it has no triangles that stand still")
  for part in parts:
    if not part["indices"]:
      raise BakeError(f"the spinning part '{part['name']}' has no triangles")
  if len(vertices) >= 2**32:
    raise BakeError("it has more vertices than 32-bit indices reach")
  # By name, so the order is the one the outliner shows and does not move when the file is saved again.
  hardpoints.sort(key=lambda hardpoint: hardpoint[0])
  parts.sort(key=lambda part: part["name"])
  indices = list(fixed)
  for part in parts:
    part["first"] = len(indices)
    indices.extend(part["indices"])

  output = bytearray(NMF_HEADER.pack(NMF_MAGIC, NMF_VERSION, len(vertices), len(indices), len(hardpoints), len(parts), 0))
  for vertex in vertices:
    output += NMF_VERTEX.pack(*vertex)
  output += struct.pack(f"<{len(indices)}I", *indices)
  for _, tag, position, forward, up, size in hardpoints:
    encoded = tag.encode("ascii")
    output += struct.pack("<B", len(encoded)) + encoded
    output += NMF_HARDPOINT_FLOATS.pack(*position, *forward, *up, size)
  for part in parts:
    output += NMF_PART.pack(part["first"], len(part["indices"]), *part["pivot"], *part["axis"], part["period"])
  return bytes(output)


def read_nmf_extras(data):
  """The hardpoints and the parts of an .nmf's bytes, for --list and the self-test."""
  _, _, vertex_count, index_count, hardpoint_count, part_count, _ = NMF_HEADER.unpack_from(data, 0)
  offset = NMF_HEADER.size + vertex_count * NMF_VERTEX.size + index_count * 4
  hardpoints = []
  for _ in range(hardpoint_count):
    tag_length = data[offset]
    tag = data[offset + 1:offset + 1 + tag_length].decode("ascii")
    offset += 1 + tag_length
    values = NMF_HARDPOINT_FLOATS.unpack_from(data, offset)
    offset += NMF_HARDPOINT_FLOATS.size
    hardpoints.append((tag, values[0:3], values[3:6], values[6:9], values[9]))
  parts = []
  for _ in range(part_count):
    first, count, *values = NMF_PART.unpack_from(data, offset)
    offset += NMF_PART.size
    parts.append((first, count, tuple(values[0:3]), tuple(values[3:6]), values[6]))
  return hardpoints, parts


def read_nmf_hardpoints(data):
  return read_nmf_extras(data)[0]


# ── Writing a .glb: for the self-test's sources, and for the one-off migration from .cmo (ADR-018) ─────────────────


def write_glb(meshes, hardpoints=(), extra=None, animations=()):
  """A binary glTF of one node per mesh, each with its hardpoints as child nodes.

  Each mesh is a dict of name, positions, normals, indices and, optionally, texcoords, a node translation, rotation and
  scale, and a parent (an earlier mesh's position in the list) to be the child of. Each hardpoint is a dict of parent (a
  mesh's position in the list), name, translation, rotation (x, y, z, w) and scale. Each animation is a dict of node (a
  mesh's position in the list), times and values, and optionally path (rotation unless given) and interpolation. extra
  is merged into the document last, so a test can break it."""
  binary = bytearray()
  document = {"asset": {"version": "2.0", "generator": "Outpost Commander Tools/BakeMeshes.py"}, "scene": 0,
              "scenes": [{"nodes": []}], "nodes": [], "meshes": [], "accessors": [], "bufferViews": [], "buffers": []}

  def add_view(payload, target):
    while len(binary) % 4:
      binary.append(0)
    document["bufferViews"].append({"buffer": 0, "byteOffset": len(binary), "byteLength": len(payload), "target": target})
    binary.extend(payload)
    return len(document["bufferViews"]) - 1

  def add_vec(values, with_bounds):
    flat = [float(component) for value in values for component in value]
    width = len(values[0])
    view = add_view(struct.pack(f"<{len(flat)}f", *flat), 34962)
    accessor = {"bufferView": view, "componentType": FLOAT, "count": len(values), "type": f"VEC{width}"}
    if with_bounds:
      accessor["min"] = [min(value[axis] for value in values) for axis in range(width)]
      accessor["max"] = [max(value[axis] for value in values) for axis in range(width)]
    document["accessors"].append(accessor)
    return len(document["accessors"]) - 1

  mesh_nodes = []
  for mesh in meshes:
    attributes = {"POSITION": add_vec(mesh["positions"], True), "NORMAL": add_vec(mesh["normals"], False)}
    if mesh.get("texcoords"):
      attributes["TEXCOORD_0"] = add_vec(mesh["texcoords"], False)
    wide = max(mesh["indices"]) > 0xFFFF
    view = add_view(struct.pack(f"<{len(mesh['indices'])}{'I' if wide else 'H'}", *mesh["indices"]), 34963)
    document["accessors"].append({"bufferView": view, "componentType": 5125 if wide else 5123,
                                  "count": len(mesh["indices"]), "type": "SCALAR"})
    document["meshes"].append({"name": mesh["name"],
                               "primitives": [{"attributes": attributes, "indices": len(document["accessors"]) - 1}]})
    node = {"name": mesh["name"], "mesh": len(document["meshes"]) - 1}
    for key in ("translation", "rotation", "scale"):
      if key in mesh:
        node[key] = list(mesh[key])
    document["nodes"].append(node)
    if "parent" in mesh:
      mesh_nodes[mesh["parent"]].setdefault("children", []).append(len(document["nodes"]) - 1)
    else:
      document["scenes"][0]["nodes"].append(len(document["nodes"]) - 1)
    mesh_nodes.append(node)

  for hardpoint in hardpoints:
    node = {"name": hardpoint["name"]}
    for key in ("translation", "rotation", "scale"):
      if key in hardpoint:
        node[key] = [float(value) for value in hardpoint[key]]
    document["nodes"].append(node)
    mesh_nodes[hardpoint.get("parent", 0)].setdefault("children", []).append(len(document["nodes"]) - 1)

  for animation in animations:
    times = [float(time) for time in animation["times"]]
    view = add_view(struct.pack(f"<{len(times)}f", *times), 34962)
    document["accessors"].append({"bufferView": view, "componentType": FLOAT, "count": len(times), "type": "SCALAR",
                                  "min": [min(times)], "max": [max(times)]})
    sampler = {"input": len(document["accessors"]) - 1, "output": add_vec(animation["values"], False),
               "interpolation": animation.get("interpolation", "LINEAR")}
    target = {"node": animation["node"], "path": animation.get("path", "rotation")}
    document.setdefault("animations", []).append({"channels": [{"sampler": 0, "target": target}], "samplers": [sampler]})

  while len(binary) % 4:
    binary.append(0)
  document["buffers"].append({"byteLength": len(binary)})
  if extra:
    document.update(extra)
  text = json.dumps(document, separators=(",", ":")).encode("utf-8")
  text += b" " * (-len(text) % 4)
  total = 12 + 8 + len(text) + 8 + len(binary)
  return (struct.pack("<4sII", GLB_MAGIC, GLB_VERSION, total) + struct.pack("<II", len(text), GLB_JSON_CHUNK) + text +
          struct.pack("<II", len(binary), GLB_BIN_CHUNK) + bytes(binary))


# ── The commands ────────────────────────────────────────────────────────────────────────────────────────────────────


def sources():
  return sorted(SOURCE_DIRECTORY.glob("*/*.glb"))


def output_for(source):
  return OUTPUT_DIRECTORY / source.parent.name / (source.stem + ".nmf")


def bake_file(source):
  try:
    return bake(source.read_bytes())
  except (BakeError, KeyError, IndexError, TypeError, ValueError, struct.error) as error:
    raise BakeError(f"{source.relative_to(ROOT).as_posix()}: {error}") from None


def bake_all():
  failures = 0
  for source in sources():
    try:
      baked = bake_file(source)
    except BakeError as error:
      print(error)
      failures += 1
      continue
    target = output_for(source)
    target.parent.mkdir(parents=True, exist_ok=True)
    if not target.is_file() or target.read_bytes() != baked:
      target.write_bytes(baked)
      print(f"baked {target.relative_to(ROOT).as_posix()}")
  return 1 if failures else 0


def check_all():
  findings = []
  expected = set()
  for source in sources():
    target = output_for(source)
    expected.add(target)
    try:
      baked = bake_file(source)
    except BakeError as error:
      findings.append(str(error))
      continue
    if not target.is_file():
      findings.append(f"{target.relative_to(ROOT).as_posix()}: missing; run python Tools/BakeMeshes.py")
    elif target.read_bytes() != baked:
      findings.append(f"{target.relative_to(ROOT).as_posix()}: not what {source.relative_to(ROOT).as_posix()} bakes to; "
                      "run python Tools/BakeMeshes.py")
  for target in sorted(OUTPUT_DIRECTORY.glob("*/*.nmf")):
    if target not in expected:
      findings.append(f"{target.relative_to(ROOT).as_posix()}: has no source in Art/Models")
  for finding in findings:
    print(finding)
  print(f"Meshes: {len(expected)} sources, " + ("all baked and current." if not findings else f"{len(findings)} findings."))
  return 1 if findings else 0


def list_hardpoints(path):
  hardpoints, parts = read_nmf_extras(bake_file(Path(path)))
  for tag, position, forward, up, size in hardpoints:
    print(f"{tag:10s} at ({position[0]:9.3f}, {position[1]:9.3f}, {position[2]:9.3f})  forward "
          f"({forward[0]:6.3f}, {forward[1]:6.3f}, {forward[2]:6.3f})  up ({up[0]:6.3f}, {up[1]:6.3f}, {up[2]:6.3f})  "
          f"size {size:.3f}")
  for first, count, pivot, axis, period in parts:
    print(f"part       at ({pivot[0]:9.3f}, {pivot[1]:9.3f}, {pivot[2]:9.3f})  axis "
          f"({axis[0]:6.3f}, {axis[1]:6.3f}, {axis[2]:6.3f})  a turn in {period:.3f} s  {count // 3} triangles from index {first}")
  return 0


def self_test():
  """Bakes sources built here: what a good one becomes, and that every refusal fires."""
  failures = []

  def expect(condition, message):
    if not condition:
      failures.append(message)

  # One triangle whose front faces glTF +y, and a hardpoint 2 m ahead of the model's origin, its arrow pointing back.
  triangle = {"name": "Triangle", "positions": [(0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (0.0, 0.0, 2.0)],
              "normals": [(0.0, 1.0, 0.0)] * 3, "indices": [0, 2, 1]}
  # A quarter turn about x takes the empty's +y (Blender's +z, its arrow) to glTF's -z, which is backward.
  quarter = math.sqrt(0.5)
  exhaust = {"name": "hp_exhaust.001", "translation": (0.0, 0.0, 2.0), "rotation": (-quarter, 0.0, 0.0, quarter),
             "scale": (0.5, 0.5, 0.5)}
  good = write_glb([triangle], [exhaust, {"name": "hp_gun", "translation": (0.0, 1.0, 0.0)}])
  baked = bake(good)
  expect(bake(good) == baked, "baking the same source twice gave different bytes")
  expect(NMF_HEADER.unpack_from(baked, 0) == (NMF_MAGIC, 2, 3, 3, 2, 0, 0),
         "the header of a one-triangle source with two hardpoints is wrong")
  vertices = [NMF_VERTEX.unpack_from(baked, NMF_HEADER.size + i * NMF_VERTEX.size) for i in range(3)]
  expect([vertex[:3] for vertex in vertices] == [(0.0, 0.0, 0.0), (0.0, 0.0, 1.0), (2.0, 0.0, 0.0)],
         "a point (x, y, z) did not become (z, y, x)")
  expect(all(vertex[3:] == (0.0, 1.0, 0.0) for vertex in vertices), "the normals did not keep pointing up")
  corners = struct.unpack_from("<3I", baked, NMF_HEADER.size + 3 * NMF_VERTEX.size)
  expect(corners == (0, 1, 2), "a triangle's corners were not reversed")
  # Clockwise seen from above in the game's left-handed frame: cross(b - a, c - a) points along the normal.
  a, b, c = (vertices[corner][:3] for corner in corners)
  edge1 = tuple(b[i] - a[i] for i in range(3))
  edge2 = tuple(c[i] - a[i] for i in range(3))
  expect(edge1[2] * edge2[0] - edge1[0] * edge2[2] > 0.0, "the triangle does not wind clockwise seen from its front")
  hardpoints = read_nmf_hardpoints(baked)
  expect([hardpoint[0] for hardpoint in hardpoints] == ["exhaust", "gun"], "the hardpoints are not tagged and in name order")
  tag, position, forward, up, size = hardpoints[0]
  expect(position == (2.0, 0.0, 0.0), "the exhaust is not 2 m ahead along +x")
  expect(all(abs(value - wanted) < 1e-6 for value, wanted in zip(forward, (-1.0, 0.0, 0.0))),
         "the exhaust's arrow does not point back along -x")
  # The same turn takes the empty's up, Blender's +y, from back to down.
  expect(all(abs(value - wanted) < 1e-6 for value, wanted in zip(up, (0.0, -1.0, 0.0))), "the exhaust's up is not -y")
  expect(abs(size - 0.5) < 1e-7, "the exhaust's size is not its scale")
  gun = hardpoints[1]
  expect(gun[2] == (0.0, 1.0, 0.0) and gun[3] == (-1.0, 0.0, 0.0),
         "an unturned empty's arrow is not up, or its up is not back")

  # A parented hardpoint and a moved mesh are baked in world space.
  moved = dict(triangle, translation=(1.0, 2.0, 3.0))
  placed = read_nmf_hardpoints(bake(write_glb([moved], [{"name": "hp_gun", "translation": (0.0, 0.0, 1.0)}])))
  expect(placed[0][1] == (4.0, 2.0, 1.0), "a hardpoint under a moved model is not where the two transforms put it")
  # A mirrored model keeps its front faces: the mirror and the change of frame both turn the winding over.
  mirrored = bake(write_glb([dict(triangle, scale=(-1.0, 1.0, 1.0))]))
  mirrored_corners = struct.unpack_from("<3I", mirrored, NMF_HEADER.size + 3 * NMF_VERTEX.size)
  expect(mirrored_corners == (0, 2, 1), "a mirrored model's triangles were reversed twice over")

  # A spinning part: a triangle 4 m ahead of the model's origin, off its pivot, turned a whole turn in 8 s about glTF's +y
  # by keys a quarter turn apart, as Blender writes a spin.
  def spin_keys(axis, angles):
    return [tuple(value * math.sin(angle / 2.0) for value in axis) + (math.cos(angle / 2.0),) for angle in angles]

  quarters = [0.0, math.pi / 2.0, math.pi, 1.5 * math.pi, 2.0 * math.pi]
  radar = dict(triangle, name="Radar", positions=[(1.0, 0.0, 0.0), (2.0, 0.0, 0.0), (1.0, 0.0, 1.0)], translation=(0.0, 0.0, 4.0))
  spin = {"node": 1, "times": [0.0, 2.0, 4.0, 6.0, 8.0], "values": spin_keys((0.0, 1.0, 0.0), quarters)}
  spinning = bake(write_glb([triangle, radar], animations=[spin]))
  expect(NMF_HEADER.unpack_from(spinning, 0) == (NMF_MAGIC, 2, 6, 6, 0, 1, 0),
         "the header of a source with a spinning part is wrong")
  _, parts = read_nmf_extras(spinning)
  first, count, pivot, axis, period = parts[0]
  expect((first, count) == (3, 3), "the spinning part's triangles do not follow the fixed ones")
  expect(pivot == (4.0, 0.0, 0.0) and period == 8.0, "the spinning part's pivot is not its node's origin, or its period not 8 s")
  # Where the game puts each of the part's corners a quarter turn in is where glTF's animation puts it.
  for corner in range(3):
    point = NMF_VERTEX.unpack_from(spinning, NMF_HEADER.size + (3 + corner) * NMF_VERTEX.size)[:3]
    offset = tuple(point[i] - pivot[i] for i in range(3))
    across = (axis[1] * offset[2] - axis[2] * offset[1], axis[2] * offset[0] - axis[0] * offset[2],
              axis[0] * offset[1] - axis[1] * offset[0])
    # XMMatrixRotationAxis(axis, a quarter turn), as Rodrigues' formula writes it.
    game = tuple(pivot[i] + across[i] + axis[i] * dot(axis, offset) for i in range(3))
    keyed = node_matrix(dict(radar, rotation=spin["values"][1]))
    source = to_game(transform_point(keyed, radar["positions"][corner]))
    expect(all(abs(game[i] - source[i]) < 1e-5 for i in range(3)),
           f"the game turns corner {corner} of the spinning part away from where the animation turns it")
  spin_refusals = {
    "an animated translation": [{"node": 1, "path": "translation", "times": [0.0, 1.0], "values": [(0.0, 0.0, 0.0), (1.0, 0.0, 0.0)]}],
    "a spin with step keys": [dict(spin, interpolation="STEP")],
    "a spin that stops short of a whole turn": [dict(spin, times=spin["times"][:4], values=spin["values"][:4])],
    "a spin at two rates": [dict(spin, times=[0.0, 1.0, 4.0, 6.0, 8.0])],
    "a spin about two axes": [dict(spin, values=spin["values"][:2] + spin_keys((1.0, 0.0, 0.0), quarters[2:]))],
    "a spin with keys half a turn apart": [dict(spin, times=[0.0, 4.0, 8.0], values=spin_keys((0.0, 1.0, 0.0), quarters[::2]))],
  }
  for what, animations in spin_refusals.items():
    try:
      bake(write_glb([triangle, radar], animations=animations))
    except BakeError:
      continue
    failures.append(f"{what} was baked rather than refused")
  structure_refusals = {
    "a hardpoint on a spinning part": write_glb([triangle, radar], [{"parent": 1, "name": "hp_gun"}], animations=[spin]),
    "a spinning part in a spinning part": write_glb([triangle, radar, dict(radar, name="Dish", parent=1)],
                                                    animations=[spin, dict(spin, node=2)]),
    "a source whose every triangle spins": write_glb([radar], animations=[dict(spin, node=0)]),
  }
  for what, data in structure_refusals.items():
    try:
      bake(data)
    except BakeError:
      continue
    failures.append(f"{what} was baked rather than refused")

  refusals = {
    "a Draco-compressed source": write_glb([triangle], extra={"extensionsRequired": ["KHR_draco_mesh_compression"]}),
    "a hardpoint with a capital in its tag": write_glb([triangle], [{"name": "hp_Gun"}]),
    "a hardpoint with no tag": write_glb([triangle], [{"name": "hp_"}]),
    "a hardpoint scaled unevenly": write_glb([triangle], [{"name": "hp_gun", "scale": (1.0, 2.0, 1.0)}]),
    "a hardpoint scaled to nothing": write_glb([triangle], [{"name": "hp_gun", "scale": (0.0, 0.0, 0.0)}]),
    "a mesh with a vertex that is not a number": write_glb([dict(triangle, positions=[(math.nan, 0.0, 0.0)] + triangle["positions"][1:])]),
    "an index past the vertices": write_glb([dict(triangle, indices=[0, 1, 3])]),
    "a primitive that is not a whole number of triangles": write_glb([dict(triangle, indices=[0, 1, 2, 0])]),
    "a file cut short": good[:-4],
    "a file that is not a glTF": b"NMF\0" + good[4:],
  }

  def broken(change):
    document, binary = read_glb(good)
    change(document)
    text = json.dumps(document).encode("utf-8")
    text += b" " * (-len(text) % 4)
    total = 12 + 8 + len(text) + 8 + len(binary)
    return (struct.pack("<4sII", GLB_MAGIC, GLB_VERSION, total) + struct.pack("<II", len(text), GLB_JSON_CHUNK) + text +
            struct.pack("<II", len(binary), GLB_BIN_CHUNK) + binary)

  refusals["a line list"] = broken(lambda document: document["meshes"][0]["primitives"][0].update(mode=1))
  refusals["a mesh without normals"] = broken(lambda document: document["meshes"][0]["primitives"][0]["attributes"].pop("NORMAL"))
  refusals["a sparse accessor"] = broken(lambda document: document["accessors"][0].update(sparse={"count": 0}))
  refusals["a skinned node"] = broken(lambda document: document["nodes"][0].update(skin=0))
  refusals["morph targets"] = broken(lambda document: document["meshes"][0]["primitives"][0].update(targets=[{"POSITION": 0}]))
  refusals["a buffer outside the file"] = broken(lambda document: document["buffers"][0].update(uri="mesh.bin"))
  refusals["a mesh with no triangles"] = broken(lambda document: document["accessors"][-1].update(count=0))
  for what, data in refusals.items():
    try:
      bake(data)
    except (BakeError, KeyError, IndexError, TypeError, ValueError, struct.error):
      continue
    failures.append(f"{what} was baked rather than refused")

  for failure in failures:
    print(f"self-test: {failure}")
  print("BakeMeshes self-test: " + ("passed." if not failures else f"{len(failures)} failures."))
  return 1 if failures else 0


def main():
  parser = argparse.ArgumentParser(description="Bake the meshes' glTF sources into the NMF files the game loads.")
  group = parser.add_mutually_exclusive_group()
  group.add_argument("--check", action="store_true", help="fail when an .nmf is not what its source bakes to")
  group.add_argument("--list", metavar="GLB", help="print a source's hardpoints in the game's frame")
  group.add_argument("--self-test", action="store_true", help="check the baker against sources built here")
  arguments = parser.parse_args()
  try:
    if arguments.self_test:
      return self_test()
    if arguments.check:
      return check_all()
    if arguments.list:
      return list_hardpoints(arguments.list)
    return bake_all()
  except BakeError as error:
    print(error)
    return 1


if __name__ == "__main__":
  sys.exit(main())

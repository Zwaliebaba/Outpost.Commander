#!/usr/bin/env python3
"""Bakes the meshes' glTF sources into the NMF files the game loads (ADR-017).

Each model's source is a binary glTF, Art/Models/<Set>/<Model>.glb, edited with Blender's own glTF import and export.
This turns each into OutpostCommander/Assets/Models/<Set>/<Model>.nmf: one triangle list of positions and normals, and
the model's hardpoints, in the game's frame. The .nmf files are committed, and CI runs --check to prove each one is
exactly what its source bakes to. CPython's floats are IEEE doubles with no fused operations, so a bake is the same
bytes on every machine.

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

What a source may hold is narrow, and anything else is refused with the file's name rather than baked into something
the game would draw wrongly: triangles only, a position and a normal on every vertex, no skin, morph target or
animation, no extension the file requires (Draco or quantized meshes, say), and no sparse accessor. Materials, texture
coordinates, cameras and lights are left in the source and not baked.

The NMF layout, little-endian, version 1:

  char magic[4] = "NMF\\0"; u32 version; u32 vertexCount; u32 indexCount; u32 hardpointCount; u32 flags = 0
  vertexCount    x { f32 position[3]; f32 normal[3]; }
  indexCount     x u32, a multiple of 3, each below vertexCount
  hardpointCount x { u8 tagLength; char tag[tagLength]; f32 position[3]; f32 forward[3]; f32 up[3]; f32 size; }

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
NMF_VERSION = 1
NMF_HEADER = struct.Struct("<4s5I")
NMF_VERTEX = struct.Struct("<6f")
NMF_HARDPOINT_FLOATS = struct.Struct("<10f")
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
  width = {"SCALAR": 1, "VEC3": 3}[item_type]
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


def walk(document, node_index, parent, depth, visit):
  if depth > len(document.get("nodes", [])):
    raise BakeError("its nodes form a cycle")
  node = document["nodes"][node_index]
  world = multiply(parent, node_matrix(node))
  visit(node, world)
  for child in node.get("children", []):
    walk(document, child, world, depth + 1, visit)


def bake(data):
  """The .nmf bytes of a .glb's bytes."""
  document, binary = read_glb(data)
  if document.get("asset", {}).get("version") != "2.0":
    raise BakeError("it is not glTF 2.0")
  if document.get("extensionsRequired"):
    raise BakeError(f"it requires the extensions {', '.join(document['extensionsRequired'])}")
  if document.get("animations"):
    raise BakeError("it has animations, and the game draws rigid meshes only")
  scenes = document.get("scenes", [])
  if not scenes:
    raise BakeError("it has no scene")
  scene = scenes[document.get("scene", 0)]

  vertices = []
  indices = []
  hardpoints = []

  def visit(node, world):
    name = node.get("name", "")
    if "skin" in node:
      raise BakeError(f"node '{name}' has a skin, and the game draws rigid meshes only")
    if name.startswith("hp_"):
      add_hardpoint(node, name, world)
    if "mesh" in node:
      add_mesh(node, world)

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

  def add_mesh(node, world):
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
    walk(document, root, identity(), 0, visit)

  if not indices:
    raise BakeError("it has no triangles")
  if len(vertices) >= 2**32:
    raise BakeError("it has more vertices than 32-bit indices reach")
  # By name, so the order is the one the outliner shows and does not move when the file is saved again.
  hardpoints.sort(key=lambda hardpoint: hardpoint[0])

  output = bytearray(NMF_HEADER.pack(NMF_MAGIC, NMF_VERSION, len(vertices), len(indices), len(hardpoints), 0))
  for vertex in vertices:
    output += NMF_VERTEX.pack(*vertex)
  output += struct.pack(f"<{len(indices)}I", *indices)
  for _, tag, position, forward, up, size in hardpoints:
    encoded = tag.encode("ascii")
    output += struct.pack("<B", len(encoded)) + encoded
    output += NMF_HARDPOINT_FLOATS.pack(*position, *forward, *up, size)
  return bytes(output)


def read_nmf_hardpoints(data):
  """The hardpoints of an .nmf's bytes, for --list and the self-test."""
  _, _, vertex_count, index_count, hardpoint_count, _ = NMF_HEADER.unpack_from(data, 0)
  offset = NMF_HEADER.size + vertex_count * NMF_VERTEX.size + index_count * 4
  hardpoints = []
  for _ in range(hardpoint_count):
    tag_length = data[offset]
    tag = data[offset + 1:offset + 1 + tag_length].decode("ascii")
    offset += 1 + tag_length
    values = NMF_HARDPOINT_FLOATS.unpack_from(data, offset)
    offset += NMF_HARDPOINT_FLOATS.size
    hardpoints.append((tag, values[0:3], values[3:6], values[6:9], values[9]))
  return hardpoints


# ── Writing a .glb: for the self-test's sources, and for the one-off migration from .cmo (ADR-017) ─────────────────


def write_glb(meshes, hardpoints=(), extra=None):
  """A binary glTF of one node per mesh, each with its hardpoints as child nodes.

  Each mesh is a dict of name, positions, normals, indices and, optionally, texcoords and a node translation and
  rotation. Each hardpoint is a dict of parent (a mesh's position in the list), name, translation, rotation (x, y, z, w)
  and scale. extra is merged into the document last, so a test can break it."""
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
    document["scene"] = 0
    document["scenes"][0]["nodes"].append(len(document["nodes"]) - 1)
    mesh_nodes.append(node)

  for hardpoint in hardpoints:
    node = {"name": hardpoint["name"]}
    for key in ("translation", "rotation", "scale"):
      if key in hardpoint:
        node[key] = [float(value) for value in hardpoint[key]]
    document["nodes"].append(node)
    mesh_nodes[hardpoint.get("parent", 0)].setdefault("children", []).append(len(document["nodes"]) - 1)

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
  for tag, position, forward, up, size in read_nmf_hardpoints(bake_file(Path(path))):
    print(f"{tag:10s} at ({position[0]:9.3f}, {position[1]:9.3f}, {position[2]:9.3f})  forward "
          f"({forward[0]:6.3f}, {forward[1]:6.3f}, {forward[2]:6.3f})  up ({up[0]:6.3f}, {up[1]:6.3f}, {up[2]:6.3f})  "
          f"size {size:.3f}")
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
  magic, version, vertex_count, index_count, hardpoint_count, flags = NMF_HEADER.unpack_from(baked, 0)
  expect((magic, version, vertex_count, index_count, hardpoint_count, flags) == (NMF_MAGIC, 1, 3, 3, 2, 0),
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

  refusals = {
    "a Draco-compressed source": write_glb([triangle], extra={"extensionsRequired": ["KHR_draco_mesh_compression"]}),
    "an animated source": write_glb([triangle], extra={"animations": [{"channels": [], "samplers": []}]}),
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

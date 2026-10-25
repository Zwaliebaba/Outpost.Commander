"""A software renderer for Outpost Commander's baked .nmf meshes, in an approximation of the game's look.

Agent A6 of the design-reference build. Read-only on the repository; it writes only under DesignReference/_work/.

What it replicates, and from where (HEAD a053fe2):
  * NMF v3 reading: Tools/BakeMeshes.py header, NeuronClient/MeshData.cpp ParseNmf.
  * Fitting: Neuron::FitMesh / FitMeshAcross (MeshData.cpp), Outpost::BuildModelMesh (GameApp/ModelCatalog.cpp:372-386).
  * Crease lines: Neuron::BuildCreaseLines (NeuronClient/MeshData.cpp:289-), CREASE_DEGREES = 10 (GameApp/GameClient.cpp:49),
    built per piece (fixed triangles, then each spinning part; GameClient.cpp:374-389).
  * Face light: MeshPS.hlsl LightAt/main; constants GameApp/SceneShading.h:15-23 (TOWARD_KEY, KEY_COLOR, KEY_WRAP,
    SKY_FILL, GROUND_FILL, OCCLUSION_STRENGTH).
  * Line light: MeshLinePS.hlsl (luminance of LightAt, no occlusion); the pull toward the eye, MeshLineVS.hlsl and
    LINE_LIFT_SHARE (GameClient.cpp:53), stood in for here by a depth bias of LINE_DEPTH_PIXELS output pixels (ADR-040 says
    the share is "about 2.5 pixels of depth").
  * Face / line colors per kind: GameClient.cpp QueueModel / QueueStructure (1522-1578), SceneShading.h:45-59 and
    SceneShading.cpp (Shaded, EdgeColor, TowardGray).
  * Glow: ADR-091 / NeuronClient/Shader/Bloom*PS.hlsl / ResolvePS.hlsl with SCENE_BLOOM (GameClient.cpp:23), replayed on
    the output image.
  * sRGB encode of linear color (R8G8B8A8_UNORM_SRGB scene target, ADR-040 decision 3).
"""

import math
import struct
from pathlib import Path

import numpy as np

# ── The game's numbers (sources in the module docstring) ─────────────────────────────────────────────────────────────
TOWARD_KEY = np.array([-0.4, 0.8, 0.45])
KEY_COLOR = np.array([0.627, 0.539, 0.427])
KEY_WRAP = 0.3
SKY_FILL = np.array([0.365, 0.421, 0.571])
GROUND_FILL = np.array([0.314, 0.287, 0.389])
OCCLUSION_STRENGTH = 1.0
LUMINANCE = np.array([0.2126, 0.7152, 0.0722])

EDGE_BRIGHTNESS = 2.0
EDGE_WHITE_SHARE = 0.35
ROCK_EDGE_BRIGHTNESS = 1.35
FILL_SHADE = 0.3
ROCK_FILL_SHADE = 0.15
FIELD_SHADE = 0.7
STRUCTURE_GRAY_SHARE = 0.35
STRUCTURE_FILL_SHADE = 0.22

CREASE_DEGREES = 10.0

# SCENE_BLOOM, GameClient.cpp:23; five levels, Bloom.cpp LEVEL_COUNT (ADR-091 decision 1).
BLOOM_THRESHOLD = 0.35
BLOOM_KNEE = 0.15
BLOOM_INTENSITY = 2.0
BLOOM_SPREAD = 0.6
BLOOM_LEVELS = 5

# Camera.json: the default view width 500 m between 150 and 3000 m, pitch 40 to 70 degrees (Camera.cpp:160-165).
DEFAULT_PITCH_DEGREES = 40.0 + (500.0 - 150.0) / (3000.0 - 150.0) * (70.0 - 40.0)

# Renderer choices of this tool, not the game's.
LINE_DEPTH_PIXELS = 2.5   # ADR-040: the pull is "about 2.5 pixels of depth"
LINE_WIDTH_PIXELS = 2.0   # the game draws 1 px lines at any zoom (ADR-027 decision 3); 2 px at 1024 here so they read
SUPERSAMPLE = 2           # 2x2 samples a pixel, averaged in linear color (the game: 4x MSAA, ADR-040 decision 3)
FRAME_FILL = 0.84         # the model's projected extent fills this share of the image's side


# ── Small color helpers, as SceneShading.cpp has them ────────────────────────────────────────────────────────────────

def shaded(color, scale):
  return np.minimum(1.0, np.asarray(color) * scale)


def edge_color(color, brightness, white_share):
  color = np.asarray(color, dtype=float)
  brightest = color.max()
  if brightest <= 0.0:
    return color
  gain = min(brightness, 1.0 / brightest)
  peak = brightest * gain
  return color * gain + (peak - color * gain) * white_share


def toward_gray(color, share):
  color = np.asarray(color, dtype=float)
  lightness = float(np.dot(color, LUMINANCE))
  return color + (lightness - color) * share


def model_color(set_color, tint, built_permille=1000):
  """GameClient.cpp:344-349 ModelColor: the set's color times its tint, darker while built (UNBUILT_SHADE 0.35)."""
  built = built_permille / 1000.0
  shade = tint * (0.35 + (1.0 - 0.35) * built)
  return np.minimum(1.0, np.asarray(set_color) * shade)


def colors_for(kind, set_color, tint=1.0):
  """(face color, line color, line brightness rule) for a kind of model, as GameClient.cpp draws it."""
  set_color = np.asarray(set_color, dtype=float)
  if kind == "rock":
    # QueueModel: Shaded(color, ROCK_FILL_SHADE) faces, EdgeColor(color, ROCK_EDGE_BRIGHTNESS, 0) lines (an ore asteroid).
    return shaded(set_color, ROCK_FILL_SHADE), edge_color(set_color, ROCK_EDGE_BRIGHTNESS, 0.0)
  if kind == "structure":
    # QueueStructure: TowardGray(ModelColor(set, tint, built), STRUCTURE_GRAY_SHARE), faces at STRUCTURE_FILL_SHADE.
    color = toward_gray(model_color(set_color, tint), STRUCTURE_GRAY_SHARE)
    return shaded(color, STRUCTURE_FILL_SHADE), edge_color(color, EDGE_BRIGHTNESS, EDGE_WHITE_SHARE)
  # A ship (hull or Constructor) or a derelict: the set's color, faces at FILL_SHADE.
  return shaded(set_color, FILL_SHADE), edge_color(set_color, EDGE_BRIGHTNESS, EDGE_WHITE_SHARE)


def light_at(normals):
  """MeshPS.hlsl LightAt for unit normals (N,3) -> (N,3)."""
  to_light = TOWARD_KEY / np.linalg.norm(TOWARD_KEY)
  up = normals[:, 1:2] * 0.5 + 0.5
  fill = GROUND_FILL[None, :] + (SKY_FILL - GROUND_FILL)[None, :] * up
  key = np.clip((normals @ to_light + KEY_WRAP) / (1.0 + KEY_WRAP), 0.0, 1.0)[:, None]
  return fill + KEY_COLOR[None, :] * key


def srgb_encode(linear):
  linear = np.clip(linear, 0.0, 1.0)
  return np.where(linear <= 0.0031308, linear * 12.92, 1.055 * np.power(linear, 1.0 / 2.4) - 0.055)


# ── NMF ─────────────────────────────────────────────────────────────────────────────────────────────────────────────

def parse_nmf(path):
  data = Path(path).read_bytes()
  magic, version, vertex_count, index_count, hardpoint_count, part_count, flags = struct.unpack_from("<4s6I", data, 0)
  if magic != b"NMF\0" or version != 3 or flags != 0:
    raise ValueError(f"{path}: not an NMF v3 file")
  offset = 28
  vertex_dtype = np.dtype([("position", "<f4", 3), ("normal", "<f4", 3), ("open", "u1"), ("zero", "u1", 3)])
  vertices = np.frombuffer(data, dtype=vertex_dtype, count=vertex_count, offset=offset)
  offset += vertex_count * 28
  indices = np.frombuffer(data, dtype="<u4", count=index_count, offset=offset).astype(np.int64)
  offset += index_count * 4
  hardpoints = []
  for _ in range(hardpoint_count):
    tag_length = data[offset]
    tag = data[offset + 1:offset + 1 + tag_length].decode("ascii")
    offset += 1 + tag_length
    values = struct.unpack_from("<10f", data, offset)
    offset += 40
    hardpoints.append({"tag": tag, "position": values[0:3], "forward": values[3:6], "up": values[6:9], "size": values[9]})
  parts = []
  for _ in range(part_count):
    first, count, *values = struct.unpack_from("<2I7f", data, offset)
    offset += 36
    parts.append({"firstIndex": first, "indexCount": count, "pivot": tuple(values[0:3]), "axis": tuple(values[3:6]),
                  "periodSeconds": values[6]})
  if offset != len(data):
    raise ValueError(f"{path}: {len(data) - offset} bytes left over")
  if np.any(vertices["zero"] != 0):
    raise ValueError(f"{path}: nonzero vertex padding")
  return {
    "position": vertices["position"].astype(np.float32).copy(),
    "normal": vertices["normal"].astype(np.float32).copy(),
    "open": vertices["open"].astype(np.float32) / 255.0,
    "indices": indices,
    "hardpoints": hardpoints,
    "parts": parts,
  }


def bounds(position):
  return position.min(axis=0), position.max(axis=0)


def fit_scale(mesh, length_meters, first_level_widest_meters=None):
  """BuildModelMesh: FitMesh to lengthMeters along x, or a later level FitMeshAcross the first level's widest."""
  low, high = bounds(mesh["position"])
  extents = high - low
  if first_level_widest_meters is not None:
    return float(first_level_widest_meters / max(extents[0], extents[2]))
  return float(length_meters / extents[0])


def pieces_of(mesh):
  """The fixed triangles and each part's, as index arrays (GameClient.cpp:384-389, Neuron::MeshPiece)."""
  indices = mesh["indices"]
  if not mesh["parts"]:
    return [indices]
  fixed = mesh["parts"][0]["firstIndex"]
  result = [indices[:fixed]]
  for part in mesh["parts"]:
    result.append(indices[part["firstIndex"]:part["firstIndex"] + part["indexCount"]])
  return result


def crease_lines(position, indices, min_angle_radians):
  """Neuron::BuildCreaseLines: edges whose two faces fold by more than the angle, and edges with one face or more than
  two; corners welded by exact position; each line's normal the mean of its faces' normals. Returns (from, to, normal)
  as arrays of corner positions (L,3), (L,3) and unit normals (L,3)."""
  used = np.unique(indices)
  pos = position[used].astype(np.float32) + np.float32(0.0)  # -0 and 0 are one corner
  remap = np.full(position.shape[0], -1, dtype=np.int64)
  remap[used] = np.arange(used.size)
  keys = np.ascontiguousarray(pos).view(np.dtype((np.void, 12))).ravel()
  unique_keys, inverse = np.unique(keys, return_inverse=True)
  corner_of = inverse.ravel()[remap[indices]]
  corner_position = np.zeros((unique_keys.size, 3), dtype=np.float32)
  corner_position[inverse.ravel()] = pos
  triangles = corner_of.reshape(-1, 3)
  a = corner_position[triangles[:, 0]]
  b = corner_position[triangles[:, 1]]
  c = corner_position[triangles[:, 2]]
  face = np.cross(b - a, c - a).astype(np.float32)
  length_squared = (face * face).sum(axis=1)
  keep = length_squared > 0.0
  triangles = triangles[keep]
  normals = face[keep] / np.sqrt(length_squared[keep])[:, None]
  sides_from = np.concatenate([triangles[:, 0], triangles[:, 1], triangles[:, 2]])
  sides_to = np.concatenate([triangles[:, 1], triangles[:, 2], triangles[:, 0]])
  side_normal = np.concatenate([normals, normals, normals])
  low = np.minimum(sides_from, sides_to)
  high = np.maximum(sides_from, sides_to)
  order = np.lexsort((high, low))
  low, high, side_normal = low[order], high[order], side_normal[order]
  edge_key = low * (unique_keys.size + 1) + high
  starts = np.flatnonzero(np.r_[True, edge_key[1:] != edge_key[:-1]])
  counts = np.diff(np.r_[starts, edge_key.size])
  flat_cosine = math.cos(min_angle_radians)
  out_from, out_to, out_normal = [], [], []
  sums = np.add.reduceat(side_normal, starts, axis=0)
  for edge, (start, count) in enumerate(zip(starts, counts)):
    if count == 2:
      cosine = float(np.dot(side_normal[start], side_normal[start + 1]))
      if cosine >= flat_cosine:
        continue
    total = sums[edge]
    if float(np.dot(total, total)) > 1e-6:
      mean = total / np.linalg.norm(total)
    else:
      mean = side_normal[start]
    out_from.append(corner_position[low[start]])
    out_to.append(corner_position[high[start]])
    out_normal.append(mean)
  if not out_from:
    return np.zeros((0, 3)), np.zeros((0, 3)), np.zeros((0, 3))
  return np.array(out_from, dtype=float), np.array(out_to, dtype=float), np.array(out_normal, dtype=float)


# ── The view ────────────────────────────────────────────────────────────────────────────────────────────────────────

def heading_rotation(heading_radians):
  """WorldMatrix / PoseMatrix: XMMatrixRotationY(-heading), so +x turns toward +z (counterclockwise seen from above)."""
  c, s = math.cos(heading_radians), math.sin(heading_radians)
  # Row vectors times this matrix, as DirectXMath does: (1,0,0) -> (c, 0, s).
  return np.array([[c, 0.0, s], [0.0, 1.0, 0.0], [-s, 0.0, c]])


def view_axes(view):
  """The camera's right, up and forward in the world, as XMMatrixLookAtLH builds them (Camera.cpp:176-183)."""
  if view == "top":
    return np.array([1.0, 0.0, 0.0]), np.array([0.0, 0.0, 1.0]), np.array([0.0, -1.0, 0.0])
  pitch = math.radians(DEFAULT_PITCH_DEGREES)
  # The default yaw looks along +z (Camera.cpp:13-14): screen right +x, screen top +z.
  forward = np.array([0.0, -math.sin(pitch), math.cos(pitch)])
  right = np.array([1.0, 0.0, 0.0])
  up = np.cross(forward, right)
  return right, up, forward


VIEW_HEADINGS = {"34": math.radians(225.0), "top": math.radians(90.0)}


# ── Rasterizing ─────────────────────────────────────────────────────────────────────────────────────────────────────

def raster_faces(sx, sy, depth, triangles, openness, size):
  """Z-buffered triangles: per pixel the nearest triangle's id and interpolated openness."""
  zbuffer = np.full((size, size), np.inf, dtype=np.float64)
  face_id = np.full((size, size), -1, dtype=np.int32)
  open_buffer = np.zeros((size, size), dtype=np.float32)
  for t, (i0, i1, i2) in enumerate(triangles):
    x0, x1, x2 = sx[i0], sx[i1], sx[i2]
    y0, y1, y2 = sy[i0], sy[i1], sy[i2]
    area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0)
    if abs(area) < 1e-12:
      continue
    left = max(int(math.floor(min(x0, x1, x2) - 0.5)), 0)
    right = min(int(math.ceil(max(x0, x1, x2) - 0.5)), size - 1)
    top = max(int(math.floor(min(y0, y1, y2) - 0.5)), 0)
    bottom = min(int(math.ceil(max(y0, y1, y2) - 0.5)), size - 1)
    if left > right or top > bottom:
      continue
    px = np.arange(left, right + 1) + 0.5
    py = np.arange(top, bottom + 1)[:, None] + 0.5
    w0 = ((x1 - px) * (y2 - py) - (x2 - px) * (y1 - py)) / area
    w1 = ((x2 - px) * (y0 - py) - (x0 - px) * (y2 - py)) / area
    w2 = 1.0 - w0 - w1
    inside = (w0 >= -1e-9) & (w1 >= -1e-9) & (w2 >= -1e-9)
    if not inside.any():
      continue
    z = w0 * depth[i0] + w1 * depth[i1] + w2 * depth[i2]
    region = zbuffer[top:bottom + 1, left:right + 1]
    nearer = inside & (z < region)
    if not nearer.any():
      continue
    region[nearer] = z[nearer]
    face_id[top:bottom + 1, left:right + 1][nearer] = t
    o = w0 * openness[i0] + w1 * openness[i1] + w2 * openness[i2]
    open_buffer[top:bottom + 1, left:right + 1][nearer] = o[nearer]
  return zbuffer, face_id, open_buffer


def raster_lines(p0, p1, d0, d1, colors, zbuffer, half_width, bias):
  """Antialiased lines of a width in pixels, tested less-or-equal against the faces' depth after a pull of bias toward
  the eye; no depth write, so a line never hides another (ADR-027 decision 3). Returns coverage and color."""
  size = zbuffer.shape[0]
  coverage = np.zeros((size, size), dtype=np.float32)
  color = np.zeros((size, size, 3), dtype=np.float32)
  reach = int(math.ceil(half_width + 1.0))
  offsets = np.stack(np.meshgrid(np.arange(-reach, reach + 1), np.arange(-reach, reach + 1)), axis=-1).reshape(-1, 2)
  flat_z = zbuffer.ravel()
  flat_cov = coverage.ravel()
  flat_color = color.reshape(-1, 3)
  for k in range(p0.shape[0]):
    a, b = p0[k], p1[k]
    delta = b - a
    length = math.hypot(delta[0], delta[1])
    steps = max(1, int(math.ceil(length))) + 1
    along = np.linspace(0.0, 1.0, steps)[:, None]
    points = np.floor(a[None, :] + along * delta[None, :]).astype(np.int64)
    candidates = (points[:, None, :] + offsets[None, :, :]).reshape(-1, 2)
    ok = (candidates[:, 0] >= 0) & (candidates[:, 0] < size) & (candidates[:, 1] >= 0) & (candidates[:, 1] < size)
    candidates = candidates[ok]
    linear = np.unique(candidates[:, 1] * size + candidates[:, 0])
    cx = (linear % size) + 0.5
    cy = (linear // size) + 0.5
    if length > 1e-9:
      t = np.clip(((cx - a[0]) * delta[0] + (cy - a[1]) * delta[1]) / (length * length), 0.0, 1.0)
    else:
      t = np.zeros_like(cx)
    dx = cx - (a[0] + t * delta[0])
    dy = cy - (a[1] + t * delta[1])
    distance = np.sqrt(dx * dx + dy * dy)
    cover = np.clip(half_width + 0.5 - distance, 0.0, 1.0)
    z = d0[k] + t * (d1[k] - d0[k]) - bias
    visible = (cover > 0.0) & (z <= flat_z[linear] + 1e-9)
    linear, cover = linear[visible], cover[visible]
    better = cover > flat_cov[linear]
    linear, cover = linear[better], cover[better]
    flat_cov[linear] = cover
    flat_color[linear] = colors[k]
  return coverage, color


def box_down(image, factor):
  h, w = image.shape[:2]
  return image.reshape(h // factor, factor, w // factor, factor, *image.shape[2:]).mean(axis=(1, 3))


# ── The glow (ADR-091), replayed on the output image ────────────────────────────────────────────────────────────────

def sample_bilinear(source, u, v):
  """Bilinear, clamped, at texture coordinates u, v (0..1), as SampleLevel with linearClamp."""
  h, w = source.shape[:2]
  x = np.clip(u * w - 0.5, 0.0, w - 1.0)
  y = np.clip(v * h - 0.5, 0.0, h - 1.0)
  x0 = np.floor(x).astype(np.int64)
  y0 = np.floor(y).astype(np.int64)
  x1 = np.minimum(x0 + 1, w - 1)
  y1 = np.minimum(y0 + 1, h - 1)
  fx = (x - x0)[..., None]
  fy = (y - y0)[..., None]
  top = source[y0, x0] * (1 - fx) + source[y0, x1] * fx
  bottom = source[y1, x0] * (1 - fx) + source[y1, x1] * fx
  return top * (1 - fy) + bottom * fy


def target_uv(height, width):
  v, u = np.meshgrid((np.arange(height) + 0.5) / height, (np.arange(width) + 0.5) / width, indexing="ij")
  return u, v


def bright(color):
  """BloomBrightPS.hlsl Bright()."""
  luminance = color @ LUMINANCE
  soft = np.clip(luminance - BLOOM_THRESHOLD + BLOOM_KNEE, 0.0, 2.0 * BLOOM_KNEE)
  curve = soft * soft / (4.0 * BLOOM_KNEE + 1e-5)
  share = np.maximum(curve, luminance - BLOOM_THRESHOLD) / np.maximum(luminance, 1e-5)
  return color * share[..., None]


def tent(source, height, width):
  """BloomUpPS / ResolvePS: a 3x3 tent of bilinear taps a source texel apart, weights 1-2-1, summed (not divided)."""
  u, v = target_uv(height, width)
  th, tw = source.shape[:2]
  du, dv = 1.0 / tw, 1.0 / th
  total = np.zeros((height, width, 3))
  for ox, oy, weight in [(-1, -1, 1), (1, -1, 1), (-1, 1, 1), (1, 1, 1), (0, -1, 2), (-1, 0, 2), (1, 0, 2), (0, 1, 2), (0, 0, 4)]:
    total += weight * sample_bilinear(source, u + ox * du, v + oy * dv)
  return total


def down13(source):
  """BloomDownPS.hlsl: thirteen bilinear taps in five overlapping boxes."""
  h, w = max(1, source.shape[0] // 2), max(1, source.shape[1] // 2)
  u, v = target_uv(h, w)
  su, sv = 1.0 / source.shape[1], 1.0 / source.shape[0]

  def tap(ox, oy):
    return sample_bilinear(source, u + ox * su, v + oy * sv)

  a, b, c = tap(-2, -2), tap(0, -2), tap(2, -2)
  d, e = tap(-1, -1), tap(1, -1)
  f, g, hh = tap(-2, 0), tap(0, 0), tap(2, 0)
  i, j = tap(-1, 1), tap(1, 1)
  k, l, m = tap(-2, 2), tap(0, 2), tap(2, 2)
  total = (d + e + i + j) * 0.125
  total += (a + b + g + f) * 0.03125
  total += (b + c + hh + g) * 0.03125
  total += (f + g + l + k) * 0.03125
  total += (g + hh + m + l) * 0.03125
  return total


def bloom(linear_over_black):
  """The glow added to an image in linear color over black, as Bloom.cpp's passes and the resolve add it."""
  h, w = linear_over_black.shape[:2]
  # The first level: each pixel the mean of Bright() over two by two scene pixels.
  first = bright(linear_over_black[: h // 2 * 2, : w // 2 * 2]).reshape(h // 2, 2, w // 2, 2, 3).mean(axis=(1, 3))
  levels = [first]
  for _ in range(BLOOM_LEVELS - 1):
    levels.append(down13(levels[-1]))
  for level in range(BLOOM_LEVELS - 1, 0, -1):
    target = levels[level - 1]
    levels[level - 1] = target + tent(levels[level], *target.shape[:2]) * (BLOOM_SPREAD / 16.0)
  weights = sum(BLOOM_SPREAD ** k for k in range(BLOOM_LEVELS))
  composite_scale = BLOOM_INTENSITY / weights
  return tent(levels[0], h, w) * (composite_scale / 16.0)


# ── One render ──────────────────────────────────────────────────────────────────────────────────────────────────────

def prepare(mesh, scale):
  """The fitted mesh (centered on its bounds and scaled, CenterAndScale) and its pieces' crease lines."""
  low, high = bounds(mesh["position"])
  center = (low + high) / 2.0
  position = (mesh["position"].astype(np.float64) - center) * scale
  lines_from, lines_to, lines_normal = [], [], []
  for piece in pieces_of(mesh):
    f, t, n = crease_lines(mesh["position"], piece, math.radians(CREASE_DEGREES))
    if f.shape[0]:
      lines_from.append((f - center) * scale)
      lines_to.append((t - center) * scale)
      lines_normal.append(n)
  if lines_from:
    lf, lt, ln = np.concatenate(lines_from), np.concatenate(lines_to), np.concatenate(lines_normal)
  else:
    lf = lt = ln = np.zeros((0, 3))
  return {"position": position, "normal": mesh["normal"].astype(np.float64), "open": mesh["open"], "indices": mesh["indices"],
          "lines": (lf, lt, ln)}


def project_extent(position, view):
  right, up, _ = view_axes(view)
  rotated = position @ heading_rotation(VIEW_HEADINGS[view])
  sx, sy = rotated @ right, rotated @ up
  return sx.min(), sx.max(), sy.min(), sy.max()


def render(prepared, face_color, line_color, view, size=1024, frame=None):
  """Linear premultiplied RGB over black and alpha, size x size, and the glow to add to it (ADR-091). frame = (cx, cy,
  half) in meters, or None to frame the model by its own projected extent."""
  ss = SUPERSAMPLE
  n = size * ss
  right, up, forward = view_axes(view)
  rotation = heading_rotation(VIEW_HEADINGS[view])
  position = prepared["position"] @ rotation
  normal = prepared["normal"] @ rotation
  normal /= np.maximum(np.linalg.norm(normal, axis=1, keepdims=True), 1e-12)
  sx, sy, sz = position @ right, position @ up, position @ forward
  if frame is None:
    cx, cy = (sx.min() + sx.max()) / 2.0, (sy.min() + sy.max()) / 2.0
    half = max(sx.max() - sx.min(), sy.max() - sy.min()) / 2.0 / FRAME_FILL
  else:
    cx, cy, half = frame
  meters_per_pixel = 2.0 * half / n
  px = (sx - (cx - half)) / meters_per_pixel
  py = ((cy + half) - sy) / meters_per_pixel

  triangles = prepared["indices"].reshape(-1, 3)
  # Back faces culled (ADR-011 decision 7): a face whose normal turns away from the eye. GameAppTests holds every model's
  # winding clockwise about its normals, so this is the same test as the winding.
  face_normal = normal[triangles].mean(axis=1)
  face_normal /= np.maximum(np.linalg.norm(face_normal, axis=1, keepdims=True), 1e-12)
  front = (face_normal @ forward) < 0.0
  kept = np.flatnonzero(front)
  zbuffer, face_index, openness = raster_faces(px, py, sz, triangles[kept], prepared["open"], n)
  covered = face_index >= 0
  face_light = light_at(face_normal[kept])  # per kept face
  rgb = np.zeros((n, n, 3))
  ids = face_index[covered]
  darkened = 1.0 - OCCLUSION_STRENGTH * (1.0 - np.clip(openness[covered], 0.0, 1.0))
  rgb[covered] = np.minimum(1.0, face_color[None, :] * face_light[ids] * darkened[:, None])
  alpha = covered.astype(np.float64)

  lf, lt, ln = prepared["lines"]
  if lf.shape[0]:
    lf, lt = lf @ rotation, lt @ rotation
    ln = ln @ rotation
    ln /= np.maximum(np.linalg.norm(ln, axis=1, keepdims=True), 1e-12)
    p0 = np.stack([(lf @ right - (cx - half)) / meters_per_pixel, ((cy + half) - lf @ up) / meters_per_pixel], axis=1)
    p1 = np.stack([(lt @ right - (cx - half)) / meters_per_pixel, ((cy + half) - lt @ up) / meters_per_pixel], axis=1)
    brightness = light_at(ln) @ LUMINANCE
    colors = np.minimum(1.0, line_color[None, :] * brightness[:, None])
    bias = LINE_DEPTH_PIXELS * ss * meters_per_pixel
    coverage, line_rgb = raster_lines(p0, p1, lf @ forward, lt @ forward, colors, zbuffer, LINE_WIDTH_PIXELS * ss / 2.0, bias)
    c = coverage[..., None]
    rgb = rgb * (1.0 - c) + line_rgb * c
    alpha = alpha * (1.0 - coverage) + coverage

  rgb = box_down(rgb, ss)
  alpha = box_down(alpha, ss)
  glow = bloom(rgb)
  return rgb, alpha, glow, {"metersPerPixel": meters_per_pixel * ss, "frame": (float(cx), float(cy), float(half)),
                      "lineCount": int(lf.shape[0]), "frontFaces": int(kept.size)}


def to_rgba8(rgb_over_black, alpha):
  """Straight-alpha sRGB RGBA8 that shows the render as it is over a black sky in an ordinary (gamma-space) viewer."""
  encoded = srgb_encode(rgb_over_black)
  a = np.maximum(alpha, encoded.max(axis=-1))
  a = np.clip(a, 0.0, 1.0)
  straight = np.where(a[..., None] > 1e-6, encoded / np.maximum(a[..., None], 1e-6), 0.0)
  rgba = np.concatenate([np.clip(straight, 0.0, 1.0), a[..., None]], axis=-1)
  return (rgba * 255.0 + 0.5).astype(np.uint8)

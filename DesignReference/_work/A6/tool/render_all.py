"""Renders every shipped model of every set in Models.json, both views, plus the sheets; writes stats.json and the
model part of renders.json. Agent A6. Read-only on the repository; writes under DesignReference/_work/ only.

Usage: python3 render_all.py [--jobs 4] [--only Human/Medium]
"""

import argparse
import hashlib
import json
import math
import multiprocessing
import sys
import time
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

sys.path.insert(0, str(Path(__file__).resolve().parent))
import nmfrender as R  # noqa: E402

ROOT = Path(__file__).resolve().parents[4]
ASSETS = ROOT / "OutpostCommander" / "Assets"
ART = ROOT / "Art" / "Models"
WORK = ROOT / "DesignReference" / "_work"
OUT_MODELS = WORK / "assets" / "models"
OUT_SHEETS = WORK / "assets" / "sheets"
STATS_FILE = WORK / "A6" / "stats.json"
RENDERS_FILE = WORK / "A6" / "renders_models.json"

FONT = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
FONT_BOLD = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
APPROXIMATION = "approximation of the in-game look"
VIEW_NAMES = {"34": "3/4 view", "top": "top view"}
SHEET_BACKGROUND = (5, 6, 10, 255)
LEVELLED = ("CommandStation", "Shipyard", "ResearchLab")


def sha256(path):
  return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def rel(path):
  return str(Path(path).relative_to(ROOT))


# ── The catalogue, as ModelCatalog.cpp reads Models.json ────────────────────────────────────────────────────────────

def load_catalogue():
  data = json.loads((ASSETS / "Models.json").read_text())
  sets = data["sets"]
  by_name = {s["name"]: (i, s) for i, s in enumerate(sets)}
  hull_models = {h["model"] for h in data["hulls"]}
  structure_models = {}
  for i, s in enumerate(data["structures"]):
    structure_models.setdefault(s["model"], []).append((i, s))
  tuning = json.loads((ASSETS / "Tuning.json").read_text())
  footprint = {s["kind"]: s["footprintRadiusMeters"] for s in tuning["structures"]}
  return data, sets, by_name, hull_models, structure_models, footprint


def kind_of(set_name, model_name, data, hull_models, structure_models):
  if set_name == "Asteroids":
    return "rock"
  if model_name in hull_models or model_name == data["constructor"]:
    return "ship"
  if model_name in structure_models:
    return "structure"
  return "unknown"


def entries():
  """Every render the catalogue asks for: (set, entry name, mesh folder, model, level, kind, tint, length source...)."""
  data, sets, by_name, hull_models, structure_models, footprint = load_catalogue()
  result = []
  for set_index, s in enumerate(sets):
    name = s["name"]
    meshes = s.get("meshes", name)
    lender_index, lender = by_name[meshes]
    color = [s["color"]["red"], s["color"]["green"], s["color"]["blue"]]
    for model_index, model in enumerate(lender["models"]):
      kind = kind_of(name, model["name"], data, hull_models, structure_models)
      # A derelict is drawn by its hull's model only (GameClient.cpp PlaceModel, derelict branch; ADR-074).
      if name == data["derelicts"] and model["name"] not in hull_models:
        continue
      levels = model.get("levels", 0)
      for level in (range(1, levels + 1) if levels else [0]):
        entry = f"{model['name']}_L{level}" if level else model["name"]
        result.append({
          "set": name, "entry": entry, "meshes": meshes, "model": model["name"], "level": level, "kind": kind, "tint": 1.0,
          "color": color, "colorKey": f"Models.json: sets[{set_index}].color",
          "lengthMeters": model["lengthMeters"],
          "lengthKey": f"Models.json: sets[{lender_index}].models[{model_index}].lengthMeters"
                       + (f" (borrowed via sets[{set_index}].meshes = \"{meshes}\")" if meshes != name else ""),
          "levels": levels,
          "nmf": f"OutpostCommander/Assets/Models/{meshes}/{entry}.nmf",
          "glb": f"Art/Models/{meshes}/{entry}.glb",
          "variantOf": None,
        })
    # The structures drawn with another's model and a tint: Relay (ResearchLab, 1.6) and Repair Bay (Shipyard, 0.6),
    # always its model's first level (DrawnLevel, ModelCatalog.h:158). Only the players' sets build them.
    if name in [p["set"] for p in data["players"]]:
      for structure_index, st in enumerate(data["structures"]):
        if "tint" not in st:
          continue
        model_index, model = next((i, m) for i, m in enumerate(lender["models"]) if m["name"] == st["model"])
        level = 1 if model.get("levels", 0) else 0
        mesh_entry = f"{model['name']}_L{level}" if level else model["name"]
        result.append({
          "set": name, "entry": st["structure"], "meshes": meshes, "model": model["name"], "level": level, "kind": "structure",
          "tint": st["tint"], "color": color, "colorKey": f"Models.json: sets[{set_index}].color",
          "tintKey": f"Models.json: structures[{structure_index}].tint",
          "lengthMeters": model["lengthMeters"],
          "lengthKey": f"Models.json: sets[{lender_index}].models[{model_index}].lengthMeters",
          "levels": model.get("levels", 0),
          "nmf": f"OutpostCommander/Assets/Models/{meshes}/{mesh_entry}.nmf",
          "glb": f"Art/Models/{meshes}/{mesh_entry}.glb",
          "variantOf": mesh_entry,
        })
  return result


# ── Images ──────────────────────────────────────────────────────────────────────────────────────────────────────────

def font(size, bold=False):
  return ImageFont.truetype(FONT_BOLD if bold else FONT, size)


def label(image, title, size):
  """A small title top-left and the approximation note bottom-left, composited over the render."""
  layer = Image.new("RGBA", image.size, (0, 0, 0, 0))
  draw = ImageDraw.Draw(layer)
  big = 18 if size >= 1024 else 11
  margin = 14 if size >= 1024 else 7
  stroke = 1
  draw.text((margin, margin), title, font=font(big), fill=(215, 220, 232, 255), stroke_width=stroke, stroke_fill=(12, 14, 20, 255))
  note = font(big - 2 if size >= 1024 else 10)
  height = draw.textbbox((0, 0), APPROXIMATION, font=note)[3]
  draw.text((margin, size - margin - height - 2), APPROXIMATION, font=note, fill=(190, 195, 208, 255), stroke_width=stroke,
            stroke_fill=(12, 14, 20, 255))
  return Image.alpha_composite(image, layer)


def save_render(rgb, alpha, path, title):
  size = rgb.shape[0]
  image = Image.fromarray(R.to_rgba8(rgb, alpha), "RGBA")
  label(image, title, size).save(path, optimize=True)
  small_rgb, small_alpha = R.box_down(rgb, 2), R.box_down(alpha, 2)
  small = Image.fromarray(R.to_rgba8(small_rgb, small_alpha), "RGBA")
  small_path = path.with_name(path.stem + "_512.png")
  label(small, title, size // 2).save(small_path, optimize=True)
  return small_path


def prepare_entry(entry, first_level_widest=None):
  mesh = R.parse_nmf(ROOT / entry["nmf"])
  scale = R.fit_scale(mesh, entry["lengthMeters"], first_level_widest)
  return mesh, scale, R.prepare(mesh, scale)


def first_level_widest(entry):
  """Level 1's widest across x and z, in meters, as LoadClientAssets keeps it (GameClient.cpp:367-372)."""
  first = dict(entry, nmf=entry["nmf"].replace(f"_L{entry['level']}.nmf", "_L1.nmf"))
  mesh = R.parse_nmf(ROOT / first["nmf"])
  low, high = R.bounds(mesh["position"])
  scale = entry["lengthMeters"] / (high - low)[0]
  extents = (high - low) * scale
  return float(max(extents[0], extents[2]))


def render_job(job):
  entry, view, frame, out_path = job["entry"], job["view"], job["frame"], Path(job["out"])
  widest = first_level_widest(entry) if entry["level"] and entry["level"] > 1 else None
  _, _, prepared = prepare_entry(entry, widest)
  face, line = R.colors_for(entry["kind"], entry["color"], entry["tint"])
  started = time.time()
  rgb, alpha, glow, info = R.render(prepared, np.asarray(face), np.asarray(line), view, 1024, frame)
  title = job["title"]
  # The transparent images keep the glow over the model only: an added light outside it cannot be stored in a
  # transparent pixel without showing as a pale halo on a light page. On black, as in the sheets, it is added whole.
  inside = np.minimum(1.0, rgb + glow * alpha[..., None])
  on_black = np.minimum(1.0, rgb + glow)
  small_path = save_render(inside, alpha, out_path, title)
  if job.get("thumb"):
    thumb = Image.fromarray(R.to_rgba8(R.box_down(on_black, 4), R.box_down(alpha, 4)), "RGBA")
    thumb.save(job["thumb"])
  if job.get("keepArrays"):
    np.savez_compressed(out_path.with_suffix(".npz"), rgb=R.box_down(on_black, 2).astype(np.float32),
                        alpha=R.box_down(alpha, 2).astype(np.float32))
  return {"out": str(out_path), "small": str(small_path), "seconds": round(time.time() - started, 1),
          "metersPerPixel": float(info["metersPerPixel"]), "lineCount": info["lineCount"], "faceColor": [float(v) for v in face],
          "lineColor": [float(v) for v in line]}


# ── Stats ───────────────────────────────────────────────────────────────────────────────────────────────────────────

def stats_for(entry):
  mesh = R.parse_nmf(ROOT / entry["nmf"])
  widest = first_level_widest(entry) if entry["level"] and entry["level"] > 1 else None
  scale = R.fit_scale(mesh, entry["lengthMeters"], widest)
  low, high = R.bounds(mesh["position"])
  tags = {}
  for hardpoint in mesh["hardpoints"]:
    tags[hardpoint["tag"]] = tags.get(hardpoint["tag"], 0) + 1
  creases = sum(R.crease_lines(mesh["position"], piece, math.radians(R.CREASE_DEGREES))[0].shape[0] for piece in R.pieces_of(mesh))
  half = (high - low) * scale / 2.0
  return {
    "vertexCount": int(mesh["position"].shape[0]),
    "triangles": int(mesh["indices"].size // 3),
    "boundsMin": [round(float(v), 4) for v in low],
    "boundsMax": [round(float(v), 4) for v in high],
    "fitScale": scale,
    "fitRule": "FitMeshAcross(level 1 widest)" if widest is not None else "FitMesh(lengthMeters)",
    "firstLevelWidestMeters": widest,
    "fittedMin": [round(float(-v), 3) for v in half],
    "fittedMax": [round(float(v), 3) for v in half],
    "fittedExtentsMeters": [round(float(2 * v), 3) for v in half],
    "hardpoints": tags,
    "parts": [{"periodSeconds": round(p["periodSeconds"], 4), "axis": [round(a, 4) for a in p["axis"]],
               "triangles": p["indexCount"] // 3} for p in mesh["parts"]],
    "creaseLines": int(creases),
    "meanOpenness": round(float(mesh["open"].mean()), 3),
    "nmfSha256": sha256(ROOT / entry["nmf"]),
    "glbSha256": sha256(ROOT / entry["glb"]) if (ROOT / entry["glb"]).exists() else None,
    "nmfBytes": (ROOT / entry["nmf"]).stat().st_size,
    "glbBytes": (ROOT / entry["glb"]).stat().st_size if (ROOT / entry["glb"]).exists() else None,
  }


# ── Sheets ──────────────────────────────────────────────────────────────────────────────────────────────────────────

def common_frame(level_entries, view):
  """One frame for every level of a model, at the size the game fits each (ADR-064): the union of their projections."""
  xs0, xs1, ys0, ys1 = [], [], [], []
  for entry in level_entries:
    widest = first_level_widest(entry) if entry["level"] > 1 else None
    _, _, prepared = prepare_entry(entry, widest)
    a, b, c, d = R.project_extent(prepared["position"], view)
    xs0.append(a), xs1.append(b), ys0.append(c), ys1.append(d)
  x0, x1, y0, y1 = min(xs0), max(xs1), min(ys0), max(ys1)
  return ((x0 + x1) / 2.0, (y0 + y1) / 2.0, max(x1 - x0, y1 - y0) / 2.0 / 0.9)


def tile_from_npz(path, size):
  arrays = np.load(path)
  rgb, alpha = arrays["rgb"], arrays["alpha"]
  factor = rgb.shape[0] // size
  if factor > 1:
    rgb, alpha = R.box_down(rgb, factor), R.box_down(alpha, factor)
  return Image.fromarray(R.to_rgba8(rgb, alpha), "RGBA")


def compose_level_sheet(set_name, model, tiles, path, lengths):
  tile = 400
  header = 70
  row_label = 34
  width = tile * 5
  height = header + 2 * (tile + row_label) + 40
  sheet = Image.new("RGBA", (width, height), SHEET_BACKGROUND)
  draw = ImageDraw.Draw(sheet)
  draw.text((16, 14), f"{set_name} · {model} · levels L1 → L5 at one scale", font=font(26, True), fill=(225, 230, 240, 255))
  draw.text((16, 46), "Each level as the game fits it: L1 to lengthMeters along x, L2–L5 within L1's widest (ModelCatalog.cpp:382-385, ADR-064)",
            font=font(15), fill=(160, 166, 180, 255))
  for row, view in enumerate(("34", "top")):
    top = header + row * (tile + row_label)
    for column in range(5):
      image = tiles[(view, column + 1)]
      sheet.alpha_composite(image, (column * tile, top + row_label))
      draw.text((column * tile + 12, top + 6), f"L{column + 1} · {VIEW_NAMES[view]}", font=font(17, True), fill=(205, 210, 222, 255))
  draw.text((16, height - 30), f"{APPROXIMATION} · rendered from the shipped .nmf by DesignReference/_work/A6/tool",
            font=font(15), fill=(150, 155, 170, 255))
  sheet.convert("RGB").save(path, optimize=True)


def compose_set_sheet(set_name, items, path, color):
  cell_w, cell_h = 512, 256 + 44
  columns = 4
  rows = math.ceil(len(items) / columns)
  header = 76
  width = cell_w * columns
  height = header + rows * cell_h + 40
  sheet = Image.new("RGBA", (width, height), SHEET_BACKGROUND)
  draw = ImageDraw.Draw(sheet)
  swatch = tuple(int(round(v * 255)) for v in R.srgb_encode(np.asarray(color)))
  draw.rectangle((16, 18, 44, 46), fill=swatch + (255,))
  draw.text((56, 14), f"{set_name}", font=font(28, True), fill=(225, 230, 240, 255))
  draw.text((56, 48), "Each model in its 3/4 view and top view, each framed by its own bounds (not to scale)", font=font(15),
            fill=(160, 166, 180, 255))
  for i, item in enumerate(items):
    left = (i % columns) * cell_w
    top = header + (i // columns) * cell_h
    for j, view in enumerate(("34", "top")):
      image = Image.open(item[view]).convert("RGBA")
      sheet.alpha_composite(image, (left + j * 256, top + 44))
    draw.text((left + 10, top + 6), item["title"], font=font(17, True), fill=(210, 215, 228, 255))
    draw.text((left + 10, top + 26), item["subtitle"], font=font(13), fill=(150, 156, 172, 255))
  draw.text((16, height - 30), f"{APPROXIMATION} · rendered from the shipped .nmf by DesignReference/_work/A6/tool",
            font=font(15), fill=(150, 155, 170, 255))
  sheet.convert("RGB").save(path, optimize=True)


# ── Main ────────────────────────────────────────────────────────────────────────────────────────────────────────────

def main():
  parser = argparse.ArgumentParser()
  parser.add_argument("--jobs", type=int, default=4)
  parser.add_argument("--only", default=None)
  parser.add_argument("--skip-renders", action="store_true")
  arguments = parser.parse_args()

  all_entries = entries()
  if arguments.only:
    all_entries = [e for e in all_entries if f"{e['set']}/{e['entry']}" == arguments.only]

  stats = []
  for entry in all_entries:
    stats.append(dict(entry, **stats_for(entry)))
  STATS_FILE.write_text(json.dumps(stats, indent=1))

  jobs = []
  for entry in all_entries:
    folder = OUT_MODELS / entry["set"]
    folder.mkdir(parents=True, exist_ok=True)
    for view in ("34", "top"):
      jobs.append({"entry": entry, "view": view, "frame": None, "out": str(folder / f"{entry['entry']}_{view}.png"),
                   "thumb": str(WORK / "A6" / "tool" / "_tiles" / f"thumb_{entry['set']}_{entry['entry']}_{view}.png"),
                   "title": f"{entry['set']} · {entry['entry']} · {VIEW_NAMES[view]}", "kind": "model"})

  # Level sheets: every levelled structure of every set that has one, at one scale for its five levels.
  level_groups = {}
  for entry in all_entries:
    if entry["model"] in LEVELLED and entry["level"] and entry["variantOf"] is None:
      level_groups.setdefault((entry["set"], entry["model"]), []).append(entry)
  tile_dir = WORK / "A6" / "tool" / "_tiles"
  tile_dir.mkdir(parents=True, exist_ok=True)
  for (set_name, model), group in level_groups.items():
    group.sort(key=lambda e: e["level"])
    for view in ("34", "top"):
      frame = common_frame(group, view)
      for entry in group:
        jobs.append({"entry": entry, "view": view, "frame": frame, "keepArrays": True,
                     "out": str(tile_dir / f"{set_name}_{entry['entry']}_{view}.png"),
                     "title": f"{set_name} · {entry['entry']} · {VIEW_NAMES[view]} · common scale", "kind": "levelTile"})

  results = []
  if not arguments.skip_renders:
    started = time.time()
    with multiprocessing.Pool(arguments.jobs) as pool:
      for k, result in enumerate(pool.imap_unordered(render_job, jobs)):
        results.append(result)
        if k % 20 == 0:
          print(f"{k + 1}/{len(jobs)} {result['out']} {result['seconds']}s ({time.time() - started:.0f}s)", flush=True)
    print(f"rendered {len(jobs)} in {time.time() - started:.0f}s")

  render_info = {r["out"]: r for r in results}
  records = []
  for job in jobs:
    if job["kind"] != "model":
      continue
    entry = job["entry"]
    info = render_info.get(job["out"], {})
    for path, size in ((Path(job["out"]), 1024), (Path(job["out"]).with_name(Path(job["out"]).stem + "_512.png"), 512)):
      records.append({"file": rel(path), "set": entry["set"], "model": entry["entry"], "mesh": entry["nmf"], "view": job["view"],
                      "size": [size, size], "kind": "model render" if size == 1024 else "model render, web size",
                      "metersPerPixel": (info.get("metersPerPixel", 0) * (1024 / size)) if info else None,
                      "label": APPROXIMATION})

  # Level sheets.
  OUT_SHEETS.mkdir(parents=True, exist_ok=True)
  for (set_name, model), group in level_groups.items():
    if len(group) != 5:
      continue
    tiles = {}
    for view in ("34", "top"):
      for entry in group:
        tiles[(view, entry["level"])] = tile_from_npz(tile_dir / f"{set_name}_{entry['entry']}_{view}.npz", 512).resize(
          (400, 400), Image.LANCZOS)
    path = OUT_SHEETS / f"{set_name}_{model}_levels.png"
    compose_level_sheet(set_name, model, tiles, path, None)
    records.append({"file": rel(path), "set": set_name, "model": model, "view": "34 and top, L1-L5", "size": list(Image.open(path).size),
                    "kind": "level sheet", "label": APPROXIMATION})

  # Set sheets.
  data = json.loads((ASSETS / "Models.json").read_text())
  for s in data["sets"]:
    items = []
    for entry in all_entries:
      if entry["set"] != s["name"]:
        continue
      folder = OUT_MODELS / entry["set"]
      subtitle = f"{entry['lengthMeters']} m (Models.json) · {entry['kind']}"
      if entry["variantOf"]:
        subtitle += f" · {entry['variantOf']} mesh, tint {entry['tint']}"
      thumbs = WORK / "A6" / "tool" / "_tiles"
      items.append({"34": thumbs / f"thumb_{entry['set']}_{entry['entry']}_34.png", "top": thumbs / f"thumb_{entry['set']}_{entry['entry']}_top.png",
                    "title": entry["entry"], "subtitle": subtitle})
    if not items:
      continue
    path = OUT_SHEETS / f"{s['name']}.png"
    color = [s["color"]["red"], s["color"]["green"], s["color"]["blue"]]
    compose_set_sheet(s["name"], items, path, color)
    records.append({"file": rel(path), "set": s["name"], "model": "all", "view": "34 and top", "size": list(Image.open(path).size),
                    "kind": "set sheet", "label": APPROXIMATION})

  RENDERS_FILE.write_text(json.dumps(records, indent=1))
  (WORK / "A6" / "render_times.json").write_text(json.dumps(results, indent=1))


if __name__ == "__main__":
  main()

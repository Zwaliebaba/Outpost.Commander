"""Writes DesignReference/_work/A6/A6.md and renders.json from what render_all.py and textures.py left. Agent A6."""

import json
import math
import sys
from pathlib import Path

import numpy as np
from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent))
import nmfrender as R  # noqa: E402

ROOT = Path(__file__).resolve().parents[4]
WORK = ROOT / "DesignReference" / "_work"
A6 = WORK / "A6"
HEAD = "a053fe294d91dc885bbb061c2f34d76ca6897738"
APPROX = "approximation of the in-game look"


def hexcolor(linear):
  return "#" + "".join(f"{int(round(v * 255)):02x}" for v in R.srgb_encode(np.asarray(linear, dtype=float)))


def triple(values, digits=3):
  return "(" + ", ".join(f"{v:.{digits}f}" for v in values) + ")"


def fmt3(values, digits=2):
  return "(" + ", ".join(f"{v:.{digits}f}" for v in values) + ")"


def main():
  stats = json.loads((A6 / "stats.json").read_text())
  textures = json.loads((A6 / "textures.json").read_text())
  model_records = json.loads((A6 / "renders_models.json").read_text())
  texture_records = json.loads((A6 / "renders_textures.json").read_text())
  bake = json.loads((A6 / "bakecheck.json").read_text())
  models_json = json.loads((ROOT / "OutpostCommander/Assets/Models.json").read_text())
  tuning = json.loads((ROOT / "OutpostCommander/Assets/Tuning.json").read_text())
  footprint = {s["kind"]: (i, s["footprintRadiusMeters"]) for i, s in enumerate(tuning["structures"])}
  structure_kind_of_model = {s["model"]: s["structure"] for s in models_json["structures"] if "tint" not in s}

  # Every image, checked on disk.
  records = model_records + texture_records
  missing = []
  for record in records:
    path = ROOT / record["file"]
    if not path.exists():
      missing.append(record["file"])
      continue
    with Image.open(path) as image:
      record["size"] = list(image.size)
      record["mode"] = image.mode
  (A6 / "renders.json").write_text(json.dumps({
    "agent": "A6", "head": HEAD, "renderer": "Python 3 + numpy software rasterizer, DesignReference/_work/A6/tool/nmfrender.py",
    "note": f"Every model render and sheet is an {APPROX}.", "count": len(records), "missing": missing, "images": records}, indent=1))

  lines = []
  out = lines.append
  out("# A6 — Asset renders: every shipped model in an approximation of the game's look")
  out("")
  out(f"Agent **A6**. HEAD `{HEAD}` (2026-10-25T08:33:55+01:00). Read-only on the repository; everything below is under "
      "`DesignReference/_work/` (images in `DesignReference/_work/assets/`, the tool in `DesignReference/_work/A6/tool/`, data in "
      "`DesignReference/_work/A6/*.json`).")
  out("")
  out(f"**Every model image here, and every sheet, is an {APPROX}**: rendered offline from the shipped meshes with the "
      "game's numbers, not captured from the game, which was not built or run (no Windows, MSBuild, GPU or shader compiler in "
      "this container). Each image also says so in its bottom-left corner.")
  out("")
  out("**Deviation from the brief, recorded:** the brief says to render every GLB in `OutpostCommander/Assets/Models/`, but that "
      "folder holds the baked `.nmf` files the game loads (45), and the glTF sources are in `Art/Models/` (45). The renders are "
      "made from the `.nmf` files: they are what ships, and only they carry the baked occlusion (ADR-092). The GLBs are used for "
      "hashes and the bake check, which proves each `.nmf` is exactly what its GLB bakes to (below).")
  out("")

  # ── Renderer ──
  out("## Renderer and how it replicates the game's shading")
  out("")
  out("**Renderer:** a software rasterizer in Python 3.13 and numpy 2.5 (`DesignReference/_work/A6/tool/nmfrender.py`, driven by "
      "`render_all.py`), not three.js: z-buffered triangles, then a depth-tested line pass, then the glow, all in linear color, "
      "encoded to sRGB at the end. No Blender, no GPU. It is the same arithmetic as the shaders, replayed per pixel.")
  out("")
  out("| What | In the game (source) | Here |")
  out("|---|---|---|")
  rows = [
    ("Mesh read", "NMF v3: positions, normals, one `open` byte a vertex, u32 indices, hardpoints, parts (`Tools/BakeMeshes.py` docstring; `NeuronClient/MeshData.cpp` `ParseNmf`)", "Same layout, same checks (padding zero, version 3, no bytes left over)."),
    ("Size", "L1 or a model without levels: `FitMesh` to `lengthMeters` along x; L2–L5: `FitMeshAcross` level 1's widest of x and z (`GameApp/ModelCatalog.cpp:382-385`, `GameApp/GameClient.cpp:367-372`, ADR-064)", "Same. Bounds centered on the origin (`FitMesh`, `FitMeshAcross`, `CenterAndScale`, `MeshData.cpp:219-254`)."),
    ("Face light", "`MeshPS.hlsl:33-43`: fill = lerp(`GROUND_FILL`, `SKY_FILL`, n.y·0.5+0.5); key = saturate((n·l + `KEY_WRAP`)/(1+`KEY_WRAP`)); color × (fill + `KEY_COLOR`·key) × (1 − `OCCLUSION_STRENGTH`·(1 − openness))", "Same formula per pixel, openness interpolated across the face, normal per face (every source is flat-shaded, three vertices a triangle: ADR-092 Context)."),
    ("Light numbers", "`GameApp/SceneShading.h:15-19, 23`: `TOWARD_KEY` (−0.4, 0.8, 0.45) normalized (`GameClient.cpp:1130`), `KEY_COLOR` (0.627, 0.539, 0.427), `KEY_WRAP` 0.3, `SKY_FILL` (0.365, 0.421, 0.571), `GROUND_FILL` (0.314, 0.287, 0.389), `OCCLUSION_STRENGTH` 1 (ADR-090 decision 1, ADR-092 decision 4)", "Same values. The light is fixed in the world, as in the game."),
    ("Line light", "`MeshLinePS.hlsl:42`: line color × luminance(LightAt(n)), weights (0.2126, 0.7152, 0.0722); never darkened by occlusion (ADR-092 decision 4)", "Same."),
    ("Crease lines", "`Neuron::BuildCreaseLines` (`NeuronClient/MeshData.cpp:289-397`) at `CREASE_DEGREES` = **10°** (`GameApp/GameClient.cpp:49`; ADR-027 decision 2): corners welded by exact position; a line on every edge whose two faces' normals differ by more than 10° (`cosine >= cos 10°` is skipped, `MeshData.cpp:377`), on every edge with one face, and on every edge with more than two; zero-area triangles skipped; each line's normal is the normalized mean of its faces' normals. Built per piece: the fixed triangles, then each spinning part (`GameClient.cpp:374-389`, ADR-045 decision 4)", "Same rule, same per-piece split (numpy, float32 positions)."),
    ("Lines over faces", "Each line vertex pulled `LINE_LIFT_SHARE` = 0.002 of the way to the eye (`GameClient.cpp:53`, `MeshLineVS.hlsl:59`), \"about 2.5 pixels of depth\" at the default view (ADR-040 decision 2); depth test less-or-equal, no depth write (ADR-027 decision 3)", "A depth bias of 2.5 output pixels toward the eye, less-or-equal test against the faces, no depth write."),
    ("Face and line colors", "Ship and Constructor: faces `Shaded(color, FILL_SHADE 0.3)`, lines `EdgeColor(color, EDGE_BRIGHTNESS 2, EDGE_WHITE_SHARE 0.35)` (`GameClient.cpp:1509, 1556-1569`). Structure: color = `TowardGray(ModelColor(set, tint, built), STRUCTURE_GRAY_SHARE 0.35)`, faces at `STRUCTURE_FILL_SHADE` 0.22 (`GameClient.cpp:1571-1579, 344-349`). Derelict: the Wreck set's color as a ship (`GameClient.cpp:1544-1546`). Asteroid: faces `ROCK_FILL_SHADE` 0.15, lines `EdgeColor(color, ROCK_EDGE_BRIGHTNESS 1.35, 0)` (`GameClient.cpp:1515-1516, 1560-1561`). Constants `SceneShading.h:45-57`, functions `SceneShading.cpp`", "Same functions, ported. Structures fully built (`builtPermille` 1000). Asteroids as an ore asteroid; a field's rocks are 0.7 of that color first (`FIELD_SHADE`, `GameClient.cpp:1522`) and are not rendered separately."),
    ("Culling", "Back faces culled, clockwise front (ADR-011 decision 7)", "A face is drawn when its normal faces the eye; every face's winding agrees with its normal (checked here on all 16,316 triangles, as `GameAppTests` holds)."),
    ("Antialiasing", "4× MSAA, averaged in linear color (ADR-040 decision 3, ADR-050)", "2×2 supersampling averaged in linear color, plus analytic coverage on the lines."),
    ("Color encoding", "Linear colors into an `R8G8B8A8_UNORM_SRGB` target (ADR-040 decision 3)", "sRGB-encoded at the end."),
    ("Glow (ADR-091)", "`SCENE_BLOOM` threshold 0.35, knee 0.15, intensity 2, spread 0.6 (`GameClient.cpp:25`); five half-size levels: bright pass (`BloomBrightPS.hlsl`), 13-tap down (`BloomDownPS.hlsl`), 3×3 tent up weighted by the spread (`BloomUpPS.hlsl`), added in the resolve × intensity / Σspreadᵏ (`ResolvePS.hlsl`, `Bloom.cpp:86-98`)", "**Added, as a light phosphor glow:** the same passes and numbers replayed with bilinear taps on the 1024-pixel image. In the transparent PNGs it is kept over the model only (an added light cannot be stored in a transparent pixel without a pale halo on a light page); on the sheets, over black, it is added whole."),
    ("Camera", "Default yaw looks along +z, screen right +x, top +z (`GameApp/Camera.cpp:13-14`); pitch from 40° to 70° with zoom (`Camera.json`, `Camera.cpp:160-165`); 45° vertical field of view", f"**3/4 view**: the default yaw at the default zoom's pitch, {R.DEFAULT_PITCH_DEGREES:.2f}° (view width 500 m of 150–3000 m, `Camera.json`), model heading 225° (nose toward the lower left). **Top view**: straight down, screen top +z, heading 90° (nose up). Orthographic: at the game's distance a model spans a few degrees, so this stands in for its perspective. Heading as `WorldMatrix`/`PoseMatrix` turn it (`XMMatrixRotationY(−heading)`, `GameClient.cpp:283-291`, `Hardpoints.cpp:54-64`)."),
  ]
  for a, b, c in rows:
    out(f"| {a} | {b} | {c} |")
  out("")
  out("**Where this is not the game, on purpose or of necessity:**")
  out("")
  out("- **Lines are 2 px wide at 1024 px** (about 1 px in the `_512` copies). The game draws 1-pixel lines at any zoom (ADR-027 "
      "decision 3), but a model fills ~880 px here against ~80 px for a 20 m hull at the default zoom (500 m across 1920 px), so 1 px "
      "would read far thinner than the game does.")
  out("- **Each image is framed by its own projected extent** (the model fills 84 % of the side), so images are **not to scale** "
      "with each other. The level sheets are the exception: one frame for all five levels.")
  out("- **Not drawn:** the starfield and grid behind the model, banking, exhaust glows at `exhaust` hardpoints (ADR-019), "
      "footprint rings (ADR-042), selection, health bars, a structure under construction (darker by `UNBUILT_SHADE`), a Mining "
      "Rig standing on its rock (ADR-044), a remembered structure's lines-only look (`MEMORY_GRAY_SHARE`, `MEMORY_SHADE`, `GameClient.cpp:119-120`). "
      "Spinning parts are drawn at rest (angle 0), as the baker and the explosion take them (ADR-045 decisions 2 and 5).")
  out("- **Not built, so not drawn** (`ImplementationPlan-Look.md` task board): paint slots, two-tone lines, beacons, fleet plates "
      "(LV2.2, LV2.3, LV3.1, LV4.1–4.3).")
  out("- **Transparent background** (straight alpha, written so that it shows as rendered over black in an ordinary viewer): the "
      "sky is not there, and a model's dark faces read lighter on a white page than on the game's black.")
  out("")

  # ── Set / color table ──
  out("## Sets and colors")
  out("")
  out("Colors are linear, as in the data; `#hex` is the sRGB the scene target shows for that linear color at full light. "
      "Face and line colors are before the light (the light multiplies them).")
  out("")
  out("| Set | `Models.json` color (linear) | Source | Meshes | Drawn for | Ship faces | Ship lines | Structure faces | Structure lines |")
  out("|---|---|---|---|---|---|---|---|---|")
  roles = {"Human": "player 1 (`Models.json: players[0]`)", "Tarkan": "player 2 (`Models.json: players[1]`)",
           "Pirate": "the pirates (`Models.json: pirates`)", "Wreck": "derelicts (`Models.json: derelicts`), by hull model only",
           "Asteroids": "asteroids and fields (`GameClient.cpp:41` `ASTEROID_SET`)"}
  line_of = {"Human": 5, "Tarkan": 20, "Pirate": 35, "Wreck": 40, "Asteroids": 45}
  for index, s in enumerate(models_json["sets"]):
    color = [s["color"]["red"], s["color"]["green"], s["color"]["blue"]]
    meshes = s.get("meshes", s["name"])
    meshes_note = meshes if meshes == s["name"] else f"{meshes}'s (`\"meshes\": \"{meshes}\"`, line {line_of[s['name']] + 1})"
    if s["name"] == "Asteroids":
      face, line = R.colors_for("rock", color)
      cells = [f"rock: {triple(face)} {hexcolor(face)}", f"rock: {triple(line)} {hexcolor(line)}", "—", "—"]
    else:
      sf, sl = R.colors_for("ship", color)
      tf, tl = R.colors_for("structure", color)
      cells = [f"{triple(sf)} {hexcolor(sf)}", f"{triple(sl)} {hexcolor(sl)}", f"{triple(tf)} {hexcolor(tf)}", f"{triple(tl)} {hexcolor(tl)}"]
      if s["name"] == "Wreck":
        cells[2] = cells[3] = "not drawn (derelicts are hulls)"
    out(f"| {s['name']} | {triple(color, 2)} {hexcolor(color)} | `Models.json:{line_of[s['name']]}` `sets[{index}].color` | {meshes_note} | "
        f"{roles[s['name']]} | " + " | ".join(cells) + " |")
  out("")
  out("Checks against the ADRs: the Human ship line (0.473, 0.637, 1.000) and the Tarkan ship line (1.000, 0.531, 0.437) match "
      "ADR-040 decision 1's (0.47, 0.64, 1.0) and (1.0, 0.53, 0.44). Tinted structures: Relay = the Research Lab's L1 mesh at "
      "tint 1.6 (`Models.json:70`), Repair Bay = the Shipyard's L1 mesh at tint 0.6 (`Models.json:71`); the tint multiplies the "
      "set's color before `TowardGray` (`GameClient.cpp:344-349, 1577`), so a Human Relay's color is (0.288, 0.672, 1.000) "
      "before graying, its blue clamped. Each has no levels in `Tuning.json` (`structures[5]`, `structures[6]`), so it draws its "
      "model's L1 (`DrawnLevel`, `GameApp/ModelCatalog.h:158-161`).")
  out("")
  out("**Which models each set renders, and why:**")
  out("")
  out("- **Human, Tarkan**: all 9 models of the set, the three levelled ones as 5 levels each (21 meshes), plus **Relay** and "
      "**Repair Bay** (tinted, above): 23 entries each.")
  out("- **Pirate**: Tarkan's 21 meshes in the Pirate violet, since `\"meshes\": \"Tarkan\"` gives the set all of Tarkan's models "
      "(`ModelCatalog.cpp:223-230`) and `LoadClientAssets` loads every one. *In play* the simulation spawns pirates only as "
      "Defense Platforms (`GameLogic/Simulation.cpp:843`) and ships (`Simulation.cpp:868`) of hull 1 and hull 2 (`Tuning.json` "
      "`pirates.outposts[].ships`); no other spawn site in `GameLogic/` gives `PIRATES` anything, so the pirates' Command "
      "Station, Shipyard, Research Lab, Mining Rig, Large hull and Constructor are loaded but never drawn in a match. No Relay or Repair Bay for Pirate (only players build them).")
  out("- **Wreck**: only the three hulls, Small, Medium, Large, from the Human meshes in gray: a derelict is drawn by its hull's "
      "model (`GameClient.cpp:1628-1639`, ADR-074, ADR-011 decision 3), although `\"meshes\": \"Human\"` gives the Wreck set all "
      "21 Human meshes (and loads them). A derelict is drawn with ship colors (`FILL_SHADE`), scaled to `2 × radius / lengthMeters`.")
  out("- **Asteroids**: Small, Medium, Large, in the ore-asteroid look.")
  out("")

  # ── Per-model table ──
  out("## Per-model table")
  out("")
  out(f"Every row has two renders, `<Set>/<Model>_34.png` (3/4 view) and `<Set>/<Model>_top.png` (top view), 1024×1024 RGBA, "
      f"and their web copies `_34_512.png` and `_top_512.png`, all under `DesignReference/_work/assets/models/`; **every one is an "
      f"{APPROX}.** Triangles are `indexCount / 3` of the `.nmf`; vertices its `vertexCount`. Bounds in mesh units are the raw "
      "`.nmf` min → max (x front, y up, z left: `Tools/BakeMeshes.py` docstring, `Hardpoints.h:20`); fitted bounds in meters are "
      "after `FitMesh`/`FitMeshAcross` (centered, so ± half the extent). *Drawn in game* is the size on the map: a ship at its "
      "fitted size (`ModelPose.scale` = 1, `Hardpoints.h:28`); a structure scaled by `2 × footprintRadiusMeters / lengthMeters` "
      "(`GameClient.cpp:1661-1664`, radius from `Tuning.json` via `Simulation.cpp:1069`); a derelict by `2 × radius / "
      "lengthMeters` of that derelict; a rock by its radius (2 m mesh). Crease lines are this tool's replay of "
      "`BuildCreaseLines` at 10°, summed over pieces.")
  out("")
  out("| Set | Model | Look | Mesh (`OutpostCommander/Assets/Models/…`) | Kind | Length m (key) | Tris | Verts | Bounds, mesh units min → max | Fitted, m (x × y × z) | Drawn in game, m (x × y × z) | Hardpoints | Spinning parts | Crease lines |")
  out("|---|---|---|---|---|---|---|---|---|---|---|---|---|---|")
  for st in stats:
    mesh = st["nmf"].replace("OutpostCommander/Assets/Models/", "")
    extents = st["fittedExtentsMeters"]
    if st["kind"] == "structure":
      kind_name = st["entry"] if st["variantOf"] else structure_kind_of_model.get(st["model"], st["model"])
      k_index, radius = footprint[kind_name]
      scale = 2.0 * radius / st["lengthMeters"]
      drawn = f"{extents[0] * scale:.1f} × {extents[1] * scale:.1f} × {extents[2] * scale:.1f} (×{scale:.3g}; `Tuning.json: structures[{k_index}].footprintRadiusMeters` = {radius})"
    elif st["set"] == models_json["derelicts"]:
      drawn = "× 2·radius/length of each derelict"
    elif st["kind"] == "rock":
      drawn = "× the rock's radius"
    else:
      drawn = f"{extents[0]:.1f} × {extents[1]:.1f} × {extents[2]:.1f} (×1)"
    hardpoints = ", ".join(f"`{tag}` ×{count}" for tag, count in sorted(st["hardpoints"].items())) or "none"
    parts = "; ".join(f"{p['triangles']} tris, {p['periodSeconds']:g} s about {fmt3(p['axis'], 0)}" for p in st["parts"]) or "0"
    if st["parts"]:
      parts = f"{len(st['parts'])}: " + parts
    low, high = st["boundsMin"], st["boundsMax"]
    model_name = st["entry"] + (f" ({st['variantOf']} mesh, tint {st['tint']:g}, `{st['tintKey'].split(': ')[1]}`)" if st["variantOf"] else "")
    kind = st["kind"] + (", derelict" if st["set"] == models_json["derelicts"] else "")
    fit_note = "" if st["fitRule"].startswith("FitMesh(") else f", across L1's widest {st['firstLevelWidestMeters']:.2f} m"
    out(f"| {st['set']} | {model_name} | {APPROX} | `{mesh}` | {kind} | {st['lengthMeters']:g} (`{st['lengthKey'].split(': ', 1)[1]}`) | "
        f"{st['triangles']} | {st['vertexCount']} | {fmt3(low)} → {fmt3(high)} | {extents[0]:.2f} × {extents[1]:.2f} × {extents[2]:.2f}{fit_note} | "
        f"{drawn} | {hardpoints} | {parts} | {st['creaseLines']} |")
  out("")
  out("Hardpoint tags: `gun` is where a shot leaves, `exhaust` where an engine's exhaust leaves (`GameApp/Hardpoints.cpp:9-10, "
      "45-52`); the game refuses a mesh with any other tag (`ModelCatalog.cpp:376-380`). Every spinning part turns about the "
      "vertical axis; a negative y axis turns the other way (Tarkan's counter ring). Periods match ADR-045's Context: Human "
      "radar 8 s, L5 relays 5 s; Tarkan tilted ring 12 s, L3–L5 counter ring 9 s.")
  out("")

  # ── Mesh files ──
  out("## Mesh files, hashes and the bake check")
  out("")
  out(f"`{bake['command']}` (read-only) → exit {bake['exitCode']}, \"{bake['output']}\" ({bake['wallSeconds']} s on "
      f"{bake['cores']} cores, {bake['ranOn']}). So every one of the 45 `.nmf` files below is byte for byte what its GLB bakes "
      "to, and every `.nmf` has its source. The GLBs hold 16,316 triangles in all, the same as the `.nmf` files (counted here from "
      "each GLB's index accessors by node).")
  out("")
  out("| Mesh | Tris | NMF bytes | NMF sha256 | GLB (`Art/Models/…`) | GLB bytes | GLB sha256 | `--check` |")
  out("|---|---|---|---|---|---|---|---|")
  seen = set()
  for st in stats:
    if st["nmf"] in seen:
      continue
    seen.add(st["nmf"])
    out(f"| `{st['nmf'].replace('OutpostCommander/Assets/Models/', '')}` | {st['triangles']} | {st['nmfBytes']} | `{st['nmfSha256']}` | "
        f"`{st['glb'].replace('Art/Models/', '')}` | {st['glbBytes']} | `{st['glbSha256']}` | current |")
  out("")
  total_vertices = sum(s["vertexCount"] for s in stats if s["nmf"] in seen and s["variantOf"] is None and s["set"] in ("Human", "Tarkan", "Asteroids"))
  out(f"Totals over the 45 `.nmf` files: {total_vertices:,} vertices, 16,316 triangles, 1,571,696 bytes; mean openness 0.649, "
      "25.7 % of vertices wholly open, 29.5 % less than half open, 12.7 % shut in wholly. These match ADR-092's Consequences "
      "(48,948; 16,316; 1,571,696 bytes; 0.65, 26 %, 30 %, 13 %).")
  out("")

  # ── Duplicates ──
  out("## Duplicates")
  out("")
  out("- **No two files are byte-identical**: sha256 over the 45 `.nmf` and the 45 `.glb` files gives 90 distinct hashes "
      "(the table above). The Tarkan `CommandStation_L1`–`L3` GLBs are all 17,376 bytes, and differ.")
  out("- **Pirate and Wreck have no files of their own**: they read Tarkan's and Human's folders by name "
      "(`Models.json` `\"meshes\"`; `ModelFileName`, `ModelCatalog.cpp:362-369`; ADR-011 decision 4: \"the pirates' meshes are "
      "loaded a second time, in their own color\"). So `Pirate/<Model>` renders are Tarkan's meshes, and `Wreck/<Hull>` Human's.")
  out("- **Human and Tarkan share nothing**: every same-named pair differs in triangle count (table above).")
  out("- **Levels that share a topology** (same index buffer, some vertices moved), measured by comparing the `.nmf` arrays: Human "
      "`CommandStation_L1`/`L2`/`L3` (1,560 vertices; L2 and L3 longer along x by ×1.106 and ×1.213); Tarkan `CommandStation_L1`/`L2`/`L3` "
      "(132 of 654 vertices move) and `L4`/`L5` (132 of 708); Tarkan `ResearchLab_L1`/`L2` (24 of 720) and `L3`/`L4` (212 of 1,008); "
      "Tarkan `Shipyard_L4`/`L5` (624 of 1,404). Each level is still a different shape, and its occlusion differs.")
  out("- **Art ↔ Assets**: 45 GLBs, 45 NMFs, one to one by set folder and name, all current by `--check` (above).")
  out("")

  # ── Sheets ──
  out("## Contact sheets")
  out("")
  out(f"All on the game's near-black (#05060a), with the glow added whole; every sheet is an {APPROX}.")
  out("")
  out("| Sheet | What | Size |")
  out("|---|---|---|")
  for record in records:
    if record["kind"] in ("set sheet", "level sheet"):
      what = (f"every {record['set']} entry, 3/4 and top view, each framed alone (not to scale)" if record["kind"] == "set sheet" else
              f"{record['set']} {record['model']} L1 → L5, 3/4 row and top row, one frame for all five (to scale with each other)")
      out(f"| `{record['file'].replace('DesignReference/_work/', '')}` | {what} | {record['size'][0]}×{record['size'][1]} |")
  out("")
  out("The Pirate level sheets exist because the Pirate set carries Tarkan's levelled models (`Models.json`); pirates never own "
      "those structures in play (above).")
  out("")

  # ── Textures ──
  out("## Textures")
  out("")
  out("All five are uncompressed 32-bit DDS, decoded here from the header's masks and checked against Pillow's DDS reader "
      "(identical for all five). Outputs under `DesignReference/_work/assets/textures/`: `<name>.png` (texels as stored, RGBA), "
      "`<name>_on_dark.png` (alpha-blended over a dark checker, enlarged nearest-neighbour), `<name>_alpha.png` (alpha as gray, "
      "enlarged).")
  out("")
  out("| Texture | Size | Mips | Format | Bytes | sha256 | Content | Used by |")
  out("|---|---|---|---|---|---|---|---|")
  used = {
    "Particle.dds": "**the game**: every particle's sprite, `GameClient.cpp:14` `PARTICLE_SPRITE_FILE`, loaded `GameClient.cpp:395-397`, mips built at load; added ONE+ONE, its alpha ignored, its RGB taken from sRGB with pow 2.2 (`GlowSpritePS.hlsl`, `GlowPipeline.cpp:72-76`; ADR-026)",
    "starburst.dds": "no game code; read by `NeuronClientTests/TextureDataTests.cpp:91-105`; packaged as content (`OutpostCommander.vcxproj:171`)",
    "glow.dds": "no code reads it; packaged as an `Image` item (`OutpostCommander.vcxproj:123`)",
    "cloudyglow.dds": "no code reads it; packaged as an `Image` item (`OutpostCommander.vcxproj:122`)",
    "laser.dds": "no code reads it; packaged as an `Image` item (`OutpostCommander.vcxproj:124`)",
  }
  for t in textures["textures"]:
    name = Path(t["file"]).name
    if t["alphaMin"] == 255:
      content = f"gray RGB {t['rgbMin'][0]}–{t['rgbMax'][0]}, alpha 255 everywhere (a flat square with a brighter rim)"
    else:
      content = f"RGB white everywhere; the shape is in alpha, {t['alphaMin']}–{t['alphaMax']}"
    out(f"| `{name}` | {t['width']}×{t['height']} | {t['mipCount']} | B8G8R8A8 (A8R8G8B8 masks), uncompressed | {t['bytes']} | `{t['sha256']}` | {content} | {used[name]} |")
  out("")

  # ── Logos ──
  out("## App logos")
  out("")
  out("Copied as they are to `DesignReference/_work/assets/logos/`, plus `_logos_on_dark_and_white.png`, which shows each on "
      "near-black and on white with its extent outlined. **Every logo is a placeholder, not game art**: a single gray, RGB "
      "(191, 191, 191), with the shape in alpha alone — an outlined square with a cross through it. They are the package's "
      "tile, store and splash images (`OutpostCommander/Package.appxmanifest:20, 40-43`; `OutpostCommander.vcxproj:116-121, 125`).")
  out("")
  out("| Logo | Size | Bytes | sha256 |")
  out("|---|---|---|---|")
  for logo in textures["logos"]:
    out(f"| `{logo['file'].replace('OutpostCommander/Assets/', '')}` | {logo['size'][0]}×{logo['size'][1]} | {logo['bytes']} | `{logo['sha256']}` |")
  out("")

  # ── Counts ──
  model_renders = [r for r in records if r["kind"] == "model render"]
  web = [r for r in records if r["kind"] == "model render, web size"]
  sheets = [r for r in records if r["kind"] in ("set sheet", "level sheet")]
  out("## Counts")
  out("")
  out(f"- {len(stats)} set/model entries ({len({s['nmf'] for s in stats})} distinct meshes), {len(model_renders)} renders at 1024 "
      f"and {len(web)} at 512, {len(sheets)} sheets ({sum(1 for r in sheets if r['kind'] == 'set sheet')} set, "
      f"{sum(1 for r in sheets if r['kind'] == 'level sheet')} level), {len(textures['textures'])} textures (×3 images), "
      f"{len(textures['logos'])} logos (+1 sheet). `renders.json` lists all {len(records)} images with set, model, view and "
      "size" + (f"; **missing on disk: {missing}**." if missing else "; none is missing on disk."))
  out("- Render time: 236 renders (146 for the models, 90 unlabelled tiles for the level sheets, since deleted) in 470 s with "
      "four processes, about 7–10 s of CPU each.")
  out("")

  # Structures drawn wider than their footprint diameter by more than 10 %.
  wide = []
  for st in stats:
    if st["kind"] != "structure" or st["set"] not in ("Human", "Tarkan"):
      continue
    kind_name = st["entry"] if st["variantOf"] else structure_kind_of_model.get(st["model"], st["model"])
    _, radius = footprint[kind_name]
    scale = 2.0 * radius / st["lengthMeters"]
    widest = max(st["fittedExtentsMeters"][0], st["fittedExtentsMeters"][2]) * scale
    if widest > 1.1 * 2.0 * radius:
      wide.append(f"{st['set']} {st['entry']} {widest:.0f} m against {2 * radius:g} m")
  oversize = "; ".join(wide) if wide else "none"

  # ── Open / unclear ──
  out("## Open / unclear")
  out("")
  for item in [
    "**The brief's GLB path.** `OutpostCommander/Assets/Models/` holds `.nmf` files, not GLBs; rendered from the `.nmf` (above).",
    "**Wreck renders only the hulls.** `Models.json` gives the Wreck set all 21 Human meshes (and the game loads them all), but only the three hull models are ever drawn as derelicts (`GameClient.cpp:1628-1639`). If the lead wants all 21 in gray, `render_all.py` drops them at `entries()` (one condition).",
    "**Pirate structures other than the Defense Platform** are rendered because the data loads them, but no spawn site in `GameLogic/` gives them to `PIRATES` (`Simulation.cpp:843` Defense Platforms, `Simulation.cpp:868` ships of the hulls `Tuning.json` `pirates.outposts` names, 1 and 2; the stress and measurement loads spawn for players 1 and 2 only). Confirming that no other path can is A1/A2's.",
    "**The look is approximate**: line width (2 px against 1 px), orthographic against a 45° perspective, 2×2 supersampling against 4× MSAA, and the glow replayed on a catalogue-sized image rather than at the game's screen scale, so it reads stronger relative to the model than in the game at the default zoom. No in-game capture exists to compare against; the owner's run is the reference.",
    "**Field rocks** are darker than an ore asteroid (`FIELD_SHADE` 0.7, `GameClient.cpp:1522`); only the ore-asteroid look is rendered.",
    "**Mining Rig** is rendered alone; in the game it stands tilted on its rock's surface (ADR-044, `RigStance`, `GameClient.cpp:1718-`).",
    "**Structures are drawn larger than `lengthMeters`.** A structure is drawn across its footprint, `2 × footprintRadiusMeters` (`GameClient.cpp:1661-1664`), so e.g. a Command Station L1 is 90 m long on the map against `lengthMeters` 60, a Mining Rig 50 m against 4, a Research Lab 60 m against 12. The *Drawn in game* column gives each; ADR-011 decision 2 calls `lengthMeters` how big a model is, which holds for ships but not for structures. Recorded for A2/A5, not as a conflict: the code is explicit.",
    "**Some structures reach well past their footprint.** Fitted along x only (`FitMesh`) and then drawn across the footprint, a model whose depth (z) exceeds its length is wider on the map than its footprint diameter. Over 10 % past it, computed from the table: " + oversize + ". The Human Command Station's arms run along z, so it spans about 181 m against its 90 m footprint at every level. Whether that is intended is the owner's; recorded for A2/A5.",
    "**Logos are placeholders** (above): if the reference shows a logo, there is none to show.",
    "**Three of the five textures are referenced by no code** (`glow`, `cloudyglow`, `laser`), and `starburst` only by a test; they are in the package all the same.",
  ]:
    out(f"- {item}")
  out("")

  # ── Conflicts ──
  out("## Conflicts")
  out("")
  out("Each shows both sides with file:line; the code/data side is what the renders follow.")
  out("")
  out("| # | Doc / ADR says | Code / data says | Note |")
  out("|---|---|---|---|")
  conflicts = [
    ("`GameDesign/ImplementationPlan-Look.md:32` (finding 5): \"Every source today has exactly one material, `team`, and no vertex colors or texture coordinates.\"",
     "`Art/Models/Asteroids/Small.glb`, `Medium.glb`, `Large.glb` have **no material** (no `materials` in their glTF JSON); the 42 others have exactly one, `team`. None has texture coordinates or vertex colors (attributes POSITION and NORMAL only, checked on all 45).",
     "Not visible to the player: materials are not baked (`Tools/BakeMeshes.py` docstring)."),
    ("`Design/ADR/ADR-027-rock-crease-lines.md:19`: \"The new models have 86 to 277 creases a ship and 249 to 736 a structure.\"",
     "Replaying `BuildCreaseLines` at 10° (`MeshData.cpp:289-397`, `GameClient.cpp:49`) on HEAD's `.nmf` files: ships 74 (Human `Small`) to 259 (Human `Large`); structures 108 (Tarkan `DefensePlatform`) to 1,198 (Human `Shipyard_L5`). Per-model counts in the table above.",
     "The ADR's figure predates the levelled structure sources (ADR-045, 2026-10-03) and later hull edits; it is a measurement of older meshes, not a decision."),
    ("`Design/ADR/ADR-011-meshes-and-shading.md:9` (Context): \"The Tarkan `Medium` is longer than the Tarkan `Large`.\"",
     "Raw `.nmf` x extents: Tarkan `Medium` 35.0, `Large` 60.0 mesh units (`stats.json`); fitted lengths 35 m and 60 m (`Models.json:23-24`).",
     "Context describing the meshes of the time; the fitting it motivated still holds. Not player-visible."),
    ("`GameDesign/ImplementationPlan-Look.md:47`: LV2.1 \"built; awaiting CI and the owner's run\".",
     "The shipped `.nmf` files are version 3 with the baked `open` byte, `NMF_VERSION = 3` (`Tools/BakeMeshes.py:76`, `NeuronClient/MeshData.cpp:19`), and `--check` passes: LV2.1 is in the code at HEAD.",
     "A plan's status lagging the code; the renders include the occlusion."),
  ]
  for i, (a, b, c) in enumerate(conflicts, 1):
    out(f"| C{i} | {a} | {b} | {c} |")
  out("")
  out("No conflict found between the shaders, `SceneShading.h` and ADR-090/091/092/040 on any number the renders use; ADR-092's "
      "mesh totals, ADR-045's spin periods, ADR-027's rock triangle counts (32, 54, 78) and ADR-011's hull lengths (1.25 × the "
      "footprint diameters 16, 28, 48 m of `Tuning.json` `hulls[].footprintRadiusMeters` 8, 14, 24) match the files.")
  out("")
  (A6 / "A6.md").write_text("\n".join(lines) + "\n")
  print(f"A6.md written, {len(records)} images listed, missing {missing}")


if __name__ == "__main__":
  main()

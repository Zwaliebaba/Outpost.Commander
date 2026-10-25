"""Builds the Design canvas for the Game Design Reference from DesignReference/_work.

  python3 DesignReference/compose/build.py <root> [--assets <assets.json>] [--heights <heights.json>]

writes <root>/project/canvas.json and one .dc.html per artboard. Without --assets, images point at the local files,
so the pages can be measured; --heights takes the measured heights (measure.cjs) for the artboards' frames.
"""
import json
import math
import re
import sys
from pathlib import Path

from lib import (ANNOTATION, C, DISPLAY, HEAD, LABEL, MANIFEST, MONO, READ, ROOT, TOKENS, WORK, chapter_head, color, corners,
                 demote, doc, esc, footer_note, gem, hatch, md, ore, page_shell, panel, read, section)
import screens

ASSET_DIR = WORK / "assets"
TUNING = json.loads((ROOT / "OutpostCommander" / "Assets" / "Tuning.json").read_text())
MAP = json.loads((ROOT / "OutpostCommander" / "Assets" / "Map.json").read_text())
MODELS = json.loads((ROOT / "OutpostCommander" / "Assets" / "Models.json").read_text())

PAGES = [
  ("p01", "01 Cover"), ("p02", "02 The game"), ("p03", "03 Sides"), ("p04", "04 Economy & territory"),
  ("p05", "05 Structures"), ("p06", "06 Ships & the Designer"), ("p07", "07 Research"), ("p08", "08 Combat"),
  ("p09", "09 Orders & automation"), ("p10", "10 AI & difficulty"), ("p11", "11 The persistent world"),
  ("p12", "12 Visual language"), ("p13", "13 Screens"), ("p14", "14 The battlefield"), ("p15", "15 Asset catalogue"),
  ("p16", "16 Controls"), ("p17", "17 Open items"),
]

ASSETS = {}
USED = set()


def img(key):
  """An image's address: the canvas's upload when published, else the local file for measuring."""
  USED.add(key)
  if key in ASSETS:
    return ASSETS[key]
  return (ASSET_DIR / key).resolve().as_uri() if not key.startswith("/") else Path(key).as_uri()


BOARDS = []  # (file, page, title, width, body)


def board(name, page, title, width, body):
  BOARDS.append((f"{name}.dc.html", page, title, width, body))


def sec(rel, heading, demote_by=1, **kw):
  return md(demote(section(rel, heading, **kw), demote_by))


def sections(rel, headings, demote_by=1):
  return "".join(sec(rel, h, demote_by) for h in headings)


def table_parts(text, parts):
  """Splits the first markdown table in `text` into `parts` tables, each with the header; prose before goes with the
  first, prose after with the last."""
  lines = text.splitlines()
  start = next(i for i, l in enumerate(lines) if l.startswith("|"))
  end = start
  while end < len(lines) and lines[end].startswith("|"):
    end += 1
  head, rows = lines[start:start + 2], lines[start + 2:end]
  size = math.ceil(len(rows) / parts)
  out = []
  for k in range(parts):
    chunk = head + rows[k * size:(k + 1) * size]
    pre = "\n".join(lines[:start]) + "\n" if k == 0 else ""
    post = "\n" + "\n".join(lines[end:]) if k == parts - 1 else ""
    out.append(pre + "\n".join(chunk) + "\n" + post)
  return out


def split_at(text, parts, pattern=r"^#{2,3} "):
  """Splits markdown into `parts` chunks of about equal length, each starting at a heading matching `pattern`."""
  starts = [m.start() for m in re.finditer(pattern, text, flags=re.M)]
  target = len(text) / parts
  cuts, next_cut = [0], target
  for st in starts:
    if st >= next_cut and len(cuts) < parts:
      cuts.append(st)
      next_cut = st + target
  cuts.append(len(text))
  return [text[a:b] for a, b in zip(cuts, cuts[1:])]


def card(title, inner, label=None):
  return panel(title, inner, label)


# ---------------------------------------------------------------------------------------------------- diagrams

STEP = re.compile(r"^\s*(\d+)\.\s+(.*)$")


def steps_of(markdown_text):
  """An ordered list's items as (number, title, body): the bold lead or a leading code name is the title; the body keeps
  the item's own words and its sources, inline."""
  out = []
  for line in markdown_text.splitlines():
    m = STEP.match(line)
    sub = re.match(r"^\s+[-*]\s+(.*)$", line)
    if not m:
      if sub and out:
        n, title, body, srcs = out[-1]
        out[-1] = (n, title, body + "  \n– " + sub.group(1), srcs)
      continue
    rest = m.group(2).strip()
    b = re.match(r"^\*\*(.+?)\*\*[:.]?\s*(.*)$", rest)
    c = re.match(r"^`([^`]+)`\s*(?:—|-)?\s*(.*)$", rest)
    if b:
      title, body = b.group(1).rstrip(":.").strip(), b.group(2)
    elif c:
      title, body = c.group(1), c.group(2)
    else:
      words = re.split(r"[;,—]", re.sub(r"`[^`]+`", "", rest))[0].split()
      title, body = " ".join(words[:6]), rest
    out.append((m.group(1), title, body, []))
  return out


def flow(steps, columns=4, box_h=None):
  """Numbered steps as boxes joined by arrows, row after row (a reading-order flow)."""
  cells = []
  for i, (n, title, text, srcs) in enumerate(steps):
    arrow = ""
    last_in_row = (i % columns == columns - 1)
    if i < len(steps) - 1:
      if last_in_row:
        arrow = (f'<div aria-hidden="true" style="position:absolute;left:50%;bottom:-22px;width:0;height:0;border-left:7px solid transparent;'
                 f'border-right:7px solid transparent;border-top:10px solid {C["WINDOW_EDGE_COLOR"]};transform:translateX(-50%)"></div>')
      else:
        arrow = (f'<div aria-hidden="true" style="position:absolute;right:-19px;top:22px;width:0;height:0;border-top:7px solid transparent;'
                 f'border-bottom:7px solid transparent;border-left:10px solid {C["WINDOW_EDGE_COLOR"]}"></div>')
    cells.append(
      f'<div style="position:relative;box-sizing:border-box;padding:14px 16px 14px 16px;background:{corners(C["WINDOW_EDGE_COLOR"])},{C["WINDOW_COLOR"]};'
      f'border:1px solid {C["EDGE_COLOR"]}">'
      f'<div style="display:flex;gap:10px;align-items:baseline"><span style="font-family:{MONO};font-weight:700;font-size:20px;color:{C["ACCENT_COLOR"]}">{esc(n)}</span>'
      f'<span style="font-family:{LABEL};font-weight:600;font-size:16px;color:{C["TEXT_COLOR"]}">{esc(title)}</span></div>'
      f'<div class="md" style="margin-top:6px;font-size:13.5px;line-height:1.5;color:{C["NUMBERS_COLOR"]}">{md_inline(text)}</div>{arrow}</div>')
  return (f'<div style="display:grid;grid-template-columns:repeat({columns},1fr);column-gap:28px;row-gap:30px">' + "".join(cells) + "</div>")


def md_inline(text):
  out = md(text)
  return out.replace('<div class="md"><p>', "").replace("</p></div>", "")


def core_loop():
  """MVP.md:40 names the loop: mine, build, research, design, fight. Each node says what it is as built (A1's match loop)."""
  nodes = [
    ("Mine", "Constructors build Mining Rigs on ore asteroids in held sectors; income flows each tick.", "A1 match loop 4; Simulation.cpp:3003-3027"),
    ("Build", "Relays claim sectors; Shipyard, Research Lab, Defence Platforms and Repair Bay; paid at the start.", "A1 match loop 5-6; Simulation.cpp:2145-2185"),
    ("Research", "Topics within the Lab's open tier; structures upgraded a level at a time.", "A1 match loop 8; ADR-064 §Decision 9-11"),
    ("Design", "Warships from saved designs: a hull, a drive, a weapon and an optional module.", "A1 match loop 9; ADR-014; ADR-058"),
    ("Fight", "Suppress and destroy Relays, defend sectors; damaged ships retreat to be repaired.", "A1 match loop 10; ADR-056 §Decision 6; ADR-075 §Decision 5"),
  ]
  cx, cy, r = 420, 330, 230
  parts = []
  pos = []
  for i in range(len(nodes)):
    a = -math.pi / 2 + i * 2 * math.pi / len(nodes)
    pos.append((cx + r * math.cos(a), cy + r * math.sin(a)))
  arrows = []
  for i in range(len(nodes)):
    x0, y0 = pos[i]
    x1, y1 = pos[(i + 1) % len(nodes)]
    dx, dy = x1 - x0, y1 - y0
    d = math.hypot(dx, dy)
    ux, uy = dx / d, dy / d
    sx, sy = x0 + ux * 80, y0 + uy * 80
    ex, ey = x1 - ux * 84, y1 - uy * 84
    arrows.append(f'<line x1="{sx:.1f}" y1="{sy:.1f}" x2="{ex:.1f}" y2="{ey:.1f}" stroke="{C["WINDOW_EDGE_COLOR"]}" stroke-width="2" marker-end="url(#ah)"/>')
  svg = (f'<svg role="img" aria-label="The core loop: mine, build, research, design, fight" width="840" height="660" viewBox="0 0 840 660" style="display:block">'
         f'<defs><marker id="ah" markerWidth="10" markerHeight="10" refX="8" refY="5" orient="auto"><path d="M0,0 L10,5 L0,10 z" fill="{C["WINDOW_EDGE_COLOR"]}"/></marker></defs>'
         + "".join(arrows))
  for (title, text, src), (x, y) in zip(nodes, pos):
    svg += (f'<circle cx="{x:.1f}" cy="{y:.1f}" r="74" fill="{C["WINDOW_COLOR"]}" stroke="{C["WINDOW_EDGE_COLOR"]}" stroke-width="1.5"/>'
            f'<text x="{x:.1f}" y="{y + 7:.1f}" text-anchor="middle" style="font-family:{DISPLAY};font-weight:700;font-size:24px" fill="{C["TEXT_COLOR"]}">{esc(title)}</text>')
  svg += (f'<text x="{cx}" y="{cy - 4}" text-anchor="middle" style="font-family:{LABEL};font-weight:600;font-size:13px;letter-spacing:2px" fill="{C["LABEL_COLOR"]}">THE LOOP</text>'
          f'<text x="{cx}" y="{cy + 16}" text-anchor="middle" style="font-family:{MONO};font-size:12px" fill="{C["LABEL_COLOR"]}">MVP.md:40</text></svg>')
  legend = "".join(
    f'<li style="margin:0 0 12px 0"><span style="font-family:{LABEL};font-weight:600;font-size:16px;color:{C["TEXT_COLOR"]}">{esc(t)}.</span> '
    f'<span style="color:{C["NUMBERS_COLOR"]}">{esc(x)}</span><br><code>{esc(s)}</code></li>' for t, x, s in nodes)
  return (f'<div style="display:flex;gap:40px;align-items:center">{svg}<div class="md" style="flex:1"><blockquote>Is the loop — mine, build, research, design, fight — fun? '
          f'<code>MVP.md:40</code> (the MVP\'s question Q1)</blockquote><ul style="list-style:none;padding:0">{legend}</ul></div></div>')


# ---------------------------------------------------------------------------------------------------- the map


def map_svg():
  size = MAP["sizeMeters"]
  px = 760
  k = px / size

  def P(x, z):  # +x right, +z up (Hud.cpp:2432-2451)
    return (x + size / 2) * k, (size / 2 - z) * k

  kinds = {}
  out = []
  for s in MAP["sectors"]:
    x0, y0 = P(s["minXMeters"], s["maxZMeters"])
    x1, y1 = P(s["maxXMeters"], s["minZMeters"])
    kinds.setdefault(s["kind"], 0)
    kinds[s["kind"]] += 1
    out.append(f'<rect x="{x0:.1f}" y="{y0:.1f}" width="{x1 - x0:.1f}" height="{y1 - y0:.1f}" fill="{C["MAP_COLOR"]}" stroke="{C["LATTICE_COLOR"]}" stroke-width="1"/>')
    out.append(f'<text x="{x0 + 6:.1f}" y="{y0 + 16:.1f}" style="font-family:{LABEL};font-weight:600;font-size:13px" fill="{C["ROW_LABEL_COLOR"]}">{esc(s["name"])}'
               f'<tspan dx="6" style="font-family:{MONO};font-weight:400;font-size:10.5px" fill="{C["LABEL_COLOR"]}">{esc(s["kind"])}</tspan></text>')
  by_id = {s["id"]: s for s in MAP["sectors"]}
  for s in MAP["sectors"]:
    for a in s["adjacent"]:
      if a > s["id"]:
        x0, y0 = P(s["node"]["xMeters"], s["node"]["zMeters"])
        n = by_id[a]["node"]
        x1, y1 = P(n["xMeters"], n["zMeters"])
        out.append(f'<line x1="{x0:.1f}" y1="{y0:.1f}" x2="{x1:.1f}" y2="{y1:.1f}" stroke="{C["LABEL_COLOR"]}" stroke-width="1" stroke-dasharray="3 4"/>')
  for f in MAP["asteroidFields"]:
    x, y = P(f["xMeters"], f["zMeters"])
    out.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{max(2, f["radiusMeters"] * k):.1f}" fill="{C["ASTEROID_FIELD_COLOR"]}" opacity="0.9"/>')
  for o in MAP["oreAsteroids"]:
    x, y = P(o["xMeters"], o["zMeters"])
    r = max(4, o["radiusMeters"] * k)
    out.append(f'<rect x="{x - r:.1f}" y="{y - r:.1f}" width="{2 * r:.1f}" height="{2 * r:.1f}" fill="none" stroke="{C["ORE_ASTEROID_COLOR"]}" stroke-width="2"/>')
  for s in MAP["sectors"]:
    x, y = P(s["node"]["xMeters"], s["node"]["zMeters"])
    out.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="4" fill="{C["TEXT_COLOR"]}"/>')
  for i, st in enumerate(MAP["starts"]):
    x, y = P(st["xMeters"], st["zMeters"])
    side = "OWN_COLOR" if i == 0 else "ENEMY_COLOR"
    out.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="11" fill="none" stroke="{C[side]}" stroke-width="2.5"/>')
    out.append(f'<text x="{x + 15:.1f}" y="{y - 10:.1f}" style="font-family:{LABEL};font-weight:600;font-size:12px" fill="{C[side]}">start {i + 1}</text>')
  svg = (f'<svg role="img" aria-label="Map.json: 25 sectors, nodes, adjacency, starts, home ore and asteroid fields" width="{px}" height="{px}" '
         f'viewBox="0 0 {px} {px}" style="display:block;background:{C["FIELD_COLOR"]}">' + "".join(out) + "</svg>")
  legend_rows = [
    (f'<span style="display:inline-block;width:14px;height:14px;background:{C["MAP_COLOR"]};border:1px solid {C["LATTICE_COLOR"]}"></span>',
     "Sector, 2,000 m square, its name and kind", "Map.json: sectors[]"),
    (f'<span style="display:inline-block;width:8px;height:8px;border-radius:4px;background:{C["TEXT_COLOR"]}"></span>', "Node, where a Relay claims the sector", "Map.json: sectors[].node; ADR-056"),
    (f'<span style="display:inline-block;width:22px;border-top:1px dashed {C["LABEL_COLOR"]}"></span>', "Adjacency: the lattice a claim grows along", "Map.json: sectors[].adjacent; ADR-036"),
    (f'<span style="display:inline-block;width:12px;height:12px;border:2px solid {C["ORE_ASTEROID_COLOR"]}"></span>', "Home ore asteroid (the six Map.json places)", "Map.json: oreAsteroids[]"),
    (f'<span style="display:inline-block;width:12px;height:12px;border-radius:6px;background:{C["ASTEROID_FIELD_COLOR"]}"></span>', "Asteroid field, at its radius", "Map.json: asteroidFields[] (80)"),
    (f'<span style="display:inline-block;width:14px;height:14px;border-radius:7px;border:2px solid {C["OWN_COLOR"]}"></span>', "Start 1 and start 2", "Map.json: starts[]"),
  ]
  legend = "".join(f'<tr><td style="width:30px">{a}</td><td>{esc(b)}</td><td><code>{esc(c)}</code></td></tr>' for a, b, c in legend_rows)
  counts = ", ".join(f"{v} {esc(k)}" for k, v in sorted(kinds.items()))
  note = (f'<p>Drawn from <code>Map.json</code> at HEAD: {MAP["sizeMeters"]:,} m square, {len(MAP["sectors"])} sectors ({counts}), '
          f'{len(MAP["asteroidFields"])} asteroid fields and {len(MAP["oreAsteroids"])} home ore asteroids. +x runs right and +z up, as the minimap draws it '
          f'(<code>Hud.cpp:2432-2451</code>). The other ore asteroids, the pirate outposts and the derelicts are placed by the match\'s seed by each sector\'s kind, '
          f'mirrored through the center (<code>ADR-072</code>, <code>Map.json: sectorKinds, outposts, derelicts, placement</code>), so no map can show them: '
          f'<strong>not drawn</strong>. Map colors are the minimap\'s tokens.</p>')
  return (f'<div style="display:flex;gap:36px;align-items:flex-start">{svg}<div class="md" style="flex:1">{note}<table><tbody>{legend}</tbody></table></div></div>')


# ---------------------------------------------------------------------------------------------------- research tree


def effect_text(eff):
  """A topic's effect as its Tuning.json fields read, with the names of the parts it points at."""
  names = {"unlockHull": "hulls", "unlockDrive": "drives", "unlockWeapon": "weapons", "unlockModule": "modules"}
  for key, table in names.items():
    if key in eff:
      part = next((p for p in TUNING[table] if p["id"] == eff[key]), None)
      return f"{key} {eff[key]}" + (f" · {part['name']}" if part else "")
  target = eff.get("upgrade", "")
  if "weapon" in eff:
    w = next((p for p in TUNING["weapons"] if p["id"] == eff["weapon"]), None)
    target += f" {eff['weapon']}" + (f" ({w['name']})" if w else "")
  if "structureWeapon" in eff:
    target += f" {eff['structureWeapon']}"
  return f"{target} · {eff.get('stat', '')} +{eff.get('percent', '')}%"


def research_tree():
  topics = TUNING["research"]
  by_id = {t["id"]: t for t in topics}
  tiers = sorted({t["tier"] for t in topics})
  lab = None
  for s in TUNING["structures"]:
    if s.get("id") == 3 or s.get("type") == "ResearchLab" or s.get("name") == "Research Lab":
      lab = s
  cols = []
  for tier in tiers:
    cards = []
    for t in [t for t in topics if t["tier"] == tier]:
      req = ", ".join(by_id[r]["name"] for r in t.get("requires", [])) or "—"
      eff_text = effect_text(t.get("effect", {}))
      locked = bool(t.get("requires"))
      face = (f'background:{hatch(C["LOCKED_HATCH_COLOR"], C["LOCKED_COLOR"])}' if locked else f'background:{C["CARD_COLOR"]}')
      cards.append(
        f'<div style="{face};border:1px solid {C["EDGE_COLOR"]};padding:10px 12px;margin:0 0 10px 0">'
        f'<div style="display:flex;justify-content:space-between;gap:8px"><span style="font-family:{LABEL};font-weight:600;font-size:15px;color:{C["TEXT_COLOR"]}">'
        f'{esc(t["name"])}</span>{ore(t["cost"], 13)}</div>'
        f'<div style="font-family:{MONO};font-size:12px;color:{C["NUMBERS_COLOR"]};margin-top:4px">{esc(eff_text)}</div>'
        f'<div style="font-family:{MONO};font-size:12px;color:{C["LABEL_COLOR"]};margin-top:4px">TIER {t["tier"]} · {t["researchSeconds"]} s · id {t["id"]}</div>'
        + (f'<div style="display:flex;gap:6px;align-items:center;margin-top:4px;font-family:{LABEL};font-weight:600;font-size:12px;letter-spacing:1px;color:{C["AMBER_COLOR"]}">'
           f'<span style="display:inline-block;width:9px;height:9px;border:1px solid {C["AMBER_COLOR"]}"></span>NEEDS · {esc(req.upper())}</div>' if locked else "")
        + f'<div style="font-family:{MONO};font-size:11px;color:{C["LABEL_COLOR"]};margin-top:4px">Tuning.json: research[{topics.index(t)}]</div></div>')
    cols.append(f'<div><div style="font-family:{LABEL};font-weight:600;font-size:13px;letter-spacing:2px;color:{C["LABEL_COLOR"]};margin:0 0 10px 0">TIER {tier}</div>{"".join(cards)}</div>')
  return f'<div style="display:grid;grid-template-columns:repeat({len(tiers)},1fr);gap:20px">{"".join(cols)}</div>'


# ---------------------------------------------------------------------------------------------------- visual language


def swatches(names, group):
  rows = []
  for n in names:
    entry = TOKENS[group][n] if n in TOKENS[group] else None
    if entry is None:
      continue
    hx = entry.get("hex") or "#000000"
    meaning = entry.get("meaning") or entry.get("role") or ""
    src = entry.get("source") or ""
    rows.append(
      f'<div style="display:flex;gap:14px;align-items:flex-start;padding:10px 0;border-bottom:1px solid {C["EDGE_COLOR"]}">'
      f'<div style="flex:0 0 64px;height:44px;background:{hx};border:1px solid {C["EDGE_COLOR"]}"></div>'
      f'<div style="flex:1"><div style="display:flex;gap:12px;align-items:baseline"><span style="font-family:{MONO};font-weight:600;font-size:13px;color:{C["TEXT_COLOR"]}">{esc(n)}</span>'
      f'<span style="font-family:{MONO};font-size:13px;color:{C["ACCENT_COLOR"]}">{esc(hx)}</span></div>'
      f'<div style="font-size:13px;line-height:1.4;color:{C["NUMBERS_COLOR"]}">{esc(meaning)}</div>'
      f'<div style="font-family:{MONO};font-size:11px;color:{C["LABEL_COLOR"]};margin-top:2px">{esc(src)}</div></div></div>')
  return f'<div style="display:grid;grid-template-columns:1fr 1fr;column-gap:36px">{"".join(rows)}</div>'


def type_scale():
  rows = []
  sample = {"Title": "Outpost Commander 1,240", "Label": "PERFORMANCE · QUEUE · 0 / 5", "Name": "Shipyards · BUILDING 2 · WAITING 0",
            "Figure": "+14.5/s  29 / 30", "LargeFigure": "34.4", "Detail": "220 HP · ARM 2 · 60m/s", "Body": "Rasterized, drawn by no text"}
  from lib import FACE
  for s in TOKENS["type"]["scale"]:
    face = s["face"]
    track = 2 if face == "Label" else 0
    text = sample.get(face, face)
    rows.append(
      f'<tr><td style="width:150px"><code>{esc(face)}</code></td><td style="width:90px">{esc(s["sizeRefUnits"])} units · {esc(s["weight"])}</td>'
      f'<td><span style="{FACE[face]};font-size:{s["sizeRefUnits"]}px;letter-spacing:{track}px;color:{C["TEXT_COLOR"]}">{esc(text)}</span>'
      f'<div style="margin-top:4px;font-size:12.5px;color:{C["NUMBERS_COLOR"]}">{esc(s["role"])}</div></td>'
      f'<td style="width:240px"><code>{esc(s["source"])}</code> {esc(s.get("decision", ""))}</td></tr>')
  return (f'<div class="md"><table><thead><tr><th>Face</th><th>Size</th><th>Specimen and role</th><th>Source</th></tr></thead><tbody>'
          + "".join(rows) + "</tbody></table></div>")


def btn(label, face, edge, text, key=None, cost=None, note=None, w=356, h=34):
  extra = ""
  if key:
    extra += (f'<span style="position:absolute;right:12px;top:7px;width:20px;height:20px;box-sizing:border-box;background:{C["FIELD_COLOR"]};'
              f'border:1px solid {C["ROW_LABEL_COLOR"]};font-family:{LABEL};font-weight:600;font-size:13px;line-height:18px;text-align:center;color:{C["ROW_LABEL_COLOR"]}">{key}</span>')
  if cost:
    extra += f'<span style="position:absolute;right:12px;top:7px">{ore(cost, 14) if text != C["LOCKED_TEXT_COLOR"] else ore(cost, 14).replace(C["GOLD_COLOR"], C["LOCKED_TEXT_COLOR"])}</span>'
  if note:
    extra += (f'<span style="position:absolute;right:12px;top:9px;font-family:{LABEL};font-weight:600;font-size:13px;color:{C["LABEL_COLOR"]}">{note}</span>')
  return (f'<div style="position:relative;width:{w}px;height:{h}px;box-sizing:border-box;background:{face};border:1px solid {edge}">'
          f'<span style="position:absolute;left:12px;top:6px;font-family:{LABEL};font-weight:600;font-size:16px;color:{text}">{esc(label)}</span>{extra}</div>')


def component_gallery():
  def item(name, drawing, rule, src):
    return (f'<div style="padding:16px 0;border-bottom:1px solid {C["EDGE_COLOR"]};display:grid;grid-template-columns:420px 1fr;gap:28px;align-items:start">'
            f'<div>{drawing}</div><div><div style="font-family:{LABEL};font-weight:600;font-size:16px;color:{C["TEXT_COLOR"]}">{esc(name)}</div>'
            f'<div style="font-size:13.5px;line-height:1.45;color:{C["NUMBERS_COLOR"]};margin-top:4px">{esc(rule)}</div>'
            f'<div style="font-family:{MONO};font-size:11.5px;color:{C["LABEL_COLOR"]};margin-top:4px">{esc(src)}</div></div></div>')

  window = (f'<div style="width:400px;box-sizing:border-box;background:{corners(C["WINDOW_EDGE_COLOR"])},{C["WINDOW_COLOR"]}">'
            f'<div style="display:flex;height:36px"><div style="flex:1;background:{hatch(C["TITLE_HATCH_COLOR"])};font-family:{LABEL};font-weight:600;font-size:13px;'
            f'letter-spacing:2px;line-height:36px;padding-left:24px;color:{C["HEADER_LABEL_COLOR"]}">PRODUCTION</div>'
            f'<div style="width:36px;background:{C["CLOSE_BOX_COLOR"]};text-align:center;font-family:{DISPLAY};font-weight:700;font-size:22px;line-height:36px;color:{C["TEXT_COLOR"]}">×</div></div>'
            f'<div style="height:90px"></div></div>')
  panel_ = (f'<div style="width:380px;height:44px;box-sizing:border-box;padding:0 12px;display:flex;align-items:center;justify-content:space-between;'
            f'background:{corners(C["WINDOW_EDGE_COLOR"])},{C["WINDOW_COLOR"]}">{ore("2,350", 20)}<span style="font-family:{MONO};font-weight:600;font-size:14px;color:{C["NUMBERS_COLOR"]}">+14.5/s</span></div>')
  chip = (f'<span style="display:inline-block;padding:0 4px;height:22px;line-height:22px;background:{C["WARNING_COLOR"]};font-family:{LABEL};font-weight:600;font-size:16px;color:{C["WINDOW_COLOR"]}">IDLE 1</span>'
          f'<span style="display:inline-block;margin-left:12px;padding:0 4px;height:22px;line-height:22px;background:{C["WARNING_COLOR"]};font-family:{MONO};font-weight:600;font-size:14px;color:{C["WINDOW_COLOR"]}">+0/s</span>')

  def part(name, cost, numbers, state):
    if state == "locked":
      face = f"background:{hatch(C['LOCKED_HATCH_COLOR'], C['LOCKED_COLOR'])}"
      tc, nc = C["LOCKED_TEXT_COLOR"], C["LOCKED_TEXT_COLOR"]
      edge = C["EDGE_COLOR"]
    elif state == "picked":
      face, tc, nc, edge = f"background:{C['PICKED_COLOR']}", C["TEXT_COLOR"], C["NUMBERS_COLOR"], C["PICKED_EDGE_COLOR"]
    else:
      face, tc, nc, edge = f"background:{C['CARD_COLOR']}", C["TEXT_COLOR"], C["NUMBERS_COLOR"], C["EDGE_COLOR"]
    lock = ""
    if state == "locked":
      lock = (f'<div style="position:absolute;left:12px;bottom:8px;display:flex;gap:6px;align-items:center;font-family:{LABEL};font-weight:600;font-size:12px;'
              f'letter-spacing:1px;color:{C["AMBER_COLOR"]}"><span style="width:9px;height:9px;border:1px solid {C["AMBER_COLOR"]}"></span>RESEARCH · LARGE HULL</div>')
    cost_html = ore(cost, 14) if state != "locked" else ore(cost, 14).replace(C["GOLD_COLOR"], C["LOCKED_TEXT_COLOR"])
    return (f'<div style="position:relative;width:200px;height:80px;box-sizing:border-box;{face};border:1px solid {edge};padding:8px 12px">'
            f'<div style="display:flex;justify-content:space-between"><span style="font-family:{LABEL};font-weight:600;font-size:16px;color:{tc}">{esc(name)}</span>{cost_html}</div>'
            f'<div style="font-family:{MONO};font-size:13px;color:{nc};margin-top:6px">{esc(numbers)}</div>{lock}</div>')

  cards = (f'<div style="display:flex;flex-wrap:wrap;gap:8px;width:410px">{part("Small", 96, "220 HP · ARM 2 · 60m/s", "picked")}'
           f'{part("Medium", 330, "500 HP · ARM 8 · 40m/s", "normal")}{part("Large", 900, "1,200 HP · ARM 14", "locked")}</div>')
  buttons = (f'<div style="display:flex;flex-direction:column;gap:6px">'
             f'{btn("Attack-move", C["PICKED_COLOR"], C["PICKED_EDGE_COLOR"], C["TEXT_COLOR"], key="A", w=380)}'
             f'{btn("Hold sector", C["CARD_COLOR"], C["HOVER_EDGE_COLOR"], C["TEXT_COLOR"], key="H", w=380)}'
             f'{btn("Shipyard", C["CARD_COLOR"], C["EDGE_COLOR"], C["TEXT_COLOR"], cost="300", w=380)}'
             f'{btn("Defence Platform", C["FIELD_COLOR"], C["EDGE_COLOR"], C["LOCKED_TEXT_COLOR"], cost="450", w=380)}'
             f'{btn("Research Lab", C["FIELD_COLOR"], C["EDGE_COLOR"], C["LOCKED_TEXT_COLOR"], note="ONE PER PLAYER", w=380)}</div>')
  bars = (f'<div style="display:flex;flex-direction:column;gap:10px">'
          f'<div style="display:flex;gap:4px">' + "".join(
            f'<span style="width:17px;height:10px;background:linear-gradient(90deg,{c} {p}%,{C["BAR_TRACK_COLOR"]} {p}%)"></span>' for c, p in
            [(C["HEALTH_GOOD_COLOR"], 100), (C["HEALTH_GOOD_COLOR"], 70), (C["HEALTH_FAIR_COLOR"], 45), (C["HEALTH_POOR_COLOR"], 20)]) + "</div>"
          f'<div style="width:356px;height:6px;background:linear-gradient(90deg,{C["BAR_FILL_COLOR"]} 62%,{C["BAR_TRACK_COLOR"]} 62%)"></div>'
          f'<div style="display:flex;gap:10px;align-items:center"><span style="display:block;width:32px;height:5px;background:linear-gradient(90deg,{C["HEALTH_GOOD_COLOR"]} 55%,#426391 55%)"></span>'
          f'<span style="display:block;width:32px;height:5px;background:linear-gradient(90deg,{C["BUILD_BAR_COLOR"]} 40%,#426391 40%)"></span>'
          f'<span style="font-size:12px;color:{C["LABEL_COLOR"]}">over entities: health, build</span></div></div>')
  retreat = (f'<div style="display:flex;gap:4px">'
             f'{btn("25%", C["PICKED_COLOR"], C["PICKED_EDGE_COLOR"], C["TEXT_COLOR"], w=116)}{btn("50%", C["CARD_COLOR"], C["EDGE_COLOR"], C["TEXT_COLOR"], w=116)}'
             f'{btn("Never", C["CARD_COLOR"], C["EDGE_COLOR"], C["TEXT_COLOR"], w=116)}</div>')
  queue = (f'<div style="position:relative;width:326px;height:48px;box-sizing:border-box;background:{hatch(C["QUEUE_HATCH_COLOR"], C["QUEUE_COLOR"])};border:1px solid {C["QUEUE_EDGE_COLOR"]}">'
           f'<span style="position:absolute;left:18px;top:2px;font-family:{DISPLAY};font-weight:700;font-size:22px;letter-spacing:2px;color:{C["QUEUE_TEXT_COLOR"]}">QUEUE</span>'
           f'<span style="position:absolute;left:18px;top:29px;font-family:{MONO};font-size:13px;color:{C["QUEUE_TEXT_COLOR"]}">30 s each</span>'
           f'<span style="position:absolute;right:16px;top:10px">{ore("261", 22)}</span></div>')
  keycap = (f'<span style="display:inline-block;width:20px;height:20px;box-sizing:border-box;background:{C["FIELD_COLOR"]};border:1px solid {C["ROW_LABEL_COLOR"]};'
            f'font-family:{LABEL};font-weight:600;font-size:13px;line-height:18px;text-align:center;color:{C["ROW_LABEL_COLOR"]}">P</span>'
            f'<span style="display:inline-block;margin-left:16px;width:10px;height:10px;border:1px solid {C["AMBER_COLOR"]}"></span>'
            f'<span style="display:inline-block;margin-left:16px">{gem(13)} {gem(8)}</span>')
  return "".join([
    item("Window chrome", window, "Opaque WINDOW_COLOR body; hatched title bar in TITLE_HATCH_COLOR that drags the window; close box in CLOSE_BOX_COLOR with ×; 12-unit corner brackets in WINDOW_EDGE_COLOR. The name in the Label face, tracking 2, HEADER_LABEL_COLOR.", "Hud.cpp:871-899, 852-867; ADR-031 d3; A5 Components"),
    item("HUD panel", panel_, "Every HUD panel takes the windows' look: WINDOW_COLOR body and the four corners.", "Hud.cpp:903-908; ADR-043 d1"),
    item("Warning chip", chip, "A warning is a chip: dark WINDOW_COLOR words on a WARNING_COLOR tag, 4 units wider than its words each side, 22 tall. Plain WARNING_COLOR text only for an invalid name or the menu notice.", "Hud.cpp:1155-1168; ADR-085 d1"),
    item("Part cards: picked, normal, locked", cards, "Picked: PICKED_COLOR face, PICKED_EDGE_COLOR edge. Normal: CARD_COLOR, EDGE_COLOR. Locked: LOCKED_COLOR hatched with LOCKED_HATCH_COLOR, words in LOCKED_TEXT_COLOR, and the research that unlocks it in AMBER_COLOR beside a checkbox. 220 × 80 (96 with a note) in the game; drawn narrower here.", "Hud.cpp:1429-1482; ADR-062 d4"),
    item("HUD buttons: picked, hovered, enabled, disabled", buttons, "Face PICKED_COLOR when selected, CARD_COLOR when enabled, FIELD_COLOR when disabled; edge PICKED_EDGE_COLOR, HOVER_EDGE_COLOR (hovered) or EDGE_COLOR; label in the Name face; a key cap, a cost (gem + figure) or a note at the right. No button has a pressed state: Not specified.", "Hud.cpp:1264-1323; ADR-043 d4; ADR-088 d1; A5 Open items"),
    item("Retreat as three choices", retreat, "Three cells 116 × 34, 4 apart; the setting is picked; no cell is lit while the ships differ.", "Hud.cpp:463-477, 1328-1346, 3441-3452"),
    item("Bars", bars, "Per-ship bars 17 × 10 in rows of 12; health fill GOOD above 1/2, FAIR above 1/4, POOR below; own progress in BAR_FILL_COLOR on BAR_TRACK_COLOR; bars over entities 32 × 5 on the side's color × 0.3, build progress in BUILD_BAR_COLOR.", "Hud.cpp:1026-1029, 3095-3126; ADR-085 d2; ADR-088 d5"),
    item("The Queue button", queue, "The commit action: QUEUE_COLOR hatched with QUEUE_HATCH_COLOR, QUEUE_EDGE_COLOR edge, QUEUE_TEXT_COLOR words, and the cost of the count.", "Hud.cpp:826-833, 1581-1597"),
    item("Key cap, checkbox and the Ore gem", keycap, "Key cap 20 × 20 FIELD_COLOR outlined in ROW_LABEL_COLOR; the checkbox a 10-unit square outline in AMBER_COLOR; the Ore gem in GOLD_COLOR, 13 beside the Title face and 8 beside the Figure face.", "Hud.cpp:1297-1310, 3022-3024; GlyphAtlas.cpp:27; ADR-085 d3"),
  ])


# ---------------------------------------------------------------------------------------------------- controls


def controls():
  rows = section("A4/A4.md", "4.4 Controls")
  table = []
  for line in rows.splitlines():
    m = re.match(r"^\|\s*`([^`]+)`\s*\|\s*(\(heading\)|`([^`]+)`)\s*\|\s*([\d\-]+)\s*\|", line)
    if m:
      table.append((m.group(1), m.group(3), m.group(4)))
  groups, cur = [], None
  for keys, does, src in table:
    if does is None:
      cur = [keys, src, []]
      groups.append(cur)
    else:
      cur[2].append((keys, does, src))
  out = []
  for head, hsrc, items in groups:
    li = "".join(
      f'<div style="display:grid;grid-template-columns:300px 1fr 150px;gap:16px;padding:9px 0;border-bottom:1px solid {C["EDGE_COLOR"]}">'
      f'<span style="font-family:{LABEL};font-weight:600;font-size:17px;color:{C["ACCENT_COLOR"]}">{esc(k)}</span>'
      f'<span style="font-family:{LABEL};font-weight:600;font-size:17px;color:{C["TEXT_COLOR"]}">{esc(d)}</span>'
      f'<code style="font-family:{MONO};font-size:12px;color:{C["LABEL_COLOR"]}">KeyBindings.h:{esc(s)}</code></div>' for k, d, s in items)
    out.append(panel(head, li, label=f"{len(items)} actions"))
  return "".join(out), sum(len(g[2]) for g in groups), len(groups)


# ---------------------------------------------------------------------------------------------------- images


def image(key, alt, width=None, caption=None):
  w = f"width:{width}px;" if width else "width:100%;"
  cap = (f'<figcaption style="margin-top:6px;font-size:12.5px;color:{C["LABEL_COLOR"]}">{caption}</figcaption>' if caption else "")
  return f'<figure style="margin:0 0 18px 0"><img src="{img(key)}" alt="{esc(alt)}" style="{w}height:auto;display:block;border:1px solid {C["EDGE_COLOR"]}">{cap}</figure>'


APPROX = "Approximation of the in-game look: rendered from the shipped .nmf by A6's software rasterizer, replaying MeshPS/MeshLinePS, not captured from the game."


# ---------------------------------------------------------------------------------------------------- chapters


def build_all():
  W = 1440

  # 01 Cover
  m = MANIFEST
  scope_in = ("The MVP design as amended by Phases 1 → 5, the two interface plans and the built tasks of the Look plan (LV1.1, LV1.2, LV2.1), "
              "and the code and data at HEAD.")
  scope_out = ("Horizon (design and plan), self-play (plan, network blueprint, ADR-063), and what a plan lists as not built: "
               "paint slots, two-tone lines, beacons, fleet plates (Look plan LV2.2–LV4.3).")
  how = [
    ("Tuning.json: hulls[0].hitPoints", "a key path into a data file under OutpostCommander/Assets/"),
    ("Hud.cpp:3143-3157", "a file and line range at HEAD; GameApp/ unless a folder is given"),
    ("ADR-064 §Decision 9 · ADR-085 d1", "a decision in Design/ADR/"),
    ("Phase4.md:120 · MVP.md:40", "a line of a design document (GameDesign/ and GameDesign/Archive/)"),
    ("Not specified", "nothing at HEAD decides it; never filled in by inference"),
    ("C-0xx / G-0xx", "a conflict or gap in 17 Open items, where code, ADRs and docs disagree or are silent"),
  ]
  how_rows = "".join(f'<tr><td style="width:340px"><code>{esc(a)}</code></td><td>{esc(b)}</td></tr>' for a, b in how)
  chapters = "".join(f'<li>{esc(n)}</li>' for _, n in PAGES[1:])
  cover = (
    f'<div style="display:grid;grid-template-columns:1fr 520px;gap:48px;align-items:start">'
    f'<div><div style="font-family:{LABEL};font-weight:600;font-size:14px;letter-spacing:3px;color:{C["LABEL_COLOR"]}">GAME DESIGN REFERENCE · AS BUILT</div>'
    f'<h1 style="margin:10px 0 0 0;font-family:{DISPLAY};font-weight:700;font-stretch:87.5%;font-size:96px;line-height:1;color:{C["GOLD_COLOR"]}">Outpost Commander</h1>'
    f'<p style="margin:18px 0 0 0;font-size:20px;line-height:1.5;color:{C["NUMBERS_COLOR"]};max-width:760px">Every rule, system, screen and asset of the game as it is built, '
    f'drawn and annotated, each number and rule with its source.</p>'
    f'<div style="margin-top:28px;display:grid;grid-template-columns:auto 1fr;column-gap:20px;row-gap:6px;font-size:15px">'
    f'<span style="color:{C["LABEL_COLOR"]}">Commit</span><code style="font-family:{MONO};color:{C["TEXT_COLOR"]}">{esc(m["head"]["sha"])}</code>'
    f'<span style="color:{C["LABEL_COLOR"]}">Commit date</span><span style="font-family:{MONO}">{esc(m["head"]["commitDate"])}</span>'
    f'<span style="color:{C["LABEL_COLOR"]}">Repository</span><span style="font-family:{MONO}">{esc(m["repo"])}</span>'
    f'<span style="color:{C["LABEL_COLOR"]}">Compiled</span><span style="font-family:{MONO}">{esc(m["referenceBuiltOn"])}</span></div></div>'
    f'{image("models/Human/Large_34_512.png", "Human Large hull", 520, "Human Large hull, 3/4 view. " + APPROX)}</div>')
  scope = (f'<div class="md" style="display:grid;grid-template-columns:1fr 1fr;gap:28px">'
           f'<div><h3>In scope</h3><p>{esc(scope_in)}</p><h3>Out of scope</h3><p>{esc(scope_out)}</p>'
           f'<h3>Precedence</h3><p>For what the player sees and the numbers behind it, code and data are ground truth; ADRs record why and usually the exact values; '
           f'a later phase document wins over an earlier one; plans are work queues, not authorities (<code>AGENTS.md</code>). Where they disagree, a page shows the code\'s value '
           f'and the disagreement is listed in 17 Open items.</p>'
           f'<h3>How this was made</h3><p>Seven extraction agents read the docs, ADRs, code and data; a reconciliation pass listed every conflict and gap; '
           f'this canvas is generated from their files (<code>DesignReference/_work/</code>, <code>DesignReference/compose/</code>). '
           f'Nothing was built or run: the container had no Windows, MSBuild or GPU. Screen geometry is derived from <code>Hud.cpp</code>\'s arithmetic, '
           f'and model images are approximations rendered from the shipped meshes.</p></div>'
           f'<div><h3>How to read a source</h3><table><tbody>{how_rows}</tbody></table><h3>Chapters</h3><ol>{chapters}</ol></div></div>')
  board("Main", "p01", "Cover", W, doc(cover + "<div style='height:36px'></div>" + panel("In this reference", stats()) +
                                       panel("Scope and sources", scope) + footer_note(), W))

  # 02 The game
  board("game-pitch", "p02", "The game: pitch and pillars", W, doc(
    chapter_head(2, "The game", "The pitch and the pillars in the owner's words, the loop the MVP set out to test, and how a match and a world run.",
                 ["MVP.md", "Phase1–5 designs", "A1"]) +
    card("Pitch and pillars", sec("A1/A1.md", "Pitch and pillars")) +
    card("The core loop", core_loop()) + footer_note(), W))
  loops = section("A1/A1.md", "Loops")
  match_loop = section("A1/A1.md", "Match loop")
  tick_loop = section("A1/A1.md", "Tick loop")
  world_loop = section("A1/A1.md", "World loop")
  board("game-loops", "p02", "Match, tick and world loops", W, doc(
    card("Match loop", flow(steps_of(match_loop), 4), label="player level") +
    card("Tick loop", flow(steps_of(tick_loop), 4), label="one simulation tick, code order") +
    card("World loop", flow(steps_of(world_loop), 4), label="a persistent world on a dedicated server") +
    card("Match flow (skirmish)", sec("A3/A3.md", "9. Match flow")) +
    card("Tick rate and wall time", sec("A1/A1.md", "Tick rate and wall time")) + footer_note(), W))
  for k, part in enumerate(table_parts(demote(read("glossary.md"), 1), 2), 1):
    board(f"glossary-{k}", "p02", f"Glossary, part {k} of 2", W, doc(card("Glossary", md(part), label=f"part {k} of 2") + footer_note(), W))

  # 03 Sides
  sides = []
  for name in ("Human", "Tarkan", "Pirate", "Wreck", "Asteroids"):
    s = TOKENS["sides"].get(name)
    if not s:
      continue
    sides.append(f'<tr><td><span style="display:inline-block;width:40px;height:24px;background:{s.get("hex")};border:1px solid {C["EDGE_COLOR"]}"></span></td>'
                 f'<td><strong>{esc(name)}</strong></td><td><code>{esc(s.get("hex"))}</code><br><code>{esc(s.get("value"))}</code> linear</td>'
                 f'<td>{esc(s.get("meshes", ""))}</td><td>{esc(s.get("role", ""))}</td><td><code>{esc(s.get("source", ""))}</code></td></tr>')
  side_table = (f'<div class="md"><table><thead><tr><th>Color</th><th>Side</th><th>Value</th><th>Meshes</th><th>Role</th><th>Source</th></tr></thead><tbody>'
                + "".join(sides) + "</tbody></table></div>")
  sheets = "".join(f'<div>{image(f"sheets/{n}.png", f"{n} models", None, n)}</div>' for n in ("Human", "Tarkan", "Pirate", "Wreck"))
  board("sides", "p03", "Sides", W, doc(
    chapter_head(3, "Sides", "Human and Tarkan with their own meshes; Pirate on Tarkan's meshes in its own color; Wreck in grey. Colors come from Models.json.",
                 ["Models.json", "A5 Sides", "A6 Sets and colors", "A7 Side colors"]) +
    card("The four sides and the rocks", side_table + md(TOKENS["sides"].get("_method", ""))) +
    card("Every model of each side", f'<div style="display:grid;grid-template-columns:1fr 1fr;gap:20px">{sheets}</div><p class="md" style="color:{C["LABEL_COLOR"]}">{APPROX}</p>') +
    card("Sides as A5 resolved them", sec("A5/A5.md", "Sides")) +
    card("Sets and colors in the renders", sec("A6/A6.md", "Sets and colors")) +
    card("Side colors in the world", sec("A7/A7.md", "Side colors")) + footer_note(), W))

  # 04 Economy & territory
  board("economy-ore", "p04", "Ore, Mining Rigs and depletion", W, doc(
    chapter_head(4, "Economy & territory", "Ore in hundredths, Mining Rigs and the reserves they draw down, the 10 km map of 25 sectors, territory held by Relays, domination and the fleet cap.",
                 ["Tuning.json", "Map.json", "A1", "ADR-016/035/036/056/057/071/072"]) +
    card("Ore", sec("A1/A1.md", "Ore: units")) +
    card("Mining Rigs", sec("A1/A1.md", "Mining Rigs")) +
    card("Ore depletion", sec("A1/A1.md", "Ore depletion")) + footer_note(), W))
  board("economy-map", "p04", "The map", W, doc(
    card("The map", map_svg(), label="Map.json") +
    card("Sectors, nodes and adjacency", sec("A1/A1.md", "The map")) +
    card("Seeded placement and mirroring", sec("A1/A1.md", "Seeded placement")) + footer_note(), W))
  board("economy-territory", "p04", "Territory, domination and the fleet cap", W, doc(
    card("Territory", sec("A1/A1.md", "Territory")) +
    card("Domination", sec("A1/A1.md", "Domination")) +
    card("Fleet cap", sec("A1/A1.md", "Fleet cap")) + footer_note(), W))

  # 05 Structures
  levels = "".join(image(f"sheets/{s}_{k}_levels.png", f"{s} {k} levels L1 to L5", None, f"{s} · {k} · L1 → L5")
                   for k in ("CommandStation", "Shipyard", "ResearchLab") for s in ("Human", "Tarkan"))
  board("structures", "p05", "Structures", W, doc(
    chapter_head(5, "Structures", "Every structure with its levels, costs and effects as the data gives them. Only the Command Station reaches L5; the Shipyard stops at L3 and the Research Lab at L4.",
                 ["Tuning.json: structures", "A2 Structures", "ADR-016", "ADR-064"]) +
    card("Structures", sec("A2/A2.md", "Structures")) + footer_note(), W))
  board("structures-levels", "p05", "Structure levels as drawn", W, doc(
    card("Levels as drawn", f'<p class="md" style="color:{C["LABEL_COLOR"]}">{APPROX} The art ships five levels for each; the data reaches L5, L3 and L4 (see Open items).</p>'
         f'<div style="display:grid;grid-template-columns:1fr 1fr;gap:20px">{levels}</div>') + footer_note(), W))

  # 06 Ships & the Designer
  board("ships-parts", "p06", "Hulls, drives, weapons, modules", W, doc(
    chapter_head(6, "Ships & the Designer", "Hulls, drives, weapons and the module; how a design's stats are derived; the starting designs; damage after armor as the Designer shows it.",
                 ["Tuning.json", "A2", "ADR-014/017/018/058/069"]) +
    card("Units and the clock", sec("A2/A2.md", "Units of measure")) +
    card("Hulls", sec("A2/A2.md", "Hulls")) + card("Drives", sec("A2/A2.md", "Drives")) +
    card("Weapons", sec("A2/A2.md", "Weapons")) + card("Modules", sec("A2/A2.md", "Modules")) +
    card("The structure weapon", sec("A2/A2.md", "The structure weapon")) + card("The Constructor", sec("A2/A2.md", "The Constructor")) +
    card("Hardpoints", sec("A2/A2.md", "Hardpoints")) + footer_note(), W))
  board("ships-designs", "p06", "Derived stats and starting designs", W, doc(
    card("How a design's stats are derived", sec("A2/A2.md", "How a design's stats are derived")) +
    card("Designs, starting designs and names", sec("A2/A2.md", "Designs, starting designs")) + footer_note(), W))
  board("ships-damage", "p06", "Damage after armor", W, doc(
    card("Damage after armor", sec("A2/A2.md", "Damage after armor")) + footer_note(), W))

  # 07 Research
  board("research", "p07", "Research", W, doc(
    chapter_head(7, "Research", "Twenty-three topics in three tiers, opened by the Research Lab's levels; a topic that needs another is drawn hatched, as the Research window draws it.",
                 ["Tuning.json: research", "A2 Research tree", "ADR-033"]) +
    card("The tree", research_tree(), label="drawn from Tuning.json: research") +
    card("Research tree as A2 traced it", sec("A2/A2.md", "Research tree")) + footer_note(), W))

  # 08 Combat
  board("combat", "p08", "Combat", W, doc(
    chapter_head(8, "Combat", "How hits land, matchups, retreat and repair, salvage, Pirates and losing all production.",
                 ["A1", "A2", "Tuning.json", "Matchups.json", "ADR-014/073/074/075/083"]) +
    card("Combat rules", sec("A2/A2.md", "Combat rules")) +
    card("Movement as far as it changes play", sec("A2/A2.md", "Movement as far as")) +
    card("Repair and retreat", sec("A1/A1.md", "Repair and retreat")) +
    card("Salvage", sec("A1/A1.md", "Salvage")) + card("Pirates", sec("A1/A1.md", "Pirates")) +
    card("Losing all production", sec("A1/A1.md", "Losing all production")) +
    card("Battle matchups", sec("A2/A2.md", "Battle matchups")) + footer_note(), W))
  tex = "".join(image(f"textures/{n}_on_dark.png", f"{n} texture", 200, n) for n in ("Particle", "glow", "cloudyglow", "laser", "starburst"))
  board("combat-effects", "p08", "Effects that carry information", W, doc(
    card("Shot looks", sec("A7/A7.md", "Shot looks")) +
    card("Explosions: shards", sec("A7/A7.md", "Explosions: shards")) +
    card("Explosions: particles", sec("A7/A7.md", "Explosions: particles")) +
    card("The textures they draw with", f'<div style="display:flex;gap:16px;flex-wrap:wrap">{tex}</div>') + footer_note(), W))

  # 09 Orders & automation
  board("orders", "p09", "Selection, commands, standing orders, alerts", W, doc(
    chapter_head(9, "Orders & automation", "Selection and commands, the two standing orders, alerts, scheduled orders, the deputy and the Away report, as the player meets them.",
                 ["A3", "Phase5.md", "ADR-059/079/080/088"]) +
    card("Selection and commands", sec("A3/A3.md", "1. Selection and commands")) +
    card("Standing orders", sec("A3/A3.md", "2. Standing orders")) +
    card("Alerts", sec("A3/A3.md", "3. Alerts")) + footer_note(), W))
  board("orders-scheduled", "p09", "Scheduled orders, Away, the deputy", W, doc(
    card("Scheduled orders", sec("A3/A3.md", "4. Scheduled orders")) +
    card("The Away report", sec("A3/A3.md", "5. The Away report")) +
    card("The seat controller and the deputy", sec("A3/A3.md", "6. The seat controller")) +
    card("Away → deputy → return", flow(steps_of(section("A3/A3.md", "14.3 Away")), 4)) + footer_note(), W))

  # 10 AI & difficulty
  board("ai", "p10", "AI behavior", W, doc(
    chapter_head(10, "AI & difficulty", "What the AI does as the player meets it, and how Easy, Normal and Hard differ, field by field.",
                 ["Opponent.json", "OpponentEasy.json", "OpponentHard.json", "A3", "ADR-020/041/065/076"]) +
    card("AI behavior", sec("A3/A3.md", "7. AI behavior")) + footer_note(), W))
  board("ai-difficulty", "p10", "Easy, Normal, Hard", W, doc(
    card("Difficulty, field by field", sec("A3/A3.md", "8. Difficulty diff")) + footer_note(), W))

  # 11 The persistent world
  board("world", "p11", "The persistent world", W, doc(
    chapter_head(11, "The persistent world", "A world on a dedicated server: seats taken with a token, saves every minute and a log of every command, a deputy while the player is away, and no end.",
                 ["Phase5.md", "A3", "A1", "ADR-077/078/079/082/084/086"]) +
    card("The dedicated server", sec("A3/A3.md", "10. The dedicated server")) +
    card("Join world, --join, lost connection", sec("A3/A3.md", "11. Join world")) +
    card("Join → play → lost connection", flow(steps_of(section("A3/A3.md", "14.2 Join")), 4)) + footer_note(), W))
  board("world-state", "p11", "World state, log and the week", W, doc(
    card("World state on disk", sec("A3/A3.md", "12. World state on disk")) +
    card("The world log, the week, a player who loses", sec("A3/A3.md", "13. The world log")) +
    card("A world without an end", sec("A1/A1.md", "A world without an end")) + footer_note(), W))

  # 12 Visual language
  hud_colors = [n for n in TOKENS["colors"] if TOKENS["colors"][n].get("source", "").startswith(("Hud", "GameApp/Hud")) or n in (
    "TEXT_COLOR",)]
  all_colors = list(TOKENS["colors"].keys())
  board("visual-colors", "p12", "Color tokens", W, doc(
    chapter_head(12, "Visual language", "The design system the game draws its interface with: one meaning per color, the type scale and its contrast, the components and their states, and how it scales.",
                 ["Hud.cpp", "Hud.h", "GameClient.cpp", "A5 tokens.json", "ADR-030/043/061/062/085/090/091"]) +
    card("How a color reaches the screen", sec("A5/A5.md", "How a color reaches the screen")) +
    card("Color tokens", swatches(all_colors, "colors"), label=f"{len(all_colors)} tokens, each with its one meaning") +
    card("Scene light and effects", swatches(list(TOKENS["light"].keys()), "light") + swatches(list(TOKENS["effects"].keys()), "effects")) + footer_note(), W))
  board("visual-type", "p12", "Type", W, doc(
    card("Faces", sec("A5/A5.md", "Faces")) +
    card("Type scale", type_scale(), label="reference units, 1920×1080 at factor 1.0") +
    card("Contrast: text on the panel it sits on", sec("A5/A5.md", "Contrast")) + footer_note(), W))
  board("visual-components", "p12", "Components", W, doc(
    card("Components and their states", component_gallery(), label="drawn from A5's specs with the tokens") + footer_note(), W))
  comp = section("A5/A5.md", "Components")
  heads = [m.start() for m in re.finditer(r"^### ", comp, flags=re.M)]
  half = heads[len(heads) // 2]
  board("visual-anatomy-1", "p12", "Component anatomy, part 1", W, doc(card("Anatomy and sources", md(demote(comp[:half], 1)), label="part 1 of 2") + footer_note(), W))
  board("visual-anatomy-2", "p12", "Component anatomy, part 2", W, doc(card("Anatomy and sources", md(demote(comp[half:], 1)), label="part 2 of 2") + footer_note(), W))
  board("visual-rules", "p12", "Rules, spacing and frame", W, doc(
    card("Rules", sec("A5/A5.md", "Rules")) +
    card("Spacing and frame", sec("A5/A5.md", "Spacing and frame")) + footer_note(), W))
  board("visual-scale", "p12", "Interface scale and the mockup", W, doc(
    card("Interface scale", sec("A4/A4.md", "Interface scale", demote_by=0)) +
    card("The designer mockup", image("mockup/ShipDesigner.png", "The owner's designer mockup", None, "GameDesign/Mockups/ShipDesigner.png: evidence, not ground truth; the code differs as below.") +
         sec("A5/A5.md", "Mockup vs code")) + footer_note(), W))

  # 13 Screens
  board("screens-flow", "p13", "Screens: inventory and flow", W, doc(
    chapter_head(13, "Screens", "Every screen and window in each state, drawn at 1920×1080 from the layout Hud.cpp computes, with numbered callouts that cite the rule behind each element.",
                 ["Hud.cpp", "A4 layouts", "ADR-013/015/031/043/066/070/088"]) +
    card("Flow", screen_flow()) +
    card("State inventory", sec("A4/A4.md", "State inventory")) + footer_note(), W))
  idx = json.loads((WORK / "A4" / "layouts" / "_index.json").read_text())
  frames = 0
  board("screen-startup-loading", "p13", "Startup: no screen while loading", 1920 + screens.CALLOUT_WIDTH, screens.empty_frame(
    "startup", "loading", "Startup: no screen is drawn while the game loads",
    ["The window is created hidden and shown once the first server and the AI settings, the client's assets, the interface atlas and the GPU uploads are ready, in parallel (WinMain.cpp:273-316; ADR-049 d2-d3).",
     "A loading screen is Not specified: none exists, and ADR-049's Consequences forecloses a window that shows anything before the game has loaded.",
     "A failure while loading shows a Windows message box captioned Outpost Commander, and the game exits (WinMain.cpp:22, 95-101, 559-572)."]))
  for it in idx:
    data, root, _ = screens.frame(it["file"])
    board("screen-" + it["file"].replace(".json", ""), "p13", it["title"][:120], 1920 + screens.CALLOUT_WIDTH, root)
    frames += 1
  specs = [
    ("spec-menu", "Main menu and startup", ["1. Main menu", "2. Startup"]),
    ("spec-hud-1", "HUD: Ore, status, territory, alerts, hint, connection", ["3. The match HUD", "3.1 The column", "3.2 Ore panel", "3.3 Status panel", "3.4 Territory panel", "3.5 Alerts panel", "3.6 Hint", "3.7 Where the match runs"]),
    ("spec-hud-2", "HUD: selection, commands, minimap, bars, banner", ["3.8 Selection panel", "3.9 Commands beside", "3.10 Minimap", "3.11 Bars over entities", "3.12 End-of-match banner"]),
    ("spec-windows", "Windows: common rules", ["4. Floating windows"]),
    ("spec-designer", "Window: Designer", ["4.1 Designer"]),
    ("spec-production-research", "Windows: Production and Research", ["4.2 Production", "4.3 Research"]),
    ("spec-orders-away", "Windows: Orders and Away", ["4.5 Orders", "4.6 Away"]),
  ]
  for name, title, heads in specs:
    inner = ""
    for h in heads:
      body = section("A4/A4.md", h)
      if h in ("3. The match HUD", "4. Floating windows"):
        body = section("A4/A4.md", h, stop="### ")
      inner += card(h.split(" ", 1)[1] if h[0].isdigit() else h, md(demote(body, 1)))
    board(name, "p13", title, W, doc(inner + footer_note(), W))

  # 14 The battlefield
  a7 = [
    ("battlefield-1", "Frame, camera, sky and grid", ["Draw order", "Frame and scene target", "Camera", "Starfield", "Vector grid"]),
    ("battlefield-2", "Models, light and motion", ["Model faces and lines", "Lighting", "Baked occlusion", "Ships, banking", "Structures: levels", "Derelicts", "Asteroids and asteroid fields"]),
    ("battlefield-3", "Fog, territory and marks", ["Fog of war", "Memories over the fog", "Territory marks", "Footprint rings", "Selection ring", "Placement ghost", "Health and build bars", "Move and order markers"]),
    ("battlefield-4", "Glows and what is not built", ["Exhaust glows", "Phosphor glow", "Not built at HEAD"]),
  ]
  first = True
  for name, title, heads in a7:
    inner = ""
    if first:
      inner += chapter_head(14, "The battlefield", "What the world looks like and the rule behind each element: camera, sky, grid, the lit vector look, fog and memories, territory, rings, bars, exhaust and the phosphor glow.",
                            ["Camera.json", "GameClient.cpp", "NeuronClient/Shader", "A7", "ADR-012/019/022/024/026/028/040/081/090/091/092"])
      inner += card("The vector look, in the game's light", f'<div style="display:grid;grid-template-columns:1fr 1fr;gap:20px">'
                    f'{image("models/Human/Medium_34_512.png", "Human Medium hull", None, "Human Medium · " + APPROX)}'
                    f'{image("models/Tarkan/Medium_34_512.png", "Tarkan Medium hull", None, "Tarkan Medium · " + APPROX)}</div>')
      first = False
    for h in heads:
      inner += card(h, sec("A7/A7.md", h))
    board(name, "p14", title, W, doc(inner + footer_note(), W))

  # 15 Asset catalogue
  set_sheets = "".join(image(f"sheets/{n}.png", f"{n} sheet", None, f"{n} · every model, 3/4 and top") for n in ("Human", "Tarkan", "Pirate", "Wreck", "Asteroids"))
  board("assets-sheets", "p15", "Contact sheets", W, doc(
    chapter_head(15, "Asset catalogue", "Every shipped model of every set in the game's look, the level sheets, the textures and the app's logos, with lengths, triangles, bounds and hashes.",
                 ["Models.json", "OutpostCommander/Assets/Models/*.nmf", "Art/Models/*.glb", "A6"]) +
    card("Renderer", sec("A6/A6.md", "Renderer and how")) +
    card("Sheets", f'<p class="md" style="color:{C["LABEL_COLOR"]}">{APPROX}</p>{set_sheets}') + footer_note(), W))
  all_levels = "".join(image(f"sheets/{st}_{k}_levels.png", f"{st} {k} levels", None, f"{st} · {k} · L1 → L5")
                       for st in ("Human", "Tarkan", "Pirate") for k in ("CommandStation", "Shipyard", "ResearchLab"))
  board("assets-levels", "p15", "Level sheets", W, doc(
    card("Level sheets, L1 → L5 at one scale", f'<p class="md" style="color:{C["LABEL_COLOR"]}">{APPROX}</p>'
         f'<div style="display:grid;grid-template-columns:1fr 1fr;gap:20px">{all_levels}</div>') + footer_note(), W))
  per_model = table_parts(demote(section("A6/A6.md", "Per-model table"), 1), 3)
  for k, part in enumerate(per_model, 1):
    board(f"assets-table-{k}", "p15", f"Per-model table, part {k} of 3", W, doc(card("Per-model table", md(part), label=f"part {k} of 3") + footer_note(), W))
  board("assets-files", "p15", "Mesh files, hashes, duplicates", W, doc(
    card("Mesh files, hashes and the bake check", sec("A6/A6.md", "Mesh files")) +
    card("Duplicates", sec("A6/A6.md", "Duplicates")) + card("Contact sheets", sec("A6/A6.md", "Contact sheets")) + footer_note(), W))
  tex2 = "".join(image(f"textures/{n}_on_dark.png", f"{n} texture", 220, n) for n in ("Particle", "glow", "cloudyglow", "laser", "starburst"))
  board("assets-textures", "p15", "Textures and logos", W, doc(
    card("Textures", f'<div style="display:flex;gap:16px;flex-wrap:wrap">{tex2}</div>' + sec("A6/A6.md", "Textures")) +
    card("App logos", image("logos/_logos_on_dark_and_white.png", "App logos on dark and white", None, "The packaged logos (OutpostCommander/Assets/*.png)") + sec("A6/A6.md", "App logos")) +
    card("Open and conflicts (A6)", sec("A6/A6.md", "Open / unclear") + sec("A6/A6.md", "Conflicts")) + footer_note(), W))

  # 16 Controls
  ctrl, n_actions, n_heads = controls()
  board("controls", "p16", "Controls", W, doc(
    chapter_head(16, "Controls", f"Every binding the Controls window lists ({n_actions} actions under {n_heads} headings), grouped as the window groups them. "
                 f"The window is built from KeyBindings.h, so it cannot name a key the code does not read (ADR-068 d4).",
                 ["KeyBindings.h:93-125", "Hud.cpp:1888-1926", "A4 §4.4"]) + ctrl +
    card("Keys and mouse for orders", sec("A3/A3.md", "1.2 Keys and mouse")) + footer_note(), W))

  # 17 Open items
  board("open-owner", "p17", "Open items: for the owner", W, doc(
    chapter_head(17, "Open items", "Every conflict between code, ADRs and design documents, and every gap where nothing decides, for the owner. The reference shows the code's value in each case.",
                 ["conflicts.md", "gaps.md", "R/summary.md", "R/leak-check.md"]) +
    card("Summary", md(demote(read("R/summary.md"), 1))) +
    card("Horizon and self-play leak check", md(demote(read("R/leak-check.md"), 1))) + footer_note(), W))
  parts = split_at(demote(read("conflicts.md"), 1), 3, pattern=r"^#{3,4} ")
  for k, part in enumerate(parts, 1):
    board(f"open-conflicts-{k}", "p17", f"Conflicts, part {k} of {len(parts)}", W,
          doc(card("Conflicts", md(part), label=f"part {k} of {len(parts)}") + footer_note(), W))
  board("open-gaps", "p17", "Gaps", W, doc(card("Gaps", md(demote(read("gaps.md"), 1))) + footer_note(), W))
  return frames


def stats():
  """Counts read from the work files, not typed."""
  srcpat = re.compile(r"`([^`]*(?:\.cpp|\.h|\.json|\.hlsl|\.md|\.py|ADR-\d{3})[^`]*)`")
  files = [f"A{i}/A{i}.md" for i in range(1, 8)] + ["conflicts.md", "gaps.md", "glossary.md"]
  citations = sum(len(srcpat.findall(read(f))) for f in files)
  conflicts = len(re.findall(r"^#+ C-\d+", read("conflicts.md"), flags=re.M))
  gaps = len(re.findall(r"G-\d{3}", read("gaps.md")))
  gaps = len(set(re.findall(r"G-\d{3}", read("gaps.md"))))
  terms = sum(1 for l in read("glossary.md").splitlines() if l.startswith("|")) - 2
  idx = json.loads((WORK / "A4" / "layouts" / "_index.json").read_text())
  renders = json.loads(read("A6/renders.json"))
  models = sum(len(s.get("models", [])) for s in MODELS["sets"] if "models" in s)
  cells = [
    (f"{len(idx) + 1}", "screen states drawn", "every state in A4's inventory, at 1920×1080"),
    ("6", "floating windows", "Designer, Production, Research, Controls, Orders, Away"),
    (f"{models}", "models in Models.json", "Human and Tarkan, 3 asteroids; Pirate and Wreck reuse meshes"),
    (f"{renders.get('count', {}).get('total', '') if isinstance(renders.get('count'), dict) else renders.get('count', '')}", "images rendered", "A6 renders.json"),
    (f"{len(TUNING['research'])}", "research topics", "Tuning.json: research"),
    (f"{citations:,}", "source citations", "in the agents' files this canvas is built from"),
    (f"{conflicts}", "conflicts", "code's value shown; 5 for the owner"),
    (f"{gaps}", "gaps", "Not specified, or a rule with no surface"),
    (f"{terms}", "glossary terms", "as the game spells them"),
  ]
  return (f'<div style="display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:18px">' + "".join(
    f'<div style="padding:12px 16px;background:{C["FIELD_COLOR"]};border:1px solid {C["EDGE_COLOR"]}">'
    f'<div style="font-family:{MONO};font-weight:700;font-size:34px;color:{C["TEXT_COLOR"]}">{esc(a)}</div>'
    f'<div style="font-family:{LABEL};font-weight:600;font-size:16px;color:{C["ROW_LABEL_COLOR"]}">{esc(b)}</div>'
    f'<div style="font-size:13px;color:{C["LABEL_COLOR"]};margin-top:2px">{esc(c)}</div></div>' for a, b, c in cells) + "</div>")


def screen_flow():
  """A4's Flow table as a graph: menu → match → windows → end / lost connection."""
  nodes = {
    "launch": ("Launch", 40, 40), "menu": ("Main menu", 360, 40), "exit": ("Exit / message box", 40, 260),
    "match": ("Match, before its first snapshot", 680, 40), "hud": ("Match HUD", 1000, 40),
    "windows": ("Windows: Designer, Production, Research, Controls, Orders", 1000, 260), "away": ("Away window", 1000, 480),
    "banner": ("End banner: Victory, Defeat, Draw, Empire fallen", 680, 260), "silent": ("HUD: No word from the server", 680, 480),
    "lost": ("Main menu with a notice", 360, 260),
  }
  edges = [
    ("launch", "menu", "loaded (window shown)"), ("launch", "match", "--join <file>, --measure"), ("launch", "exit", "load failure"),
    ("menu", "match", "Easy / Normal / Hard skirmish, Join world"), ("menu", "lost", "join fails"), ("menu", "exit", "Quit"),
    ("match", "hud", "first snapshot"), ("hud", "windows", "D P R F1 O, buttons"), ("windows", "hud", "×, Esc, key"),
    ("hud", "away", "snapshot with a report"), ("hud", "banner", "matchOver / restartTick"), ("banner", "menu", "Back to menu"),
    ("hud", "silent", "no snapshot 2 s"), ("silent", "lost", "receive throws"), ("hud", "lost", "connection lost"),
  ]
  bw, bh = 240, 64
  svg = [f'<defs><marker id="fa" markerWidth="10" markerHeight="10" refX="9" refY="5" orient="auto"><path d="M0,0 L10,5 L0,10 z" fill="{C["WINDOW_EDGE_COLOR"]}"/></marker></defs>']

  def center(k):
    _, x, y = nodes[k]
    return x + bw / 2, y + bh / 2

  for a, b, label in edges:
    (x0, y0), (x1, y1) = center(a), center(b)
    dx, dy = x1 - x0, y1 - y0
    # cut at the boxes' edges
    def cut(cx, cy, ux, uy):
      tx = (bw / 2) / abs(ux) if ux else 1e9
      ty = (bh / 2) / abs(uy) if uy else 1e9
      t = min(tx, ty)
      return cx + ux * t, cy + uy * t
    d = math.hypot(dx, dy)
    ux, uy = dx / d, dy / d
    sx, sy = cut(x0, y0, ux, uy)
    ex, ey = cut(x1, y1, -ux, -uy)
    off = 6 if (b, a) in [(e[0], e[1]) for e in edges] else 0
    sx += -uy * off; ex += -uy * off; sy += ux * off; ey += ux * off
    svg.append(f'<line x1="{sx:.1f}" y1="{sy:.1f}" x2="{ex:.1f}" y2="{ey:.1f}" stroke="{C["WINDOW_EDGE_COLOR"]}" stroke-width="1.5" marker-end="url(#fa)"/>')
    mx, my = (sx + ex) / 2, (sy + ey) / 2
    svg.append(f'<text x="{mx:.1f}" y="{my - 6:.1f}" text-anchor="middle" style="font-family:{MONO};font-size:11px" fill="{C["NUMBERS_COLOR"]}">{esc(label)}</text>')
  for k, (title, x, y) in nodes.items():
    svg.append(f'<rect x="{x}" y="{y}" width="{bw}" height="{bh}" fill="{C["WINDOW_COLOR"]}" stroke="{C["WINDOW_EDGE_COLOR"]}" stroke-width="1"/>')
    words = title.split(" ")
    lines, cur = [], ""
    for w in words:
      if len(cur) + len(w) > 30:
        lines.append(cur)
        cur = w
      else:
        cur = (cur + " " + w).strip()
    lines.append(cur)
    for i, ln in enumerate(lines):
      svg.append(f'<text x="{x + bw / 2}" y="{y + bh / 2 + 5 + (i - (len(lines) - 1) / 2) * 18:.1f}" text-anchor="middle" '
                 f'style="font-family:{LABEL};font-weight:600;font-size:15px" fill="{C["TEXT_COLOR"]}">{esc(ln)}</text>')
  graph = (f'<svg role="img" aria-label="Screen flow" width="1280" height="580" viewBox="0 0 1280 580" style="display:block">{"".join(svg)}</svg>')
  return graph + sec("A4/A4.md", "Flow", demote_by=1)


# ---------------------------------------------------------------------------------------------------- canvas


def write(root, heights):
  project = root / "project"
  project.mkdir(parents=True, exist_ok=True)
  for old in project.glob("*.dc.html"):
    old.unlink()
  boards, order, notes = {}, [], {}
  per_page = {}
  for file, page, title, width, body in BOARDS:
    per_page.setdefault(page, []).append((file, title, width, body))
  for page, items in per_page.items():
    x, y, row_h = 0, 0, 0
    per_row = 4 if page == "p13" else 5
    for i, (file, title, width, body) in enumerate(items):
      h = int(heights.get(file, 1080 if width > 2000 else 2000))
      if i and i % per_row == 0:
        x, y = 0, y + row_h + 120
        row_h = 0
      boards[file] = {"x": x, "y": y, "w": width, "h": h, "title": title, "page": page}
      order.append(file)
      x += width + 80
      row_h = max(row_h, h)
      (project / file).write_text(page_shell(title, body, width, h), encoding="utf-8")
    notes[f"t_{page}"] = {"x": 0, "y": -300, "text": dict(PAGES)[page], "kind": "title1", "maxW": 1840, "page": page}
  canvas = {
    "v": 3, "createdOnFiles": {"v": 1, "at": "2026-10-25T12:00:00Z"}, "title": "Outpost Commander — Game Design Reference",
    "launch": {"view": "canvas", "page": "p01"}, "pages": [{"id": p, "name": n} for p, n in PAGES],
    "boards": boards, "order": order, "notes": notes, "designSystems": [],
  }
  existing = project / "canvas.json"
  if existing.exists():
    old = json.loads(existing.read_text())
    canvas["createdOnFiles"] = old.get("createdOnFiles", canvas["createdOnFiles"])
  (project / "canvas.json").write_text(json.dumps(canvas, indent=1), encoding="utf-8")
  return boards


def main():
  root = Path(sys.argv[1])
  args = sys.argv[2:]
  heights = {}
  if "--assets" in args:
    ASSETS.update(json.loads(Path(args[args.index("--assets") + 1]).read_text()))
  if "--heights" in args:
    heights = json.loads(Path(args[args.index("--heights") + 1]).read_text())
  frames = build_all()
  boards = write(root, heights)
  (root / "assets-used.json").write_text(json.dumps(sorted(USED), indent=1))
  print(f"{len(boards)} artboards, {frames} screen frames")


if __name__ == "__main__":
  main()

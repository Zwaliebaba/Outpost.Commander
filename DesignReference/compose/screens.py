"""Draws a screen state from A4's derived layout: every panel, outline, bar, sprite, line and text at its rectangle, in
draw order (layer, then panels, lines, sprites, texts: Hud.h:714-760, GameClient.cpp:1300-1336), with its color token.
An annotation layer (ANNOTATION, not a game color) numbers the groups, and a column beside the frame says each group's
rule and sources.
"""
import json
import re

from lib import ANNOTATION, C, COLORS, FACE, LABEL, MONO, READ, WORK, alpha_of, color, esc, hatch, rgba

LAYOUTS = WORK / "A4" / "layouts"

SIDE_BAR_BACK = "#426391"  # Human's bar back, Shaded(side, HEALTH_BACK_SHADE 0.3): A5 tokens.json sides.Human.derived


def _enc(c):
  c = max(0.0, min(1.0, c))
  return 12.92 * c if c <= 0.0031308 else 1.055 * c ** (1 / 2.4) - 0.055


def _hex(lin):
  return "#" + "".join(f"{round(_enc(c) * 255):02X}" for c in lin)


def _linear(name):
  value = COLORS[name].get("value")
  return list(value[:3]), (float(value[3]) if len(value) > 3 else 1.0)


def _over(top, alpha, under):
  return [t * alpha + u * (1 - alpha) for t, u in zip(top, under)]


# The interface blends in linear light (UiPipeline.cpp:110-116, an sRGB render-target view, Renderer.h:25), so a
# translucent color is composited here, in linear, over what it is drawn on, and drawn opaque. The minimap's ground is
# MAP_COLOR over the black world; anything else translucent sits on a window.
_MAP_RGB, _MAP_A = _linear("MAP_COLOR")
MAP_GROUND = _over(_MAP_RGB, _MAP_A, [0.0, 0.0, 0.0])
WINDOW_GROUND = _linear("WINDOW_COLOR")[0]


def resolve(spec, ground=None):
  """A layout's color string to CSS: a token, a token at an alpha (composited in linear light over `ground`), or one
  of the few described colors."""
  if not spec:
    return None
  spec = spec.strip()
  name = spec.split("@")[0].split(" ")[0]
  if name in COLORS and COLORS[name].get("value") and isinstance(COLORS[name]["value"], list):
    rgb, a = _linear(name)
    if "@" in spec:
      m = re.search(r"\(([\d.]+)\)|@([\d.]+)", spec)
      a = float(m.group(1) or m.group(2)) if m else 1.0
    if a >= 0.999:
      return color(name, 1.0)
    under = ground if ground is not None else ([0.0, 0.0, 0.0] if name == "MAP_COLOR" else WINDOW_GROUND)
    return _hex(_over(rgb, a, under))
  if name in COLORS:
    return color(name)
  if spec.startswith("side color"):
    return SIDE_BAR_BACK
  if spec.startswith("black"):
    return FOG_CSS
  raise KeyError(f"unresolved layout color {spec!r}")


# Fog: black at 0.55 over what was seen before (FogOfWar.h:13-15: 0 in sight, 0.55 seen, 0.9 never seen). Blended in
# linear light, black at a over c leaves c × (1 − a); the sRGB alpha that gives the same is 1 − (1 − a)^(1/2.2).
FOG_ALPHA = 0.55
FOG_CSS = f"rgba(0,0,0,{1 - (1 - FOG_ALPHA) ** (1 / 2.2):.3f})"

DRAW_LIST = {"bars": 0, "panels": 1, "lines": 2, "sprites": 3, "texts": 4}


def ordered(elements):
  return sorted(elements, key=lambda e: (e.get("layer", 0), DRAW_LIST.get(e.get("drawList"), 1), e.get("order", 0)))


def _box(e):
  return f"left:{e['x']:.2f}px;top:{e['y']:.2f}px;width:{max(e['w'], 0):.2f}px;height:{max(e['h'], 0):.2f}px"


def draw_element(e):
  kind = e["kind"]
  ground = MAP_GROUND if e["id"].startswith("minimap") and e["id"] not in ("minimap.map",) else None
  if kind in ("panel", "button", "bar", "entityBar"):
    fill = e.get("fill", "solid")
    if fill == "none":
      return ""
    css = resolve(e.get("color"), ground)
    if css is None:
      return ""
    if fill == "hatched":
      # Laid on the screen, so neighbouring hatched panels line up: shift by (x + y) mod 7 (UiPS.hlsl:23-27).
      shift = -((e["x"] + e["y"]) % 7)
      bg = f"background:{hatch(css)};background-position:{shift:.2f}px 0"
    elif fill == "fog":
      bg = f"background:{FOG_CSS}"
    else:
      bg = f"background:{css}"
    return f'<div style="position:absolute;{_box(e)};{bg}"></div>'
  if kind == "outline":
    css = resolve(e.get("edge"), ground)
    width = e.get("edgeWidth", 1)
    style = "dashed" if e.get("pattern") else "solid"
    return f'<div style="position:absolute;{_box(e)};box-sizing:border-box;border:{width}px {style} {css}"></div>'
  if kind == "sprite":
    css = resolve(e.get("color"), ground)
    w, h = e["w"], e["h"]
    sprite = e.get("sprite")
    if sprite == "Corner":
      mx, my = e.get("mirror", {}).get("x"), e.get("mirror", {}).get("y")
      x0 = w - 0.5 if mx else 0.5
      y0 = h - 0.5 if my else 0.5
      x1 = 0 if mx else w
      y1 = 0 if my else h
      path = f"M{x1},{y0} L{x0},{y0} L{x0},{y1}"
      inner = f'<path d="{path}" stroke="{css}" stroke-width="1" fill="none"/>'
    elif sprite == "OreMark":
      pts = [(0.28, 0.14), (0.72, 0.14), (0.97, 0.40), (0.50, 0.94), (0.03, 0.40)]
      inner = f'<polygon points="{" ".join(f"{x * w:.2f},{y * h:.2f}" for x, y in pts)}" fill="{css}"/>'
    elif sprite == "Checkbox":
      inner = f'<rect x="0.5" y="0.5" width="{w - 1}" height="{h - 1}" stroke="{css}" stroke-width="1" fill="none"/>'
    else:
      raise KeyError(sprite)
    return (f'<svg aria-hidden="true" style="position:absolute;left:{e["x"]:.2f}px;top:{e["y"]:.2f}px;overflow:visible" '
            f'width="{w}" height="{h}" viewBox="0 0 {w} {h}">{inner}</svg>')
  if kind == "text":
    css = resolve(e.get("color"), ground)
    face = e.get("typeface") or "Name"
    size = round(e.get("size") or 16)  # rasterized at round(units × scale) px (UiPipeline.cpp:28-31)
    tracking = e.get("tracking") or 0
    label = e.get("label") or ""
    return (f'<div style="position:absolute;left:{e["x"]:.2f}px;top:{e["y"]:.2f}px;height:{e["h"]:.2f}px;line-height:{e["h"]:.2f}px;'
            f'white-space:pre;{FACE[face]};font-size:{size}px;letter-spacing:{tracking}px;color:{css}">{esc(label)}</div>')
  if kind == "line":
    return ""  # drawn in the frame's one SVG overlay
  raise KeyError(kind)


def lines_overlay(elements):
  segs = []
  for e in elements:
    if e["kind"] != "line":
      continue
    css = resolve(e.get("color"), MAP_GROUND if e["id"].startswith("minimap") else None)
    pts = " ".join(f"{x:.2f},{y:.2f}" for x, y in e["points"])
    segs.append(f'<polyline points="{pts}" stroke="{css}" stroke-width="{e.get("widthPixels", 1)}" fill="none"/>')
  if not segs:
    return ""
  return ('<svg aria-hidden="true" style="position:absolute;left:0;top:0" width="1920" height="1080" viewBox="0 0 1920 1080">'
          + "".join(segs) + "</svg>")


# ---------------------------------------------------------------------------------------------------- groups

GROUPS = [
  # (key, id prefixes, title, rule, sources)
  ("menu", ("menu",), "Main menu",
   "One panel 440 wide, centred: Easy, Normal and Hard skirmish, Join world when a join file exists, Quit. After a lost connection or a failed join a notice says why, in WARNING_COLOR as plain text. No hover lighting.",
   "Hud.cpp:2329-2359; ADR-065 d4; ADR-078 d9-d10; ADR-085 d1"),
  ("ore", ("ore",), "Ore panel",
   "The stockpile as the gem and figure in GOLD_COLOR, written one way everywhere; the income in NUMBERS_COLOR, a warning chip at +0/s. The word Ore is not written.",
   "Hud.cpp:3143-3157; ADR-043 d3; ADR-047 d5; ADR-085 d1"),
  ("status", ("status",), "Production status",
   "What the Research Lab and the Shipyards are doing and the fleet against its cap. Each line opens its window; IDLE is a chip only when work could start now. Its place is kept at 94 units.",
   "Hud.cpp:281-414, 3239-3308; ADR-066"),
  ("territory", ("territory",), "Territory",
   "Nodes held, own : enemy, against the Command Station's cap; domination tickets while both sides have them; the drain, a chip when it is the player's. Only on a map with sectors.",
   "Hud.cpp:3167-3213; ADR-056; ADR-057"),
  ("alerts", ("alerts",), "Alerts",
   "Alerts of the last 8 s, at most 4, newest first and a chip. One kind in one sector is not repeated within 20 s, except a fired order. Space looks at the newest.",
   "Alerts.h:15-18; Alerts.cpp:57-60; ADR-059 d1-d2; ADR-085 d1"),
  ("hint", ("hint",), "Hint",
   "What a click on the ground will do, or what is under the pointer: a placement wins over a derelict or a remembered structure.",
   "Hud.cpp:3311-3316; GameClient.cpp:742-749"),
  ("connection", ("connection",), "Where the match runs",
   "Local skirmish, or Server host:port. After 2 s without a snapshot a chip counts the silence. Never on the menu.",
   "Hud.cpp:3320-3337; Hud.h:469; ADR-086"),
  ("selection", ("selection",), "Selection",
   "Name, state and hit points as lines and bars. Per-ship bars for 2 to 24 ships, rows of 12; a click on a bar selects that ship and looks at it.",
   "Hud.cpp:3357-3426; Hud.h:566; ADR-088 d4"),
  ("buttons", ("buttons",), "Commands beside the selection",
   "A structure's windows and upgrade; a warship selection's orders with key caps; a Constructor's build buttons with costs; retreat as three choices (25%, 50%, Never).",
   "Hud.cpp:3427-3453, 2614-2828; ADR-088 d2-d3"),
  ("minimap", ("minimap",), "Minimap",
   "10,000 m in 284 units (35.2 m a unit): sector wash, marks, fog, the sector lattice, held and guarded outlines, remembered crosses, alerts and the camera's view.",
   "Hud.cpp:3456-3630; ADR-068 d2; ADR-081"),
  ("entityBar", ("entityBar",), "Bars over entities",
   "32×5 at the back buffer's scale, not the interface factor. Health while hurt or on Alt; build progress in BUILD_BAR_COLOR, no side's, while unfinished.",
   "Hud.cpp:3095-3126; ADR-047 d4; ADR-088 d5; ADR-085 d2"),
  ("outcome", ("outcome",), "End-of-match banner",
   "Victory, Defeat, Draw or, in a world, Empire fallen, with how and how long. The world runs on behind it and the HUD still works.",
   "Hud.cpp:3340-3350, 2188-2218; ADR-084 d7"),
  ("window", ("window",), "Window chrome",
   "Opaque WINDOW_COLOR body, hatched title bar (drag it), close box, corner brackets. Esc closes the front window; a press brings a window to the front.",
   "Hud.cpp:871-899; ADR-031; WindowManager.cpp"),
  ("header", ("header",), "Header band and Ore box",
   "The hatching runs on to y 72; the subject steps with < and >; the Ore box repeats the stockpile.",
   "Hud.cpp:1237-1249, 1658-1671"),
  ("dhead", ("designer.shipyard", "designer.titleLabel", "designer.title", "designer.queueLabel", "designer.built", "designer.queueSlot"),
   "Designer header", "The target Shipyard (stepped), its queue as five slots and the ships it has built.",
   "Hud.cpp:1361-1379; ADR-068 d1"),
  ("dname", ("designer.name", "designer.save", "designer.saved", "designer.chip", "designer.chips"), "Name and saved designs",
   "The name field (32 characters, printable ASCII), SAVE or SAVED, and the saved designs as chips with their codes.",
   "Hud.cpp:1385-1427; Designer.cpp:122-151; ADR-069"),
  ("dslots", ("designer.slot", "designer.card"), "Parts",
   "Hull, drive, weapon and module cards, three to a line. A locked card is hatched and names the research that unlocks it; hovering an unlocked card previews it.",
   "Hud.cpp:1429-1482, 622-635; ADR-062 d4"),
  ("dperf", ("designer.performance", "designer.bar", "designer.sectionDivider"), "Performance",
   "Each stat as a bar against the best of all designs; a preview shows the change in GOOD_COLOR or POOR_COLOR.",
   "Hud.cpp:1487-1505, 494-527"),
  ("ddamage", ("designer.damage",), "Damage / s after armor · vs a formation",
   "One card per hull: per-ship damage a second, the ships a splash reaches, a Good / Fair / Poor bar and the damage per 100 Ore.",
   "Hud.cpp:1508-1540, 417-418"),
  ("dhint", ("designer.hint",), "Hint", "Hover any part to preview its effect.", "Hud.cpp:731-732, 1541-1542"),
  ("dfoot", ("designer.rename", "designer.retreat", "designer.fewer", "designer.more", "designer.count", "designer.queue"),
   "Footer", "RENAME or UPDATE, the design's retreat, how many, and QUEUE, which saves new picks first.",
   "Hud.cpp:1543-1597; ADR-023"),
  ("prod", ("production",), "Production cards",
   "A Constructor at the Command Station, or each saved design at a Shipyard: name, code, cost, time and S / M / L strength segments.",
   "Hud.cpp:1708-1784; ADR-068 d3"),
  ("queue", ("queue",), "Queue",
   "Up to 5 jobs; the front one shows progress, WAITING FOR ORE or WAITING FOR FLEET CAP.",
   "Hud.cpp:1675-1706; Snapshot.h:42"),
  ("research", ("research",), "Topics",
   "Each topic not yet researched or queued, ten to a page. A topic that needs something is hatched and says what it needs; else its tier and time.",
   "Hud.cpp:1786-1884; ADR-061 d4; ADR-033"),
  ("controls", ("controls",), "Controls",
   "Every key and mouse action the game reads in a match, built from KeyBindings.h so it cannot name a key the code does not read.",
   "Hud.cpp:1888-1926; KeyBindings.h:93-125; ADR-068 d4"),
  ("owait", ("orders.titleLabel", "orders.waiting", "orders.none", "orders.scheduled", "orders.more"), "Waiting orders",
   "The seat's scheduled orders, up to 6 listed, then and n more.",
   "Hud.cpp:1952-1965; OrderForm.cpp:291-315; ADR-080"),
  ("oform", ("orders.form", "orders.row", "orders.fires", "orders.give"), "New order for the selection",
   "One trigger, one action, at most one condition; each row steps with < and >. Give is dim with its reason when no selected ship can take it.",
   "Hud.cpp:1967-1980, 2248-2289; OrderForm.cpp:83-205; ADR-080"),
  ("away", ("away",), "While you were away",
   "Opens itself with the first snapshot that carries the report: how long, what was built and lost, sectors gained and lost, each fired order. Closed, it is gone.",
   "Hud.cpp:1983-2007; GameClient.cpp:539-544, 763-765; Phase 5 design §11"),
]


def group_of(element_id):
  best, best_len = None, -1
  for key, prefixes, *_ in GROUPS:
    for p in prefixes:
      if (element_id == p or element_id.startswith(p + ".") or element_id.startswith(p)) and len(p) > best_len:
        best, best_len = key, len(p)
  return best


def groups_in(elements):
  boxes = {}
  for e in elements:
    if e["id"].startswith("corner"):
      continue
    g = group_of(e["id"])
    if g is None:
      continue
    x0, y0, x1, y1 = e["x"], e["y"], e["x"] + max(e["w"], 1), e["y"] + max(e["h"], 1)
    if e["kind"] == "line":
      xs = [p[0] for p in e["points"]]
      ys = [p[1] for p in e["points"]]
      x0, x1, y0, y1 = min(xs), max(xs), min(ys), max(ys)
    if g in boxes:
      b = boxes[g]
      boxes[g] = (min(b[0], x0), min(b[1], y0), max(b[2], x1), max(b[3], y1))
    else:
      boxes[g] = (x0, y0, x1, y1)
  order = [g[0] for g in GROUPS]
  return [(g, boxes[g]) for g in order if g in boxes]


def ungrouped(elements):
  return sorted({e["id"] for e in elements if not e["id"].startswith("corner") and group_of(e["id"]) is None})


# ---------------------------------------------------------------------------------------------------- the frame

CALLOUT_WIDTH = 520


def frame(layout_file):
  data = json.loads((LAYOUTS / layout_file).read_text())
  elements = data["elements"]
  body = "".join(draw_element(e) for e in ordered(elements))
  body += lines_overlay(elements)
  groups = groups_in(elements)
  info = {g[0]: g for g in GROUPS}
  marks, legend = [], []
  for i, (g, (x0, y0, x1, y1)) in enumerate(groups, 1):
    marks.append(f'<div style="position:absolute;left:{x0 - 2:.1f}px;top:{y0 - 2:.1f}px;width:{x1 - x0 + 4:.1f}px;height:{y1 - y0 + 4:.1f}px;'
                 f'box-sizing:border-box;border:1px dashed {ANNOTATION};opacity:0.75"></div>')
    bx = max(2, min(1920 - 26, x0 - 12))
    by = max(2, min(1080 - 26, y0 - 12))
    marks.append(f'<div style="position:absolute;left:{bx:.1f}px;top:{by:.1f}px;width:22px;height:22px;border-radius:11px;background:{ANNOTATION};'
                 f'color:#000;font:700 12px/22px {READ};text-align:center">{i}</div>')
    _, _, title, rule, src = info[g]
    legend.append(
      f'<li style="display:flex;gap:12px;margin:0 0 14px 0"><span style="flex:0 0 22px;height:22px;border-radius:11px;background:{ANNOTATION};color:#000;'
      f'font:700 12px/22px {READ};text-align:center">{i}</span><span><span style="display:block;font-family:{LABEL};font-weight:600;font-size:15px;color:{C["TEXT_COLOR"]}">{esc(title)}</span>'
      f'<span style="display:block;font-size:13.5px;line-height:1.45;color:{C["NUMBERS_COLOR"]}">{esc(rule)}</span>'
      f'<span style="display:block;margin-top:3px;font-family:{MONO};font-size:11.5px;color:{C["LABEL_COLOR"]}">{esc(src)}</span></span></li>')
  note_list = list(data.get("notes") or [])
  if not elements:
    note_list.append("No HUD is laid out until the view holds a snapshot (GameClient.cpp:689-775), and DrawHud draws nothing while fontPixels is 0 "
                     "(GameClient.cpp:1291-1293): the screen shows the sky and the grid only. The camera centres on the player's ships with the first snapshot (GameClient.cpp:548-567).")
  notes = "".join(f'<li style="margin:0 0 6px 0">{esc(n)}</li>' for n in note_list)
  estimated = sum(1 for e in elements if e.get("measured") is False)
  world_note = ("The world behind the HUD (ships, grid, fog) is not drawn here; see The battlefield. "
                if data["screen"] in ("hud",) else "")
  column = (
    f'<aside style="flex:0 0 {CALLOUT_WIDTH}px;min-height:1080px;box-sizing:border-box;padding:28px 28px 28px 32px;'
    f'background:{C["FIELD_COLOR"]};border-left:1px solid {C["EDGE_COLOR"]};font-family:{READ};color:{C["TEXT_COLOR"]}">'
    f'<div style="font-family:{LABEL};font-weight:600;font-size:13px;letter-spacing:2px;text-transform:uppercase;color:{C["LABEL_COLOR"]}">'
    f'{esc(data["screen"])} · {esc(data["state"])} · factor {data.get("interfaceFactor", 1.0):g}</div>'
    f'<h2 style="margin:6px 0 12px 0;font-family:{LABEL};font-weight:600;font-size:22px;line-height:1.25;color:{C["TEXT_COLOR"]}">{esc(data["title"])}</h2>'
    f'<ol style="list-style:none;margin:0 0 16px 0;padding:0">{"".join(legend)}</ol>'
    f'<div style="font-size:12.5px;line-height:1.5;color:{C["LABEL_COLOR"]}">'
    f'<p style="margin:0 0 8px 0">Geometry derived from Hud.cpp by A4\'s derive_layouts.py, not dumped from the game: '
    f'{estimated} of {len(elements)} elements depend on DirectWrite text measurement and are estimated. '
    f'Content values are samples built from Tuning.json and Map.json. {world_note}'
    f'Translucent colors are composited in linear light, as the game blends them; the minimap\'s fog is drawn at 0.55, '
    f'the shade of what was seen before: the game draws 0 in sight and 0.9 where never seen (FogOfWar.h:13-15). '
    f'Dashed boxes and numbered discs in {ANNOTATION} are this reference\'s annotations, not the game.</p>'
    f'<ul style="margin:0;padding-left:18px">{notes}</ul>'
    f'<p style="margin:8px 0 0 0;font-family:{MONO};font-size:11.5px">DesignReference/_work/A4/layouts/{esc(layout_file)}</p></div></aside>')
  screen = (f'<div aria-label="{esc(data["title"])}" role="img" style="position:absolute;left:0;top:0;width:1920px;height:1080px;overflow:hidden;background:#000">'
            f'{body}</div>')
  overlay = f'<div aria-hidden="true" style="position:absolute;left:0;top:0;width:1920px;height:1080px;pointer-events:none">{"".join(marks)}</div>'
  root = (f'<div data-root="1" style="display:flex;align-items:flex-start;width:{1920 + CALLOUT_WIDTH}px;background:{C["FIELD_COLOR"]}">'
          f'<div style="position:relative;flex:0 0 1920px;height:1080px">{screen}{overlay}</div>{column}</div>')
  return data, root, ungrouped(elements)


def empty_frame(screen, state, title, lines):
  """A state that draws nothing of the interface: a black frame and the reason, with its sources."""
  items = "".join(f'<li style="margin:0 0 10px 0">{esc(t)}</li>' for t in lines)
  column = (f'<aside style="flex:0 0 {CALLOUT_WIDTH}px;min-height:1080px;box-sizing:border-box;padding:28px 28px 28px 32px;background:{C["FIELD_COLOR"]};'
            f'border-left:1px solid {C["EDGE_COLOR"]};font-family:{READ};color:{C["TEXT_COLOR"]}">'
            f'<div style="font-family:{LABEL};font-weight:600;font-size:13px;letter-spacing:2px;text-transform:uppercase;color:{C["LABEL_COLOR"]}">{esc(screen)} · {esc(state)}</div>'
            f'<h2 style="margin:6px 0 12px 0;font-family:{LABEL};font-weight:600;font-size:22px;line-height:1.25">{esc(title)}</h2>'
            f'<ul style="margin:0;padding-left:18px;font-size:14px;line-height:1.5;color:{C["NUMBERS_COLOR"]}">{items}</ul></aside>')
  screen_div = (f'<div role="img" aria-label="{esc(title)}" style="position:relative;flex:0 0 1920px;height:1080px;background:#000">'
                f'<div style="position:absolute;left:0;right:0;top:500px;text-align:center;font-family:{LABEL};font-weight:600;font-size:15px;letter-spacing:2px;color:{ANNOTATION}">'
                f'NOTHING OF THE INTERFACE IS DRAWN IN THIS STATE</div></div>')
  return f'<div data-root="1" style="display:flex;align-items:flex-start;width:{1920 + CALLOUT_WIDTH}px;background:{C["FIELD_COLOR"]}">{screen_div}{column}</div>'

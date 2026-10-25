"""Shared pieces of the reference's composer: tokens, the page shell, markdown sections and the canvas index.

Everything a page shows is read from DesignReference/_work (the agents' extractions and A5's tokens), so a number
reaches the canvas without being typed again. The page look is built from A5's tokens: a page is a HUD panel
(WINDOW_COLOR body, WINDOW_EDGE_COLOR corner brackets), a section head is a window's hatched title bar, and a table's
head is the game's spaced capitals in LABEL_COLOR.
"""
import html
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
WORK = ROOT / "DesignReference" / "_work"
sys.path.insert(0, str(Path(__file__).resolve().parent / "pylib"))

TOKENS = json.loads((WORK / "A5" / "tokens.json").read_text())
MANIFEST = json.loads((WORK / "manifest.json").read_text())
HEAD = MANIFEST["head"]


def _hex_of(entry):
  value = entry.get("hex")
  return value if value else "#FF00FF"


COLORS = {}
for name, entry in TOKENS["colors"].items():
  COLORS[name.split(" ")[0]] = entry

ANNOTATION = "#FF2D95"  # the reference's own annotation layer; deliberately no game color


def hex_(name):
  return _hex_of(COLORS[name])


def alpha_of(name):
  entry = COLORS[name]
  value = entry.get("value")
  if isinstance(value, list) and len(value) == 4:
    return float(value[3])
  return 1.0


def rgba(hexcolor, alpha):
  hexcolor = hexcolor.lstrip("#")
  r, g, b = int(hexcolor[0:2], 16), int(hexcolor[2:4], 16), int(hexcolor[4:6], 16)
  return f"rgba({r},{g},{b},{alpha:.3f})"


def color(name, alpha=None):
  """A token as CSS, with its own alpha unless one is given."""
  base = hex_(name)
  a = alpha_of(name) if alpha is None else alpha
  return base if a >= 0.999 else rgba(base, a)


C = {n: hex_(n) for n in COLORS}

FONT_LINK = ('<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Barlow+Semi+Condensed:wght@600;700'
             '&amp;family=Barlow:wght@500;600;700&amp;family=JetBrains+Mono:wght@400;600;700&amp;family=Noto+Sans:wght@400;600&amp;display=swap">')

# The game's faces, first the installed Windows font, then a Google Fonts stand-in for a viewer without it.
FACE = {
  "Title": "font-family:Bahnschrift,'Barlow Semi Condensed',sans-serif;font-weight:700;font-stretch:87.5%",
  "Label": "font-family:Bahnschrift,'Barlow',sans-serif;font-weight:600",
  "Name": "font-family:Bahnschrift,'Barlow',sans-serif;font-weight:600",
  "Figure": "font-family:'Cascadia Mono',Consolas,'JetBrains Mono',monospace;font-weight:600",
  "LargeFigure": "font-family:'Cascadia Mono',Consolas,'JetBrains Mono',monospace;font-weight:700",
  "Detail": "font-family:'Cascadia Mono',Consolas,'JetBrains Mono',monospace;font-weight:400",
  "Body": "font-family:'Segoe UI','Noto Sans',sans-serif;font-weight:600",
}
READ = "'Segoe UI','Noto Sans',sans-serif"
MONO = "'Cascadia Mono',Consolas,'JetBrains Mono',monospace"
DISPLAY = "Bahnschrift,'Barlow Semi Condensed',sans-serif"
LABEL = "Bahnschrift,'Barlow',sans-serif"


def corners(edge, size=12, width=1):
  """Four L-shaped corner brackets as background layers (the game's Corner sprite, AddCorners Hud.cpp:852-867)."""
  e = edge
  s, w = size, width
  layers = []
  for pos in ("left top", "right top", "left bottom", "right bottom"):
    layers.append(f"linear-gradient({e},{e}) {pos} / {s}px {w}px no-repeat")
    layers.append(f"linear-gradient({e},{e}) {pos} / {w}px {s}px no-repeat")
  return ",".join(layers)


def hatch(stripe, base=None, period=7, width=2):
  """The game's hatching: stripes where fmod(x + y, 7) < 2, rising to the right (Hud.h:639-641, UiPS.hlsl:23-27)."""
  p = period / 2 ** 0.5
  w = width / 2 ** 0.5
  layer = f"repeating-linear-gradient(135deg,{stripe} 0 {w:.3f}px,transparent {w:.3f}px {p:.3f}px)"
  return layer + (f",{base}" if base else "")


BASE_CSS = f"""
body{{margin:0;background:{C['FIELD_COLOR']}}}
a{{color:{C['ACCENT_COLOR']}}}a:hover{{color:{C['TEXT_COLOR']}}}
.md{{font-family:{READ};font-size:15px;line-height:1.55;color:{C['TEXT_COLOR']}}}
.md p{{margin:0 0 10px 0;max-width:980px}}
.md ul,.md ol{{margin:0 0 12px 0;padding-left:22px;max-width:1040px}}
.md li{{margin:0 0 4px 0}}
.md strong{{color:{C['TEXT_COLOR']};font-weight:700}}
.md h1,.md h2{{font-family:{DISPLAY};font-weight:700;font-size:24px;letter-spacing:0;color:{C['TEXT_COLOR']};margin:22px 0 10px 0}}
.md h3{{font-family:{LABEL};font-weight:600;font-size:13px;letter-spacing:2px;text-transform:uppercase;color:{C['ROW_LABEL_COLOR']};margin:22px 0 10px 0}}
.md h4{{font-family:{LABEL};font-weight:600;font-size:17px;color:{C['TEXT_COLOR']};margin:20px 0 8px 0}}
.md code{{font-family:{MONO};font-size:12.5px;color:{C['ROW_LABEL_COLOR']};background:{C['FIELD_COLOR']};border:1px solid {C['EDGE_COLOR']};padding:0 4px;border-radius:2px;white-space:normal;overflow-wrap:anywhere}}
.md blockquote{{margin:0 0 12px 0;padding:8px 16px;border-left:2px solid {C['GOLD_COLOR']};background:{C['FIELD_COLOR']};color:{C['NUMBERS_COLOR']};font-family:{READ};font-size:16px}}
.md table{{border-collapse:collapse;width:100%;margin:6px 0 16px 0;font-size:13px;line-height:1.45}}
.md th{{font-family:{LABEL};font-weight:600;font-size:12px;letter-spacing:1.5px;text-transform:uppercase;color:{C['LABEL_COLOR']};text-align:left;padding:6px 8px;border-bottom:1px solid {C['EDGE_COLOR']};vertical-align:bottom;background:{C['FIELD_COLOR']};overflow-wrap:anywhere;word-break:break-word}}
.md td{{padding:5px 8px;border-bottom:1px solid {C['EDGE_COLOR']};vertical-align:top;color:{C['NUMBERS_COLOR']};overflow-wrap:anywhere;word-break:break-word}}
.md td code{{font-size:11.5px}}
.md pre{{margin:6px 0 14px 0;padding:12px 14px;background:{C['FIELD_COLOR']};border:1px solid {C['EDGE_COLOR']};overflow:hidden;white-space:pre-wrap}}
.md pre code{{border:0;padding:0;background:none;font-size:12.5px;line-height:1.5;color:{C['NUMBERS_COLOR']};white-space:pre-wrap}}
.md hr{{border:0;border-top:1px solid {C['EDGE_COLOR']};margin:18px 0}}
.md .good{{color:{C['GOOD_COLOR']}}}.md .fair{{color:{C['FAIR_COLOR']}}}.md .poor{{color:{C['POOR_COLOR']}}}
"""


def page_shell(title, body, width, height, extra_css=""):
  props = json.dumps({"$preview": {"width": width, "height": height}})
  return f"""<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>{html.escape(title)}</title>
<script src="./support.js"></script>
</head>
<body>
<x-dc>
<helmet>
{FONT_LINK}
<style>
{BASE_CSS}{extra_css}
</style>
</helmet>
{body}
</x-dc>
<script type="text/x-dc" data-dc-script data-props='{props}'>
class Component extends DCLogic {{
renderVals() {{
return {{}};
}}
}}
</script>
</body>
</html>
"""


# ---------------------------------------------------------------------------------------------------- markdown


def _markdown():
  import markdown  # installed under compose/pylib (pip --target); a build tool, never shipped

  return markdown.Markdown(extensions=["tables", "sane_lists", "fenced_code"])


_CODE = re.compile(r"(`+)(.+?)\1", re.S)


def _escape_outside_code(text):
  out, last = [], 0
  for m in _CODE.finditer(text):
    out.append(text[last:m.start()].replace("<", "&lt;"))
    out.append(m.group(0))
    last = m.end()
  out.append(text[last:].replace("<", "&lt;"))
  return "".join(out)


RATING = re.compile(r"<td>(\s*(?:<strong>)?)(Good|Fair|Poor)\b")


def md(text):
  text = text.replace("{{", "{ {").replace("}}", "} }")
  text = _escape_outside_code(text)
  converter = _markdown()
  out = converter.convert(text)
  out = RATING.sub(lambda m: f'<td>{m.group(1)}<span class="{m.group(2).lower()}">{m.group(2)}</span>', out)
  out = out.replace("{{", "{ {")
  return f'<div class="md">{out}</div>'


def read(rel):
  path = WORK / rel
  if not path.exists():
    return f"**Pending:** `{rel}` is not written yet.\n"
  return path.read_text(encoding="utf-8")


HEADING = re.compile(r"^(#{1,6})\s+(.*)$")


def section(rel, heading, keep_heading=False, stop=None):
  """The markdown under a heading (matched by its start), up to the next heading of the same or a higher level."""
  lines = read(rel).splitlines()
  start = level = None
  for i, line in enumerate(lines):
    m = HEADING.match(line)
    if m and m.group(2).strip().startswith(heading):
      start, level = i, len(m.group(1))
      break
  if start is None:
    raise KeyError(f"{rel}: no heading starting {heading!r}")
  end = len(lines)
  for j in range(start + 1, len(lines)):
    m = HEADING.match(lines[j])
    if m and len(m.group(1)) <= level:
      end = j
      break
    if stop and lines[j].startswith(stop):
      end = j
      break
  body = lines[start:end] if keep_heading else lines[start + 1:end]
  return "\n".join(body).strip() + "\n"


def demote(text, by=1):
  """Lowers every heading by `by` levels, so a section's own subheadings sit under the card's title."""
  return re.sub(r"^(#{1,6})(\s)", lambda m: "#" * min(6, len(m.group(1)) + by) + m.group(2), text, flags=re.M)


# ---------------------------------------------------------------------------------------------------- page parts


def esc(text):
  return html.escape(str(text), quote=True)


def panel(title, inner, label=None, width=None, pad=24, extra=""):
  """A HUD panel with a window's hatched title bar: the reference's card."""
  bar = ""
  if title:
    bar = (f'<div style="height:36px;display:flex;align-items:center;padding:0 {pad}px;background:{hatch(C["TITLE_HATCH_COLOR"], C["WINDOW_COLOR"])};'
           f'font-family:{LABEL};font-weight:600;font-size:13px;letter-spacing:2px;text-transform:uppercase;color:{C["HEADER_LABEL_COLOR"]}">'
           f'{esc(title)}{(" · " + esc(label)) if label else ""}</div>')
  w = f"width:{width}px;" if width else ""
  return (f'<section style="{w}box-sizing:border-box;background:{corners(C["WINDOW_EDGE_COLOR"])},{C["WINDOW_COLOR"]};margin:0 0 24px 0;{extra}">'
          f'{bar}<div style="padding:{pad - 4}px {pad}px {pad}px {pad}px">{inner}</div></section>')


def chapter_head(number, title, lede, sources):
  src = " ".join(f'<code>{esc(s)}</code>' for s in sources)
  return (f'<header style="margin:0 0 28px 0">'
          f'<div style="font-family:{LABEL};font-weight:600;font-size:13px;letter-spacing:2px;color:{C["LABEL_COLOR"]};text-transform:uppercase">'
          f'Outpost Commander · design reference · {number:02d}</div>'
          f'<h1 style="margin:6px 0 10px 0;font-family:{DISPLAY};font-weight:700;font-stretch:87.5%;font-size:56px;line-height:1.05;color:{C["TEXT_COLOR"]}">{esc(title)}</h1>'
          f'<p style="margin:0 0 10px 0;max-width:1000px;font-family:{READ};font-size:18px;line-height:1.5;color:{C["NUMBERS_COLOR"]}">{lede}</p>'
          f'<div class="md" style="font-size:13px;color:{C["LABEL_COLOR"]}">Sources: {src}</div></header>')


def doc(inner, width=1440, pad=64):
  return (f'<div data-root="1" style="width:{width}px;box-sizing:border-box;padding:{pad - 8}px {pad}px {pad}px {pad}px;'
          f'background:{C["FIELD_COLOR"]};color:{C["TEXT_COLOR"]};font-family:{READ}">{inner}</div>')


def footer_note():
  return (f'<p style="margin:8px 0 0 0;font-family:{MONO};font-size:12px;color:{C["LABEL_COLOR"]}">'
          f'As built at {HEAD["short"]} ({HEAD["commitDate"][:10]}). Code and data are ground truth; '
          f'disagreements are in Open items, never resolved silently.</p>')


def gem(size=13, fill=None):
  """The Ore gem (OreMark), cut-gem corners from GlyphAtlas.cpp:27."""
  f = fill or C["GOLD_COLOR"]
  pts = [(0.28, 0.14), (0.72, 0.14), (0.97, 0.40), (0.50, 0.94), (0.03, 0.40)]
  p = " ".join(f"{x * size:.2f},{y * size:.2f}" for x, y in pts)
  return (f'<svg width="{size}" height="{size}" viewBox="0 0 {size} {size}" aria-hidden="true" style="display:inline-block;vertical-align:-1px">'
          f'<polygon points="{p}" fill="{f}"/></svg>')


def ore(amount, size=14):
  return (f'<span style="white-space:nowrap">{gem(round(size * 0.85))}<span style="margin-left:4px;font-family:{MONO};font-weight:600;'
          f'font-size:{size}px;color:{C["GOLD_COLOR"]}">{esc(amount)}</span></span>')

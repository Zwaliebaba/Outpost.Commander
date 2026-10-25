#!/usr/bin/env python3
"""A4 -- screen layouts derived from the code, not dumped.

Transcribes the arithmetic of GameApp/Hud.cpp (Hud::Lay, Hud::LayMenu, LayDesigner, LayProduction, LayResearch,
LayControls, LayOrders, LayAway, AddButton, RetreatRow, Painter) at HEAD a053fe2 into Python and writes one JSON file per
screen state into DesignReference/_work/A4/layouts/.

The game was not built or run: this is a Linux container with no MSVC and no DirectWrite. Hud::Lay is stateless
(ADR-015, ADR-031), so its rectangles follow from its constants and its content; the one thing that cannot be reproduced is
text measurement (ADR-061: Hud::TextMetrics measures with the DirectWrite atlas). Every width that depends on a measured
text is estimated here from per-character em shares (TEXT ESTIMATE below) and the element carries
"measured": false and "estimate": "<how>". Content that comes from a match (Ore, names, counts) is example content built
from OutpostCommander/Assets/*.json and carries "sample": true; fixed strings are verbatim from the code.

Coordinates are pixels on a 1920x1080 back buffer, origin top-left: reference units at interface factor 1.0 (ADR-006,
ADR-070); the 110% and 125% states multiply by the factor, as Hud::Scale does.

Run:  python3 -I DesignReference/_work/A4/derive_layouts.py   (from the repository root)
"""

import json
import math
import os
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", ".."))
OUT = os.path.join(ROOT, "DesignReference", "_work", "A4", "layouts")
ASSETS = os.path.join(ROOT, "OutpostCommander", "Assets")
HEAD = "a053fe294d91dc885bbb061c2f34d76ca6897738"

# --------------------------------------------------------------------------------------------------------------------
# Constants, transcribed (Hud.h / Hud.cpp line numbers at HEAD)
# --------------------------------------------------------------------------------------------------------------------
REFERENCE_W, REFERENCE_H = 1920.0, 1080.0  # Hud.h:23-24
FACE_UNITS = {"Body": 20.0, "Title": 22.0, "Label": 13.0, "Name": 16.0, "Figure": 14.0, "LargeFigure": 28.0, "Detail": 13.0}  # Hud.h:46
FACE_FAMILY = {  # Hud.cpp:3003-3018
    "Body": "Segoe UI 600", "Title": "Bahnschrift 700 SemiCondensed", "Label": "Bahnschrift 600", "Name": "Bahnschrift 600",
    "Figure": "Cascadia Mono 600 (else Consolas)", "LargeFigure": "Cascadia Mono 700 (else Consolas)", "Detail": "Cascadia Mono 400 (else Consolas)"}
TITLE_BAR = 36.0          # Hud.h:645
KEPT_ON_SCREEN = 120.0    # Hud.h:646
TOPIC_COLUMNS, TOPICS_SHOWN = 2, 10  # Hud.h:400-401
SILENT_SECONDS = 2        # Hud.h:469
ORDERS_SHOWN, ORDER_ROWS = 6, 7      # Hud.h:526,528
SHIP_BARS_MOST = 24       # Hud.h:566
QUEUE_LIMIT = 5           # GameProtocol/Snapshot.h:42
DESIGN_NAME_LIMIT = 32    # GameProtocol/Command.h:68
REPORTED_ORDERS = 16      # GameProtocol/Snapshot.h:468
AWAY_LINES = 4 + REPORTED_ORDERS  # Hud.h:539

MARGIN, PADDING = 16.0, 12.0                                        # Hud.cpp:62-63
TITLE_LINE, NAME_LINE, FIGURE_LINE = 30.0, 22.0, 17.0               # Hud.cpp:65-67
ORE_PANEL_HEIGHT = 44.0                                             # Hud.cpp:72
SEL_MIN_W, SEL_MAX_W = 280.0, 560.0                                 # Hud.cpp:75-76
SELECTION_DESIGN_LINES = 5                                          # Hud.cpp:78
HEALTH_BAR, HEALTH_BAR_GAP = 6.0, 8.0                               # Hud.cpp:81-82
SHIP_BAR_W, SHIP_BAR_H, SHIP_BAR_GAP, SHIP_BARS_A_ROW = 17.0, 10.0, 4.0, 12  # Hud.cpp:87-90
ENTITY_BAR_W, ENTITY_BAR_H, ENTITY_BAR_GAP, ENTITY_BAR_LIFT = 32.0, 5.0, 2.0, 4.0  # Hud.cpp:98-101
BUTTON_PANEL_W, BUTTON_H, BUTTON_GAP, BUTTON_INSET = 380.0, 34.0, 6.0, 12.0     # Hud.cpp:104-107
BUTTON_BAR, BUTTON_BAR_INSET = 3.0, 2.0                             # Hud.cpp:109-110
MINIMAP_SIZE, MINIMAP_PADDING = 300.0, 8.0                          # Hud.cpp:114-115
SHIP_MARK, STRUCTURE_MARK, ORE_MARK, VIEW_LINE, ORE_OUTLINE = 3.0, 6.0, 8.0, 1.5, 2.0  # Hud.cpp:118-123
HINT_W = 640.0                                                      # Hud.cpp:126
COLUMN_LEAST = 380.0                                                # Hud.cpp:132
COLUMN_WIDEST_LINES = ["Shipyards · BUILDING 99 · WAITING 99 · IDLE 99",
                       "Shipyards at the fleet cap · Station upgrading, 100%",
                       "Research: every open topic done"]          # Hud.cpp:133-135
COLUMN_WIDEST_QUEUED = "+99"                                        # Hud.cpp:136
STATUS_LINES_KEPT, PANEL_GAP, TERRITORY_INSET = 3.0, 8.0, 10.0      # Hud.cpp:140-142
STATUS_BAR_ROW, STATUS_UNDER_BAR, STATUS_BESIDE_BAR, STATUS_BAR_GAP = 8.0, 4.0, 6.0, 8.0  # Hud.cpp:145-148
STATUS_PANEL_KEPT = 2 * TERRITORY_INSET + STATUS_LINES_KEPT * NAME_LINE + STATUS_BAR_ROW  # Hud.cpp:149 -> 94
ALERT_MARK = 14.0                                                   # Hud.cpp:151
BANNER_W = 520.0                                                    # Hud.cpp:154
CONNECTION_MOST = 360.0                                             # Hud.cpp:158
CONNECTION_WIDEST_SILENCE = "No word from the server · 999 s"  # Hud.cpp:159
MENU_W = 440.0                                                      # Hud.cpp:161
SECTOR_WASH_ALPHA, SECTOR_OUTLINE_ALPHA, SECTOR_HATCH_ALPHA, SECTOR_LINE = 0.12, 0.55, 0.35, 1.0  # Hud.cpp:42-45
CUT_OFF_DASH, CUT_OFF_GAP, CLAIMABLE_INSET = 6.0, 4.0, 3.0          # Hud.cpp:53-55
MEMORY_MARK, MEMORY_ARM = 8.0, 2.0                                  # Hud.cpp:58-59
CORNER = 12.0                                                       # Hud.cpp:844
TITLE_TEXT_INSET, CLOSE_TEXT_LEFT = 5.0, 11.0                       # Hud.cpp:846-847
DESIGNER_W, DESIGNER_INSET, DESIGNER_HEADER = 818.0, 24.0, 72.0     # Hud.cpp:913-916
NAME_ROW_TOP, NAME_ROW_H = 84.0, 38.0                               # Hud.cpp:917-918
CHIP_ROW_TOP, CHIP_H, CHIPS_LEFT, CHIP_W = 132.0, 26.0, 70.0, 216.0  # Hud.cpp:919-923
SLOTS_TOP, SLOT_GAP, SLOT_DIVIDER_LEFT, CARDS_LEFT = 168.0, 10.0, 108.0, 118.0  # Hud.cpp:924-927
CARD_W, CARD_H, CARD_GAP, CARDS_PER_LINE = 220.0, 80.0, 7.0, 3      # Hud.cpp:928-931
CARD_NUMBERS_TOP, CARD_NOTE_TOP, CARD_NOTE_ROOM, CARD_LOCK_FROM_FOOT = 34.0, 52.0, 16.0, 20.0  # Hud.cpp:934-937
SECTION_GAP, SECTION_LABEL_H, BAR_ROW_H, BAR_LEFT = 20.0, 26.0, 25.0, 104.0  # Hud.cpp:938-941
BAR_W, BAR_VALUE_RIGHT, DAMAGE_LEFT, DAMAGE_CARD_H, DAMAGE_CARD_GAP = 90.0, 316.0, 364.0, 96.0, 8.0  # Hud.cpp:943-947
FOOTER_H, SMALL_BUTTON, ARROW_GAP, LINE = 48.0, 24.0, 8.0, 1.0      # Hud.cpp:948-952
LABEL_TRACKING = 2.0                                                # Hud.cpp:954
ORE_MARK_SHARE, ORE_MARK_GAP = 0.6, 5.0                             # Hud.cpp:956-957
FIT_GAP, CHIP_PAD = 6.0, 4.0                                        # Hud.cpp:959,962
RETREAT_CELL_GAP, RETREAT_LINE_SHARE, RETREAT_LABEL = 4.0, 1.25, 20.0  # Hud.cpp:965-967
ORE_BOX_W, SMALL_BUTTON_TEXT_TOP, KEY_CAP, KEY_CAP_TEXT_TOP = 116.0, 4.0, 20.0, 2.0  # Hud.cpp:969-974
ARROWS_IN_TITLE_TOP, ARROWS_IN_HEADER_TOP = 6.0, 43.0               # Hud.cpp:1049-1050
RESEARCH_W = 560.0                                                  # Hud.cpp:1606
PRODUCTION_W = REFERENCE_W - 4 * MARGIN - DESIGNER_W - RESEARCH_W   # Hud.cpp:1607 -> 478
WINDOW_INSET, SECTION_TOP, SECTION_BODY_GAP = 24.0, 84.0, 20.0      # Hud.cpp:1608-1611
QUEUE_ROW_H, QUEUE_ROW_GAP, QUEUE_BAR_W = 30.0, 4.0, 120.0          # Hud.cpp:1612-1614
OPTION_H, OPTION_THIRD = 74.0, 50.0                                 # Hud.cpp:1616-1617
SEG_W, SEG_H, SEG_GAP, LETTER_GAP, GROUP_GAP, SEGMENTS = 6.0, 8.0, 2.0, 3.0, 10.0, 3  # Hud.cpp:1619-1624
TOPIC_LINES_TOP, TOPIC_LINE, TOPIC_FOOT, TOPIC_LEAST_LINES = 28.0, 18.0, 4.0, 2  # Hud.cpp:1627-1630
CARD_SPACING, CARD_INSET = 8.0, 12.0                                # Hud.cpp:1631-1633
WINDOWS_TOP = MARGIN + ORE_PANEL_HEIGHT + PANEL_GAP + STATUS_PANEL_KEPT + MARGIN  # Hud.cpp:1635 -> 178
CARDS_TOP = SECTION_TOP + SECTION_BODY_GAP                          # Hud.cpp:1636 -> 104
CONTROLS_W, CONTROLS_KEYS_W, CONTROLS_HEADING_GAP = 700.0, 240.0, 10.0  # Hud.cpp:1888-1890
ORDERS_W, ORDER_ROW, ORDER_VALUE_LEFT = 640.0, 30.0, 150.0          # Hud.cpp:1931-1933
ORDER_ARROW_TOP, ORDER_TEXT_TOP, ORDER_LABEL_TOP = 3.0, 6.0, 9.0    # Hud.cpp:1934-1936
ORDERS_FORM_TOP = TITLE_BAR + PADDING + NAME_LINE + (ORDERS_SHOWN + 1) * NAME_LINE + SECTION_GAP  # Hud.cpp:1939-1940 -> 244
ORDERS_ROWS_TOP = ORDERS_FORM_TOP + NAME_LINE                       # Hud.cpp:1941 -> 266
ORDERS_FIRES_TOP = ORDERS_ROWS_TOP + ORDER_ROWS * ORDER_ROW + PANEL_GAP  # Hud.cpp:1942 -> 484
ORDERS_GIVE_TOP = ORDERS_FIRES_TOP + NAME_LINE + PANEL_GAP          # Hud.cpp:1943 -> 514
ORDERS_H = ORDERS_GIVE_TOP + BUTTON_H + PADDING                     # Hud.cpp:1944 -> 560
AWAY_W = 600.0                                                      # Hud.cpp:1983
INTERFACE_STEPS = [1.0, 1.1, 1.25, 1.5, 1.75, 2.0]                  # Hud.h:861
DOT = " · "                                                    # Hud.cpp:208
TIMES = "×"                                                    # Hud.cpp:209
CHIPS_SHOWN = 3                                                     # Hud.cpp:211

# --------------------------------------------------------------------------------------------------------------------
# TEXT ESTIMATE -- stands in for DirectWrite (ADR-061). Per-character advance as a share of the em, rounded to whole
# pixels per character at the back buffer's scale, as UiPipeline::DrawText sets text; tracking rounded to a whole pixel.
# Line boxes: Bahnschrift ~1.2 em (Hud.cpp:1525 "about 19 units" for the 16-unit name face; Hud.cpp:1593 "up to 27 units"
# for the 22-unit title face), Cascadia Mono ~1.17 em (FIGURE_LINE_UNITS 17 for the 14-unit figure face, Hud.cpp:67).
# --------------------------------------------------------------------------------------------------------------------
LINE_BOX = {"Body": 25.0, "Title": 27.0, "Label": 16.0, "Name": 19.0, "Figure": 17.0, "LargeFigure": 33.0, "Detail": 16.0}
EST_TEXT = "width estimated from per-character em shares (Bahnschrift: caps 0.60, lower 0.50, digits 0.54, space 0.22; " \
           "title face x0.92 for SemiCondensed Bold; Cascadia Mono 0.586 em fixed), whole-pixel advances; height is the " \
           "face's estimated line box"
NARROW = set("ijlI.,:;'!|")
SEMI = set("frt()[]")
WIDE = set("mwMW%")


def em_share(face, ch):
    if face in ("Figure", "LargeFigure", "Detail"):
        return 0.586
    if ch == " ":
        e = 0.22
    elif ch in NARROW:
        e = 0.25
    elif ch in SEMI:
        e = 0.33
    elif ch in WIDE:
        e = 0.80
    elif ch == "·":
        e = 0.26
    elif ch == "×":
        e = 0.50
    elif ch.isdigit():
        e = 0.54
    elif ch.isupper():
        e = 0.60
    elif ch.islower():
        e = 0.50
    else:
        e = 0.42
    if face == "Title":
        e *= 0.92
    if face == "Body":
        e *= 1.05
    return e


def text_width(face, text, tracking_units=0.0, scale=1.0):
    """Hud::TextMetrics::Width (Hud.cpp:2055-2061), estimated: units at _scale."""
    if not text:
        return 0.0
    px = FACE_UNITS[face] * scale
    total = sum(round(em_share(face, c) * px) for c in text)
    total += round(tracking_units * scale) * (len(text) - 1)
    return total / scale


def fit(face, text, room, tracking_units=0.0, scale=1.0):
    """Hud::TextMetrics::Fit (Hud.cpp:2063-2086): whole, or cut short with three dots."""
    if text_width(face, text, tracking_units, scale) <= room:
        return text
    for n in range(len(text) - 1, -1, -1):
        start = text[:n].rstrip(" ")
        cut = start + "..."
        if text_width(face, cut, tracking_units, scale) <= room:
            return cut
    return ""


def wrap(face, text, room, scale=1.0):
    """Hud::TextMetrics::Wrap (Hud.cpp:2088-2112)."""
    lines, line = [], ""
    for word in [w for w in text.split(" ") if w]:
        joined = word if not line else line + " " + word
        if text_width(face, joined, 0.0, scale) <= room:
            line = joined
            continue
        if line:
            lines.append(line)
        line = fit(face, word, room, 0.0, scale)
    if line:
        lines.append(line)
    return lines


def thousands(v):
    return "{:,}".format(v)  # WithThousands, Hud.cpp:2114-2125


def tenths(v):  # Hud.cpp:179-183
    r = round(v * 10.0) / 10.0
    return "{:.0f}".format(r) if r == round(r) else "{:.1f}".format(r)


def mmss(seconds):  # MinutesAndSeconds, Hud.cpp:2127-2133
    h, m, s = seconds // 3600, (seconds // 60) % 60, seconds % 60
    return "{}:{:02}:{:02}".format(h, m, s) if h > 0 else "{}:{:02}".format(m, s)


# --------------------------------------------------------------------------------------------------------------------
# Layout output
# --------------------------------------------------------------------------------------------------------------------
LIST_RANK = {"panel": 0, "outline": 0, "bar": 0, "button": 0, "line": 1, "sprite": 2, "text": 3}


class Out:
    """The layout's element list: each element a rectangle in back-buffer pixels with its layer and draw list."""

    def __init__(self, scale=1.0, width=1920, height=1080):
        self.scale = scale
        self.width = width
        self.height = height
        self.elements = []
        self.layer = 0
        self.seq = 0
        self.ids = {}

    def uid(self, base):
        n = self.ids.get(base, 0)
        self.ids[base] = n + 1
        return base if n == 0 else "{}#{}".format(base, n + 1)

    def add(self, kind, eid, x, y, w, h, **kw):
        e = {"id": self.uid(eid), "kind": kind, "x": round(x, 2), "y": round(y, 2), "w": round(max(w, 0.0), 2),
             "h": round(max(h, 0.0), 2)}
        for k, v in kw.items():
            if v is not None and v is not False:
                e[k] = v
        e["layer"] = self.layer if kind != "entityBar" else -1
        e["drawList"] = {0: "panels", 1: "lines", 2: "sprites", 3: "texts"}.get(LIST_RANK.get(kind, 0)) if kind != "entityBar" else "bars"
        e["order"] = self.seq
        self.seq += 1
        self.elements.append(e)
        return e


class Painter:
    """Hud.cpp:1096-1257: draws in reference units from an origin, in pixels at _scale."""

    def __init__(self, out, corner, estimated_origin=None):
        self.o = out
        self.s = out.scale
        self.cx, self.cy = corner
        self.est = estimated_origin  # why this panel's origin is itself an estimate, if it is

    def tracking(self):
        return LABEL_TRACKING  # in units; Painter::Tracking, Hud.cpp:1109-1112

    def W(self, text, face, tracking=0.0):
        return text_width(face, text, tracking, self.s)

    def Fit(self, text, face, room, tracking=0.0):
        return fit(face, text, room, tracking, self.s)

    def px(self, l, t):
        return (self.cx + l) * self.s, (self.cy + t) * self.s

    def _est(self, kw, why=None):
        reasons = [r for r in (self.est, why) if r]
        if reasons:
            kw["measured"] = False
            kw["estimate"] = "; ".join(reasons)
        return kw

    def panel(self, eid, l, t, w, h, color, src, fill="solid", action=None, est=None, **kw):
        x, y = self.px(l, t)
        kind = "button" if action else ("bar" if kw.pop("bar", False) else "panel")
        kw = self._est(kw, est)
        return self.o.add(kind, eid, x, y, w * self.s, h * self.s, color=color, fill=fill, action=action, source=src, **kw)

    def outline(self, eid, l, t, w, h, color, src, est=None):
        x, y = self.px(l, t)
        kw = self._est({}, est)
        return self.o.add("outline", eid, x, y, w * self.s, h * self.s, edge=color, edgeWidth=round(LINE * self.s, 2), source=src, **kw)

    def text(self, eid, txt, l, t, color, face, src, tracking=0.0, sample=False, fmt=None, est=None):
        x, y = self.px(l, t)
        w = self.W(txt, face, tracking)
        kw = self._est({}, est)
        kw["measured"] = False
        kw["estimate"] = "; ".join([r for r in (self.est, est, EST_TEXT) if r])
        return self.o.add("text", eid, x, y, w * self.s, LINE_BOX[face] * self.s, label=txt, typeface=face,
                          size=round(FACE_UNITS[face] * self.s, 2), family=FACE_FAMILY[face], color=color,
                          tracking=round(tracking * self.s, 2) if tracking else None, source=src, sample=sample, format=fmt, **kw)

    def right_text(self, eid, txt, right, t, color, face, src, tracking=0.0, sample=False, fmt=None):
        left = right - self.W(txt, face, tracking)
        return self.text(eid, txt, left, t, color, face, src, tracking, sample, fmt, est="right edge exact at {} units; left follows the estimated width".format(round(right, 2)))

    def chip_width(self, txt, face):
        return self.W(txt, face) + 2 * CHIP_PAD

    def chip(self, eid, txt, l, t, face, line_units, src, sample=False, fmt=None):
        w = self.chip_width(txt, face)
        self.panel(eid + ".tag", l, t, w, line_units, "WARNING_COLOR", "Hud.cpp:1165 (Painter::Chip)", est="tag width = estimated text width + 2 x CHIP_PAD_UNITS")
        self.text(eid + ".words", txt, l + CHIP_PAD, t, "WINDOW_COLOR", face, "Hud.cpp:1166 (Painter::Chip); " + src, sample=sample, fmt=fmt)
        return w

    def sprite(self, eid, sprite, l, t, size, color, src, mirror=None):
        x, y = self.px(l, t)
        kw = self._est({})
        return self.o.add("sprite", eid, x, y, size * self.s, size * self.s, sprite=sprite, color=color, mirror=mirror, source=src, **kw)

    def small_button(self, eid, mark, l, t, enabled, action, src):
        self.panel(eid, l, t, SMALL_BUTTON, SMALL_BUTTON, "CARD_COLOR" if enabled else "DISABLED_BUTTON_COLOR",
                   "Hud.cpp:1192 (Painter::SmallButton); " + src, action=action if enabled else None, enabled=enabled)
        left = l + (SMALL_BUTTON - self.W(mark, "Label")) / 2.0
        self.text(eid + ".mark", mark, left, t + SMALL_BUTTON_TEXT_TOP, "TEXT_COLOR" if enabled else "DIM_TEXT_COLOR", "Label",
                  "Hud.cpp:1195-1196")

    def stepped(self, eid, subject, l, arrow_top, text_top, room, can_step, prev, nxt, color, face, src, tracking=0.0, sample=True):
        self.small_button(eid + ".previous", "<", l, arrow_top, can_step, prev, src)
        subject_left = l + SMALL_BUTTON + ARROW_GAP
        s = self.Fit(subject, face, room - 2 * (SMALL_BUTTON + ARROW_GAP), tracking)
        next_left = subject_left + self.W(s, face, tracking) + ARROW_GAP
        self.text(eid + ".subject", s, subject_left, text_top, color, face, "Hud.cpp:1208-1210 (Painter::Stepped); " + src, tracking, sample)
        save = self.est
        self.est = "; ".join([r for r in (save, "x follows the subject's estimated width") if r])
        self.small_button(eid + ".next", ">", next_left, arrow_top, can_step, nxt, src)
        self.est = save
        return next_left + SMALL_BUTTON

    def gem_figure_width(self, ore, face):
        return round(FACE_UNITS[face] * ORE_MARK_SHARE) + ORE_MARK_GAP + self.W(thousands(ore), face)

    def gem_figure_from(self, eid, ore, l, t, face, color, src, sample=True, est=None):
        size = FACE_UNITS[face]
        mark = round(size * ORE_MARK_SHARE)
        save = self.est
        if est:
            self.est = "; ".join([r for r in (save, est) if r])
        self.sprite(eid + ".gem", "OreMark", l, t + ((size * 1.25) - mark) / 2.0, mark, color, "Hud.cpp:1231-1232 (GemAndFigureFrom); " + src)
        self.text(eid + ".figure", thousands(ore), l + mark + ORE_MARK_GAP, t, color, face, "Hud.cpp:1233; " + src, sample=sample, fmt="WithThousands(ore)")
        self.est = save

    def gem_figure(self, eid, ore, right, t, face, color, src, sample=True):
        self.gem_figure_from(eid, ore, right - self.gem_figure_width(ore, face), t, face, color, src, sample,
                             est="right edge exact at {} units; gem x follows the figure's estimated width".format(round(right, 2)))

    def header_band(self, w, bottom):
        self.panel("header.band", 0, TITLE_BAR, w, bottom - TITLE_BAR, "TITLE_HATCH_COLOR", "Hud.cpp:1239 (Painter::HeaderBand)", fill="hatched")
        self.panel("header.edge", 0, bottom - LINE, w, LINE, "EDGE_COLOR", "Hud.cpp:1240")

    def ore_box(self, ore, right):
        self.panel("header.oreBox", right - ORE_BOX_W, 38, ORE_BOX_W, 30, "FIELD_COLOR", "Hud.cpp:1246 (Painter::OreBox)")
        self.outline("header.oreBox.edge", right - ORE_BOX_W, 38, ORE_BOX_W, 30, "EDGE_COLOR", "Hud.cpp:1247")
        self.gem_figure("header.ore", ore, right - 10, 39, "Title", "GOLD_COLOR", "Hud.cpp:1248; Snapshot::ore (Snapshot.h:539)")


def corners(out, x, y, w, h, src):
    c = CORNER * out.scale
    for right in (False, True):
        for bottom in (False, True):
            out.add("sprite", "corner." + ("b" if bottom else "t") + ("r" if right else "l"), x + w - c if right else x,
                    y + h - c if bottom else y, c, c, sprite="Corner", color="WINDOW_EDGE_COLOR",
                    mirror={"x": right, "y": bottom}, source="Hud.cpp:852-866 (AddCorners); " + src)


def frame(out, eid, corner_units, w, h, src, est=None):
    """Hud.cpp:903-908 (Frame): a HUD panel in a window's look."""
    s = out.scale
    x, y, ww, hh = corner_units[0] * s, corner_units[1] * s, w * s, h * s
    kw = {}
    if est:
        kw = {"measured": False, "estimate": est}
    out.add("panel", eid, x, y, ww, hh, color="WINDOW_COLOR", fill="solid", source="Hud.cpp:905 (Frame); " + src, **kw)
    corners(out, x, y, ww, hh, src)
    return Painter(out, corner_units, est)


def open_window(out, kind, corner_units, w, body_h, src):
    """Hud.cpp:871-899 (OpenWindow): frame, hatched title bar, close box, x, corners; a new layer."""
    out.layer += 1
    s = out.scale
    x, y = corner_units[0] * s, corner_units[1] * s
    tb = TITLE_BAR * s
    out.add("panel", "window.frame", x, y, w * s, tb + body_h * s, color="WINDOW_COLOR", fill="solid", window=kind,
            source="Hud.cpp:879,888 (OpenWindow frame); " + src)
    out.add("panel", "window.titleBar", x, y, w * s - tb, tb, color="TITLE_HATCH_COLOR", fill="hatched", action="drag window",
            source="Hud.cpp:880,889 (title bar, dragged by it: GameClient.cpp:993-996)")
    out.add("button", "window.closeBox", x + w * s - tb, y, tb, tb, color="CLOSE_BOX_COLOR", fill="solid", action="close window",
            source="Hud.cpp:881,890 (close box; GameClient.cpp:987-991)")
    out.add("text", "window.close", x + w * s - tb + CLOSE_TEXT_LEFT * s, y + TITLE_TEXT_INSET * s, text_width("Title", "×", 0, s) * s,
            LINE_BOX["Title"] * s, label="×", typeface="Title", size=round(22 * s, 2), family=FACE_FAMILY["Title"],
            color="TEXT_COLOR", source="Hud.cpp:849,894-895 (CLOSE_MARK)", measured=False, estimate=EST_TEXT)
    corners(out, x, y, w * s, tb + body_h * s, src)
    return Painter(out, corner_units)


def keep_on_screen(corner, w, sw, sh):  # Hud.cpp:2420-2426
    kept = min(KEPT_ON_SCREEN, w)
    return (min(max(corner[0], kept - w), max(kept - w, sw - kept)), min(max(corner[1], 0.0), max(0.0, sh - TITLE_BAR)))


# --------------------------------------------------------------------------------------------------------------------
# AddButton (Hud.cpp:1264-1323) and RetreatRow (Hud.cpp:1328-1346)
# --------------------------------------------------------------------------------------------------------------------
def add_button(out, eid, area_units, button, hovered=False, src="", est=None):
    l, t, w, h = area_units
    p = Painter(out, (l, t), est)
    enabled = button.get("enabled", True)
    selected = button.get("selected", False)
    face_color = "PICKED_COLOR" if selected else ("CARD_COLOR" if enabled else "FIELD_COLOR")
    p.panel(eid, 0, 0, w, h, face_color, "Hud.cpp:1270-1275 (AddButton face); " + src, action=button.get("action") if enabled else None,
            enabled=enabled, selected=selected, hovered=hovered)
    p.outline(eid + ".edge", 0, 0, w, h, "PICKED_EDGE_COLOR" if selected else ("HOVER_EDGE_COLOR" if hovered else "EDGE_COLOR"), "Hud.cpp:1276")
    if button.get("progress") is not None:
        bw = w - 2 * BUTTON_BAR_INSET
        bt = h - BUTTON_BAR_INSET - BUTTON_BAR
        p.panel(eid + ".progress.track", BUTTON_BAR_INSET, bt, bw, BUTTON_BAR, "BAR_TRACK_COLOR", "Hud.cpp:1281", bar=True)
        p.panel(eid + ".progress.fill", BUTTON_BAR_INSET, bt, bw * min(max(button["progress"], 0), 1000) / 1000.0, BUTTON_BAR, "BAR_FILL_COLOR",
                "Hud.cpp:1282-1285; EntityView::upgradePermille (Snapshot.h:108)", bar=True, sample=True)
    label_full = button["label"]
    split = label_full.find("|")
    label = label_full if split < 0 else label_full[:split]
    label_color = "TEXT_COLOR" if enabled else "LOCKED_TEXT_COLOR"
    label_top = (h - NAME_LINE) / 2.0
    figure_top = (h - FIGURE_LINE) / 2.0
    room = w - 2 * BUTTON_INSET
    sample = button.get("sample", False)
    if button.get("key"):
        cap_left = w - BUTTON_INSET - KEY_CAP
        cap_top = (h - KEY_CAP) / 2.0
        p.panel(eid + ".keyCap", cap_left, cap_top, KEY_CAP, KEY_CAP, "FIELD_COLOR", "Hud.cpp:1296-1298 (key cap)")
        p.outline(eid + ".keyCap.edge", cap_left, cap_top, KEY_CAP, KEY_CAP, "ROW_LABEL_COLOR", "Hud.cpp:1299")
        p.text(eid + ".keyCap.letter", button["key"], cap_left + (KEY_CAP - p.W(button["key"], "Label")) / 2.0, cap_top + KEY_CAP_TEXT_TOP,
               "ROW_LABEL_COLOR", "Label", "Hud.cpp:1300-1301; KeyBindings.h")
        room -= KEY_CAP + FIT_GAP
    figure = "" if split < 0 else label_full[split + 1:]
    costed = split >= 0 and figure.isdigit()
    if split >= 0 and not enabled and button.get("note"):
        p.right_text(eid + ".note", button["note"], w - BUTTON_INSET, figure_top, "LABEL_COLOR", "Label", "Hud.cpp:1309 (note in place of the cost)")
        p.text(eid + ".label", p.Fit(label, "Name", room - p.W(button["note"], "Label") - FIT_GAP), BUTTON_INSET, label_top, label_color, "Name",
               "Hud.cpp:1310-1311; " + src, sample=sample)
        return
    if not costed:
        p.text(eid + ".label", p.Fit(label, "Name", room), BUTTON_INSET, label_top, label_color, "Name", "Hud.cpp:1316; " + src, sample=sample)
        return
    cost = int(figure)
    cw = p.gem_figure_width(cost, "Figure")
    p.text(eid + ".label", p.Fit(label, "Name", room - cw - FIT_GAP), BUTTON_INSET, label_top, label_color, "Name", "Hud.cpp:1320-1321; " + src,
           sample=sample)
    p.gem_figure(eid + ".cost", cost, w - BUTTON_INSET, figure_top, "Figure", "GOLD_COLOR" if enabled else "LOCKED_TEXT_COLOR", "Hud.cpp:1322")


RETREAT_CELLS = [("Quarter", "25%"), ("Half", "50%"), ("Never", "Never")]  # Hud.cpp:463-477


def retreat_row(p, eid, l, t, w, h, lit, kind, face, tracking, hovered_cell=None, src=""):
    cell = (w - 2 * RETREAT_CELL_GAP) / 3.0
    for i, (name, words) in enumerate(RETREAT_CELLS):
        left = l + i * (cell + RETREAT_CELL_GAP)
        on = lit == name
        p.panel("{}.{}".format(eid, name.lower()), left, t, cell, h, "PICKED_COLOR" if on else "CARD_COLOR",
                "Hud.cpp:1339 (RetreatRow); " + src, action="{}({})".format(kind, name), selected=on)
        p.outline("{}.{}.edge".format(eid, name.lower()), left, t, cell, h,
                  "PICKED_EDGE_COLOR" if on else ("HOVER_EDGE_COLOR" if hovered_cell == name else "EDGE_COLOR"), "Hud.cpp:1340")
        shown = words.upper() if face == "Label" else words
        line = round(FACE_UNITS[face] * RETREAT_LINE_SHARE)
        p.text("{}.{}.words".format(eid, name.lower()), shown, left + (cell - p.W(shown, face, tracking)) / 2.0, t + (h - line) / 2.0,
               "TEXT_COLOR", face, "Hud.cpp:1341-1344, 470-476", tracking)


def health_color(share):  # Hud.cpp:1026-1029
    return "HEALTH_GOOD_COLOR" if share > 0.5 else ("HEALTH_FAIR_COLOR" if share > 0.25 else "HEALTH_POOR_COLOR")


# --------------------------------------------------------------------------------------------------------------------
# Data, for sample content: OutpostCommander/Assets/Tuning.json and Map.json
# --------------------------------------------------------------------------------------------------------------------
with open(os.path.join(ASSETS, "Tuning.json"), encoding="utf-8") as f:
    TUNING = json.load(f)
with open(os.path.join(ASSETS, "Map.json"), encoding="utf-8") as f:
    MAP = json.load(f)
with open(os.path.join(ASSETS, "Camera.json"), encoding="utf-8") as f:
    CAMERA = json.load(f)

HULLS = TUNING["hulls"]
DRIVES = TUNING["drives"]
WEAPONS = TUNING["weapons"]
MODULES = TUNING["modules"]
HULL_LEVEL = {}
for s in TUNING["structures"]:
    if s["kind"] == "Shipyard":
        for h in s.get("hulls", []):
            HULL_LEVEL[h] = 1
        for i, lv in enumerate(s.get("levels", [])):
            for h in lv.get("hulls", []):
                HULL_LEVEL[h] = i + 2
UNLOCKED_BY = {}
for r in TUNING["research"]:
    for k in ("unlockHull", "unlockDrive", "unlockWeapon"):
        if k in r["effect"]:
            UNLOCKED_BY[(k, r["effect"][k])] = r["name"]


def find(lst, i):
    return next(x for x in lst if x["id"] == i)


def abbreviation(name):  # GameProtocol/DesignStats.h:25-38
    out, start = "", True
    for c in name:
        letter = c.isalpha()
        if letter and start:
            out += c.upper()
        start = c == " "
    return out


def stats(h, d, w, m=None):  # DesignStatsOf, GameProtocol/DesignStats.h:76-94, at the tuning's base numbers
    hull, drive, weapon = find(HULLS, h), find(DRIVES, d), find(WEAPONS, w)
    module = find(MODULES, m) if m else None
    return {"hp": round(hull["hitPoints"] * 100 * drive["hitPointsFactor"]), "armor": hull["armor"] * 100,
            "speed": hull["speedMetersPerSecond"] * drive["speedFactor"] * (module["speedFactor"] if module else 1.0),
            "cost": hull["cost"] + drive["cost"] + weapon["cost"] + (module["cost"] if module else 0), "build": hull["buildSeconds"],
            "damage": weapon["damage"] * 100, "interval": weapon["fireIntervalSeconds"], "range": weapon["rangeMeters"],
            "splash": weapon["splashRadiusMeters"], "sight": module["sightMeters"] if module else 0.0}


def ships_reached(splash, radius):  # DesignStats.h ShipsReached
    spacing = 3.0 * radius
    if splash <= 0 or spacing <= 0:
        return 1
    steps = int(splash / spacing)
    n = 0
    for r in range(-steps, steps + 1):
        for c in range(-steps, steps + 1):
            n += 1 if (c * spacing) ** 2 + (r * spacing) ** 2 <= splash * splash else 0
    return n


def formation_dps(st, hull):
    hit = max(st["damage"] // 4, st["damage"] - hull["armor"] * 100)
    return hit / 100.0 / st["interval"] * ships_reached(st["splash"], hull["footprintRadiusMeters"])


def best_of_all():  # Hud.cpp:494-527
    best = {"hp": 0, "armor": 0, "speed": 0, "range": 0, "cost": 0, "build": 0, "damage": [0.0] * len(HULLS)}
    for h in HULLS:
        for d in DRIVES:
            for w in WEAPONS:
                st = stats(h["id"], d["id"], w["id"])
                for k in ("hp", "armor", "speed", "range", "cost", "build"):
                    best[k] = max(best[k], st[k])
                for i, hh in enumerate(HULLS):
                    best["damage"][i] = max(best["damage"][i], formation_dps(st, hh))
    best["cost"] += max(m["cost"] for m in MODULES)
    best["sensor"] = max(m["sightMeters"] for m in MODULES)
    return best


BEST = best_of_all()


def rating(share):  # Hud.cpp:535-538
    return "Good" if share >= 2 / 3 else ("Fair" if share >= 1 / 3 else "Poor")


def design_code(h, d, w, m=None):  # Hud.cpp:422-435
    code = "·".join([abbreviation(find(HULLS, h)["name"]), abbreviation(find(DRIVES, d)["name"]), abbreviation(find(WEAPONS, w)["name"])])
    if m:
        code += "·" + abbreviation(find(MODULES, m)["name"])
    return code


STARTING = TUNING["startingDesigns"]  # Swarm, Picket, Brawler, Lancer (ADR-069)


def research_names():
    return [r["name"] for r in TUNING["research"]]


def column_units(scale):  # Hud.cpp:3134-3139
    col = COLUMN_LEAST
    queued = text_width("Figure", COLUMN_WIDEST_QUEUED, 0, scale) + FIT_GAP
    for n in research_names():
        col = max(col, text_width("Name", n, 0, scale) + queued + 2 * PADDING)
    for line in COLUMN_WIDEST_LINES:
        col = max(col, text_width("Name", line, 0, scale) + 2 * CHIP_PAD + 2 * PADDING)
    return col


COLUMN_EST = "column width = max(380, longest research name + '+99', widest of COLUMN_WIDEST_LINES) measured by DirectWrite (Hud.cpp:3134-3139); estimated"


# --------------------------------------------------------------------------------------------------------------------
# Minimap (Hud.cpp:3456-3630)
# --------------------------------------------------------------------------------------------------------------------
def minimap_pixel(out, p, clamp=True):
    s = out.scale
    left = (MARGIN + MINIMAP_PADDING) * s
    top = out.height - (MARGIN + MINIMAP_SIZE) * s + MINIMAP_PADDING * s
    inner = (MINIMAP_SIZE - 2 * MINIMAP_PADDING) * s
    size = MAP["sizeMeters"]
    half = size / 2.0
    x = (p[0] + half) / size
    y = (half - p[1]) / size
    if clamp:
        x, y = min(max(x, 0.0), 1.0), min(max(y, 0.0), 1.0)
    return left + x * inner, top + y * inner


def camera_view_corners(focus):
    """The ground under the screen's four corners at the default zoom, unturned (Camera.cpp:160-236, Camera.json)."""
    vw = CAMERA["defaultViewWidthMeters"]
    along = (vw - CAMERA["minimumViewWidthMeters"]) / (CAMERA["maximumViewWidthMeters"] - CAMERA["minimumViewWidthMeters"])
    pitch = math.radians(CAMERA["pitchAtMinimumDegrees"] + (CAMERA["pitchAtMaximumDegrees"] - CAMERA["pitchAtMinimumDegrees"]) * along)
    aspect = 1920 / 1080
    half_v = math.radians(CAMERA["verticalFieldOfViewDegrees"]) / 2
    dist = vw / (2 * math.tan(half_v) * aspect)
    fwd = (0.0, 1.0)  # INITIAL_YAW_RADIANS = pi/2 (Camera.cpp:14): forward +z
    eye = (focus[0] - fwd[0] * dist * math.cos(pitch), dist * math.sin(pitch), focus[1] - fwd[1] * dist * math.cos(pitch))
    look = (focus[0] - eye[0], -eye[1], focus[1] - eye[2])
    n = math.sqrt(sum(c * c for c in look))
    look = tuple(c / n for c in look)
    right = (1.0, 0.0, 0.0)
    up = (look[1] * right[2] - look[2] * right[1], look[2] * right[0] - look[0] * right[2], look[0] * right[1] - look[1] * right[0])
    up = tuple(-c for c in up) if up[1] < 0 else up
    tv, th = math.tan(half_v), math.tan(half_v) * aspect
    pts = []
    for sx, sy in ((-1, 1), (1, 1), (1, -1), (-1, -1)):  # screen corners (0,0), (w,0), (w,h), (0,h)
        d = tuple(look[i] + sx * th * right[i] + sy * tv * up[i] for i in range(3))
        t = -eye[1] / d[1]
        pts.append((eye[0] + d[0] * t, eye[2] + d[2] * t))
    return pts


def lay_minimap(out, content):
    s = out.scale
    left, top = MARGIN * s, out.height - (MARGIN + MINIMAP_SIZE) * s
    out.add("panel", "minimap.frame", left, top, MINIMAP_SIZE * s, MINIMAP_SIZE * s, color="WINDOW_COLOR", fill="solid",
            source="Hud.cpp:3461 (Frame)")
    corners(out, left, top, MINIMAP_SIZE * s, MINIMAP_SIZE * s, "minimap")
    inner = (MINIMAP_SIZE - 2 * MINIMAP_PADDING) * s
    mx, my = left + MINIMAP_PADDING * s, top + MINIMAP_PADDING * s
    out.add("panel", "minimap.map", mx, my, inner, inner, color="MAP_COLOR", fill="solid", action="left: look there; right: send selection",
            source="Hud.cpp:3462-3465; clicks GameClient.cpp:1011-1022; Snapshot::mapSizeMeters (Snapshot.h:545)")
    ppm = inner / MAP["sizeMeters"]
    side_color = {"Own": "OWN_COLOR", "Enemy": "ENEMY_COLOR", "Pirate": "PIRATE_COLOR"}

    def sector_rect(sec):
        a = minimap_pixel(out, (sec["minXMeters"], sec["maxZMeters"]))
        b = minimap_pixel(out, (sec["maxXMeters"], sec["minZMeters"]))
        return a[0], a[1], b[0] - a[0], b[1] - a[1]

    for sec in MAP["sectors"]:
        side = content["sectors"].get(sec["name"], {}).get("side", "Neutral")
        if side != "Neutral":
            x, y, w, h = sector_rect(sec)
            out.add("panel", "minimap.sector.wash." + sec["name"], x, y, w, h, color="{}@SECTOR_WASH_ALPHA(0.12)".format(side_color[side]),
                    fill="solid", sample=True, source="Hud.cpp:3481-3485; SectorView::holder (Snapshot.h:299), guarded (Snapshot.h:308)")
    # marks: the neutral first, then the sided (Hud.cpp:3486-3497)
    for sided in (False, True):
        for m in content["marks"]:
            if (m["side"] != "Neutral") != sided or m.get("remembered"):
                continue
            smallest = {"Ship": SHIP_MARK, "Asteroid": ORE_MARK}.get(m["kind"], STRUCTURE_MARK) * s
            side = max(smallest, 2.0 * m["radius"] * ppm)
            at = minimap_pixel(out, m["pos"])
            if m.get("dry"):
                color = "DRY_COLOR"
            elif m["side"] in side_color:
                color = side_color[m["side"]]
            else:
                color = {"Derelict": "DERELICT_COLOR", "AsteroidField": "ASTEROID_FIELD_COLOR", "Asteroid": "ORE_ASTEROID_COLOR"}.get(m["kind"], "NEUTRAL_COLOR")
            src = "Hud.cpp:3498-3528 (marks); EntityView position/radius/owner (Snapshot.h:77-95)"
            if m["kind"] == "Asteroid" and m["side"] == "Neutral":
                out.add("outline", "minimap.mark.ore", at[0] - side / 2, at[1] - side / 2, side, side, edge=color, edgeWidth=ORE_OUTLINE * s,
                        source="Hud.cpp:3517-3525 (ore asteroid outlined, ORE_OUTLINE_UNITS 2); " + src, sample=m.get("sample", True))
            else:
                out.add("panel", "minimap.mark." + m["kind"].lower(), at[0] - side / 2, at[1] - side / 2, side, side, color=color, fill="solid",
                        source=src, sample=m.get("sample", True))
    if content.get("fog", True):
        out.add("panel", "minimap.fog", mx, my, inner, inner, color="black, opacity from the fog texture's shade", fill="fog",
                source="Hud.cpp:3531-3535; drawn GameClient.cpp:1310-1316 from the ground mask (ADR-052)")
    sl = SECTOR_LINE * s
    for sec in MAP["sectors"]:
        x, y, w, h = sector_rect(sec)
        out.add("outline", "minimap.sector.lattice." + sec["name"], x, y, w, h, edge="LATTICE_COLOR", edgeWidth=sl,
                source="Hud.cpp:3567-3568; Map.json sectors")
    for sec in MAP["sectors"]:
        info = content["sectors"].get(sec["name"], {})
        side = info.get("side", "Neutral")
        if side == "Neutral":
            continue
        x, y, w, h = sector_rect(sec)
        out.add("outline", "minimap.sector.held." + sec["name"], x, y, w, h, edge="{}@SECTOR_OUTLINE_ALPHA(0.55)".format(side_color[side]),
                edgeWidth=sl, pattern="dashed 6 on 4 off" if info.get("cutOff") else "solid", sample=True,
                source="Hud.cpp:3569-3577 (dashed while cut off, CUT_OFF_DASH_UNITS/GAP_UNITS Hud.cpp:53-54)")
        if info.get("suppressed"):
            out.add("panel", "minimap.sector.suppressed." + sec["name"], x, y, w, h, color="{}@SECTOR_HATCH_ALPHA(0.35)".format(side_color[side]),
                    fill="hatched", sample=True, source="Hud.cpp:3578-3579")
    for sec in MAP["sectors"]:
        if content["sectors"].get(sec["name"], {}).get("claimable"):
            x, y, w, h = sector_rect(sec)
            i = CLAIMABLE_INSET * s
            out.add("outline", "minimap.sector.claimable." + sec["name"], x + i, y + i, w - 2 * i, h - 2 * i, edge="OWN_COLOR", edgeWidth=sl,
                    sample=True, source="Hud.cpp:3584-3592 (while placing a Relay)")
    for m in content["marks"]:
        if not m.get("remembered"):
            continue
        at = minimap_pixel(out, m["pos"])
        a, arm = MEMORY_MARK * s, MEMORY_ARM * s
        col = "DERELICT_COLOR" if m["side"] == "Neutral" else side_color[m["side"]]
        out.add("panel", "minimap.memory.across", at[0] - a / 2, at[1] - arm / 2, a, arm, color=col, fill="solid", sample=True,
                source="Hud.cpp:3595-3605 (remembered: a cross)")
        out.add("panel", "minimap.memory.down", at[0] - arm / 2, at[1] - a / 2, arm, a, color=col, fill="solid", sample=True, source="Hud.cpp:3604")
    for i, (txt, pos) in enumerate(content.get("alerts", [])):
        at = minimap_pixel(out, pos)
        half = ALERT_MARK * s / 2
        out.add("outline", "minimap.alert", at[0] - half, at[1] - half, 2 * half, 2 * half, edge="WARNING_COLOR", edgeWidth=sl, sample=True,
                source="Hud.cpp:3608-3617 (ALERT_MARK_UNITS 14)")
    view = content.get("view")
    if view:
        pts = [minimap_pixel(out, p, clamp=False) for p in view]
        for i in range(4):
            a, b = pts[i], pts[(i + 1) % 4]
            out.add("line", "minimap.view", min(a[0], b[0]), min(a[1], b[1]), abs(a[0] - b[0]), abs(a[1] - b[1]),
                    points=[[round(a[0], 2), round(a[1], 2)], [round(b[0], 2), round(b[1], 2)]], widthPixels=VIEW_LINE * s, color="VIEW_COLOR",
                    sample=True, source="Hud.cpp:3621-3629 (view's four ground corners, cut at the minimap's edge); GameClient.cpp:1040-1054",
                    estimate="corners computed from Camera.cpp:160-236 and Camera.json at the default 500 m view, unturned, focus at the sample's")


# --------------------------------------------------------------------------------------------------------------------
# The HUD: Hud::Lay (Hud.cpp:3075-3689), less the windows
# --------------------------------------------------------------------------------------------------------------------
def lay_hud(out, c, hovered=None):
    s = out.scale
    sw, sh = out.width / s, out.height / s
    col = column_units(s)

    # Entity bars, before every layer (Hud.cpp:3095-3126); at the back buffer's scale, not the factor (ADR-088 d5).
    bs = min(out.width / REFERENCE_W, out.height / REFERENCE_H)
    for eb in c.get("entityBars", []):
        bw, bh, gap, lift = round(ENTITY_BAR_W * bs), max(round(ENTITY_BAR_H * bs), 1), round(ENTITY_BAR_GAP * bs), round(ENTITY_BAR_LIFT * bs)
        left = round(eb["foot"][0] - bw / 2.0)
        bottom = round(eb["foot"][1]) - lift
        for kind, share, fill in (("build", eb.get("built"), "BUILD_BAR_COLOR"), ("health", eb.get("health"), None)):
            if share is None:
                continue
            top = bottom - bh
            out.add("entityBar", "entityBar.{}.back".format(kind), left, top, bw, bh, color="side color x HEALTH_BACK_SHADE 0.3 (GameClient.cpp:97-98)",
                    sample=True, source="Hud.cpp:3109-3117")
            out.add("entityBar", "entityBar.{}.fill".format(kind), left, top, bw * share, bh, color=fill or health_color(share), sample=True,
                    source="Hud.cpp:3115, 3118-3124")
            bottom = top - gap

    # Ore panel (Hud.cpp:3143-3157)
    p = frame(out, "ore.frame", (MARGIN, MARGIN), col, ORE_PANEL_HEIGHT, "Ore panel", est=COLUMN_EST)
    p.est = None
    p.gem_figure_from("ore.stockpile", c["ore"], PADDING, (ORE_PANEL_HEIGHT - TITLE_LINE) / 2.0, "Title", "GOLD_COLOR",
                      "Hud.cpp:3145; Snapshot::ore (Snapshot.h:539)")
    inc = c["income"]
    inc_text = "+{}/s".format(inc // 100) if inc % 100 == 0 else "+{:.1f}/s".format(inc / 100.0)
    inc_top = (ORE_PANEL_HEIGHT - FIGURE_LINE) / 2.0 + 2.0
    p.est = "right edge = column width - 12, column width estimated"
    if inc > 0:
        p.right_text("ore.income", inc_text, col - PADDING, inc_top, "NUMBERS_COLOR", "Figure",
                     "Hud.cpp:3147-3151; Snapshot::oreIncomeHundredthsPerSecond (Snapshot.h:540)", sample=True, fmt="+{}/s or +{:.1f}/s")
    else:
        p.chip("ore.income.chip", inc_text, col - PADDING - p.chip_width(inc_text, "Figure"), inc_top, "Figure", FIGURE_LINE,
               "Hud.cpp:3152-3156 (an income of nothing is a warning chip, ADR-047 d5, ADR-085 d1)", sample=False)
    p.est = None

    under_ore = MARGIN + ORE_PANEL_HEIGHT + PANEL_GAP
    under_status = under_ore + STATUS_PANEL_KEPT + PANEL_GAP

    # Territory (Hud.cpp:3167-3213)
    terr_units = 0.0
    if c.get("territory"):
        t = c["territory"]
        tickets = t.get("own_tickets") is not None and t.get("enemy_tickets") is not None
        rows = 1 + (1 if tickets else 0) + (1 if t.get("drain") else 0)
        h = 2 * TERRITORY_INSET + rows * NAME_LINE
        terr_units = h + PANEL_GAP
        p = frame(out, "territory.frame", (MARGIN, under_status), col, h, "territory panel", est=COLUMN_EST)
        p.est = None

        def row(rid, label, note, own, enemy, top, src):
            ft = top + (NAME_LINE - FIGURE_LINE) / 2.0 + 2.0
            right = col - PADDING
            own_s = "{} : ".format(own)
            own_right = right - p.W(enemy, "Figure")
            room = own_right - p.W(own_s, "Figure") - PADDING - FIT_GAP
            lab = p.Fit(label, "Name", room)
            p.text(rid + ".label", lab, PADDING, top, "TEXT_COLOR", "Name", "Hud.cpp:3188")
            if note:
                nl = PADDING + p.W(lab, "Name") + FIT_GAP
                p.text(rid + ".note", p.Fit(note, "Name", PADDING + room - nl), nl, top, "LABEL_COLOR", "Name", "Hud.cpp:3189-3190, 3194-3195", sample=True,
                       fmt="cap {} of {} / of {}")
            p.est = "right edge = column width - 12, column width estimated"
            p.right_text(rid + ".enemy", enemy, right, ft, "ENEMY_COLOR", "Figure", src, sample=True)
            p.right_text(rid + ".own", own_s, own_right, ft, "OWN_COLOR", "Figure", src, sample=True, fmt="{} : ")
            p.est = None

        note = "cap {} of {}".format(t["cap"], t["nodes"]) if t["cap"] > 0 else "of {}".format(t["nodes"])
        row("territory.nodes", "Nodes", note, str(t["own_nodes"]), str(t["enemy_nodes"]), TERRITORY_INSET,
            "Hud.cpp:3196; SectorView::holder counted (Hud.cpp:2483-2497); Snapshot::nodeCap (Snapshot.h:562)")
        top = TERRITORY_INSET + NAME_LINE
        if tickets:
            row("territory.tickets", "Tickets", None, thousands(t["own_tickets"]), thousands(t["enemy_tickets"]), top,
                "Hud.cpp:3198-3202; Snapshot::tickets (Snapshot.h:581)")
            top += NAME_LINE
        if t.get("drain"):
            if t.get("drain_warns"):
                p.chip("territory.drain.chip", p.Fit(t["drain"], "Name", col - 2 * PADDING - 2 * CHIP_PAD), PADDING - CHIP_PAD, top, "Name", NAME_LINE,
                       "Hud.cpp:3205-3208; text Hud.cpp:2506-2526", sample=True, fmt="{You|Enemy} -{} a minute · out in {}:{:02}")
            else:
                p.text("territory.drain", p.Fit(t["drain"], "Name", col - 2 * PADDING), PADDING, top, "TEXT_COLOR", "Name",
                       "Hud.cpp:3211; text Hud.cpp:2506-2526", sample=t["drain"] != "No drain")

    # Alerts (Hud.cpp:3217-3238)
    if c.get("alerts"):
        top_units = under_status + terr_units
        h = 2 * TERRITORY_INSET + len(c["alerts"]) * NAME_LINE
        p = frame(out, "alerts.frame", (MARGIN, top_units), col, h, "alerts panel", est=COLUMN_EST)
        p.est = None
        for i, (txt, _) in enumerate(c["alerts"]):
            top = TERRITORY_INSET + i * NAME_LINE
            if i == 0:
                p.chip("alerts.newest", p.Fit(txt, "Name", col - 2 * PADDING - 2 * CHIP_PAD), PADDING - CHIP_PAD, top, "Name", NAME_LINE,
                       "Hud.cpp:3228-3233; text Alerts.cpp:78-98", sample=True)
            else:
                p.text("alerts.line", p.Fit(txt, "Name", col - 2 * PADDING), PADDING, top, "TEXT_COLOR", "Name", "Hud.cpp:3235-3236; text Alerts.cpp:78-98",
                       sample=True)

    # Status panel (Hud.cpp:3241-3308)
    if c.get("status"):
        def row_units(line):
            return NAME_LINE + (STATUS_BAR_ROW if line.get("bar") and line["bar"].get("under") else 0.0)
        h = 2 * TERRITORY_INSET + sum(row_units(l) for l in c["status"])
        p = frame(out, "status.frame", (MARGIN, under_ore), col, h, "status panel", est=COLUMN_EST)
        p.est = None
        right = col - PADDING
        top = TERRITORY_INSET
        for i, line in enumerate(c["status"]):
            lu = row_units(line)
            rt = 0.0 if i == 0 else top
            rb = h if i + 1 == len(c["status"]) else top + lu
            hov = hovered is not None and hovered == line["action"]
            p.panel("status.row{}".format(i), 0, rt, col, rb - rt, "CARD_COLOR" if hov else None,
                    "Hud.cpp:3259-3263 (the line's row takes the click; drawn on CARD_COLOR only under the pointer, else a press area only)",
                    fill="solid" if hov else "none", action=line["action"], hovered=hov, est="width = column width, estimated")
            words_right = right
            p.est = "right edge = column width - 12, column width estimated"
            bar = line.get("bar")
            if bar and bar.get("figure"):
                p.right_text("status.row{}.figure".format(i), bar["figure"], right, top + (NAME_LINE - FIGURE_LINE) / 2.0 + 2.0, "NUMBERS_COLOR", "Figure",
                             "Hud.cpp:3266-3269", sample=True)
                words_right -= p.W(bar["figure"], "Figure") + (FIT_GAP if bar.get("under") else STATUS_BAR_GAP)
            p.est = None
            left = PADDING
            for r, run in enumerate(line["runs"]):
                if run.get("chip"):
                    left += p.chip("status.row{}.chip".format(i), run["text"], left, top, "Name", NAME_LINE, "Hud.cpp:3277-3281; " + line["src"],
                                   sample=run.get("sample", True))
                    continue
                after = sum(p.W(rr["text"], "Name") + (2 * CHIP_PAD if rr.get("chip") else 0) for rr in line["runs"][r + 1:])
                txt = p.Fit(run["text"], "Name", words_right - left - after)
                p.text("status.row{}.words".format(i), txt, left, top, "TEXT_COLOR", "Name", "Hud.cpp:3285-3288; " + line["src"],
                       sample=run.get("sample", True), est="x follows the run before" if r > 0 else None)
                left += p.W(txt, "Name")
            if bar:
                under = bar.get("under")
                bl = PADDING if under else left + STATUS_BAR_GAP
                bwid = (right if under else words_right) - bl
                th = STATUS_UNDER_BAR if under else STATUS_BESIDE_BAR
                bt = top + NAME_LINE + (STATUS_BAR_ROW - th) / 2.0 if under else top + (NAME_LINE - th) / 2.0
                if bwid > 0:
                    e = None if under else "starts after the words' estimated width"
                    p.panel("status.row{}.bar.track".format(i), bl, bt, bwid, th, "BAR_TRACK_COLOR", "Hud.cpp:3291-3301", bar=True,
                            est=("width = column width - 24, estimated" if under else e))
                    if bar["share"] > 0:
                        p.panel("status.row{}.bar.fill".format(i), bl, bt, bwid * min(bar["share"], 1.0), th, "BAR_FILL_COLOR", "Hud.cpp:3302-3303",
                                bar=True, sample=True, est="share of the estimated track")
            top += lu

    # Hint (Hud.cpp:3311-3316)
    if c.get("hint"):
        p = frame(out, "hint.frame", ((sw - HINT_W) / 2.0, MARGIN), HINT_W, ORE_PANEL_HEIGHT, "hint")
        p.text("hint.text", p.Fit(c["hint"], "Name", HINT_W - 2 * PADDING), PADDING, (ORE_PANEL_HEIGHT - NAME_LINE) / 2.0, "TEXT_COLOR", "Name",
               "Hud.cpp:3314-3315; " + c.get("hint_src", ""), sample=c.get("hint_sample", True))

    # Where the match runs (Hud.cpp:3320-3337)
    if c.get("connection") is not None:
        conn = c["connection"]
        where = "Local skirmish" if not conn["server"] else "Server {}".format(conn["server"])
        silent = conn["silent"] >= SILENT_SECONDS
        tu = min(text_width("Name", where, 0, s), CONNECTION_MOST)
        if silent:
            tu = max(tu, text_width("Name", CONNECTION_WIDEST_SILENCE, 0, s) + 2 * CHIP_PAD)
        w = tu + 2 * PADDING
        h = 2 * TERRITORY_INSET + (2 if silent else 1) * NAME_LINE
        p = frame(out, "connection.frame", (sw - MARGIN - w, MARGIN), w, h, "where the match runs (ADR-086)",
                  est="width = text width + 24, measured by DirectWrite; estimated (right edge exact at screen width - 16)")
        p.est = None
        p.text("connection.where", p.Fit(where, "Name", tu), PADDING, TERRITORY_INSET, "TEXT_COLOR", "Name", "Hud.cpp:3323, 3331",
               sample=bool(conn["server"]), fmt=None if not conn["server"] else "Server {address} (Hud::ServerName, Hud.cpp:2221-2224)")
        if silent:
            p.chip("connection.silent", "No word from the server{}{} s".format(DOT, min(conn["silent"], 999)), PADDING - CHIP_PAD,
                   TERRITORY_INSET + NAME_LINE, "Name", NAME_LINE, "Hud.cpp:3332-3336", sample=True, fmt="No word from the server · {} s")

    # Outcome banner (Hud.cpp:3340-3350)
    if c.get("outcome"):
        h = 2 * PADDING + TITLE_LINE + NAME_LINE + BUTTON_GAP + BUTTON_H
        p = frame(out, "outcome.frame", ((sw - BANNER_W) / 2.0, under_ore), BANNER_W, h, "outcome banner")
        room = BANNER_W - 2 * PADDING
        p.text("outcome.title", p.Fit(c["outcome"][0], "Title", room), PADDING, PADDING, "GOLD_COLOR", "Title", "Hud.cpp:3345; DescribeOutcome Hud.cpp:2185-2219")
        p.text("outcome.detail", p.Fit(c["outcome"][1], "Name", room), PADDING, PADDING + TITLE_LINE, "TEXT_COLOR", "Name",
               "Hud.cpp:3346; DescribeOutcome Hud.cpp:2185-2219", sample=True)
        add_button(out, "outcome.backToMenu", (p.cx + PADDING, p.cy + PADDING + TITLE_LINE + NAME_LINE + BUTTON_GAP, BANNER_W - 2 * PADDING, BUTTON_H),
                   {"label": "Back to menu", "action": "BackToMenu"}, hovered == "BackToMenu", "Hud.cpp:3347-3349")

    # Selection and its buttons (Hud.cpp:3357-3453)
    sel = c.get("selection", [])
    sel_units = 0.0
    if sel:
        tw = max(text_width("Title" if i == 0 else "Name", l, 0, s) for i, l in enumerate(sel))
        sel_units = min(max(tw + 2 * PADDING, SEL_MIN_W), SEL_MAX_W)
    buttons = c.get("buttons", [])
    has_buttons = bool(buttons) or c.get("retreat") is not None
    btn_units = BUTTON_PANEL_W if has_buttons else 0.0
    pair_left = (sw - sel_units - btn_units) / 2.0
    if c.get("minimap", True):
        pair_left = max(pair_left, MARGIN + MINIMAP_SIZE + PANEL_GAP)
    sel_est = "selection width = longest line + 24 clamped to 280..560, measured by DirectWrite (Hud.cpp:3358-3365); estimated" if sel and SEL_MIN_W < sel_units < SEL_MAX_W else None
    pair_est = "pair centered: x depends on the selection panel's estimated width" if sel_est else None
    if sel:
        lines_units = TITLE_LINE + NAME_LINE * (len(sel) - 1)
        bars = c.get("shipBars", [])
        rows = (len(bars) + SHIP_BARS_A_ROW - 1) // SHIP_BARS_A_ROW
        if rows > 0:
            bar_units = HEALTH_BAR_GAP + rows * SHIP_BAR_H + (rows - 1) * SHIP_BAR_GAP
        elif c.get("selectionHealth") is not None:
            bar_units = HEALTH_BAR_GAP + HEALTH_BAR
        else:
            bar_units = 0.0
        h = 2 * PADDING + lines_units + bar_units
        p = frame(out, "selection.frame", (pair_left, sh - MARGIN - h), sel_units, h, "selection panel", est=sel_est)
        p.est = pair_est
        room = sel_units - 2 * PADDING
        p.text("selection.title", p.Fit(sel[0], "Title", room), PADDING, PADDING, "TEXT_COLOR", "Title", "Hud.cpp:3389; " + c.get("selection_src", ""), sample=True)
        for i in range(1, len(sel)):
            p.text("selection.line{}".format(i), p.Fit(sel[i], "Name", room), PADDING, PADDING + TITLE_LINE + NAME_LINE * (i - 1), "NUMBERS_COLOR", "Name",
                   "Hud.cpp:3390-3394; " + c.get("selection_src", ""), sample=True)
        bar_top = PADDING + lines_units + HEALTH_BAR_GAP
        for i, b in enumerate(bars):
            r, col_i = divmod(i, SHIP_BARS_A_ROW)
            l = PADDING + col_i * (SHIP_BAR_W + SHIP_BAR_GAP)
            t = bar_top + r * (SHIP_BAR_H + SHIP_BAR_GAP)
            p.panel("selection.shipBar{}".format(i), l, t, SHIP_BAR_W, SHIP_BAR_H, "BAR_TRACK_COLOR", "Hud.cpp:3399-3417 (click selects that ship alone)",
                    action="Select(ship)", sample=True)
            if b["share"] > 0:
                p.panel("selection.shipBar{}.fill".format(i), l, t, SHIP_BAR_W * min(b["share"], 1.0), SHIP_BAR_H, health_color(b["share"]),
                        "Hud.cpp:3408-3410", bar=True, sample=True)
            if b.get("retreating") or b.get("hovered"):
                p.outline("selection.shipBar{}.edge".format(i), l - LINE, t - LINE, SHIP_BAR_W + 2 * LINE, SHIP_BAR_H + 2 * LINE,
                          "TEXT_COLOR" if b.get("retreating") else "HOVER_EDGE_COLOR", "Hud.cpp:3411-3415")
        if rows == 0 and c.get("selectionHealth") is not None:
            share = min(max(c["selectionHealth"], 0), 1)
            bw = sel_units - 2 * PADDING
            p.panel("selection.health.track", PADDING, bar_top, bw, HEALTH_BAR, "BAR_TRACK_COLOR", "Hud.cpp:3418-3422", bar=True, est=sel_est)
            if share > 0:
                p.panel("selection.health.fill", PADDING, bar_top, bw * share, HEALTH_BAR, health_color(share), "Hud.cpp:3423-3424", bar=True, sample=True,
                        est=sel_est)
    if has_buttons:
        n = len(buttons)
        bh = n * BUTTON_H + (n - 1) * BUTTON_GAP if n else 0.0
        retreat_top = PADDING + bh + (BUTTON_GAP if n else 0.0)
        h = retreat_top + RETREAT_LABEL + BUTTON_H + PADDING if c.get("retreat") else 2 * PADDING + bh
        p = frame(out, "buttons.frame", (pair_left + sel_units, sh - MARGIN - h), BUTTON_PANEL_W, h, "buttons panel (ADR-088 d2)", est=pair_est)
        for i, b in enumerate(buttons):
            add_button(out, "buttons.{}".format(b.get("id", i)), (p.cx + PADDING, p.cy + PADDING + i * (BUTTON_H + BUTTON_GAP), BUTTON_PANEL_W - 2 * PADDING, BUTTON_H),
                       b, b.get("enabled", True) and hovered is not None and hovered == b.get("action"), b.get("src", "Hud.cpp:3436-3441"), est=pair_est)
        if c.get("retreat"):
            r = c["retreat"]
            p.text("buttons.retreat.label", "RETREAT", PADDING, retreat_top, "ROW_LABEL_COLOR", "Label", "Hud.cpp:3447", p.tracking())
            if r.get("mixed"):
                p.right_text("buttons.retreat.mixed", "MIXED", BUTTON_PANEL_W - PADDING, retreat_top, "NUMBERS_COLOR", "Label", "Hud.cpp:3448-3449", p.tracking())
            retreat_row(p, "buttons.retreat", PADDING, retreat_top + RETREAT_LABEL, BUTTON_PANEL_W - 2 * PADDING, BUTTON_H,
                        None if r.get("mixed") else r["setting"], "SetRetreat", "Name", 0.0, r.get("hovered"),
                        "Hud.cpp:3450-3451; EntityView::retreat (Snapshot.h:137)")

    if c.get("minimap", True):
        lay_minimap(out, c)


def window_corner(out, default, w):
    return keep_on_screen(default, w, out.width / out.scale, out.height / out.scale)


# --------------------------------------------------------------------------------------------------------------------
# Windows
# --------------------------------------------------------------------------------------------------------------------
def card_lines(slot):
    return max(1, (len(slot["cards"]) + CARDS_PER_LINE - 1) // CARDS_PER_LINE)


def card_height(slot):
    return CARD_H + CARD_NOTE_ROOM if any(cd.get("note") for cd in slot["cards"]) else CARD_H


def designer_extent(panel):  # Hud.cpp:1075-1092
    y = SLOTS_TOP
    tops = []
    for slot in panel["slots"]:
        tops.append(y)
        lines = card_lines(slot)
        y += lines * card_height(slot) + (lines - 1) * CARD_GAP + SLOT_GAP
    slots_end = y - SLOT_GAP
    sections = slots_end + SECTION_GAP
    bars = SECTION_LABEL_H + 6 * BAR_ROW_H
    damage = SECTION_LABEL_H + DAMAGE_CARD_H + 2 * PADDING
    footer = sections + max(bars, damage) + PADDING
    return {"tops": tops, "slotsEnd": slots_end, "sections": sections, "footer": footer, "height": footer + FOOTER_H + SECTION_GAP}


def lay_designer(out, panel, corner, hovered=None):
    ext = designer_extent(panel)
    open_window(out, "Designer", corner, DESIGNER_W, ext["height"] - TITLE_BAR, "designer window (Hud.cpp:1352)")
    p = Painter(out, corner)
    tr = p.tracking()
    p.header_band(DESIGNER_W, DESIGNER_HEADER)
    slots_left = DESIGNER_W - DESIGNER_INSET - ORE_BOX_W - 14.0 - QUEUE_LIMIT * (30.0 + 3.0) + 3.0  # Hud.cpp:1363-1364 -> 502
    end = p.stepped("designer.shipyard", panel["shipyard"], DESIGNER_INSET, ARROWS_IN_TITLE_TOP, 11.0, slots_left - DESIGNER_INSET - 120.0,
                    panel["hasShipyard"], "PreviousShipyard", "NextShipyard", "HEADER_LABEL_COLOR", "Label",
                    "Hud.cpp:1365-1367; target Shipyard 'SHIPYARD {:02}' or 'NO SHIPYARD' (Hud.cpp:591)", tr, sample=panel["hasShipyard"])
    p.est = "x follows the Shipyard's estimated width"
    p.text("designer.titleLabel", "· DESIGNER", end + ARROW_GAP, 11.0, "HEADER_LABEL_COLOR", "Label", "Hud.cpp:1368", tr)
    p.est = None
    p.text("designer.title", "SHIP DESIGN", DESIGNER_INSET, 40.0, "TEXT_COLOR", "Title", "Hud.cpp:1370")
    p.text("designer.queueLabel", "QUEUE{}{} / {}".format(DOT, panel["queued"], QUEUE_LIMIT), slots_left, 12.0, "HEADER_LABEL_COLOR", "Label",
           "Hud.cpp:1371, 1043-1046 (QueueLabel); EntityView::queue (Snapshot.h:115)", tr, sample=True, fmt="QUEUE · {} / 5")
    p.right_text("designer.built", "BUILT{}{}".format(DOT, panel["built"]), slots_left - 12.0, 44.0, "HEADER_LABEL_COLOR", "Label",
                 "Hud.cpp:1372-1373; EntityView::shipsBuilt (Snapshot.h:112)", tr, sample=True, fmt="BUILT · {}")
    for i in range(QUEUE_LIMIT):
        p.panel("designer.queueSlot{}".format(i), slots_left + i * 33.0, 40.0, 30.0, 24.0, "SLOT_FILLED_COLOR" if i < panel["queued"] else "FIELD_COLOR",
                "Hud.cpp:1374-1378")
    p.ore_box(panel["ore"], DESIGNER_W - DESIGNER_INSET)

    save_left = DESIGNER_W - DESIGNER_INSET - 68.0
    fw = save_left - DESIGNER_INSET - 8.0
    p.panel("designer.name.field", DESIGNER_INSET, NAME_ROW_TOP, fw, NAME_ROW_H, "FIELD_COLOR", "Hud.cpp:1385-1386 (press starts typing)", action="EditName")
    p.outline("designer.name.field.edge", DESIGNER_INSET, NAME_ROW_TOP, fw, NAME_ROW_H, "PICKED_EDGE_COLOR" if panel["editing"] else "EDGE_COLOR",
              "Hud.cpp:1387 (lit while typing)")
    p.text("designer.name.label", "NAME", DESIGNER_INSET + 12, NAME_ROW_TOP + 13, "LABEL_COLOR", "Label", "Hud.cpp:1388", tr)
    count = "{}/{}".format(len(panel["name"]), DESIGN_NAME_LIMIT)
    room = fw - 66 - 12 - p.W(count, "Figure") - FIT_GAP
    shown = panel["name"] + ("_" if panel["editing"] else "")
    p.text("designer.name.value", p.Fit(shown, "Name", room), DESIGNER_INSET + 66, NAME_ROW_TOP + 9, "TEXT_COLOR" if panel["nameValid"] else "WARNING_COLOR",
           "Name", "Hud.cpp:1391-1392; Designer::Name (Designer.cpp:122-151)", sample=True)
    p.right_text("designer.name.count", count, DESIGNER_INSET + fw - 12, NAME_ROW_TOP + 12, "LABEL_COLOR", "Figure", "Hud.cpp:1389, 1393", sample=True,
                 fmt="{}/32")
    sv = panel["save"]
    p.panel("designer.save", save_left, NAME_ROW_TOP, 68, NAME_ROW_H, "CARD_COLOR" if sv["enabled"] else "FIELD_COLOR", "Hud.cpp:1396-1399",
            action="SaveDesign" if sv["enabled"] else None, enabled=sv["enabled"], selected=sv.get("selected"))
    p.outline("designer.save.edge", save_left, NAME_ROW_TOP, 68, NAME_ROW_H, "QUEUE_EDGE_COLOR" if sv.get("selected") else "EDGE_COLOR", "Hud.cpp:1397")
    p.text("designer.save.label", sv["label"], save_left + 12, NAME_ROW_TOP + 13,
           "QUEUE_EDGE_COLOR" if sv.get("selected") else ("TEXT_COLOR" if sv["enabled"] else "DIM_TEXT_COLOR"), "Label", "Hud.cpp:1400-1404, 602", tr)

    p.text("designer.saved.label", "SAVED", DESIGNER_INSET, CHIP_ROW_TOP + 7, "LABEL_COLOR", "Label", "Hud.cpp:1408", tr)
    first = panel.get("firstChip", 0)
    for col_i, chip in enumerate(panel["chips"][first:first + CHIPS_SHOWN]):
        l = CHIPS_LEFT + col_i * (CHIP_W + 6.0)
        p.panel("designer.chip{}".format(col_i), l, CHIP_ROW_TOP, CHIP_W, CHIP_H, "PICKED_COLOR" if chip["shown"] else "CARD_COLOR", "Hud.cpp:1413 (LoadDesign)",
                action="LoadDesign", selected=chip["shown"])
        p.outline("designer.chip{}.edge".format(col_i), l, CHIP_ROW_TOP, CHIP_W, CHIP_H, "PICKED_EDGE_COLOR" if chip["shown"] else "EDGE_COLOR", "Hud.cpp:1414")
        nr = CHIP_W - 16 - p.W(chip["code"], "Detail") - FIT_GAP
        p.text("designer.chip{}.name".format(col_i), p.Fit(chip["name"], "Label", nr), l + 8, CHIP_ROW_TOP + 6, "TEXT_COLOR", "Label", "Hud.cpp:1415-1417; DesignView::nameUtf8",
               sample=True)
        p.right_text("designer.chip{}.code".format(col_i), chip["code"], l + CHIP_W - 8, CHIP_ROW_TOP + 7, "ACCENT_COLOR" if chip["shown"] else "CODE_COLOR", "Detail",
                     "Hud.cpp:1418-1419; DesignCodeOf Hud.cpp:422-435", sample=True)
    if len(panel["chips"]) > CHIPS_SHOWN:
        al = DESIGNER_W - DESIGNER_INSET - 2 * SMALL_BUTTON - 4
        p.small_button("designer.chips.previous", "<", al, CHIP_ROW_TOP + 1, first > 0, "PreviousDesigns", "Hud.cpp:1424")
        p.small_button("designer.chips.next", ">", al + SMALL_BUTTON + 4, CHIP_ROW_TOP + 1, first + CHIPS_SHOWN < len(panel["chips"]), "NextDesigns", "Hud.cpp:1425-1426")

    for si, slot in enumerate(panel["slots"]):
        top = ext["tops"][si]
        lines = card_lines(slot)
        ch = card_height(slot)
        p.text("designer.slot.{}.label".format(slot["label"].lower()), slot["label"], DESIGNER_INSET, top + 21, "LABEL_COLOR", "Label", "Hud.cpp:1438, 639/658/675/695", tr)
        p.text("designer.slot.{}.picked".format(slot["label"].lower()), p.Fit(slot["picked"], "Name", SLOT_DIVIDER_LEFT - DESIGNER_INSET - FIT_GAP), DESIGNER_INSET, top + 39,
               "ACCENT_COLOR", "Name", "Hud.cpp:1439-1440", sample=True)
        p.panel("designer.slot.{}.divider".format(slot["label"].lower()), SLOT_DIVIDER_LEFT, top + 6, LINE, lines * ch + (lines - 1) * CARD_GAP - 12, "EDGE_COLOR", "Hud.cpp:1441")
        for i, card in enumerate(slot["cards"]):
            line, col_i = divmod(i, CARDS_PER_LINE)
            l = CARDS_LEFT + col_i * (CARD_W + CARD_GAP)
            ct = top + line * (ch + CARD_GAP)
            locked = bool(card.get("lockedBy"))
            cid = "designer.card.{}.{}".format(slot["label"].lower(), i)
            hov = hovered is not None and hovered == card["name"]
            p.panel(cid, l, ct, CARD_W, ch, "PICKED_COLOR" if card["picked"] else ("LOCKED_COLOR" if locked else "CARD_COLOR"), "Hud.cpp:1450-1457",
                    action=None if locked else "Pick{}({})".format(slot["label"].capitalize(), card["name"]), selected=card["picked"], locked=locked,
                    hoveredPreview=hov)
            if locked:
                p.panel(cid + ".hatch", l, ct, CARD_W, ch, "LOCKED_HATCH_COLOR", "Hud.cpp:1455", fill="hatched")
            p.outline(cid + ".edge", l, ct, CARD_W, ch, "PICKED_EDGE_COLOR" if card["picked"] else "EDGE_COLOR", "Hud.cpp:1458")
            inner = CARD_W - 24
            cw = p.gem_figure_width(card["cost"], "Figure")
            p.text(cid + ".name", p.Fit(card["name"], "Name", inner - cw - FIT_GAP), l + 12, ct + 8, "LOCKED_TEXT_COLOR" if locked else "TEXT_COLOR", "Name",
                   "Hud.cpp:1462-1463; Tuning.json {}s[].name".format(slot["label"].lower()), sample=card["name"] != "None")
            p.gem_figure(cid + ".cost", card["cost"], l + CARD_W - 12, ct + 11, "Figure", "LOCKED_TEXT_COLOR" if locked else "GOLD_COLOR", "Hud.cpp:1464-1465")
            p.text(cid + ".numbers", p.Fit(card["numbers"], "Detail", inner), l + 12, ct + CARD_NUMBERS_TOP, "LOCKED_TEXT_COLOR" if locked else "NUMBERS_COLOR",
                   "Detail", "Hud.cpp:1467-1468; formats Hud.cpp:651-652, 669, 686-687, 700, 714", sample=card["numbers"] != "No module")
            if card.get("note"):
                p.text(cid + ".note", p.Fit(card["note"], "Detail", inner), l + 12, ct + CARD_NOTE_TOP, "LOCKED_TEXT_COLOR" if locked else "NUMBERS_COLOR", "Detail",
                       "Hud.cpp:1469-1473, 688 ('splash {}m')", sample=True)
            if locked:
                lt = ct + ch - CARD_LOCK_FROM_FOOT
                p.sprite(cid + ".lockBox", "Checkbox", l + 12, lt + 2, 10, "AMBER_COLOR", "Hud.cpp:1477")
                p.text(cid + ".lockedBy", p.Fit(card["lockedBy"], "Label", CARD_W - 28 - 12), l + 28, lt, "AMBER_COLOR", "Label",
                       "Hud.cpp:1478-1479; LockedBy Hud.cpp:576-580", sample=True, fmt="RESEARCH · {TOPIC IN CAPITALS}")
    p.panel("designer.sectionDivider", DESIGNER_INSET, ext["slotsEnd"] + SECTION_GAP / 2, DESIGNER_W - 2 * DESIGNER_INSET, LINE, "EDGE_COLOR", "Hud.cpp:1483")

    sec = ext["sections"]
    p.text("designer.performance.label", "PERFORMANCE", DESIGNER_INSET, sec, "LABEL_COLOR", "Label", "Hud.cpp:1487", tr)
    for i, b in enumerate(panel["bars"]):
        top = sec + SECTION_LABEL_H + i * BAR_ROW_H
        bid = "designer.bar.{}".format(b["label"].lower().replace(" ", ""))
        p.text(bid + ".label", b["label"], DESIGNER_INSET, top + 2, "ROW_LABEL_COLOR", "Label", "Hud.cpp:1492, 763-779")
        p.panel(bid + ".track", BAR_LEFT, top + 6, BAR_W, 7, "BAR_TRACK_COLOR", "Hud.cpp:1493", bar=True)
        prev = b.get("previewShare")
        if prev is not None and b.get("change", "Same") != "Same":
            ccol = "GOOD_COLOR" if b["change"] == "Better" else "POOR_COLOR"
            p.panel(bid + ".change", BAR_LEFT, top + 6, BAR_W * max(b["share"], prev), 7, ccol, "Hud.cpp:1497", bar=True, sample=True)
            p.panel(bid + ".fill", BAR_LEFT, top + 6, BAR_W * min(b["share"], prev), 7, "BAR_FILL_COLOR", "Hud.cpp:1498", bar=True, sample=True)
        else:
            p.panel(bid + ".fill", BAR_LEFT, top + 6, BAR_W * b["share"], 7, "BAR_FILL_COLOR", "Hud.cpp:1501", bar=True, sample=True)
        ch = b.get("change", "Same")
        p.right_text(bid + ".value", b["previewValue"] if prev is not None else b["value"], BAR_VALUE_RIGHT, top,
                     {"Better": "GOOD_COLOR", "Worse": "POOR_COLOR"}.get(ch, "TEXT_COLOR"), "Figure", "Hud.cpp:1503-1504", sample=True,
                     fmt="{value}, or while previewing '{preview} ({signed change})'")
        if b["unit"]:
            p.text(bid + ".unit", b["unit"], BAR_VALUE_RIGHT + 6, top + 2, "LABEL_COLOR", "Label", "Hud.cpp:1505")
        if i >= 6:
            out.elements[-1]["note"] = "the seventh bar row runs past ExtentOf's six rows (Hud.cpp:1087) into the footer by 13 units"

    dw = DESIGNER_W - DESIGNER_INSET - DAMAGE_LEFT
    p.text("designer.damage.label", "DAMAGE / S AFTER ARMOR{}VS A FORMATION".format(DOT), DAMAGE_LEFT, sec, "LABEL_COLOR", "Label", "Hud.cpp:1510-1511", tr)
    n = max(1, len(panel["damage"]))
    cw = (dw - (n - 1) * DAMAGE_CARD_GAP) / n
    ctop = sec + SECTION_LABEL_H
    for i, d in enumerate(panel["damage"]):
        l = DAMAGE_LEFT + i * (cw + DAMAGE_CARD_GAP)
        did = "designer.damage.{}".format(d["hull"].lower())
        p.panel(did, l, ctop, cw, DAMAGE_CARD_H, "FIELD_COLOR", "Hud.cpp:1519")
        p.outline(did + ".edge", l, ctop, cw, DAMAGE_CARD_H, "EDGE_COLOR", "Hud.cpp:1520")
        aw = p.W(d["armor"], "Label")
        p.text(did + ".hull", p.Fit(d["hull"], "Name", cw - 18 - aw - FIT_GAP), l + 10, ctop + 8, "TEXT_COLOR", "Name", "Hud.cpp:1522-1523; HullView::nameUtf8", sample=True)
        p.right_text(did + ".armor", d["armor"], l + cw - 8, ctop + 11, "LABEL_COLOR", "Label", "Hud.cpp:1524, 796", sample=True, fmt="ARM {}")
        chc = {"Better": "GOOD_COLOR", "Worse": "POOR_COLOR"}.get(d.get("change", "Same"), "TEXT_COLOR")
        p.text(did + ".perShip", d["perShip"], l + 10, ctop + 30, chc, "LargeFigure", "Hud.cpp:1526, 797", sample=True, fmt="{:.1f}")
        if d.get("reach"):
            p.right_text(did + ".reach", d["reach"], l + cw - 8, ctop + 28, "LABEL_COLOR", "Label", "Hud.cpp:1528-1529, 801", sample=True, fmt="×{} ships")
        if d.get("perShipChange"):
            p.right_text(did + ".change", d["perShipChange"], l + cw - 8, ctop + 44, chc, "Figure", "Hud.cpp:1531-1535", sample=True)
        p.panel(did + ".rating.track", l + 10, ctop + 66, cw - 20, 4, "BAR_TRACK_COLOR", "Hud.cpp:1536", bar=True)
        p.panel(did + ".rating.fill", l + 10, ctop + 66, (cw - 20) * d["share"], 4, {"Good": "GOOD_COLOR", "Fair": "FAIR_COLOR"}.get(d["rating"], "POOR_COLOR"),
                "Hud.cpp:1537, 1037-1040", bar=True, sample=True)
        p.text(did + ".perOre", p.Fit(d["perOre"], "Detail", cw - 20), l + 10, ctop + 76, "SOFT_COLOR", "Detail", "Hud.cpp:1538-1539, 802", sample=True,
               fmt="{:.1f} / 100 ore")
    p.text("designer.hint", p.Fit(panel["hint"], "Detail", dw), DAMAGE_LEFT, ctop + DAMAGE_CARD_H + 10, "LABEL_COLOR", "Detail", "Hud.cpp:1541-1542, 731-732",
           sample=panel["hint"] != "Hover any part to preview its effect.")

    ft = ext["footer"]
    rn = panel["rename"]
    p.panel("designer.rename", DESIGNER_INSET, ft, 90, FOOTER_H, "CARD_COLOR" if rn["enabled"] else "FIELD_COLOR", "Hud.cpp:1548-1551",
            action="SaveDesign" if rn["enabled"] else None, enabled=rn["enabled"])
    p.outline("designer.rename.edge", DESIGNER_INSET, ft, 90, FOOTER_H, "EDGE_COLOR", "Hud.cpp:1549")
    p.text("designer.rename.label", rn["label"], DESIGNER_INSET + 14, ft + 16, "TEXT_COLOR" if rn["enabled"] else "DIM_TEXT_COLOR", "Label", "Hud.cpp:1552-1553, 607", tr)
    rl = DESIGNER_INSET + 102.0
    p.text("designer.retreat.label", "RETREAT", rl, ft, "ROW_LABEL_COLOR", "Label", "Hud.cpp:1561", tr)
    retreat_row(p, "designer.retreat", rl, ft + 18.0, 206.0, FOOTER_H - 18.0, panel["retreat"], "DesignRetreat", "Label", tr, None,
                "Hud.cpp:1562-1563; Designer::Retreat (Designer.cpp:106-112)")
    sl_, step_w, count_w = 346.0, 36.0, 42.0

    def step(sid, mark, l, enabled, kind):
        p.panel(sid, l, ft, step_w, FOOTER_H, "CARD_COLOR" if enabled else "FIELD_COLOR", "Hud.cpp:1570-1573", action=kind if enabled else None, enabled=enabled)
        p.outline(sid + ".edge", l, ft, step_w, FOOTER_H, "EDGE_COLOR", "Hud.cpp:1571")
        p.text(sid + ".mark", mark, l + 13, ft + 13, "TEXT_COLOR" if enabled else "DIM_TEXT_COLOR", "Name", "Hud.cpp:1574")

    step("designer.fewer", "-", sl_, panel["canFewer"], "FewerShips")
    p.panel("designer.count", sl_ + step_w, ft, count_w, FOOTER_H, "FIELD_COLOR", "Hud.cpp:1577")
    p.outline("designer.count.edge", sl_ + step_w, ft, count_w, FOOTER_H, "EDGE_COLOR", "Hud.cpp:1578")
    p.text("designer.count.value", "{}{}".format(TIMES, panel["count"]), sl_ + step_w + 10, ft + 13, "TEXT_COLOR", "Name", "Hud.cpp:1579", sample=True, fmt="×{}")
    step("designer.more", "+", sl_ + step_w + count_w, panel["canMore"], "MoreShips")
    ql = 468.0
    qw = DESIGNER_W - DESIGNER_INSET - ql
    q = panel["queue"]
    p.panel("designer.queue", ql, ft, qw, FOOTER_H, "QUEUE_COLOR" if q["enabled"] else "FIELD_COLOR", "Hud.cpp:1585, 826-833",
            action=q.get("action") if q["enabled"] else None, enabled=q["enabled"])
    if q["enabled"]:
        p.panel("designer.queue.hatch", ql, ft, qw, FOOTER_H, "QUEUE_HATCH_COLOR", "Hud.cpp:1588", fill="hatched")
    p.outline("designer.queue.edge", ql, ft, qw, FOOTER_H, "QUEUE_EDGE_COLOR" if q["enabled"] else "EDGE_COLOR", "Hud.cpp:1591")
    color = "QUEUE_TEXT_COLOR" if q["enabled"] else "DIM_TEXT_COLOR"
    p.text("designer.queue.label", "QUEUE", ql + 18, ft + 2, color, "Title", "Hud.cpp:1594, 827/831", tr)
    p.text("designer.queue.detail", panel["queueDetail"], ql + 18, ft + 31, color, "Detail", "Hud.cpp:1595, 818, 824", sample=True,
           fmt="{} s each | Needs Shipyard L{}")
    p.gem_figure("designer.queue.cost", panel["queueCost"], ql + qw - 16, ft + 10, "Title", "GOLD_COLOR" if q["enabled"] else "DIM_TEXT_COLOR", "Hud.cpp:1596-1597, 817")
    return ext


def cards_bottom(lines, h):
    return CARDS_TOP + max(lines, 1) * (h + CARD_SPACING) - CARD_SPACING


def queue_rows(p, queue, w, top):  # Hud.cpp:1675-1706
    p.text("queue.label", "QUEUE{}{} / {}".format(DOT, len(queue), QUEUE_LIMIT), WINDOW_INSET, top, "LABEL_COLOR", "Label", "Hud.cpp:1677, 1043-1046", p.tracking(),
           sample=True, fmt="QUEUE · {} / 5")
    width = w - 2 * WINDOW_INSET
    for i, q in enumerate(queue):
        t = top + SECTION_BODY_GAP + i * (QUEUE_ROW_H + QUEUE_ROW_GAP)
        rid = "queue.row{}".format(i)
        p.panel(rid, WINDOW_INSET, t, width, QUEUE_ROW_H, "CARD_COLOR", "Hud.cpp:1682")
        p.outline(rid + ".edge", WINDOW_INSET, t, width, QUEUE_ROW_H, "EDGE_COLOR", "Hud.cpp:1683")
        p.text(rid + ".number", str(i + 1), WINDOW_INSET + 10, t + 7, "LABEL_COLOR", "Figure", "Hud.cpp:1684")
        right = WINDOW_INSET + width - 10
        bar_left = right - QUEUE_BAR_W - 44
        waiting = "WAITING FOR FLEET CAP" if q.get("capped") else "WAITING FOR ORE"
        name_left = WINDOW_INSET + 32
        name_right = right - p.W(waiting, "Label") if q.get("waiting") else (bar_left if q.get("front") else right)
        p.text(rid + ".name", p.Fit(q["name"], "Name", name_right - name_left - FIT_GAP), name_left, t + 5, "TEXT_COLOR", "Name", "Hud.cpp:1692-1693", sample=True)
        if q.get("waiting"):
            p.right_text(rid + ".waiting", waiting, right, t + 9, "AMBER_COLOR", "Label", "Hud.cpp:1689, 1694-1695")
        elif q.get("front"):
            p.panel(rid + ".bar.track", bar_left, t + 12, QUEUE_BAR_W, 6, "BAR_TRACK_COLOR", "Hud.cpp:1698", bar=True)
            p.panel(rid + ".bar.fill", bar_left, t + 12, QUEUE_BAR_W * q["permille"] / 1000.0, 6, "BAR_FILL_COLOR", "Hud.cpp:1699-1702; EntityView::jobPermille", bar=True,
                    sample=True)
            p.right_text(rid + ".percent", "{}%".format(q["permille"] // 10), right, t + 7, "TEXT_COLOR", "Figure", "Hud.cpp:1703", sample=True, fmt="{}%")


def header(p, subject, ore, w, steps=None, can_step=False):  # Hud.cpp:1658-1671
    p.header_band(w, DESIGNER_HEADER)
    room = w - 2 * WINDOW_INSET - ORE_BOX_W - FIT_GAP
    if steps:
        p.stepped("header.subject", subject, WINDOW_INSET, ARROWS_IN_HEADER_TOP, 40.0, room, can_step, steps[0], steps[1], "TEXT_COLOR", "Title",
                  "Hud.cpp:1665-1666")
    else:
        p.text("header.subject", p.Fit(subject, "Title", room), WINDOW_INSET, 40.0, "TEXT_COLOR", "Title", "Hud.cpp:1669", sample=True)
    p.ore_box(ore, w - WINDOW_INSET)


def production_height(panel):
    lines = (len(panel["options"]) + 1) // 2
    cb = cards_bottom(lines, OPTION_H)
    return cb + SECTION_GAP + SECTION_BODY_GAP + len(panel["queue"]) * (QUEUE_ROW_H + QUEUE_ROW_GAP) + SECTION_GAP


def lay_production(out, panel, corner):
    open_window(out, "Production", corner, PRODUCTION_W, production_height(panel) - TITLE_BAR, "production window (Hud.cpp:1722-1723)")
    p = Painter(out, corner)
    p.text("production.titleLabel", "PRODUCTION", WINDOW_INSET, 11.0, "HEADER_LABEL_COLOR", "Label", "Hud.cpp:1725", p.tracking())
    header(p, panel["producer"], panel["ore"], PRODUCTION_W, ("PreviousProducer", "NextProducer"), panel["canStep"])
    p.text("production.build.label", "BUILD", WINDOW_INSET, SECTION_TOP, "LABEL_COLOR", "Label", "Hud.cpp:1730", p.tracking())
    width = (PRODUCTION_W - 2 * WINDOW_INSET - CARD_SPACING) / 2.0
    inner = width - 2 * CARD_INSET
    for i, o in enumerate(panel["options"]):
        line, col_i = divmod(i, 2)
        l = WINDOW_INSET + col_i * (width + CARD_SPACING)
        ct = CARDS_TOP + line * (OPTION_H + CARD_SPACING)
        en = o["enabled"]
        oid = "production.option{}".format(i)
        p.panel(oid, l, ct, width, OPTION_H, "CARD_COLOR" if en else "FIELD_COLOR", "Hud.cpp:1740-1742 (Queue)", action="Queue" if en else None, enabled=en)
        p.outline(oid + ".edge", l, ct, width, OPTION_H, "EDGE_COLOR", "Hud.cpp:1743")
        p.text(oid + ".name", p.Fit(o["name"], "Name", inner), l + CARD_INSET, ct + 6, "TEXT_COLOR" if en else "LOCKED_TEXT_COLOR", "Name",
               "Hud.cpp:1745-1746; DesignView::nameUtf8 or 'Constructor' (Hud.cpp:164)", sample=o["name"] != "Constructor")
        cw = p.gem_figure_width(o["cost"], "Figure")
        if o.get("detail"):
            p.text(oid + ".detail", p.Fit(o["detail"], "Detail", inner - cw - FIT_GAP), l + CARD_INSET, ct + 28, "CODE_COLOR" if en else "LOCKED_TEXT_COLOR", "Detail",
                   "Hud.cpp:1748-1749, 2878 (code, or code · NEEDS L{})", sample=True)
        p.gem_figure(oid + ".cost", o["cost"], l + width - CARD_INSET, ct + 28, "Figure", "GOLD_COLOR" if en else "LOCKED_TEXT_COLOR", "Hud.cpp:1750-1751")
        third = ct + OPTION_THIRD
        if o.get("time"):
            p.text(oid + ".time", o["time"], l + CARD_INSET, third, "LABEL_COLOR" if en else "LOCKED_TEXT_COLOR", "Detail", "Hud.cpp:1755, 2863, 2884", sample=True,
                   fmt="{} s")
        seg_w = SEGMENTS * (SEG_W + SEG_GAP) - SEG_GAP
        gw = sum(p.W(h, "Label") + LETTER_GAP + seg_w + GROUP_GAP for h, _ in o.get("strengths", []))
        x = l + width - CARD_INSET - max(0.0, gw - GROUP_GAP)
        for h, r in o.get("strengths", []):
            save = p.est
            p.est = "x follows the hull initials' estimated widths"
            p.text(oid + ".strength." + h, h, x, third, "ROW_LABEL_COLOR" if en else "LOCKED_TEXT_COLOR", "Label", "Hud.cpp:1764, 2888 (hull initial)", sample=True)
            x += p.W(h, "Label") + LETTER_GAP
            lit = {"Poor": 1, "Fair": 2, "Good": 3}[r]
            for sgi in range(SEGMENTS):
                colr = "BAR_TRACK_COLOR" if sgi >= lit else ({"Good": "GOOD_COLOR", "Fair": "FAIR_COLOR"}.get(r, "POOR_COLOR") if en else "LOCKED_TEXT_COLOR")
                p.panel(oid + ".strength.{}.segment{}".format(h, sgi), x, third + 5, SEG_W, SEG_H, colr, "Hud.cpp:1766-1774 (rating {})".format(r), bar=True, sample=True)
                x += SEG_W + SEG_GAP
            x += GROUP_GAP - SEG_GAP
            p.est = save
    if panel.get("hint"):
        p.text("production.hint", p.Fit(panel["hint"], "Detail", PRODUCTION_W - 2 * WINDOW_INSET), WINDOW_INSET, CARDS_TOP + 16, "LABEL_COLOR", "Detail",
               "Hud.cpp:1778-1782, 2838, 2896")
    lines = (len(panel["options"]) + 1) // 2
    queue_rows(p, panel["queue"], PRODUCTION_W, cards_bottom(lines, OPTION_H) + SECTION_GAP)


def topic_width():
    return (RESEARCH_W - 2 * WINDOW_INSET - (TOPIC_COLUMNS - 1) * CARD_SPACING) / TOPIC_COLUMNS


def lay_research(out, panel, corner):
    s = out.scale
    eff = lambda t: wrap("Detail", t["effect"], topic_width() - 2 * CARD_INSET, s)
    lines = TOPIC_LEAST_LINES
    for t in panel["topics"]:
        lines = max(lines, len(eff(t)) + max(1, len(t["needs"])))
    th = TOPIC_LINES_TOP + lines * TOPIC_LINE + TOPIC_FOOT
    shown = min(TOPICS_SHOWN, len(panel["topics"]) - min(panel["first"], len(panel["topics"])))
    tl = (shown + TOPIC_COLUMNS - 1) // TOPIC_COLUMNS
    cb = cards_bottom(tl, th)
    height = cb + SECTION_GAP + SECTION_BODY_GAP + len(panel["queue"]) * (QUEUE_ROW_H + QUEUE_ROW_GAP) + SECTION_GAP
    open_window(out, "Research", corner, RESEARCH_W, height - TITLE_BAR, "research window (Hud.cpp:1822-1823)")
    p = Painter(out, corner)
    p.text("research.titleLabel", "RESEARCH", WINDOW_INSET, 11.0, "HEADER_LABEL_COLOR", "Label", "Hud.cpp:1825", p.tracking())
    header(p, panel["lab"], panel["ore"], RESEARCH_W)
    n = len(panel["topics"])
    page_end = min(panel["first"] + TOPICS_SHOWN, n)
    p.text("research.topics.label", "TOPICS" if n == 0 else "TOPICS{}{}-{} OF {}".format(DOT, panel["first"] + 1, page_end, n), WINDOW_INSET, SECTION_TOP,
           "LABEL_COLOR", "Label", "Hud.cpp:1830-1832", p.tracking(), sample=n > 0, fmt="TOPICS · {}-{} OF {}")
    if n > TOPICS_SHOWN:
        al = RESEARCH_W - WINDOW_INSET - 2 * SMALL_BUTTON - 4
        p.small_button("research.topics.previous", "<", al, SECTION_TOP - 6, panel["first"] > 0, "PreviousTopics", "Hud.cpp:1837")
        p.small_button("research.topics.next", ">", al + SMALL_BUTTON + 4, SECTION_TOP - 6, panel["first"] + TOPICS_SHOWN < n, "NextTopics", "Hud.cpp:1838-1839")
    w = topic_width()
    for k, t in enumerate(panel["topics"][panel["first"]:panel["first"] + TOPICS_SHOWN]):
        line, col_i = divmod(k, TOPIC_COLUMNS)
        l = WINDOW_INSET + col_i * (w + CARD_SPACING)
        ct = CARDS_TOP + line * (th + CARD_SPACING)
        blocked = bool(t["needs"])
        tid = "research.topic{}".format(k)
        p.panel(tid, l, ct, w, th, "LOCKED_COLOR" if blocked else ("CARD_COLOR" if t["enabled"] else "FIELD_COLOR"), "Hud.cpp:1850-1857 (Research)",
                action="Research" if t["enabled"] else None, enabled=t["enabled"], locked=blocked,
                note="card height = TOPIC_LINES_TOP + lines x 18 + 4, lines from the wrapped effects (estimated wrap)")
        if blocked:
            p.panel(tid + ".hatch", l, ct, w, th, "LOCKED_HATCH_COLOR", "Hud.cpp:1855", fill="hatched")
        p.outline(tid + ".edge", l, ct, w, th, "EDGE_COLOR", "Hud.cpp:1858")
        color = "TEXT_COLOR" if t["enabled"] else "LOCKED_TEXT_COLOR"
        cw = p.gem_figure_width(t["cost"], "Figure")
        p.text(tid + ".name", p.Fit(t["name"], "Name", w - 2 * CARD_INSET - cw - FIT_GAP), l + CARD_INSET, ct + 6, color, "Name", "Hud.cpp:1861-1862; Tuning.json research[].name",
               sample=True)
        p.gem_figure(tid + ".cost", t["cost"], l + w - CARD_INSET, ct + 9, "Figure", "GOLD_COLOR" if t["enabled"] else "LOCKED_TEXT_COLOR", "Hud.cpp:1863-1864")
        lt = ct + TOPIC_LINES_TOP
        for e in eff(t):
            p.text(tid + ".effect", e, l + CARD_INSET, lt, "NUMBERS_COLOR" if t["enabled"] else "LOCKED_TEXT_COLOR", "Detail",
                   "Hud.cpp:1866-1870; EffectText GameLogic/Research.cpp:142-181", sample=True)
            lt += TOPIC_LINE
        for ni, need in enumerate(t["needs"]):
            p.sprite(tid + ".needBox", "Checkbox", l + CARD_INSET, lt + 2, 10, "AMBER_COLOR", "Hud.cpp:1874")
            txt = "NEEDS{}{}".format(DOT, need) if ni == 0 else "+ {}".format(need)
            p.text(tid + ".needs", p.Fit(txt, "Label", w - 28 - CARD_INSET), l + 28, lt, "AMBER_COLOR", "Label", "Hud.cpp:1875-1877, 2963-2972", sample=True,
                   fmt="NEEDS · {} / + {}")
            lt += TOPIC_LINE
        if not blocked:
            p.text(tid + ".time", t["time"], l + CARD_INSET, lt, "LABEL_COLOR" if t["enabled"] else "LOCKED_TEXT_COLOR", "Detail", "Hud.cpp:1880-1881, 2978-2979",
                   sample=True, fmt="TIER {} · {} s[ · SALVAGED]")
    queue_rows(p, panel["queue"], RESEARCH_W, cb + SECTION_GAP)


def key_bindings():
    """KeyBindings() (KeyBindings.h:275-313), as the Controls window lists them."""
    return [("SELECTING AND ORDERS", ""), ("Click", "Select a ship, or a structure of yours"), ("Drag", "Select your ships in the box"),
            ("Shift+click", "Add to the selection"), ("Double-click", "Select every ship of that design in view"),
            ("Right-click", "Move, attack, or build and repair with Constructors"), ("A, then click", "Attack-move"),
            ("H, then click", "Hold the sector clicked in"), ("T, then click", "Patrol to the point clicked"), ("S", "Stop"),
            ("Ctrl+0-9", "Make a control group"), ("0-9", "Select a group; twice, look at it"), ("Shift+click", "Place a structure and keep placing"),
            ("CAMERA", ""), ("Up Down Left Right", "Pan"), ("Screen edge", "Pan, while the pointer is held in the window"), ("Middle-drag", "Pan"),
            ("Wheel", "Zoom"), ("Q and E", "Turn"), ("Minimap click", "Look there"), ("Minimap right-click", "Send the selection there"),
            ("Space", "Look at the newest alert"), ("WINDOWS", ""), ("D", "Ship designer"), ("P", "Production"), ("R", "Research"), ("O", "Scheduled orders"),
            ("F1", "These controls"), ("Esc", "Close the front window, or cancel an order"), ("Alt, held", "Every health bar"),
            ("Ctrl+= and Ctrl+-", "The interface larger and smaller")]


KB_LINES = {0: 93, 1: 94, 2: 95, 3: 96, 4: 97, 5: 98, 6: 99, 7: 100, 8: 101, 9: 102, 10: 103, 11: 104, 12: 105, 13: 106, 14: 107, 15: 109, 16: 110, 17: 111, 18: 112, 19: 113, 20: 114, 21: 115, 22: 116, 23: 117, 24: 118, 25: 119, 26: 120, 27: 121, 28: 122, 29: 123, 30: 124}


def controls_height():  # Hud.cpp:1892-1898
    h = TITLE_BAR + PADDING
    for i, (k, d) in enumerate(key_bindings()):
        h += NAME_LINE + (CONTROLS_HEADING_GAP if not d and i > 0 else 0)
    return h + PADDING


def lay_controls(out, corner):
    open_window(out, "Controls", corner, CONTROLS_W, controls_height() - TITLE_BAR, "Controls window (Hud.cpp:1903-1904)")
    p = Painter(out, corner)
    p.text("controls.titleLabel", "CONTROLS", WINDOW_INSET, 11.0, "HEADER_LABEL_COLOR", "Label", "Hud.cpp:1906", p.tracking())
    does_left = WINDOW_INSET + CONTROLS_KEYS_W
    top = TITLE_BAR + PADDING
    for i, (k, d) in enumerate(key_bindings()):
        src = "KeyBindings.h:{}".format(KB_LINES[i])
        if not d:
            top += CONTROLS_HEADING_GAP if i > 0 else 0
            p.text("controls.heading", k, WINDOW_INSET, top + 3, "LABEL_COLOR", "Label", "Hud.cpp:1914-1915; " + src, p.tracking())
        else:
            p.text("controls.keys", p.Fit(k, "Name", CONTROLS_KEYS_W - FIT_GAP), WINDOW_INSET, top, "ACCENT_COLOR", "Name", "Hud.cpp:1919-1920; " + src)
            p.text("controls.does", p.Fit(d, "Name", CONTROLS_W - WINDOW_INSET - does_left), does_left, top, "TEXT_COLOR", "Name", "Hud.cpp:1921-1922; " + src)
        top += NAME_LINE


def lay_orders(out, panel, corner, hovered=None):
    open_window(out, "Orders", corner, ORDERS_W, ORDERS_H - TITLE_BAR, "orders window (Hud.cpp:1949-1950)")
    p = Painter(out, corner)
    tr = p.tracking()
    p.text("orders.titleLabel", "SCHEDULED ORDERS", WINDOW_INSET, 11.0, "HEADER_LABEL_COLOR", "Label", "Hud.cpp:1952", tr)
    room = ORDERS_W - 2 * WINDOW_INSET
    top = TITLE_BAR + PADDING
    p.text("orders.waiting.label", "WAITING", WINDOW_INSET, top + 3, "LABEL_COLOR", "Label", "Hud.cpp:1955", tr)
    top += NAME_LINE
    if not panel["scheduled"]:
        p.text("orders.none", "None", WINDOW_INSET, top, "DIM_TEXT_COLOR", "Name", "Hud.cpp:1957-1958")
    for line in panel["scheduled"]:
        p.text("orders.scheduled", p.Fit(line, "Name", room), WINDOW_INSET, top, "TEXT_COLOR", "Name", "Hud.cpp:1959-1963, 2231-2241; DescribeScheduledOrder OrderForm.cpp:291-315",
               sample=True, fmt="{When}, {action}[, unless over {} enemy CP] · {n} ship(s)")
        top += NAME_LINE
    if panel.get("more"):
        p.text("orders.more", "and {} more".format(panel["more"]), WINDOW_INSET, top, "DIM_TEXT_COLOR", "Name", "Hud.cpp:1964-1965", sample=True, fmt="and {} more")
    p.text("orders.form.label", "NEW ORDER FOR THE SELECTION", WINDOW_INSET, ORDERS_FORM_TOP + 3, "LABEL_COLOR", "Label", "Hud.cpp:1967", tr)
    for r, (label, value) in enumerate(panel["rows"][:ORDER_ROWS]):
        rt = ORDERS_ROWS_TOP + r * ORDER_ROW
        p.text("orders.row{}.label".format(r), label, WINDOW_INSET, rt + ORDER_LABEL_TOP, "ROW_LABEL_COLOR", "Label", "Hud.cpp:1972, 2248-2274", tr)
        p.stepped("orders.row{}".format(r), value, ORDER_VALUE_LEFT, rt + ORDER_ARROW_TOP, rt + ORDER_TEXT_TOP, ORDERS_W - WINDOW_INSET - ORDER_VALUE_LEFT, True,
                  "StepOrder(-1)", "StepOrder(+1)", "TEXT_COLOR", "Name", "Hud.cpp:1973-1976; OrderForm::NameOf OrderForm.cpp:233-266", sample=True)
    p.text("orders.fires", p.Fit(panel["fires"], "Name", room), WINDOW_INSET, ORDERS_FIRES_TOP, "NUMBERS_COLOR", "Name", "Hud.cpp:1978, 2276-2283", sample=True,
           fmt="Fires at {hh:mm} on your clock, in {} | Fires once, the first time it happens")
    add_button(out, "orders.give", (corner[0] + WINDOW_INSET, corner[1] + ORDERS_GIVE_TOP, room, BUTTON_H), panel["give"], hovered == "GiveOrder",
               "Hud.cpp:1979, 2284-2289")


def away_height(lines):
    return TITLE_BAR + PADDING + TITLE_LINE + lines * NAME_LINE + PADDING


def lay_away(out, panel, corner):
    lines = min(len(panel["lines"]), AWAY_LINES)
    open_window(out, "Away", corner, AWAY_W, away_height(lines) - TITLE_BAR, "away window (Hud.cpp:1994-1995)")
    p = Painter(out, corner)
    p.text("away.titleLabel", "WHILE YOU WERE AWAY", WINDOW_INSET, 11.0, "HEADER_LABEL_COLOR", "Label", "Hud.cpp:1997", p.tracking())
    room = AWAY_W - 2 * WINDOW_INSET
    top = TITLE_BAR + PADDING
    p.text("away.since", p.Fit(panel["since"], "Title", room), WINDOW_INSET, top, "TEXT_COLOR", "Title", "Hud.cpp:2000, 2297", sample=True, fmt="Away for {}")
    top += TITLE_LINE
    for line in panel["lines"][:lines]:
        p.text("away.line", p.Fit(line, "Name", room), WINDOW_INSET, top, "NUMBERS_COLOR", "Name", "Hud.cpp:2002-2006, 2312-2319", sample=True)
        top += NAME_LINE


# --------------------------------------------------------------------------------------------------------------------
# The menu: Hud::LayMenu (Hud.cpp:2323-2361)
# --------------------------------------------------------------------------------------------------------------------
def lay_menu(out, join_world=False, notice=""):
    s = out.scale
    buttons = [("Easy skirmish", "StartSkirmish(Easy)", 2329), ("Normal skirmish", "StartSkirmish(Normal)", 2330), ("Hard skirmish", "StartSkirmish(Hard)", 2331)]
    if join_world:
        buttons.append(("Join world", "JoinWorld", 2333))
    buttons.append(("Quit", "Quit", 2334))
    lines = wrap("Name", notice, MENU_W - 2 * PADDING, s) if notice else []
    count = len(buttons)
    h = 2 * PADDING + TITLE_LINE + NAME_LINE + len(lines) * NAME_LINE + BUTTON_GAP + count * BUTTON_H + (count - 1) * BUTTON_GAP
    corner = (out.width / s / 2.0 - MENU_W / 2.0, out.height / s / 2.0 - h / 2.0)
    p = frame(out, "menu.frame", corner, MENU_W, h, "main menu (Hud.cpp:2344)")
    p.text("menu.title", "Outpost Commander", PADDING, PADDING, "GOLD_COLOR", "Title", "Hud.cpp:2346")
    p.text("menu.subtitle", "A skirmish against the AI", PADDING, PADDING + TITLE_LINE, "DIM_TEXT_COLOR", "Name", "Hud.cpp:2347")
    top = PADDING + TITLE_LINE + NAME_LINE
    for ln in lines:
        p.text("menu.notice", ln, PADDING, top, "WARNING_COLOR", "Name", "Hud.cpp:2336-2337, 2349-2353; notice from WinMain.cpp:363 or 460", sample=True,
               est="wrapped to 416 units by TextMetrics::Wrap; line breaks estimated")
        top += NAME_LINE
    top += BUTTON_GAP
    for label, action, line in buttons:
        add_button(out, "menu." + label.lower().replace(" ", ""), (corner[0] + PADDING, corner[1] + top, MENU_W - 2 * PADDING, BUTTON_H),
                   {"label": label, "action": action}, False, "Hud.cpp:{}, 2355-2359".format(line),
                   est="y depends on the notice's estimated line count" if lines else None)
        top += BUTTON_H + BUTTON_GAP
    return corner, h


# --------------------------------------------------------------------------------------------------------------------
# Sample content
# --------------------------------------------------------------------------------------------------------------------
P1_START = tuple(MAP["starts"][0].values())
FOCUS = (P1_START[0] + 75.0, P1_START[1] + 75.0)  # the player's Constructors, ahead of its Command Station (Simulation.cpp:731-748)


def map_marks(extra=()):
    marks = []
    for f_ in MAP["asteroidFields"]:
        marks.append({"kind": "AsteroidField", "pos": (f_["xMeters"], f_["zMeters"]), "radius": f_["radiusMeters"], "side": "Neutral", "sample": False})
    for a in MAP["oreAsteroids"]:
        marks.append({"kind": "Asteroid", "pos": (a["xMeters"], a["zMeters"]), "radius": a["radiusMeters"], "side": "Neutral", "sample": False})
    marks.append({"kind": "Structure", "pos": P1_START, "radius": 45, "side": "Own", "sample": False})
    marks.append({"kind": "Ship", "pos": (FOCUS[0] - 15, FOCUS[1] + 15), "radius": 10, "side": "Own"})
    marks.append({"kind": "Ship", "pos": (FOCUS[0] + 15, FOCUS[1] - 15), "radius": 10, "side": "Own"})
    marks.extend(extra)
    return marks


def start_sectors():
    return {"A1": {"side": "Own"}, "E5": {"side": "Enemy"}, "D2": {"side": "Pirate"}, "B4": {"side": "Pirate"}}


def drain_text(own_nodes, enemy_nodes, own_t, enemy_t):  # Hud.cpp:2506-2526
    behind = abs(own_nodes - enemy_nodes)
    if behind == 0:
        return "No drain", False
    own = own_nodes < enemy_nodes
    terr = TUNING["territory"]
    per_drain = terr["drainTicketsPerNodeDifference"] * behind
    left = max(0, own_t if own else enemy_t)
    drains = (left + per_drain - 1) // per_drain
    seconds = math.ceil(drains * terr["drainIntervalSeconds"])
    per_minute = round(per_drain * 60.0 / terr["drainIntervalSeconds"])
    return "{} -{} a minute{}out in {}:{:02}".format("You" if own else "Enemy", thousands(per_minute), DOT, seconds // 60, seconds % 60), own


def match_start_content():
    return {"ore": TUNING["rules"]["startingOre"], "income": 0, "connection": {"server": "", "silent": 0},
            "territory": {"own_nodes": 1, "enemy_nodes": 1, "nodes": len(MAP["sectors"]), "cap": TUNING["structures"][0]["nodes"],
                          "own_tickets": TUNING["territory"]["tickets"], "enemy_tickets": TUNING["territory"]["tickets"], "drain": "No drain"},
            "sectors": start_sectors(), "marks": map_marks(), "view": camera_view_corners(FOCUS)}


def build_buttons(ore, has_lab=False, capped=False):  # Hud.cpp:2805-2828
    out = []
    for st in TUNING["structures"]:
        if "cost" not in st:
            continue
        second_lab = st["kind"] == "ResearchLab" and has_lab
        relay_capped = st["kind"] == "Relay" and capped
        note = "ONE PER PLAYER" if second_lab else ("NODE CAP" if relay_capped else "")
        out.append({"id": "build." + st["kind"], "label": "{}|{}".format(st["name"], st["cost"]), "action": "Build({})".format(st["kind"]),
                    "enabled": not second_lab and not relay_capped and ore >= st["cost"], "note": note,
                    "src": "Hud.cpp:2821-2826; StructureTypeView name/cost (Snapshot.h:276-280); Tuning.json structures[].name/cost"})
    return out


def order_buttons(armed=None, sectors=True):  # Hud.cpp:2794-2803
    out = [{"id": "order.AttackMove", "label": "Attack-move", "action": "AttackMove", "key": "A", "selected": armed == "AttackMove", "src": "Hud.cpp:2798; KeyBindings.h:10"}]
    if sectors:
        out.append({"id": "order.HoldSector", "label": "Hold sector", "action": "HoldSector", "key": "H", "selected": armed == "HoldSector", "src": "Hud.cpp:2800; KeyBindings.h:12"})
    out.append({"id": "order.Patrol", "label": "Patrol", "action": "Patrol", "key": "T", "selected": armed == "Patrol", "src": "Hud.cpp:2801; KeyBindings.h:13"})
    out.append({"id": "order.Stop", "label": "Stop", "action": "Stop", "key": "S", "src": "Hud.cpp:2802; KeyBindings.h:11"})
    return out


def designer_panel(picks=(1, 1, 1, None), preview=None, shipyard="SHIPYARD 01", has_shipyard=True, shipyard_level=1, queued=0, built=4, ore=1240,
                   editing=False, typed=None, available=None, retreat="Quarter", designs=None):
    """DescribeDesigner (Hud.cpp:585-835) over sample content."""
    available = available or {"hull": {1, 2}, "drive": {1}, "weapon": {1, 2}}
    designs = designs if designs is not None else [{"name": d["name"], "h": d["hull"], "d": d["drive"], "w": d["weapon"], "m": None} for d in STARTING]
    h, d, w, m = picks
    match = next((x for x in designs if (x["h"], x["d"], x["w"], x["m"]) == (h, d, w, m)), None)
    hull, drive, weapon = find(HULLS, h), find(DRIVES, d), find(WEAPONS, w)
    name = typed if typed is not None else (match["name"] if match else "{}+{}+{}".format(hull["name"], drive["name"], weapon["name"]) +
                                            ("+" + find(MODULES, m)["name"] if m else ""))
    valid = 0 < len(name) <= DESIGN_NAME_LIMIT
    saves_new = match is None and valid
    save_selected = match is not None and typed is None
    panel = {"shipyard": shipyard if has_shipyard else "NO SHIPYARD", "hasShipyard": has_shipyard, "queued": queued, "built": built if has_shipyard else 0,
             "ore": ore, "name": name, "editing": editing, "nameValid": valid,
             "save": {"label": "SAVED" if save_selected else "SAVE", "enabled": saves_new, "selected": save_selected},
             "rename": {"label": "RENAME", "enabled": match is not None and typed is not None and typed != match["name"]},
             "retreat": retreat, "firstChip": 0,
             "chips": [{"name": x["name"], "code": design_code(x["h"], x["d"], x["w"], x["m"]), "shown": match is x} for x in designs]}
    pv = list(picks)
    previewing = None
    if preview:
        slot, pid = preview
        idx = {"hull": 0, "drive": 1, "weapon": 2, "module": 3}[slot]
        if pv[idx] != pid:
            pv[idx] = pid
            previewing = {"hull": find(HULLS, pid), "drive": find(DRIVES, pid), "weapon": find(WEAPONS, pid)}.get(slot, {"name": "no module"})["name"] if pid else "no module"
    slots = []
    rows = [("HULL", HULLS, "hull", lambda x: "{} HP{}ARM {}{}{}m/s".format(thousands(x["hitPoints"]), DOT, tenths(x["armor"]), DOT, tenths(x["speedMetersPerSecond"])),
             "Hud.cpp:651-652", "unlockHull"),
            ("DRIVE", DRIVES, "drive", lambda x: "SPD {}{}{}HP {}{}".format(TIMES, tenths(x["speedFactor"]), DOT, TIMES, tenths(x["hitPointsFactor"])), "Hud.cpp:669",
             "unlockDrive"),
            ("WEAPON", WEAPONS, "weapon", lambda x: "{} dmg / {}s{}{}m".format(tenths(x["damage"]), tenths(x["fireIntervalSeconds"]), DOT, tenths(x["rangeMeters"])),
             "Hud.cpp:686-687", "unlockWeapon")]
    for label, lst, key, numbers, _, unlock in rows:
        cards = []
        picked_name = ""
        for x in lst:
            if x["id"] == picks[["hull", "drive", "weapon"].index(key)]:
                picked_name = x["name"]
            locked = x["id"] not in available[key]
            cards.append({"name": x["name"], "cost": x["cost"], "numbers": numbers(x),
                          "note": "splash {}m".format(tenths(x["splashRadiusMeters"])) if key == "weapon" and x["splashRadiusMeters"] > 0 else "",
                          "picked": x["id"] == picks[["hull", "drive", "weapon"].index(key)],
                          "lockedBy": "RESEARCH{}{}".format(DOT, UNLOCKED_BY[(unlock, x["id"])].upper()) if locked else ""})
        slots.append({"label": label, "picked": picked_name, "cards": cards})
    mods = [{"name": "None", "cost": 0, "numbers": "No module", "picked": m is None, "lockedBy": ""}]
    for x in MODULES:
        mods.append({"name": x["name"], "cost": x["cost"], "numbers": "SIGHT {}m{}SPD {}{}".format(tenths(x["sightMeters"]), DOT, TIMES, tenths(x["speedFactor"])),
                     "picked": m == x["id"], "lockedBy": ""})
    slots.append({"label": "MODULE", "picked": find(MODULES, m)["name"] if m else "None", "cards": mods})
    panel["slots"] = slots
    st = stats(h, d, w, m)
    ps = stats(*pv) if previewing else None
    panel["hint"] = "Preview: with {} instead".format(previewing) if previewing else "Hover any part to preview its effect."

    def bar(label, unit, best, val, text, shown, whole, lower=False):
        b = {"label": label, "unit": unit, "value": text(st), "share": min(max(val(st) / best, 0), 1) if best > 0 else 0}
        if ps:
            b["previewShare"] = min(max(val(ps) / best, 0), 1) if best > 0 else 0
            a, z = val(st), val(ps)
            b["change"] = "Same" if a == z else ("Better" if (z > a) != lower else "Worse")
            if b["change"] == "Same":
                b["previewValue"] = text(ps)
            else:
                stp = 1.0 if whole else 10.0
                ch = (round(shown(ps) * stp) - round(shown(st) * stp)) / stp
                sign = "-" if ch < 0 or (ch == 0 and shown(ps) < shown(st)) else "+"
                b["previewValue"] = "{} ({}{})".format(text(ps), sign, thousands(int(round(abs(ch)))) if whole else tenths(abs(ch)))
        return b

    hp_whole = lambda x: (x["hp"] + 99) // 100
    panel["bars"] = [bar("Hit points", "", BEST["hp"], lambda x: x["hp"], lambda x: thousands(hp_whole(x)), hp_whole, True),
                     bar("Armor", "", BEST["armor"], lambda x: x["armor"], lambda x: tenths(x["armor"] / 100), lambda x: x["armor"] / 100, False),
                     bar("Speed", "m/s", BEST["speed"], lambda x: x["speed"], lambda x: tenths(x["speed"]), lambda x: x["speed"], False),
                     bar("Range", "m", BEST["range"], lambda x: x["range"], lambda x: tenths(x["range"]), lambda x: x["range"], False),
                     bar("Sensors", "m", BEST["sensor"], lambda x: x["sight"], lambda x: tenths(x["sight"]) if x["sight"] > 0 else "-", lambda x: x["sight"], False),
                     bar("Cost", "ore", BEST["cost"], lambda x: x["cost"], lambda x: str(x["cost"]), lambda x: x["cost"], True, True),
                     bar("Build", "s", BEST["build"], lambda x: x["build"], lambda x: tenths(x["build"]), lambda x: x["build"], False, True)]
    shown = ps or st
    dmg = []
    for i, hh in enumerate(HULLS):
        dps = formation_dps(shown, hh)
        cur = formation_dps(st, hh)
        share = min(max(dps / BEST["damage"][i], 0), 1)
        change = "Same" if not ps else ("Same" if dps == cur else ("Better" if dps > cur else "Worse"))
        t10 = (round(dps * 10) - round(cur * 10)) / 10.0
        reached = ships_reached(shown["splash"], hh["footprintRadiusMeters"])
        dmg.append({"hull": hh["name"], "armor": "ARM {}".format(tenths(hh["armor"])), "perShip": "{:.1f}".format(dps),
                    "perShipChange": "" if change == "Same" else "{}{:.1f}".format("-" if dps < cur else "+", abs(t10)),
                    "reach": "{}{} ships".format(TIMES, reached) if reached > 1 else "", "perOre": "{:.1f} / 100 ore".format(dps * 100.0 / shown["cost"]),
                    "share": share, "rating": rating(share), "change": change})
    panel["damage"] = dmg
    free = QUEUE_LIMIT - min(queued, QUEUE_LIMIT) if has_shipyard else 0
    panel["count"] = 1
    panel["canFewer"] = False
    panel["canMore"] = panel["count"] < free
    cost = match and stats(match["h"], match["d"], match["w"], match["m"])["cost"] or st["cost"]
    panel["queueCost"] = cost * panel["count"]
    leveled = not has_shipyard or shipyard_level >= HULL_LEVEL[h]
    panel["queueDetail"] = "{} s each".format(tenths(st["build"])) if leveled else "Needs Shipyard L{}".format(HULL_LEVEL[h])
    panel["queue"] = {"label": "QUEUE", "action": "Queue" if match else "SaveAndQueue",
                      "enabled": leveled and free > 0 and ore >= cost and (match is not None or saves_new)}
    return panel


def production_panel_shipyard(level=1, ore=1240):
    opts = []
    for dsg in STARTING:
        st = stats(dsg["hull"], dsg["drive"], dsg["weapon"])
        lv = HULL_LEVEL[dsg["hull"]]
        leveled = level >= lv
        code = design_code(dsg["hull"], dsg["drive"], dsg["weapon"])
        strengths = []
        for i, hh in enumerate(HULLS):
            strengths.append((abbreviation(hh["name"]), rating(min(max(formation_dps(st, hh) / BEST["damage"][i], 0), 1))))
        opts.append({"name": dsg["name"], "detail": code if leveled else "{}{}NEEDS L{}".format(code, DOT, lv), "cost": st["cost"],
                     "enabled": leveled and ore >= st["cost"], "time": "{} s".format(tenths(st["build"])), "strengths": strengths})
    return {"producer": "SHIPYARD 02", "canStep": True, "ore": ore, "options": opts,
            "queue": [{"name": "Swarm", "front": True, "permille": 620}, {"name": "Swarm"}, {"name": "Picket"}]}


def research_panel(ore=1240, has_lab=True, built=True, queued=(2, 1), researched=(), first=0):
    """DescribeResearch (Hud.cpp:2900-2989) over Tuning.json's topics, nothing researched yet but what is passed."""
    topics = []
    lab_levels = next(s for s in TUNING["structures"] if s["kind"] == "ResearchLab")["levels"]
    level_for = {lv["opensTier"]: i + 2 for i, lv in enumerate(lab_levels) if "opensTier" in lv}
    known = set(researched) | set(queued)
    name = {r["id"]: r["name"] for r in TUNING["research"]}
    for r in TUNING["research"]:
        if r["id"] in known:
            continue
        needs = [name[q].upper() for q in r["requires"] if q not in known]
        if r["tier"] > 1:
            needs.insert(0, "RESEARCH LAB L{}".format(level_for.get(r["tier"], 1)))
        enabled = has_lab and built and len(queued) < QUEUE_LIMIT and not needs and ore >= r["cost"]
        topics.append({"name": r["name"], "effect": effect_text(r), "cost": r["cost"], "needs": needs, "enabled": enabled, "tier": r["tier"],
                       "time": "TIER {}{}{} s".format(r["tier"], DOT, tenths(r["researchSeconds"]))})
    topics.sort(key=lambda t: t["tier"])
    queue = []
    for i, q in enumerate(queued):
        queue.append({"name": name[q], "front": i == 0, "permille": 450 if i == 0 else 0})
    return {"lab": ("RESEARCH LAB" if built else "RESEARCH LAB{}40% BUILT".format(DOT)) if has_lab else "NO RESEARCH LAB", "ore": ore,
            "topics": topics, "first": first, "queue": queue if has_lab else []}


def effect_text(r):  # GameLogic/Research.cpp:142-181
    e = r["effect"]
    wname = {x["id"]: x["name"] for x in WEAPONS}
    if "upgrade" in e:
        u = e["upgrade"]
        return {"miningRig": "Mining Rig income +{}%", "allHulls": "Hull hit points +{}%", "shipyards": "Shipyard build speed +{}%",
                "allStructures": "Structure hit points +{}%", "allShips": "Ship speed +{}%", "constructors": "Constructor build and repair rate +{}%",
                "asteroids": "Asteroid ore reserves +{}%"}.get(u, "{} fire rate +{}%").format(
            *(([wname[e["weapon"]]] if u == "weapon" else (["Defence gun"] if u == "structureWeapon" else [])) + [e["percent"]]))
    if "unlockHull" in e:
        return "Unlocks the {} hull".format(find(HULLS, e["unlockHull"])["name"])
    if "unlockDrive" in e:
        return "Unlocks the {} drive".format(find(DRIVES, e["unlockDrive"])["name"])
    return "Unlocks the {}".format(find(WEAPONS, e["unlockWeapon"])["name"])


# --------------------------------------------------------------------------------------------------------------------
# States
# --------------------------------------------------------------------------------------------------------------------
STATES = []


def state(screen, name, title, scale=1.0, notes=None):
    def deco(fn):
        STATES.append((screen, name, title, scale, notes or [], fn))
        return fn
    return deco


@state("menu", "default", "Main menu, no join file", notes=["Drawn over the empty grid and the sky (GameClient.cpp:1156-1158); no HUD, no tag (ADR-086)."])
def s_menu_default(out):
    lay_menu(out)


@state("menu", "join-world", "Main menu with a join file in Documents\\Outpost Commander\\Join.json (ADR-078 d9)")
def s_menu_join(out):
    lay_menu(out, join_world=True)


@state("menu", "connection-lost", "Main menu after a lost connection, saying why (ADR-078 d10)",
       notes=["Notice text 'The connection to the server was lost: {reason}' from WinMain.cpp:460; sample reason from GameProtocol/WireFormat.cpp:81."])
def s_menu_lost(out):
    lay_menu(out, join_world=True, notice="The connection to the server was lost: The server ended the game.")


@state("menu", "join-failed", "Main menu after a join that failed (WinMain.cpp:359-364)",
       notes=["Notice 'Could not join the world: {error}' (WinMain.cpp:363); sample error from GameProtocol/QuicTransport.cpp:34 (WELCOME_TIMEOUT 10,000 ms, QuicTransport.cpp:9)."])
def s_menu_join_failed(out):
    lay_menu(out, join_world=True, notice="Could not join the world: The server did not seat player 1 within 10000 ms.")


@state("menu", "scale-125", "Main menu at interface factor 125%, the largest the menu reaches on 16:9 (ADR-070)", scale=1.25)
def s_menu_125(out):
    lay_menu(out)


@state("match", "awaiting-first-snapshot", "A match started, before its first snapshot",
       notes=["GameClient::StartMatch clears the layout (GameClient.cpp:472-478, 517); Update lays the HUD out only once the view holds a snapshot "
              "(GameClient.cpp:689-775); DrawHud draws nothing while fontPixels is 0 (GameClient.cpp:1291-1293). The screen shows the sky and the grid only "
              "(GameClient.cpp:1138-1158)."])
def s_awaiting(out):
    pass


@state("hud", "match-start", "First snapshot of a Normal skirmish: nothing selected",
       notes=["Starting Ore and tickets from Tuning.json rules.startingOre / territory.tickets; no Mining Rig yet, so the income is a '+0/s' chip.",
              "No Lab and no finished Shipyard, so the status panel is not drawn, but its 94 units are kept (Hud.cpp:149, 3160-3161).",
              "Pirate-guarded sectors D2 and B4 are sample (seeded, ADR-072/073)."])
def s_hud_start(out):
    lay_hud(out, match_start_content())


def busy_content(hovered_stop=True):
    c = match_start_content()
    c["ore"] = 2350
    c["income"] = 1450
    drain, warns = drain_text(6, 8, 642, 731)
    c["territory"] = {"own_nodes": 6, "enemy_nodes": 8, "nodes": 25, "cap": 10, "own_tickets": 642, "enemy_tickets": 731, "drain": drain, "drain_warns": warns}
    c["sectors"] = {"A1": {"side": "Own"}, "B1": {"side": "Own"}, "A2": {"side": "Own"}, "B2": {"side": "Own"}, "C2": {"side": "Own", "suppressed": True},
                    "A3": {"side": "Own", "cutOff": True}, "E5": {"side": "Enemy"}, "D5": {"side": "Enemy"}, "E4": {"side": "Enemy"}, "D4": {"side": "Enemy"},
                    "C4": {"side": "Enemy"}, "E3": {"side": "Enemy"}, "D3": {"side": "Enemy"}, "C3": {"side": "Enemy"}, "D2": {"side": "Pirate"}}
    c["alerts"] = [("Enemy ships in B2", (-2000, -2000)), ("Relay under attack: C2", (0, -2000)), ("Mining Rig lost: A3", (-4000, 0))]
    c["status"] = [
        {"runs": [{"text": "Large Hull"}], "bar": {"share": 0.62, "figure": "+2", "under": True}, "action": "OpenResearch",
         "src": "StatusLinesOf Hud.cpp:303-311 (topic name over a bar, '+N' queued)"},
        {"runs": [{"text": "Shipyards{}BUILDING 2{}WAITING 0{}".format(DOT, DOT, DOT), "sample": True}, {"text": "IDLE 1", "chip": True}], "action": "OpenProduction",
         "src": "StatusLinesOf Hud.cpp:403-410 (IDLE a chip when an idle Shipyard could start a ship)"},
        {"runs": [{"text": "Fleet", "sample": False}], "bar": {"share": 29 / 30.0, "figure": "29 / 30", "under": False}, "action": "OpenProduction",
         "src": "StatusLinesOf Hud.cpp:373-380, 411-412"}]
    c["marks"] = map_marks([{"kind": "Ship", "pos": (-2100, -1900), "radius": 14, "side": "Own"}, {"kind": "Ship", "pos": (-1900, -2050), "radius": 8, "side": "Own"},
                            {"kind": "Ship", "pos": (-1950, -1950), "radius": 8, "side": "Enemy"},
                            {"kind": "Structure", "pos": (3000, 3000), "radius": 40, "side": "Enemy", "remembered": True},
                            {"kind": "Derelict", "pos": (-1500, -3500), "radius": 20, "side": "Neutral"}])
    c["view"] = camera_view_corners((-2000, -2000))
    designs = [("Swarm", 6, 1, 1, 1), ("Lancer", 4, 2, 1, 2), ("Brawler", 2, 2, 1, 1)]
    c["selection"] = ["12 ships"] + ["{} {} {}".format(n, TIMES, name) for name, n, *_ in designs] + ["Hit points 3,860 / 4,320", "On patrol",
                                                                                                     "1 retreating to be repaired",
                                                                                                     "Scheduled: at 02:00, attack-move to C3"]
    c["selection_src"] = "Describe Hud.cpp:2750-2790; PendingOrderLine OrderForm.cpp:317-334 (GameClient.cpp:769-771)"
    bars = []
    for k in range(12):
        bars.append({"share": [1.0, 1.0, 0.82, 1.0, 0.55, 1.0, 0.9, 0.4, 1.0, 1.0, 0.2, 1.0][k], "retreating": k == 10, "hovered": False})
    c["shipBars"] = bars
    c["buttons"] = order_buttons(armed="AttackMove")
    c["retreat"] = {"setting": "Quarter", "mixed": True}
    c["entityBars"] = [{"foot": (960, 520), "health": 0.55, "built": None}, {"foot": (1100, 600), "health": 0.9, "built": 0.4}]
    return c


@state("hud", "busy-warships", "Mid-match: twelve warships of three designs selected, attack-move armed, the pointer over Stop",
       notes=["Every content value is sample; geometry is the code's. Drain row: own 6 nodes vs enemy 8, 642 tickets, Tuning.json territory "
              "(drainIntervalSeconds 3, drainTicketsPerNodeDifference 1).", "Two sample entity bars over the world (layer -1, drawn under every panel)."])
def s_hud_busy(out):
    lay_hud(out, busy_content(), hovered="Stop")


@state("hud", "scale-110", "The busy state at interface factor 110%, the most a match reaches on 16:9 (ADR-070 Consequences)", scale=1.1)
def s_hud_110(out):
    lay_hud(out, busy_content(), hovered="Stop")


@state("hud", "constructors", "Two Constructors selected: build buttons with their costs, and the retreat row")
def s_hud_constructors(out):
    c = match_start_content()
    c["selection"] = ["2 {} Constructor".format(TIMES), "Hit points 600 / 600"]
    c["selection_src"] = "Describe Hud.cpp:2757-2769; Tuning.json constructor.hitPoints"
    c["shipBars"] = [{"share": 1.0}, {"share": 1.0}]
    c["buttons"] = build_buttons(c["ore"])
    c["retreat"] = {"setting": "Quarter", "mixed": False}
    lay_hud(out, c, hovered="Build(Shipyard)")


@state("hud", "placing-relay", "A Relay's placement armed: the hint, and the sectors it could claim outlined on the minimap")
def s_hud_placing(out):
    c = match_start_content()
    c["ore"] = 640
    c["income"] = 350
    c["hint"] = "Placing Relay: left-click to build, right-click to cancel"
    c["hint_src"] = "Describe Hud.cpp:2551 ('Placing {}: left-click to build, right-click to cancel')"
    c["hint_sample"] = False
    c["sectors"]["B1"] = {"claimable": True}
    c["sectors"]["A2"] = {"claimable": True}
    c["selection"] = ["2 {} Constructor".format(TIMES), "Hit points 600 / 600"]
    c["shipBars"] = [{"share": 1.0}, {"share": 1.0}]
    c["buttons"] = build_buttons(c["ore"])
    c["retreat"] = {"setting": "Quarter", "mixed": False}
    lay_hud(out, c)


@state("hud", "relay-node-cap", "Placing a Relay at the node cap: the hint says why, and the Relay button says NODE CAP")
def s_hud_cap(out):
    c = match_start_content()
    c["ore"] = 900
    c["income"] = 450
    c["territory"]["own_nodes"] = 4
    c["territory"]["drain"], c["territory"]["drain_warns"] = drain_text(4, 1, 1000, 1000)
    c["hint"] = "Relay: at the node cap; upgrade the Command Station to claim more"
    c["hint_src"] = "Describe Hud.cpp:2553-2554"
    c["hint_sample"] = False
    c["selection"] = ["Constructor", "Hit points 300 / 300"]
    c["selectionHealth"] = 1.0
    c["buttons"] = build_buttons(c["ore"], has_lab=True, capped=True)
    c["retreat"] = {"setting": "Quarter", "mixed": False}
    lay_hud(out, c)


@state("hud", "one-ship", "One Lancer selected, holding a sector, hurt; Hold sector armed")
def s_hud_one(out):
    c = match_start_content()
    c["ore"] = 1810
    c["income"] = 950
    c["selection"] = ["Lancer", "Hit points 270 / 450", "Holding a sector"]
    c["selection_src"] = "Describe Hud.cpp:2755-2756, 2768-2769, 2784-2785"
    c["selectionHealth"] = 0.6
    c["buttons"] = order_buttons(armed="HoldSector")
    c["retreat"] = {"setting": "Half", "mixed": False, "hovered": "Never"}
    lay_hud(out, c)


@state("hud", "shipyard-selected", "A finished Shipyard selected: its work line, its next level and its window buttons with key caps")
def s_hud_shipyard(out):
    c = match_start_content()
    c["ore"] = 1240
    c["income"] = 950
    c["selection"] = ["Shipyard 01{}L1".format(DOT), "Hit points 2,500 / 2,500", "Building Swarm{}62%{}+2 queued".format(DOT, DOT),
                      "L2: Medium hulls, 3,000 hit points"]
    c["selection_src"] = "Describe Hud.cpp:2566-2572, 2587-2588, 2618-2620 (WorkLine Hud.cpp:244-254), 2676-2677"
    c["selectionHealth"] = 1.0
    c["status"] = [{"runs": [{"text": "Shipyards{}BUILDING 1{}WAITING 0{}IDLE 0".format(DOT, DOT, DOT)}], "action": "OpenProduction", "src": "StatusLinesOf Hud.cpp:403-410"},
                   {"runs": [{"text": "Fleet", "sample": False}], "bar": {"share": 8 / 12.0, "figure": "8 / 12", "under": False}, "action": "OpenProduction",
                    "src": "StatusLinesOf Hud.cpp:373-380"}]
    c["buttons"] = [{"id": "Production", "label": "Production", "action": "OpenProduction", "key": "P", "src": "Hud.cpp:2629-2635; KeyBindings.h:28"},
                    {"id": "ShipDesigner", "label": "Ship designer", "action": "OpenDesigner", "key": "D", "src": "Hud.cpp:2636-2638; KeyBindings.h:27"},
                    {"id": "Upgrade", "label": "Upgrade to L2{}0:30|150".format(DOT), "action": "Upgrade", "enabled": True,
                     "src": "Hud.cpp:2688-2696 ('Upgrade to L{}{}{}|{}'); Tuning.json structures[Shipyard].levels[0]"}]
    lay_hud(out, c, hovered="OpenDesigner")


@state("hud", "station-upgrading", "The Command Station selected while its upgrade is built: the button says UPGRADING with its bar")
def s_hud_station(out):
    c = match_start_content()
    c["ore"] = 420
    c["income"] = 950
    c["selection"] = ["Command Station{}L1".format(DOT), "Upgrading to L2, 40%", "Hit points 5,000 / 5,000", "Idle", "L2: 7 nodes, fleet 20, 6,000 hit points"]
    c["selection_src"] = "Describe Hud.cpp:2566-2572, 2585-2588, 2618-2620, 2655-2677"
    c["selectionHealth"] = 1.0
    c["buttons"] = [{"id": "Production", "label": "Production", "action": "OpenProduction", "key": "P", "src": "Hud.cpp:2629-2632"},
                    {"id": "Upgrade", "label": "Upgrade to L2{}0:45|300".format(DOT), "action": "Upgrade", "enabled": False,
                     "note": "UPGRADING{}40%".format(DOT), "progress": 400, "src": "Hud.cpp:2688-2696; Tuning.json structures[CommandStation].levels[0]"}]
    lay_hud(out, c)


@state("hud", "lab-needs-research", "The Research Lab selected: its next level needs research, so Upgrade says NEEDS RESEARCH")
def s_hud_lab(out):
    c = match_start_content()
    c["ore"] = 1100
    c["income"] = 950
    c["selection"] = ["Research Lab{}L1".format(DOT), "Hit points 1,500 / 1,500", "Researching Fusion Drive{}35%".format(DOT), "L2: tier 2, 1,800 hit points",
                      "L2 needs Improved Extraction and Hull Plating"]
    c["selection_src"] = "Describe Hud.cpp:2566-2572, 2622-2627, 2662-2687"
    c["selectionHealth"] = 1.0
    c["status"] = [{"runs": [{"text": "Fusion Drive"}], "bar": {"share": 0.35, "figure": "", "under": True}, "action": "OpenResearch",
                    "src": "StatusLinesOf Hud.cpp:303-311"}]
    c["buttons"] = [{"id": "Research", "label": "Research", "action": "OpenResearch", "key": "R", "src": "Hud.cpp:2640-2643; KeyBindings.h:29"},
                    {"id": "Upgrade", "label": "Upgrade to L2{}1:00|400".format(DOT), "action": "Upgrade", "enabled": False, "note": "NEEDS RESEARCH",
                     "src": "Hud.cpp:2688-2696"}]
    lay_hud(out, c)


@state("hud", "derelict-hint", "The pointer over a derelict: the hint says what it holds", notes=["Hint text from Hud::DescribeDerelict (Hud.cpp:2135-2145), set in GameClient.cpp:742-749."])
def s_hud_derelict(out):
    c = match_start_content()
    c["ore"] = 1500
    c["income"] = 950
    c["hint"] = "Derelict: 300 Ore, and part of Large Hull's research time{}salvaged 40%".format(DOT)
    c["hint_src"] = "DescribeDerelict Hud.cpp:2137-2143"
    lay_hud(out, c)


@state("hud", "server-silent", "A world on a dedicated server whose snapshots stopped 5 s ago",
       notes=["A world has no tickets (ADR-084), so the territory panel has the Nodes row only; nodeCap still applies.",
              "Address sample in Hud::ServerName's form (Hud.cpp:2221-2224)."])
def s_hud_silent(out):
    c = match_start_content()
    c["connection"] = {"server": "203.0.113.5:4433", "silent": 5}
    c["ore"] = 3120
    c["income"] = 1450
    c["territory"] = {"own_nodes": 5, "enemy_nodes": 4, "nodes": 25, "cap": 7}
    lay_hud(out, c)


@state("hud", "end-victory", "The match's end: Victory by domination, the world running on behind it", notes=["DescribeOutcome Hud.cpp:2199-2218."])
def s_hud_victory(out):
    c = busy_content()
    c["selection"] = []
    c["buttons"] = []
    c["retreat"] = None
    c["shipBars"] = []
    c["entityBars"] = []
    c["outcome"] = ("Victory", "By domination. Match length 47:12")
    lay_hud(out, c, hovered="BackToMenu")


@state("hud", "end-empire-fallen", "A world: the player's empire fell, and its seat restarts", notes=["DescribeOutcome Hud.cpp:2188-2196 (ADR-084 d7)."])
def s_hud_fallen(out):
    c = match_start_content()
    c["connection"] = {"server": "203.0.113.5:4433", "silent": 0}
    c["ore"] = 1000
    c["income"] = 0
    c["territory"] = {"own_nodes": 0, "enemy_nodes": 9, "nodes": 25, "cap": 0}
    c["sectors"] = {"E5": {"side": "Enemy"}, "A1": {"side": "Enemy"}}
    c["outcome"] = ("Empire fallen", "It restarts at your start in 58:41")
    lay_hud(out, c)


def window_state(c=None):
    return c or match_start_content()


@state("window", "designer-default", "The designer on Shipyard 01, the Swarm loaded (saved), nothing hovered",
       notes=["Default place: top-right corner (Hud.cpp:3655-3659). The designer covers the 'where the match runs' tag (ADR-086 d3).",
              "Height from ExtentOf with Tuning.json's 3 hulls, 3 drives, 5 weapons (two with splash notes) and None + 1 module: 913 units (ADR-070)."])
def s_designer(out):
    c = window_state()
    lay_hud(out, c)
    lay_designer(out, designer_panel(), window_corner(out, (out.width / out.scale - MARGIN - DESIGNER_W, MARGIN), DESIGNER_W))


@state("window", "designer-preview", "The designer with the Lance card under the pointer: bars and damage cards preview the change")
def s_designer_preview(out):
    lay_hud(out, window_state())
    lay_designer(out, designer_panel(preview=("weapon", 2)), window_corner(out, (out.width / out.scale - MARGIN - DESIGNER_W, MARGIN), DESIGNER_W), hovered="Lance")


@state("window", "designer-unsaved-editing", "New picks (Medium + Ion + Lance + Sensor Array), the name being typed, too long for the server",
       notes=["Medium needs Shipyard L2 (Tuning.json structures[Shipyard].levels[0].hulls), so Queue is dim and its detail reads 'Needs Shipyard L2' (Hud.cpp:819-824)."])
def s_designer_editing(out):
    lay_hud(out, window_state())
    lay_designer(out, designer_panel(picks=(2, 1, 2, 1), editing=True, typed="Lancer with the long range sensor fit"),
                 window_corner(out, (out.width / out.scale - MARGIN - DESIGNER_W, MARGIN), DESIGNER_W))


@state("window", "designer-no-shipyard", "The designer with no finished Shipyard: arrows dim, Queue dim, no slots filled")
def s_designer_none(out):
    lay_hud(out, window_state())
    lay_designer(out, designer_panel(has_shipyard=False, queued=0), window_corner(out, (out.width / out.scale - MARGIN - DESIGNER_W, MARGIN), DESIGNER_W))


@state("window", "production-shipyard", "Production on Shipyard 02 (L1): four starting designs, two dim as NEEDS L2, a queue of three")
def s_prod_yard(out):
    lay_hud(out, window_state())
    lay_production(out, production_panel_shipyard(), window_corner(out, (MARGIN, WINDOWS_TOP), PRODUCTION_W))


@state("window", "production-station", "Production on the Command Station: the Constructor card, the front job waiting for Ore")
def s_prod_station(out):
    lay_hud(out, window_state())
    c = TUNING["constructor"]
    lay_production(out, {"producer": "COMMAND STATION", "canStep": False, "ore": 40,
                         "options": [{"name": "Constructor", "detail": "", "cost": c["cost"], "enabled": False, "time": "{} s".format(tenths(c["buildSeconds"])), "strengths": []}],
                         "queue": [{"name": "Constructor", "front": True, "permille": 0, "waiting": True}, {"name": "Constructor"}]},
                   window_corner(out, (MARGIN, WINDOWS_TOP), PRODUCTION_W))


@state("window", "production-none", "Production with no finished producer: the hint")
def s_prod_none(out):
    lay_hud(out, window_state())
    lay_production(out, {"producer": "NO PRODUCER", "canStep": False, "ore": 1000, "options": [], "queue": [], "hint": "Build a Shipyard to make warships."},
                   window_corner(out, (MARGIN, WINDOWS_TOP), PRODUCTION_W))


@state("window", "research-page1", "Research: the first page of topics, tier-2 topics needing the Lab's level, a queue of two")
def s_research(out):
    lay_hud(out, window_state())
    lay_research(out, research_panel(), window_corner(out, (2 * MARGIN + PRODUCTION_W, WINDOWS_TOP), RESEARCH_W))


@state("window", "research-no-lab", "Research with no Research Lab: no topic can be queued")
def s_research_none(out):
    lay_hud(out, window_state())
    lay_research(out, research_panel(has_lab=False, queued=()), window_corner(out, (MARGIN, WINDOWS_TOP), RESEARCH_W))


@state("window", "controls", "The Controls window (F1), at its default place in the middle of the screen")
def s_controls(out):
    lay_hud(out, window_state())
    sw, sh = out.width / out.scale, out.height / out.scale
    lay_controls(out, window_corner(out, ((sw - CONTROLS_W) / 2.0, max(MARGIN, (sh - controls_height()) / 2.0)), CONTROLS_W))


@state("window", "orders-time-of-day", "Orders (O): two orders waiting; a time-of-day attack-move ready to give to 12 ships",
       notes=["Rows for a time-of-day trigger and a sector action: WHEN, HOUR, MINUTE, DO, WHERE, UNLESS (Hud.cpp:2248-2274). Form defaults 02:00 attack-move (OrderForm.h:103-108)."])
def s_orders(out):
    lay_hud(out, busy_content())
    panel = {"scheduled": ["At 02:00, attack-move to C3{}12 ships".format(DOT), "When enemies enter B2, hold B2, unless over 8 enemy CP{}6 ships".format(DOT)],
             "more": 0,
             "rows": [("WHEN", "At a time of day"), ("HOUR", "02"), ("MINUTE", "00"), ("DO", "Attack-move"), ("WHERE", "C3"), ("UNLESS", "Never")],
             "fires": "Fires at 02:00 on your clock, in 3:12:00",
             "give": {"label": "Give to 12 ships|", "action": "GiveOrder", "enabled": True, "sample": True}}
    lay_orders(out, panel, window_corner(out, (out.width / out.scale - MARGIN - ORDERS_W, WINDOWS_TOP), ORDERS_W), hovered="GiveOrder")


@state("window", "orders-event-disabled", "Orders: an event trigger with a target, nothing selected that can take it (Give the order dim, SELECT WARSHIPS)",
       notes=["Rows for an event trigger and a target action: WHEN, WATCHING, DO, WHERE, TARGET, UNLESS: 6 of the 7 kept (ORDER_ROWS, Hud.h:528)."])
def s_orders_event(out):
    lay_hud(out, match_start_content())
    panel = {"scheduled": [], "more": 0,
             "rows": [("WHEN", "Enemies enter a sector"), ("WATCHING", "A1"), ("DO", "Attack"), ("WHERE", "C3"), ("TARGET", "Shipyard (1 of 3)"),
                      ("UNLESS", "Over 8 enemy CP there")],
             "fires": "Fires once, the first time it happens",
             "give": {"label": "Give the order|", "action": "GiveOrder", "enabled": False, "note": "SELECT WARSHIPS"}}
    lay_orders(out, panel, window_corner(out, (out.width / out.scale - MARGIN - ORDERS_W, WINDOWS_TOP), ORDERS_W))


@state("window", "away", "While you were away: opened by itself with the first snapshot after the seat is taken again (GameClient.cpp:539-544)")
def s_away(out):
    lay_hud(out, match_start_content())
    lines = ["Built 14 ships and 3 structures", "Lost 9 ships and no structures", "Sectors gained: C2, B3", "Sectors lost: A3",
             "Order fired: attack-move to C3", "Order held back: hold B2"]
    h = away_height(len(lines))
    sw, sh = out.width / out.scale, out.height / out.scale
    lay_away(out, {"since": "Away for 7:42:10", "lines": lines}, window_corner(out, ((sw - AWAY_W) / 2.0, max(MARGIN, (sh - h) / 2.0)), AWAY_W))


@state("window", "three-windows-default", "Production, research and the designer open together, each at its default place (ADR-062 d6: side by side at 100%)",
       notes=["Back to front in the order opened: production (slot 0), research (slot 1), designer (Hud.cpp:3634-3687; WindowManager.cpp:99-114).",
              "16 + 478 + 16 + 560 + 16 + 818 + 16 = 1920 (ADR-062 d6)."])
def s_three(out):
    lay_hud(out, busy_content())
    lay_production(out, production_panel_shipyard(), window_corner(out, (MARGIN, WINDOWS_TOP), PRODUCTION_W))
    lay_research(out, research_panel(), window_corner(out, (2 * MARGIN + PRODUCTION_W, WINDOWS_TOP), RESEARCH_W))
    lay_designer(out, designer_panel(), window_corner(out, (out.width / out.scale - MARGIN - DESIGNER_W, MARGIN), DESIGNER_W))


def main():
    os.makedirs(OUT, exist_ok=True)
    index = []
    for screen, name, title, scale, notes, fn in STATES:
        out = Out(scale)
        fn(out)
        layers = sorted({e["layer"] for e in out.elements})
        doc = {"screen": screen, "state": name, "title": title, "head": HEAD, "frame": {"widthPixels": 1920, "heightPixels": 1080},
               "interfaceFactor": scale, "pixelsPerReferenceUnit": scale,
               "geometry": "derived from GameApp/Hud.cpp arithmetic by derive_layouts.py; not dumped from the game (no build in this container). "
                           "Elements with measured:false depend on DirectWrite text measurement (ADR-061) and are estimated.",
               "drawOrder": "layer -1 (entity bars) first; then layer 0 (HUD) and each window layer back to front; within a layer every 'panels' "
                            "element (panel/outline/bar/button) in order, then 'lines', then 'sprites', then 'texts' (Hud.h:714-760, GameClient.cpp:1289-1338).",
               "notes": notes, "layers": layers, "elements": out.elements}
        fname = "{}-{}.json".format(screen, name)
        with open(os.path.join(OUT, fname), "w", encoding="utf-8", newline="\n") as f:
            json.dump(doc, f, ensure_ascii=False, indent=1)
        index.append({"file": fname, "screen": screen, "state": name, "title": title, "elements": len(out.elements), "interfaceFactor": scale})
    with open(os.path.join(OUT, "_index.json"), "w", encoding="utf-8", newline="\n") as f:
        json.dump(index, f, ensure_ascii=False, indent=1)
    print("wrote {} layouts to {}".format(len(index), OUT))
    return index


if __name__ == "__main__":
    sys.exit(0 if main() else 1)

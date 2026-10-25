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
import re
import struct
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
            if v is None or (v is False and k not in ("measured", "enabled")):
                continue
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


def llround(v):  # std::llround: half away from zero
    return int(math.floor(v + 0.5)) if v >= 0 else -int(math.floor(-v + 0.5))


# Hull Plating's +15% (Tuning.json research[id 2]); Composite Plating and Ablative Armour add to the same percent.
HULL_PLATING_FACTOR = 1.0 + next(r for r in TUNING["research"] if r["id"] == 2)["effect"]["percent"] / 100.0


def hull_hundredths(hull, hull_factor=1.0):  # ViewOf(HullTuning), GameLogic/ShipDesign.cpp:38-50: hit points as the player has them
    return llround(hull["hitPoints"] * 100 * hull_factor)


def stats(h, d, w, m=None, hull_factor=1.0):  # DesignStatsOf, GameProtocol/DesignStats.h:76-94; hull_factor from research (Research.cpp:110)
    hull, drive, weapon = find(HULLS, h), find(DRIVES, d), find(WEAPONS, w)
    module = find(MODULES, m) if m else None
    return {"hp": llround(hull_hundredths(hull, hull_factor) * drive["hitPointsFactor"]), "armor": hull["armor"] * 100,
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


def best_of_all(hull_factor=1.0):  # Hud.cpp:494-527, over the snapshot's hull views, which carry the research's hit points
    best = {"hp": 0, "armor": 0, "speed": 0, "range": 0, "cost": 0, "build": 0, "damage": [0.0] * len(HULLS)}
    for h in HULLS:
        for d in DRIVES:
            for w in WEAPONS:
                st = stats(h["id"], d["id"], w["id"], hull_factor=hull_factor)
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
                    fill="solid", sample=True, source="Hud.cpp:3481-3485; SectorView::holder (Snapshot.h:299), guarded (Snapshot.h:308)",
                    note=GUARDED_NOTE if side == "Pirate" else None)
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
                        source="Hud.cpp:3517-3525 (ore asteroid outlined, ORE_OUTLINE_UNITS 2; dry when its known reserve is 0, Hud.cpp:2538, known only where seen, Simulation.cpp:331-342); " + src,
                        sample=m.get("sample", True), note=m.get("note"))
            else:
                out.add("panel", "minimap.mark." + m["kind"].lower(), at[0] - side / 2, at[1] - side / 2, side, side, color=color, fill="solid",
                        source=src, sample=m.get("sample", True), note=m.get("note"))
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
                source="Hud.cpp:3595-3605 (remembered: a cross)", note=m.get("note"))
        out.add("panel", "minimap.memory.down", at[0] - arm / 2, at[1] - a / 2, arm, a, color=col, fill="solid", sample=True, source="Hud.cpp:3604",
                note=m.get("note"))
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
        if "marks" in c:
            check_marks(c)
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
    # LayOrders passes no hovered action to AddButton (Hud.cpp:1979), so the Give button never lights under the pointer.
    add_button(out, "orders.give", (corner[0] + WINDOW_INSET, corner[1] + ORDERS_GIVE_TOP, room, BUTTON_H), panel["give"], False,
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
# The match's seed (ADR-072, ADR-073, ADR-074). Outpost::PlaceContent (GameLogic/Map.cpp:354-508) and Neuron::Random
# (NeuronServer/Random.cpp) are replayed here, so that the 34 seeded ore asteroids, the pirate outposts and the derelicts
# every in-match frame shows are what one seed places by the map's rules: placed asteroids (Map.cpp:381-419), then the
# outposts (421-440), then the derelicts and their topics (442-507). A match's seed is the clock (WinMain.cpp:134-139),
# so any seed is a sample. Seed 342 was picked because it puts the camps on the contested pairs D1/B5 and E2/A4 and the
# Large Hull topic on the flank derelict next to the player's home, which suits the frames. The generator is checked
# against RandomTests' pinned draws below. Float arithmetic is rounded to float at each step, as the code computes it
# under /fp:precise without contraction (AGENTS.md R16); the placement was not checked against a build.
# --------------------------------------------------------------------------------------------------------------------
SEED = 342
M64 = (1 << 64) - 1
PLACEMENT_SEED_SALT = 0x9E3779B97F4A7C15  # Map.cpp:17
PLACEMENT_ATTEMPTS = 10000                # Map.cpp:19


def f32(v):
    return struct.unpack("<f", struct.pack("<f", v))[0]


def rotl(x, k):
    return ((x << k) | (x >> (64 - k))) & M64


class Random:
    """Neuron::Random (NeuronServer/Random.cpp): xoshiro256** seeded through SplitMix64."""

    def __init__(self, seed):
        self.s = []
        state = seed & M64
        for _ in range(4):
            state = (state + 0x9E3779B97F4A7C15) & M64
            z = state
            z = ((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9) & M64
            z = ((z ^ (z >> 27)) * 0x94D049BB133111EB) & M64
            self.s.append(z ^ (z >> 31))

    def next_uint64(self):
        s = self.s
        result = (rotl((s[1] * 5) & M64, 7) * 9) & M64
        shifted = (s[1] << 17) & M64
        s[2] ^= s[0]
        s[3] ^= s[1]
        s[1] ^= s[2]
        s[0] ^= s[3]
        s[2] ^= shifted
        s[3] = rotl(s[3], 45)
        return result

    def next_below(self, bound):  # Lemire's method
        product = (self.next_uint64() >> 32) * bound
        low = product & 0xFFFFFFFF
        if low < bound:
            threshold = ((1 << 32) - bound) % bound
            while low < threshold:
                product = (self.next_uint64() >> 32) * bound
                low = product & 0xFFFFFFFF
        return product >> 32

    def next_unit(self):
        return (self.next_uint64() >> 11) * 2.0 ** -53


def check_random():  # the pinned draws of NeuronServerTests/RandomTests.cpp:13-39
    zero = Random(0)
    assert [zero.next_uint64() for _ in range(4)] == [0x99EC5F36CB75F2B4, 0xBF6E1F784956452A, 0x1A5F849D4933E6E0, 0x6AA594F1262D2D2C]
    other = Random(0xDEADBEEF)
    assert [other.next_uint64() for _ in range(2)] == [0xC5555444A74D7E83, 0x65C30D37B4B16E38]
    bounded = Random(42)
    assert [bounded.next_below(6) for _ in range(10)] == [0, 2, 4, 5, 5, 4, 4, 5, 4, 3]
    unit = Random(42)
    assert unit.next_unit() == 0.08386297105988216 and unit.next_unit() == 0.3789802506626686


SECTORS = MAP["sectors"]
SECTOR = {s["name"]: s for s in SECTORS}
SECTOR_ID = {s["id"]: s for s in SECTORS}


def node_of(name):
    return (SECTOR[name]["node"]["xMeters"], SECTOR[name]["node"]["zMeters"])


def contains(sec, p):  # SectorPlacement::Contains, borders included
    return sec["minXMeters"] <= p[0] <= sec["maxXMeters"] and sec["minZMeters"] <= p[1] <= sec["maxZMeters"]


def sector_at(p):  # Simulation::SectorAt (Simulation.cpp:2795-2799): the first sector that holds the point
    return next((s for s in SECTORS if contains(s, p)), None)


def mirror_of(sec):  # Map.cpp:216-222
    across = (-sec["node"]["xMeters"], -sec["node"]["zMeters"])
    return next(o for o in SECTORS if contains(o, across))


def drawing_sectors(kind):  # Map.cpp:252-263: the one of each pair with the lower identifier, in the map's order
    return [s for s in SECTORS if s.get("kind") == kind and mirror_of(s)["id"] >= s["id"]]


def distance32(a, b):  # Distance, GameLogic/PlaneVector.h:56-59, in float
    dx, dz = f32(a[0] - b[0]), f32(a[1] - b[1])
    return f32(math.sqrt(f32(f32(dx * dx) + f32(dz * dz))))


def place_content(seed, topics):
    """Outpost::PlaceContent (GameLogic/Map.cpp:354-508) on Map.json: the ore asteroids, the outposts and the derelicts."""
    rnd = Random(seed ^ PLACEMENT_SEED_SALT)
    gap = f32(MAP["minimumGapMeters"])
    rules = MAP["placement"]
    border, clearance, spacing = f32(rules["borderMeters"]), f32(rules["nodeClearanceMeters"]), f32(rules["oreSpacingMeters"])
    half = MAP["sizeMeters"] / 2.0
    edge = lambda p: min(half - abs(p[0]), half - abs(p[1]))  # EdgeClearance, Map.cpp:128-132, in double
    ore = [{"pos": (a["xMeters"], a["zMeters"]), "r": f32(a["radiusMeters"]), "yield": a["yield"], "reserve": a["reserve"], "seeded": False}
           for a in MAP["oreAsteroids"]]
    fields = [((f_["xMeters"], f_["zMeters"]), f32(f_["radiusMeters"])) for f_ in MAP["asteroidFields"]]
    starts = [(s["xMeters"], s["zMeters"]) for s in MAP["starts"]]

    def is_clear(p, r):  # Map.cpp:363-379
        if edge(p) - r < gap:
            return False
        for a in ore:
            meters = distance32(p, a["pos"])
            if f32(f32(meters - r) - a["r"]) < gap or meters < spacing:
                return False
        if any(f32(f32(distance32(p, fp) - r) - fr) < gap for fp, fr in fields):
            return False
        return not any(f32(distance32(p, s) - r) < gap for s in starts)

    def draw_in(sec, r):
        inset = f32(border + r)
        width = f32(f32(sec["maxXMeters"] - sec["minXMeters"]) - f32(2.0 * inset))
        depth = f32(f32(sec["maxZMeters"] - sec["minZMeters"]) - f32(2.0 * inset))
        if width <= 0 or depth <= 0:
            return None
        x = f32(f32(sec["minXMeters"] + inset) + f32(f32(rnd.next_unit()) * width))
        z = f32(f32(sec["minZMeters"] + inset) + f32(f32(rnd.next_unit()) * depth))
        return (x, z)

    kinds = {k["name"]: k for k in MAP["sectorKinds"]}
    for sec in SECTORS:
        if not sec.get("kind"):
            continue
        mirror = mirror_of(sec)
        if mirror["id"] < sec["id"]:
            continue
        own_mirror = mirror["id"] == sec["id"]
        node = node_of(sec["name"])
        for rule in kinds[sec["kind"]]["ore"]:
            r = f32(rule["radiusMeters"])
            for _ in range(rule["count"] // 2 if own_mirror else rule["count"]):
                placed = False
                for _ in range(PLACEMENT_ATTEMPTS):
                    p = draw_in(sec, r)
                    if p is None:
                        break
                    q = (-p[0], -p[1])
                    if distance32(p, node) < clearance or not is_clear(p, r):
                        continue
                    apart = distance32(p, q)
                    if f32(apart - f32(2.0 * r)) < gap or apart < spacing or not is_clear(q, r):
                        continue
                    for at in (p, q):
                        ore.append({"pos": at, "r": r, "yield": rule["yield"], "reserve": rule["reserve"], "seeded": True})
                    placed = True
                    break
                if not placed:
                    raise RuntimeError("seed {}: sector {} has no room for its asteroids (PlaceContent throws)".format(seed, sec["name"]))
    outposts, taken = [], []
    for rule in MAP["outposts"]:
        free = [s for s in drawing_sectors(rule["kind"]) if s["id"] not in taken]
        for _ in range(rule["count"]):
            sec = free.pop(rnd.next_below(len(free)))
            taken.append(sec["id"])
            outposts.append((sec["name"], rule["outpost"]))
            if mirror_of(sec)["id"] != sec["id"]:
                outposts.append((mirror_of(sec)["name"], rule["outpost"]))
    derelicts, salvaged, pairs = [], [], []

    def clear_of_all(p, r):  # Map.cpp:447-459
        if edge(p) - r < gap:
            return False
        apart = lambda other, other_r: f32(f32(distance32(p, other) - r) - other_r) >= gap
        return (all(apart(a["pos"], a["r"]) for a in ore) and all(apart(fp, fr) for fp, fr in fields) and
                all(apart(d["pos"], d["r"]) for d in derelicts) and all(apart(s, 0.0) for s in starts))

    for rule in MAP["derelicts"]:
        free = [s for s in drawing_sectors(rule["kind"]) if s["id"] not in salvaged]
        for _ in range(rule["count"]):
            sec = free.pop(rnd.next_below(len(free)))
            salvaged.append(sec["id"])
            r = f32(rule["radiusMeters"])
            node = node_of(sec["name"])
            placed = False
            for _ in range(PLACEMENT_ATTEMPTS):
                p = draw_in(sec, r)
                if p is None:
                    break
                q = (-p[0], -p[1])
                if (distance32(p, node) < clearance or not clear_of_all(p, r) or f32(distance32(p, q) - f32(2.0 * r)) < gap or
                        not clear_of_all(q, r)):
                    continue
                pairs.append(len(derelicts))
                for at, where in ((p, sec), (q, mirror_of(sec))):
                    derelicts.append({"pos": at, "r": r, "ore": rule["ore"], "hull": rule["hull"], "sector": where["name"], "topic": None})
                placed = True
                break
            if not placed:
                raise RuntimeError("seed {}: sector {} has no room for its derelict (PlaceContent throws)".format(seed, sec["name"]))
    for _ in range(llround(len(pairs) * MAP["derelictResearchPercent"] / 100.0)):  # 33% of 6 pairs: 2
        first = pairs.pop(rnd.next_below(len(pairs)))
        topic = topics[rnd.next_below(len(topics))]
        derelicts[first]["topic"] = derelicts[first + 1]["topic"] = topic
    return {"ore": ore, "outposts": outposts, "derelicts": derelicts}


def check_placement(placed):
    """The map's rules, checked again in double precision on what was placed (Map.json sectorKinds, placement, outposts, derelicts)."""
    rules = MAP["placement"]
    kinds = {k["name"]: k for k in MAP["sectorKinds"]}
    ore = placed["ore"]
    for sec in SECTORS:
        count = sum(1 for a in ore if sector_at(a["pos"]) is sec)
        expected = sum(r["count"] for r in kinds[sec["kind"]]["ore"]) or sum(1 for a in MAP["oreAsteroids"] if contains(sec, (a["xMeters"], a["zMeters"])))
        assert count == expected, (sec["name"], count, expected)
    for a in ore:
        if not a["seeded"]:
            continue
        sec = sector_at(a["pos"])
        inset = rules["borderMeters"] + a["r"] - 1e-3
        assert sec["minXMeters"] + inset <= a["pos"][0] <= sec["maxXMeters"] - inset and sec["minZMeters"] + inset <= a["pos"][1] <= sec["maxZMeters"] - inset
        assert math.dist(a["pos"], node_of(sec["name"])) >= rules["nodeClearanceMeters"] - 1e-3
        assert any(b["pos"] == (-a["pos"][0], -a["pos"][1]) for b in ore)
        for b in ore:
            if b is not a:
                d = math.dist(a["pos"], b["pos"])
                assert d >= rules["oreSpacingMeters"] - 1e-3 and d - a["r"] - b["r"] >= MAP["minimumGapMeters"] - 1e-3
        for f_ in MAP["asteroidFields"]:
            assert math.dist(a["pos"], (f_["xMeters"], f_["zMeters"])) - a["r"] - f_["radiusMeters"] >= MAP["minimumGapMeters"] - 1e-3
    assert len(ore) == 40 and sum(1 for a in ore if a["seeded"]) == 34
    guarded = [name for name, _ in placed["outposts"]]
    assert len(set(guarded)) == 7 and all(mirror_of(SECTOR[n])["name"] in guarded for n in guarded)
    assert {SECTOR[n]["kind"] for n in guarded} == {"contested", "rich", "center"} and sum(1 for n in guarded if SECTOR[n]["kind"] == "contested") == 4
    assert len(placed["derelicts"]) == 2 * sum(r["count"] for r in MAP["derelicts"])


check_random()
TOPIC_IDS = [r["id"] for r in TUNING["research"]]  # TopicsOf (InProcessServer.cpp:47-54): every topic, in the tuning data's order
PLACED = place_content(SEED, TOPIC_IDS)
check_placement(PLACED)
GUARDED = [name for name, _ in PLACED["outposts"]]  # E2, A4, D1, B5 (camps); E1, A5, C3 (strongholds)
OUTPOST_OF = dict(PLACED["outposts"])
SEED_NOTE = ("sample: placed by the match's seed (GameLogic/Map.cpp:354-508; Map.json sectorKinds and placement; ADR-072); here seed 342, "
             "replayed by derive_layouts.py's port of PlaceContent and Neuron::Random")
GUARDED_NOTE = ("sample: seed 342's pirate outposts: camps on 2 of the 4 contested pairs, D1/B5 and E2/A4; strongholds on the rich pair E1/A5 "
                "and the center C3 (Map.json outposts; Map.cpp:421-440; ADR-072 d7, ADR-073): 7 guarded sectors at the start of every match")
DERELICT_NOTE = ("sample: seed 342's derelict (Map.json derelicts; Map.cpp:442-507; ADR-074); seen while it is in sight or in a sector the "
                 "player holds unsuppressed (Simulation.cpp:289-304, 2981-2987)")


# --------------------------------------------------------------------------------------------------------------------
# Sample content: sectors, territory, income and the minimap's marks, each derived from the sample's holdings so that
# the panels agree with each other and with the rules
# --------------------------------------------------------------------------------------------------------------------
P1_START = tuple(MAP["starts"][0].values())
FOCUS = (P1_START[0] + 75.0, P1_START[1] + 75.0)  # the player's Constructors, ahead of its Command Station (Simulation.cpp:731-748)
HOMES = {"Own": "A1", "Enemy": "E5"}  # the sector each player's Command Station holds, on its start (Map.json starts)
STATION = next(s for s in TUNING["structures"] if s["kind"] == "CommandStation")


def node_cap(level):  # NodeCap, GameLogic/Tuning.cpp:732-741; a player without a station has level 1's (Simulation.cpp:2912-2920)
    nodes = STATION["nodes"]
    for lv in STATION["levels"][:level - 1]:
        nodes = max(nodes, lv.get("nodes", 0))
    return nodes


def fleet_cap(level):  # FleetCap, GameLogic/Tuning.cpp:743-752
    points = STATION["commandPoints"]
    for lv in STATION["levels"][:level - 1]:
        points = max(points, lv.get("commandPoints", 0))
    return points


def sectors_of(own=(), enemy=(), cleared=(), suppressed=(), claimable=(), stations=("Own", "Enemy")):
    """Each sector as Simulation::UpdateTerritory leaves it (Simulation.cpp:2812-2895): held, guarded by pirates while a pirate structure
    stands in it, suppressed, and cut off when its holder holds no path of adjacent sectors to its home sector."""
    side = {}
    for name in own:
        side[name] = "Own"
    for name in enemy:
        assert name not in side
        side[name] = "Enemy"
    assert all(n in GUARDED for n in cleared)
    guarded = [n for n in GUARDED if n not in cleared]
    assert not set(guarded) & set(side), "a guarded sector cannot be claimed (ADR-073)"
    linked = set()
    for holder in stations:
        home = HOMES[holder]
        if side.get(home) != holder:
            continue
        linked.add(home)
        frontier = [home]
        while frontier:
            for a in SECTOR[frontier.pop()]["adjacent"]:
                name = SECTOR_ID[a]["name"]
                if name not in linked and side.get(name) == holder:
                    linked.add(name)
                    frontier.append(name)
    out = {}
    for name, holder in side.items():
        out[name] = {"side": holder}
        if name in suppressed:
            assert name != HOMES[holder], "a Command Station is never suppressed"
            out[name]["suppressed"] = True
        if name not in linked:
            out[name]["cutOff"] = True
    for name in guarded:
        out[name] = {"side": "Pirate"}
    for name in claimable:
        assert name not in out
        out[name] = {"claimable": True}
    return out


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


def territory_of(sectors, cap, tickets=None):
    """Hud::Describe's Territory (Hud.cpp:2476-2531), counted from the same sectors the minimap washes; tickets only in a skirmish (ADR-084)."""
    t = {"own_nodes": sum(1 for v in sectors.values() if v.get("side") == "Own"),
         "enemy_nodes": sum(1 for v in sectors.values() if v.get("side") == "Enemy"), "nodes": len(SECTORS), "cap": cap}
    if tickets:
        t["own_tickets"], t["enemy_tickets"] = tickets
        t["drain"], t["drain_warns"] = drain_text(t["own_nodes"], t["enemy_nodes"], *tickets)
    return t


ORE = PLACED["ore"]
OWN_HOME_ORE = [a["pos"] for a in ORE if not a["seeded"] and sector_at(a["pos"])["name"] == "A1"]  # Map.json: three in each home


def ore_in(sector, i=0):
    return [a["pos"] for a in ORE if sector_at(a["pos"])["name"] == sector][i]


def rig_income(rigs, sectors, extraction=False, dry=()):
    """IncomeHundredthsPerSecond and RigIncomeHundredthsPerSecond (Simulation.cpp:2768-2793), TerritoryShare (2971-2979)."""
    rules = TUNING["rules"]
    rate = {"home": rules["miningRigOrePerSecondHome"], "near": rules["miningRigOrePerSecondNear"],
            "contested": rules["miningRigOrePerSecondContested"], "rich": rules["miningRigOrePerSecondRich"]}
    factor = 1.0 + (TUNING["research"][0]["effect"]["percent"] / 100.0 if extraction else 0.0)  # Improved Extraction
    total = 0
    for pos in rigs:
        asteroid = next(a for a in ORE if a["pos"] == pos)
        info = sectors.get(sector_at(pos)["name"], {})
        held = 0.0 if info.get("side") != "Own" or info.get("suppressed") else (TUNING["territory"]["cutOffIncomePercent"] / 100.0 if info.get("cutOff") else 1.0)
        share = rules["exhaustedYieldPercent"] / 100.0 if pos in dry else 1.0
        total += llround(rate[asteroid["yield"]] * 100 * factor * share * held)
    return total


def derelict_in(sector):
    return next(d for d in PLACED["derelicts"] if d["sector"] == sector)


def map_marks(sectors, extra=(), station=True, constructors=True, rigs=(), dry=(), derelicts=(), remembered=()):
    """The snapshot's entities as minimap marks (Hud.cpp:2530-2541): every asteroid field and ore asteroid, which are always seen
    (Simulation.cpp:245-247), the player's Command Station, Relays and rigs, and the derelicts the sample's player sees or remembers."""
    marks = []
    for f_ in MAP["asteroidFields"]:
        marks.append({"kind": "AsteroidField", "pos": (f_["xMeters"], f_["zMeters"]), "radius": f_["radiusMeters"], "side": "Neutral", "sample": False})
    for a in ORE:
        marks.append({"kind": "Asteroid", "pos": a["pos"], "radius": a["r"], "side": "Neutral", "sample": a["seeded"] or a["pos"] in dry,
                      "note": SEED_NOTE if a["seeded"] else None, "dry": a["pos"] in dry})
    if station:
        marks.append({"kind": "Structure", "pos": P1_START, "radius": STATION["footprintRadiusMeters"], "side": "Own", "sample": False})
    if constructors:
        for pos in starting_constructors():
            marks.append({"kind": "Ship", "pos": pos, "radius": CONSTRUCTOR["footprintRadiusMeters"], "side": "Own", "sector": "A1", "added": True,
                          "note": "sample: one of the two starting Constructors, where PlaceStartingBase puts it (Simulation.cpp:731-748; Tuning.json "
                                  "rules.startingConstructors)"})
    relay = next(s for s in TUNING["structures"] if s["kind"] == "Relay")
    for name, info in sectors.items():
        if info.get("side") == "Own" and name != HOMES["Own"]:  # a held sector has a finished Relay on its node (Simulation.cpp:2807-2811)
            marks.append({"kind": "Structure", "pos": node_of(name), "radius": relay["footprintRadiusMeters"], "side": "Own"})
    rig = next(s for s in TUNING["structures"] if s["kind"] == "MiningRig")
    for pos in rigs:  # a rig stands on its asteroid's center (Simulation.cpp:2081-2109)
        marks.append({"kind": "Structure", "pos": pos, "radius": rig["footprintRadiusMeters"], "side": "Own"})
    for name in derelicts:
        d = derelict_in(name)
        marks.append({"kind": "Derelict", "pos": d["pos"], "radius": d["r"], "side": "Neutral", "note": DERELICT_NOTE})
    for name in remembered:
        d = derelict_in(name)
        marks.append({"kind": "Derelict", "pos": d["pos"], "radius": d["r"], "side": "Neutral", "remembered": True, "note": DERELICT_NOTE})
    marks.extend(extra)
    return marks


def wreck_mark(sector):
    """What a cleared outpost leaves: a derelict where its last structure fell (Simulation.cpp:1740-1761); a camp's one platform stands on
    its node (Simulation.cpp:833-845)."""
    wreck = next(o for o in TUNING["pirates"]["outposts"] if o["name"] == OUTPOST_OF[sector])["wreck"]
    return {"kind": "Derelict", "pos": node_of(sector), "radius": wreck["radiusMeters"], "side": "Neutral",
            "note": "sample: the wreck of the {} the player cleared in {} ({} Ore, Tuning.json pirates.outposts)".format(OUTPOST_OF[sector], sector, wreck["ore"])}


# --------------------------------------------------------------------------------------------------------------------
# The player's base and fleets, and the enemy ships its alerts imply (V2 defect C). Hud::Describe marks every entity in the
# snapshot (Hud.cpp:2533-2541), so every structure and ship a frame's panels imply has a mark; check_marks holds each frame to it.
# --------------------------------------------------------------------------------------------------------------------
STRUCTURE = {s["kind"]: s for s in TUNING["structures"]}
CONSTRUCTOR = TUNING["constructor"]
HULL = {h["id"]: h for h in HULLS}
DESIGN = {d["name"]: d for d in STARTING}  # every seat starts with these (Simulation.cpp:569-580; InProcessServer.cpp:160), the enemy too
BASE_ROW_GAP_METERS, BASE_ROW_SPACING_METERS = 25.0, 10.0  # Simulation.cpp:78-79
# Where the sample's player built its base: in A1, the held sector a Shipyard must stand in (Placement.cpp:62-69), each footprint clear of the
# station, the home asteroids, the Constructors and each other (Placement.cpp:70-78), about 330 m out so that the 6-unit marks part.
# Shipyard 01 and the Lab stand inside the camera's view at FOCUS, which the frames that select them look at.
SHIPYARD_AT = [(-4230.0, -3760.0), (-4330.0, -4050.0), (-4050.0, -4330.0)]  # Shipyard 01, 02, 03
LAB_AT = (-3700.0, -3620.0)


def starting_constructors():
    """PlaceStartingBase (Simulation.cpp:731-748): a row of Constructors ahead of the station, facing the map's center (StartForward, 94-98)."""
    heading = math.atan2(-P1_START[1], -P1_START[0])
    forward = (math.cos(heading), math.sin(heading))
    across = (forward[1], -forward[0])
    radius, count = CONSTRUCTOR["footprintRadiusMeters"], TUNING["rules"]["startingConstructors"]
    ahead = STATION["footprintRadiusMeters"] + BASE_ROW_GAP_METERS + radius
    out = []
    for i in range(count):
        offset = (i - (count - 1) / 2.0) * (2.0 * radius + BASE_ROW_SPACING_METERS)
        out.append((P1_START[0] + forward[0] * ahead + across[0] * offset, P1_START[1] + forward[1] * ahead + across[1] * offset))
    return out


def structure_mark(kind, pos, note, site=False, side="Own"):
    """A structure's mark, its radius the footprint (Tuning.json structures[].footprintRadiusMeters); a site is marked as the structure is."""
    return {"kind": "Structure", "structure": kind, "pos": pos, "radius": STRUCTURE[kind]["footprintRadiusMeters"], "side": side, "site": site,
            "sector": sector_at(pos)["name"], "added": True, "note": note}


def ship_marks(counts, center, note, side="Own", spacing=45.0, per_row=4, **flags):
    """Warships of the named designs in rows of per_row, spacing apart, centred on center, each its hull's radius (Tuning.json
    hulls[].footprintRadiusMeters). counts: (design name, how many) pairs."""
    names = [name for name, n in counts for _ in range(n)]
    rows = (len(names) + per_row - 1) // per_row
    marks = []
    for i, name in enumerate(names):
        row, col = divmod(i, per_row)
        in_row = min(per_row, len(names) - row * per_row)
        pos = (center[0] + (col - (in_row - 1) / 2.0) * spacing, center[1] - (row - (rows - 1) / 2.0) * spacing)
        marks.append(dict({"kind": "Ship", "design": name, "pos": pos, "radius": HULL[DESIGN[name]["hull"]]["footprintRadiusMeters"], "side": side,
                           "sector": sector_at(pos)["name"], "added": True, "note": note}, **flags))
    return marks


def beside(at, kind, ahead):
    """A point ahead of a producer on its side toward the map's center, where it delivers a ship (Simulation.cpp:2752-2759)."""
    length = math.hypot(at[0], at[1])
    out = (-at[0] / length, -at[1] / length)
    reach = STRUCTURE[kind]["footprintRadiusMeters"] + ahead
    return (at[0] + out[0] * reach, at[1] + out[1] * reach)


def command_points(marks, building=()):
    """CommandPointsOf (Simulation.cpp:2932-2949): the player's warships, and the warship at the front of each Shipyard building one, each by
    its hull's Tuning.json hulls[].commandPoints; a Constructor counts nothing."""
    points = sum(HULL[DESIGN[m["design"]]["hull"]]["commandPoints"] for m in marks if m["side"] == "Own" and m.get("design"))
    return points + sum(HULL[DESIGN[name]["hull"]]["commandPoints"] for name in building)


def sight_of(m):
    """SightMetersOf (Simulation.cpp:252-262): an armed entity sees its weapon's range and the margin; anything else the unarmed sight."""
    sight = TUNING["sight"]
    if m.get("design"):
        return find(WEAPONS, DESIGN[m["design"]]["weapon"])["rangeMeters"] + sight["weaponMarginMeters"]
    if m["kind"] == "Structure" and not m.get("site") and "structureWeapon" in STRUCTURE.get(m.get("structure", ""), {}):
        return find(TUNING["structureWeapons"], STRUCTURE[m["structure"]]["structureWeapon"])["rangeMeters"] + sight["weaponMarginMeters"]
    return sight["unarmedMeters"]


def in_polygon(p, corners):
    inside = False
    for i in range(len(corners)):
        (ax, az), (bx, bz) = corners[i], corners[(i + 1) % len(corners)]
        if (az > p[1]) != (bz > p[1]) and p[0] < ax + (p[1] - az) * (bx - ax) / (bz - az):
            inside = not inside
    return inside


def check_marks(c):
    """Holds a frame's minimap marks to its own panels and to the rules:
    - one Shipyard mark per Shipyard the status line counts, a Lab mark when it shows the Lab's line, and the Fleet line's command points from
      the marked warships and the Shipyards building (StatusLinesOf, Hud.cpp:281-414; Simulation.cpp:2932-2949); no warship without that line;
    - every added mark inside its sector, its footprint clear of every other (Placement.cpp:70-78), a selected one inside the camera's view;
    - an enemy ship only where the player sees it: in a sector it holds unsuppressed, or in sight of its own (Simulation.cpp:243-249, 278-304,
      2981-2987); and a held sector suppressed exactly when an enemy warship is within the suppression radius of its Relay and none of the
      player's is (Simulation.cpp:2838-2850); each 'Enemy ships in X' alert with an enemy ship in X (Simulation.cpp:378-405)."""
    marks, status, sectors = c["marks"], c.get("status", []), c["sectors"]
    text = " ".join(run["text"] for line in status for run in line["runs"])
    counted = re.search(r"BUILDING (\d+)\D+WAITING (\d+)\D+IDLE (\d+)", text)
    own = [m for m in marks if m["side"] == "Own" and not m.get("remembered")]
    yards = [m for m in own if m.get("structure") == "Shipyard" and not m.get("site")]
    assert len(yards) == (sum(int(g) for g in counted.groups()) if counted else 0), (len(yards), text)
    assert len(c.get("building", ())) == (int(counted.group(1)) if counted else 0)
    labs = [m for m in own if m.get("structure") == "ResearchLab" and not m.get("site")]
    assert len(labs) == (1 if any(line["action"] == "OpenResearch" for line in status) else 0)
    fleet = next((line["bar"]["figure"] for line in status if line["runs"][0]["text"] == "Fleet"), None)
    assert command_points(own, c.get("building", ())) == (int(fleet.split(" / ")[0]) if fleet else 0), (command_points(own, c.get("building", ())), fleet)
    for a in marks:
        if not a.get("added"):
            continue
        assert sector_at(a["pos"])["name"] == a["sector"], (a["sector"], a["pos"])
        for b in marks:
            if b is not a and not b.get("remembered"):
                assert math.dist(a["pos"], b["pos"]) >= a["radius"] + b["radius"], (a["pos"], b["pos"])
        if a.get("selected"):
            assert in_polygon(a["pos"], c["view"]), a["pos"]
    observers = [m for m in own if m["kind"] in ("Ship", "Structure")]
    for e in marks:
        if e["side"] != "Enemy" or e.get("remembered") or e["kind"] != "Ship":
            continue
        held = sectors.get(sector_at(e["pos"])["name"], {})
        seen = held.get("side") == "Own" and not held.get("suppressed")
        assert seen or any(math.dist(o["pos"], e["pos"]) <= sight_of(o) + e["radius"] for o in observers), e["pos"]
    for name, info in sectors.items():
        if info.get("side") == "Own" and name != HOMES["Own"]:
            near = lambda side: any(m.get("design") and m["side"] == side and math.dist(m["pos"], node_of(name)) <= TUNING["territory"]["suppressionRadiusMeters"]
                                    for m in marks)
            assert bool(info.get("suppressed")) == (near("Enemy") and not near("Own")), name
    for txt, _ in c.get("alerts", []):
        entered = re.match(r"Enemy ships in (\w+)$", txt)
        if entered:
            assert any(m["side"] == "Enemy" and m["kind"] == "Ship" and sector_at(m["pos"])["name"] == entered.group(1) for m in marks), txt


def content_of(ore, sectors, cap, tickets=None, rigs=(), dry=(), extraction=False, server="", silent=0, view=FOCUS, **marks):
    """A HUD's content: Ore and income, the territory panel and the minimap, all from one sample state."""
    return {"ore": ore, "income": rig_income(rigs, sectors, extraction, dry), "connection": {"server": server, "silent": silent},
            "territory": territory_of(sectors, cap, tickets), "sectors": sectors, "marks": map_marks(sectors, rigs=rigs, dry=dry, **marks),
            "view": camera_view_corners(view)}


START_TICKETS = (TUNING["territory"]["tickets"], TUNING["territory"]["tickets"])
SERVER = "203.0.113.5:4433"  # in Hud::ServerName's form (Hud.cpp:2221-2224)


def match_start_content():
    return content_of(TUNING["rules"]["startingOre"], sectors_of(own=["A1"], enemy=["E5"]), node_cap(1), START_TICKETS)


def early_content(ore, **kw):
    """A few minutes in: the home sector only, a rig on each of its three asteroids (3 x 3.5 Ore a second)."""
    return content_of(ore, sectors_of(own=["A1"], enemy=["E5"]), node_cap(1), START_TICKETS, rigs=OWN_HOME_ORE, **kw)


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


def is_valid_design_name(name):  # IsValidDesignName, GameProtocol/Command.h:70-84
    return 0 < len(name) <= DESIGN_NAME_LIMIT and all(" " <= ch <= "~" for ch in name) and any(ch != " " for ch in name)


def designer_name(picks, designs):  # Designer::Name, GameApp/Designer.cpp:122-151, when nothing is typed
    h, d, w, m = picks
    match = next((x for x in designs if (x["h"], x["d"], x["w"], x["m"]) == (h, d, w, m)), None)
    if match:
        return match["name"]
    name = "{}+{}+{}".format(find(HULLS, h)["name"], find(DRIVES, d)["name"], find(WEAPONS, w)["name"])
    if m:
        module = find(MODULES, m)["name"]
        base = next((x for x in designs if (x["h"], x["d"], x["w"]) == (h, d, w) and x["m"] is None), None)
        if base and base["name"] != name and is_valid_design_name("{}+{}".format(base["name"], abbreviation(module))):
            return "{}+{}".format(base["name"], abbreviation(module))  # "Lancer+SA" (interface plan 2, task UI4.5)
        whole = "{}+{}".format(name, module)
        name = whole if is_valid_design_name(whole) else "{}+{}".format(name, abbreviation(module))
    return name


def designer_panel(picks=(1, 1, 1, None), preview=None, shipyard="SHIPYARD 01", has_shipyard=True, shipyard_level=1, queued=0, built=4, ore=1240,
                   editing=False, typed=None, available=None, retreat="Quarter", designs=None, hull_factor=1.0):
    """DescribeDesigner (Hud.cpp:585-835) over sample content; hull_factor is the research's hull hit points (Hull Plating 1.15)."""
    available = available or {"hull": {1, 2}, "drive": {1}, "weapon": {1, 2}}
    designs = designs if designs is not None else [{"name": d["name"], "h": d["hull"], "d": d["drive"], "w": d["weapon"], "m": None} for d in STARTING]
    h, d, w, m = picks
    match = next((x for x in designs if (x["h"], x["d"], x["w"], x["m"]) == (h, d, w, m)), None)
    hull, drive, weapon = find(HULLS, h), find(DRIVES, d), find(WEAPONS, w)
    # Typing starts from the shown name (Designer::BeginEditing, Designer.cpp:220-225) and stops at 32 characters (Designer.cpp:236).
    name = typed if typed is not None else designer_name(picks, designs)
    assert len(name) <= DESIGN_NAME_LIMIT, "a typed name stops at DESIGN_NAME_LIMIT (Designer.cpp:236)"
    valid = is_valid_design_name(name)
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
    rows = [("HULL", HULLS, "hull", lambda x: "{} HP{}ARM {}{}{}m/s".format(thousands((hull_hundredths(x, hull_factor) + 99) // 100), DOT, tenths(x["armor"]),
                                                                            DOT, tenths(x["speedMetersPerSecond"])),
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
    st = stats(h, d, w, m, hull_factor)
    ps = stats(*pv, hull_factor=hull_factor) if previewing else None
    best_all = best_of_all(hull_factor) if hull_factor != 1.0 else BEST
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
    panel["bars"] = [bar("Hit points", "", best_all["hp"], lambda x: x["hp"], lambda x: thousands(hp_whole(x)), hp_whole, True),
                     bar("Armor", "", best_all["armor"], lambda x: x["armor"], lambda x: tenths(x["armor"] / 100), lambda x: x["armor"] / 100, False),
                     bar("Speed", "m/s", best_all["speed"], lambda x: x["speed"], lambda x: tenths(x["speed"]), lambda x: x["speed"], False),
                     bar("Range", "m", best_all["range"], lambda x: x["range"], lambda x: tenths(x["range"]), lambda x: x["range"], False),
                     bar("Sensors", "m", best_all["sensor"], lambda x: x["sight"], lambda x: tenths(x["sight"]) if x["sight"] > 0 else "-", lambda x: x["sight"], False),
                     bar("Cost", "ore", best_all["cost"], lambda x: x["cost"], lambda x: str(x["cost"]), lambda x: x["cost"], True, True),
                     bar("Build", "s", best_all["build"], lambda x: x["build"], lambda x: tenths(x["build"]), lambda x: x["build"], False, True)]
    shown = ps or st
    dmg = []
    for i, hh in enumerate(HULLS):
        dps = formation_dps(shown, hh)
        cur = formation_dps(st, hh)
        share = min(max(dps / best_all["damage"][i], 0), 1)
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
    """DescribeProduction (Hud.cpp:2836-2898) on Shipyard 02, at L1, with the four starting designs saved."""
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


def research_panel(ore=1240, has_lab=True, built=True, queued=(2, 1), researched=(), first=0, front_permille=450):
    """DescribeResearch (Hud.cpp:2900-2989) over Tuning.json's topics, nothing researched yet but what is passed. A topic may follow its
    prerequisite in the queue (Simulation.cpp:3170-3189), so a queued prerequisite counts as known."""
    assert len(queued) <= QUEUE_LIMIT
    name_of = {r["id"]: r for r in TUNING["research"]}
    for i, q in enumerate(queued):  # the queue is researched in order, so the front topic's prerequisites are researched
        assert all(p in researched or (i > 0 and p in queued[:i]) for p in name_of[q]["requires"]), (q, researched, queued)
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
        queue.append({"name": name[q], "front": i == 0, "permille": front_permille if i == 0 else 0})
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


SEED_STATE_NOTE = ("Map content is seed 342's, a sample (a match's seed is the clock, WinMain.cpp:134-139): the 7 sectors pirates guard at the start of every "
                   "match, camps on the contested pairs D1/B5 and E2/A4 and strongholds on the rich pair E1/A5 and the center C3 (Map.json outposts; "
                   "Map.cpp:421-440; ADR-072 d7, ADR-073); and all 40 ore asteroids, Map.json's 6 home ones and the 34 the seed places by Map.json "
                   "sectorKinds and placement (Map.cpp:381-419), drawn under the fog since the map's asteroids are always seen (Simulation.cpp:245-247; "
                   "Hud.cpp:3486-3528). A held sector shows its Relay on its node, and each rig the income implies stands on its asteroid.")
WORLD_NOTE = ("A world: no tickets and no drain (ADR-084), the tag names the server (ADR-086). Its map is placed by its own seed (InProcessServer.cpp:134); "
              "seed 342's placement stands in for it.")


@state("hud", "match-start", "First snapshot of a Normal skirmish: nothing selected",
       notes=["Starting Ore and tickets from Tuning.json rules.startingOre / territory.tickets; no Mining Rig yet, so the income is a '+0/s' chip.",
              "No Lab and no finished Shipyard, so the status panel is not drawn, but its 94 units are kept (Hud.cpp:149, 3160-3161).", SEED_STATE_NOTE])
def s_hud_start(out):
    lay_hud(out, match_start_content())


# The busy state, mid-match (CS L3: node cap 10, fleet cap 30). Hull Plating is researched, since Large Hull, at the front of the Lab's queue,
# needs it (Tuning.json research[id 6].requires; a queue is researched in order, Simulation.cpp:3170-3189).
BUSY_RESEARCHED = (2,)
BUSY_QUEUE = (6, 1, 3)  # Large Hull 62%, then Improved Extraction and Mass Driver Calibration: "+2"
BUSY_STATUS = [
    {"runs": [{"text": "Large Hull"}], "bar": {"share": 0.62, "figure": "+2", "under": True}, "action": "OpenResearch",
     "src": "StatusLinesOf Hud.cpp:303-311 (topic name over a bar, '+N' queued)"},
    {"runs": [{"text": "Shipyards{}BUILDING 2{}WAITING 0{}".format(DOT, DOT, DOT), "sample": True}, {"text": "IDLE 1", "chip": True}], "action": "OpenProduction",
     "src": "StatusLinesOf Hud.cpp:403-410 (IDLE a chip when an idle Shipyard could start a ship: a Swarm, 1 command point, fits 29 of 30)"},
    {"runs": [{"text": "Fleet", "sample": False}], "bar": {"share": 29 / 30.0, "figure": "29 / 30", "under": False}, "action": "OpenProduction",
     "src": "StatusLinesOf Hud.cpp:373-380, 411-412; CommandPointsOf counts the ships building (Simulation.cpp:2932-2949)"}]
BUSY_FLEET = [("Swarm", (1, 1, 1), [1.0, 1.0, 0.82, 1.0, 0.55, 1.0]), ("Lancer", (2, 1, 2), [0.9, 0.4, 1.0, 1.0]), ("Brawler", (2, 1, 1), [0.2, 1.0])]


def busy_selection():
    """Describe's lines for the twelve ships (Hud.cpp:2704-2790): each ship's maximum is its design's, with Hull Plating's +15%
    (DesignStats.h:86, ShipDesign.cpp:42; a finished topic rescales the ships, Simulation.cpp:3110-3130)."""
    hp = mx = 0
    bars = []
    for _, (h, d, w), shares in BUSY_FLEET:
        most = stats(h, d, w, hull_factor=HULL_PLATING_FACTOR)["hp"]
        for share in shares:
            now = llround(most * share)
            hp, mx = hp + now, mx + most
            bars.append(now / most)
    lines = ["12 ships"] + ["{} {} {}".format(len(shares), TIMES, name) for name, _, shares in BUSY_FLEET]
    lines += ["Hit points {} / {}".format(thousands((hp + 99) // 100), thousands((mx + 99) // 100)), "On patrol", "1 retreating to be repaired",
              "Scheduled: at 02:00, attack-move to C3"]
    return lines, [{"share": share, "retreating": k == 10, "hovered": False} for k, share in enumerate(bars)]


BUSY_SITE_AT = (-1965.0, -2050.0)  # under the 40% site's entity bar: the ground below the screen's (1100, 600) with the camera at B2's node


def busy_marks():
    """What the busy state's panels imply on the minimap. The status panel counts 3 Shipyards (BUILDING 2, IDLE 1) and the Lab's line; the 12
    selected warships fight in B2, where the camera looks and 'Enemy ships in B2' points; 'Relay under attack: C2' and C2's suppression put
    enemy warships within 400 m of C2's Relay and none of the player's; and A3's rig was lost within the alert's 8 s (Alerts.h:15)."""
    yard = "sample: the status line's Shipyards (BUILDING 2 · WAITING 0 · IDLE 1, Hud.cpp:403-410): "
    marks = [structure_mark("Shipyard", SHIPYARD_AT[0], yard + "Shipyard 01, idle at L1, having built 14 ships (designer 'BUILT · 14')"),
             structure_mark("Shipyard", SHIPYARD_AT[1], yard + "Shipyard 02, L1, building a Swarm, 62% (the production window)"),
             structure_mark("Shipyard", SHIPYARD_AT[2], yard + "Shipyard 03, at L2 since Medium hulls were built, building a Lancer"),
             structure_mark("ResearchLab", LAB_AT, "sample: the Lab the status panel's 'Large Hull' line implies (Hud.cpp:281-311)"),
             structure_mark("DefensePlatform", BUSY_SITE_AT, "sample: the site 40% built whose bars stand beside B2's Relay (its foot at 1100, 600), "
                            "taken for a Defence Platform: 61 m from the Relay, clear of its footprint (Placement.cpp:70-78)", site=True)]
    # The two Constructors build it, one each side, clear of its footprint and the Relay's.
    marks += [{"kind": "Ship", "pos": p, "radius": CONSTRUCTOR["footprintRadiusMeters"], "side": "Own", "sector": "B2", "added": True,
               "note": "sample: one of the two Constructors, building the Defence Platform site in B2"} for p in ((-1925.0, -2085.0), (-2000.0, -2078.0))]
    # The twelve selected, as the selection panel lists them (6 Swarm, 4 Lancer, 2 Brawler: 18 command points), in the camera's view over B2;
    # the first Brawler, at 20%, retreats to the Command Station, the nearest repairer without a Repair Bay (Simulation.cpp:2553-2589).
    picked = "sample: one of the 12 selected warships ('12 ships'), on patrol in B2 in the camera's view"
    marks += ship_marks([("Swarm", 6), ("Lancer", 4), ("Brawler", 1)], (-2100.0, -1880.0), picked, selected=True)
    marks += ship_marks([("Brawler", 1)], (-2400.0, -2400.0), "sample: the selected Brawler '1 retreating to be repaired', on its way to the Command Station")
    # The six the Orders window gives 'hold B2' to, waiting in A1: 2 Swarm, 2 Picket and 2 Lancer, 8 command points.
    marks += ship_marks([("Swarm", 2), ("Picket", 2), ("Lancer", 2)], (-3550.0, -3550.0), "sample: one of the 6 ships the scheduled 'hold B2' order "
                        "waits with (Orders window), in A1", per_row=3)
    # The enemy: in B2, by the alert's place and among the player's ships, which keep B2 from being suppressed; at C2's Relay, which they
    # attack and suppress, seen within its 200 m sight; and in A3, by the lost rig's asteroid, more than 400 m from A3's Relay, seen since A3
    # is held unsuppressed (Simulation.cpp:2981-2987).
    marks += ship_marks([("Swarm", 2), ("Brawler", 2)], (-1915.0, -1890.0), "sample: an enemy warship in B2, which the alert 'Enemy ships in B2' "
                        "names; seen in a sector the player holds unsuppressed", side="Enemy", spacing=60.0, per_row=2)
    marks += ship_marks([("Swarm", 3)], (60.0, -1910.0), "sample: an enemy warship attacking C2's Relay ('Relay under attack: C2') from within "
                        "its Mass Driver's 120 m, which suppresses it; seen within the Relay's sight", side="Enemy", spacing=35.0)
    marks += ship_marks([("Swarm", 2)], (-3650.0, 760.0), "sample: an enemy warship by the asteroid whose rig it destroyed ('Mining Rig lost: A3'); "
                        "seen, since A3 is held unsuppressed", side="Enemy", spacing=40.0)
    marks.append({"kind": "Structure", "pos": node_of("C4"), "radius": 30, "side": "Enemy", "remembered": True})
    return marks


def busy_content():
    # A2's Relay was lost in a raid, so A3, held beyond it, is no longer linked to A1: cut off (Simulation.cpp:2862-2885).
    sectors = sectors_of(own=["A1", "B1", "C1", "B2", "C2", "A3"], enemy=["E5", "D5", "E4", "D4", "E3", "C5", "C4", "D3"], suppressed=["C2"])
    assert sectors["A3"].get("cutOff") and not any(v.get("cutOff") for n, v in sectors.items() if n != "A3")
    marks = busy_marks()
    # Seen derelicts: A3's and C1's, in held sectors; C2's is remembered, its sector suppressed; B1's was salvaged (the derelict hint's).
    c = content_of(2350, sectors, node_cap(3), (642, 731), rigs=OWN_HOME_ORE + [ore_in("B1", 1)], view=(-2000, -2000), derelicts=["A3", "C1"],
                   remembered=["C2"], constructors=False, extra=marks)
    c["building"] = ["Swarm", "Lancer"]  # Shipyard 02's Swarm (the production window) and Shipyard 03's Lancer: 3 of the 29 command points
    assert c["income"] == 1450  # three home rigs and one in B1; A3's rig was lost
    c["alerts"] = [("Enemy ships in B2", node_of("B2")), ("Relay under attack: C2", node_of("C2")), ("Mining Rig lost: A3", ore_in("A3"))]
    c["status"] = BUSY_STATUS
    c["selection"], c["shipBars"] = busy_selection()
    c["selection_src"] = ("Describe Hud.cpp:2704-2790 (maximum: the designs' hit points with Hull Plating, DesignStats.h:86); "
                          "PendingOrderLine OrderForm.cpp:317-334 (GameClient.cpp:769-771)")
    c["buttons"] = order_buttons(armed="AttackMove")
    c["retreat"] = {"setting": "Quarter", "mixed": True}
    # A hurt structure, and a site 40% built: a site starts at a tenth of its hit points and gains the rest with the work (Simulation.cpp:71,
    # 2449-2456), 0.1 + 0.9 x 0.4 = 46%.
    built = 0.4
    c["entityBars"] = [{"foot": (960, 520), "health": 0.55, "built": None},
                       {"foot": (1100, 600), "health": 1.0 / SITE_DIVISOR + (1.0 - 1.0 / SITE_DIVISOR) * built, "built": built}]
    return c


SITE_DIVISOR = 10  # SITE_STARTING_HIT_POINTS_DIVISOR, Simulation.cpp:71
BUSY_NOTES = ["Every content value is sample; geometry is the code's. Drain row: own 6 nodes vs enemy 8, 642 tickets, Tuning.json territory "
              "(drainIntervalSeconds 3, drainTicketsPerNodeDifference 1).",
              "Hit points: 6 Swarm at 227.7 and 6 Medium-hull ships at 517.5 with Hull Plating, a maximum of 4,472; the per-ship bars are the ships' "
              "shares of theirs and add up to the line's figure.",
              "Two sample entity bars over the world (layer -1, drawn under every panel): a hurt structure, and a site 40% built at 46% of its hit points.",
              "Sectors: A3 is cut off because A2 is not held; C2's Relay is suppressed. " + SEED_STATE_NOTE]


@state("hud", "busy-warships", "Mid-match: twelve warships of three designs selected, attack-move armed, the pointer over Stop", notes=BUSY_NOTES)
def s_hud_busy(out):
    lay_hud(out, busy_content(), hovered="Stop")


@state("hud", "scale-110", "The busy state at interface factor 110%, the most a match reaches on 16:9 (ADR-070 Consequences)", scale=1.1, notes=BUSY_NOTES)
def s_hud_110(out):
    lay_hud(out, busy_content(), hovered="Stop")


@state("hud", "constructors", "Two Constructors selected: build buttons with their costs, and the retreat row", notes=[SEED_STATE_NOTE])
def s_hud_constructors(out):
    c = match_start_content()
    c["selection"] = ["2 {} Constructor".format(TIMES), "Hit points 600 / 600"]
    c["selection_src"] = "Describe Hud.cpp:2757-2769; Tuning.json constructor.hitPoints"
    c["shipBars"] = [{"share": 1.0}, {"share": 1.0}]
    c["buttons"] = build_buttons(c["ore"])
    c["retreat"] = {"setting": "Quarter", "mixed": False}
    lay_hud(out, c, hovered="Build(Shipyard)")


@state("hud", "placing-relay", "A Relay's placement armed: the hint, and the sectors it could claim outlined on the minimap",
       notes=["Claimable: free, unguarded, next to a held sector, under the node cap (CanClaim, Placement.cpp:81-93): B1 and A2. One home rig, 3.5 Ore a second.",
              SEED_STATE_NOTE])
def s_hud_placing(out):
    c = content_of(640, sectors_of(own=["A1"], enemy=["E5"], claimable=["B1", "A2"]), node_cap(1), START_TICKETS, rigs=OWN_HOME_ORE[:1])
    c["hint"] = "Placing Relay: left-click to build, right-click to cancel"
    c["hint_src"] = "Describe Hud.cpp:2551 ('Placing {}: left-click to build, right-click to cancel')"
    c["hint_sample"] = False
    c["selection"] = ["2 {} Constructor".format(TIMES), "Hit points 600 / 600"]
    c["shipBars"] = [{"share": 1.0}, {"share": 1.0}]
    c["buttons"] = build_buttons(c["ore"])
    c["retreat"] = {"setting": "Quarter", "mixed": False}
    lay_hud(out, c)


@state("hud", "relay-node-cap", "Placing a Relay at the node cap: the hint says why, and the Relay button says NODE CAP",
       notes=["Four sectors held at the Command Station L1's cap of 4 (Tuning.json structures[CommandStation].nodes); the enemy, on one, has been "
              "drained 60 tickets as the player claimed its three.",
              "The Research Lab is still being built: ONE PER PLAYER counts any Lab of the player's (Hud.cpp:2806-2811), and the status panel only a "
              "finished one (Hud.cpp:281-300), so with no finished Shipyard either, no status panel is drawn.", SEED_STATE_NOTE])
def s_hud_cap(out):
    sectors = sectors_of(own=["A1", "B1", "A2", "B2"], enemy=["E5"])
    lab = structure_mark("ResearchLab", LAB_AT, "sample: the Research Lab still being built, which the Lab button's ONE PER PLAYER implies "
                         "(Hud.cpp:2806-2811); a site is marked as a finished structure is", site=True)
    c = content_of(900, sectors, node_cap(1), (1000, 940), rigs=OWN_HOME_ORE + [ore_in("B1", 1)], derelicts=["B1"], extra=[lab])
    assert c["territory"]["own_nodes"] == c["territory"]["cap"] and c["income"] == 1450
    c["hint"] = "Relay: at the node cap; upgrade the Command Station to claim more"
    c["hint_src"] = "Describe Hud.cpp:2553-2554"
    c["hint_sample"] = False
    c["selection"] = ["Constructor", "Hit points 300 / 300"]
    c["selectionHealth"] = 1.0
    c["buttons"] = build_buttons(c["ore"], has_lab=True, capped=True)
    c["retreat"] = {"setting": "Quarter", "mixed": False}
    lay_hud(out, c)


@state("hud", "one-ship", "One Lancer selected, holding a sector, hurt; Hold sector armed",
       notes=["A Lancer's Medium hull needs a Shipyard at L2, so a finished Shipyard stands and the status panel shows the Shipyards' and the fleet's lines.",
              "No Lab, so no Hull Plating: the Lancer's maximum is 500 x 0.9 = 450 (DesignStats.h:86).", SEED_STATE_NOTE])
def s_hud_one(out):
    sectors = sectors_of(own=["A1", "B1", "A2"], enemy=["E5", "D5", "E4"])
    # The status panel's one Shipyard, at L2 (the Lancer's Medium hull), building a Lancer; the selected Lancer holds A1, by its node, in the
    # camera's view; 3 Swarm and a Brawler hold B1: 2 + 2 + 5 = the fleet line's 9 command points.
    marks = [structure_mark("Shipyard", SHIPYARD_AT[0], "sample: the status line's one Shipyard (BUILDING 1 · WAITING 0 · IDLE 0), at L2, "
                            "building a Lancer")]
    marks += ship_marks([("Lancer", 1)], (-3925.0, -3860.0), "sample: the selected Lancer, holding A1 by its node, in the camera's view",
                        selected=True)
    marks += ship_marks([("Swarm", 3), ("Brawler", 1)], (-2000.0, -3880.0), "sample: one of the other warships the fleet line counts, "
                        "holding B1")
    c = content_of(1810, sectors, node_cap(1), (994, 988), rigs=OWN_HOME_ORE + [ore_in("B1", 0), ore_in("B1", 1)], derelicts=["B1"], extra=marks)
    c["building"] = ["Lancer"]
    c["status"] = [{"runs": [{"text": "Shipyards{}BUILDING 1{}WAITING 0{}IDLE 0".format(DOT, DOT, DOT)}], "action": "OpenProduction",
                    "src": "StatusLinesOf Hud.cpp:403-410"},
                   {"runs": [{"text": "Fleet", "sample": False}], "bar": {"share": 9 / 12.0, "figure": "9 / 12", "under": False}, "action": "OpenProduction",
                    "src": "StatusLinesOf Hud.cpp:373-380, 411-412"}]
    c["selection"] = ["Lancer", "Hit points 270 / 450", "Holding a sector"]
    c["selection_src"] = "Describe Hud.cpp:2755-2756, 2768-2769, 2784-2785"
    c["selectionHealth"] = 0.6
    c["buttons"] = order_buttons(armed="HoldSector")
    c["retreat"] = {"setting": "Half", "mixed": False, "hovered": "Never"}
    lay_hud(out, c)


EARLY_NOTE = ("A few minutes in: the home sector only, a rig on each of its three asteroids, 3 x 3.5 = 10.5 Ore a second (Tuning.json "
              "rules.miningRigOrePerSecondHome). " + SEED_STATE_NOTE)


@state("hud", "shipyard-selected", "A finished Shipyard selected: its work line, its next level and its window buttons with key caps", notes=[EARLY_NOTE])
def s_hud_shipyard(out):
    # The one Shipyard the status line counts is the selected one, in the camera's view; at L1 it builds Small hulls only, so the fleet line's
    # 8 command points are its Swarm under way and 7 Small-hull warships it delivered (Simulation.cpp:2752-2759).
    marks = [structure_mark("Shipyard", SHIPYARD_AT[0], "sample: the selected Shipyard 01, L1, the status line's one Shipyard (BUILDING 1 · "
                            "WAITING 0 · IDLE 0)")]
    marks[0]["selected"] = True
    marks += ship_marks([("Swarm", 5), ("Picket", 2)], beside(SHIPYARD_AT[0], "Shipyard", 60.0), "sample: one of the 7 Small-hull warships "
                        "the fleet line counts (8 / 12, the Swarm building included), by Shipyard 01", spacing=30.0)
    c = early_content(1240, extra=marks)
    c["building"] = ["Swarm"]
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


@state("hud", "station-upgrading", "The Command Station selected while its upgrade is built: the button says UPGRADING with its bar", notes=[EARLY_NOTE])
def s_hud_station(out):
    c = early_content(420)
    c["selection"] = ["Command Station{}L1".format(DOT), "Upgrading to L2, 40%", "Hit points 5,000 / 5,000", "Idle", "L2: 7 nodes, fleet 20, 6,000 hit points"]
    c["selection_src"] = "Describe Hud.cpp:2566-2572, 2585-2588, 2618-2620, 2655-2677"
    c["selectionHealth"] = 1.0
    c["buttons"] = [{"id": "Production", "label": "Production", "action": "OpenProduction", "key": "P", "src": "Hud.cpp:2629-2632"},
                    {"id": "Upgrade", "label": "Upgrade to L2{}0:45|300".format(DOT), "action": "Upgrade", "enabled": False,
                     "note": "UPGRADING{}40%".format(DOT), "progress": 400, "src": "Hud.cpp:2688-2696; Tuning.json structures[CommandStation].levels[0]"}]
    lay_hud(out, c)


LAB = next(s for s in TUNING["structures"] if s["kind"] == "ResearchLab")


def missing_for_lab_level(level, researched):  # Hud.cpp:2678-2687: the topics the next level needs that are not researched
    names = {r["id"]: r["name"] for r in TUNING["research"]}
    return " and ".join(names[t] for t in LAB["levels"][level - 1].get("requires", []) if t not in researched)


@state("hud", "lab-needs-research", "The Research Lab selected: its next level needs research, so Upgrade says NEEDS RESEARCH",
       notes=["Fusion Drive needs Hull Plating (Tuning.json research[id 5].requires), so with Fusion Drive at the front of the queue Hull Plating is "
              "researched, and the Lab's L2 (requires 1 and 2) still needs Improved Extraction alone (Hud.cpp:2678-2687).", EARLY_NOTE])
def s_hud_lab(out):
    lab = structure_mark("ResearchLab", LAB_AT, "sample: the selected Research Lab, L1, researching Fusion Drive, in the camera's view")
    lab["selected"] = True
    c = early_content(1100, extra=[lab])
    missing = missing_for_lab_level(1, researched=(2,))
    assert missing == "Improved Extraction"
    c["selection"] = ["Research Lab{}L1".format(DOT), "Hit points 1,500 / 1,500", "Researching Fusion Drive{}35%".format(DOT), "L2: tier 2, 1,800 hit points",
                      "L2 needs {}".format(missing)]
    c["selection_src"] = "Describe Hud.cpp:2566-2572, 2622-2627, 2662-2687"
    c["selectionHealth"] = 1.0
    c["status"] = [{"runs": [{"text": "Fusion Drive"}], "bar": {"share": 0.35, "figure": "", "under": True}, "action": "OpenResearch",
                    "src": "StatusLinesOf Hud.cpp:303-311"}]
    c["buttons"] = [{"id": "Research", "label": "Research", "action": "OpenResearch", "key": "R", "src": "Hud.cpp:2640-2643; KeyBindings.h:29"},
                    {"id": "Upgrade", "label": "Upgrade to L2{}1:00|400".format(DOT), "action": "Upgrade", "enabled": False, "note": "NEEDS RESEARCH",
                     "src": "Hud.cpp:2688-2696"}]
    lay_hud(out, c)


@state("hud", "derelict-hint", "The pointer over a derelict: the hint says what it holds",
       notes=["Hint text from Hud::DescribeDerelict (Hud.cpp:2135-2145), set in GameClient.cpp:742-749.",
              "The derelict is seed 342's flank derelict in B1 (Map.json derelicts: 300 Ore, Small hull, radius 20), one of the two pairs that name a "
              "topic, Large Hull (Map.cpp:495-507). The player's two Constructors salvage it, so it is in sight (Simulation.cpp:289-304), and the "
              "camera looks at it.", EARLY_NOTE])
def s_hud_derelict(out):
    d = derelict_in("B1")
    assert d["ore"] == 300 and d["topic"] == 6
    constructors = [{"kind": "Ship", "pos": (d["pos"][0] + 32, d["pos"][1] + 12), "radius": 10, "side": "Own"},
                    {"kind": "Ship", "pos": (d["pos"][0] + 26, d["pos"][1] - 24), "radius": 10, "side": "Own"}]
    c = early_content(1500, view=d["pos"], constructors=False, derelicts=["B1"], extra=constructors)
    c["hint"] = "Derelict: {} Ore, and part of {}'s research time{}salvaged 40%".format(thousands(d["ore"]), find(TUNING["research"], d["topic"])["name"], DOT)
    c["hint_src"] = "DescribeDerelict Hud.cpp:2137-2143"
    lay_hud(out, c)


@state("hud", "server-silent", "A world on a dedicated server whose snapshots stopped 5 s ago",
       notes=["A world has no tickets (ADR-084), so the territory panel has the Nodes row only; nodeCap still applies (Command Station L2: 7).",
              "Address sample in Hud::ServerName's form (Hud.cpp:2221-2224).", WORLD_NOTE, SEED_STATE_NOTE])
def s_hud_silent(out):
    sectors = sectors_of(own=["A1", "B1", "A2", "B2", "C1"], enemy=["E5", "D5", "E4", "D4"])
    c = content_of(3120, sectors, node_cap(2), rigs=OWN_HOME_ORE + [ore_in("B1", 1)], server=SERVER, silent=5, derelicts=["B1", "C1"])
    lay_hud(out, c)


def victory_content():
    """47:12 in: the player cleared A4's camp and holds A4, and the enemy's tickets ran out (a domination ends at 0, Simulation.cpp:3778-3804)."""
    sectors = sectors_of(own=["A1", "A2", "B1", "C1", "B2", "C2", "A3", "B3", "A4"], enemy=["E5", "D5", "E4", "D4", "E3"], cleared=["A4"])
    rigs = OWN_HOME_ORE + [ore_in("B1", 0), ore_in("B1", 1), ore_in("C1"), ore_in("B2"), ore_in("C2"), ore_in("A3"), ore_in("B3")]
    # The status panel's 3 Shipyards (BUILDING 2, IDLE 1) and Lab; the fleet line's 38 command points are a Swarm and a Lancer under way and
    # 35 in warships: 12 in B2, where the camera looks; 11 at C2's Relay, which keep C2 unsuppressed against the enemy ships its alert names;
    # and 12 holding A4, the camp they cleared.
    yard = "sample: the status line's Shipyards (BUILDING 2 · WAITING 0 · IDLE 1): "
    ships = [structure_mark("Shipyard", SHIPYARD_AT[0], yard + "Shipyard 01, idle"),
             structure_mark("Shipyard", SHIPYARD_AT[1], yard + "Shipyard 02, building a Swarm"),
             structure_mark("Shipyard", SHIPYARD_AT[2], yard + "Shipyard 03, building a Lancer"),
             structure_mark("ResearchLab", LAB_AT, "sample: the Lab the status panel's 'Lance Capacitors' line implies (Hud.cpp:281-311)")]
    fleet = "sample: one of the warships the fleet line counts (38 / 40, two under way), "
    ships += ship_marks([("Swarm", 4), ("Lancer", 4)], (-2050.0, -1880.0), fleet + "in B2, where the camera looks")
    ships += ship_marks([("Swarm", 3), ("Lancer", 2), ("Brawler", 2)], (-100.0, -1880.0), fleet + "at C2's Relay, which it keeps unsuppressed")
    ships += ship_marks([("Picket", 4), ("Brawler", 4)], (-3800.0, 2160.0), fleet + "holding A4, the camp it cleared")
    ships += ship_marks([("Swarm", 2), ("Brawler", 2)], (120.0, -1860.0), "sample: an enemy warship in C2, which the alert 'Enemy ships in C2' "
                        "names; seen in a sector the player holds unsuppressed", side="Enemy", spacing=40.0, per_row=2)
    # The home rigs have mined their asteroids dry (7,500 Ore at 3.5 a second runs out in 35:43) and earn a fifth (exhaustedYieldPercent);
    # Improved Extraction is researched, since the Lab's L2, which tier 2 needs, requires it.
    c = content_of(2350, sectors, node_cap(4), (386, 0), rigs=rigs, dry=OWN_HOME_ORE, extraction=True, view=(-2000, -2000), derelicts=["C2"],
                   extra=[wreck_mark("A4")] + ships)
    assert c["territory"]["enemy_tickets"] == 0 and c["territory"]["own_nodes"] > c["territory"]["enemy_nodes"]
    c["building"] = ["Swarm", "Lancer"]
    c["alerts"] = [("Enemy ships in C2", node_of("C2"))]
    c["status"] = [{"runs": [{"text": "Lance Capacitors"}], "bar": {"share": 0.30, "figure": "+1", "under": True}, "action": "OpenResearch",
                    "src": "StatusLinesOf Hud.cpp:303-311"},
                   {"runs": [{"text": "Shipyards{}BUILDING 2{}WAITING 0{}".format(DOT, DOT, DOT), "sample": True}, {"text": "IDLE 1", "chip": True}],
                    "action": "OpenProduction", "src": "StatusLinesOf Hud.cpp:403-410"},
                   {"runs": [{"text": "Fleet", "sample": False}], "bar": {"share": 38 / 40.0, "figure": "38 / {}".format(fleet_cap(4)), "under": False},
                    "action": "OpenProduction", "src": "StatusLinesOf Hud.cpp:373-380, 411-412"}]
    c["outcome"] = ("Victory", "By domination. Match length 47:12")
    return c


@state("hud", "end-victory", "The match's end: Victory by domination, the world running on behind it",
       notes=["DescribeOutcome Hud.cpp:2199-2218. A domination ends when a player's tickets reach 0 (Simulation.cpp:3796-3804): the enemy's are 0, and the "
              "drain row, still computed, reads 'Enemy -80 a minute · out in 0:00' as plain text (Hud.cpp:2506-2526).",
              "Later in the match than the busy state: the player cleared A4's camp (its wreck, 600 Ore, lies on A4's node) and holds 9 sectors to the enemy's 5; "
              "the 6 other outposts still stand. The player's three home asteroids are dry (DRY_COLOR).", SEED_STATE_NOTE])
def s_hud_victory(out):
    lay_hud(out, victory_content(), hovered="BackToMenu")


def fallen_content():
    """A world in which the player's Command Station fell with no finished Shipyard (Simulation.cpp:3684-3708): it holds nothing and owns nothing."""
    sectors = sectors_of(enemy=["E5", "D5", "E4", "D4", "E3", "C5", "C4", "D3", "D2"])
    c = content_of(1000, sectors, node_cap(1), server=SERVER, station=False, constructors=False, remembered=["C1"])
    assert c["territory"]["cap"] == 4 and c["income"] == 0
    c["outcome"] = ("Empire fallen", "It restarts at your start in 58:41")
    return c


@state("hud", "end-empire-fallen", "A world: the player's empire fell, and its seat restarts",
       notes=["DescribeOutcome Hud.cpp:2188-2196 (ADR-084 d7).",
              "With no station the player keeps level 1's node cap (NodeCapOf, Simulation.cpp:2912-2920; Snapshot::nodeCap, 1116), so the note reads "
              "'cap 4 of 25' (Hud.cpp:3194-3196).", WORLD_NOTE, SEED_STATE_NOTE])
def s_hud_fallen(out):
    lay_hud(out, fallen_content())


# The windows' early state W: two Shipyards at L1 (01 idle, having built four Swarms; 02 building a Swarm, 62%, two more queued) and a Lab
# researching Hull Plating, 45%, with Improved Extraction after it. Every window over it shows the same 1,240 Ore as the HUD.
W_ORE = 1240
W_STATUS = [
    {"runs": [{"text": "Hull Plating"}], "bar": {"share": 0.45, "figure": "+1", "under": True}, "action": "OpenResearch",
     "src": "StatusLinesOf Hud.cpp:303-311 (topic name over a bar, '+N' queued)"},
    {"runs": [{"text": "Shipyards{}BUILDING 1{}WAITING 0{}".format(DOT, DOT, DOT), "sample": True}, {"text": "IDLE 1", "chip": True}], "action": "OpenProduction",
     "src": "StatusLinesOf Hud.cpp:403-410 (IDLE a chip: Shipyard 01 is idle and could start a Swarm, 261 Ore, under the cap)"},
    {"runs": [{"text": "Fleet", "sample": False}], "bar": {"share": 5 / 12.0, "figure": "5 / 12", "under": False}, "action": "OpenProduction",
     "src": "StatusLinesOf Hud.cpp:373-380, 411-412 (four Swarms and the one Shipyard 02 builds, Simulation.cpp:2932-2949)"}]
W_NOTE = ("The HUD behind: two finished Shipyards and a Lab, so the status panel shows the Lab's topic, the Shipyards' counts and the fleet "
          "(StatusLinesOf, Hud.cpp:281-414; drawn 3241-3308). " + EARLY_NOTE)


def window_marks():
    """State W on the minimap: Shipyards 01 and 02, the Lab, and the four Swarms Shipyard 01 built, by it (Simulation.cpp:2752-2759)."""
    yard = "sample: the status line's Shipyards (BUILDING 1 · WAITING 0 · IDLE 1): "
    marks = [structure_mark("Shipyard", SHIPYARD_AT[0], yard + "Shipyard 01, idle at L1, having built four Swarms (designer 'BUILT · 4')"),
             structure_mark("Shipyard", SHIPYARD_AT[1], yard + "Shipyard 02, L1, building a Swarm, 62% (the production window)"),
             structure_mark("ResearchLab", LAB_AT, "sample: the Lab researching Hull Plating, 45%, as the status panel's first line says")]
    marks += ship_marks([("Swarm", 4)], beside(SHIPYARD_AT[0], "Shipyard", 60.0), "sample: one of the four Swarms the fleet line counts "
                        "(5 / 12, Shipyard 02's Swarm included), by Shipyard 01, which built them", spacing=30.0, per_row=2)
    return marks


def window_state():
    c = early_content(W_ORE, extra=window_marks())
    c["building"] = ["Swarm"]
    c["status"] = W_STATUS
    return c


DESIGNER_CORNER = lambda out: window_corner(out, (out.width / out.scale - MARGIN - DESIGNER_W, MARGIN), DESIGNER_W)


@state("window", "designer-default", "The designer on Shipyard 01, the Swarm loaded (saved), nothing hovered",
       notes=["Default place: top-right corner (Hud.cpp:3655-3659). The designer covers the 'where the match runs' tag (ADR-086 d3).",
              "Height from ExtentOf with Tuning.json's 3 hulls, 3 drives, 5 weapons (two with splash notes) and None + 1 module: 913 units (ADR-070).", W_NOTE])
def s_designer(out):
    lay_hud(out, window_state())
    lay_designer(out, designer_panel(ore=W_ORE), DESIGNER_CORNER(out))


@state("window", "designer-preview", "The designer with the Lance card under the pointer: bars and damage cards preview the change", notes=[W_NOTE])
def s_designer_preview(out):
    lay_hud(out, window_state())
    lay_designer(out, designer_panel(preview=("weapon", 2), ore=W_ORE), DESIGNER_CORNER(out), hovered="Lance")


@state("window", "designer-unsaved-editing", "New picks (Medium + Ion + Lance + Sensor Array), the name typed away to nothing, so invalid; Queue says Needs Shipyard L2",
       notes=["Typing starts from the picks' name, 'Lancer+SA' (Designer::Name, Designer.cpp:122-151; BeginEditing 220-225), and Backspace has emptied it. "
              "Typing stops at 32 characters (Designer.cpp:236), and a name is invalid only when empty or all spaces (IsValidDesignName, Command.h:70-84), "
              "so the field shows '_' in WARNING_COLOR and '0/32', and SAVE and QUEUE are dim.",
              "Medium needs Shipyard L2 (Tuning.json structures[Shipyard].levels[0].hulls), so Queue's detail reads 'Needs Shipyard L2' (Hud.cpp:819-824).", W_NOTE])
def s_designer_editing(out):
    lay_hud(out, window_state())
    lay_designer(out, designer_panel(picks=(2, 1, 2, 1), editing=True, typed="", ore=W_ORE), DESIGNER_CORNER(out))


@state("window", "designer-no-shipyard", "The designer with no finished Shipyard: arrows dim, Queue dim, no slots filled",
       notes=["Over the match's start, with its 1,000 Ore.", SEED_STATE_NOTE])
def s_designer_none(out):
    c = match_start_content()
    lay_hud(out, c)
    lay_designer(out, designer_panel(has_shipyard=False, queued=0, ore=c["ore"]), DESIGNER_CORNER(out))


@state("window", "production-shipyard", "Production on Shipyard 02 (L1): four starting designs, two dim as NEEDS L2, a queue of three", notes=[W_NOTE])
def s_prod_yard(out):
    lay_hud(out, window_state())
    lay_production(out, production_panel_shipyard(ore=W_ORE), window_corner(out, (MARGIN, WINDOWS_TOP), PRODUCTION_W))


@state("window", "production-station", "Production on the Command Station: the Constructor card, the front job waiting for Ore",
       notes=["40 Ore against a Constructor's 60 (Tuning.json constructor.cost), paid when the job starts; one home rig, 3.5 Ore a second.", SEED_STATE_NOTE])
def s_prod_station(out):
    c = content_of(40, sectors_of(own=["A1"], enemy=["E5"]), node_cap(1), START_TICKETS, rigs=OWN_HOME_ORE[:1])
    lay_hud(out, c)
    k = TUNING["constructor"]
    lay_production(out, {"producer": "COMMAND STATION", "canStep": False, "ore": c["ore"],
                         "options": [{"name": "Constructor", "detail": "", "cost": k["cost"], "enabled": False, "time": "{} s".format(tenths(k["buildSeconds"])), "strengths": []}],
                         "queue": [{"name": "Constructor", "front": True, "permille": 0, "waiting": True}, {"name": "Constructor"}]},
                   window_corner(out, (MARGIN, WINDOWS_TOP), PRODUCTION_W))


@state("window", "production-none", "Production with no finished producer, after the empire fell in a world: the hint",
       notes=["NO PRODUCER needs no finished Command Station and no finished Shipyard (ProductionTarget::Producers, ProductionTarget.cpp:12-33). That loses "
              "a skirmish in the same tick (Simulation.cpp:3684-3709: Defeat), and in a world it is the 'Empire fallen' state, drawn here behind the window.",
              WORLD_NOTE, SEED_STATE_NOTE])
def s_prod_none(out):
    c = fallen_content()
    lay_hud(out, c)
    lay_production(out, {"producer": "NO PRODUCER", "canStep": False, "ore": c["ore"], "options": [], "queue": [], "hint": "Build a Shipyard to make warships."},
                   window_corner(out, (MARGIN, WINDOWS_TOP), PRODUCTION_W))


@state("window", "research-page1", "Research: the first page of topics, tier-2 topics needing the Lab's level, a queue of two", notes=[W_NOTE])
def s_research(out):
    lay_hud(out, window_state())
    lay_research(out, research_panel(ore=W_ORE), window_corner(out, (2 * MARGIN + PRODUCTION_W, WINDOWS_TOP), RESEARCH_W))


@state("window", "research-no-lab", "Research with no Research Lab: no topic can be queued", notes=["Over the match's start, with its 1,000 Ore.", SEED_STATE_NOTE])
def s_research_none(out):
    c = match_start_content()
    lay_hud(out, c)
    lay_research(out, research_panel(ore=c["ore"], has_lab=False, queued=()), window_corner(out, (MARGIN, WINDOWS_TOP), RESEARCH_W))


@state("window", "controls", "The Controls window (F1), at its default place in the middle of the screen", notes=[SEED_STATE_NOTE])
def s_controls(out):
    lay_hud(out, match_start_content())
    sw, sh = out.width / out.scale, out.height / out.scale
    lay_controls(out, window_corner(out, ((sw - CONTROLS_W) / 2.0, max(MARGIN, (sh - controls_height()) / 2.0)), CONTROLS_W))


@state("window", "orders-time-of-day", "Orders (O): two orders waiting; a time-of-day attack-move ready to give to 12 ships",
       notes=["Rows for a time-of-day trigger and a sector action: WHEN, HOUR, MINUTE, DO, WHERE, UNLESS (Hud.cpp:2248-2274). Form defaults 02:00 attack-move "
              "(OrderForm.h:103-108).", "Over the busy state (hud-busy-warships)."] + BUSY_NOTES[1:])
def s_orders(out):
    lay_hud(out, busy_content())
    panel = {"scheduled": ["At 02:00, attack-move to C3{}12 ships".format(DOT), "When enemies enter B2, hold B2, unless over 8 enemy CP{}6 ships".format(DOT)],
             "more": 0,
             "rows": [("WHEN", "At a time of day"), ("HOUR", "02"), ("MINUTE", "00"), ("DO", "Attack-move"), ("WHERE", "C3"), ("UNLESS", "Never")],
             "fires": "Fires at 02:00 on your clock, in 3:12:00",
             "give": {"label": "Give to 12 ships|", "action": "GiveOrder", "enabled": True, "sample": True}}
    lay_orders(out, panel, window_corner(out, (out.width / out.scale - MARGIN - ORDERS_W, WINDOWS_TOP), ORDERS_W), hovered="GiveOrder")


def stronghold_platforms(sector):
    """A pirate outpost's Defence Platforms, in a row through its node across the line to the map's center, 140 m apart (Simulation.cpp:833-845)."""
    outpost = next(o for o in TUNING["pirates"]["outposts"] if o["name"] == OUTPOST_OF[sector])
    nx, nz = node_of(sector)
    length = math.hypot(nx, nz)
    toward = (-nx / length, -nz / length) if length > 0 else (1.0, 0.0)
    across = (-toward[1], toward[0])
    n = outpost["defensePlatforms"]
    return [(nx + across[0] * (i - (n - 1) / 2.0) * 140.0, nz + across[1] * (i - (n - 1) / 2.0) * 140.0) for i in range(n)]


@state("window", "orders-event-disabled", "Orders: an event trigger with a target, nothing selected that can take it (Give the order dim, SELECT WARSHIPS)",
       notes=["Rows for an event trigger and a target action: WHEN, WATCHING, DO, WHERE, TARGET, UNLESS: 6 of the 7 kept (ORDER_ROWS, Hud.h:528).",
              "TARGET lists the other players' entities the player sees or remembers in the action's sector (OrderForm::Targets, OrderForm.cpp:144-171). "
              "C3 is seed 342's stronghold: a scout saw its two Defence Platforms, which the player remembers (crosses on the minimap), so the first "
              "of them is 'Pirate Defence Platform (1 of 2)' (OrderForm.cpp:268-289); its ships are not remembered.", W_NOTE])
def s_orders_event(out):
    c = window_state()
    platform = next(s for s in TUNING["structures"] if s["kind"] == "DefensePlatform")
    c["marks"].extend({"kind": "Structure", "pos": p, "radius": platform["footprintRadiusMeters"], "side": "Pirate", "remembered": True}
                      for p in stronghold_platforms("C3"))
    lay_hud(out, c)
    panel = {"scheduled": [], "more": 0,
             "rows": [("WHEN", "Enemies enter a sector"), ("WATCHING", "A1"), ("DO", "Attack"), ("WHERE", "C3"), ("TARGET", "Pirate Defence Platform (1 of 2)"),
                      ("UNLESS", "Over 8 enemy CP there")],
             "fires": "Fires once, the first time it happens",
             "give": {"label": "Give the order|", "action": "GiveOrder", "enabled": False, "note": "SELECT WARSHIPS"}}
    lay_orders(out, panel, window_corner(out, (out.width / out.scale - MARGIN - ORDERS_W, WINDOWS_TOP), ORDERS_W))


@state("window", "away", "While you were away: opened by itself with the first snapshot after the seat is taken again (GameClient.cpp:539-544)",
       notes=["Only a world's seat has a deputy (OutpostServer/Main.cpp:193-199; InProcessServer.cpp:512-533), so the report comes in a world: the HUD "
              "behind is a world's, holding C2 and B3 and no longer A3, as the lines say.",
              "A sector is lost when its Relay is (Simulation.cpp:2807-2837, 2896-2900), which is a structure lost (Snapshot.h:492-494): 'Lost 9 ships and 1 structure'.",
              WORLD_NOTE, SEED_STATE_NOTE])
def s_away(out):
    sectors = sectors_of(own=["A1", "B1", "A2", "B2", "C2", "B3"], enemy=["E5", "D5", "E4", "D4", "E3", "C4"])
    # The status line's one Shipyard, at L1, building a Swarm; the fleet line's 18 command points are that Swarm and 17 Small-hull warships,
    # holding the two sectors the report says were gained.
    marks = [structure_mark("Shipyard", SHIPYARD_AT[0], "sample: the status line's one Shipyard (BUILDING 1 · WAITING 0 · IDLE 0), L1, "
                            "building a Swarm")]
    fleet = "sample: one of the 17 Small-hull warships the fleet line counts (18 / 20, a Swarm under way), "
    marks += ship_marks([("Swarm", 9)], (-2150.0, 100.0), fleet + "holding B3", per_row=3)
    marks += ship_marks([("Swarm", 4), ("Picket", 4)], (0.0, -1880.0), fleet + "holding C2")
    c = content_of(2860, sectors, node_cap(2), rigs=OWN_HOME_ORE + [ore_in("B1", 1), ore_in("B2"), ore_in("C2")], server=SERVER, derelicts=["B1", "C2"],
                   extra=marks)
    c["building"] = ["Swarm"]
    c["status"] = [{"runs": [{"text": "Shipyards{}BUILDING 1{}WAITING 0{}IDLE 0".format(DOT, DOT, DOT)}], "action": "OpenProduction",
                    "src": "StatusLinesOf Hud.cpp:403-410"},
                   {"runs": [{"text": "Fleet", "sample": False}], "bar": {"share": 18 / 20.0, "figure": "18 / {}".format(fleet_cap(2)), "under": False},
                    "action": "OpenProduction", "src": "StatusLinesOf Hud.cpp:373-380, 411-412"}]
    lay_hud(out, c)
    lines = ["Built 14 ships and 3 structures", "Lost 9 ships and 1 structure", "Sectors gained: C2, B3", "Sectors lost: A3",
             "Order fired: attack-move to C3", "Order held back: hold B2"]
    h = away_height(len(lines))
    sw, sh = out.width / out.scale, out.height / out.scale
    lay_away(out, {"since": "Away for 7:42:10", "lines": lines}, window_corner(out, ((sw - AWAY_W) / 2.0, max(MARGIN, (sh - h) / 2.0)), AWAY_W))


@state("window", "three-windows-default", "Production, research and the designer open together, each at its default place (ADR-062 d6: side by side at 100%)",
       notes=["Back to front in the order opened: production (slot 0), research (slot 1), designer (Hud.cpp:3634-3687; WindowManager.cpp:99-114).",
              "16 + 478 + 16 + 560 + 16 + 818 + 16 = 1920 (ADR-062 d6).",
              "Over the busy state: the windows show its 2,350 Ore, its research (Hull Plating done; Large Hull 62% at the front, then Improved Extraction and "
              "Mass Driver Calibration) and Hull Plating's hull hit points in the designer (253, 575 and 1,380 HP).", "Shipyard 01 is the idle one, "
              "having built 14 ships; Shipyard 02 builds the Swarm."] + BUSY_NOTES[1:])
def s_three(out):
    c = busy_content()
    lay_hud(out, c)
    lay_production(out, production_panel_shipyard(ore=c["ore"]), window_corner(out, (MARGIN, WINDOWS_TOP), PRODUCTION_W))
    lay_research(out, research_panel(ore=c["ore"], researched=BUSY_RESEARCHED, queued=BUSY_QUEUE, front_permille=620),
                 window_corner(out, (2 * MARGIN + PRODUCTION_W, WINDOWS_TOP), RESEARCH_W))
    lay_designer(out, designer_panel(ore=c["ore"], built=14, hull_factor=HULL_PLATING_FACTOR), DESIGNER_CORNER(out))


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

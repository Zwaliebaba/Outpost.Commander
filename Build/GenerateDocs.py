#!/usr/bin/env python3
"""The generated documents of AGENTS.md §8.

A fact has one home, and a document that only shows a fact from its home is written from it here rather than by hand,
so that the two cannot drift apart:

  Docs/Reference/Catalogue.md      the tuning data as tables: OutpostCommander/Assets/Tuning.json and Map.json, the
                                   AI's settings with a column for each difficulty (OpponentEasy.json, Opponent.json
                                   and OpponentHard.json), Matchups.json and Camera.json
  Docs/Reference/Controls.md       every KEY_ constant in GameApp/KeyBindings.h: its key as the Controls window names
                                   it, and the comment above it
  Docs/Reference/Colors.md         every constexpr whose name ends in _COLOR, in the .h and .cpp of GameApp and
                                   NeuronClient: its file, its linear RGBA and its sRGB hex
  Docs/ADR/README.md               every ADR: its number, its title linked to its file, and the keys of its Owns: line
  Docs/Reference/OpenDecisions.md  every gate-table row whose Status is open, in every .md under Docs/ but the documents
                                   generated here, grouped by the document that holds it

It is strict. A file, a constant or a key it cannot read fails the run and names the file, rather than being left out
of a document that would then look complete: JSON that does not parse or repeats a key, a value it cannot write in a
table, an identifier the data refers by that names nothing (a design's hull, a topic's prerequisite), a difficulty
whose keys are not Normal's, a KEY_ constant whose key it cannot read or that has no comment, a _COLOR constant it
cannot read, an ADR without a Status: or an Owns: line, an Owns: key another ADR owns too, a ## Decision that is not a
numbered list, and a gate table with another header or a Status that is neither "open" nor "decided YYYY-MM-DD".

The Catalogue gives every top-level key of every file an anchor of exactly that key, before its section or on its row,
so that a link such as Reference/Catalogue.md#hulls or Reference/Catalogue.md#attackGroupShips reaches it; two anchors
with one id fail the run. A unit comes from the key's name (Meters, Seconds, Degrees, Percent, Pixels, Hz, PerSecond),
and an identifier the data refers by is written as the name it refers to.

The output is deterministic: sections and rows in the order their source gives them, and files, ADRs and documents
sorted by path where nothing else gives an order. Every document is UTF-8 with LF line endings and a final newline,
and its first line says it is generated, by this script, and how to regenerate it. Nothing here reads git, so a file
that is not yet added is read like any other.

Usage:
  python Build/GenerateDocs.py              write the generated documents
  python Build/GenerateDocs.py --check      regenerate them in memory and fail, naming the file, on a byte that differs
  python Build/GenerateDocs.py --self-test  break a copy of each input once per check and show that each check fails

Exit status: 0 when the documents are written or current, 1 when an input cannot be read, a document is not current,
or a self-test case failed.
"""

import argparse
import json
import posixpath
import re
import shutil
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from CheckProjectFiles import VENDORED_FILES, strip_cpp  # noqa: E402

REPO_ROOT = Path(__file__).resolve().parents[1]
GENERATOR = "Build/GenerateDocs.py"
COMMAND = "python Build/GenerateDocs.py"

CATALOGUE = "Docs/Reference/Catalogue.md"
CONTROLS = "Docs/Reference/Controls.md"
COLORS = "Docs/Reference/Colors.md"
ADR_INDEX = "Docs/ADR/README.md"
OPEN_DECISIONS = "Docs/Reference/OpenDecisions.md"
OUTPUTS = (CATALOGUE, CONTROLS, COLORS, ADR_INDEX, OPEN_DECISIONS)

ASSETS = "OutpostCommander/Assets"
TUNING = f"{ASSETS}/Tuning.json"
MAP = f"{ASSETS}/Map.json"
DIFFICULTIES = (("Easy", f"{ASSETS}/OpponentEasy.json"), ("Normal", f"{ASSETS}/Opponent.json"),
                ("Hard", f"{ASSETS}/OpponentHard.json"))
MATCHUPS = f"{ASSETS}/Matchups.json"
CAMERA = f"{ASSETS}/Camera.json"

# The sections each file must have. A file may have more, and they are written too; one of these missing is a section
# the Catalogue would silently lack.
TUNING_KEYS = ("rules", "sight", "territory", "hulls", "drives", "weapons", "modules", "constructor",
               "structureWeapons", "structures", "research", "startingDesigns", "pirates", "salvage", "shipRepair")
MAP_KEYS = ("sizeMeters", "starts", "sectors", "sectorKinds", "outposts", "derelicts")

# A unit is a run of words in a key's name. Longest first, so that MetersPerSecond is m/s and not m.
UNITS = (
  (("view", "widths", "per", "second"), "view widths/s"),
  (("meters", "per", "second"), "m/s"),
  (("degrees", "per", "second"), "°/s"),
  (("percent", "per", "second"), "%/s"),
  (("ore", "per", "second"), "Ore/s"),
  (("per", "second"), "/s"),
  (("meters",), "m"),
  (("seconds",), "s"),
  (("degrees",), "°"),
  (("percent",), "%"),
  (("pixels",), "px"),
  (("hz",), "Hz"),
)
UNITS_WITHOUT_A_SPACE = {"°/s", "%/s", "°", "%"}

# The identifiers the data refers by, and the list each names an entry of. A list of them is written as their names.
ID_REFERENCES = {"hull": "hulls", "drive": "drives", "weapon": "weapons", "module": "modules",
                 "structureWeapon": "structureWeapons", "unlockHull": "hulls", "unlockDrive": "drives",
                 "unlockWeapon": "weapons"}
ID_LIST_REFERENCES = {"hulls": "hulls", "requires": "research", "researchOrder": "research", "adjacent": "sectors"}
DESIGN_PARTS = {"hull", "drive", "weapon"}
# A list of objects under one of these keys is a table of its own, after its owner's: a structure's levels.
SUBTABLE_KEYS = {"levels"}

KEY_BINDINGS = "GameApp/KeyBindings.h"
KEY_CONSTANT = re.compile(r"(?:inline\s+)?(?:static\s+)?constexpr\s+[\w:]+\s+(KEY_[A-Z0-9_]+)\s*=\s*([^;]+?)\s*;"
                          r"(?:\s*//\s*(.*))?")
KEY_NAME_CASE = re.compile(r'case\s+(VK_\w+)\s*:\s*return\s+"([^"]*)"\s*;')

COLOR_DIRECTORIES = ("GameApp", "NeuronClient")
CONSTEXPR = re.compile(r"\bconstexpr\b")
CONSTEXPR_NAME = re.compile(r"constexpr\b[^;{}()=\[]*?\b([A-Za-z_]\w*)\s*[{=(\[;]")
COLOR_DECLARATION = re.compile(r"constexpr\s+(?P<type>(?:\w+\s*::\s*)*\w+)\s+(?P<name>[A-Za-z_]\w*_COLOR)\s*"
                               r"(?:\{(?P<braced>[^{};]*)\}"
                               r"|=\s*(?:(?P<cast>(?:\w+\s*::\s*)*\w+)\s*)?\{(?P<assigned>[^{};]*)\}"
                               r"|=\s*(?P<alias>[A-Za-z_]\w*_COLOR))\s*;")
FLOAT_LITERAL = re.compile(r"[-+]?(?:\d+\.\d*|\.\d+|\d+)(?:[eE][-+]?\d+)?[fF]?")
COLOR_TYPES = {"XMFLOAT3": 3, "XMFLOAT4": 4}

ADR_DIRECTORY = "Docs/ADR"
ADR_FILE = re.compile(r"ADR-(\d{3})-[a-z0-9]+(?:-[a-z0-9]+)*\.md")
ADR_TITLE = re.compile(r"# ADR-(\d{3}) — (\S.*)")
ADR_STATUS = re.compile(r"Status: \*\*[a-z]+\*\* · \d{4}-\d{2}-\d{2}")
ADR_OWNS = re.compile(r"Owns: (.+)")
OWNS_KEY = re.compile(r"[a-z0-9]+(?:-[a-z0-9]+)*")
NUMBERED = re.compile(r"(\d+)\. \S")

GATE_DIRECTORIES = ("Docs",)
GATE_HEADER = "| Gate | Decision | Proposed in | Blocks | Status |"
GATE_STATUS = re.compile(r"open|decided \d{4}-\d{2}-\d{2}")
TABLE_DELIMITER = re.compile(r"\|?\s*:?-{3,}:?\s*(?:\|\s*:?-{3,}:?\s*)*\|?")
FENCE = re.compile(r"[ \t]*(`{3,}|~{3,})")
CELL_SPLIT = re.compile(r"(?<!\\)\|")
INLINE_LINK = re.compile(r"(\]\(\s*)(<[^>]*>|[^)\s]+)")
URL_SCHEME = re.compile(r"[A-Za-z][A-Za-z0-9+.-]*:")


class GenerateError(Exception):
  pass


def fail(_where, _message):
  raise GenerateError(f"{_where}: {_message}")


# ── Reading. ────────────────────────────────────────────────────────────────────────────────────────────────────


def read_text(_root, _relative):
  try:
    return (_root / _relative).read_text(encoding="utf-8-sig").replace("\r\n", "\n")
  except FileNotFoundError:
    fail(_relative, "does not exist")
  except (OSError, UnicodeDecodeError) as error:
    fail(_relative, f"cannot be read: {error}")


def read_json(_root, _relative):
  def unique(_pairs):
    result = {}
    for key, value in _pairs:
      if key in result:
        fail(_relative, f"repeats the key \"{key}\" in one object")
      result[key] = value
    return result

  def no_constant(_name):
    fail(_relative, f"holds {_name}, which is not a JSON number")

  try:
    return json.loads(read_text(_root, _relative), object_pairs_hook=unique, parse_constant=no_constant)
  except json.JSONDecodeError as error:
    fail(f"{_relative}:{error.lineno}", f"is not JSON: {error.msg}")


def camel_words(_key, _where):
  words = re.findall(r"[a-z]+|[A-Z][a-z]*|\d+", _key)
  if "".join(words) != _key:
    fail(_where, f"has the key \"{_key}\", which is not camelCase, so its words and its unit cannot be read")
  return words


def split_unit(_key, _where):
  """The words of a key's name without its unit, and the unit: speedMetersPerSecond is (['speed'], 'm/s')."""
  words = camel_words(_key, _where)
  lowered = [word.lower() for word in words]
  for unit_words, unit in UNITS:
    for start in range(len(lowered) - len(unit_words) + 1):
      if tuple(lowered[start:start + len(unit_words)]) == unit_words:
        return [word.lower() for word in words[:start] + words[start + len(unit_words):]], unit
  return [word.lower() for word in words], ""


def title(_key, _where):
  return " ".join(word.lower() for word in camel_words(_key, _where)).capitalize()


def escape(_text):
  """A string as it can stand in a table cell: a pipe escaped, and a line break a space."""
  return re.sub(r"\r?\n", " ", _text).replace("|", "\\|")


def is_scalar(_value):
  return isinstance(_value, (bool, int, float, str))


def is_inline(_value):
  return is_scalar(_value) or (isinstance(_value, list) and all(is_scalar(item) for item in _value))


def scalar_text(_value):
  if isinstance(_value, bool):
    return "true" if _value else "false"
  if isinstance(_value, (int, float)):
    return repr(_value)
  return escape(_value)


def table(_header, _rows):
  lines = ["| " + " | ".join(_header) + " |", "|" + "---|" * len(_header)]
  lines += ["| " + " | ".join(row) + " |" for row in _rows]
  return lines


def link_from(_output, _target):
  """A relative link from one repository file to another, as Markdown wants it."""
  return posixpath.relpath(_target, posixpath.dirname(_output))


def adr_link(_output, _file_name):
  """A relative link from a generated document to the ADR in _file_name."""
  return link_from(_output, f"{ADR_DIRECTORY}/{_file_name}")


def banner(_source, _change):
  return (f"*Generated by `{GENERATOR}` from {_source}. Do not edit it: change {_change} and run `{COMMAND}`.*")


def document(_lines):
  text = "\n".join(_lines).rstrip("\n") + "\n"
  return re.sub(r"\n{3,}", "\n\n", text)


# ── The Catalogue. ──────────────────────────────────────────────────────────────────────────────────────────────


class Catalogue:
  """Writes the tuning data as tables, resolving every identifier the data refers by to the name it refers to."""

  def __init__(self, _root):
    self.root = _root
    self.names = {}
    self.anchors = {}

  def add_names(self, _table, _items, _where):
    if not isinstance(_items, list):
      fail(_where, "is not a list")
    names = {}
    for index, item in enumerate(_items):
      where = f"{_where}[{index}]"
      if not isinstance(item, dict):
        fail(where, "is not an object")
      identifier, name = item.get("id"), item.get("name")
      if not isinstance(identifier, int) or isinstance(identifier, bool):
        fail(where, "has no whole-number \"id\"")
      if not isinstance(name, str):
        fail(where, "has no \"name\"")
      if identifier in names:
        fail(where, f"repeats the id {identifier}")
      names[identifier] = name
    self.names[_table] = names

  def name(self, _table, _identifier, _where):
    if _table not in self.names:
      fail(_where, f"refers to the {_table}, which this file cannot refer to")
    if not isinstance(_identifier, int) or isinstance(_identifier, bool):
      fail(_where, f"is not the whole-number id of one of the {_table}")
    if _identifier not in self.names[_table]:
      fail(_where, f"names {_identifier}, which none of the {_table} has as its id")
    return escape(self.names[_table][_identifier])

  def anchor(self, _id, _where):
    if _id in self.anchors:
      fail(_where, f"would have the anchor \"{_id}\", which {self.anchors[_id]} has already")
    self.anchors[_id] = _where
    return f"<a id=\"{_id}\"></a>"

  # Values, written in one table cell.

  def value_text(self, _key, _value, _where, _depth=0):
    if is_scalar(_value):
      if _key in ID_REFERENCES:
        return self.name(ID_REFERENCES[_key], _value, _where)
      return scalar_text(_value)
    if isinstance(_value, list):
      if not _value:
        return "none"
      if all(is_scalar(item) for item in _value):
        if _key in ID_LIST_REFERENCES:
          return ", ".join(self.name(ID_LIST_REFERENCES[_key], item, f"{_where}[{index}]")
                           for index, item in enumerate(_value))
        return ", ".join(scalar_text(item) for item in _value)
      if _depth == 0 and all(isinstance(item, dict) for item in _value):
        return "<br>".join(self.object_text(_key, item, f"{_where}[{index}]", 1) for index, item in enumerate(_value))
      if _depth == 0 and _key == "sides" and all(isinstance(side, list) for side in _value):
        return " against ".join(self.side_text(side, f"{_where}[{index}]") for index, side in enumerate(_value))
      fail(_where, "is a list this cannot write in a table cell")
    if isinstance(_value, dict):
      return self.object_text(_key, _value, _where, _depth + 1)
    fail(_where, "is null, which this cannot write")

  def side_text(self, _side, _where):
    groups = []
    for index, group in enumerate(_side):
      if not isinstance(group, dict):
        fail(f"{_where}[{index}]", "is not an object")
      groups.append(self.object_text("side", group, f"{_where}[{index}]", 1))
    return " + ".join(groups) if groups else "none"

  def object_text(self, _key, _value, _where, _depth):
    if _key == "effect":
      return self.effect_text(_value, _where)
    keys = set(_value)
    if DESIGN_PARTS <= keys and keys <= DESIGN_PARTS | {"module", "count"}:
      text = " ".join(self.value_text(part, _value[part], f"{_where}.{part}") for part in ("hull", "drive", "weapon"))
      if "module" in _value:
        text += " with " + self.value_text("module", _value["module"], f"{_where}.module")
      if "count" in _value:
        if not isinstance(_value["count"], int) or isinstance(_value["count"], bool):
          fail(f"{_where}.count", "is not a whole number")
        text = f"{_value['count']} × {text}"
      return text
    if _depth > 2:
      fail(_where, "is nested deeper than this can write in a table cell")
    parts = []
    for key, value in _value.items():
      where = f"{_where}.{key}"
      if isinstance(value, list) and not all(is_scalar(item) for item in value):
        fail(where, "is a list of objects inside an object, which this cannot write in a table cell")
      words, unit = split_unit(key, where)
      text = self.value_text(key, value, where, _depth) if not isinstance(value, dict) else \
        self.object_text(key, value, where, _depth + 1)
      if unit:
        text += unit if unit in UNITS_WITHOUT_A_SPACE else f" {unit}"
      parts.append(" ".join(words + [text]) if words else text)
    return ", ".join(parts)

  def effect_text(self, _effect, _where):
    if not isinstance(_effect, dict):
      fail(_where, "is not an object")
    keys = set(_effect)
    if "upgrade" in keys:
      if not keys <= {"upgrade", "stat", "percent", "weapon", "structureWeapon"} or not {"stat", "percent"} <= keys:
        fail(_where, f"is an upgrade with the keys {sorted(keys)}, which this cannot write")
      percent = _effect["percent"]
      if not isinstance(percent, (int, float)) or isinstance(percent, bool):
        fail(f"{_where}.percent", "is not a number")
      if not isinstance(_effect["upgrade"], str) or not isinstance(_effect["stat"], str):
        fail(_where, "names its upgrade or its stat with something other than a string")
      target = scalar_text(_effect["upgrade"])
      for part in ("weapon", "structureWeapon"):
        if part in _effect:
          target += " " + self.value_text(part, _effect[part], f"{_where}.{part}")
      sign = "+" if percent >= 0 else ""
      return f"{target}: {scalar_text(_effect['stat'])} {sign}{scalar_text(percent)}%"
    if len(keys) == 1 and keys <= {"unlockHull", "unlockDrive", "unlockWeapon"}:
      key = next(iter(keys))
      return f"{key} {self.value_text(key, _effect[key], f'{_where}.{key}')}"
    fail(_where, f"is an effect with the keys {sorted(keys)}, which this cannot write")

  # Tables.

  def column_header(self, _key, _where):
    _, unit = split_unit(_key, _where)
    return f"{_key} ({unit})" if unit else _key

  def key_value_table(self, _items, _where, _anchored):
    """A table of keys and their values. Anchored, each row is a file's top-level key, and _where is the file."""
    rows = []
    for key, value in _items:
      where = f"{_where}, {key}" if _anchored else f"{_where}.{key}"
      _, unit = split_unit(key, where)
      anchor = self.anchor(key, where) if _anchored else ""
      rows.append([f"{anchor}`{key}`", self.value_text(key, value, where), unit])
    return table(["Key", "Value", "Unit"], rows)

  def list_table(self, _key, _items, _where):
    lines = []
    for index, item in enumerate(_items):
      if not isinstance(item, dict):
        fail(f"{_where}[{index}]", "is not an object, in a list of objects")
    columns = []
    for item in _items:
      columns += [key for key in item if key not in columns and key not in SUBTABLE_KEYS]
    numbered = not any("id" in item or "name" in item for item in _items)
    header = (["#"] if numbered else []) + [self.column_header(column, _where) for column in columns]
    rows = []
    for index, item in enumerate(_items):
      row = [str(index + 1)] if numbered else []
      row += [self.value_text(column, item[column], f"{_where}[{index}].{column}") if column in item else "—"
              for column in columns]
      rows.append(row)
    lines += table(header, rows)
    for subtable in sorted(SUBTABLE_KEYS):
      for index, item in enumerate(_items):
        if subtable not in item:
          continue
        where = f"{_where}[{index}].{subtable}"
        levels = item[subtable]
        if not isinstance(levels, list) or not levels or not all(isinstance(level, dict) for level in levels):
          fail(where, "is not a list of objects")
        owner = escape(item["name"]) if isinstance(item.get("name"), str) else f"{title(_key, _where)} {index + 1}"
        lines += ["", f"#### {owner} {subtable}", ""]
        if subtable == "levels":
          lines += ["Level 1 is the structure as it is built, and each row is the level an upgrade takes it to.", ""]
        level_columns = []
        for level in levels:
          level_columns += [key for key in level if key not in level_columns]
        level_rows = []
        for number, level in enumerate(levels):
          if any(not is_inline(value) for value in level.values()):
            fail(f"{where}[{number}]", "holds a value this cannot write in a table cell")
          level_rows.append([str(number + 2)] + [
            self.value_text(column, level[column], f"{where}[{number}].{column}") if column in level else "—"
            for column in level_columns])
        lines += table(["level"] + [self.column_header(column, where) for column in level_columns], level_rows)
    return lines

  def section(self, _key, _value, _where):
    if not isinstance(_value, (list, dict)):
      fail(_where, "is null, which this cannot write")
    lines = [self.anchor(_key, _where), "", f"### {title(_key, _where)}", ""]
    if isinstance(_value, list):
      if not all(isinstance(item, dict) for item in _value):
        fail(_where, "mixes objects with other values")
      return lines + self.list_table(_key, _value, _where)
    tables = {key for key, value in _value.items()
              if isinstance(value, list) and value and all(isinstance(item, dict) for item in value)}
    inline = [(key, value) for key, value in _value.items() if key not in tables]
    if inline:
      lines += self.key_value_table(inline, _where, False)
    for key, value in _value.items():
      if key not in tables:
        continue
      words = " ".join(word.lower() for word in camel_words(key, f"{_where}.{key}"))
      lines += ["", f"#### {title(_key, _where)}: {words}", ""]
      lines += self.list_table(key, value, f"{_where}.{key}")
    return lines

  def file_part(self, _heading, _relative, _data, _required, _introduction):
    if not isinstance(_data, dict):
      fail(_relative, "is not a JSON object")
    for key in _required:
      if key not in _data:
        fail(_relative, f"has no \"{key}\"")
    lines = [f"## {_heading}", "", f"{_introduction} From [`{_relative}`]({link_from(CATALOGUE, _relative)}).", ""]
    inline = [(key, value) for key, value in _data.items() if is_inline(value)]
    if inline:
      lines += self.key_value_table(inline, _relative, True) + [""]
    for key, value in _data.items():
      if not is_inline(value):
        lines += self.section(key, value, f"{_relative}, {key}") + [""]
    return lines

  def opponent_part(self):
    files = [(difficulty, relative, read_json(self.root, relative)) for difficulty, relative in DIFFICULTIES]
    for difficulty, relative, data in files:
      if not isinstance(data, dict) or not data:
        fail(relative, "is not a JSON object with keys")
    normal = next(data for difficulty, _, data in files if difficulty == "Normal")
    for difficulty, relative, data in files:
      missing = [key for key in normal if key not in data]
      extra = [key for key in data if key not in normal]
      if missing or extra:
        fail(relative, f"has not the keys of {DIFFICULTIES[1][1]}: " +
             "; ".join(part for part in (f"lacks {', '.join(missing)}" if missing else "",
                                         f"adds {', '.join(extra)}" if extra else "") if part))
    rows = []
    for key in normal:
      _, unit = split_unit(key, f"{DIFFICULTIES[1][1]}, {key}")
      anchor = self.anchor(key, f"{DIFFICULTIES[1][1]}, {key}")
      cells = []
      for difficulty, relative, data in files:
        text = self.value_text(key, data[key], f"{relative}, {key}")
        if difficulty != "Normal" and not is_scalar(data[key]) and data[key] == normal[key]:
          text = "as Normal"
        cells.append(text)
      rows.append([f"{anchor}`{key}`"] + cells + [unit])
    sources = ", ".join(f"[`{relative}`]({link_from(CATALOGUE, relative)})" for _, relative in DIFFICULTIES)
    difficulty_adr = adr_link(CATALOGUE, "ADR-065-ai-difficulty.md")
    return (["## Opponent", "",
             f"How the AI plays at each difficulty ([ADR-065]({difficulty_adr})), a column "
             f"for each file: {sources}. A list or a design that is the same as Normal's reads \"as Normal\".", ""] +
            table(["Key"] + [difficulty for difficulty, _ in DIFFICULTIES] + ["Unit"], rows) + [""])

  def generate(self):
    tuning = read_json(self.root, TUNING)
    if not isinstance(tuning, dict):
      fail(TUNING, "is not a JSON object")
    for table_key in ("hulls", "drives", "weapons", "modules", "structureWeapons", "research"):
      if table_key not in tuning:
        fail(TUNING, f"has no \"{table_key}\"")
      self.add_names(table_key, tuning[table_key], f"{TUNING}, {table_key}")
    world = read_json(self.root, MAP)
    if not isinstance(world, dict) or "sectors" not in world:
      fail(MAP, "is not a JSON object with \"sectors\"")
    self.add_names("sectors", world["sectors"], f"{MAP}, sectors")

    lines = [banner(f"the data in `{ASSETS}/`", "the data"), "", "# Catalogue", "",
             "The game's tuning data, as tables. The data files are each number's one home, and this shows them. Every "
             "top-level key of a file has an anchor of exactly its name, such as `Catalogue.md#hulls` or "
             "`Catalogue.md#attackGroupShips`, which is how a document links a number. A unit comes from the key's "
             "name: Meters is m, Seconds s, Degrees °, Percent %, Pixels px, Hz is Hz, and PerSecond is /s. An "
             "identifier the data refers by, such as a design's hull or a topic's prerequisites, is written as the "
             "name of what it refers to. Costs are in Ore.", ""]
    lines += self.file_part("Tuning", TUNING, tuning, TUNING_KEYS,
                            "The match's rules, every component, structure and research topic, the pirates and the "
                            f"neutral sites ([ADR-008]({adr_link(CATALOGUE, 'ADR-008-tuning-data.md')})).")
    lines += self.file_part("Map", MAP, world, MAP_KEYS, "The map a match is played on.")
    lines += self.opponent_part()
    lines += self.file_part("Matchups", MATCHUPS, read_json(self.root, MATCHUPS), ("matchups",),
                            f"The battle matchups ([ADR-083]({adr_link(CATALOGUE, 'ADR-083-battle-matchups.md')})).")
    lines += self.file_part("Camera", CAMERA, read_json(self.root, CAMERA), (),
                            f"How the camera moves ([ADR-012]({adr_link(CATALOGUE, 'ADR-012-rts-camera.md')})).")
    return document(lines)


def catalogue(_root):
  return Catalogue(_root).generate()


# ── The controls. ───────────────────────────────────────────────────────────────────────────────────────────────


def controls(_root):
  text = read_text(_root, KEY_BINDINGS)
  names = {}
  for vk, name in KEY_NAME_CASE.findall(text):
    if vk in names:
      fail(KEY_BINDINGS, f"names {vk} twice in KeyName")
    names[vk] = name
  if not names:
    fail(KEY_BINDINGS, "has no KeyName switch to name its keys by")

  groups = []
  comment = []
  previous_was_comment = False
  for number, line in enumerate(text.split("\n"), 1):
    where = f"{KEY_BINDINGS}:{number}"
    stripped = line.strip()
    if stripped.startswith("//"):
      if not previous_was_comment:
        comment = []
      comment.append(stripped.lstrip("/").strip())
      previous_was_comment = True
      continue
    previous_was_comment = False
    if "KEY_" in stripped and re.search(r"\bconstexpr\b", stripped):
      match = KEY_CONSTANT.fullmatch(stripped)
      if match is None:
        fail(where, "is a KEY_ constant this cannot read")
      constant, value, trailing = match.groups()
      if re.fullmatch(r"'[^'\\]'", value):
        key = value[1]
      elif value in names:
        key = names[value]
      elif value.startswith("VK_"):
        fail(where, f"is {value}, which KeyName does not name")
      else:
        fail(where, f"is {constant} = {value}, a key this cannot read")
      said = trailing.strip() if trailing else " ".join(part for part in comment if part)
      if not said:
        fail(where, f"has no comment to say what {constant} is for")
      if groups and groups[-1][0] == said:
        groups[-1][1].append((key, constant))
      else:
        groups.append((said, [(key, constant)]))
      continue
    comment = []
  if not groups:
    fail(KEY_BINDINGS, "has no KEY_ constant")

  lines = [banner(f"`{KEY_BINDINGS}`", "the header"), "", "# Controls", "",
           f"Every key the game reads, from its constant in [`{KEY_BINDINGS}`]({link_from(CONTROLS, KEY_BINDINGS)}): "
           "the key as the Controls window names it, the constant, and what the header says it is for. The Controls "
           "window also lists the mouse's actions, which have no constant.", ""]
  for said, keys in groups:
    lines += [escape(said), ""] + table(["Key", "Constant"], [[f"`{escape(key)}`", f"`{constant}`"]
                                                                for key, constant in keys]) + [""]
  return document(lines)


# ── The colors. ─────────────────────────────────────────────────────────────────────────────────────────────────


def srgb_byte(_linear):
  value = min(max(_linear, 0.0), 1.0)
  encoded = 12.92 * value if value <= 0.0031308 else 1.055 * value ** (1.0 / 2.4) - 0.055
  return int(encoded * 255.0 + 0.5)


def color_sources(_root):
  sources = []
  for directory in COLOR_DIRECTORIES:
    if not (_root / directory).is_dir():
      fail(directory, "does not exist")
    for path in sorted((_root / directory).iterdir()):
      relative = f"{directory}/{path.name}"
      if path.is_file() and path.suffix in (".h", ".cpp") and relative not in VENDORED_FILES:
        sources.append(relative)
  return sources


def colors(_root):
  found = []
  for relative in color_sources(_root):
    text = read_text(_root, relative)
    code = strip_cpp(text, True)
    for start in CONSTEXPR.finditer(code):
      head = CONSTEXPR_NAME.match(code, start.start())
      if head is None or not head.group(1).endswith("_COLOR"):
        continue
      where = f"{relative}:{code.count(chr(10), 0, start.start()) + 1}"
      match = COLOR_DECLARATION.match(code, start.start())
      if match is None:
        fail(where, f"declares {head.group(1)} in a form this cannot read as a color")
      type_name = re.sub(r"\s", "", match.group("type")).split("::")[-1]
      if type_name not in COLOR_TYPES:
        fail(where, f"declares {match.group('name')} as the type {type_name}, which this does not read as a color")
      literal = match.group("braced") if match.group("braced") is not None else match.group("assigned")
      if match.group("cast") is not None and re.sub(r"\s", "", match.group("cast")).split("::")[-1] != type_name:
        fail(where, f"declares {match.group('name')} from a {match.group('cast')}, not a {type_name}")
      components = None
      if literal is not None:
        parts = [part.strip() for part in literal.split(",")]
        if len(parts) != COLOR_TYPES[type_name] or not all(FLOAT_LITERAL.fullmatch(part) for part in parts):
          fail(where, f"gives {match.group('name')} {{{literal.strip()}}}, "
                      f"not {COLOR_TYPES[type_name]} number literals")
        components = [float(part.rstrip("fF")) for part in parts]
      found.append({"name": match.group("name"), "file": relative, "where": where, "type": type_name,
                    "components": components, "alias": match.group("alias")})
  if not found:
    fail(" and ".join(COLOR_DIRECTORIES), "hold no _COLOR constant")

  for color in found:
    if color["alias"] is None:
      continue
    same_file = [other for other in found if other["name"] == color["alias"] and other["file"] == color["file"]]
    anywhere = [other for other in found if other["name"] == color["alias"]]
    targets = same_file or anywhere
    if len(targets) != 1:
      fail(color["where"], f"makes {color['name']} {color['alias']}, which is " +
           ("not a _COLOR constant this reads" if not targets else "defined in more than one file"))
    target = targets[0]
    if target["alias"] is not None:
      fail(color["where"], f"makes {color['name']} {color['alias']}, which is itself an alias")
    if target["type"] != color["type"]:
      fail(color["where"], f"makes the {color['type']} {color['name']} the {target['type']} {color['alias']}")
    color["components"] = target["components"]

  rows = []
  for color in found:
    rgba = color["components"]
    name = f"`{color['name']}`" + (f" = `{color['alias']}`" if color["alias"] else "")
    hex_color = "#" + "".join(f"{srgb_byte(component):02X}" for component in rgba[:3])
    rows.append([name, f"[{color['file']}]({link_from(COLORS, color['file'])})",
                 ", ".join(repr(component) for component in rgba), f"`{hex_color}`"])
  lines = [banner("the `_COLOR` constants in `GameApp/` and `NeuronClient/`", "the constants"), "", "# Colors", "",
           "Every color constant, a `constexpr` whose name ends in `_COLOR`, in the `.h` and `.cpp` files of "
           "`GameApp/` and `NeuronClient/`: its file, its linear RGBA as the code states it, and the sRGB hex it shows "
           "as. The render target is sRGB (`RENDER_TARGET_FORMAT` in `NeuronClient/Renderer.h`), so the linear color "
           "is encoded on its way to the screen; an alpha below 1 is blended over what is behind it, and the hex is "
           "the color alone. An alias names the constant it equals. Files are sorted by path, and each file's "
           "constants are in its own order.", ""]
  lines += table(["Name", "File", "Linear RGBA", "sRGB"], rows)
  return document(lines)


# ── The ADR index. ──────────────────────────────────────────────────────────────────────────────────────────────


def fence_after(_line, _fence):
  """The fence a Markdown line leaves open: _fence is the one open before it, or None. A fence closes on a line of
  nothing but the same character, at least as many times."""
  opening = FENCE.match(_line)
  if _fence is None:
    return opening.group(1) if opening else None
  if opening and opening.group(1)[0] == _fence[0] and len(opening.group(1)) >= len(_fence) and \
      _line.strip() == opening.group(1):
    return None
  return _fence


def check_decisions(_relative, _lines):
  starts = [index for index, line in enumerate(_lines) if line == "## Decision"]
  if len(starts) != 1:
    fail(_relative, "has no \"## Decision\" section" if not starts else "has two \"## Decision\" sections")
  expected = 1
  fence = None
  for index in range(starts[0] + 1, len(_lines)):
    line = _lines[index]
    where = f"{_relative}:{index + 1}"
    if line.startswith("## "):
      break
    was_open = fence is not None
    fence = fence_after(line, fence)
    if was_open or fence is not None:
      if not was_open and not line.startswith((" ", "\t")):
        fail(where, "opens a code block outside a numbered decision, so ## Decision is not one numbered list")
      continue
    if not line.strip() or line.startswith((" ", "\t")):
      continue
    item = NUMBERED.match(line)
    if item is None:
      fail(where, "is not a numbered decision or indented under one, so ## Decision is not one numbered list")
    if int(item.group(1)) != expected:
      fail(where, f"numbers a decision {item.group(1)} where {expected} comes next")
    expected += 1
  if expected == 1:
    fail(_relative, "has no numbered decision under ## Decision")


def adr_index(_root):
  directory = _root / ADR_DIRECTORY
  if not directory.is_dir():
    fail(ADR_DIRECTORY, "does not exist")
  adrs = []
  owners = {}
  numbers = {}
  for path in sorted(directory.glob("ADR-*.md")):
    relative = f"{ADR_DIRECTORY}/{path.name}"
    name = ADR_FILE.fullmatch(path.name)
    if name is None:
      fail(relative, "is not named ADR-NNN-<lowercase-slug>.md")
    number = name.group(1)
    if number in numbers:
      fail(relative, f"has the number of {numbers[number]}")
    numbers[number] = relative
    lines = read_text(_root, relative).split("\n")
    heading = ADR_TITLE.fullmatch(lines[0].rstrip())
    if heading is None:
      fail(f"{relative}:1", "is not a title \"# ADR-NNN — Title\"")
    if heading.group(1) != number:
      fail(f"{relative}:1", f"is titled ADR-{heading.group(1)} in the file of ADR-{number}")
    end = next((index for index, line in enumerate(lines) if line.startswith("## ")), len(lines))
    status = [index for index in range(end) if lines[index].startswith("Status:")]
    owns = [index for index in range(end) if lines[index].startswith("Owns:")]
    if len(status) != 1:
      fail(relative, "has no Status: line before its first section" if not status else "has two Status: lines")
    if not ADR_STATUS.fullmatch(lines[status[0]].rstrip()):
      fail(f"{relative}:{status[0] + 1}", "is not a status line \"Status: **accepted** · YYYY-MM-DD\"")
    if len(owns) != 1:
      fail(relative, "has no Owns: line before its first section" if not owns else "has two Owns: lines")
    owned = ADR_OWNS.fullmatch(lines[owns[0]].rstrip())
    keys = [key.strip() for key in owned.group(1).split(",")] if owned else []
    if not keys or not all(OWNS_KEY.fullmatch(key) for key in keys):
      fail(f"{relative}:{owns[0] + 1}", "is not an Owns: line of comma-separated kebab-case keys")
    for key in keys:
      if key in owners:
        fail(f"{relative}:{owns[0] + 1}", f"owns \"{key}\", which {owners[key]} owns too")
      owners[key] = f"ADR-{number}"
    check_decisions(relative, lines)
    adrs.append((number, heading.group(2).strip(), path.name, keys))
  if not adrs:
    fail(ADR_DIRECTORY, "holds no ADR")

  rows = [[f"ADR-{number}", f"[{escape(adr_title)}]({file_name})", ", ".join(f"`{key}`" for key in keys)]
          for number, adr_title, file_name, keys in adrs]
  lines = [banner("each ADR's title and `Owns:` line", "the ADR"), "", "# Architecture decision records", "",
           "Every ADR, by number: its title, and the subjects it owns. An ADR holds one engineering decision, in "
           "numbered decisions whose numbers never change, and each subject has exactly one owning ADR "
           "(AGENTS.md §6 and §8).", ""]
  lines += table(["ADR", "Title", "Owns"], rows)
  return document(lines)


# ── The open decisions. ─────────────────────────────────────────────────────────────────────────────────────────


def split_row(_line):
  text = _line.strip()
  if text.startswith("|"):
    text = text[1:]
  if text.endswith("|") and not text.endswith("\\|"):
    text = text[:-1]
  return [cell.strip() for cell in CELL_SPLIT.split(text)]


def relink(_text, _source, _output):
  """Re-points the relative links of a cell copied from one document into another."""
  def replace(_match):
    target = _match.group(2)
    bracketed = target.startswith("<")
    bare = target[1:-1] if bracketed else target
    if URL_SCHEME.match(bare) or bare.startswith("/"):
      return _match.group(0)
    path, hash_mark, fragment = bare.partition("#")
    resolved = posixpath.normpath(posixpath.join(posixpath.dirname(_source), path)) if path else _source
    moved = link_from(_output, resolved) + (hash_mark + fragment if hash_mark else "")
    return _match.group(1) + (f"<{moved}>" if bracketed else moved)

  return INLINE_LINK.sub(replace, _text)


def gate_documents(_root):
  documents = []
  for directory in GATE_DIRECTORIES:
    if not (_root / directory).is_dir():
      fail(directory, "does not exist")
    for path in (_root / directory).rglob("*.md"):
      relative = path.relative_to(_root).as_posix()
      if relative not in OUTPUTS:
        documents.append(relative)
  return sorted(documents)


def open_decisions(_root):
  found = {}
  for relative in gate_documents(_root):
    lines = read_text(_root, relative).split("\n")
    fence = None
    index = 0
    while index < len(lines):
      line = lines[index]
      was_open = fence is not None
      fence = fence_after(line, fence)
      if was_open or fence is not None:
        index += 1
        continue
      header = line.strip()
      if not (header.startswith("|") and index + 1 < len(lines) and TABLE_DELIMITER.fullmatch(lines[index + 1].strip())
              and split_row(header)[0].strip("*").lower() in ("gate", "gates")):
        index += 1
        continue
      where = f"{relative}:{index + 1}"
      if header != GATE_HEADER:
        fail(where, f"is a gate table headed \"{header}\", where a gate table is headed \"{GATE_HEADER}\"")
      if len(split_row(lines[index + 1])) != 5:
        fail(f"{relative}:{index + 2}", "does not divide the gate table's header into its five columns")
      index += 2
      while index < len(lines) and lines[index].strip().startswith("|"):
        cells = split_row(lines[index])
        where = f"{relative}:{index + 1}"
        if len(cells) != 5:
          fail(where, f"is a gate row of {len(cells)} cells, where a gate table has 5")
        if not GATE_STATUS.fullmatch(cells[4]):
          fail(where, f"gives gate {cells[0]} the Status \"{cells[4]}\", which is neither \"open\" nor "
                      "\"decided YYYY-MM-DD\"")
        if cells[4] == "open":
          found.setdefault(relative, []).append(
            [relink(cell, relative, OPEN_DECISIONS) for cell in (cells[0], cells[1], cells[3])])
        index += 1

  total = sum(len(rows) for rows in found.values())
  lines = [banner("the gate tables under `Docs/`", "the gate table"), "", "# Open decisions", "",
           "Every owner decision that is still open: each row of a gate table whose Status is `open`, in every "
           "document under `Docs/`, grouped by the document that holds it. A gate is decided in its own table, and "
           "this list follows (AGENTS.md §8).", ""]
  if not found:
    lines += ["No gate is open."]
  else:
    documents = "document" if len(found) == 1 else "documents"
    lines += [f"{total} {'gate is' if total == 1 else 'gates are'} open, in {len(found)} {documents}.", ""]
    for relative, rows in found.items():
      lines += [f"## [`{relative}`]({link_from(OPEN_DECISIONS, relative)})", ""]
      lines += table(["Gate", "Decision", "Blocks"], rows) + [""]
  return document(lines)


# ── Generating, checking and writing. ───────────────────────────────────────────────────────────────────────────


GENERATORS = ((CATALOGUE, catalogue), (CONTROLS, controls), (COLORS, colors), (ADR_INDEX, adr_index),
              (OPEN_DECISIONS, open_decisions))


def generate(_root):
  """Every generated document, as bytes, by its path. A document that cannot be generated fails the run, and every one
  that cannot is named, not only the first."""
  outputs = {}
  errors = []
  for relative, generator in GENERATORS:
    try:
      outputs[relative] = generator(_root).encode("utf-8")
    except GenerateError as error:
      errors.append(f"{relative} cannot be generated: {error}")
  if errors:
    raise GenerateError("\n".join(errors))
  return outputs


def stale(_root, _outputs):
  findings = []
  for relative, content in _outputs.items():
    path = _root / relative
    if not path.is_file():
      findings.append(f"{relative}: missing; run {COMMAND} and commit what it writes")
    elif path.read_bytes() != content:
      findings.append(f"{relative}: not what {GENERATOR} generates; run {COMMAND} and commit what it writes, and "
                      "change the source rather than the document")
  return findings


def write_all():
  for relative, content in generate(REPO_ROOT).items():
    path = REPO_ROOT / relative
    if path.is_file() and path.read_bytes() == content:
      print(f"current   {relative}")
      continue
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(content)
    print(f"wrote     {relative}")
  return 0


def check_all():
  findings = stale(REPO_ROOT, generate(REPO_ROOT))
  for finding in findings:
    print(finding)
  if findings:
    print(f"\n{len(findings)} of {len(OUTPUTS)} generated documents are not current.")
    return 1
  print(f"Generated documents: {len(OUTPUTS)}, all current.")
  return 0


# ── Self-test: each check, shown to fail on a copy of its input with one thing broken. ─────────────────────────


SELF_TEST_ADRS = {
  "ADR-001-first.md": "# ADR-001 — The first\n\nStatus: **accepted** · 2026-10-01\n"
                      "Owns: first-subject, shared-subject\n\n## Context\n\nWhy.\n\n## Decision\n\n"
                      "1. **One.** Text.\n   More of the first decision.\n\n   ```\n   a diagram\n   ```\n"
                      "2. *(retired)*\n3. **Three.** Text.\n\n## Consequences\n\nSome.\n",
  "ADR-002-second.md": "# ADR-002 — The second\n\nStatus: **accepted** · 2026-10-02\nOwns: second-subject\n\n"
                       "## Context\n\nWhy.\n\n## Decision\n\n1. **One.** Text.\n\n## Consequences\n\nSome.\n",
}
# A plan in a directory of its own under Docs/, and a proposal directly in it, so that gates are collected from both
# and a link copied from each is re-pointed from where it was written to where OpenDecisions.md is.
SELF_TEST_PLAN_PATH = "Docs/Plans/ImplementationPlan-Test.md"
SELF_TEST_PROPOSAL_PATH = "Docs/Proposal.md"
SELF_TEST_PLAN = ("# A plan\n\n## Gates\n\n" + GATE_HEADER + "\n|---|---|---|---|---|\n"
                  "| K1 | **Open.** See [the second](../ADR/ADR-002-second.md) and [the tasks](#tasks). "
                  "| this plan | K1.1 | open |\n"
                  "| K2 | **Decided.** | this plan | K2.1 | decided 2026-10-03 |\n\n"
                  "```\n| Gate | Question |\n|---|---|\n| X1 | In a code block, so not a gate table |\n```\n\n"
                  "## Tasks\n")
SELF_TEST_PROPOSAL = ("# A proposal\n\n| Gate | Decision | Proposed in | Blocks | Status |\n|---|---|---|---|---|\n"
                      "| N1 | **Also open.** As [ADR-001](ADR/ADR-001-first.md) has it. | here | the network "
                      "| open |\n")


def build_fixture(_root):
  for relative in (TUNING, MAP, MATCHUPS, CAMERA, KEY_BINDINGS) + tuple(relative for _, relative in DIFFICULTIES):
    (_root / relative).parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(REPO_ROOT / relative, _root / relative)
  for directory in COLOR_DIRECTORIES:
    (_root / directory).mkdir(parents=True, exist_ok=True)
    for relative in color_sources(REPO_ROOT):
      if relative.startswith(directory + "/"):
        shutil.copy2(REPO_ROOT / relative, _root / relative)
  for name, text in SELF_TEST_ADRS.items():
    write_fixture(_root, f"{ADR_DIRECTORY}/{name}", text)
  write_fixture(_root, SELF_TEST_PLAN_PATH, SELF_TEST_PLAN)
  write_fixture(_root, SELF_TEST_PROPOSAL_PATH, SELF_TEST_PROPOSAL)


def write_fixture(_root, _relative, _text):
  (_root / _relative).parent.mkdir(parents=True, exist_ok=True)
  (_root / _relative).write_bytes(_text.encode("utf-8"))


def edit(_root, _relative, _old, _new):
  path = _root / _relative
  text = path.read_bytes().decode("utf-8-sig")
  if _old not in text:
    raise AssertionError(f"self-test fixture: '{_old}' is not in {_relative}")
  path.write_bytes(text.replace(_old, _new, 1).encode("utf-8"))


def edit_json(_root, _relative, _change):
  data = json.loads((_root / _relative).read_text(encoding="utf-8-sig"))
  _change(data)
  write_fixture(_root, _relative, json.dumps(data, indent=2))


SELF_TEST_CASES = (
  # (the check, what is broken, the file the failure must name, what the failure must say, the change)
  ("catalogue", "Tuning.json that is not JSON", TUNING, "is not JSON",
   lambda r: edit(r, TUNING, "{", "{,")),
  ("catalogue", "Tuning.json without its salvage", TUNING, "has no \"salvage\"",
   lambda r: edit_json(r, TUNING, lambda d: d.pop("salvage"))),
  ("catalogue", "a key repeated in one object", MAP, "repeats the key",
   lambda r: edit(r, MAP, "\"sizeMeters\": 10000,", "\"sizeMeters\": 10000, \"sizeMeters\": 9000,")),
  ("catalogue", "a value nested too deep for a cell", TUNING, "cannot write in a table cell",
   lambda r: edit_json(r, TUNING, lambda d: d["hulls"][0].update(hitPoints=[[1, 2]]))),
  ("catalogue", "a key that is not camelCase", TUNING, "which is not camelCase",
   lambda r: edit_json(r, TUNING, lambda d: d["rules"].update(tick_rate=20))),
  ("catalogue", "a section that is null", MAP, "is null",
   lambda r: edit_json(r, MAP, lambda d: d.update(placement=None))),
  ("catalogue", "a starting design whose hull is no hull", TUNING, "none of the hulls",
   lambda r: edit_json(r, TUNING, lambda d: d["startingDesigns"][0].update(hull=99))),
  ("catalogue", "a research effect it cannot write", TUNING, "is an effect with the keys",
   lambda r: edit_json(r, TUNING, lambda d: d["research"][0].update(effect={"unlockEverything": 1}))),
  ("catalogue", "a sector adjacent to a sector that is not there", MAP, "none of the sectors",
   lambda r: edit_json(r, MAP, lambda d: d["sectors"][0].update(adjacent=[2, 99]))),
  ("catalogue", "two anchors with one id", MAP, "would have the anchor \"rules\"",
   lambda r: edit_json(r, MAP, lambda d: d.update(rules={"spare": 1}))),
  ("catalogue", "a difficulty without one of Normal's keys", DIFFICULTIES[2][1], "lacks raidShips",
   lambda r: edit_json(r, DIFFICULTIES[2][1], lambda d: d.pop("raidShips"))),
  ("catalogue", "a difficulty with a key Normal lacks", DIFFICULTIES[0][1], "adds cheats",
   lambda r: edit_json(r, DIFFICULTIES[0][1], lambda d: d.update(cheats=1))),
  ("catalogue", "Matchups.json without its matchups", MATCHUPS, "has no \"matchups\"",
   lambda r: write_fixture(r, MATCHUPS, "{ \"battles\": [] }\n")),
  ("catalogue", "Camera.json missing", CAMERA, "does not exist",
   lambda r: (r / CAMERA).unlink()),
  ("controls", "a KEY_ constant whose key is a number", KEY_BINDINGS, "a key this cannot read",
   lambda r: edit(r, KEY_BINDINGS, "KEY_STOP = 'S';", "KEY_STOP = 0x53;")),
  ("controls", "a KEY_ constant on a key KeyName does not name", KEY_BINDINGS, "which KeyName does not name",
   lambda r: edit(r, KEY_BINDINGS, "KEY_STOP = 'S';", "KEY_STOP = VK_TAB;")),
  ("controls", "a KEY_ constant with no comment", KEY_BINDINGS, "has no comment",
   lambda r: edit(r, KEY_BINDINGS, "inline constexpr std::uint8_t KEY_PAN_UP",
                  "inline constexpr std::uint8_t KEY_LONELY = 'L';\n\ninline constexpr std::uint8_t KEY_PAN_UP")),
  ("controls", "a KEY_ constant in a form it cannot read", KEY_BINDINGS, "is a KEY_ constant this cannot read",
   lambda r: edit(r, KEY_BINDINGS, "KEY_STOP = 'S';", "KEY_STOP{'S'};")),
  ("colors", "a _COLOR constant with two components", "GameApp/Hud.cpp", "not 4 number literals",
   lambda r: edit(r, "GameApp/Hud.cpp", "TEXT_COLOR{0.82f, 0.88f, 0.96f, 1.0f}", "TEXT_COLOR{0.82f, 0.88f}")),
  ("colors", "a _COLOR constant from a function", "GameApp/Hud.cpp", "in a form this cannot read as a color",
   lambda r: edit(r, "GameApp/Hud.cpp", "TEXT_COLOR{0.82f, 0.88f, 0.96f, 1.0f}", "TEXT_COLOR = Mix(0.5f)")),
  ("colors", "a _COLOR alias of a constant that is not there", "GameApp/Hud.cpp", "is not a _COLOR constant this reads",
   lambda r: edit(r, "GameApp/Hud.cpp", "TEXT_COLOR{0.82f, 0.88f, 0.96f, 1.0f}", "TEXT_COLOR = MISSING_COLOR")),
  ("colors", "a _COLOR constant of a type that is not a color", "GameApp/Hud.cpp",
   "which this does not read as a color",
   lambda r: edit(r, "GameApp/Hud.cpp", "constexpr DirectX::XMFLOAT4 TEXT_COLOR{0.82f, 0.88f, 0.96f, 1.0f}",
                  "constexpr DirectX::XMFLOAT2 TEXT_COLOR{0.82f, 0.88f}")),
  ("adr-index", "an ADR without a Status: line", f"{ADR_DIRECTORY}/ADR-002-second.md", "has no Status: line",
   lambda r: edit(r, f"{ADR_DIRECTORY}/ADR-002-second.md", "Status: **accepted** · 2026-10-02\n", "")),
  ("adr-index", "an ADR without an Owns: line", f"{ADR_DIRECTORY}/ADR-002-second.md", "has no Owns: line",
   lambda r: edit(r, f"{ADR_DIRECTORY}/ADR-002-second.md", "Owns: second-subject\n", "")),
  ("adr-index", "a subject two ADRs own", f"{ADR_DIRECTORY}/ADR-002-second.md", "which ADR-001 owns too",
   lambda r: edit(r, f"{ADR_DIRECTORY}/ADR-002-second.md", "Owns: second-subject",
                  "Owns: second-subject, shared-subject")),
  ("adr-index", "an Owns: key that is not kebab-case", f"{ADR_DIRECTORY}/ADR-002-second.md", "kebab-case keys",
   lambda r: edit(r, f"{ADR_DIRECTORY}/ADR-002-second.md", "Owns: second-subject", "Owns: Second Subject")),
  ("adr-index", "a ## Decision with a paragraph between its decisions", f"{ADR_DIRECTORY}/ADR-001-first.md",
   "is not one numbered list",
   lambda r: edit(r, f"{ADR_DIRECTORY}/ADR-001-first.md", "2. *(retired)*", "A loose paragraph.\n\n2. *(retired)*")),
  ("adr-index", "a ## Decision that skips a number", f"{ADR_DIRECTORY}/ADR-001-first.md", "where 3 comes next",
   lambda r: edit(r, f"{ADR_DIRECTORY}/ADR-001-first.md", "3. **Three.**", "4. **Three.**")),
  ("adr-index", "a ## Decision with a code block outside its decisions", f"{ADR_DIRECTORY}/ADR-001-first.md",
   "opens a code block outside a numbered decision",
   lambda r: edit(r, f"{ADR_DIRECTORY}/ADR-001-first.md", "   ```\n   a diagram\n   ```", "```\na diagram\n```")),
  ("adr-index", "an ADR without a ## Decision", f"{ADR_DIRECTORY}/ADR-002-second.md", "has no \"## Decision\" section",
   lambda r: edit(r, f"{ADR_DIRECTORY}/ADR-002-second.md", "## Decision", "## Decisions")),
  ("adr-index", "an ADR titled with another number", f"{ADR_DIRECTORY}/ADR-002-second.md", "is titled ADR-003",
   lambda r: edit(r, f"{ADR_DIRECTORY}/ADR-002-second.md", "# ADR-002 —", "# ADR-003 —")),
  ("open-decisions", "a gate table without a Status column", SELF_TEST_PROPOSAL_PATH, "is a gate table headed",
   lambda r: write_fixture(r, SELF_TEST_PROPOSAL_PATH, "| Gate | Decision | Proposed in | Blocks |\n|---|---|---|---|\n"
                                                       "| N1 | **Open.** | here | the network |\n")),
  ("open-decisions", "a gate whose Status is neither open nor decided", SELF_TEST_PLAN_PATH, "the Status \"done\"",
   lambda r: edit(r, SELF_TEST_PLAN_PATH, "| decided 2026-10-03 |", "| done |")),
  ("open-decisions", "a gate decided without a date", SELF_TEST_PLAN_PATH, "the Status \"decided\"",
   lambda r: edit(r, SELF_TEST_PLAN_PATH, "| decided 2026-10-03 |", "| decided |")),
  ("open-decisions", "a gate row with a cell too few", SELF_TEST_PROPOSAL_PATH, "is a gate row of 4 cells",
   lambda r: edit(r, SELF_TEST_PROPOSAL_PATH, "| here | the network | open |", "| here | open |")),
)


def self_test():
  failures = 0
  cases = 0

  def report(_ok, _label, _detail=None):
    nonlocal failures, cases
    cases += 1
    failures += not _ok
    print(f"{'pass' if _ok else 'FAIL'}  {_label}")
    if _detail and not _ok:
      print(f"        {_detail}")

  with tempfile.TemporaryDirectory(ignore_cleanup_errors=True) as scratch:
    pristine = Path(scratch) / "pristine"
    build_fixture(pristine)
    try:
      first, second = generate(pristine), generate(pristine)
    except GenerateError as error:
      print(f"FAIL  the unbroken fixture generates\n        {error}")
      print("\nThe self-test cannot run without an unbroken fixture.")
      return 1
    report(True, "(quiet) the unbroken fixture generates")
    report(first == second, "(quiet) generating twice gives the same bytes")
    for relative, content in first.items():
      text = content.decode("utf-8")
      report(text.startswith(f"*Generated by `{GENERATOR}`") and text.endswith("\n") and "\r" not in text
             and not text.endswith("\n\n"),
             f"(quiet) {relative} opens with its banner and is LF with one final newline")
    opened = first[OPEN_DECISIONS].decode("utf-8")
    report("| K1 |" in opened and "| N1 |" in opened and "| K2 |" not in opened and "| X1 |" not in opened,
           "(quiet) the open gates are listed, and the decided one and the one in a code block are not")
    report("](../ADR/ADR-002-second.md)" in opened and "](../Plans/ImplementationPlan-Test.md#tasks)" in opened and
           "](../ADR/ADR-001-first.md)" in opened and "](ADR/ADR-001-first.md)" not in opened,
           "(quiet) a link copied from a gate row is re-pointed")
    index = first[ADR_INDEX].decode("utf-8")
    report("| ADR-001 | [The first](ADR-001-first.md) | `first-subject`, `shared-subject` |" in index,
           "(quiet) an ADR's row holds its number, its title linked to its file and its Owns: keys")
    listed = first[CATALOGUE].decode("utf-8")
    report("<a id=\"hulls\"></a>" in listed and "<a id=\"attackGroupShips\"></a>" in listed and
           "<a id=\"sizeMeters\"></a>" in listed, "(quiet) a section and a key's row each have an anchor of its key")

    for check, description, named, said, mutate in SELF_TEST_CASES:
      case = Path(scratch) / f"case{cases}"
      shutil.copytree(pristine, case)
      mutate(case)
      try:
        generate(case)
        report(False, f"[{check}] {description}", "it generated rather than failing")
      except GenerateError as error:
        report(named in str(error) and said in str(error), f"[{check}] {description}",
               f"it failed, but not by naming {named} and saying \"{said}\": {error}")
      shutil.rmtree(case, ignore_errors=True)

    for description, mutate in (
        ("a generated document that differs by a byte", lambda r, o: (r / o).write_bytes((r / o).read_bytes() + b" ")),
        ("a generated document that is missing", lambda r, o: (r / o).unlink()),
        ("a generated document with CRLF line endings",
         lambda r, o: (r / o).write_bytes((r / o).read_bytes().replace(b"\n", b"\r\n")))):
      for relative in OUTPUTS:
        case = Path(scratch) / f"case{cases}"
        shutil.copytree(pristine, case)
        for output, content in first.items():
          (case / output).parent.mkdir(parents=True, exist_ok=True)
          (case / output).write_bytes(content)
        clean = not stale(case, generate(case))
        mutate(case, relative)
        findings = stale(case, generate(case))
        report(clean and len(findings) == 1 and findings[0].startswith(relative), f"[check] {description}: {relative}",
               f"findings: {findings}")
        shutil.rmtree(case, ignore_errors=True)

  print(f"\n{cases - failures} of {cases} self-test cases passed.")
  return 1 if failures else 0


def main():
  parser = argparse.ArgumentParser(description="Write the documents generated from the code and data (AGENTS.md §8).")
  group = parser.add_mutually_exclusive_group()
  group.add_argument("--check", action="store_true", help="fail when a generated document is not what is generated")
  group.add_argument("--self-test", action="store_true",
                     help="show that each check fails on a broken copy of its input")
  arguments = parser.parse_args()
  try:
    if arguments.self_test:
      return self_test()
    if arguments.check:
      return check_all()
    return write_all()
  except GenerateError as error:
    for line in str(error).split("\n"):
      print(f"GenerateDocs: {line}")
    return 1


if __name__ == "__main__":
  sys.exit(main())

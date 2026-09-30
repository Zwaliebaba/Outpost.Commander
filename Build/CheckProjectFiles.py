#!/usr/bin/env python3
"""The build-shape gate of AGENTS.md §1, §2 and §3.

Reads OutpostCommander.slnx, every .vcxproj and every .vcxproj.filters, and the C++ and HLSL source of the tree, and
fails on anything AGENTS.md says a build or a review would otherwise have to catch:

  solution         every project the solution names exists and has a .filters; every .vcxproj is in the solution
  registration     every source file in a project folder is in its .vcxproj and its .filters (§2)
  missing-file     every file a .vcxproj or .filters names exists (package paths are 0.3's, and restored in CI)
  filters-match    the .vcxproj and the .filters list the same files
  filter-name      no Visual Studio default filter (Source Files, Header Files, Resource Files) (§2)
  filter-declared  every filter an item is in is declared
  filter-split     a .h sits in the same filter as its .cpp (§2)
  location         source sits directly in its project folder; HLSL only in <Lib>/Shader/; nothing committed from
                   CompiledShader/; no source outside a project (§2)
  extension        no .hpp, .cc, .inl or other spelling of a C++ file (R7)
  file-name        .h and .cpp names are PascalCase, apart from R7's exceptions
  shader-name      shaders are <Shader>VS.hlsl or <Shader>PS.hlsl (§2)
  affix            no type is named with an I/C/S/E prefix, a Base/Abstract/Impl affix or a _t suffix (R2)
  spelling         no identifier uses the non-SDK spelling of a word family (R11)
  include-climb    no #include leaves its own project with '..' (ADR-002)
  wrl              no Microsoft::WRL::ComPtr and no WRL header; a COM pointer is a winrt::com_ptr (R12)
  header-filter    .clang-tidy's HeaderFilterRegex covers every project in the solution (§2)

and, for the build settings that stand in for the Release and ARM64 builds CI never runs (§3, ADR-003):

  platforms        the solution and every project have x64 and ARM64, Debug and Release, and nothing else
  settings         v145, stdcpplatest, ConformanceMode, Level4, TreatWarningAsError and Precise are stated, with
                   AVX2 on x64 and ARMv8.0 on ARM64 (R16)
  alignment        Debug and Release differ only in what §3 lists, x64 and ARM64 only in the instruction set, and
                   each configuration defines its own _DEBUG or NDEBUG
  macros           no project defines the Windows macro family and no header but NeuronCore.h does (§4); nothing
                   defines USE_PIX, USE_PIX_RETAIL or PROFILE (ADR-005)
  include-path     each project's include path is its row of ADR-002's table, spelled $(SolutionDir)<Project>, and
                   the executable does not inherit the default one (ADR-001)
  packages         only R14's projects have a packages.config, each lists only its own packages, and every package
                   folder a project names is in its packages.config at that version

The settings are what the project file states for each configuration, with each element's Condition evaluated for
$(Configuration) and $(Platform). MSBuild's defaults are not read: a setting the file does not state is absent.

R2 and R11 read declarations and identifiers with regular expressions over the source with comments and string
literals blanked out. They do not parse C++, so they are written to miss rather than to guess: a type is checked where
it is defined (a name followed by '{', or ':' for a base or an enum's underlying type), not where it is forward
declared, which is also what lets a header forward-declare an SDK interface such as IDXGISwapChain4 (R4).

Usage:
  python Build/CheckProjectFiles.py              check the tree
  python Build/CheckProjectFiles.py --self-test  copy the tree, break it once per check, and show each check fires

Exit status: 0 when the tree is clean, 1 when there are findings (or a self-test case failed), 2 when the tree could
not be read.
"""

import argparse
import dataclasses
import posixpath
import re
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
MSBUILD_NS = "{http://schemas.microsoft.com/developer/msbuild/2003}"

# Every spelling of a C++ or HLSL source file, so that one in the wrong place or with the wrong extension is seen.
CPP_EXTENSIONS = {".h", ".cpp"}
BANNED_EXTENSIONS = {".hpp", ".hh", ".hxx", ".h++", ".cc", ".cxx", ".c++", ".c", ".inl", ".ipp", ".tpp", ".ixx"}
SHADER_EXTENSIONS = {".hlsl", ".hlsli", ".fx", ".fxh"}
SOURCE_EXTENSIONS = CPP_EXTENSIONS | BANNED_EXTENSIONS | SHADER_EXTENSIONS

SHADER_DIRECTORY = "Shader"
COMPILED_SHADER_DIRECTORY = "CompiledShader"
R7_EXCEPTIONS = {"pch.h", "pch.cpp", "framework.h", "targetver.h", "Resource.h"}
DEFAULT_FILTERS = {"source files", "header files", "resource files"}

# The item types that name a file. Anything else (ProjectConfiguration, ProjectCapability, ...) is not a path.
FILE_ITEMS = {"ClInclude", "ClCompile", "FXCompile", "None", "Content", "Image", "Text", "Natvis", "Manifest",
              "AppxManifest", "ResourceCompile", "Midl", "Xml", "CopyFileToFolders", "Font", "Media"}

# R11. The SDK (US) spelling wins; each entry is the other spelling's stem. A word in an identifier is flagged when it
# is one of these stems followed by one of SPELLING_SUFFIXES, so 'Colours' and 'initialised' are caught and 'realistic'
# is not. AGENTS.md lists the families it has met; the rest are the same families, and the design document's own
# words (armour, defence, metre) are here because they are the ones an identifier will reach for first.
BRITISH_STEMS = {
  "colour": "color", "behaviour": "behavior", "neighbour": "neighbor", "flavour": "flavor", "harbour": "harbor",
  "armour": "armor", "honour": "honor", "favour": "favor", "labour": "labor", "rumour": "rumor", "vapour": "vapor",
  "centre": "center", "metre": "meter", "litre": "liter", "fibre": "fiber", "grey": "gray", "defence": "defense",
  "licence": "license", "offence": "offense", "catalogue": "catalog", "analys": "analyz",
  "cancell": "cancel", "travell": "travel", "labell": "label", "modell": "model", "signall": "signal",
  "initialis": "initializ", "serialis": "serializ", "deserialis": "deserializ", "normalis": "normaliz",
  "quantis": "quantiz", "synchronis": "synchroniz", "optimis": "optimiz", "finalis": "finaliz",
  "materialis": "materializ", "visualis": "visualiz", "randomis": "randomiz", "customis": "customiz",
  "minimis": "minimiz", "maximis": "maximiz", "prioritis": "prioritiz", "recognis": "recogniz", "organis": "organiz",
  "authoris": "authoriz", "utilis": "utiliz", "summaris": "summariz", "capitalis": "capitaliz", "specialis": "specializ",
}
SPELLING_SUFFIXES = ("", "s", "e", "es", "ed", "er", "ers", "ing", "ings", "ation", "ations", "ful", "less", "scale")

# R2. A type we name must not carry a Hungarian-style prefix, an abstractness affix or a C-style suffix.
AFFIX_PATTERNS = (
  (re.compile(r"^[ICSE][A-Z]"), "an I/C/S/E prefix"),
  (re.compile(r"^(Base|Abstract|Impl)[A-Z0-9]"), "a Base/Abstract/Impl prefix"),
  (re.compile(r".(Base|Abstract|Impl)$"), "a Base/Abstract/Impl suffix"),
  (re.compile(r"_t$"), "a _t suffix"),
)
ATTRIBUTES = r"(?:\s*(?:\[\[[^\]]*\]\]|__declspec\s*\([^)]*\)|alignas\s*\([^)]*\)))*"
TYPE_DEFINITION = re.compile(r"\b(?:class|struct|union|enum(?:\s+(?:class|struct))?)" + ATTRIBUTES +
                             r"\s+(?:\w+\s*::\s*)*(\w+)\s*(?:final\s*)?(?:\{|:(?!:))")
TYPE_ALIAS = re.compile(r"\b(?:using|concept)\s+(\w+)\s*=")
INCLUDE = re.compile(r'^[ \t]*#[ \t]*include[ \t]*([<"])([^>"]*)[>"]', re.MULTILINE)
IDENTIFIER = re.compile(r"\b[A-Za-z_]\w*\b")
WORD = re.compile(r"[A-Z]+(?=[A-Z][a-z])|[A-Z]?[a-z]+|[A-Z]+")


@dataclasses.dataclass(frozen=True, order=True)
class Finding:
  location: str
  check: str
  message: str

  def __str__(self):
    return f"{self.location}: [{self.check}] {self.message}"


@dataclasses.dataclass
class Item:
  kind: str
  path: str        # repository-relative, forward slashes, or the raw Include when it is not a local file
  local: bool      # False for a path with an MSBuild property in it, or one in the restored packages folder
  filter: str | None


@dataclasses.dataclass
class Project:
  name: str
  directory: str   # repository-relative, forward slashes
  vcxproj: ET.Element
  filters: ET.Element | None
  vcxproj_path: str
  filters_path: str

  def items(self, _root=None):
    return read_items(self.vcxproj if _root is None else _root, self.directory)

  def filter_items(self):
    return [] if self.filters is None else read_items(self.filters, self.directory)

  def declared_filters(self):
    if self.filters is None:
      return set()
    return {element.get("Include") for element in self.filters.iter(f"{MSBUILD_NS}Filter") if element.get("Include")}


class Tree:
  """What the checks read: the solution, its projects and the source files, all relative to one root."""

  def __init__(self, root):
    self.root = root
    self.solution_path = self.find_solution()
    self.projects, self.solution_findings = self.load_projects()
    self.tracked, self.files = self.list_files()
    self._text = {}

  def find_solution(self):
    solutions = sorted(self.root.glob("*.slnx"))
    if len(solutions) != 1:
      raise SystemExit(f"CheckProjectFiles: expected one .slnx at {self.root}, found {len(solutions)}.")
    return solutions[0]

  def load_projects(self):
    findings = []
    projects = []
    solution = parse_xml(self.solution_path)
    for element in solution.iter("Project"):
      path = element.get("Path", "").replace("\\", "/")
      if not path.endswith(".vcxproj"):
        continue
      where = self.solution_path.name
      if not (self.root / path).is_file():
        findings.append(Finding(where, "solution", f"names {path}, which does not exist"))
        continue
      filters_path = path + ".filters"
      filters = parse_xml(self.root / filters_path) if (self.root / filters_path).is_file() else None
      if filters is None:
        findings.append(Finding(path, "solution", f"has no {Path(filters_path).name}"))
      projects.append(Project(Path(path).stem, posixpath.dirname(path), parse_xml(self.root / path), filters, path,
                              filters_path))
    return projects, findings

  def list_files(self):
    def ls(*_flags):
      result = subprocess.run(["git", "ls-files", "-z", *_flags], cwd=self.root, capture_output=True)
      if result.returncode != 0:
        raise SystemExit(f"CheckProjectFiles: 'git ls-files' failed: {result.stderr.decode(errors='replace').strip()}")
      return {name for name in result.stdout.decode().split("\0") if name and (self.root / name).is_file()}

    tracked = ls("--cached")
    return tracked, sorted(tracked | ls("--others", "--exclude-standard"))

  def text(self, _path):
    if _path not in self._text:
      self._text[_path] = (self.root / _path).read_text(encoding="utf-8-sig", errors="replace")
    return self._text[_path]

  def project_for(self, _path):
    for project in self.projects:
      if project.directory and _path.startswith(project.directory + "/"):
        return project
    return None

  def source_files(self):
    return [path for path in self.files if Path(path).suffix.lower() in SOURCE_EXTENSIONS]

  def cpp_files(self):
    """.h and .cpp that are hand-written: not generated shader headers."""
    return [path for path in self.files
            if Path(path).suffix in CPP_EXTENSIONS and COMPILED_SHADER_DIRECTORY not in Path(path).parts]


def parse_xml(_path):
  try:
    return ET.parse(_path).getroot()
  except ET.ParseError as error:
    raise SystemExit(f"CheckProjectFiles: {_path} is not well-formed XML: {error}")


def read_items(_root, _directory):
  items = []
  for group in _root.iter(f"{MSBUILD_NS}ItemGroup"):
    for element in group:
      kind = element.tag.removeprefix(MSBUILD_NS)
      include = element.get("Include")
      if kind not in FILE_ITEMS or not include:
        continue
      raw = include.replace("\\", "/")
      filter_element = element.find(f"{MSBUILD_NS}Filter")
      filter_name = filter_element.text.strip() if filter_element is not None and filter_element.text else None
      path = posixpath.normpath(posixpath.join(_directory, raw))
      local = "$(" not in raw and not path.startswith("packages/")
      items.append(Item(kind, path if local else raw, local, filter_name))
  return items


def strip_cpp(_text, _keep_strings=False):
  """Blanks out comments and, unless asked not to, the contents of string and character literals. Newlines survive,
  so a line number in the result is a line number in the file."""
  out = []
  i = 0
  length = len(_text)

  def blank(_segment):
    return "".join("\n" if character == "\n" else " " for character in _segment)

  while i < length:
    character = _text[i]
    pair = _text[i:i + 2]
    if pair == "//":
      end = _text.find("\n", i)
      end = length if end < 0 else end
      out.append(blank(_text[i:end]))
      i = end
    elif pair == "/*":
      end = _text.find("*/", i + 2)
      end = length if end < 0 else end + 2
      out.append(blank(_text[i:end]))
      i = end
    elif character == '"' or (character == "'" and literal_prefix_allows(_text, i)):
      # A raw string R"delim( ... )delim" ends only at its own delimiter.
      raw = re.match(r'R"([^(\s]*)\(', _text[i - 1:i + 18]) if i > 0 and _text[i - 1] == "R" else None
      if raw:
        end = _text.find(")" + raw.group(1) + '"', i)
        end = length if end < 0 else end + len(raw.group(1)) + 2
      else:
        end = i + 1
        while end < length and _text[end] != character and _text[end] != "\n":
          end += 2 if _text[end] == "\\" else 1
        end = min(end + 1, length)
      literal = _text[i:end]
      out.append(literal if _keep_strings else character + blank(literal[1:-1]) + literal[-1:])
      i = end
    else:
      out.append(character)
      i += 1
  return "".join(out)


def literal_prefix_allows(_text, _index):
  """A quote after an identifier character is a digit separator (1'000) unless that identifier is a literal prefix."""
  if _index == 0 or not (_text[_index - 1].isalnum() or _text[_index - 1] == "_"):
    return True
  match = re.search(r"(\w+)$", _text[max(0, _index - 3):_index])
  return match is not None and match.group(1) in ("L", "u", "U", "u8") and not re.search(
    r"\w", _text[_index - len(match.group(1)) - 1:_index - len(match.group(1))] if _index > len(match.group(1)) else "")


def line_of(_text, _offset):
  return _text.count("\n", 0, _offset) + 1


# ── Checks. Each takes the tree and returns findings. ────────────────────────────────────────────────────────────


def check_solution(_tree):
  findings = list(_tree.solution_findings)
  in_solution = {project.vcxproj_path for project in _tree.projects}
  for path in _tree.files:
    if path.endswith(".vcxproj") and path not in in_solution:
      findings.append(Finding(path, "solution", f"is not in {_tree.solution_path.name}"))
  return findings


def check_registration(_tree):
  findings = []
  for path in _tree.source_files():
    project = _tree.project_for(path)
    if project is None or COMPILED_SHADER_DIRECTORY in Path(path).parts:
      continue
    if path not in {item.path for item in project.items()}:
      findings.append(Finding(path, "registration", f"is not in {project.vcxproj_path}"))
    if project.filters is not None and path not in {item.path for item in project.filter_items()}:
      findings.append(Finding(path, "registration", f"is not in {project.filters_path}"))
  return findings


def check_missing_files(_tree):
  findings = []
  for project in _tree.projects:
    for where, items in ((project.vcxproj_path, project.items()), (project.filters_path, project.filter_items())):
      for item in items:
        if item.local and not (_tree.root / item.path).is_file():
          findings.append(Finding(where, "missing-file", f"{item.kind} {item.path} does not exist"))
    for element in project.vcxproj.iter(f"{MSBUILD_NS}ProjectReference"):
      path = posixpath.normpath(posixpath.join(project.directory, element.get("Include", "").replace("\\", "/")))
      if not (_tree.root / path).is_file():
        findings.append(Finding(project.vcxproj_path, "missing-file", f"ProjectReference {path} does not exist"))
  return findings


def check_filters_match(_tree):
  findings = []
  for project in _tree.projects:
    if project.filters is None:
      continue
    in_project = {(item.kind, item.path) for item in project.items()}
    in_filters = {(item.kind, item.path) for item in project.filter_items()}
    for kind, path in sorted(in_filters - in_project):
      findings.append(Finding(project.filters_path, "filters-match", f"lists {kind} {path}, which the .vcxproj does not"))
    for kind, path in sorted(in_project - in_filters):
      findings.append(Finding(project.filters_path, "filters-match", f"does not list {kind} {path}"))
  return findings


def check_filter_names(_tree):
  findings = []
  for project in _tree.projects:
    for name in sorted(project.declared_filters()):
      if any(part.strip().lower() in DEFAULT_FILTERS for part in name.split("\\")):
        findings.append(Finding(project.filters_path, "filter-name",
                                f"'{name}' groups by file kind; filters are functional (AGENTS.md §2)"))
  return findings


def check_filter_declared(_tree):
  findings = []
  for project in _tree.projects:
    declared = project.declared_filters()
    for item in project.filter_items():
      if item.filter is not None and item.filter not in declared:
        findings.append(Finding(project.filters_path, "filter-declared",
                                f"{item.path} is in filter '{item.filter}', which is not declared"))
  return findings


def check_filter_split(_tree):
  findings = []
  for project in _tree.projects:
    placed = {item.path: item.filter for item in project.filter_items() if item.kind in ("ClInclude", "ClCompile")}
    for path, filter_name in sorted(placed.items()):
      if not path.endswith(".h"):
        continue
      source = path[:-2] + ".cpp"
      if source in placed and placed[source] != filter_name:
        findings.append(Finding(project.filters_path, "filter-split",
                                f"{path} is in '{filter_name or '(root)'}' but {source} is in "
                                f"'{placed[source] or '(root)'}'"))
  return findings


def check_location(_tree):
  findings = []
  for path in _tree.source_files():
    parts = Path(path).parts
    project = _tree.project_for(path)
    if COMPILED_SHADER_DIRECTORY in parts:
      if path in _tree.tracked:
        findings.append(Finding(path, "location", "is compiler output and must not be committed (AGENTS.md §2)"))
      continue
    if project is None:
      findings.append(Finding(path, "location", "is source outside every project in the solution"))
      continue
    inside = Path(path).relative_to(project.directory).parts
    is_shader = Path(path).suffix.lower() in SHADER_EXTENSIONS
    if is_shader and inside[:-1] != (SHADER_DIRECTORY,):
      findings.append(Finding(path, "location", f"a shader belongs in {project.directory}/{SHADER_DIRECTORY}/"))
    elif not is_shader and len(inside) != 1:
      findings.append(Finding(path, "location", "source sits directly in its project folder; a header in a "
                                                "subdirectory is never seen by clang-tidy (AGENTS.md §2)"))
  return findings


def check_extensions(_tree):
  return [Finding(path, "extension", f"'{Path(path).suffix}' is not used here; C++ is .h and .cpp (R7)")
          for path in _tree.source_files() if Path(path).suffix.lower() in BANNED_EXTENSIONS]


def check_file_names(_tree):
  findings = []
  for path in _tree.cpp_files():
    name = Path(path).name
    if name not in R7_EXCEPTIONS and not re.fullmatch(r"[A-Z][A-Za-z0-9]*\.(h|cpp)", name):
      findings.append(Finding(path, "file-name", "a file is named for its primary type, in PascalCase (R7)"))
  return findings


def check_shader_names(_tree):
  findings = []
  for path in _tree.source_files():
    name = Path(path).name
    if Path(path).suffix.lower() in SHADER_EXTENSIONS and not re.fullmatch(r"[A-Z][A-Za-z0-9]*(VS|PS)\.hlsl", name):
      findings.append(Finding(path, "shader-name", "a shader is <Shader>VS.hlsl or <Shader>PS.hlsl (AGENTS.md §2)"))
  return findings


def check_affixes(_tree):
  findings = []
  for path in _tree.cpp_files():
    code = strip_cpp(_tree.text(path))
    for pattern in (TYPE_DEFINITION, TYPE_ALIAS):
      for match in pattern.finditer(code):
        name = match.group(1)
        for affix, description in AFFIX_PATTERNS:
          if affix.search(name):
            findings.append(Finding(f"{path}:{line_of(code, match.start(1))}", "affix",
                                    f"type '{name}' has {description}; name the concept (R2)"))
  return findings


def british_word(_word):
  lower = _word.lower()
  for stem, us in BRITISH_STEMS.items():
    if lower.startswith(stem) and lower[len(stem):] in SPELLING_SUFFIXES:
      return stem, us
  return None


def check_spelling(_tree):
  findings = []
  for path in _tree.cpp_files():
    code = strip_cpp(_tree.text(path))
    seen = set()
    for match in IDENTIFIER.finditer(code):
      identifier = match.group(0)
      for word in WORD.findall(identifier):
        hit = british_word(word)
        if hit and (identifier, hit) not in seen:
          seen.add((identifier, hit))
          findings.append(Finding(f"{path}:{line_of(code, match.start())}", "spelling",
                                  f"'{identifier}' spells '{hit[0]}'; identifiers use the SDK's '{hit[1]}' (R11)"))
  return findings


def check_includes(_tree):
  findings = []
  for path in _tree.cpp_files():
    code = strip_cpp(_tree.text(path), _keep_strings=True)
    for match in INCLUDE.finditer(code):
      target = match.group(2).replace("\\", "/")
      where = f"{path}:{line_of(code, match.start())}"
      if ".." in target.split("/"):
        findings.append(Finding(where, "include-climb", f"'{match.group(2)}' climbs out of its project; include "
                                                        f"another library through its include path (ADR-002)"))
      if target.lower() == "wrl.h" or target.lower().startswith("wrl/"):
        findings.append(Finding(where, "wrl", f"'{match.group(2)}' is WRL; use winrt::com_ptr from <winrt/base.h> (R12)"))
  return findings


def check_com_ptr(_tree):
  findings = []
  for path in _tree.cpp_files():
    code = strip_cpp(_tree.text(path))
    for match in re.finditer(r"\bComPtr\b|\bMicrosoft\s*::\s*WRL\b", code):
      findings.append(Finding(f"{path}:{line_of(code, match.start())}", "wrl",
                              f"'{match.group(0)}' is WRL; a COM pointer is a winrt::com_ptr (R12)"))
  return findings


def check_header_filter(_tree):
  config = _tree.root / ".clang-tidy"
  if not config.is_file():
    return [Finding(".clang-tidy", "header-filter", "does not exist")]
  match = re.search(r"^HeaderFilterRegex:\s*'((?:[^']|'')*)'\s*$", config.read_text(encoding="utf-8"), re.MULTILINE)
  if match is None:
    return [Finding(".clang-tidy", "header-filter", "has no single-quoted HeaderFilterRegex")]
  regex = re.compile(match.group(1).replace("''", "'"))
  findings = []
  for project in _tree.projects:
    for separator in ("/", "\\"):
      sample = separator.join(("C:", "Repository", project.directory.replace("/", separator), "Sample.h"))
      if not regex.search(sample):
        findings.append(Finding(".clang-tidy", "header-filter",
                                f"HeaderFilterRegex does not match {project.name}'s headers, so clang-tidy never "
                                f"checks them"))
        break
  return findings


# ── Build settings (AGENTS.md §3, R14, R16; ADR-001, ADR-002, ADR-003, ADR-005). ─────────────────────────────────


def condition_holds(_condition, _configuration, _platform):
  """Evaluates an MSBuild condition for one configuration and platform. A comparison that still names another property
  after $(Configuration) and $(Platform) are substituted, or a function such as Exists(), does not depend on the
  configuration, so it is taken as true: it cannot make two configurations differ, which is all this script asks."""
  if not _condition or not _condition.strip():
    return True
  text = _condition.replace("$(Configuration)", _configuration).replace("$(Platform)", _platform)

  def term_holds(_term):
    term = _term.strip()
    while term.startswith("(") and term.endswith(")"):
      term = term[1:-1].strip()
    match = re.fullmatch(r"'([^']*)'\s*(==|!=)\s*'([^']*)'", term)
    if match is None or "$(" in match.group(1) + match.group(3):
      return True
    equal = match.group(1).lower() == match.group(3).lower()
    return equal if match.group(2) == "==" else not equal

  return any(all(term_holds(term) for term in re.split(r"\s+and\s+", alternative, flags=re.IGNORECASE))
             for alternative in re.split(r"\s+or\s+", text, flags=re.IGNORECASE))


def split_list(_value):
  return [entry.strip() for entry in _value.split(";") if entry.strip()]


def effective_settings(_project, _configuration, _platform):
  """What the project file itself states for one configuration: properties by name, tool metadata as Tool.Name, and
  per-item metadata as Kind[path].Name. MSBuild's own defaults are not read; a setting the file does not state is
  absent, which is what 'stated, not inherited' asks for."""
  settings = {}

  def holds(_element):
    return condition_holds(_element.get("Condition"), _configuration, _platform)

  for group in _project.vcxproj:
    tag = group.tag.removeprefix(MSBUILD_NS)
    if not holds(group):
      continue
    if tag == "PropertyGroup":
      for element in group:
        if holds(element):
          settings[element.tag.removeprefix(MSBUILD_NS)] = (element.text or "").strip()
    elif tag == "ItemDefinitionGroup":
      for tool in group:
        for element in tool:
          if not holds(element):
            continue
          name = element.tag.removeprefix(MSBUILD_NS)
          key = f"{tool.tag.removeprefix(MSBUILD_NS)}.{name}"
          value = (element.text or "").strip()
          self_reference = f"%({name})"
          if self_reference in value:
            if key not in settings:
              settings[key + ":inherits"] = "true"
            value = value.replace(self_reference, settings.get(key, ""))
          settings[key] = ";".join(split_list(value)) if ";" in value else value
    elif tag == "ItemGroup":
      for element in group:
        kind = element.tag.removeprefix(MSBUILD_NS)
        if kind not in FILE_ITEMS or not element.get("Include") or not holds(element):
          continue
        for metadata in element:
          if holds(metadata):
            settings[f"{kind}[{element.get('Include')}].{metadata.tag.removeprefix(MSBUILD_NS)}"] = \
              (metadata.text or "").strip()
  return settings


CONFIGURATIONS = ("Debug", "Release")
PLATFORMS = ("x64", "ARM64")
REQUIRED_SETTINGS = {
  "PlatformToolset": "v145",
  "ClCompile.LanguageStandard": "stdcpplatest",
  "ClCompile.ConformanceMode": "true",
  "ClCompile.WarningLevel": "Level4",
  "ClCompile.TreatWarningAsError": "true",
  "ClCompile.FloatingPointModel": "Precise",
}
INSTRUCTION_SET_KEY = "ClCompile.EnableEnhancedInstructionSet"
INSTRUCTION_SETS = {"x64": "AdvancedVectorExtensions2", "ARM64": "CPUExtensionRequirementsARMv80"}

# AGENTS.md §3: the whole of what may differ between Debug and Release, by property or metadata name. The preprocessor
# definitions may differ only in _DEBUG against NDEBUG.
MAY_DIFFER_BY_CONFIGURATION = {"UseDebugLibraries", "RuntimeLibrary", "LinkIncremental", "WholeProgramOptimization",
                               "Optimization", "FunctionLevelLinking", "IntrinsicFunctions", "EnableCOMDATFolding",
                               "OptimizeReferences", "LinkTimeCodeGeneration"}
CONFIGURATION_DEFINES = {"Debug": "_DEBUG", "Release": "NDEBUG"}

# ADR-002's include-path table, one row per project. A project not in it is a project the ADR has not placed yet.
INCLUDE_PATHS = {
  "NeuronCore": (),
  "NeuronClient": ("NeuronCore",),
  "NeuronServer": ("NeuronCore",),
  "GameProtocol": ("NeuronCore",),
  "Opponent": ("NeuronCore", "GameProtocol"),
  "GameLogic": ("NeuronCore", "NeuronServer", "GameProtocol"),
  "GameApp": ("NeuronCore", "NeuronClient", "GameProtocol"),
  "OutpostCommander": ("NeuronCore", "NeuronClient", "GameProtocol", "Opponent", "GameApp"),
}
# ADR-001: the Windows Store project type adds its own folder, Generated Files\ and its intermediate folder to the
# default include path, so the executable states its include path whole rather than appending to the default.
NO_INHERITED_INCLUDES = {"OutpostCommander"}

# R14's table: the only projects with a packages.config, and the only packages each may list and import.
PACKAGES = {
  "OutpostCommander": {"Microsoft.Windows.SDK.BuildTools", "Microsoft.Windows.SDK.BuildTools.MSIX"},
  "NeuronCore": {"Microsoft.Native.Quic.MsQuic.Schannel"},
  "NeuronClient": {"WinPixEventRuntime"},
}

WINDOWS_MACROS = ("NOMINMAX", "WIN32_LEAN_AND_MEAN", "NOMCX", "NOSERVICE", "NOHELP")
WINDOWS_MACRO_OWNER = "NeuronCore/NeuronCore.h"
PIX_MACROS = ("USE_PIX", "USE_PIX_RETAIL", "PROFILE")


def configurations():
  return [(configuration, platform) for configuration in CONFIGURATIONS for platform in PLATFORMS]


def all_settings(_project):
  return {(configuration, platform): effective_settings(_project, configuration, platform)
          for configuration, platform in configurations()}


def grouped(_findings):
  """Folds the same message across configurations into one finding that names them."""
  by_message = {}
  for location, check, message, where in _findings:
    by_message.setdefault((location, check, message), []).append(where)
  return [Finding(location, check, f"{message} ({', '.join(f'{c}|{p}' for c, p in wheres)})")
          for (location, check, message), wheres in by_message.items()]


def check_platforms(_tree):
  findings = []
  solution = parse_xml(_tree.solution_path)
  named = {element.get("Name") for element in solution.iter("Platform") if element.get("Name")}
  if named != set(PLATFORMS):
    findings.append(Finding(_tree.solution_path.name, "platforms",
                            f"declares {sorted(named)}; the platforms are {sorted(PLATFORMS)} (ADR-003)"))
  expected = {f"{configuration}|{platform}" for configuration, platform in configurations()}
  for project in _tree.projects:
    declared = {element.get("Include") for element in project.vcxproj.iter(f"{MSBUILD_NS}ProjectConfiguration")}
    for missing in sorted(expected - declared):
      findings.append(Finding(project.vcxproj_path, "platforms", f"has no {missing} configuration (ADR-003)"))
    for extra in sorted(declared - expected):
      findings.append(Finding(project.vcxproj_path, "platforms", f"has a {extra} configuration; only x64 and ARM64, "
                                                                 f"Debug and Release, exist (ADR-003)"))
  return findings


def check_required_settings(_tree):
  raw = []
  for project in _tree.projects:
    for (configuration, platform), settings in all_settings(project).items():
      required = dict(REQUIRED_SETTINGS)
      required[INSTRUCTION_SET_KEY] = INSTRUCTION_SETS[platform]
      for key, value in required.items():
        stated = settings.get(key)
        if stated != value:
          text = "is not stated" if stated is None else f"is '{stated}'"
          raw.append((project.vcxproj_path, "settings", f"{key} {text}; it must be '{value}' (AGENTS.md §3, R16)",
                      (configuration, platform)))
  return grouped(raw)


def comparable(_key, _value, _configuration):
  """The value with what may legitimately differ taken out: _DEBUG or NDEBUG from the preprocessor definitions."""
  if _key.endswith(".PreprocessorDefinitions"):
    return ";".join(entry for entry in split_list(_value) if entry not in CONFIGURATION_DEFINES.values())
  return _value


def check_alignment(_tree):
  findings = []
  for project in _tree.projects:
    settings = all_settings(project)

    def compare(_left, _right, _exempt, _what):
      a, b = settings[_left], settings[_right]
      for key in sorted(set(a) | set(b)):
        if _exempt(key):
          continue
        left = comparable(key, a.get(key, "(not stated)"), _left[0])
        right = comparable(key, b.get(key, "(not stated)"), _right[0])
        if left != right:
          findings.append(Finding(project.vcxproj_path, "alignment",
                                  f"{key} is '{left}' in {'|'.join(_left)} but '{right}' in {'|'.join(_right)}; "
                                  f"{_what} (AGENTS.md §3)"))

    for platform in PLATFORMS:
      compare(("Debug", platform), ("Release", platform),
              lambda _key: _key.split(".")[-1].removesuffix(":inherits") in MAY_DIFFER_BY_CONFIGURATION,
              "Debug and Release differ only in optimisation")
    for configuration in CONFIGURATIONS:
      compare((configuration, "x64"), (configuration, "ARM64"),
              lambda _key: _key == INSTRUCTION_SET_KEY,
              "x64 and ARM64 differ only in the instruction set")

    for (configuration, platform), values in settings.items():
      defines = split_list(values.get("ClCompile.PreprocessorDefinitions", ""))
      wanted = CONFIGURATION_DEFINES[configuration]
      unwanted = [name for name in CONFIGURATION_DEFINES.values() if name != wanted and name in defines]
      if wanted not in defines or unwanted:
        findings.append(Finding(project.vcxproj_path, "alignment",
                                f"{configuration}|{platform} must define {wanted} and not "
                                f"{CONFIGURATION_DEFINES['Release' if wanted == '_DEBUG' else 'Debug']} (AGENTS.md §3)"))
  return findings


def defined_macros(_settings):
  names = set()
  for key, value in _settings.items():
    if key.endswith(".PreprocessorDefinitions"):
      names.update(entry.split("=")[0].strip() for entry in split_list(value))
    elif key.endswith(".AdditionalOptions"):
      names.update(re.findall(r"(?:^|\s)[/-]D\s*([A-Za-z_]\w*)", value))
  return names


def check_macros(_tree):
  raw = []
  for project in _tree.projects:
    for where, settings in all_settings(project).items():
      defined = defined_macros(settings)
      for name in WINDOWS_MACROS:
        if name in defined:
          raw.append((project.vcxproj_path, "macros", f"defines {name}; the Windows macro family is defined in "
                                                      f"{WINDOWS_MACRO_OWNER} and nowhere else (AGENTS.md §4)", where))
      for name in PIX_MACROS:
        if name in defined:
          raw.append((project.vcxproj_path, "macros", f"defines {name}, which turns PIX markers on in Release "
                                                      f"(ADR-005)", where))
  findings = grouped(raw)
  for path in _tree.cpp_files():
    code = strip_cpp(_tree.text(path))
    for match in re.finditer(r"^[ \t]*#[ \t]*define[ \t]+(\w+)", code, re.MULTILINE):
      name = match.group(1)
      where = f"{path}:{line_of(code, match.start())}"
      if name in WINDOWS_MACROS and path != WINDOWS_MACRO_OWNER:
        findings.append(Finding(where, "macros", f"defines {name}; only {WINDOWS_MACRO_OWNER} does (AGENTS.md §4)"))
      if name in PIX_MACROS:
        findings.append(Finding(where, "macros", f"defines {name}, which turns PIX markers on in Release (ADR-005)"))
  return findings


def check_include_paths(_tree):
  raw = []
  for project in _tree.projects:
    if project.name not in INCLUDE_PATHS:
      raw.append((project.vcxproj_path, "include-path",
                  "is not in ADR-002's include-path table; add its row there and to INCLUDE_PATHS here", None))
      continue
    expected = set(INCLUDE_PATHS[project.name])
    for where, settings in all_settings(project).items():
      for property_name in ("IncludePath", "ExternalIncludePath"):
        if property_name in settings:
          raw.append((project.vcxproj_path, "include-path", f"sets {property_name}; a project's include path is its "
                                                            f"AdditionalIncludeDirectories (ADR-002)", where))
      entries = split_list(settings.get("ClCompile.AdditionalIncludeDirectories", ""))
      if project.name in NO_INHERITED_INCLUDES and settings.get("ClCompile.AdditionalIncludeDirectories:inherits"):
        raw.append((project.vcxproj_path, "include-path", "inherits %(AdditionalIncludeDirectories), which the Windows "
                                                          "Store project type fills with its own folders (ADR-001)",
                    where))
      named = set()
      for entry in entries:
        match = re.fullmatch(r"\$\(SolutionDir\)([A-Za-z0-9]+)[\\/]?", entry)
        if match is None:
          raw.append((project.vcxproj_path, "include-path",
                      f"'{entry}' is not a $(SolutionDir)<Project> entry (AGENTS.md §3, ADR-002)", where))
        else:
          named.add(match.group(1))
      for extra in sorted(named - expected):
        raw.append((project.vcxproj_path, "include-path",
                    f"includes {extra}, which ADR-002 does not give {project.name}", where))
      for missing in sorted(expected - named):
        raw.append((project.vcxproj_path, "include-path",
                    f"does not include {missing}, which ADR-002 gives {project.name}", where))
  return grouped([(location, check, message, where or ("any", "any")) for location, check, message, where in raw])


def check_packages(_tree):
  findings = []
  for project in _tree.projects:
    allowed = PACKAGES.get(project.name, set())
    config_path = f"{project.directory}/packages.config"
    listed = {}
    if (_tree.root / config_path).is_file():
      if not allowed:
        findings.append(Finding(config_path, "packages", f"{project.name} is not in R14's table, so it has no packages"))
      for package in parse_xml(_tree.root / config_path).iter("package"):
        identifier, version = package.get("id", ""), package.get("version", "")
        listed[f"{identifier}.{version}".lower()] = identifier
        if identifier not in allowed:
          findings.append(Finding(config_path, "packages", f"lists {identifier}, which R14 does not give {project.name}"))
    elif allowed:
      findings.append(Finding(project.vcxproj_path, "packages", f"has no packages.config, but R14 gives it "
                                                                f"{', '.join(sorted(allowed))}"))
    text = (_tree.root / project.vcxproj_path).read_text(encoding="utf-8-sig")
    for folder in sorted(set(re.findall(r"packages[\\/]([A-Za-z0-9_.\-]+)[\\/]", text))):
      if folder.lower() not in listed:
        findings.append(Finding(project.vcxproj_path, "packages", f"references packages\\{folder}, which is not in its "
                                                                  f"packages.config at that version (R14)"))
  return findings


CHECKS = (check_solution, check_registration, check_missing_files, check_filters_match, check_filter_names,
          check_filter_declared, check_filter_split, check_location, check_extensions, check_file_names,
          check_shader_names, check_affixes, check_spelling, check_includes, check_com_ptr, check_header_filter,
          check_platforms, check_required_settings, check_alignment, check_macros, check_include_paths, check_packages)


def run_checks(_root):
  tree = Tree(_root)
  return sorted({finding for check in CHECKS for finding in check(tree)})


# ── Self-test: each check, shown to fire on a copy of the tree with one thing broken. ──────────────────────────


def edit(_root, _path, _old, _new):
  file = _root / _path
  text = file.read_text(encoding="utf-8-sig")
  if _old not in text:
    raise AssertionError(f"self-test fixture: '{_old}' is not in {_path}")
  file.write_text(text.replace(_old, _new, 1), encoding="utf-8")


def append(_root, _path, _xml):
  """Adds XML just before the document's closing </Project>, which is the last one: a ProjectReference carries a
  <Project> element of its own."""
  file = _root / _path
  head, tail = file.read_text(encoding="utf-8-sig").rsplit("</Project>", 1)
  file.write_text(f"{head}  {_xml}\n</Project>{tail}", encoding="utf-8")


def write(_root, _path, _text):
  (_root / _path).parent.mkdir(parents=True, exist_ok=True)
  (_root / _path).write_text(_text, encoding="utf-8")


def register(_root, _project, _kind, _include, _filter=None):
  """Adds a file item to a project and to its filters, as Visual Studio would."""
  body = f"<{_kind} Include=\"{_include}\" />"
  append(_root, f"{_project}/{_project}.vcxproj", f"<ItemGroup>{body}</ItemGroup>")
  if _filter is not None:
    body = f"<{_kind} Include=\"{_include}\"><Filter>{_filter}</Filter></{_kind}>"
  append(_root, f"{_project}/{_project}.vcxproj.filters", f"<ItemGroup>{body}</ItemGroup>")


def declare_filter(_root, _project, _name):
  append(_root, f"{_project}/{_project}.vcxproj.filters", f"<ItemGroup><Filter Include=\"{_name}\" /></ItemGroup>")


def add_source(_root, _project, _name, _text="#include \"pch.h\"\n", _filter=None):
  write(_root, f"{_project}/{_name}", _text)
  kind = "ClCompile" if _name.endswith(".cpp") else "ClInclude"
  register(_root, _project, kind, _name.replace("/", "\\"), _filter)


SELF_TEST_CASES = (
  ("solution", "a project in the solution that does not exist",
   lambda r: edit(r, "OutpostCommander.slnx", "<Configurations>",
                  "<Project Path=\"Ghost/Ghost.vcxproj\" />\n  <Configurations>")),
  ("solution", "a .vcxproj the solution does not name",
   lambda r: shutil.copy(r / "GameApp/GameApp.vcxproj", r / "GameApp/Stray.vcxproj")),
  ("registration", "a .cpp in a project folder that the project does not list",
   lambda r: write(r, "NeuronCore/Stray.cpp", "#include \"pch.h\"\n")),
  ("missing-file", "a project item whose file does not exist",
   lambda r: register(r, "NeuronCore", "ClCompile", "Ghost.cpp")),
  ("filters-match", "a file in the .filters that the .vcxproj does not list",
   lambda r: append(r, "GameApp/GameApp.vcxproj.filters", "<ItemGroup><None Include=\"pch.cpp\" /></ItemGroup>")),
  ("filter-name", "a Visual Studio default filter",
   lambda r: declare_filter(r, "GameApp", "Source Files")),
  ("filter-declared", "an item in a filter that is not declared",
   lambda r: add_source(r, "GameApp", "Widget.cpp", _filter="Nowhere")),
  ("filter-split", "a .h in a different filter from its .cpp",
   lambda r: (declare_filter(r, "GameApp", "Rendering"), declare_filter(r, "GameApp", "Input"),
              add_source(r, "GameApp", "Widget.h", "#pragma once\n", "Rendering"),
              add_source(r, "GameApp", "Widget.cpp", _filter="Input"))),
  ("location", "a header in a project subdirectory",
   lambda r: add_source(r, "GameApp", "Detail/Widget.h", "#pragma once\n")),
  ("location", "a shader outside Shader/",
   lambda r: (write(r, "GameApp/SkyVS.hlsl", "float4 main() : SV_Position { return 0; }\n"),
              register(r, "GameApp", "FXCompile", "SkyVS.hlsl"))),
  ("location", "a committed CompiledShader/ header",
   lambda r: (write(r, "GameApp/CompiledShader/SkyVS.h", "#pragma once\n"),
              subprocess.run(["git", "add", "-f", "GameApp/CompiledShader/SkyVS.h"], cwd=r, check=True,
                             capture_output=True))),
  ("location", "source outside every project",
   lambda r: write(r, "Loose.cpp", "int Loose();\n")),
  ("extension", "a .hpp",
   lambda r: add_source(r, "GameApp", "Widget.hpp", "#pragma once\n")),
  ("file-name", "a file name that is not PascalCase",
   lambda r: add_source(r, "GameApp", "widget_util.cpp")),
  ("shader-name", "a shader not named for its stage",
   lambda r: (write(r, "GameApp/Shader/Sky.hlsl", "float4 main() : SV_Position { return 0; }\n"),
              register(r, "GameApp", "FXCompile", "Shader\\Sky.hlsl"))),
  ("affix", "an interface named with an I prefix",
   lambda r: edit(r, "GameApp/GameApp.h", "#pragma once", "#pragma once\nclass ITransport\n{\n};")),
  ("affix", "a base class named with a Base suffix",
   lambda r: edit(r, "GameApp/GameApp.h", "#pragma once", "#pragma once\nstruct TransportBase : Other\n{\n};")),
  ("affix", "an alias with a _t suffix",
   lambda r: edit(r, "GameApp/GameApp.h", "#pragma once", "#pragma once\nusing tick_t = int;")),
  ("spelling", "an identifier with a British spelling",
   lambda r: edit(r, "GameApp/GameApp.h", "#pragma once", "#pragma once\nint TeamColour();")),
  ("include-climb", "an include that climbs into another project",
   lambda r: edit(r, "GameApp/GameApp.h", "#pragma once", "#pragma once\n#include \"../GameLogic/GameLogic.h\"")),
  ("wrl", "the WRL header",
   lambda r: edit(r, "GameApp/GameApp.h", "#pragma once", "#pragma once\n#include <wrl/client.h>")),
  ("wrl", "a Microsoft::WRL::ComPtr",
   lambda r: edit(r, "GameApp/GameApp.h", "#pragma once",
                  "#pragma once\nMicrosoft::WRL::ComPtr<ID3D12Device> g_device;")),
  ("header-filter", "a project the clang-tidy header filter does not name",
   lambda r: edit(r, ".clang-tidy", "|GameApp|", "|")),
  ("platforms", "a Win32 configuration in a project",
   lambda r: edit(r, "GameApp/GameApp.vcxproj", "<ProjectConfiguration Include=\"Debug|x64\">",
                  "<ProjectConfiguration Include=\"Debug|Win32\"><Configuration>Debug</Configuration>"
                  "<Platform>Win32</Platform></ProjectConfiguration>\n"
                  "    <ProjectConfiguration Include=\"Debug|x64\">")),
  ("platforms", "a Win32 platform in the solution",
   lambda r: edit(r, "OutpostCommander.slnx", "<Platform Name=\"x64\" />",
                  "<Platform Name=\"x64\" />\n    <Platform Name=\"Win32\" />")),
  ("settings", "warning level 3",
   lambda r: edit(r, "GameApp/GameApp.vcxproj", "<WarningLevel>Level4</WarningLevel>",
                  "<WarningLevel>Level3</WarningLevel>")),
  ("settings", "AVX rather than AVX2 on x64",
   lambda r: edit(r, "GameApp/GameApp.vcxproj", ">AdvancedVectorExtensions2<", ">AdvancedVectorExtensions<")),
  ("settings", "a floating-point model left to the default",
   lambda r: edit(r, "GameApp/GameApp.vcxproj", "<FloatingPointModel>Precise</FloatingPointModel>", "")),
  ("alignment", "an include directory removed from Release only",
   lambda r: append(r, "GameApp/GameApp.vcxproj",
                    "<ItemDefinitionGroup Condition=\"'$(Configuration)'=='Release'\"><ClCompile>"
                    "<AdditionalIncludeDirectories>$(SolutionDir)NeuronCore;$(SolutionDir)GameProtocol"
                    "</AdditionalIncludeDirectories></ClCompile></ItemDefinitionGroup>")),
  ("alignment", "a setting stated for x64 only",
   lambda r: append(r, "GameApp/GameApp.vcxproj",
                    "<ItemDefinitionGroup Condition=\"'$(Platform)'=='x64'\"><ClCompile><SDLCheck>false</SDLCheck>"
                    "</ClCompile></ItemDefinitionGroup>")),
  ("alignment", "_DEBUG defined in Release",
   lambda r: edit(r, "GameApp/GameApp.vcxproj", "NDEBUG;", "_DEBUG;")),
  ("macros", "NOMINMAX defined by a project",
   lambda r: edit(r, "GameApp/GameApp.vcxproj", "_DEBUG;", "_DEBUG;NOMINMAX;")),
  ("macros", "NOMINMAX defined by a header other than NeuronCore.h",
   lambda r: edit(r, "GameApp/GameApp.h", "#pragma once", "#pragma once\n#define NOMINMAX")),
  ("macros", "USE_PIX defined by a project",
   lambda r: edit(r, "NeuronClient/NeuronClient.vcxproj", "NDEBUG;", "NDEBUG;USE_PIX;")),
  ("include-path", "NeuronServer on GameApp's include path",
   lambda r: edit(r, "GameApp/GameApp.vcxproj", "$(SolutionDir)GameProtocol;",
                  "$(SolutionDir)GameProtocol;$(SolutionDir)NeuronServer;")),
  ("include-path", "the executable inheriting the default include path",
   lambda r: edit(r, "OutpostCommander/OutpostCommander.vcxproj", "$(SolutionDir)GameApp<",
                  "$(SolutionDir)GameApp;%(AdditionalIncludeDirectories)<")),
  ("include-path", "an include directory not spelled $(SolutionDir)<Project>",
   lambda r: edit(r, "GameApp/GameApp.vcxproj", "$(SolutionDir)NeuronCore;", "..\\NeuronCore;")),
  ("packages", "a packages.config in a project R14 gives no packages",
   lambda r: shutil.copy(r / "NeuronCore/packages.config", r / "GameApp/packages.config")),
  ("packages", "a package folder at a version packages.config does not list",
   lambda r: edit(r, "NeuronClient/NeuronClient.vcxproj", "WinPixEventRuntime.1.0.240308001\\bin",
                  "WinPixEventRuntime.1.0.230101001\\bin")),
)

# Things that look like findings and are not: each must stay silent.
SELF_TEST_QUIET = (
  ("comments and strings are exempt from R11",
   lambda r: edit(r, "GameApp/GameApp.h", "#pragma once",
                  "#pragma once\n// the colour of the team\ninline const char* g_name = \"colour\";")),
  ("a forward-declared SDK interface is not an R2 finding",
   lambda r: edit(r, "GameApp/GameApp.h", "#pragma once", "#pragma once\nstruct IDXGISwapChain4;")),
  ("a word that only starts like a stem is not an R11 finding",
   lambda r: edit(r, "GameApp/GameApp.h", "#pragma once", "#pragma once\nbool IsRealistic();\nint g_greyhoundCount;")),
  ("a digit separator does not start a character literal",
   lambda r: edit(r, "GameApp/GameApp.h", "#pragma once",
                  "#pragma once\ninline constexpr int ORE = 1'000; // it's\nint TeamColor();")),
)


def remove_tree(_path):
  # Git writes its object files read-only, and on Windows a read-only file cannot be deleted until that is undone.
  def make_writable(_function, _name, _info):
    Path(_name).chmod(0o700)
    _function(_name)

  if sys.version_info >= (3, 12):
    shutil.rmtree(_path, onexc=make_writable)
  else:
    shutil.rmtree(_path, onerror=make_writable)


def copy_tree(_destination):
  result = subprocess.run(["git", "ls-files", "-z", "--cached", "--others", "--exclude-standard"], cwd=REPO_ROOT,
                          capture_output=True, check=True)
  for name in result.stdout.decode().split("\0"):
    source = REPO_ROOT / name
    if not name or not source.is_file() or name.split("/")[0] in ("Art", "GameDesign", "Design", "Tools"):
      continue
    target = _destination / name
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, target)
  subprocess.run(["git", "init", "-q"], cwd=_destination, check=True)


def self_test():
  failures = 0
  with tempfile.TemporaryDirectory(ignore_cleanup_errors=True) as scratch:
    pristine = Path(scratch) / "pristine"
    copy_tree(pristine)
    baseline = set(run_checks(pristine))
    if baseline:
      print(f"note: the tree already has {len(baseline)} findings; each case is judged on what it adds")

    cases = [(check, description, mutate, True) for check, description, mutate in SELF_TEST_CASES]
    cases += [(None, description, mutate, False) for description, mutate in SELF_TEST_QUIET]
    for index, (check, description, mutate, should_fire) in enumerate(cases):
      case = Path(scratch) / f"case{index}"
      shutil.copytree(pristine, case)
      mutate(case)
      added = sorted(set(run_checks(case)) - baseline)
      fired = [finding for finding in added if finding.check == check] if should_fire else added
      ok = bool(fired) == should_fire
      failures += not ok
      label = f"[{check}] {description}" if should_fire else f"(quiet) {description}"
      print(f"{'pass' if ok else 'FAIL'}  {label}")
      if not ok or (should_fire and len(added) > len(fired)):
        for finding in added:
          print(f"        {finding}")
      remove_tree(case)
    remove_tree(pristine)
  print(f"\n{len(cases) - failures} of {len(cases)} self-test cases passed.")
  return 1 if failures else 0


def main():
  parser = argparse.ArgumentParser(description="Check the solution, the project files and the source tree's shape.")
  parser.add_argument("--self-test", action="store_true", help="show that each check fires on a broken copy of the tree")
  args = parser.parse_args()
  if args.self_test:
    return self_test()

  findings = run_checks(REPO_ROOT)
  for finding in findings:
    print(finding)
  if findings:
    print(f"\n{len(findings)} findings.")
    return 1
  print("Build shape: clean.")
  return 0


if __name__ == "__main__":
  sys.exit(main())

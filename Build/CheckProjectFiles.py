#!/usr/bin/env python3
"""The build-shape gate of AGENTS.md §1 and §2.

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


CHECKS = (check_solution, check_registration, check_missing_files, check_filters_match, check_filter_names,
          check_filter_declared, check_filter_split, check_location, check_extensions, check_file_names,
          check_shader_names, check_affixes, check_spelling, check_includes, check_com_ptr, check_header_filter)


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


def write(_root, _path, _text):
  (_root / _path).parent.mkdir(parents=True, exist_ok=True)
  (_root / _path).write_text(_text, encoding="utf-8")


def register(_root, _project, _kind, _include, _filter=None):
  """Adds a file item to a project and to its filters, as Visual Studio would."""
  body = f"<{_kind} Include=\"{_include}\" />"
  edit(_root, f"{_project}/{_project}.vcxproj", "</Project>", f"  <ItemGroup>{body}</ItemGroup>\n</Project>")
  if _filter is not None:
    body = f"<{_kind} Include=\"{_include}\"><Filter>{_filter}</Filter></{_kind}>"
  edit(_root, f"{_project}/{_project}.vcxproj.filters", "</Project>", f"  <ItemGroup>{body}</ItemGroup>\n</Project>")


def declare_filter(_root, _project, _name):
  edit(_root, f"{_project}/{_project}.vcxproj.filters", "</Project>",
       f"  <ItemGroup><Filter Include=\"{_name}\" /></ItemGroup>\n</Project>")


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
   lambda r: edit(r, "GameApp/GameApp.vcxproj.filters", "</Project>",
                  "  <ItemGroup><ClInclude Include=\"GameApp.h\" /><None Include=\"pch.cpp\" /></ItemGroup>\n</Project>")),
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

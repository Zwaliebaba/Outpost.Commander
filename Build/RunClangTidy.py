#!/usr/bin/env python3
"""The naming and lint gate of AGENTS.md §1.

Runs clang-tidy over every .cpp the solution compiles, against /.clang-tidy, and fails on any finding. Headers are
covered through .clang-tidy's HeaderFilterRegex: a header is checked when a .cpp that includes it is.

There is no CMake and so no compile_commands.json. Each translation unit's command line is built from its .vcxproj,
for Debug|x64 (the configuration CI builds), through clang's MSVC driver (--driver-mode=cl):

  - the include path, $(SolutionDir) resolved to the repository root;
  - the language standard, conformance mode, exception model and C runtime the project compiles with;
  - _DEBUG and the project's own preprocessor definitions, and UNICODE/_UNICODE for CharacterSet=Unicode;
  - the instruction set, so code behind __AVX2__ is what gets checked;
  - the precompiled header, read as a normal include: every .cpp includes pch.h first, so it is not named here.

The Windows SDK and the MSVC headers come from the INCLUDE variable, which clang's MSVC driver reads the way cl.exe
does. That is why this needs a Developer PowerShell locally; CI imports the same environment.

Which clang-tidy: the one on PATH, or --clang-tidy. CI installs CLANG_TIDY_VERSION from pip (see
.github/workflows/build.yml), and a different version may report differently; the version line is printed first.

Usage:
  python Build/RunClangTidy.py                     check every translation unit in the solution
  python Build/RunClangTidy.py NeuronCore/X.cpp    check only these translation units
  python Build/RunClangTidy.py --self-test         show that a misnamed member fails, in a .cpp and in a header, and
                                                   that a well-named one passes; needs no INCLUDE

Exit status: 0 when clean, 1 on any finding (or a failed self-test case), 2 when clang-tidy could not be run.
"""

import argparse
import concurrent.futures
import os
import re
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from CheckProjectFiles import MSBUILD_NS, REPO_ROOT, Tree, effective_settings, split_list  # noqa: E402

CONFIGURATION = "Debug"
PLATFORM = "x64"
TARGET = "x86_64-pc-windows-msvc"

LANGUAGE_STANDARDS = {"stdcpplatest": "/std:c++latest", "stdcpp23": "/std:c++23preview", "stdcpp20": "/std:c++20",
                      "stdcpp17": "/std:c++17"}
INSTRUCTION_SETS = {"AdvancedVectorExtensions2": "/arch:AVX2", "AdvancedVectorExtensions": "/arch:AVX"}
RUNTIME_LIBRARIES = {"MultiThreadedDebugDLL": "/MDd", "MultiThreadedDLL": "/MD", "MultiThreadedDebug": "/MTd",
                     "MultiThreaded": "/MT"}


def fail(_message):
  print(f"RunClangTidy: {_message}", file=sys.stderr)
  sys.exit(2)


def resolve_clang_tidy(_name):
  path = shutil.which(_name)
  if path is None:
    fail(f"'{_name}' was not found. Install it with 'python -m pip install clang-tidy==<CLANG_TIDY_VERSION>', or name "
         f"the binary with --clang-tidy.")
  result = subprocess.run([path, "--version"], capture_output=True, text=True)
  match = re.search(r"LLVM version (\S+)", result.stdout)
  if result.returncode != 0 or match is None:
    fail(f"'{path} --version' failed: {(result.stderr or result.stdout).strip()}")
  return path, match.group(1)


def compiler_arguments(_project, _root):
  """The clang-cl switches that match what MSBuild passes cl.exe for this project in Debug|x64."""
  settings = effective_settings(_project, CONFIGURATION, PLATFORM)
  arguments = ["--driver-mode=cl", f"--target={TARGET}", "/EHsc", "/permissive-", "/Zc:__cplusplus"]

  standard = settings.get("ClCompile.LanguageStandard", "stdcpplatest")
  if standard not in LANGUAGE_STANDARDS:
    fail(f"{_project.vcxproj_path}: no clang-cl switch is known for LanguageStandard '{standard}'.")
  arguments.append(LANGUAGE_STANDARDS[standard])

  # MSBuild's default runtime follows UseDebugLibraries; a stated RuntimeLibrary wins.
  default_runtime = "MultiThreadedDebugDLL" if settings.get("UseDebugLibraries") == "true" else "MultiThreadedDLL"
  arguments.append(RUNTIME_LIBRARIES[settings.get("ClCompile.RuntimeLibrary", default_runtime)])

  instruction_set = settings.get("ClCompile.EnableEnhancedInstructionSet")
  if instruction_set in INSTRUCTION_SETS:
    arguments.append(INSTRUCTION_SETS[instruction_set])

  defines = split_list(settings.get("ClCompile.PreprocessorDefinitions", ""))
  if settings.get("CharacterSet") == "Unicode":
    defines += ["UNICODE", "_UNICODE"]
  arguments += [f"/D{define}" for define in defines]

  # The solution's own folders are ordinary include directories. Anything else, such as the unit-test framework in the
  # Visual Studio install, is someone else's code and is included as a system directory, so its findings are not ours.
  solution_dir = str(_root) + os.sep
  for directory in split_list(settings.get("ClCompile.AdditionalIncludeDirectories", "")):
    if directory.startswith("%("):
      continue
    if directory.startswith("$(SolutionDir)"):
      arguments.append(f"/I{directory.replace('$(SolutionDir)', solution_dir)}")
      continue
    resolved = directory
    for variable in ("VCInstallDir", "VSInstallDir"):
      value = os.environ.get(variable.upper())
      if value:
        resolved = resolved.replace(f"$({variable})", value.rstrip("\\/") + os.sep)
    if "$(" in resolved:
      fail(f"{_project.vcxproj_path}: cannot resolve include directory '{directory}' outside a Developer environment.")
    arguments += ["/imsvc", resolved]

  for directory in package_include_directories(_project, _root):
    arguments += ["/imsvc", directory]

  for forced in split_list(settings.get("ClCompile.ForcedIncludeFiles", "")):
    arguments.append(f"/FI{forced}")
  return arguments


def package_include_directories(_project, _root):
  """The include directories a restored package's .targets adds to the project, such as pix3.h's for NeuronClient
  (ADR-005). MSBuild reads them from the import; this reads the same file. A package that is not restored yet adds
  nothing, and clang then reports the header it cannot find."""
  directories = []
  for element in _project.vcxproj.iter(f"{MSBUILD_NS}Import"):
    target = element.get("Project", "").replace("\\", "/")
    if not target.startswith("../packages/") or not target.endswith(".targets"):
      continue
    path = (_root / _project.directory / target).resolve()
    if not path.is_file():
      continue
    this_directory = str(path.parent) + os.sep
    for group in ET.parse(path).getroot().iter(f"{MSBUILD_NS}ItemDefinitionGroup"):
      for include in group.iter(f"{MSBUILD_NS}AdditionalIncludeDirectories"):
        for directory in split_list(include.text or ""):
          if directory.startswith("%("):
            continue
          resolved = directory.replace("$(MSBuildThisFileDirectory)", this_directory).replace("\\", "/")
          if "$(" not in resolved:
            directories.append(os.path.normpath(resolved))
  return directories


def translation_units(_tree):
  """(project, repository-relative .cpp) for every ClCompile the solution builds in Debug|x64."""
  units = []
  for project in _tree.projects:
    for group in project.vcxproj.iter(f"{MSBUILD_NS}ItemGroup"):
      for element in group.iter(f"{MSBUILD_NS}ClCompile"):
        include = element.get("Include")
        excluded = element.find(f"{MSBUILD_NS}ExcludedFromBuild")
        if not include or (excluded is not None and (excluded.text or "").strip() == "true"):
          continue
        path = f"{project.directory}/{include.replace(chr(92), '/')}"
        units.append((project, path))
  return units


def run_one(_exe, _root, _path, _arguments):
  command = [_exe, "--quiet", str(_root / _path), "--", *_arguments]
  result = subprocess.run(command, cwd=_root, capture_output=True, text=True, errors="replace")
  # clang-tidy prints "N warnings generated" and suppressed-header counts to stderr even with --quiet; the findings
  # themselves go to stdout.
  output = result.stdout.strip()
  noise = re.compile(r"^\d+ (warning|error)s? generated\.$|^Suppressed \d+ warnings|^Use -header-filter|^Error while "
                     r"processing")
  errors = "\n".join(line for line in result.stderr.splitlines() if line.strip() and not noise.match(line.strip()))
  return result.returncode, output, errors


DIAGNOSTIC = re.compile(r"^(.+?):(\d+):(\d+): (error|warning): ")


def split_diagnostics(_output):
  """One block per diagnostic: its header line, then the notes and source excerpt under it."""
  blocks = []
  for line in _output.splitlines():
    if DIAGNOSTIC.match(line) or not blocks:
      blocks.append([line])
    else:
      blocks[-1].append(line)
  return blocks


def check(_exe, _root, _units):
  """Runs every unit, then prints each distinct finding once: a header included by every .cpp would otherwise be
  reported once per translation unit."""
  failures = 0
  distinct = {}
  with concurrent.futures.ThreadPoolExecutor(max_workers=os.cpu_count() or 4) as pool:
    futures = {pool.submit(run_one, _exe, _root, path, arguments): path for path, arguments in _units}
    for future in concurrent.futures.as_completed(futures):
      path = futures[future]
      code, output, errors = future.result()
      if code == 0 and not output:
        print(f"clean     {path}")
        continue
      failures += 1
      print(f"FINDINGS  {path}")
      for block in split_diagnostics(output):
        distinct.setdefault(block[0].lower(), block)
      if errors:
        print("\n".join(f"    {line}" for line in errors.splitlines()))
  if distinct:
    print(f"\n{len(distinct)} distinct findings:\n")
    for key in sorted(distinct):
      print("\n".join(distinct[key]))
  return failures


# ── Self-test: the gate fires on a misnamed member, in a .cpp and through a header, and stays quiet otherwise. ────


PROBE_CLASS = "class Probe\n{{\npublic:\n  int Value() const {{ return {0}; }}\n\nprivate:\n  int {0} = 0;\n}};\n"
PROBE_BAD_CLASS = PROBE_CLASS.format("foo")
PROBE_GOOD_CLASS = PROBE_CLASS.format("m_value")
PROBE_USE = "#include \"Probe.h\"\n\nint UseProbe()\n{\n  return Probe{}.Value();\n}\n"
SELF_TEST_CASES = (
  ("a misnamed member in a .cpp", True,
   {"Probe.cpp": PROBE_BAD_CLASS + "\nint UseProbe()\n{\n  return Probe{}.Value();\n}\n"}),
  ("a misnamed member in a header the .cpp includes", True,
   {"Probe.h": "#pragma once\n\n" + PROBE_BAD_CLASS, "Probe.cpp": PROBE_USE}),
  ("a well-named class", False, {"Probe.h": "#pragma once\n\n" + PROBE_GOOD_CLASS, "Probe.cpp": PROBE_USE}),
)


def self_test(_exe):
  failures = 0
  # No INCLUDE is needed: the probes include nothing from the SDK. Only the switches are the real ones.
  arguments = ["--driver-mode=cl", f"--target={TARGET}", "/EHsc", "/std:c++latest", "/D_DEBUG", "/DUNICODE",
               "/D_UNICODE"]
  for description, should_fail, files in SELF_TEST_CASES:
    with tempfile.TemporaryDirectory() as scratch:
      root = Path(scratch)
      shutil.copy(REPO_ROOT / ".clang-tidy", root / ".clang-tidy")
      # A project folder name the header filter knows, so that the header case tests the real filter.
      for name, text in files.items():
        (root / "GameApp").mkdir(exist_ok=True)
        (root / "GameApp" / name).write_text(text, encoding="utf-8")
      code, output, errors = run_one(_exe, root, "GameApp/Probe.cpp", arguments)
      fired = code != 0 or bool(output)
      named = "foo" in output
      ok = fired == should_fail and (named or not should_fail)
      failures += not ok
      print(f"{'pass' if ok else 'FAIL'}  {'(fires)' if should_fail else '(quiet)'} {description}")
      if not ok or should_fail:
        for block in (output, errors):
          if block:
            print("\n".join(f"        {line}" for line in block.splitlines()[:6]))
  print(f"\n{len(SELF_TEST_CASES) - failures} of {len(SELF_TEST_CASES)} self-test cases passed.")
  return 1 if failures else 0


def main():
  parser = argparse.ArgumentParser(description="Run clang-tidy over every translation unit in the solution.")
  parser.add_argument("files", nargs="*", help="only these .cpp files, repository-relative")
  parser.add_argument("--clang-tidy", default="clang-tidy", metavar="EXE", help="the clang-tidy to run")
  parser.add_argument("--self-test", action="store_true", help="show the gate fires on a misnamed member")
  args = parser.parse_args()

  exe, version = resolve_clang_tidy(args.clang_tidy)
  print(f"clang-tidy {version} ({exe})")
  if args.self_test:
    return self_test(exe)

  if not os.environ.get("INCLUDE"):
    fail("INCLUDE is not set, so clang cannot see the Windows SDK or the MSVC headers. Run this from a Developer "
         "PowerShell, or import VsDevCmd's environment as CI does.")

  tree = Tree(REPO_ROOT)
  units = translation_units(tree)
  if args.files:
    wanted = {Path(name).as_posix() for name in args.files}
    units = [(project, path) for project, path in units if path in wanted]
    missing = wanted - {path for _, path in units}
    if missing:
      fail(f"not a translation unit of the solution: {', '.join(sorted(missing))}")
  if not units:
    fail("the solution compiles no .cpp files.")

  failures = check(exe, REPO_ROOT, [(path, compiler_arguments(project, REPO_ROOT)) for project, path in units])
  if failures:
    print(f"\n{failures} of {len(units)} translation units have findings.")
    return 1
  print(f"\n{len(units)} translation units checked, all clean.")
  return 0


if __name__ == "__main__":
  sys.exit(main())

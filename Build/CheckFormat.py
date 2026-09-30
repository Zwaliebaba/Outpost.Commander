#!/usr/bin/env python3
"""The formatting gate of AGENTS.md §4.

Runs clang-format over every .h and .cpp in the tree and fails when any of them would change. /.clang-format is the
authority for the layout; this script only asks clang-format whether each file already matches it.

Which files: those git tracks, plus new files it does not yet track but does not ignore, so a file you have not added
yet is checked before you push rather than first in CI. CompiledShader/ is build output (AGENTS.md §2) and is skipped
even if something has put it in the index, and so is vendored third-party source (ADR-007), which keeps its upstream
layout.

How a file is judged: clang-format writes the formatted file to stdout, and the file is clean when that is
byte-for-byte what is on disk. That is what `clang-format --dry-run` reports, with one difference: a clang-format that
fails outright, on a broken /.clang-format or a file it cannot parse, is an error here rather than a formatting finding
or, worse, a pass.

Which clang-format: CI pins 18.1.3, because the output changes between releases (/.clang-format says why). A run on any
other version still checks, but says it is not the pinned one, so a disagreement with CI can be traced to the version
before anyone argues about the code. On Windows, Visual Studio's bundled clang-format is 22.

Usage:
  python Build/CheckFormat.py                                  check the tree
  python Build/CheckFormat.py --fix                            rewrite the files that are not clean
  python Build/CheckFormat.py --clang-format clang-format-18   use this binary (CI passes clang-format-18)

Exit status: 0 when every file is clean (or, with --fix, clean afterwards), 1 when a file is not, 2 when clang-format
or git could not be run.
"""

import argparse
import concurrent.futures
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from CheckProjectFiles import VENDORED_FILES  # noqa: E402

REPO_ROOT = Path(__file__).resolve().parents[1]
PINNED_VERSION = "18.1.3"
EXTENSIONS = (".h", ".cpp")
SKIPPED_DIRECTORIES = ("CompiledShader",)


def fail(message):
  print(f"CheckFormat: {message}", file=sys.stderr)
  sys.exit(2)


def resolve_clang_format(name):
  path = shutil.which(name)
  if path is None:
    fail(f"'{name}' was not found. Install clang-format {PINNED_VERSION}, or name the binary with --clang-format.")
  return path


def clang_format_version(exe):
  result = subprocess.run([exe, "--version"], capture_output=True, text=True)
  if result.returncode != 0:
    fail(f"'{exe} --version' failed: {result.stderr.strip()}")
  match = re.search(r"clang-format version (\d+\.\d+\.\d+)", result.stdout)
  if match is None:
    fail(f"cannot read a version from '{result.stdout.strip()}'.")
  return match.group(1)


def source_files():
  # --others --exclude-standard adds what is new and not ignored; -z keeps names with spaces intact.
  result = subprocess.run(["git", "ls-files", "-z", "--cached", "--others", "--exclude-standard"], cwd=REPO_ROOT,
                          capture_output=True)
  if result.returncode != 0:
    fail(f"'git ls-files' failed: {result.stderr.decode(errors='replace').strip()}")
  files = set()
  for name in result.stdout.decode().split("\0"):
    if not name.endswith(EXTENSIONS):
      continue
    relative = Path(name)
    if any(part in SKIPPED_DIRECTORIES for part in relative.parts) or name in VENDORED_FILES:
      continue
    # Deleted from the working tree but still in the index: nothing to format.
    if (REPO_ROOT / relative).is_file():
      files.add(relative)
  return sorted(files)


def check(exe, relative):
  """Returns (formatted, error): formatted is None when the file is already clean, error is None unless clang-format
  itself failed."""
  path = REPO_ROOT / relative
  original = path.read_bytes()
  # The path is passed rather than piped in, so clang-format finds /.clang-format by walking up from the file.
  result = subprocess.run([exe, "--style=file", str(path)], cwd=REPO_ROOT, capture_output=True)
  if result.returncode != 0:
    return None, result.stderr.decode(errors="replace").strip() or f"exit status {result.returncode}"
  return (None if result.stdout == original else result.stdout), None


def main():
  parser = argparse.ArgumentParser(description="Check every .h and .cpp in the tree against /.clang-format.")
  parser.add_argument("--fix", action="store_true", help="rewrite the files that are not clean")
  parser.add_argument("--clang-format", default="clang-format", metavar="EXE", help="the clang-format to run")
  args = parser.parse_args()

  exe = resolve_clang_format(args.clang_format)
  version = clang_format_version(exe)
  if version == PINNED_VERSION:
    print(f"clang-format {version} ({exe})")
  else:
    print(f"clang-format {version} ({exe}) -- NOT the pinned {PINNED_VERSION}. CI's verdict is the one that counts; "
          f"a difference from it may be the version, not the code.")

  files = source_files()
  with concurrent.futures.ThreadPoolExecutor(max_workers=os.cpu_count() or 4) as pool:
    results = dict(zip(files, pool.map(lambda file: check(exe, file), files)))

  errors = {file: error for file, (_, error) in results.items() if error is not None}
  dirty = {file: formatted for file, (formatted, _) in results.items() if formatted is not None}

  for file, error in errors.items():
    print(f"ERROR     {file.as_posix()}: {error}")

  if args.fix:
    for file, formatted in dirty.items():
      (REPO_ROOT / file).write_bytes(formatted)
      print(f"fixed     {file.as_posix()}")
    # Clean now, unless clang-format is not idempotent on a file, which is worth hearing about rather than assuming.
    dirty = {file: formatted for file in dirty for formatted, _ in [check(exe, file)] if formatted is not None}

  for file in dirty:
    print(f"unclean   {file.as_posix()}")

  if errors:
    print(f"\n{len(errors)} of {len(files)} files could not be formatted.")
    return 2
  if dirty:
    hint = "" if args.fix else " Run 'python Build/CheckFormat.py --fix' and commit the result."
    print(f"\n{len(dirty)} of {len(files)} files are not formatted.{hint}")
    return 1
  print(f"{len(files)} files checked, all clean.")
  return 0


if __name__ == "__main__":
  sys.exit(main())

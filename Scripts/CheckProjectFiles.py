#!/usr/bin/env python3
"""Assert AGENTS.md section 3's table over every project file, without a build.

CI gates Debug|x64 and the format check and nothing else, so a Release that lost a setting,
an ARM64 that assumes an x86 intrinsic switch, or a Debug group that quietly grew a setting
that is not about optimisation all reach main green (AGENTS.md section 6). Compiling three
more pairs costs minutes; reading the XML costs under a second and catches the drift that
those pairs would have caught. It cannot catch a break in code -- only in configuration.

    python3 Scripts/CheckProjectFiles.py          # from the repository root

Exit 0 when every project agrees with section 3, 1 otherwise, naming file and setting.
"""
import argparse
import pathlib
import sys
import xml.etree.ElementTree as ET

NS = "{http://schemas.microsoft.com/developer/msbuild/2003}"
ROOT = pathlib.Path(__file__).resolve().parent.parent

TOOLSET = "v145"
SDK = "10.0.26100.0"
PLATFORMS = {"x64", "ARM64"}
CONFIGURATIONS = {"Debug", "Release"}

# Section 3: the settings that are ABOUT optimisation, and may therefore be conditioned on the
# configuration. Anything else found inside a configuration-conditioned group is the rule being
# broken -- that is the whole point of the check, and it is the half no build would ever fail on.
BY_CONFIGURATION = {
    "Debug": {"UseDebugLibraries": "true", "LinkIncremental": "true", "Optimization": "Disabled"},
    "Release": {"UseDebugLibraries": "false", "WholeProgramOptimization": "true",
                "LinkIncremental": "false", "Optimization": "MaxSpeed",
                "FunctionLevelLinking": "true", "IntrinsicFunctions": "true",
                "EnableCOMDATFolding": "true", "OptimizeReferences": "true"},
}
# Linker optimisations. A StaticLibrary never invokes the linker, so these are required only of an
# Application or a DynamicLibrary -- NeuronClient and GameClient DO carry a <Link> section, holding
# GenerateWindowsMetadata and nothing else (section 3), so the presence of one decides nothing.
LINK_ONLY = {"EnableCOMDATFolding", "OptimizeReferences"}
LINKS = {"Application", "DynamicLibrary"}
DEFINE = {"Debug": "_DEBUG", "Release": "NDEBUG"}
ALLOWED = set(BY_CONFIGURATION["Debug"]) | set(BY_CONFIGURATION["Release"]) | {"PreprocessorDefinitions"}

# Written once in an unconditioned ItemDefinitionGroup, so there is no second copy to drift.
INVARIANT = {"WarningLevel": "Level4", "TreatWarningAsError": "true", "ConformanceMode": "true",
             "LanguageStandard": "stdcpplatest", "FloatingPointModel": "Precise", "SDLCheck": "true"}

# The one compiler setting that belongs to the platform rather than the configuration.
BY_PLATFORM = {"x64": "AdvancedVectorExtensions2", "ARM64": "NotSet"}

faults = []


def fault(path, message):
    faults.append(f"{path.relative_to(ROOT)}: {message}")


def condition_of(element, macro):
    """The single value a Condition pins $(macro) to, or None."""
    condition = element.get("Condition", "")
    for value in (CONFIGURATIONS if macro == "Configuration" else PLATFORMS):
        if f"'$({macro})'=='{value}'" in condition.replace(" ", ""):
            return value
    return None


def leaves(group):
    """Settings in a PropertyGroup or an ItemDefinitionGroup, flattening ClCompile/Link."""
    for child in group:
        tag = child.tag.replace(NS, "")
        if tag in ("ClCompile", "Link", "Lib", "Midl", "Manifest"):
            for leaf in child:
                yield leaf.tag.replace(NS, ""), (leaf.text or "").strip()
        else:
            yield tag, (child.text or "").strip()


def check_project(path):
    root = ET.parse(path).getroot()
    groups = list(root.iter(NS + "PropertyGroup")) + list(root.iter(NS + "ItemDefinitionGroup"))

    # 1. Exactly the four pairs, and no 32-bit anything.
    pairs = {c.get("Include") for c in root.iter(NS + "ProjectConfiguration")}
    expected = {f"{c}|{p}" for c in CONFIGURATIONS for p in PLATFORMS}
    if pairs != expected:
        fault(path, f"configurations are {sorted(pairs)}, expected {sorted(expected)}")

    # 2. The toolset and the SDK are pinned, and the pin is the runner's.
    for tag, want in ((NS + "PlatformToolset", TOOLSET), (NS + "WindowsTargetPlatformVersion", SDK)):
        got = {(e.text or "").strip() for e in root.iter(tag)}
        if got != {want}:
            fault(path, f"{tag.replace(NS, '')} is {sorted(got) or 'unset'}, expected {want}")

    # 3. Nothing but section 3's rows may be conditioned on the configuration, and each row's
    #    value must be the one in the table.
    seen = {"Debug": set(), "Release": set()}
    kinds = {(e.text or "").strip() for e in root.iter(NS + "ConfigurationType")}
    links = bool(kinds & LINKS)
    for group in groups:
        configuration = condition_of(group, "Configuration")
        if configuration is None:
            continue
        table = BY_CONFIGURATION[configuration]
        other = BY_CONFIGURATION["Release" if configuration == "Debug" else "Debug"]
        for name, value in leaves(group):
            if name == "PreprocessorDefinitions":
                if DEFINE[configuration] not in value:
                    fault(path, f"{configuration} does not define {DEFINE[configuration]}")
                seen[configuration].add(name)
                continue
            if name not in ALLOWED:
                fault(path, f"{configuration} group sets {name}, which is not about optimisation "
                            f"-- section 3 says it belongs in the unconditioned group")
            elif name not in table:
                fault(path, f"{configuration} sets {name}, which section 3 gives only to "
                            f"{'Release' if name in other else 'the other configuration'}")
            elif value != table[name]:
                fault(path, f"{configuration} {name} is {value!r}, expected {table[name]!r}")
            seen[configuration].add(name)

    for configuration, table in BY_CONFIGURATION.items():
        missing = set(table) | {"PreprocessorDefinitions"}
        if not links:
            missing -= LINK_ONLY
        missing -= seen[configuration]
        for name in sorted(missing):
            fault(path, f"{configuration} never sets {name}")

    # 4. The invariants are stated once, unconditioned, so Debug and Release cannot disagree.
    unconditioned = {}
    for group in root.iter(NS + "ItemDefinitionGroup"):
        if not group.get("Condition"):
            unconditioned.update(dict(leaves(group)))
    for name, want in INVARIANT.items():
        if unconditioned.get(name) != want:
            fault(path, f"{name} is {unconditioned.get(name)!r} in the unconditioned group, "
                        f"expected {want!r}")

    # 5. EnableEnhancedInstructionSet is stated per platform rather than inherited: an MSVC
    #    default is not a decision, and this is the one setting an ARM64 break hides behind.
    for platform, want in BY_PLATFORM.items():
        got = [v for group in groups if condition_of(group, "Platform") == platform
               for n, v in leaves(group) if n == "EnableEnhancedInstructionSet"]
        if got != [want]:
            fault(path, f"EnableEnhancedInstructionSet for {platform} is {got or 'unstated'}, "
                        f"expected [{want!r}]")


def check_shared_items(path):
    """A .vcxitems is compiled with the importing project's settings and has none of its own."""
    root = ET.parse(path).getroot()
    for group in list(root.iter(NS + "PropertyGroup")) + list(root.iter(NS + "ItemDefinitionGroup")):
        if condition_of(group, "Configuration"):
            fault(path, "a shared-items project conditions a setting on the configuration; it is "
                        "compiled with the importing project's settings and must carry none")


def check_packages():
    """R14 and section 2: one package, and every packages.config pins the same version."""
    pins = {}
    for path in sorted(ROOT.rglob("packages.config")):
        for package in ET.parse(path).getroot():
            pins.setdefault(package.get("id"), {}).setdefault(package.get("version"), []).append(
                str(path.relative_to(ROOT)))
    for identity, versions in pins.items():
        if len(versions) > 1:
            faults.append(f"packages.config: {identity} is pinned to {sorted(versions)} -- "
                          "a restore that fetches two copies links whichever import was written last")
    extra = set(pins) - {"Microsoft.Windows.CppWinRT"}
    if extra:
        faults.append(f"packages.config: {sorted(extra)} is a second NuGet package (R14) -- "
                      "a decision, not a convenience")


def main():
    global ROOT
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=pathlib.Path, default=ROOT,
                        help="tree to check; defaults to this script's repository")
    ROOT = parser.parse_args().root.resolve()
    projects = sorted(ROOT.rglob("*.vcxproj"))
    items = sorted(ROOT.rglob("*.vcxitems"))
    if not projects:
        print("No .vcxproj found. Run this from the repository root.")
        return 1
    for path in projects:
        check_project(path)
    for path in items:
        check_shared_items(path)
    check_packages()

    for line in faults:
        print(f"  {line}")
    print(f"\n{len(faults)} fault(s) across {len(projects)} projects and {len(items)} shared-items "
          f"projects, against AGENTS.md section 3.")
    return 1 if faults else 0


if __name__ == "__main__":
    sys.exit(main())

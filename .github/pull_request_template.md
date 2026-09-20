<!--
  AGENTS.md §7 is the checklist this mirrors. Delete the parts that do not apply; do not delete
  the parts that do because they are inconvenient.
-->

## What this changes

<!-- One paragraph. What the change does, not what you did to make it. -->

## Why

<!-- The problem, or the decision this implements. If this change IS a standing decision — a file
     format, a wire protocol, a subsystem's shape, an exception to a rule — write it into AGENTS.md
     in this same PR (§6). -->

## How it was verified

<!-- Be specific and be honest. "Builds clean, not run" and "builds and runs" are different
     claims. Say which configurations you actually built. -->

- [ ] Builds clean through the solution: `Debug|x64`
- [ ] Every test suite runs and passes
- [ ] The other three pairs build: `Release|x64`, `Debug|ARM64`, `Release|ARM64` — **CI builds none
      of these**, so an unticked box here means nobody checked, not that nothing was wrong
- [ ] `clang-format --dry-run --Werror` over the files touched, on the pinned version
- [ ] Ran the executable (**required** if this touches rendering, input, audio or presentation —
      for the client that means deployed as a package and launched)

<!-- Nothing in this tree checks naming: there is no script that drives .clang-tidy yet
     (AGENTS.md §1). Read your own diff against the table. -->

## Conformance

- [ ] Naming follows AGENTS.md §1 — `_` on parameters, `m_` on class state, `UPPER_CASE`
      constants, `PascalCase` enumerators, no `I`/`C`/`Base` affixes
- [ ] New, removed or moved files are in the `.vcxproj` or `.vcxitems` **and** the `.filters`
- [ ] No `.filters` gained a `Source Files` or `Header Files` filter — Visual Studio adds them back
      on its own, and filters here are functional only (§2)
- [ ] A new `.vcxitems` import was added to exactly one project per link closure (§2)
- [ ] A new library has a master include, a `pch.h` that includes it, a suite under `Tests/`, and a
      name in `.clang-tidy`'s `HeaderFilterRegex`
- [ ] Debug and Release still differ in exactly the rows of AGENTS.md §3's table and nothing else
- [ ] No warning silenced, no `ConformanceMode`/`LanguageStandard`/`WarningLevel` changed
- [ ] No new third-party dependency, and no second NuGet package (R14); every `packages.config`
      still pins the same version
- [ ] Only the lines the task required were changed

## Anything you had to bend

<!-- Rules you deviated from and why, assumptions you made, things you noticed but left alone.
     An empty section here is a claim; make sure it is true. -->

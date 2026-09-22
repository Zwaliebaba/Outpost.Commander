<!--
  AGENTS.md §7 is the checklist this mirrors, section for section. Delete the parts that do not
  apply; do not delete the parts that do because they are inconvenient. A box left unticked is a
  claim that nobody checked — which is useful — so say which it is rather than leaving it blank.
-->

## What this changes

<!-- One paragraph. What the change does, not what you did to make it. -->

## Why

<!-- The problem, or the decision this implements. If this change IS a standing decision — a file
     format, a wire protocol, a subsystem's shape, an exception to a rule — write it down in this
     same PR (§6), in exactly one of: a rule in AGENTS.md citing its source, an ADR under
     Design/ADR/, a row on Design/OpenQuestions.md, or nowhere. -->

## How it was verified

<!-- Be specific and be honest. "Builds clean, not run" and "builds and runs" are different
     claims. Say which configurations you actually built. -->

**The gates, which are seconds and are what CI runs:**

- [ ] `python3 Scripts/CheckProjectFiles.py` — AGENTS.md §3's table over every project file
- [ ] `python3 Scripts/CheckDeterminism.py` — R16 over `GameCore` and `GameLogic`
- [ ] `python3 Scripts/CheckDesign.py` — figures, citations and links across `Design/`
- [ ] `clang-format --dry-run --Werror` over the files touched, on the pinned version

**The build, which is slower and which CI only half covers:**

- [ ] Builds clean through the solution: `Debug|x64`
- [ ] Every test suite runs and passes
- [ ] The other three pairs build: `Release|x64`, `Debug|ARM64`, `Release|ARM64` — **CI builds none
      of these**, so an unticked box here means nobody checked, not that nothing was wrong
- [ ] Ran the executable (**required** if this touches rendering, input, audio or presentation —
      for the client that means deployed as a package and launched)

<!-- Naming is still review's problem. Scripts/RunClangTidy.ps1 drives .clang-tidy over the tree
     and CI runs it, but it REPORTS and does not gate: clang is not MSVC, /std:c++latest is ahead
     of what clang implements, and a parse error there is a clang limitation rather than a defect
     (AGENTS.md §1). Read its output, and read your own diff against §1's table. -->

## Conformance

- [ ] Naming follows AGENTS.md §1 — `_` on parameters, `m_` on class state, `UPPER_CASE`
      constants, `PascalCase` enumerators, no `I`/`C`/`Base` affixes
- [ ] Only the lines the task required were changed; no reformatting, no drive-by fixes
- [ ] No new third-party dependency, and no second NuGet package (R14); every `packages.config`
      still pins the same version

**If you touched `Design/` or `AGENTS.md`:**

- [ ] `Scripts/CheckDesign.py` is clean — figures agree across every document that states them,
      citations resolve, links resolve
- [ ] Each decision went to exactly one of: a rule in AGENTS.md citing its source, an ADR,
      `Design/OpenQuestions.md`, or nowhere

**If you added, removed or moved a file:**

- [ ] It is in the owning `.vcxproj` or `.vcxitems` **and** the `.filters`
- [ ] No `.filters` gained a `Source Files` or `Header Files` filter — Visual Studio adds them back
      on its own, and filters here are functional only (§2)

**If you added a library:**

- [ ] It has a master include, a `pch.h` that includes it, a suite under `Tests/`, and **a name in
      `.clang-tidy`'s `HeaderFilterRegex`** — without that its headers are silently unchecked
- [ ] A new `.vcxitems` import was added to exactly one project per link closure (§2)

**If you touched a project file:**

- [ ] Debug and Release still differ in exactly the rows of AGENTS.md §3's table and nothing else —
      `Scripts/CheckProjectFiles.py` asserts this, so run it rather than reading
- [ ] No warning silenced, no `ConformanceMode`/`LanguageStandard`/`WarningLevel`/
      `TreatWarningAsError` changed

**If you touched the simulation or the wire format:**

- [ ] No third coordinate reached either (R22); no map transmitted that the seed already derives
      (R23); no ship stat baked onto a type rather than derived (R24)
- [ ] Nothing in the client links the simulation (R19), and nothing was put in an executable that a
      suite could have covered (R20)
- [ ] `Scripts/CheckDeterminism.py` is clean **including `--review`**, and the judgment calls it
      reports were answered rather than dismissed — a sort's comparator is a total order on entity
      identity, a draw comes from the match's engine
- [ ] **If a datagram moved, `Scripts/DatagramBudget.py` was run** and its figures — not estimates —
      are in this PR and in `Design/TechnicalDesign.md` §4

## Anything you had to bend

<!-- Rules you deviated from and why, assumptions you made, things you noticed but left alone.
     An empty section here is a claim; make sure it is true. -->

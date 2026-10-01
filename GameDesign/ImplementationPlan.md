# Outpost Commander — MVP Implementation Plan

Status: **draft for review** · 2026-09-30 · Derived from [the MVP design](OutpostCommander-MVP.md)

The MVP design says *what* is built, AGENTS.md says *how* code is written, and `Design/ADR/` records the engineering decisions. This plan says **in what order**, as a queue of tasks an agent can pick up one at a time. It is a work queue, not an authority: where it disagrees with the design, AGENTS.md or an ADR, those win and this plan gets fixed.

---

## How an agent uses this plan

1. **Read AGENTS.md first, then the design sections the task names.** Every task assumes both.
2. **Take the lowest-numbered task whose status is `todo`, whose dependencies are `done`, and whose gate is clear** (see the table below). Deliver one milestone per PR, in the order under Gates; a milestone too large for one PR is split in this file first.
3. **A gate is an owner decision.** If a task's gate is open, do not guess. Write the options with their costs, ask the owner, record the answer where the task says (the design document or an ADR), and only then build. The same applies to any design question this plan does not answer: AGENTS.md says the answer is written down before the code is.
4. **Branch off `main`, fill in the PR template, and get CI green.** CI builds Debug|x64 only, and it runs the three checkers (phase 0).
5. **Know what you cannot verify.** An agent in a cloud container has no Windows, no MSBuild and no GPU: CI is its only build. Anything touching rendering, input, audio or presentation must also be **run by the owner** (AGENTS.md §3). Such tasks say *Owner run*. Say plainly in the PR that it was not run, and the task stays `in review` until the owner has run it.
6. **When the PR merges, update this file in the same PR or the next one:** status `done`, a link to the PR, and anything learned that changes a later task.
7. **ADRs.** A task marked *ADR* takes an engineering decision. Write the ADR in the same PR, numbered after the highest existing one. Until the MVP is done, ADRs are edited in place (AGENTS.md §6).

Namespaces: the engine is `Neuron`, and the game layers (GameProtocol, GameLogic, GameApp, Opponent) share `Outpost` (R9, gate G4, decided by the owner on 2026-09-30).

---

## Task board

| Task | Title | Depends on | Gate | Status |
|---|---|---|---|---|
| 0.1 | `Build/CheckFormat.py` | — | — | done, [#26](https://github.com/Zwaliebaba/Outpost.Commander/pull/26) |
| 0.2 | `Build/CheckProjectFiles.py`: tree shape and names | — | — | done, [#26](https://github.com/Zwaliebaba/Outpost.Commander/pull/26) |
| 0.3 | `Build/CheckProjectFiles.py`: build settings and include paths | 0.2 | — | done, [#26](https://github.com/Zwaliebaba/Outpost.Commander/pull/26) |
| 0.4 | `Build/RunClangTidy.py` | 0.3 | — | done, [#26](https://github.com/Zwaliebaba/Outpost.Commander/pull/26) |
| 0.5 | `GameLogicTests` with `SuiteSmoke` | 0.3 | — | done, [#26](https://github.com/Zwaliebaba/Outpost.Commander/pull/26) |
| 1.1 | A Win32 window and message loop | 0.3 | — | done, [#27](https://github.com/Zwaliebaba/Outpost.Commander/pull/27), run by the owner |
| 1.2 | D3D12 device and flip-model swap chain | 1.1 | — | done, [#29](https://github.com/Zwaliebaba/Outpost.Commander/pull/29), run by the owner |
| 1.3 | Mesh loading, with scale and forward axis as data | 1.2 | G2 decided (`.cmo`, meshes converted); provenance before shipping | done, [23f0c1f](https://github.com/Zwaliebaba/Outpost.Commander/commit/23f0c1f), run by the owner |
| 1.4 | Flat-lit, team-coloured shading | 1.3 | — | done, [23f0c1f](https://github.com/Zwaliebaba/Outpost.Commander/commit/23f0c1f), run by the owner |
| 1.5 | The RTS camera | 1.4 | G3 zoom limits (provisional in use) | done, [23f0c1f](https://github.com/Zwaliebaba/Outpost.Commander/commit/23f0c1f), run by the owner |
| 1.6 | Milestone 1 review | 1.5 | — | done, run by the owner |
| 2.1 | Protocol types: IDs, commands, snapshots, `Transport` | 0.5 | — | done, [#30](https://github.com/Zwaliebaba/Outpost.Commander/pull/30) |
| 2.2 | Tick host, seeded PRNG, in-process server | 2.1, 3.1 | — | done, [#32](https://github.com/Zwaliebaba/Outpost.Commander/pull/32) |
| 2.3 | The map as data | 2.2 | — | done, [#33](https://github.com/Zwaliebaba/Outpost.Commander/pull/33), layout confirmed by the owner |
| 2.4 | Movement, pathing and formations | 2.3 | G5 footprint radii (provisional in use) | done, [#34](https://github.com/Zwaliebaba/Outpost.Commander/pull/34) |
| 2.5 | Rendering from interpolated snapshots | 1.6, 2.4 | — | done, [bcaead5](https://github.com/Zwaliebaba/Outpost.Commander/commit/bcaead5), run by the owner |
| 2.6 | Selection, orders and control groups | 2.5 | — | done, [bcaead5](https://github.com/Zwaliebaba/Outpost.Commander/commit/bcaead5), run by the owner |
| 2.7 | Measure Q5 and the tick half of Q4 | 2.6 | — | done, [0d90731](https://github.com/Zwaliebaba/Outpost.Commander/commit/0d90731), run by the owner |
| 3.1 | Tuning data file, loaded by the game and the model | 0.5 | — | done, [#31](https://github.com/Zwaliebaba/Outpost.Commander/pull/31) |
| 3.2 | Components and designs | 3.1 | — | in review, [#36](https://github.com/Zwaliebaba/Outpost.Commander/pull/36) |
| 3.3 | Combat rules | 3.2, 2.4 | — | in review, [#36](https://github.com/Zwaliebaba/Outpost.Commander/pull/36) |
| 3.4 | The Q2 check as headless battles | 3.3, 0.5 | — | in review, [#36](https://github.com/Zwaliebaba/Outpost.Commander/pull/36); the check fails (design §12) |
| 3.5 | Combat effects | 3.3, 2.5 | — | in review, [#36](https://github.com/Zwaliebaba/Outpost.Commander/pull/36), awaiting the owner's run |
| 3.6 | In-game UI drawing and a first HUD | 2.6 | G7 decided | in review, [#36](https://github.com/Zwaliebaba/Outpost.Commander/pull/36), awaiting the owner's run |
| 3.7 | Q4 stress scene and measurement | 3.5, 3.6 | — | in review, [#36](https://github.com/Zwaliebaba/Outpost.Commander/pull/36), awaiting the owner's measurement |
| 4.1 | Ore, Mining Rigs and costs | 3.2, 2.3 | — | todo |
| 4.2 | Structures, placement and Constructors | 4.1 | G8 Constructor numbers | todo |
| 4.3 | Shipyard and Command Station queues | 4.2 | — | todo |
| 4.4 | The Defence gun and structure armour | 4.2, 3.3 | — | todo |
| 4.5 | The full HUD and the minimap | 4.3, 3.6 | — | todo |
| 4.6 | Hand checks of the structure numbers | 4.4 | — | todo |
| 5.1 | Research | 4.3 | — | todo |
| 5.2 | The ship designer in the Shipyard panel | 5.1, 4.5 | — | todo |
| 5.3 | The Missile Rack, in the game and in the model | 5.1, 3.4 | G5 footprint radii | todo |
| 6.1 | The AI player | 5.2 | G9 attack-group threshold | todo |
| 6.2 | Win, lose and the menu | 6.1 | — | todo |
| 6.3 | Q1 and Q3 playtests | 6.2 | — | todo |

## Gates

Each gate is an owner decision. Most are already listed as open in design §15.

**PRs from here are one per milestone** (owner, 2026-09-30). Milestone 2's later tasks need milestone 1, so the order is: 2.4 on its own, then milestone 1 (1.3–1.6), then the rest of milestone 2 (2.5–2.7).

| Gate | Decision | Where it is recorded | Blocks |
|---|---|---|---|
| G1 | The renderer's shape: frames in flight, vsync and tearing, window style (windowed, borderless), resize behaviour, device-removed handling, and which failed `HRESULT`s the renderer handles instead of letting `winrt::check_hresult` throw (R12). Exclusive full screen is not ruled out by ADR-001, but it needs a reason. **Decided on 2026-09-30:** borderless full screen with an Alt+Enter window, two frames in flight, vsync, a native back buffer with the UI in 1920×1080 reference units, fatal device loss. | [ADR-006](../Design/ADR/ADR-006-renderer-shape.md) | — |
| G2 | How meshes reach the game, how they get into the MSIX package, and the art's provenance (design §11, §15) before the meshes ship in a package. **Decided on 2026-09-30: a loader for DirectX's `.cmo` format in the game; the owner converts the meshes.** The converted Human and Tarkan sets and the asteroid are in `OutpostCommander/Assets/Models/` and packaged under `Assets\Models\`. Provenance is still open. | [ADR-011](../Design/ADR/ADR-011-meshes-and-shading.md); design §15 for provenance | 1.3 (provenance before shipping) |
| G3 | The camera's zoom range around the 500 m default view (design §4, §15). Until it is decided, 1.5 uses provisional limits held as data: 150 m to 1,600 m. | `OutpostCommander/Assets/Camera.json`; design §4, §15 for the reasons | 1.5 (final values) |
| G4 | The namespace for the game layers. **Decided on 2026-09-30: `Outpost`.** | AGENTS.md §1, R9 | — |
| G5 | Ship sizes in metres: footprint radii for movement and formation, and the spacing the Missile Rack's splash depends on (design §11, §12, §15). 2.4 can start with provisional radii held as data. 5.3 cannot start without them. | `OutpostCommander/Assets/Tuning.json`; the reasons in design §12 | 2.4 (final values), 5.3 |
| G6 | The format of the tuning data that replaces design §12 as the source of numbers, and whether §12 keeps a copy. **Decided on 2026-09-30: JSON, and §12 keeps no copy.** It covers the map (2.3) and the provisional radii and turn rates (2.4) too. | [ADR-008](../Design/ADR/ADR-008-tuning-data.md); design §12 | — |
| G7 | How the game draws its UI: text, panels, input focus (ADR-001, design §9, §15). R14 rules out the usual libraries, so it is DirectWrite or GDI text from the Windows SDK, or a bitmap font drawn by D3D12. **Decided on 2026-10-01: a DirectWrite glyph atlas drawn as quads by D3D12.** | [ADR-015](../Design/ADR/ADR-015-ui-drawing.md) | — |
| G8 | The Constructor's HP, speed, cost and build time, and the build and repair rates (design §7, §12, §15). | `OutpostCommander/Assets/Tuning.json`; the reasons in design §12 | 4.2 |
| G9 | The AI's attack-group threshold (design §10, §15). | `OutpostCommander/Assets/Tuning.json`; the reasons in design §12 | 6.1 |

Turn rates (design §15) do not gate anything: weapons are turrets and hits are instant, so turn rates only shape movement. 2.4 uses provisional values held as data.

---

## Phase 0 — The checkers

AGENTS.md leans on three checkers, which phase 0 wrote, and CI already runs each one the moment its file lands (`.github/workflows/build.yml`). They come first so that every later PR is gated by them. They are Python, and they run on Windows in CI; 0.1 and 0.2 must also run on Linux, so a cloud agent can run them before pushing.

### What phase 0 changed for later tasks

Phase 0 landed as one PR, [#26](https://github.com/Zwaliebaba/Outpost.Commander/pull/26), at the owner's choice, rather than one per task.

- **All three checkers gate in CI**, and `CheckProjectFiles.py` and `RunClangTidy.py` run a `--self-test` there too. A task that adds a project adds its row to `INCLUDE_PATHS` in `CheckProjectFiles.py` as well as to ADR-002's table; the checker fails until it does.
- **R11 is checked on the design's own words.** `CheckProjectFiles.py` rejects `armour`, `defence`, `metre` and the other non-SDK spellings in identifiers, so 4.4's Defence gun and structure armour are `DefenseGun` and `armor` in code, and distances are `...Meters` (R6). The design document keeps its spelling; only identifiers are checked. The owner confirmed this on 2026-09-30.
- **`ENUM_HELPER` is gone from `NeuronHelper.h`.** It was unused and did not preprocess under clang. The first task with an enum that needs `++`, bit operators or a range adds a lint-clean replacement.
- **`GameLogicTests` started** with only `SuiteSmoke`, and 2.1's first real suite deleted it (AGENTS.md §3). CI finds the DLL at `x64\Debug\GameLogicTests.dll`.

### 0.1 — `Build/CheckFormat.py`

- **Goal:** the formatting gate of AGENTS.md §4.
- **Scope:** `Build/CheckFormat.py`. It finds every tracked `.h` and `.cpp` except `CompiledShader/` output, and runs `clang-format --dry-run` on each. `--fix` rewrites the offending files in place. `--clang-format <exe>` picks the binary, which CI already passes as `clang-format-18`. It exits non-zero if any file is not clean, and names each one.
- **Acceptance:** exit 0 on today's tree, which is clean under clang-format 18.1.3. Deliberately misformat one file and it names that file with exit 1; `--fix` then makes it clean. It needs nothing but the standard library.
- **Verify:** run locally with `clang-format-18` on Linux; CI's `clang-format` job starts gating.

### 0.2 — `Build/CheckProjectFiles.py`: tree shape and names

- **Goal:** the checks in AGENTS.md §1's enforcement table that belong to this script, for the tree's shape.
- **Scope:** `Build/CheckProjectFiles.py`, parsing `OutpostCommander.slnx`, every `.vcxproj` and every `.vcxproj.filters` with `xml.etree`. It checks:
  - Every `.h`/`.cpp` in a project folder is in its `.vcxproj` and in its `.filters`, and every file those name exists (§2).
  - Source sits directly in the project folder, apart from `Shader/` and `CompiledShader/` (§2). Shaders are named `<Shader>VS.hlsl` / `<Shader>PS.hlsl`.
  - Filters are functional: no `Source Files`, `Header Files` or `Resource Files`, and a `.h` shares its `.cpp`'s filter (§2).
  - File names are PascalCase `.h`/`.cpp` with R7's exceptions (`pch.h`, `pch.cpp`, `framework.h`, `targetver.h`, `Resource.h`). No `.hpp`, `.cc` or `.inl`.
  - R2: no type name with an `I`/`C`/`S`/`E` prefix before a capital, a `Base`/`Abstract`/`Impl` affix, or a `_t` suffix. This is a regex over declarations; it does not need a parser.
  - R11: no identifier uses the non-SDK spelling of a listed family (`colour`, `initialise`, `behaviour`, `centre`, …). Comments and strings are exempt.
  - No `#include` climbs out of its own project (`..`), which is what makes ADR-002's include paths binding.
  - R12: no `Microsoft::WRL::ComPtr` and no `<wrl/client.h>`. A COM pointer is a `winrt::com_ptr`.
  - `.clang-tidy`'s `HeaderFilterRegex` names every project in the solution.
- **Acceptance:** exit 0 on today's tree, or a fix in the same PR for every finding (say which). Each check has a small negative case, as a script flag or a test, that shows it fires.
- **Verify:** run locally on Linux; CI's "Check the build shape" step starts gating.

### 0.3 — `Build/CheckProjectFiles.py`: build settings and include paths

- **Goal:** the static alignment check that stands in for the Release and ARM64 builds CI never runs (AGENTS.md §3, ADR-003).
- **Scope:** extend 0.2's script so that, for every project:
  - `v145`, `stdcpplatest`, `ConformanceMode` true, `Level4`, `TreatWarningAsError` true and `Precise` are stated.
  - The instruction set is `AdvancedVectorExtensions2` on x64 and `CPUExtensionRequirementsARMv80` on ARM64.
  - Every setting outside AGENTS.md §3's list of what may differ reads the same in Debug and Release, and on x64 and ARM64 apart from the instruction set.
  - Both platforms exist, and no others do.
  - No project defines the Windows macro family (§4).
  - Each project's include path matches ADR-002's table exactly. The executable does not inherit `%(AdditionalIncludeDirectories)`, because its Windows Store project type would add its own folder, `Generated Files\` and its intermediate folder (ADR-001).
  - Only the projects in R14's table have a `packages.config`, and each one lists and imports only the packages in its own row.
  - No project defines `USE_PIX`, `USE_PIX_RETAIL` or `PROFILE` (ADR-005).
- **Acceptance:** exit 0 on today's tree. Removing one include directory from one configuration, or adding `NeuronServer` to `GameApp`'s include path, makes it fail and say which project and setting.
- **Verify:** run locally; CI.

### 0.4 — `Build/RunClangTidy.py`

- **Goal:** the naming and lint gate of AGENTS.md §1.
- **Scope:** `Build/RunClangTidy.py`. There is no CMake, so it builds each translation unit's command line from its `.vcxproj`:
  - the include paths;
  - `/std:c++latest`, `_DEBUG`, and the configuration's defines;
  - the precompiled header, read as a normal include.

  It then runs the pinned `clang-tidy` (CI installs `CLANG_TIDY_VERSION`) through its MSVC driver, with `INCLUDE` taken from the Developer environment, over every `.cpp` in the solution. Headers are covered through `.clang-tidy`'s `HeaderFilterRegex`. Any finding is exit 1.
- **Acceptance:** CI's "Run clang-tidy" step passes on the tree, with any finding fixed in the same PR. A deliberately misnamed member (`int foo;` in a class) fails it.
- **Verify:** CI only. It needs Windows.

### 0.5 — `GameLogicTests` with `SuiteSmoke`

- **Goal:** the test project that ADR-002 names, so that later tasks can test the simulation headlessly.
- **Scope:** a native unit-test DLL project, `GameLogicTests`, using the Microsoft C++ unit-test framework that ships with Visual Studio. Its include path is `GameLogic`, `NeuronServer`, `GameProtocol` and `NeuronCore`, and it links those four; ADR-002 allows a test project to include `GameLogic`. It has a `SuiteSmoke` placeholder (AGENTS.md §3). It sits in the solution, and it matches 0.3's rules on both platforms. Add it to the ADR-002 table and to `.clang-tidy`'s header filter.
- **Acceptance:** CI's "Run the tests" step finds the DLL and passes. `CheckProjectFiles.py` passes.
- **Verify:** CI.

---

## Milestone 1 — A ship on screen

Design §14: *the Win32 window with a D3D12 flip-model swap chain, a mesh loaded with its scale fixed, and the camera working.* The window, device and swap chain are engine code, in `NeuronClient`. `WinMain` stays in the executable.

### 1.1 — A Win32 window and message loop

- **Goal:** the executable opens a window and exits cleanly (ADR-001).
- **Scope:** a window class in `NeuronClient`. It knows no game concept: title and size come from its caller. It has a `PeekMessage` loop that returns control each frame, and `wWinMain` in `OutpostCommander` returns the `WM_QUIT` message's `wParam`. It keeps the high-DPI awareness `app.manifest` declares.
- **Acceptance:** CI green, the checkers pass, and the files are in `.vcxproj` and `.filters`.
- **Verify:** CI; **owner run** of the packaged app (F5): a window appears and closes with exit code 0.

### 1.2 — D3D12 device and flip-model swap chain

- **Gate:** G1, decided. **ADR:** [ADR-006](../Design/ADR/ADR-006-renderer-shape.md), the renderer's shape.
- **Goal:** clear the window to a colour every frame, through `CreateSwapChainForHwnd` and `DXGI_SWAP_EFFECT_FLIP_DISCARD`.
- **Scope:** in `NeuronClient`:
  - device and adapter selection;
  - command queue, allocators and lists, and fences;
  - swap chain, back buffers and resize;
  - present and device-removed handling.

  COM lifetimes are held by `winrt::com_ptr`, and `HRESULT`s are checked with `winrt::check_hresult` (R12), both already available through `NeuronCore.h`. Barriers and descriptor handles use `d3dx12.h`'s helpers through `DirectXHelper.h` (ADR-007). In Debug the D3D12 debug layer is on.

  PIX event markers name the frame's regions on the queue, the command lists and the CPU (ADR-005). They are compiled in for Debug only, and `pix3.h` sits between `#pragma warning(push)` and `pop` in a `NeuronClient` `.cpp`. The runtime's import library, DLL and licence already reach the executable through `NeuronClient`.
- **Acceptance:** CI green. There are no debug-layer errors in a run. The game covers the primary monitor, Alt+Enter toggles a 1280×720 window and back, dragging the window to a new size works, it follows a change of display resolution, and it survives being minimised and restored (Win+D). A failure shows a message box instead of closing silently. A Release build has no reference to the PIX runtime.
- **Verify:** **owner run**, x64 and ARM64: the development machine is ARM64 (ADR-003). A PIX capture of the Debug build shows the named regions.

### 1.3 — Mesh loading, with scale and forward axis as data

- **Gate:** G2. **ADR:** how meshes reach the game.
- **Goal:** load the hull meshes and draw one (design §11).
- **Scope:**
  - A `.cmo` loader in `NeuronClient` (G2) that reads positions, normals and indices from the owner's converted meshes and ignores their materials (design §11). The meshes are two sets of the same fourteen models (`Carrier`, `Colonizer`, `Drone`, `Fighter`, `Freighter`, `Huge`, `Large`, `Medium`, `Mine`, `Satellite`, `Small`, `Station`, `Tiny`, `VeryLarge`): the player's Human set in `OutpostCommander/Assets/Models/Human/` and the AI's Tarkan set in `OutpostCommander/Assets/Models/Tarkan/` (design §1, §11). The asteroid is `OutpostCommander/Assets/Models/Asteroids/Asteroid.cmo`. Their sources are in `Art/Models/`.
  - A data file giving each model of each set a scale and a forward axis, so the file is keyed by set and model. The Tarkan hulls point along x and the Tarkan `Colonizer` along z; every Human model points along z. Up is y on the Tarkan set; check the Human set for roll the same way (design §11). The scale is measured from the mesh's extents, and the task states how. It is applied at load, and it brings a hull of one set to the same size as the same hull of the other.
  - The meshes are already packaged: the executable lists each `.cmo` as deployment content, so they reach the MSIX layout as `Assets\Models\Human\<Name>.cmo`, `Assets\Models\Tarkan\<Name>.cmo` and `Assets\Models\Asteroids\Asteroid.cmo`, under the `Assets` folder where `FileSys` looks (`NeuronCore/FileSys.h`). The loader reads them by those paths.
  - A mesh that fails to load is reported, not silently skipped.
- **Acceptance:** CI green. A test or tool check shows that `Small`, `Medium` and `Large` load at their intended relative sizes in both sets, and that each hull is the same size in the Human and Tarkan sets. The design notes that the Tarkan `Medium` is larger than its `Large` before scaling.
- **Verify:** **owner run:** one Human and one Tarkan hull on screen, each facing along its forward axis.
- **As built:** [ADR-011](../Design/ADR/ADR-011-meshes-and-shading.md). `Neuron::ParseCmo` and `Neuron::OrientMesh` in `NeuronClient`; `OutpostCommander/Assets/Models.json` gives each set its color and each model its `forwardAxis` and `lengthMeters`, read by `Outpost::LoadModelCatalog` in `GameApp`. The scale is the model's length divided by its mesh's extent along its forward axis. Hull lengths are provisional with G5: Small 20 m, Medium 35 m, Large 60 m. The Human set faces −z; the Tarkan hulls are set to +x, and several of them are close to symmetric end to end; the owner's run confirmed which end is the front. `GameAppTests`, a new test project, checks that every shipped model parses, that the hulls load in size order in both sets and at the same length in both, and that orienting keeps the winding. Every model loads at start, and one that fails stops the game with the file's name.

### 1.4 — Flat-lit, team-coloured shading

- **Goal:** meshes shaded with flat lighting and a team colour (design §11).
- **Scope:** `NeuronClient/Shader/<Name>VS.hlsl` and `PS.hlsl`, compiled by `FXCompile` into `CompiledShader/` and included only by the `.cpp` that builds the pipeline state (AGENTS.md §2). Include a root signature, a pipeline state, a per-frame constant buffer and per-object colour.
- **Acceptance:** CI green, with the compiled headers generated and not committed.
- **Verify:** **owner run:** the hull reads clearly in two team colours.
- **As built:** [ADR-011](../Design/ADR/ADR-011-meshes-and-shading.md). `Neuron::MeshPipeline` with `Shader/MeshVS.hlsl` and `MeshPS.hlsl`, shader model 5.1; a root constant buffer view per frame and 20 root constants per object; Lambert lighting from one light over an ambient floor of 0.3. The renderer gained a depth buffer and a `BeginFrame`/`EndFrame` pair in place of `RenderFrame`. The provisional team colors are blue for the player and orange-red for the Tarkan (design §15).

### 1.5 — The RTS camera

- **Gate:** G3 for the final zoom limits; provisional limits are held as data.
- **Goal:** design §4's camera. Pan by edge scroll, WASD and middle-drag. Zoom with the wheel, clamped. Rotate around the focus point with Q/E. The pitch comes from the zoom level, and the default view is about 500 m wide.
- **Scope:** camera state and input in `GameApp`, since camera state is client state (ADR-002), over the view and projection math in `NeuronClient` (`DirectXMath`). A test grid on the y = 0 plane shows scale. Decide, and record, whether the cursor is clipped to the window while the game is active and what edge scroll does in a window (ADR-006).
- **Acceptance:** CI green. The camera math has unit tests where it is pure: the width at default zoom, and the pitch at each zoom limit.
- **Verify:** **owner run.**
- **As built:** [ADR-012](../Design/ADR/ADR-012-rts-camera.md). `Outpost::Camera` in `GameApp` over DirectXMath; the numbers in `OutpostCommander/Assets/Camera.json`, provisional until G3. Input comes from `Neuron::Window::ReadInput`. The cursor is held inside the window while the game is full screen and in the foreground, and edge scroll works only then; in a window, WASD and middle-drag pan. The scene is milestone 1's placeholder until 2.5: the Small, Medium and Large hulls of each set side by side, facing +x, an asteroid at a home asteroid's size, and a grid with a line every 100 m and a brighter one every 500 m.

### 1.6 — Milestone 1 review

- **Goal:** close milestone 1 the way design §14 asks: run, not just built.
- **Scope:** the owner runs the app on x64 and ARM64, Debug and Release. Record the outcome in this plan, fix what the run shows (split into tasks if large), and update design §14 if the milestone moved.
- **Verify:** **owner run.**
- **As built:** milestone 1 landed on `main` as one commit, [23f0c1f](https://github.com/Zwaliebaba/Outpost.Commander/commit/23f0c1f), and the owner closed it on 2026-09-30 after running it.

---

## Milestone 2 — Ships that obey

Design §14: *the in-process server ticking, selection, move commands, pathing around asteroids, and interpolated rendering.* This answers **Q5**, including the order-to-response delay, and the tick-time half of **Q4**. ADR-002 is the contract for everything here.

### 2.1 — Protocol types: IDs, commands, snapshots, `Transport`

- **Namespace:** `Outpost` (G4).
- **Goal:** the types that cross the client/server boundary (ADR-002 decisions 2, 4, 6). The MVP's transport is the loopback. QUIC comes after the MVP (ADR-004), and no QUIC code is written here.
- **Scope:** in `GameProtocol`:
  - `EntityId` and `PlayerId`;
  - a `Command` for each order in design §9 (move, attack, attack-move, stop, build structure, queue ship, start research, save design), tagged with its player;
  - a per-player `Snapshot`;
  - the `Transport` interface;
  - the declaration of the in-process server factory.

  All of them are plain data: no pointers into server state, and entities by ID only. The unit of each quantity is in its name (R6).
- **Acceptance:** CI green. `GameApp` and `Opponent` compile against these types, and neither can include `GameLogic` (ADR-002's C1083 check).
- **Verify:** CI.

### 2.2 — Tick host, seeded PRNG, in-process server

- **ADR:** [ADR-009](../Design/ADR/ADR-009-deterministic-core.md), the deterministic core (R16). The owner decided on 2026-09-30 that a replay promises the same build on the same platform, and that the server ticks on the frame loop's thread.
- **Goal:** the authoritative server running inside the client (ADR-002 decisions 1, 3, 5 and 8).
- **Scope:**
  - A fixed-rate tick host in `NeuronServer`, 20 Hz from the tuning data that 3.1 loads (design §12). Wall time becomes ticks at this one seam.
  - `OutpostCommander/Assets/Tuning.json` goes into the MSIX package, and the server reads it from there with `Outpost::LoadTuning` (ADR-008). 3.1 left the packaging here because nothing read the file at run time before 2.2.
  - A seeded PRNG owned by the server; never `std::random_device`.
  - In `GameLogic`: world state, applying commands at the start of a tick with validation and rejection, building a snapshot per player, a `LoopbackTransport`, and the factory definition.
  - The executable wires them together through `GameProtocol`.
- **Acceptance:** tests in `GameLogicTests`:
  - a command is applied on the next tick;
  - an invalid command is rejected;
  - the same seed and command log reproduce the same state on the same build.
- **Verify:** CI; **owner run**, because the executable now loads `Assets\Tuning.json` from the package at startup and reports a failure in a message box.
- **As built:** `Neuron::TickHost` and `Neuron::Random` (xoshiro256\*\*) in `NeuronServer`; `Simulation` and `InProcessServer` in `GameLogic`; `Server::Advance(elapsed)` in `GameProtocol`. Move and stop are applied; the other orders are rejected as not yet supported until their tasks. Starting entities are not placed yet: 2.3 placed only the map. Ships at the starts: decided by the owner on 2026-09-30, 2.5 adds a provisional starting fleet held in `Map.json`, until 4.2 places the Command Station and the Constructors (ADR-013).

### 2.3 — The map as data

- **Goal:** design §4's map. It is about 2,000 × 2,000 m, with two starts in opposite corners. It has 12 ore asteroids: 3 home asteroids by each base and 6 contested ones in the middle. Non-mineable asteroid fields act as circular obstacles and chokepoints.
- **Scope:** a map data file, `OutpostCommander/Assets/Map.json` (ADR-008), and its loader in `GameLogic`. The layout is proposed in the PR and confirmed by the owner. Asteroids are drawn from `OutpostCommander/Assets/Models/Asteroids/Asteroid.cmo`.
- **Acceptance:** tests: the map loads, no obstacles overlap, and every asteroid can be reached from both starts.
- **Verify:** CI; **owner run** once 2.5 renders it.
- **As built:** `OutpostCommander/Assets/Map.json` holds a 2,000 m square centered on the origin: two starts, 6 home and 6 contested ore asteroids, and 9 asteroid fields, point-symmetric. `Outpost::LoadMap` rejects any two obstacles, an obstacle and the edge, or a start and an obstacle closer than `minimumGapMeters` (60 m). With every gap at least that wide, a ship narrower than it cannot be walled off, and `MapTests` checks that with a flood fill. The server places ore asteroids and fields as entities (`EntityKind::Asteroid` and `AsteroidField`), and `EntityView` gains `radiusMeters`, so 2.5 can draw them from the snapshot. The JSON reading helpers moved from `Tuning.cpp` to `NeuronCore/JsonReader.h`, shared by both loaders.

### 2.4 — Movement, pathing and formations

- **Gate:** G5 for the final footprint radii; provisional radii and turn rates are held as data.
- **Goal:** ships path around circular obstacles and hold a loose formation at the pace of the group's slowest ship. They avoid overlapping but do not collide physically (design §9).
- **Scope:** in `GameLogic`, with any reusable geometry in `NeuronServer` or `NeuronCore` if it knows no game concept (R9).
- **Acceptance:** tests:
  - a ship reaches a target behind an obstacle;
  - a mixed group arrives together at its slowest member's speed;
  - no two ships' footprints overlap by more than a stated tolerance after settling.
- **Verify:** CI.
- **As built:** [ADR-010](../Design/ADR/ADR-010-movement-and-pathing.md). Hulls carry provisional `footprintRadiusMeters` (8, 14 and 24 m) and `turnRateDegreesPerSecond` (180, 120 and 60), and drives a `turnRateFactor` (Ion 1.25, Fusion 0.8), until G5. The tolerance is 0.5 m. A 200-ship order costs about one Q4 tick budget, which 2.7 measures on the development machine.

### 2.5 — Rendering from interpolated snapshots

- **Goal:** the client draws ships and asteroids from the last two snapshots, interpolated. It never draws server state (ADR-002 decision 5).
- **Scope:** `GameApp` keeps the snapshot history and hands draw lists to `NeuronClient`.
- **Acceptance:** CI green. A test covers the interpolation math.
- **Verify:** **owner run:** smooth motion at 60 fps from a 20 Hz tick.
- **As built:** [ADR-013](../Design/ADR/ADR-013-client-view-and-controls.md). Started before 1.6's owner run, at the owner's choice. `SnapshotInterpolator` in `GameApp` shows the world one tick behind the newest snapshot, on a clock pulled gently toward it, and never extrapolates. `EntityView` carries the ship's `HullId`, and `Models.json` maps players to sets and hulls to models. `Server::TicksPerSecond()` gives the client the rate. The server places a provisional starting fleet from `Map.json` at each start (the owner's decision), so there is something to draw: four Small and two Medium ships per player, facing the map's center. Ore asteroids are drawn with the asteroid mesh at their radius, and a field as a ring of rocks. The milestone 1 lineup is gone. A test drives the view at 60 fps from 20 Hz snapshots and checks that every frame steps forward by close to the same distance. Ships move only once 2.6 gives orders, so the owner's run of 2.5 happens together with 2.6's.

### 2.6 — Selection, orders and control groups

- **Goal:** design §9's player controls.
  - Left-click to select, drag to box-select, Shift to add, and double-click to select every visible ship of that design.
  - Right-click to move, or to attack an enemy. `A` and a click to attack-move, and `S` to stop.
  - Ctrl+0–9 assigns a control group. 0–9 recalls it, and a double tap centres the camera on it.
- **Scope:** selection and control groups are client state in `GameApp`. Orders become `Command`s through the transport.
- **Acceptance:** CI green. Tests cover picking and box selection where they are pure math.
- **Verify:** **owner run.**
- **As built:** [ADR-013](../Design/ADR/ADR-013-client-view-and-controls.md). The window records presses and releases as events. `PlayerControls` turns them into selection, control groups and `Command`s, which `WinMain` sends at once. The selection shows as green rings on the ground, amber while attack-move waits for its click, and a drag box as its outline on the ground until the HUD exists (G7). The arrow keys pan and A and S are orders, decided by the owner on 2026-09-30. The server rejects attack and attack-move until 3.3. Double-click compares hulls until designs exist (3.2).

### 2.7 — Measure Q5 and the tick half of Q4

- **Goal:** answer design Q5 (order-to-response delay ≤ 150 ms, and the boundary enforced by the build) and Q4's tick half (≤ 5 ms at 200 ships and 40 structures).
- **Scope:**
  - Instrument the tick time with the server's own timer. The server has no PIX markers (ADR-005).
  - Measure the time from input to the first frame showing the response. The PR states the method, so the figure is measured, not estimated (AGENTS.md §6).
  - Add a scripted load of 200 ships and 40 static structures for the tick measurement.
- **Acceptance:** the figures, the method and the machine are recorded in design §3. The Q5 boundary is confirmed by the C1083 check.
- **Verify:** **owner run** on the development machine.
- **As built:** the figures, the method and the machine are in design §3.
  - **Q5 is met:** 43 ms mean and 67 ms worst, from an injected click to the frame that shows the ship respond. The boundary fails to compile, as it should.
  - **The tick half of Q4 is met in steady play:** 0.18 ms mean and 0.37 ms at the 99th percentile. It is **missed on the ticks that order both 100-ship fleets at once**: up to 8.3 ms. Whether that needs fixing is open in design §15.
  - **How it was built:** `--measure` logs tick durations and order-to-response times to `OutpostCommander-measure.log` in the temporary folder. `--load` places `PlaceMeasurementLoad`'s 200 ships and 40 structures, and `LoadDriver` keeps both fleets moving. `Server::TakeTickDurations` reports the server's own timing. `Simulation::SpawnStructure` places the load's structures, which do not block movement until 4.2.
  - **How to repeat it:** the recorded runs were driven by a script on the owner's machine. `OutpostCommander.exe --measure` and `--measure --load` repeat them by hand. The owner closed the task on 2026-09-30.
  - **Follow-up, 2026-10-01:** the owner chose to path a group's order once for the group (ADR-010). In a Linux container the same 200-ship order tick fell from about 3.4 ms to about 1 ms; the development machine's figure needs `--measure --load` again.

---

## Milestone 3 — Ships that fight

Design §14: *weapons, damage and destruction, with designs as data from §12. A 200-ship stress scene under a representative HUD, and the Q2 check as scripted headless battles in `GameLogicTests`*. This answers **Q2** and **Q4**.

### 3.1 — Tuning data file, loaded by the game and the model

- **Gate:** G6, decided. **ADR:** [ADR-008](../Design/ADR/ADR-008-tuning-data.md). It runs before 2.2, which takes its tick rate from this file; the number stays 3.1 so that links to it hold.
- **Goal:** design §12's numbers become data that the game loads and `Tools/BattleModel.py` reads, so that neither can disagree with the other.
- **Scope:**
  - The data file, and its loader in `GameLogic`.
  - Change `Tools/BattleModel.py` to read it instead of §12's tables, together with §8's research table if G6 moves that too.
  - Update design §12 as G6 decides.
- **Acceptance:** `python Tools/BattleModel.py` gives the same verdicts as before the move. A test shows the game loads the same numbers.
- **Verify:** CI; run the model locally.
- **As built:** `OutpostCommander/Assets/Tuning.json` holds §12 and §8's research table, and both sections now point to it. The JSON parser is `Neuron::ParseJson` in `NeuronCore`, since the map (2.3) and other data files need it too; the loader is `Outpost::LoadTuning` in `GameLogic`. Packaging the file moved to 2.2, its first reader at run time.

### 3.2 — Components and designs

- **Goal:** hull + drive + weapon designs and their derived stats (design §7): hit points and speed from hull × drive, armour, cost, and the damage formula `max(damage × 0.25, damage − armour)`.
- **Scope:** `GameLogic`.
- **Acceptance:** a test compares every design's derived stats with the table `BattleModel.py` prints: cost, HP, armour, speed, range, and damage per second after armour against each hull.
- **Verify:** CI.

- **As built:** [ADR-014](../Design/ADR/ADR-014-designs-and-combat.md). `Outpost::DesignStatsFor` derives every stat from the tuning data, and `ShipDesign` holds a saved design. Hit points, armor and damage count in integer hundredths. Every player starts with the four starting designs saved, and the provisional starting fleet carries all four (owner, 2026-10-01). `DesignTests` compares every design with the model's table and the armor rule.

### 3.3 — Combat rules

- **Goal:** design §7's combat rules.
  - Hits are instant. Weapons are turrets and fire on the move.
  - Auto-targeting takes the nearest enemy ship in range, otherwise the nearest structure, and keeps it until it dies or leaves range.
  - An attack-moving ship stops at its own range and fires.
  - Ships are destroyed at 0 HP.
- **Scope:** `GameLogic`, using the server's PRNG for anything random, such as first-shot offsets.
- **Acceptance:** tests:
  - fire interval and damage after armour, per weapon;
  - target choice and stickiness;
  - attack-move stops at range;
  - a ship can fire while moving.
- **Verify:** CI.

- **As built:** [ADR-014](../Design/ADR/ADR-014-designs-and-combat.md) and [ADR-010](../Design/ADR/ADR-010-movement-and-pathing.md) decision 7. Attack, attack-move and stop are applied. A weapon reloads in thousandths of a tick, and a cold weapon's first shot comes at a random moment within its interval. Shots are chosen from where everything stands at the start of a tick and land together at its end; snapshots carry the tick's shots and the destroyed. Ships part rather than collide, and a ship standing to fire gives way only sideways round its target, so a group spreads into an arc at its range (owner, 2026-10-01). `CombatTests` covers the interval and damage, target choice and stickiness, attack-move stopping at range, a group keeping its stand-off, firing on the move, the attack order and replays.

### 3.4 — The Q2 check as headless battles

- **Goal:** design §3's Q2 check against the real simulation. From here on, where the model and the simulation disagree, the simulation is right and the model gets fixed.
- **Scope:** scripted battles in `GameLogicTests` with the same stages, budgets, fire modes and criteria (a)–(d) as the model. Focus and spread fire are forced by test hooks, not by player orders.

  The model fields a fractional ship for leftover Ore; the simulation cannot. So each budget runs as a grid of whole-ship budgets across the ±15% window, and the PR states how that maps onto the model's method.
- **Acceptance:** the verdicts, and every disagreement with `BattleModel.py` with its cause, are recorded in design §12. Q2 stays "not yet" until 5.3, because the Missile Rack is not modelled.
- **Verify:** CI. If the suite is too slow for CI, the PR proposes a split between a CI subset and a full local run.

- **As built:** `GameLogicTests/Q2Check.cpp` plays design §3's check as headless battles on every hardware thread. Whole ships are fielded at the center of each slice of the ±15% window, and the Ore left over is not; the second side is the first turned half a turn, so a mirror match is fair. `Simulation::SetTargetRule` forces spread or focus fire, for the check only. **The check fails against the simulation**, where it passes in the model, and design §12 records each failure and why the two disagree; what to do about it is open in design §15. CI runs the fast part, `TheRecordedCountersHold` among it. The full check, `TheFullCheck`, is in category `Q2Full`, which CI filters out, and took about ten minutes on four threads in a Linux container.

### 3.5 — Combat effects

- **Goal:** the minimum needed to read combat (design §11): muzzle flash, projectile or beam, hit spark and explosion. These are placeholder sprites or simple geometry. Hits are already resolved; effects are presentation.
- **Scope:** `NeuronClient` rendering, driven from `GameApp`.
- **Verify:** **owner run.**

- **As built:** `Outpost::CombatEffects` in `GameApp` turns each snapshot's shots and destroyed into effects drawn with the mesh pipeline: a muzzle flash, a tracer for the Mass Driver and a beam for the Lance, a hit spark, and an explosion where a ship or structure died. Health bars show over damaged ships and structures. Effects start one tick back, so they line up with the interpolated view. Not run yet: the owner's run decides whether they read.

### 3.6 — In-game UI drawing and a first HUD

- **Gate:** G7. **ADR:** how the game draws its UI.
- **Goal:** text and panels drawn over the D3D12 scene, and a first HUD: the Ore stockpile and the selection panel. Q4 needs the HUD on screen.
- **Scope:** UI rendering in `NeuronClient`. HUD state in `GameApp`.
- **Verify:** **owner run.**

- **As built:** [ADR-015](../Design/ADR/ADR-015-ui-drawing.md). `Neuron::RasterizeGlyphs` and `PackGlyphs` build the atlas, and `Neuron::UiPipeline` draws panels and text in one draw call with `UiVS.hlsl` and `UiPS.hlsl`. `Outpost::Hud` lays out the Ore stockpile in the top-left corner and, for a selection, a panel at the bottom middle with the ship count, the count of each design by name, and their hit points, in reference units. A button press on a HUD panel does not reach the player's controls. `HudTests` and `GlyphAtlasTests` run without a GPU; rasterizing a system font needs Windows. Not run yet: the owner's run checks the text is sharp at the native resolution.

### 3.7 — Q4 stress scene and measurement

- **Goal:** answer design Q4. With 200 ships and 40 structures in combat at 1920×1080, with the HUD drawn, 99% of frames take ≤ 16.7 ms and a tick takes ≤ 5 ms.
- **Scope:**
  - A scripted stress scene.
  - Frame-time capture: per-frame CPU and GPU work time, and the machine's refresh rate, written to a file (ADR-006). Release has no PIX markers (ADR-005), so these timings are the measurement. A PIX capture of Debug is where to look for the cause of a miss.
  - A summary script under `Tools/`.
- **Acceptance:** the figures, the method and the machine are recorded in design §3. This is x64 and ARM64 if the owner measures both.
- **Verify:** **owner run**, Release.
- **As built:** the stress scene is `Outpost::StressLoad`, run by `OutpostCommander.exe --stress`; with `--measure` the game logs each frame's CPU and GPU work, each tick, the back buffer's size and the display's refresh rate, and `python Tools/FrameTimes.py` summarizes the log against Q4. Design §3 gives the method. `Neuron::Renderer` takes the GPU timestamps and `TakeGpuFrameTimes` returns them. `StressLoadTests` runs the scene for 60 simulated seconds: each side stays above 80 ships, with more than 1,000 shots and more than 20 ships or structures destroyed. In a Linux container its ticks took 0.11 ms at the median, 0.45 ms at the 99th percentile and 1.7 ms at worst, which is not the development machine. **Q4 is not answered until the owner's measurement**, Release on the development machine, is recorded in design §3.

---

## Milestone 4 — A base

Design §14: *Constructors built at the Command Station, structures with the Defence gun, Ore, and the Shipyard queue.* Design §5 and §6 are the specification.

### 4.1 — Ore, Mining Rigs and costs

- **Goal:** design §5.
  - One resource, Ore.
  - A Mining Rig on an asteroid gives a fixed income: 5 Ore/s at home and 8 on a contested asteroid.
  - There is no depletion and no hauling. One rig per asteroid, and it can be rebuilt after it is destroyed.
  - Costs are paid when a job starts, with no refund. The starting stockpile comes from the tuning data.
- **Acceptance:** tests of income per tick, payment at the start, no refund, and one rig per asteroid.
- **Verify:** CI.

### 4.2 — Structures, placement and Constructors

- **Gate:** G8.
- **Goal:** design §6.
  - Structures have circular footprints that must not overlap, and a Mining Rig snaps to an asteroid.
  - Constructors build structures, and several on one site build faster.
  - A right-click on a damaged friendly repairs it. Repair is a new order: this task adds a repair command to `GameProtocol`, which 2.1 left out (owner, 2026-09-30).
  - The client shows a ghost of the structure being placed.
  - Players start with two Constructors, and the Command Station is placed before the match starts.
- **Acceptance:** tests of placement legality, snapping, build progress with one and with two Constructors, and repair.
- **Verify:** CI; **owner run** for the ghost and the build menu.

### 4.3 — Shipyard and Command Station queues

- **Goal:** the Shipyard builds ships from designs and the Command Station builds Constructors, each with a queue of up to 5 (design §6). Build times come from the tuning data.
- **Acceptance:** tests of queue limits, build times and payment at the start.
- **Verify:** CI.

### 4.4 — The Defence gun and structure armour

- **Goal:** the Defence Platform and the Command Station carry the Defence gun: 30 damage every 1.0 s, at 250 m. Both have armour 10; every other structure has none (design §6, §12). Auto-targeting uses the same rules as ships.
- **Acceptance:** tests: the gun outranges the Lance and not the Missile Rack, and armour cuts a Mass Driver hit from 14 to 4.
- **Verify:** CI.

### 4.5 — The full HUD and the minimap

- **Goal:** design §9's HUD: the Ore stockpile and income, the selection panel, build and research queues, and a minimap.
- **Verify:** **owner run.**

### 4.6 — Hand checks of the structure numbers

- **Goal:** check design §6's hand estimates against the simulation. Design §15 says the Defence gun and structure armour are checked at milestone 4.
- **Scope:** scripted scenarios in `GameLogicTests`:
  - a lone platform against five Small+Ion+Mass Driver ships;
  - the armed Command Station against seven;
  - the unarmed-station rush time.
- **Acceptance:** the results are recorded in design §6 and §12. Any number that misses its intent is raised with the owner, not retuned silently.
- **Verify:** CI.

---

## Milestone 5 — Designs and research

Design §14: *the designer in the Shipyard panel, components and the research tree.* Design §7, §8 and §9 are the specification.

### 5.1 — Research

- **Goal:** design §8.
  - One Research Lab per player, researching one topic at a time.
  - Topics cost Ore and time, and some require another topic.
  - Upgrades apply at once to every existing ship and structure. They change rates, never the size of a hit.
  - Some topics unlock the Large hull, the Fusion Drive or the Missile Rack.
- **Acceptance:** tests of prerequisites, the one-lab limit, upgrades applying to units that already exist, and unlocks.
- **Verify:** CI.

### 5.2 — The ship designer in the Shipyard panel

- **Goal:** design §9's designer.
  - A picker for each slot.
  - Live stats: damage per second after armour against each hull, per ship and per 100 Ore.
  - Cost and build time.
  - Save, rename and queue.
  - Every match starts with the four starting designs saved.
  - It pauses nothing.

  "Save design" is a command. The live stats need the component numbers on the client, which cannot include `GameLogic`'s loader (ADR-008): this task decides whether the tuning types move to `GameProtocol` or the server sends the numbers.
- **Verify:** CI for the stats math, which should match `BattleModel.py`; **owner run** for the panel.

### 5.3 — The Missile Rack, in the game and in the model

- **Gate:** G5.
- **Goal:** splash damage with a 30 m radius, and the 280 m range, in both the simulation and `Tools/BattleModel.py`. This means the model's clumps get the spacing that ship sizes imply. Rerun the Q2 check, since it cannot be "yes" until the Missile Rack is in (design §3, §12).
- **Acceptance:** Q2's standing is recorded in design §12, with the model and the simulation both including the Missile Rack.
- **Verify:** CI; run the model locally.

---

## Milestone 6 — An opponent

Design §14: *the AI player and the win/lose condition.* This answers **Q1** and **Q3**.

### 6.1 — The AI player

- **Gate:** G9.
- **Goal:** design §10's scripted AI, in `Opponent`, as a client. It plays **The Tarkan High Command** (design §1), drawn with the Tarkan meshes in its team colour, while the player's ships use the Human set. It reads its snapshot and sends commands, and it includes only `GameProtocol` (ADR-002).
  1. It builds a Shipyard, a Research Lab and rigs on its home asteroids, then expands to the contested ones.
  2. It researches in a fixed order.
  3. It counters the player's most common design, reviewed every 60 s.
  4. It gathers an attack group to a threshold, then attack-moves on the nearest player structure.
  5. It sends ships outside the attack group to defend a rig or platform under attack.
  6. It rebuilds rigs and replaces Constructors.
  7. It does not kite.
- **Acceptance:** tests in which scripted snapshots produce the expected commands: the build order, the counter choice after a review, the defence response.
- **Verify:** CI; **owner run.**

### 6.2 — Win, lose and the menu

- **Goal:** losing your Command Station loses the match. The menu offers Start skirmish and Quit, and nothing else (design §6, §9).
- **Verify:** CI for the rule; **owner run.**

### 6.3 — Q1 and Q3 playtests

- **Goal:** answer design Q1 and Q3. Q1: a full match against the AI lasts 15–25 minutes and is something the owner wants to play again. Q3: research choices visibly change what gets built in the mid-game.
- **Scope:** the owner plays. The agent's part is a match log: its length, the research order and timing, and the designs built over time. It writes the log to a file, and adds a summary tool under `Tools/`.
- **Acceptance:** the answers to Q1 and Q3, "no" included, are recorded in design §3. A failed answer is still a result (design §3).
- **Verify:** **owner run.**

---

## What finishes the MVP

The MVP is done when **Q1–Q5 are all answered and recorded in design §3**, not when they are all "yes". At that point the ADRs are frozen (AGENTS.md §6), and this plan is closed.

# Integration package — Outpost Commander MVP meshes → UWP client `\Assets`

Everything needed to get the 13 meshes from the design handoff into the client UWP executable's
`Assets\Meshes\` directory. **Start with [`INTEGRATION_PROMPT.md`](INTEGRATION_PROMPT.md)** — paste it into
Claude Code with the repository open.

The package is *convert-ready*, not merely described: the OBJ set is already generated and passes its own
verifier. Nothing here has to be produced before work can start.

## Contents

```
integration/
├── INTEGRATION_PROMPT.md     paste this into Claude Code
├── manifest.json             the asset contract — everything downstream reads this
├── obj/                      13 × .obj + .vcol, plus OC_Hull.mtl   (convert-ready)
├── scripts/
│   ├── verify_cmo.py         runnable today: content check + landmark tests
│   ├── meshes_to_obj.py      regenerate obj/ from meshes.json
│   └── build_meshes.py       OBJ → CMO orchestrator (two stages stubbed on purpose)
└── uwp/
    ├── MeshAssets.vcxproj.xml   13 <None … DeploymentContent> declarations (C++/WinRT)
    ├── MeshAssets.csproj.xml    the C# <Content … CopyToOutputDirectory> variant
    └── MeshCatalog.g.h          names, ms-appx URIs, counts, extents, hull palette constants
```

## Run this first

```
python design_handoff_meshes/integration/scripts/verify_cmo.py --obj design_handoff_meshes/integration/obj
```

It should pass clean. It asserts the face-split contract, the flat-normal contract, the vertex-colour value
set, every bounding box against the manifest, and four landmark tests chosen so that a failure **names the
axis that is wrong** rather than saying the mesh looks odd.

## The asset set

13 files, 2,674 triangles, 8,022 vertices, **13 instanced draws per frame** — one per mesh, covering every
owner at every player count.

| `Assets\Meshes\…` | Tris | Size (world units) |
|---|---|---|
| `Scout.cmo` | 158 | 54 × 18 × 60 |
| `Frigate.cmo` | 190 | 46 × 19 × 90 |
| `ModuleFrame.cmo` | 248 | 84 × 18 × 83 |
| `ModuleShipyardL1.cmo` | 204 | 64 × 30 × 90 |
| `ModuleShipyardL2.cmo` | 264 | 64 × 44 × 90 |
| `ModuleOreProcessorL1.cmo` | 212 | 78 × 28 × 76 |
| `ModuleOreProcessorL2.cmo` | 268 | 88 × 28 × 60 |
| `Station.cmo` | 500 | 220 × 54 × 220 |
| `AsteroidA…E.cmo` | 126 each | 62 / 82 / 111 / 128 / 167 longest |

## Three things that will bite

**1. A `.cmo` not declared as deployment content is not in the appx.** The source tree looks right, the
path looks right, and the runtime gets file-not-found. That is what `uwp/MeshAssets.*.xml` is for.

**2. The handedness flag is an open question, not a known answer.** The handoff authors left-handed, Y up,
+Z forward; OBJ is conventionally right-handed. An earlier draft of the build prompt asserted "do not flip
anything" with more confidence than was warranted, and that assertion is withdrawn. Resolve it by running
the conversion and reading the landmark failures, then record *why* in `build_meshes.py`.

**3. Package bytes and VRAM bytes are different numbers, and the technical design conflates them.**
≈ 426 KiB on disk, of which ≈ 188 KiB is tangent and UV zeros that CMO's fixed vertex requires. Those
zeros **compress to almost nothing in the appx** — so 426 KiB is the wrong figure for package size, and the
real one has to be measured from the block map. But they do **not** compress in VRAM: the GPU reads the
dead 24 bytes per vertex on every draw. Both numbers belong in the design, separately.

## Two stages are stubbed on purpose

`build_meshes.py` raises rather than silently producing wrong output:

- **Vertex-colour injection.** `meshconvert` has no vertex-colour input from OBJ, so it writes white or
  zero into the colour DWORD. The `.vcol` sidecars carry the real values. Shipping without this patch
  renders every hull flat white and looks like a shader bug for a week.
- **CMO decode verification.** Must go through the same `CmoReader` the client uses — a second parser
  written for the test just verifies two bugs against each other.

## What is not in scope here

The CMO reader itself (ours to write — DirectXTK12 is closed by R14), the renderer path, and the shipyard
of open questions the handoff refuses to close. All of it is laid out in `INTEGRATION_PROMPT.md` §4–§7 and
"Things to raise rather than decide".

`Cruiser` is not in this package and must not be authored. The shape-language cost of adding it later is in
[`../README.md`](../README.md) §9.

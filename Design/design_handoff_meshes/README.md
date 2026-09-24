# Outpost Commander — MVP mesh package v4

**2026-09-24 · 14 meshes · 3,522 triangles · 14 instanced draws per frame · ≈ 560 KiB of CMO**

Everything needed to integrate the MVP meshes into the repository and the client UWP executable's
`Assets\Meshes\` directory. **Paste [`INTEGRATION_PROMPT.md`](INTEGRATION_PROMPT.md) into Claude Code** with the
repository open and this package alongside it.

## Layout

```
OutpostCommander_MeshPackage_v4/
├── INTEGRATION_PROMPT.md       paste into Claude Code
├── PACKAGE.json                SHA-256 + size for every file
├── CHANGELOG.md                v1 → v4
├── Assets/Meshes/
│   ├── manifest.json           the asset contract — everything downstream reads this
│   └── obj/                    14 × .obj + .vcol, OC_Hull.mtl   (convert-ready, verified)
├── Scripts/
│   ├── verify_cmo.py           runnable now — content check + landmark tests
│   ├── build_meshes.py         OBJ → CMO (two stages stubbed on purpose)
│   └── meshes_to_obj.py        regenerate obj/ from Design/meshes.json
├── Client/
│   ├── MeshAssets.vcxproj.xml  14 DeploymentContent declarations (C++/WinRT)
│   ├── MeshAssets.csproj.xml   C# variant
│   └── MeshCatalog.g.h         names, ms-appx URIs, counts, extents, palette constants
└── Design/
    ├── README.md               the full design handoff — decisions, costs, open questions
    ├── meshes.json             the geometry specification
    ├── materials.json          palette, vertex-colour convention, light rig
    ├── frames/                 the seven plates
    └── viewer/                 open "Outpost Commander Meshes.dc.html" — live rasterised plates
```

## Quick start

```
python Scripts/verify_cmo.py
```

Should report 14/14 passing. Then follow `INTEGRATION_PROMPT.md` in order: wire the pipeline → resolve the
handedness flag from the landmark failures → implement the two stubbed stages → CMO reader → UWP loading →
renderer.

## The meshes

| `Assets\Meshes\…` | Tris | Size (world units) |
|---|---|---|
| `Scout.cmo` | 158 | 54 × 18 × 60 |
| `Frigate.cmo` | 190 | 46.4 × 19 × 90 |
| `Cruiser.cmo` | 316 | 64 × 35 × 180 |
| `ModuleFrame.cmo` | 248 | 83.5 × 18 × 83.2 |
| `ModuleShipyardL1.cmo` | 204 | 64.4 × 30 × 90 |
| `ModuleShipyardL2.cmo` | 264 | 64.4 × 44 × 90 |
| `ModuleOreProcessorL1.cmo` | 212 | 78.4 × 28 × 76.3 |
| `ModuleOreProcessorL2.cmo` | 268 | 88.3 × 28 × 60 |
| `Station.cmo` | 1,032 | 220 × 86 × 220 |
| `AsteroidA.cmo` | 126 | 62.4 × 45.2 × 59.5 |
| `AsteroidB.cmo` | 126 | 82.5 × 60.6 × 77.2 |
| `AsteroidC.cmo` | 126 | 111.1 × 81.6 × 93.5 |
| `AsteroidD.cmo` | 126 | 122.1 × 96.5 × 127.9 |
| `AsteroidE.cmo` | 126 | 166.9 × 141.4 × 140.8 |

## Needs a ruling before or during integration

1. **Cruiser length, 180 units** — not echoed from the brief. Confirm against the catalog.
2. **Handedness flag for `meshconvert`** — unresolved; determine from the landmark tests, record why.
3. **Near/far clip planes** — unspecified; report z-fighting risk, don't choose.
4. **Combat camera distance** — the brief says both ~1,400 and (implicitly) 1,500.
5. **Shape-coded overlay** — approved as a spec, not on the MVP readout list; needs a register entry. No Cruiser glyph.
6. **Attachment points** for the mining laser and mass drivers — not authored; request, don't infer.
7. **Design documents still state the withdrawn "meshes are functions" rule** — report, don't fix here.
8. **Package vs VRAM bytes** — measure the appx delta from the block map; ≈ 248 KiB of tangent/UV zeros compress away on disk but not in VRAM.

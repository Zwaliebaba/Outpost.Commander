# Integration prompt — Outpost Commander MVP meshes into the UWP client

Paste everything below the rule into Claude Code, with this repository open.

---

You are integrating a completed design handoff into the Outpost Commander client. **Package: `OutpostCommander_MeshPackage_v4` (2026-09-24).** Paths below are relative to the package root. `PACKAGE.json` lists a SHA-256 for every file — check them after copying. The handoff is at
this package and the integration package is at the package root. Read
`Design/README.md` in full first, then `Assets/Meshes/manifest.json`. **Read the plates as
evidence, not as specification** — `meshes.json` is the specification and `manifest.json` is the contract
between it and the client.

**The ruling this work is written against, 2026-09-22:** ADR-005's surviving half — *meshes are functions*
— is withdrawn. Meshes are **CMO files**, authored as content, shipped with the package under ADR-021.
**The tree does not say this yet.** ADR-005, `TechnicalDesign.md` §7, `GameDesign.md` §10,
`Design/README.md`, M1.9, M2.4 and M2.10b all still state the old rule. **Correcting them is a separate
change — do not fold it into this one, and do not reason onward from the old rule because you found it
still written down.** The one documented defect in the HUD handoff was a claim inherited from a brief and
reasoned onward from; do not add a second.

**Target:** the objects ship in the client UWP executable's `\Assets` directory, as
`Assets\Meshes\<Name>.cmo` — 14 files. They are reachable at runtime as
`ms-appx:///Assets/Meshes/<Name>.cmo`.

## What the package already contains, so you do not rebuild it

| Path | What it is |
|---|---|
| `Assets/Meshes/manifest.json` | the asset contract: per mesh, logical name, target asset path, counts, extents, draw group. Also the palette, the light rig, the vertex-colour convention and the landmark tests. **Everything downstream reads this, not the handoff prose.** |
| `Assets/Meshes/obj/*.obj` | **14 convert-ready OBJ files, already generated.** Left-handed, Y up, +Z forward, world units 1:1, face-split, baked per-face normals, no `vt`. |
| `Assets/Meshes/obj/*.vcol` | the vertex-colour selector, one `R G B A` line per `v` record in the same order. OBJ has no vertex-colour field, so it travels out-of-band. |
| `Assets/Meshes/obj/OC_Hull.mtl` | the one material, identical for every mesh |
| `Scripts/meshes_to_obj.py` | regenerates the OBJ set from `meshes.json`. You only need it if `meshes.json` changes. |
| `Scripts/verify_cmo.py` | **runnable today.** OBJ-stage content check + the landmark tests. CMO-stage decode assertions are stubbed for you to fill in. |
| `Scripts/build_meshes.py` | the OBJ → CMO orchestrator. Two stages are deliberately stubbed — see below. |
| `Client/MeshAssets.vcxproj.xml` | the 14 `<None … DeploymentContent>` declarations, C++/WinRT |
| `Client/MeshAssets.csproj.xml` | the C# `<Content … CopyToOutputDirectory>` variant |
| `Client/MeshCatalog.g.h` | generated catalog: names, package URIs, counts, extents, hull palette as shader constants |

**Start by running the check that already works:**

```
python <package>/Scripts/verify_cmo.py
```

It asserts the face-split contract, flat-normal contract, the vertex-colour value set, every bounding box
against the manifest, and the four landmark tests. It should pass clean. If it does not, stop — the
content is wrong and nothing downstream is worth doing.

## Build it in this order

### 1. Wire the asset pipeline

- Copy `Assets/Meshes/obj/` to `Assets/Meshes/obj/` in the repo (source, checked in, diffable).
- Copy `Scripts/*.py` to `Scripts/`.
- Copy `Assets/Meshes/manifest.json` to `Assets/Meshes/manifest.json` — **and ship it in the appx too.**
  The runtime catalog asserting against the same file the build validated against is worth the 8 KB.
- Add the `Client/MeshAssets.*.xml` item group to the client project. **Every `.cmo` must be
  declared as deployment content or it will not be in the appx**, and the failure mode is a
  file-not-found at runtime on a path that looks perfectly correct in the source tree.

### 2. Resolve the handedness flag — **empirically, once, and record why**

The handoff authors left-handed, Y up, +Z forward. OBJ is conventionally right-handed. **Whether
`meshconvert` needs a flip flag for this input is an open question and the handoff does not settle it.**
An earlier draft of the build prompt asserted "do not flip anything" with more confidence than was
warranted; that assertion has been withdrawn.

Resolve it by running the conversion and reading the landmark failures, which are designed to name the
axis that is wrong rather than just saying the mesh looks odd:

| Failure | Meaning |
|---|---|
| `Frigate` max-Z vertex is not the nose at z = +45, x ≈ 0 | Z is flipped |
| `Cruiser` max-Z slice is not the ~28-unit prow, narrower than the flared tail | Z is flipped |
| `ModuleShipyardL1` min-Z slice is flat and max-Z slice is tall | Z is flipped — the tall command block is aft, the flat lattice forward |
| `ModuleOreProcessorL2` max-Y vertex (the hatch over the main drum) is at +X | X is mirrored |
| `Station` max-Y feature (the mast) is off-centre, or size ≠ 220 × 86 × 220 | Y is flipped, or a scale factor crept in |

Then set `MESHCONVERT_FLAGS` in `build_meshes.py`, **write the reason in the comment above it**, and never
revisit it.

**Pin `meshconvert`.** Fill in `MESHCONVERT_RELEASE` with the exact DirectXMesh release tag or commit sha
you tested against, and make the not-found error say which version it wanted. This is the step that breaks
on somebody else's machine. It is a **tool in the build, not a linked library** — R14 survives on exactly
that distinction. Do not add DirectXMesh as a dependency and do not vendor its source.

### 3. Implement the two stubbed stages in `build_meshes.py`

**Vertex-colour injection.** `meshconvert` has no vertex-colour input from OBJ, so it writes white or zero
into the colour DWORD. Patch it from the `.vcol` sidecars. The CMO vertex stride is 52 bytes — position
`float3` at 0, normal `float3` at 12, tangent `float4` at 24, colour `uint32` at 40, texcoord `float2` at
44. Assert the vertex count you find equals the sidecar length **before writing anything**: a mismatch
means `meshconvert` welded or reordered, which invalidates the whole face-split contract and must abort the
build. The stub currently raises on purpose — shipping meshes with a white colour channel would look like
a shader bug for a week.

**CMO verification.** Read each file back **through the same `CmoReader` the client uses**, not a second
parser written for the test, or you are verifying two bugs against each other. Assert vertex count,
triangle count and bounding box against the manifest, re-run the landmark checks on the decoded positions,
and assert the colour DWORDs are not all white or zero — that is the signature of skipped injection.

### 4. The CMO reader — `Source/Content/CmoReader.{h,cpp}`

**CMO's only reader in the wild is DirectXTK12, which R14 closes by name.** So this is ours to write, and
ADR-021's first boundary names it again. **Budget a few hundred lines, not forty.**

What the MVP uses: one submesh per file, one material per mesh, the 52-byte vertex above, 16-bit indices.
No mesh exceeds 65,535 vertices — the largest is `Station` at 3,096 — so the index width is safe with a
wide margin.

What a conforming reader has to **skip correctly** even though every one of these is empty here:

- materials with **eight texture-name slots**, each still carrying a length-prefixed string to consume
- the **skinning** section, **bone hierarchy**, **bone influences** and **animation clips**

Skipping a section you have never seen populated is the classic place this goes wrong. **Write the skip
path against the format, not against these files, and unit-test it with a hand-built buffer that *does*
populate them.** Read CMO's own extents block but **assert it against the manifest rather than trusting
it.**

Fail loudly, not quietly: a vertex whose R byte is neither 0 nor 255 is a content error, not something to
clamp.

### 5. UWP loading — the install location is read-only

- Resolve assets through `ms-appx:///Assets/Meshes/<Name>.cmo`, or
  `Package::Current().InstalledLocation()` joined with the relative path. Do not build paths from the
  executable's own directory.
- **The appx install location is read-only.** Nothing writes beside these files. If you ever want to cache
  a repacked vertex buffer (see §7), it goes in `ApplicationData::Current().LocalFolder()`, never next to
  the asset.
- Load all 14 at startup, not on demand. The whole set is ~560 KiB and the game has 14 meshes; a streaming
  path is complexity with no payer.
- A missing or malformed asset should fail at startup with the logical name in the message, not at first
  draw.

### 6. The renderer path

- **One instanced draw per mesh. 14 of them**, listed in `MeshCatalog.g.h` in catalog order. The owner's
  colour arrives as **per-instance constant data** — never a material, never a second submesh. That
  requirement is the entire reason the vertex-colour selector exists.
- Pixel shader: `albedo = lerp(HULL[G], TEAM[owner], R/255)`, with `kHullPalette` from the catalog as
  shader constants and `G` quantised to `{0, 128, 255}`.
- **Flat shading. Do not renormalise, do not generate smooth normals, do not build a tangent basis.** The
  normals are baked per face and the tangents are zeros by design.
- The light rig in `manifest.json` is part of the specification, not a suggestion — the handoff plates are
  not reproducible without it. Key, fill, ambient exactly as stated; **ambient multiplies albedo and is
  never added flat**, or it lifts the `#04060A` backdrop.
- **A ship banks as it turns.** That is a transform the renderer applies. There is no animation in these
  files and no bones to drive.
- Asteroids: one draw per variant, five variants, picked per rock from the match seed. Jitter is
  **yaw/pitch/roll unconstrained on all three axes, uniform scale 0.75–1.35, ±240 units of Y offset.**
  **Uniform scale only** — non-uniform scale breaks the baked per-face normals, which nothing in this
  chain renormalises.

### 7. Two numbers that are not the same number, and the technical design conflates them

**Package bytes ≠ VRAM bytes, and appx compression is why.**

- **On disk, pre-packaging: ≈ 560 KiB** across 14 files. Of that, **≈ 248 KiB is tangent and UV zeros** —
  CMO's vertex is fixed and the handoff writes those fields as zeros, as the brief requires.
- **In the appx: much less.** Appx content is deflate-compressed, and 248 KiB of literal zeros compresses
  to almost nothing. **Measure the real figure** from the built package's block map and put it in the
  technical design — do not carry 560 KiB there, because it is the wrong number for package size.
- **In VRAM: the full ≈ 537 KiB of vertex buffer, uncompressed, zeros included.** The GPU reads the dead
  24 bytes per vertex on every draw. This is the number that matters for the vertex-fetch cost and it is
  the one nobody writes down.

**Raise, do not decide:** whether to repack CMO into a 28-byte vertex at load time. It halves vertex
fetch, costs a load-time transform and a second vertex layout, and CMO stays the shipped format either
way. At 3,522 triangles it is nowhere near a bottleneck. Report the numbers and let the owner rule.

## Things to raise rather than decide

These are open in the handoff. A ruling belongs to the owner, not to you. **Ask; do not pick.**

1. **Near and far clip planes are unspecified.** The tactical camera sits at 22,500 units over hulls 18–86
   units tall. Work out whether a 24-bit depth buffer z-fights across the Station's 12-unit plate step at
   your candidate planes and **report the number** — do not choose the planes.
2. **Combat camera distance: the brief says both ~1,400 and (implicitly) 1,500.** README §10 has the
   arithmetic. Leave both standing and flag it; the plates use 1,500.
3. **Attachment points.** The mining laser and the two mass drivers have slot positions implied by hull
   geometry but not authored. If the renderer needs explicit attachment transforms, that is a change to
   what `meshes.json` carries — **request it; do not infer positions from the vertices.**
4. **Asteroid ore state has no visual.** Not invented in the handoff. Do not invent one.
5. **Whether a mesh may be authored off-origin.** The handoff says the mesh origin is the simulated
   position; it does not say the *centroid* must be centred. `ModuleOreProcessorL2` is deliberately
   asymmetric in X, the shipyard truss is heavily asymmetric in Z, and `Station`'s hub puts its volume
   centroid at Y ≈ +6. If anything asserts a centred centroid, those three fail.
6. **The shape-coded overlay (plate 7)** is a readout that is not on the MVP list. It was approved as a
   design specification on 2026-09-22 but needs a question-register entry and a HUD-handoff amendment
   before it is built. **Do not build it as part of this change.** It is also the reason the hulls are
   tuned for the combat view rather than bent into shapes that lose at 3.5 px — see README §1.

## What not to do

- Do not add a mesh for anything not in `manifest.json`. **The `Cruiser` is in it as of 2026-09-24**, on the
  owner's instruction, overriding the brief. Its 180-unit length is the one size not echoed from the brief —
  flag it for confirmation against the catalog, do not change it. The overlay (plate 7) has **no Cruiser
  glyph**, deliberately; do not invent one.
- Do not add textures, normal maps, UV layouts, skinning, bones or animation clips. CMO carries a slot for
  every one and every one stays empty.
- Do not weld, merge, reorder or reindex the vertices at any stage. The arrays are face-split and the
  baked per-face normals depend on it. `verify_cmo.py` will catch it; do not suppress the check.
- Do not scale a mesh at draw time. The authored extent **is** the object's size and the verifier asserts
  it in world units.
- Do not introduce a linked third-party library. `meshconvert` is a tool in the build.
- Do not "fix" the design documents that still state the old rule. **Report them.**

## Definition of done

1. `verify_cmo.py --obj` passes clean from a fresh checkout.
2. `Scripts/build_meshes.py --out <ClientProject>/Assets/Meshes` runs clean, with the handedness flag
   resolved and its reason recorded, and `meshconvert` pinned.
3. 14 `.cmo` files land in the client's `Assets\Meshes\`, all declared as deployment content, and
   **present in the built appx** — verify that, do not assume it.
4. Every file round-trips through `CmoReader` with vertex count, triangle count and bounding box asserted
   against `manifest.json`, the landmark tests passing on decoded positions, and the colour channel proven
   non-white.
5. The renderer draws a Station, four distinct modules, a Scout and a Frigate in two team colours in **14
   instanced draws**, flat-shaded, against `#04060A`.
6. The measured appx size delta and the measured VRAM vertex-buffer size are both written down, separately
   (§7).
7. The open questions above are written up as a list for the owner **rather than answered.**

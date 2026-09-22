> **SUPERSEDED, 2026-09-22.** Use [`integration/INTEGRATION_PROMPT.md`](integration/INTEGRATION_PROMPT.md)
> instead. It targets the UWP client's `\Assets` directory, ships a convert-ready OBJ set and an asset
> manifest, and **withdraws this file's claim that `meshconvert` needs no flip flag** — that was asserted
> with more confidence than was warranted and is now an empirical check with named landmark tests.
> This file is kept for provenance only.

# Claude Code prompt — build from the mesh handoff

Paste everything below the rule.

---

You are working in the Outpost Commander repository. A design handoff has landed at
`design_handoff_meshes/`. Read `design_handoff_meshes/README.md` first, in full, then
`meshes.json` and `materials.json`. Do not read the plates as specification — they are evidence;
`meshes.json` is the specification.

**The ruling this handoff is written against, 2026-09-22:** ADR-005's surviving half — *meshes are
functions* — is withdrawn. Meshes are **CMO files**, authored as content, shipped with the package under
ADR-021. **The tree does not say this yet.** ADR-005, `TechnicalDesign.md` §7, `GameDesign.md` §10,
`Design/README.md`, M1.9, M2.4 and M2.10b all still state the old rule. **Correcting them is a separate
change from this one — do not fold it in, and do not reason onward from the old rule because you found it
still written down.**

## What to build, in this order

### 1. `Scripts/meshes_to_obj.py` — the bridge

`meshconvert` eats FBX, OBJ, VBO and SDKMESH. `meshes.json` is none of them. **The ruling is OBJ:** text,
diffable, reviewable in a pull request, and no FBX SDK in the build.

Emit one `.obj` + one `.mtl` per mesh into `Assets/Meshes/obj/`. Requirements that are not negotiable
because the rest of the chain depends on them:

- **Do not weld, do not merge, do not reorder.** `meshes.json` is already face-split with baked per-face
  normals. Every three vertices are one triangle and the indices are sequential. Emit `v`/`vn` in the
  file's own order and one `f` per triangle with explicit `v//vn`.
- **Do not emit `vt`.** There is no UV layout and nothing reads one.
- OBJ has no vertex-colour field in the base spec. **Carry the R and G selector bytes out-of-band**: emit
  a sidecar `<mesh>.vcol` of one `R G B A` line per vertex in the same order, and have step 2 read it.
  Do not use the non-standard 6-float `v x y z r g b` extension — `meshconvert` ignores it silently, which
  is the worst possible failure for a channel that carries team identity.
- Assert on write: vertex count is a multiple of 3; every R byte is exactly 0 or 255; every G byte is
  exactly 0, 128 or 255; every B byte is 0. No mesh exceeds 65,535 vertices, so 16-bit indices are safe
  with a wide margin — the largest is `Station` at 1,500. The handoff guarantees all four. A failure here means the
  content changed, not that the reader is wrong.

### 2. The `meshconvert` step

Third-party tool from DirectXMesh, in the build, invoked from `Scripts/build_meshes.py`:

- Pin the version and record the exact commit or release tag in `Scripts/build_meshes.py` as a comment.
  This is the step that fails on somebody else's machine; make the failure say which version it wanted.
- Flags: `-cmo -nodds -flipz? NO` — **do not flip anything.** The handoff is authored left-handed, Y up,
  +Z forward, which is already `meshconvert`'s native convention for CMO. Any flip flag silently mirrors
  every hull and the plan outlines are not symmetric front-to-back.
- Verify after conversion, do not assume: read each `.cmo` back and assert vertex count, triangle count
  and bounding box against the `triangles` / `vertices` / `extents` fields in `meshes.json`. **Extents are
  asserted in world units with no scale factor** — the handoff's authored extent *is* the object's size.
- Re-inject the vertex colours from the `.vcol` sidecars in this step, since `meshconvert` will have
  written zeros or white.
- Output to `Assets/Meshes/*.cmo`, shipped with the package.

### 3. The CMO reader — `Source/Content/CmoReader.{h,cpp}`

**CMO's only reader in the wild is DirectXTK12, which R14 closes by name.** So this is ours to write, and
ADR-021's first boundary names it again. **Budget a few hundred lines, not forty.**

What the MVP actually uses:

- One submesh per file, one material per mesh, vertex format
  `position float3 · normal float3 · tangent float4 · colour uint32 · texcoord float2` = 52 bytes,
  16-bit indices.

What a conforming reader has to **skip correctly** even though the MVP never uses it:

- Materials with **eight texture-name slots** — all empty here, all of which still have a length-prefixed
  string to consume.
- The **skinning** section, **bone hierarchy**, **bone influences** and **animation clips**. All absent
  here. Skipping a section you have never seen populated is the classic place this goes wrong; write the
  skip path against the format, not against these files, and unit-test it with a hand-built buffer that
  *does* populate them.
- CMO's extents block. Read it, but **assert it against the handoff figures rather than trusting it.**

Fail loudly, not quietly: a mesh whose vertex colour R byte is neither 0 nor 255 is a content error, not
something to clamp.

### 4. The renderer path

- **One instanced draw per shape.** 13 of them (2,674 triangles, 8,022 vertices, ≈ 426 KB of CMO in total): `Scout`, `Frigate`, `ModuleFrame`, `ModuleShipyardL1`,
  `ModuleShipyardL2`, `ModuleOreProcessorL1`, `ModuleOreProcessorL2`, `Station`, `AsteroidA`…`AsteroidE`.
  The owner's colour arrives as **per-instance constant data**. It is not a material and it is not a
  second submesh — that requirement is what the vertex-colour selector exists to satisfy.
- Pixel shader: `albedo = lerp(HULL[G], TEAM[owner], R/255)`, with `HULL[]` the three palette entries from
  `materials.json` as shader constants and `G` quantised to `{0, 128, 255}`.
- **Flat shading. Do not renormalise, do not generate smooth normals, do not build a tangent basis.** The
  normals are baked per face and the tangents are zeros by design.
- The light rig in `materials.json` is part of the specification, not a suggestion — the plates are not
  reproducible without it. Key, fill, ambient, exactly as stated; ambient multiplies albedo and is never
  added flat.
- **A ship banks as it turns.** That is a transform the renderer applies. There is no animation in these
  files and no bones to drive.

### 5. Asteroid instancing

One draw per variant, five variants, picked per rock from the match seed. The jitter the variants were
authored for: **yaw, pitch and roll unconstrained on all three axes; uniform scale 0.75–1.35; ±240 units
of Y offset.** Uniform scale only — a non-uniform scale breaks the baked per-face normals, which nothing
in this chain renormalises.

## Things to raise rather than decide

These are open in the handoff and a ruling belongs to the owner, not to you. **Ask; do not pick.**

1. **Near and far clip planes are unspecified.** The tactical camera sits at 22,500 units over hulls 6–44
   units tall. Work out whether a 24-bit depth buffer z-fights across the Station's 10-unit plate step at
   your candidate planes, and report the number — do not choose the planes.
2. **Combat camera distance: the brief says both ~1,400 and (implicitly) 1,500.** README §10 has the
   arithmetic. Leave both standing and flag it; the plates use 1,500.
3. **Attachment points.** The mining laser and the two mass drivers have slot positions implied by hull
   geometry but not authored. If the renderer needs explicit attachment transforms, that is a change to
   what `meshes.json` carries — request it, do not infer positions from the vertices.
4. **Asteroid ore state has no visual.** Not invented in the handoff. Do not invent one.
5. **The shape-coded overlay (plate 7)** is a readout that is not on the MVP list. It was approved as a
   design specification on 2026-09-22 but it needs a question-register entry and a HUD-handoff amendment
   before it is built. **Do not build it as part of this change.**

## What not to do

- Do not add a mesh for anything not in `meshes.json`. `Cruiser` is in the catalog and the MVP never
  builds it; **do not author it**, and do not add a parameterised fallback that generates it.
- Do not add textures, normal maps, UV layouts, skinning, bones or animation clips. CMO carries a slot for
  every one of those and every one stays empty.
- Do not introduce a linked third-party library. `meshconvert` is a **tool** in the build, not a
  dependency, and R14 survives on exactly that distinction. Keep it that way.
- Do not "fix" the design documents that still state the old rule. Report them.

## Definition of done

`Scripts/build_meshes.py` runs clean from a fresh checkout; 13 `.cmo` files land in `Assets/Meshes/`;
every one round-trips through `CmoReader` with vertex count, triangle count and bounding box asserted
against `meshes.json`; the renderer draws a Station, four distinct modules, a Scout and a Frigate in two
team colours in 13 instanced draws; and the open questions above are written up as a list for the owner
rather than answered.

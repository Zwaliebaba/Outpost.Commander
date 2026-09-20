# ADR-011 — Model normals are baked by the loader, not taken from the pixel shader's derivatives

**Status:** Accepted
**Date:** 2026-09-18
**Owner:** the author, on the choice `TechnicalDesign.md` §6.4 left open and `m1-vertical-slice/K1` asked to settle

## Context

`TechnicalDesign.md` §6.4 wrote the model path down with one thing deliberately unfixed:

> positions as `float`, one colour per vertex, no normals — the face normal is derived in the pixel
> shader from the screen-space derivatives of the world position, or the loader splits vertices and
> bakes one per triangle; the ADR that lands the first model decides.

`m1-vertical-slice/K1` is that task. Both halves of the choice already exist in the tree as working
code to argue from: `NeuronClient/Shaders/TerrainPS.hlsl` takes its normal from
`cross(ddy(world), ddx(world))` and has done since M0, and `NeuronCore/ModelDesc.h` holds a triangle
list with a colour per triangle that a loader would have to split anyway.

The question had to be answered now because the geometry pass cannot be written either way without
answering it: the vertex format, the input layout and the pixel shader all differ.

## Decision

**`NeuronClient/ModelBuffers.cpp` splits a model's vertices and bakes one normal per triangle.** The split
is keyed on the triple *(the description's vertex, the triangle's colour, the triangle's direction)*,
so two triangles that agree on both share their vertices and only a disagreement costs a duplicate.
`NeuronClient/Shaders/GeometryPS.hlsl` carries the normal `nointerpolation` and does not normalize it,
because the loader wrote it to unit length and a flat attribute reaches the pixel shader from the
triangle's first vertex without arithmetic.

**The reason is that the split is already paid for.** A Frontier model is one colour per triangle,
and a flat attribute in Direct3D comes from the *provoking* vertex — the triangle's first. A vertex
that two triangles of different colours share therefore cannot carry both colours and must already
be two vertices. Over the twenty-two models in `GameData\Models`, splitting on the colour alone —
what a renderer taking its normal from the derivatives would still have to do — costs **232**
vertices against the description's 176. Adding the normal to the key costs **528**. Both are
rounding errors: 3.7 kB against 14.8 kB for the whole model set. What the 11 kB buys is a normal
that is exact, view-independent and checkable on the CPU.

**The argument that decided it is not the arithmetic, it is the sign.** `cross(ddy, ddx)` fixes a
normal up to a sign that depends on which way the triangle wound in *screen* space. `TerrainPS.hlsl`
resolves that with `if (normal.y < 0) normal = -normal;`, which is not a general fix — it is the
terrain knowing that the ground faces up. A model has faces pointing in every direction, including
vertical ones where `normal.y` is zero and the sign is then decided by floating-point noise, so the
terrain's rule is unavailable and there is no replacement for it that does not come back to the
winding. A baked normal takes the winding once, at load, in integer arithmetic, where it can be
*checked*: `ModelMesh::inwardFacingTriangles` counts the triangles whose normal points at the
model's centroid, `ModelBuffers` logs a non-zero count as a warning, and a test pins it.

**The outward normal is `cross(c - a, b - a)`, the SDK's handedness and not the textbook's.** This
follows from the data rather than from taste: `Tools/MakePlaceholderModels.py` winds a front face
clockwise seen from outside, which is what `D3D12_RASTERIZER_DESC::FrontCounterClockwise = FALSE`
calls front, and under the right-hand rule that winding's cross product points *into* the solid.
All 264 triangles of the twenty-two *placeholder* models were wound that way, with no exception —
and every one of the twenty-two **authored** models that replaced them arrived wound the other way,
because an OBJ is counter-clockwise from outside. See the 2026-09-19 measurements below: the
convention did not move, the importer was corrected to meet it.
`ModelBuffersTests.cpp` asserts the six face normals of a box built exactly as the generator builds
one, because getting this backwards lights every model from inside and the symptom — everything
black — does not say why.

**The geometry pass draws both faces.** With the normal baked, culling no longer decides anything
about lighting, so it would buy fill rate alone, and a model wound the other way would become a hole
rather than a dark face the diagnostic already counts. `D3D12_CULL_MODE_NONE` matches what
`TerrainPass` does and for the same stated reason.

**The normal stays a `float3`.** A packed `R8G8B8A8_SNORM` normal would take the vertex from 28
bytes to 20 and is available the moment vertex bandwidth is measured to matter; at 14.8 kB for the
entire model set it does not, and a packed normal is a thing to debug rather than read.

**Positions become world units in this loader.** A `ModelDesc` is in subunits, 256 to the world unit
(`NeuronCore/FixedPoint.h`), because `Content` holds the simulation's numbers; the camera, the terrain and
the render view are in world units. The conversion happens once at load rather than per frame, and
`NeuronClient/ModelBuffers.h` is the one place it happens.

## Consequences

- **A model's triangles can no longer share a vertex across a crease**, which is the whole point,
  and it forecloses smooth shading on a model without a second vertex format. That is intended:
  `TechnicalDesign.md` §6.4 says one normal and one colour per triangle, and `SpeciesLook.md` §2
  says why — a faceted model lit by two hard directional lights is the Species look.
- **The vertex count is data-dependent**, so a pathological model could cost three vertices a
  triangle. The index buffer is `DXGI_FORMAT_R32_UINT` rather than the terrain's 16-bit so that the
  ceiling is not something authored data can cross without the code noticing.
- **The pixel shader gets cheaper and the vertex shader gets a little more expensive.** Two
  `ddx`/`ddy` pairs, a cross and a normalize leave the pixel shader; a rotation of the normal by the
  instance's heading — two multiply-adds — joins the vertex shader. Not measured on a GPU; see
  below.
- **The derivative path is not kept as an alternative.** Two ways to light a model is two ways for
  it to be wrong, and `TerrainPS.hlsl` remains the reference for anyone who needs to see the other
  approach.
- **What reopens this**: a model format that wants smooth normals, or a measured vertex-fetch cost
  at an instance count M1 does not reach. Neither is in view.

## Measurements

> **THE PLACEHOLDER FIGURES BELOW ARE SUPERSEDED. They were measured on the twenty-two generated
> primitives and the authored models replaced every one of them on 2026-09-19, which nothing
> re-measured until `m1-vertical-slice/C10`. They are kept because the reasoning they support — the
> split's cost, the handedness — is unchanged; the numbers are not the tree's.**

### The authored models, measured 2026-09-19 (`m1-vertical-slice/C10`)

Twenty-two models: **5,429** vertices and **9,632** triangles in the descriptions, against the
placeholders' 176 and 264.

**Every one of them arrived wound inside out.** The signed volume their faces enclose — the centre
of each face dotted with its outward normal, summed, which is six times the volume for a closed
mesh — was **negative on all twenty-two**, decisively and by several orders of magnitude each. The
cause is the importer: a Wavefront OBJ face is counter-clockwise seen from *outside*, this ADR's
outward normal is `cross(c − a, b − a)` which is clockwise from outside, and `Tools/ImportObj.py`
carried the OBJ's corner order straight through. It reverses each face now, and the twenty-two
models in the tree were flipped to match; the signed volume is positive on all twenty-two.

**The winding was already consistent, which is why this was a one-line cause.** Welding by position
and counting directed edges, **13,930 of 14,576** shared edges were traversed in opposite directions
by their two triangles, with 390 same-direction and 256 on open boundaries; fifteen of the
twenty-two models are perfectly consistent. These are correctly built closed surfaces that were
simply facing the wrong way — not meshes with scrambled faces.

**And `inwardFacingTriangles` is a diagnostic rather than a gate, which is this ADR corrected.** It
was introduced above on the promise that "a closed solid has none of these". That holds for a
*convex* solid — the boxes and cones it was measured on — and not for authored geometry: a thin
plate has two large faces with the centroid between them, so one of them points at the centroid
however the plate is wound. Over these models it read **6,356 of 9,632** while every model was
inside out and **3,276** once they were corrected. It moved the right way and reached neither zero
nor the total, so it can neither pass nor fail a build. `ModelMesh::signedVolumeSubunits` is the
orientation test now, it is exact for a closed mesh, and a negative one is logged at **Error** where
the count was logged at Warning — which is how 6,356 sat in a green build for a day.

### The placeholder figures, measured 2026-09-18

- **The vertex counts and the winding**, measured on 2026-09-18 by building every model in
  `GameData\Models` through `Outpost::LoadContent` and `Neuron::BuildModelMesh` — the shipped
  loader and the shipped builder — in a native Linux build of `Content` and `NeuronClient/ModelBuffers.cpp`
  against the session's stub platform headers. Twenty-two models; the descriptions hold **176**
  vertices and **264** triangles; the built meshes hold **528** vertices and **792** indices, which
  is 14,784 bytes of vertices at `sizeof(GeometryVertex)` = 28 and 3,168 bytes of indices;
  **0** triangles face inward. The colour-only split, computed in the same run as the number of
  distinct *(vertex, colour)* pairs, is **232** vertices, 3,712 bytes at a 16-byte vertex with no
  normal. Every model is a box, so every one splits 8 into 24 and the totals are that times
  twenty-two; a box is the worst case for this split and the best case for sharing, which is worth
  knowing when authored models arrive.
- **The handedness** is arithmetic on the generator's own corner order, not a GPU observation. For
  the top face `(4, 5, 6, 7)` of a box of half-length *l* and half-width *w*,
  `cross(v6 − v4, v5 − v4)` = `(0, 4lw, 0)`, which is +y and outward; the same arithmetic gives the
  outward normal for all six faces, and `ModelBuffersTests.cpp` asserts all six.
- **`ModelBuffersTests.cpp` passes 11 of 11** in the session's native runner (clang 18 on Linux,
  `NeuronClient/ModelBuffers.cpp` compiled against the stub platform headers). Nothing here was run on a
  GPU.
- **WHAT IS NOT MEASURED, AND IT IS THE THING K1'S ACCEPTANCE ASKED FOR.** K1 says the ADR decides
  "with a captured frame of a placeholder model", and there is no such frame: `m1-vertical-slice/G2`
  is what composes devices into the capture, and `m1-vertical-slice/R2` is what fills the render
  view, so the first frame with a model in it does not exist yet. This decision rests on the
  arithmetic above and on the sign argument, and neither needs a frame. What the frame is still owed
  for, and what would overturn this: a visible facet seam or a shading discontinuity that the baked
  normal cannot explain, or a frame time in which the extra vertices are measurable against the
  pixel-shader work they removed. The frame is to be taken when G2 lands, and this ADR is superseded
  rather than edited if it says something else.

# ADR-009 — Text is DirectWrite rasterised into an atlas we own

**Status:** Accepted — **the DirectWrite decision stands; its pixel-doubling consequence is superseded
by [`ADR-011`](ADR-011-the-interface-draws-after-the-scale.md)**, which moves the interface out of the
scene target so glyphs are rasterised at physical size.
**Date:** 2026-09-20
**Owner:** Stefan Zwaal

## Context

The interface needs glyphs for credits, costs and counts (`Design/Interface.md` §6), and there is no text
renderer in this tree. R14 closes the dependency list, so no font library and no font *file* may arrive
without a decision.

**The reflex answer is ruled out before the question starts.** Drawing text onto a Direct3D 12 surface is
conventionally Direct2D over `ID3D11On12Device`, and **R12 bans both D3D11 and D3D11On12 by name.** That
closes the whole D2D interop route, which is worth stating because it is what every sample reaches for.

## Decision

**DirectWrite rasterises a system font into a Direct3D 12 texture atlas at startup**, and the interface
draws glyph quads from it, instanced. `DWriteCreateFactory`, `GetSystemFontCollection`,
`IDWriteFontFace::CreateGlyphRunAnalysis`, then `GetAlphaTextureBounds` and `CreateAlphaTexture` to
obtain the coverage bytes, which we upload ourselves. **No Direct2D, no D3D11On12, and no dependency** —
DirectWrite is `dwrite.h` in the Windows SDK, and both that interface and that method are documented for
the UWP app family as well as for desktop.

**Coverage is rasterised as `DWRITE_TEXTURE_CLEARTYPE_3x1` and the three subpixel values are averaged into
one 8-bit channel — and the average is taken in linear space, not on the stored bytes.** ClearType
coverage is gamma-encoded; averaging the encoded values directly produces systematically thin or fat
stems, which is the second thing a naive implementation gets wrong after the fringing below. This is not
an oversight and it is the detail a naive implementation gets wrong:
ClearType assumes an RGB stripe at the *final* display, and this frame is scaled 2× on the way there
([`ADR-007`](ADR-007-the-authored-frame-is-1440x960.md)), so subpixel output would arrive as colour
fringing that survives the scale. `CreateAlphaTexture` offers only bi-level and ClearType, and averaging
ClearType is how you get grayscale antialiasing out of it.

**The font family is a constant — Segoe UI — and a missing family is a startup failure, not a
substitution.** R13 requires every layout number to be unconditional, and a substituted font has different
advance widths, so a silent fallback would quietly break every position in the interface on some machine
nobody tests on.

**Glyphs are rasterised at the physical size the fit transform produces** — 48 physical pixels for a
24-authored-pixel label on a Surface Pro — and drawn in the interface pass, after the scale
([`ADR-011`](ADR-011-the-interface-draws-after-the-scale.md)). They were originally rasterised at authored
size into the scene target, which doubled them.

**The atlas is therefore sized against the window rather than against a constant, and a resize
invalidates it.** So does device removal. Both are rebuild paths this decision did not previously have,
and both must not stall a frame visibly.

## Consequences

**Text is pixel-doubled on the target device, and that is worth being explicit about.** The scene target is
1440 × 960 and the fit is an exact 2×, so a glyph rasterised at 24 authored pixels reaches the glass as
48 physical pixels made of 2 × 2 blocks. R13 anticipates exactly this — "a glyph baked to an exact pixel
height reaches the glass resampled unless the scale is exactly 1" — and names a dense interface full of
small type as where it hurts.

**This interface is the opposite of that case**, which is why the cost is acceptable: the type is large
because the minimum touch target is 9 mm, and there are perhaps a dozen strings on screen. And a doubled
pixel at 267 PPI is a **133-PPI effective pixel**, which is ordinary desktop monitor density rather than
visible pixel art. It will read as normal text, not as a retro effect.

**What we gain over a hand-drawn bitmap font** is correct glyph shapes, real advance widths and kerning,
and any size for free — which matters because the interface needs at least two sizes, a body size and a
larger one for the build buttons.

**What it costs** is a glyph cache, atlas packing, and a startup step that can fail. Perhaps two to three
hundred lines more than baking a font into a header, and a failure mode (no Segoe UI) that a header has
not got.

**The pixel doubling is accepted rather than tolerated** (`Design/OpenQuestions.md` Q18). Drawing the
interface into the back buffer *after* the scale would buy pixel-perfect text, and was declined: it costs
an explicit exception to R13 and a second place that knows how big the window is, which is the one thing
R13 exists to keep singular.

**What would reopen it:** text quality disappointing on the actual device, which is a measurement at M1
and is on the register. The lever is the authored resolution rather than the text path — authoring at
2880 × 1920 makes the scale 1:1 and the text native, at the cost of a 5.5-megapixel scene target and
multisampling four times as expensive. That would supersede
[`ADR-007`](ADR-007-the-authored-frame-is-1440x960.md),
not this.

## Measurements

None yet. Two are owed at **M1**:

1. **What the text actually looks like on a Surface Pro**, at both sizes, judged by looking at the screen
   rather than at a screenshot on a desktop monitor — which would be the wrong picture at the wrong scale.
2. **The atlas build cost at startup**, and whether it is worth caching to `LocalState` rather than
   rebuilding each launch.

The claim that a doubled pixel is a 133-PPI effective pixel is arithmetic: 267 ÷ 2.

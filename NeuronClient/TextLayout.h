#pragma once

#include "AtlasPacker.h"
#include "InterfacePass.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace Neuron
{

/// The printable ASCII range, which is every character the design uses -- `design_handoff_hud` states
/// every string and the longest is `TEAM 2 HOLDS THE FIELD`. **A character outside it draws nothing
/// rather than a substitute box**, because a box in a readout is worse than a gap and the design has no
/// string that can produce one.
inline constexpr wchar_t FIRST_CHARACTER = L' ';
inline constexpr wchar_t LAST_CHARACTER = L'~';
inline constexpr std::size_t CHARACTER_COUNT = static_cast<std::size_t>(LAST_CHARACTER - FIRST_CHARACTER) + 1;

/// One character's place in the atlas and what the layout needs to know about it.
///
/// R8: a public aggregate.
struct GlyphEntry
{
  /// Where the coverage is. Empty for a space, which has metrics and no ink.
  AtlasSlot slot{};

  /// How far the pen moves after drawing it, in physical pixels.
  float advancePixels = 0.0f;

  /// From the pen position to the slot's left edge, in physical pixels. **Signed**: a `j` reaches left
  /// of where its pen sat.
  float bearingXPixels = 0.0f;

  /// From the baseline DOWN to the slot's top edge, in physical pixels. Negative for anything that rises
  /// above the baseline, which is almost everything.
  float bearingYPixels = 0.0f;

  /// False when the character is outside the range or the font had no glyph for it.
  bool present = false;
};

/// **THE METRICS FOR ONE SIZE, WHICH IS WHAT A LAYOUT ACTUALLY NEEDS.** Separating this from the atlas
/// is what makes `LayoutText` testable: the layout is arithmetic over advances and bearings and has no
/// business knowing there is a texture, so a suite builds a table by hand with known advances and pins
/// the result exactly. `GlyphAtlas` needs DirectWrite and a device; this needs neither.
using GlyphTable = std::array<GlyphEntry, CHARACTER_COUNT>;

/// The vertical metrics of one size, in physical pixels. R8: a public aggregate.
struct FaceMetrics
{
  float emSizePixels = 0.0f;

  /// Baseline up to the top of the font's ink box, positive.
  float ascentPixels = 0.0f;

  /// Baseline down to the bottom of it, positive.
  float descentPixels = 0.0f;
};

/// Where a run sits in its box. The handoff uses all three: labels left, costs and counts right, and
/// the overlays' headlines centered.
enum class TextAlign : std::uint8_t
{
  Left,
  Right,
  Center
};

/// **ONE QUAD OF THE INTERFACE, AND THE LAYOUT IS THE VERTEX SHADER'S INPUT.** A glyph is a rectangle of
/// the atlas drawn at a rectangle of the back buffer; a solid plate is the same thing over the atlas's
/// block of full coverage (`GlyphAtlas::SolidSlot`). That is what lets a panel and its text go out as
/// one instanced draw in the order they were emitted, which is the only ordering the interface has.
///
/// Forty-eight bytes, three `float4`s. R8: a public aggregate.
struct GlyphQuad
{
  /// Back-buffer pixels, physical, half-open: left, top, right, bottom.
  float left = 0.0f;
  float top = 0.0f;
  float right = 0.0f;
  float bottom = 0.0f;

  /// The atlas rectangle, normalized.
  float u0 = 0.0f;
  float v0 = 0.0f;
  float u1 = 0.0f;
  float v1 = 0.0f;

  /// **STRAIGHT ALPHA, AND THE BYTES THE PALETTE STATES.** The back buffer is `B8G8R8A8_UNORM` with no
  /// sRGB view, so what is written is what is shown: `#38D1F5` goes in as 0x38 / 255 and comes out as
  /// `#38D1F5`, and blending happens on the encoded values exactly as it does in the handoff's browser
  /// reference. Converting the palette to linear first -- which `palette.json`'s note asks for -- would
  /// be right against an sRGB render target and darkens every color against this one.
  float red = 1.0f;
  float green = 1.0f;
  float blue = 1.0f;
  float alpha = 1.0f;
};

static_assert(sizeof(GlyphQuad) == 48, "The glyph quad is the vertex shader's instance input and is three float4s");

/// A color in the handoff's terms: an sRGB hex and an alpha. R8: a public aggregate.
struct QuadColor
{
  float red = 1.0f;
  float green = 1.0f;
  float blue = 1.0f;
  float alpha = 1.0f;
};

/// `#RRGGBB` as the palette writes it, straight into the bytes the back buffer shows (see `GlyphQuad`).
[[nodiscard]] constexpr QuadColor HexColor(std::uint32_t _rgb, float _alpha = 1.0f) noexcept
{
  return QuadColor{.red = static_cast<float>((_rgb >> 16) & 0xFF) / 255.0f,
                   .green = static_cast<float>((_rgb >> 8) & 0xFF) / 255.0f,
                   .blue = static_cast<float>(_rgb & 0xFF) / 255.0f,
                   .alpha = _alpha};
}

/// **WHERE THE BASELINE SITS IN A LINE BOX**, which is the one piece of CSS the handoff's coordinates
/// silently depend on. Every text rect in `design_handoff_hud` is a box whose height equals its font
/// size -- `CREDITS` is 20 tall at (16, 14) with `line-height: 20px` -- and a browser centers the font's
/// ascent-plus-descent in that box and puts the baseline at the ascent below the result. Segoe UI's
/// ascent and descent add to about 1.33 em, so the ink overhangs a box that tall by design.
///
/// Returned unrounded; `LayoutText` rounds once.
[[nodiscard]] float BaselinePixels(const FaceMetrics& _face, float _boxTopPixels, float _boxHeightPixels) noexcept;

/// **THE LAID-OUT WIDTH OF A RUN**, in physical pixels: every advance plus the tracking after every
/// character, **the last one included**. That is what a browser's `letter-spacing` does, and matching it
/// is what makes a right-aligned cost land where the handoff drew it rather than one tracking short.
///
/// _trackingEm is the handoff's `letter-spacing` in em -- 0.10 for `CREDITS`, 0.06 for a design name.
[[nodiscard]] float MeasureRunPixels(const GlyphTable& _table, const FaceMetrics& _face, std::wstring_view _text,
                                     float _trackingEm) noexcept;

/// What `LayoutText` is asked to do with one run. R8: a public aggregate.
struct TextRun
{
  /// The run's box, in back-buffer pixels. The left and right edges are where the run aligns **and
  /// where it clips** -- an over-long string is cut at the box rather than wrapped, because a wrap is the
  /// one failure this renderer cannot reproduce (`design_handoff_hud` *Type*). Top and bottom are the
  /// line box the baseline is centered in.
  PhysicalRect box{};

  std::wstring_view text;
  TextAlign align = TextAlign::Left;
  float trackingEm = 0.0f;
  QuadColor color{};
};

/// **A RUN INTO QUADS**, appended to _out; returns how many were appended.
///
/// **POSITIONS ARE SNAPPED, ONCE, TO WHOLE PHYSICAL PIXELS.** The pen advances in floats so a long run
/// does not accumulate rounding, and each glyph is placed at the rounded pen plus its bearing, which the
/// atlas already made whole. A glyph was rasterized at exactly the size it is drawn, and a quad at a
/// fractional position would resample it -- the one thing ADR-011 moved the interface out of the scene
/// target to avoid.
///
/// Horizontal clipping crops a quad and its texture coordinates together, so a glyph cut by the box is
/// cut rather than squeezed. Nothing is clipped vertically: the ink overhangs a line box by design.
///
/// _atlasWidthPixels and _atlasHeightPixels turn slots into texture coordinates.
std::size_t LayoutText(const GlyphTable& _table, const FaceMetrics& _face, std::uint32_t _atlasWidthPixels,
                       std::uint32_t _atlasHeightPixels, const TextRun& _run, std::vector<GlyphQuad>& _out);

/// **A SOLID RECTANGLE AS A GLYPH QUAD**, sampling the middle of the atlas's full-coverage block so that
/// point sampling can never reach its edge. Returns false and appends nothing for an empty rectangle.
bool AppendSolidQuad(const AtlasSlot& _solid, std::uint32_t _atlasWidthPixels, std::uint32_t _atlasHeightPixels, const PhysicalRect& _rect,
                     const QuadColor& _color, std::vector<GlyphQuad>& _out);

} // namespace Neuron

#pragma once

namespace Neuron
{
// One font at one size, rasterized once into a single-channel coverage texture (ADR-015). DirectWrite draws each glyph
// in grayscale at the size it will be shown, so text stays sharp at every scale (ADR-006), and the renderer draws it as
// textured quads. It knows no game concept: the caller names the font and the size.
struct GlyphAtlas
{
  // Where one character sits in the texture and how it is placed on a line, in pixels.
  struct Glyph
  {
    // Its rectangle in the texture. Empty for a character with nothing to draw, such as a space.
    std::uint32_t atlasX = 0;
    std::uint32_t atlasY = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    // From the pen on the baseline to the rectangle's top-left corner.
    std::int32_t offsetX = 0;
    std::int32_t offsetY = 0;
    // How far the pen moves on after it.
    float advance = 0.0f;
  };

  // The characters the atlas holds: printable ASCII. Any other character is drawn as FALLBACK.
  static constexpr char FIRST = ' ';
  static constexpr char LAST = '~';
  static constexpr char FALLBACK = '?';

  std::uint32_t width = 0;
  std::uint32_t height = 0;
  // One byte of coverage per texel, row after row.
  std::vector<std::uint8_t> coverage;
  // Indexed by character - FIRST.
  std::vector<Glyph> glyphs;
  // A block of full coverage, for drawing solid rectangles from the same texture: its center texel.
  std::uint32_t solidX = 0;
  std::uint32_t solidY = 0;
  // From a line's top to its baseline, and from one line's top to the next.
  float ascent = 0.0f;
  float lineHeight = 0.0f;

  [[nodiscard]] const Glyph& For(char _character) const noexcept
  {
    const char shown = (_character >= FIRST && _character <= LAST) ? _character : FALLBACK;
    return glyphs[static_cast<size_t>(shown - FIRST)];
  }

  // How wide _text is set on one line, in pixels.
  [[nodiscard]] float Width(std::string_view _text) const noexcept;
};

// The glyphs' bitmaps as DirectWrite draws them, before they are packed: one per character from FIRST to LAST.
struct GlyphBitmap
{
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  std::int32_t offsetX = 0;
  std::int32_t offsetY = 0;
  float advance = 0.0f;
  std::vector<std::uint8_t> coverage;
};

// Packs the bitmaps into one texture in rows, with a texel of space round each so that sampling never bleeds from a
// neighbor, and a solid block in the top-left corner. The texture is 256 texels wide or more, both sides powers of two.
[[nodiscard]] GlyphAtlas PackGlyphs(const std::vector<GlyphBitmap>& _bitmaps, float _ascent, float _lineHeight);

// Rasterizes FIRST to LAST of the installed font _family, at _emPixels, with DirectWrite. A family that is not
// installed falls back to the system's first. Throws winrt::hresult_error when DirectWrite fails.
[[nodiscard]] GlyphAtlas RasterizeGlyphs(std::wstring_view _family, float _emPixels);
} // namespace Neuron
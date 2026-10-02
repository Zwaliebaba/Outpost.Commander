#pragma once

namespace Neuron
{
// How wide a font is set: its stretch, as DirectWrite names the ones a family may have.
enum class FontStretch : std::uint8_t
{
  Normal,
  SemiCondensed,
  Condensed
};

// One font of the interface (ADR-015, ADR-030): the families to look for, in order, of which the first installed is used;
// its weight, from 100 to 900 as DirectWrite counts it; its stretch; and its size in the caller's reference units, which
// the pipeline scales to pixels.
struct FontDesc
{
  std::vector<std::wstring> families;
  std::uint16_t weight = 400;
  FontStretch stretch = FontStretch::Normal;
  float emUnits = 0.0f;
};

// A small shape the interface draws from the atlas like a glyph, in any color (ADR-030).
enum class SpriteShape : std::uint8_t
{
  // A filled square standing on a corner.
  Diamond,
  // The outline of a square.
  Checkbox,
  // An L along the top and the left edge; drawn flipped, it makes the other three corners.
  CornerBracket
};

// A sprite, and how big it is drawn, in the caller's reference units.
struct SpriteDesc
{
  SpriteShape shape = SpriteShape::Diamond;
  float sizeUnits = 0.0f;
};

// The interface's fonts and sprites, rasterized once into one single-channel coverage texture (ADR-015, ADR-030).
// DirectWrite draws each glyph in grayscale at the size it will be shown, so text stays sharp at every scale (ADR-006),
// and the renderer draws it as textured quads. It knows no game concept: the caller names the fonts and the sprites.
struct GlyphAtlas
{
  // Where one character or sprite sits in the texture and how it is placed on a line, in pixels.
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

  // The characters every font holds: printable ASCII, then the few others the interface uses, the middle dot and the
  // multiplication sign. Any other character is drawn as FALLBACK.
  static constexpr char32_t FIRST = U' ';
  static constexpr char32_t LAST = U'~';
  static constexpr std::array<char32_t, 2> EXTRA_CHARACTERS{U'·', U'×'};
  static constexpr char32_t FALLBACK = U'?';
  static constexpr std::size_t CHARACTER_COUNT = (LAST - FIRST) + 1 + EXTRA_CHARACTERS.size();

  // One font: which family it found, and its glyphs in the order above.
  struct Font
  {
    std::wstring family;
    std::vector<Glyph> glyphs;
    // From a line's top to its baseline, and from one line's top to the next.
    float ascent = 0.0f;
    float lineHeight = 0.0f;

    [[nodiscard]] const Glyph& For(char32_t _character) const noexcept;
    // How wide _text, in UTF-8, is set on one line, in pixels, with _trackingPixels more between each two characters.
    [[nodiscard]] float Width(std::string_view _text, float _trackingPixels = 0.0f) const noexcept;
  };

  std::uint32_t width = 0;
  std::uint32_t height = 0;
  // One byte of coverage per texel, row after row.
  std::vector<std::uint8_t> coverage;
  // In the order the caller gave them.
  std::vector<Font> fonts;
  std::vector<Glyph> sprites;
  // A block of full coverage, for drawing solid rectangles from the same texture: its center texel.
  std::uint32_t solidX = 0;
  std::uint32_t solidY = 0;
};

// The index of a character in a font's glyphs, or of FALLBACK for one the atlas does not hold.
[[nodiscard]] std::size_t GlyphIndexOf(char32_t _character) noexcept;

// The code point that starts at _index in _text, read as UTF-8, and _index moved past it. A byte that does not start a
// character of one or two bytes, as the interface's characters all are, gives U+FFFD and is skipped with any
// continuation bytes after it.
[[nodiscard]] char32_t NextCodePoint(std::string_view _text, std::size_t& _index) noexcept;

// A glyph's or a sprite's bitmap as it is drawn, before it is packed.
struct GlyphBitmap
{
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  std::int32_t offsetX = 0;
  std::int32_t offsetY = 0;
  float advance = 0.0f;
  std::vector<std::uint8_t> coverage;
};

// One font's bitmaps as DirectWrite draws them: CHARACTER_COUNT of them, in the atlas's order.
struct FontBitmaps
{
  std::wstring family;
  std::vector<GlyphBitmap> glyphs;
  float ascent = 0.0f;
  float lineHeight = 0.0f;
};

// Packs every font's glyphs and the sprites into one texture in rows, with a texel of space round each so that sampling
// never bleeds from a neighbor, and a solid block in the top-left corner. The texture is 256 texels wide or more, both
// sides powers of two.
[[nodiscard]] GlyphAtlas PackGlyphs(const std::vector<FontBitmaps>& _fonts, const std::vector<GlyphBitmap>& _sprites);

// Rasterizes the atlas's characters in the first of _font's families that is installed, at _emPixels, with DirectWrite.
// Throws Neuron::Exception naming the families when none is installed, and winrt::hresult_error when DirectWrite fails.
[[nodiscard]] FontBitmaps RasterizeFont(const FontDesc& _font, float _emPixels);

// Draws _shape in a square of _sizePixels, its edges smoothed, its lines a twelfth of its size and at least a pixel.
[[nodiscard]] GlyphBitmap DrawSprite(SpriteShape _shape, std::uint32_t _sizePixels);
} // namespace Neuron

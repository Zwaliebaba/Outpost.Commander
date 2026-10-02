#include "pch.h"
#include "GlyphAtlas.h"

#include <dwrite_2.h>

#include <algorithm>
#include <bit>
#include <cmath>

#pragma comment(lib, "dwrite.lib")

namespace
{
// Space round every glyph and the solid block, so that a sample at a quad's edge never reads a neighbor.
constexpr std::uint32_t PADDING_TEXELS = 1;
constexpr std::uint32_t SOLID_TEXELS = 4;
constexpr std::uint32_t MINIMUM_WIDTH_TEXELS = 256;
constexpr std::uint8_t FULL_COVERAGE = 255;
constexpr char32_t REPLACEMENT = U'�';
// A sprite's lines are this share of its size, and at least a pixel.
constexpr float SPRITE_LINE_SHARE = 1.0f / 12.0f;

DWRITE_FONT_STRETCH StretchOf(Neuron::FontStretch _stretch) noexcept
{
  switch (_stretch)
  {
  case Neuron::FontStretch::SemiCondensed:
    return DWRITE_FONT_STRETCH_SEMI_CONDENSED;
  case Neuron::FontStretch::Condensed:
    return DWRITE_FONT_STRETCH_CONDENSED;
  case Neuron::FontStretch::Normal:
  default:
    return DWRITE_FONT_STRETCH_NORMAL;
  }
}

// The family names are the caller's, in ASCII; for a message, each is narrowed character by character.
std::string Narrow(std::wstring_view _text)
{
  std::string narrow;
  narrow.reserve(_text.size());
  for (const wchar_t character : _text)
    narrow.push_back(character < 0x80 ? static_cast<char>(character) : '?');
  return narrow;
}

// The coverage of a pixel whose center is _inside pixels inside an edge (negative outside): smoothed over one pixel.
std::uint8_t EdgeCoverage(float _inside) noexcept
{
  return static_cast<std::uint8_t>(std::lround(std::clamp(_inside + 0.5f, 0.0f, 1.0f) * FULL_COVERAGE));
}
} // namespace

std::size_t Neuron::GlyphIndexOf(char32_t _character) noexcept
{
  if (_character >= GlyphAtlas::FIRST && _character <= GlyphAtlas::LAST)
    return _character - GlyphAtlas::FIRST;
  const auto extra = std::ranges::find(GlyphAtlas::EXTRA_CHARACTERS, _character);
  if (extra != GlyphAtlas::EXTRA_CHARACTERS.end())
    return (GlyphAtlas::LAST - GlyphAtlas::FIRST) + 1 + static_cast<std::size_t>(extra - GlyphAtlas::EXTRA_CHARACTERS.begin());
  return GlyphAtlas::FALLBACK - GlyphAtlas::FIRST;
}

char32_t Neuron::NextCodePoint(std::string_view _text, std::size_t& _index) noexcept
{
  const auto byte = [&_text](std::size_t _at) { return static_cast<std::uint8_t>(_text[_at]); };
  const std::uint8_t lead = byte(_index);
  ++_index;
  if (lead < 0x80)
    return lead;
  if ((lead & 0xE0U) == 0xC0U && _index < _text.size() && (byte(_index) & 0xC0U) == 0x80U)
  {
    const char32_t codePoint = (char32_t{lead & 0x1FU} << 6U) | (byte(_index) & 0x3FU);
    ++_index;
    // An overlong form of an ASCII character is not that character.
    return codePoint >= 0x80 ? codePoint : REPLACEMENT;
  }
  while (_index < _text.size() && (byte(_index) & 0xC0U) == 0x80U)
    ++_index;
  return REPLACEMENT;
}

const Neuron::GlyphAtlas::Glyph& Neuron::GlyphAtlas::Font::For(char32_t _character) const noexcept
{
  return glyphs[GlyphIndexOf(_character)];
}

float Neuron::GlyphAtlas::Font::Width(std::string_view _text, float _trackingPixels) const noexcept
{
  float total = 0.0f;
  std::size_t characters = 0;
  for (std::size_t index = 0; index < _text.size();)
  {
    total += For(NextCodePoint(_text, index)).advance;
    ++characters;
  }
  return characters > 1 ? total + (_trackingPixels * static_cast<float>(characters - 1)) : total;
}

Neuron::GlyphAtlas Neuron::PackGlyphs(const std::vector<FontBitmaps>& _fonts, const std::vector<GlyphBitmap>& _sprites)
{
  // Every bitmap in one list, the fonts' first in their order and then the sprites, so that one pass places them all.
  std::vector<const GlyphBitmap*> bitmaps;
  for (const FontBitmaps& font : _fonts)
  {
    for (const GlyphBitmap& glyph : font.glyphs)
      bitmaps.push_back(&glyph);
  }
  for (const GlyphBitmap& sprite : _sprites)
    bitmaps.push_back(&sprite);

  // Wide enough for the widest bitmap and for them all to fit in rows.
  std::uint32_t widest = SOLID_TEXELS;
  std::uint64_t area = 0;
  for (const GlyphBitmap* bitmap : bitmaps)
  {
    widest = std::max(widest, bitmap->width);
    area += std::uint64_t{bitmap->width + (2 * PADDING_TEXELS)} * (bitmap->height + (2 * PADDING_TEXELS));
  }
  std::uint32_t width = MINIMUM_WIDTH_TEXELS;
  while (width < widest + (2 * PADDING_TEXELS) || std::uint64_t{width} * width < area * 2)
  {
    width *= 2;
  }

  // Rows of bitmaps, left to right; the solid block opens the first row.
  std::vector<GlyphAtlas::Glyph> placed(bitmaps.size());
  std::uint32_t x = SOLID_TEXELS + (2 * PADDING_TEXELS);
  std::uint32_t y = 0;
  std::uint32_t rowHeight = SOLID_TEXELS + (2 * PADDING_TEXELS);
  for (size_t i = 0; i < bitmaps.size(); ++i)
  {
    const GlyphBitmap& bitmap = *bitmaps[i];
    GlyphAtlas::Glyph& glyph = placed[i];
    glyph.offsetX = bitmap.offsetX;
    glyph.offsetY = bitmap.offsetY;
    glyph.advance = bitmap.advance;
    if (bitmap.width == 0 || bitmap.height == 0)
      continue;
    const std::uint32_t needed = bitmap.width + (2 * PADDING_TEXELS);
    if (x + needed > width)
    {
      x = 0;
      y += rowHeight;
      rowHeight = 0;
    }
    glyph.atlasX = x + PADDING_TEXELS;
    glyph.atlasY = y + PADDING_TEXELS;
    glyph.width = bitmap.width;
    glyph.height = bitmap.height;
    x += needed;
    rowHeight = std::max(rowHeight, bitmap.height + (2 * PADDING_TEXELS));
  }

  GlyphAtlas atlas;
  atlas.width = width;
  atlas.height = std::max(std::bit_ceil(y + rowHeight), 1u);
  atlas.coverage.assign(std::size_t{atlas.width} * atlas.height, 0);
  for (std::uint32_t row = 0; row < SOLID_TEXELS; ++row)
  {
    std::fill_n(atlas.coverage.begin() + static_cast<std::ptrdiff_t>((std::size_t{PADDING_TEXELS + row} * atlas.width) + PADDING_TEXELS),
                SOLID_TEXELS, FULL_COVERAGE);
  }
  atlas.solidX = PADDING_TEXELS + (SOLID_TEXELS / 2);
  atlas.solidY = PADDING_TEXELS + (SOLID_TEXELS / 2);

  for (size_t i = 0; i < bitmaps.size(); ++i)
  {
    const GlyphBitmap& bitmap = *bitmaps[i];
    const GlyphAtlas::Glyph& glyph = placed[i];
    for (std::uint32_t row = 0; row < glyph.height; ++row)
    {
      std::copy_n(bitmap.coverage.begin() + static_cast<std::ptrdiff_t>(std::size_t{row} * bitmap.width), bitmap.width,
                  atlas.coverage.begin() + static_cast<std::ptrdiff_t>((std::size_t{glyph.atlasY + row} * atlas.width) + glyph.atlasX));
    }
  }

  auto next = placed.begin();
  for (const FontBitmaps& font : _fonts)
  {
    const auto count = static_cast<std::ptrdiff_t>(font.glyphs.size());
    atlas.fonts.push_back({.family = font.family,
                           .glyphs = std::vector<GlyphAtlas::Glyph>(next, next + count),
                           .ascent = font.ascent,
                           .lineHeight = font.lineHeight});
    next += count;
  }
  atlas.sprites.assign(next, placed.end());
  return atlas;
}

Neuron::FontBitmaps Neuron::RasterizeFont(const FontDesc& _font, float _emPixels)
{
  winrt::com_ptr<IDWriteFactory2> factory;
  winrt::check_hresult(
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_ISOLATED, __uuidof(IDWriteFactory2), reinterpret_cast<IUnknown**>(factory.put())));

  winrt::com_ptr<IDWriteFontCollection> fonts;
  winrt::check_hresult(factory->GetSystemFontCollection(fonts.put(), FALSE));
  FontBitmaps result;
  UINT32 familyIndex = 0;
  for (const std::wstring& family : _font.families)
  {
    BOOL exists = FALSE;
    winrt::check_hresult(fonts->FindFamilyName(family.c_str(), &familyIndex, &exists));
    if (exists != FALSE)
    {
      result.family = family;
      break;
    }
  }
  if (result.family.empty())
  {
    std::string names;
    for (const std::wstring& family : _font.families)
      names += (names.empty() ? "" : ", ") + Narrow(family);
    throw Exception(std::format("None of the interface's fonts {} is installed.", names.empty() ? "(none named)" : names));
  }

  winrt::com_ptr<IDWriteFontFamily> fontFamily;
  winrt::check_hresult(fonts->GetFontFamily(familyIndex, fontFamily.put()));
  winrt::com_ptr<IDWriteFont> font;
  winrt::check_hresult(fontFamily->GetFirstMatchingFont(static_cast<DWRITE_FONT_WEIGHT>(_font.weight), StretchOf(_font.stretch),
                                                        DWRITE_FONT_STYLE_NORMAL, font.put()));
  winrt::com_ptr<IDWriteFontFace> face;
  winrt::check_hresult(font->CreateFontFace(face.put()));

  DWRITE_FONT_METRICS metrics{};
  face->GetMetrics(&metrics);
  const float pixelsPerDesignUnit = _emPixels / static_cast<float>(metrics.designUnitsPerEm);
  result.ascent = std::ceil(static_cast<float>(metrics.ascent) * pixelsPerDesignUnit);
  result.lineHeight = std::ceil(static_cast<float>(metrics.ascent + metrics.descent + metrics.lineGap) * pixelsPerDesignUnit);

  std::vector<char32_t> characters;
  for (char32_t character = GlyphAtlas::FIRST; character <= GlyphAtlas::LAST; ++character)
    characters.push_back(character);
  characters.insert(characters.end(), GlyphAtlas::EXTRA_CHARACTERS.begin(), GlyphAtlas::EXTRA_CHARACTERS.end());

  for (const char32_t character : characters)
  {
    const auto codePoint = static_cast<UINT32>(character);
    UINT16 glyphIndex = 0;
    winrt::check_hresult(face->GetGlyphIndices(&codePoint, 1, &glyphIndex));
    DWRITE_GLYPH_METRICS glyphMetrics{};
    winrt::check_hresult(face->GetDesignGlyphMetrics(&glyphIndex, 1, &glyphMetrics, FALSE));

    GlyphBitmap bitmap;
    // Whole pixels, so that every glyph starts on a texel and the text stays sharp.
    bitmap.advance = std::round(static_cast<float>(glyphMetrics.advanceWidth) * pixelsPerDesignUnit);

    const FLOAT noAdvance = 0.0f;
    const DWRITE_GLYPH_OFFSET noOffset{};
    DWRITE_GLYPH_RUN run{};
    run.fontFace = face.get();
    run.fontEmSize = _emPixels;
    run.glyphCount = 1;
    run.glyphIndices = &glyphIndex;
    run.glyphAdvances = &noAdvance;
    run.glyphOffsets = &noOffset;
    winrt::com_ptr<IDWriteGlyphRunAnalysis> analysis;
    winrt::check_hresult(factory->CreateGlyphRunAnalysis(&run, nullptr, DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC,
                                                         DWRITE_MEASURING_MODE_NATURAL, DWRITE_GRID_FIT_MODE_DEFAULT,
                                                         DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE, 0.0f, 0.0f, analysis.put()));
    // In grayscale, the aliased texture type is one byte of coverage per pixel.
    RECT bounds{};
    winrt::check_hresult(analysis->GetAlphaTextureBounds(DWRITE_TEXTURE_ALIASED_1x1, &bounds));
    if (bounds.right > bounds.left && bounds.bottom > bounds.top)
    {
      bitmap.width = static_cast<std::uint32_t>(bounds.right - bounds.left);
      bitmap.height = static_cast<std::uint32_t>(bounds.bottom - bounds.top);
      bitmap.offsetX = bounds.left;
      bitmap.offsetY = bounds.top;
      bitmap.coverage.resize(std::size_t{bitmap.width} * bitmap.height);
      winrt::check_hresult(analysis->CreateAlphaTexture(DWRITE_TEXTURE_ALIASED_1x1, &bounds, bitmap.coverage.data(),
                                                        static_cast<UINT32>(bitmap.coverage.size())));
    }
    result.glyphs.push_back(std::move(bitmap));
  }
  return result;
}

Neuron::GlyphBitmap Neuron::DrawSprite(SpriteShape _shape, std::uint32_t _sizePixels)
{
  const std::uint32_t size = std::max(_sizePixels, 1u);
  GlyphBitmap sprite{.width = size, .height = size, .offsetX = 0, .offsetY = 0, .advance = static_cast<float>(size), .coverage = {}};
  sprite.coverage.resize(std::size_t{size} * size);
  const auto extent = static_cast<float>(size);
  const float line = std::max(1.0f, std::round(extent * SPRITE_LINE_SHARE));
  const float center = extent / 2.0f;
  for (std::uint32_t row = 0; row < size; ++row)
  {
    for (std::uint32_t column = 0; column < size; ++column)
    {
      // The pixel's center, from the square's top-left corner.
      const float x = static_cast<float>(column) + 0.5f;
      const float y = static_cast<float>(row) + 0.5f;
      float inside = 0.0f;
      switch (_shape)
      {
      case SpriteShape::Diamond:
        inside = center - (std::abs(x - center) + std::abs(y - center));
        break;
      case SpriteShape::Checkbox:
      {
        // Within the square and within a line of its edge.
        const float toEdge = std::min({x, y, extent - x, extent - y});
        inside = std::min(toEdge, line - toEdge);
        break;
      }
      case SpriteShape::CornerBracket:
      default:
        inside = std::min(std::max(line - x, line - y), std::min(x, y));
        break;
      }
      sprite.coverage[(std::size_t{row} * size) + column] = EdgeCoverage(inside);
    }
  }
  return sprite;
}

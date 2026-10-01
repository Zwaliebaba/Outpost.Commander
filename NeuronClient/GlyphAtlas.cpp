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
} // namespace

float Neuron::GlyphAtlas::Width(std::string_view _text) const noexcept
{
  float total = 0.0f;
  for (const char character : _text)
    total += For(character).advance;
  return total;
}

Neuron::GlyphAtlas Neuron::PackGlyphs(const std::vector<GlyphBitmap>& _bitmaps, float _ascent, float _lineHeight)
{
  // Wide enough for the widest glyph and for the glyphs to fit in a few rows.
  std::uint32_t widest = SOLID_TEXELS;
  std::uint64_t area = 0;
  for (const GlyphBitmap& bitmap : _bitmaps)
  {
    widest = std::max(widest, bitmap.width);
    area += std::uint64_t{bitmap.width + (2 * PADDING_TEXELS)} * (bitmap.height + (2 * PADDING_TEXELS));
  }
  std::uint32_t width = MINIMUM_WIDTH_TEXELS;
  while (width < widest + (2 * PADDING_TEXELS) || std::uint64_t{width} * width < area * 2)
    width *= 2;

  GlyphAtlas atlas;
  atlas.ascent = _ascent;
  atlas.lineHeight = _lineHeight;
  atlas.glyphs.resize(_bitmaps.size());

  // Rows of glyphs, left to right; the solid block opens the first row.
  std::uint32_t x = SOLID_TEXELS + (2 * PADDING_TEXELS);
  std::uint32_t y = 0;
  std::uint32_t rowHeight = SOLID_TEXELS + (2 * PADDING_TEXELS);
  for (size_t i = 0; i < _bitmaps.size(); ++i)
  {
    const GlyphBitmap& bitmap = _bitmaps[i];
    GlyphAtlas::Glyph& glyph = atlas.glyphs[i];
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

  for (size_t i = 0; i < _bitmaps.size(); ++i)
  {
    const GlyphBitmap& bitmap = _bitmaps[i];
    const GlyphAtlas::Glyph& glyph = atlas.glyphs[i];
    for (std::uint32_t row = 0; row < glyph.height; ++row)
    {
      std::copy_n(bitmap.coverage.begin() + static_cast<std::ptrdiff_t>(std::size_t{row} * bitmap.width), bitmap.width,
                  atlas.coverage.begin() + static_cast<std::ptrdiff_t>((std::size_t{glyph.atlasY + row} * atlas.width) + glyph.atlasX));
    }
  }
  return atlas;
}

Neuron::GlyphAtlas Neuron::RasterizeGlyphs(std::wstring_view _family, float _emPixels)
{
  winrt::com_ptr<IDWriteFactory2> factory;
  winrt::check_hresult(
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_ISOLATED, __uuidof(IDWriteFactory2), reinterpret_cast<IUnknown**>(factory.put())));

  winrt::com_ptr<IDWriteFontCollection> fonts;
  winrt::check_hresult(factory->GetSystemFontCollection(fonts.put(), FALSE));
  const std::wstring family(_family);
  UINT32 familyIndex = 0;
  BOOL exists = FALSE;
  winrt::check_hresult(fonts->FindFamilyName(family.c_str(), &familyIndex, &exists));
  if (exists == FALSE)
    familyIndex = 0;
  winrt::com_ptr<IDWriteFontFamily> fontFamily;
  winrt::check_hresult(fonts->GetFontFamily(familyIndex, fontFamily.put()));
  winrt::com_ptr<IDWriteFont> font;
  winrt::check_hresult(
    fontFamily->GetFirstMatchingFont(DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, font.put()));
  winrt::com_ptr<IDWriteFontFace> face;
  winrt::check_hresult(font->CreateFontFace(face.put()));

  DWRITE_FONT_METRICS metrics{};
  face->GetMetrics(&metrics);
  const float pixelsPerDesignUnit = _emPixels / static_cast<float>(metrics.designUnitsPerEm);
  const float ascent = std::ceil(static_cast<float>(metrics.ascent) * pixelsPerDesignUnit);
  const float lineHeight = std::ceil(static_cast<float>(metrics.ascent + metrics.descent + metrics.lineGap) * pixelsPerDesignUnit);

  std::vector<GlyphBitmap> bitmaps;
  for (char character = GlyphAtlas::FIRST; character <= GlyphAtlas::LAST; ++character)
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
    bitmaps.push_back(std::move(bitmap));
  }
  return PackGlyphs(bitmaps, ascent, lineHeight);
}

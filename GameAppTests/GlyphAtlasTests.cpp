#include "pch.h"

#include <bit>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameAppTests
{
namespace
{
// A glyph bitmap of _width by _height texels, all at _coverage.
Neuron::GlyphBitmap Bitmap(std::uint32_t _width, std::uint32_t _height, std::uint8_t _coverage)
{
  return {.width = _width,
          .height = _height,
          .offsetX = 1,
          .offsetY = -static_cast<std::int32_t>(_height),
          .advance = static_cast<float>(_width + 2),
          .coverage = std::vector<std::uint8_t>(std::size_t{_width} * _height, _coverage)};
}
} // namespace

TEST_CLASS(GlyphAtlasTests)
{
public:
  // ADR-015: every glyph lands whole in the texture, apart from every other and from the solid block, with its coverage.
  TEST_METHOD(PacksGlyphsWithoutOverlap)
  {
    std::vector<Neuron::GlyphBitmap> bitmaps;
    for (char character = Neuron::GlyphAtlas::FIRST; character <= Neuron::GlyphAtlas::LAST; ++character)
    {
      bitmaps.push_back(character == ' ' ? Neuron::GlyphBitmap{.advance = 5.0f}
                                         : Bitmap(9 + (character % 7), 14 + (character % 5), static_cast<std::uint8_t>(character)));
    }
    const Neuron::GlyphAtlas atlas = Neuron::PackGlyphs(bitmaps, 15.0f, 20.0f);

    Assert::IsTrue(std::has_single_bit(atlas.width) && std::has_single_bit(atlas.height));
    Assert::AreEqual(std::size_t{atlas.width} * atlas.height, atlas.coverage.size());
    Assert::AreEqual(std::uint8_t{255}, atlas.coverage[(std::size_t{atlas.solidY} * atlas.width) + atlas.solidX]);

    // Each texel belongs to at most one glyph, and holds that glyph's coverage.
    std::vector<int> owner(atlas.coverage.size(), -1);
    for (size_t i = 0; i < atlas.glyphs.size(); ++i)
    {
      const Neuron::GlyphAtlas::Glyph& glyph = atlas.glyphs[i];
      Assert::AreEqual(bitmaps[i].width, glyph.width);
      Assert::IsTrue(glyph.atlasX + glyph.width <= atlas.width && glyph.atlasY + glyph.height <= atlas.height);
      for (std::uint32_t y = glyph.atlasY; y < glyph.atlasY + glyph.height; ++y)
      {
        for (std::uint32_t x = glyph.atlasX; x < glyph.atlasX + glyph.width; ++x)
        {
          const std::size_t texel = (std::size_t{y} * atlas.width) + x;
          Assert::AreEqual(-1, owner[texel]);
          owner[texel] = static_cast<int>(i);
          Assert::AreEqual(bitmaps[i].coverage.front(), atlas.coverage[texel]);
        }
      }
    }
    Assert::AreEqual(-1, owner[(std::size_t{atlas.solidY} * atlas.width) + atlas.solidX]);
    // A space has nothing to draw but still moves the pen; a character the atlas does not hold is drawn as '?'.
    Assert::AreEqual(5.0f, atlas.For(' ').advance);
    Assert::AreEqual(atlas.For('?').width, atlas.For('\n').width);
    Assert::AreEqual(5.0f + atlas.For('A').advance, atlas.Width(" A"));
  }

  // ADR-015: DirectWrite draws an installed font. Every printable character moves the pen, a letter has coverage, and a
  // wide letter is wider than a narrow one.
  TEST_METHOD(RasterizesASystemFont)
  {
    const Neuron::GlyphAtlas atlas = Neuron::RasterizeGlyphs(L"Segoe UI", 20.0f);
    for (char character = Neuron::GlyphAtlas::FIRST; character <= Neuron::GlyphAtlas::LAST; ++character)
      Assert::IsTrue(atlas.For(character).advance > 0.0f);
    const Neuron::GlyphAtlas::Glyph& letter = atlas.For('A');
    Assert::IsTrue(letter.width > 0 && letter.height > 0);
    Assert::IsTrue(atlas.For('W').advance > atlas.For('i').advance);
    Assert::IsTrue(atlas.lineHeight >= atlas.ascent && atlas.ascent > 0.0f);
  }
};
} // namespace GameAppTests
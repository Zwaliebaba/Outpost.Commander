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

// A font of made-up bitmaps, every glyph's coverage telling it apart: _seed plus its index.
Neuron::FontBitmaps MadeUpFont(std::uint32_t _seed)
{
  Neuron::FontBitmaps font{.family = L"Made Up", .glyphs = {}, .ascent = 15.0f, .lineHeight = 20.0f};
  for (std::size_t i = 0; i < Neuron::GlyphAtlas::CHARACTER_COUNT; ++i)
  {
    font.glyphs.push_back(i == 0 ? Neuron::GlyphBitmap{.advance = 5.0f}
                                 : Bitmap(9 + static_cast<std::uint32_t>(i % 7), 14 + static_cast<std::uint32_t>(i % 5),
                                          static_cast<std::uint8_t>((_seed + i) % 250)));
  }
  return font;
}

// The coverage of the texel at _x, _y of a sprite.
std::uint8_t At(const Neuron::GlyphBitmap& _sprite, std::uint32_t _x, std::uint32_t _y)
{
  return _sprite.coverage[(std::size_t{_y} * _sprite.width) + _x];
}

Neuron::FontDesc Segoe()
{
  return {.families = {L"Segoe UI"}, .weight = 600, .stretch = Neuron::FontStretch::Normal, .emUnits = 20.0f};
}
} // namespace

TEST_CLASS(GlyphAtlasTests)
{
public:
  // ADR-015, ADR-030: every glyph of every font and every sprite lands whole in the one texture, apart from every other and
  // from the solid block, with its coverage.
  TEST_METHOD(PacksFontsAndSpritesWithoutOverlap)
  {
    const std::vector<Neuron::FontBitmaps> fonts{MadeUpFont(0), MadeUpFont(100)};
    const std::vector<Neuron::GlyphBitmap> sprites{Bitmap(12, 12, 251), Bitmap(10, 10, 252)};
    const Neuron::GlyphAtlas atlas = Neuron::PackGlyphs(fonts, sprites);

    Assert::IsTrue(std::has_single_bit(atlas.width) && std::has_single_bit(atlas.height));
    Assert::AreEqual(std::size_t{atlas.width} * atlas.height, atlas.coverage.size());
    Assert::AreEqual(std::uint8_t{255}, atlas.coverage[(std::size_t{atlas.solidY} * atlas.width) + atlas.solidX]);
    Assert::AreEqual(size_t{2}, atlas.fonts.size());
    Assert::AreEqual(size_t{2}, atlas.sprites.size());

    // Each texel belongs to at most one glyph or sprite, and holds its coverage.
    std::vector<int> owner(atlas.coverage.size(), -1);
    int next = 0;
    const auto claim = [&](const Neuron::GlyphAtlas::Glyph& _glyph, const Neuron::GlyphBitmap& _bitmap)
    {
      Assert::AreEqual(_bitmap.width, _glyph.width);
      Assert::IsTrue(_glyph.atlasX + _glyph.width <= atlas.width && _glyph.atlasY + _glyph.height <= atlas.height);
      for (std::uint32_t y = _glyph.atlasY; y < _glyph.atlasY + _glyph.height; ++y)
      {
        for (std::uint32_t x = _glyph.atlasX; x < _glyph.atlasX + _glyph.width; ++x)
        {
          const std::size_t texel = (std::size_t{y} * atlas.width) + x;
          Assert::AreEqual(-1, owner[texel]);
          owner[texel] = next;
          Assert::AreEqual(_bitmap.coverage.front(), atlas.coverage[texel]);
        }
      }
      ++next;
    };
    for (size_t font = 0; font < fonts.size(); ++font)
    {
      Assert::AreEqual(Neuron::GlyphAtlas::CHARACTER_COUNT, atlas.fonts[font].glyphs.size());
      for (size_t i = 0; i < fonts[font].glyphs.size(); ++i)
        claim(atlas.fonts[font].glyphs[i], fonts[font].glyphs[i]);
    }
    for (size_t i = 0; i < sprites.size(); ++i)
      claim(atlas.sprites[i], sprites[i]);
    Assert::AreEqual(-1, owner[(std::size_t{atlas.solidY} * atlas.width) + atlas.solidX]);
  }

  // ADR-030: text is UTF-8. ASCII and the two others the interface uses have glyphs of their own; anything else, and a byte
  // that starts no character, is drawn as '?'. Tracking adds space between characters, not after the last.
  TEST_METHOD(ReadsUtf8AndTracksLetters)
  {
    const Neuron::GlyphAtlas atlas = Neuron::PackGlyphs({MadeUpFont(0)}, {});
    const Neuron::GlyphAtlas::Font& font = atlas.fonts.front();
    const Neuron::GlyphAtlas::Glyph& times = font.For(char32_t{0xD7});
    const Neuron::GlyphAtlas::Glyph& dot = font.For(char32_t{0xB7});
    const Neuron::GlyphAtlas::Glyph& fallback = font.For(U'?');
    Assert::IsTrue(&times != &fallback && &dot != &fallback && &times != &dot);
    Assert::IsTrue(&font.For(char32_t{0x20AC}) == &fallback, L"the euro sign is not held");

    // "a×b" is a, the multiplication sign in two bytes, and b.
    const std::string_view axb = "a\xC3\x97"
                                 "b";
    Assert::AreEqual(font.For(U'a').advance + times.advance + font.For(U'b').advance, font.Width(axb));
    std::size_t index = 1;
    Assert::IsTrue(Neuron::NextCodePoint(axb, index) == char32_t{0xD7});
    Assert::AreEqual(size_t{3}, index);

    // A lone continuation byte, and a three-byte character, are one fallback each.
    const std::string_view broken = "\x80"
                                    "\xE2\x82\xAC";
    Assert::AreEqual(2.0f * fallback.advance, font.Width(broken));

    Assert::AreEqual(font.Width("AB") + 3.0f, font.Width("AB", 3.0f));
    Assert::AreEqual(font.Width("A"), font.Width("A", 3.0f));
    Assert::AreEqual(0.0f, font.Width("", 3.0f));
  }

  // ADR-030: the sprites are drawn, not shipped. A diamond is full at its center and empty in its corners; a checkbox is
  // full along its edges and empty inside; a corner bracket is full along its top and left edges only.
  TEST_METHOD(DrawsTheSprites)
  {
    const Neuron::GlyphBitmap diamond = Neuron::DrawSprite(Neuron::SpriteShape::Diamond, 24);
    Assert::AreEqual(24u, diamond.width);
    Assert::AreEqual(std::uint8_t{255}, At(diamond, 12, 12));
    Assert::AreEqual(std::uint8_t{0}, At(diamond, 0, 0));
    Assert::AreEqual(std::uint8_t{0}, At(diamond, 23, 23));
    Assert::AreEqual(At(diamond, 5, 12), At(diamond, 18, 12), L"symmetric");

    const Neuron::GlyphBitmap box = Neuron::DrawSprite(Neuron::SpriteShape::Checkbox, 24);
    Assert::AreEqual(std::uint8_t{255}, At(box, 0, 12));
    Assert::AreEqual(std::uint8_t{255}, At(box, 23, 12));
    Assert::AreEqual(std::uint8_t{0}, At(box, 12, 12));

    const Neuron::GlyphBitmap corner = Neuron::DrawSprite(Neuron::SpriteShape::CornerBracket, 24);
    Assert::AreEqual(std::uint8_t{255}, At(corner, 0, 20));
    Assert::AreEqual(std::uint8_t{255}, At(corner, 20, 0));
    Assert::AreEqual(std::uint8_t{0}, At(corner, 20, 20));
    Assert::AreEqual(std::uint8_t{0}, At(corner, 23, 12));

    Assert::AreEqual(1u, Neuron::DrawSprite(Neuron::SpriteShape::Diamond, 0).width, L"never empty");
  }

  // ADR-015, ADR-030: DirectWrite draws an installed font. Every character moves the pen, a letter has coverage, and a
  // wide letter is wider than a narrow one; the multiplication sign is a glyph of the font, not its fallback.
  TEST_METHOD(RasterizesASystemFont)
  {
    const Neuron::FontBitmaps bitmaps = Neuron::RasterizeFont(Segoe(), 20.0f);
    Assert::IsTrue(bitmaps.family == L"Segoe UI");
    Assert::AreEqual(Neuron::GlyphAtlas::CHARACTER_COUNT, bitmaps.glyphs.size());
    const Neuron::GlyphAtlas atlas = Neuron::PackGlyphs({bitmaps}, {});
    const Neuron::GlyphAtlas::Font& font = atlas.fonts.front();
    for (const Neuron::GlyphBitmap& glyph : bitmaps.glyphs)
      Assert::IsTrue(glyph.advance > 0.0f);
    const Neuron::GlyphAtlas::Glyph& letter = font.For(U'A');
    Assert::IsTrue(letter.width > 0 && letter.height > 0);
    Assert::IsTrue(font.For(U'W').advance > font.For(U'i').advance);
    Assert::IsTrue(font.For(char32_t{0xD7}).width > 0);
    Assert::IsTrue(font.lineHeight >= font.ascent && font.ascent > 0.0f);
  }

  // ADR-030: a font takes the first of its families that is installed, and one with none installed fails, naming them,
  // rather than drawing in some other font.
  TEST_METHOD(TakesTheFirstInstalledFamily)
  {
    Neuron::FontDesc font = Segoe();
    font.families = {L"No Such Font Outpost", L"Segoe UI"};
    Assert::IsTrue(Neuron::RasterizeFont(font, 16.0f).family == L"Segoe UI");
    font.families = {L"No Such Font Outpost"};
    Assert::ExpectException<Neuron::Exception>([&] { (void)Neuron::RasterizeFont(font, 16.0f); });
    font.families = {};
    Assert::ExpectException<Neuron::Exception>([&] { (void)Neuron::RasterizeFont(font, 16.0f); });
  }
};
} // namespace GameAppTests

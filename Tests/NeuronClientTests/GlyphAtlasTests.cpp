#include "pch.h"

#include <bit>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace NeuronClientTests
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

// How many separate runs of half coverage or more a sprite has down its middle column.
int RunsDownTheMiddle(const Neuron::GlyphBitmap& _sprite)
{
  int runs = 0;
  bool inside = false;
  for (std::uint32_t y = 0; y < _sprite.height; ++y)
  {
    const bool covered = At(_sprite, _sprite.width / 2, y) >= 128;
    runs += covered && !inside ? 1 : 0;
    inside = covered;
  }
  return runs;
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
    std::vector<Neuron::FontBitmaps> fonts{MadeUpFont(0), MadeUpFont(100)};
    // The second font has halos, which are packed after its glyphs.
    for (const Neuron::GlyphBitmap& glyph : MadeUpFont(200).glyphs)
      fonts[1].halos.push_back(glyph);
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
      Assert::AreEqual(fonts[font].halos.size(), atlas.fonts[font].halos.size());
      for (size_t i = 0; i < fonts[font].halos.size(); ++i)
        claim(atlas.fonts[font].halos[i], fonts[font].halos[i]);
    }
    Assert::IsNull(atlas.fonts[0].HaloFor(U'A'), L"no halos");
    Assert::IsTrue(atlas.fonts[1].HaloFor(U'A') == &atlas.fonts[1].halos[Neuron::GlyphIndexOf(U'A')]);
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

  // ADR-030: the sprites are drawn, not shipped. A gem is full at its middle and empty in its corners, flat on top, full
  // in its shoulders where a diamond would be empty, and narrowing to a point at its foot (ADR-085 decision 3); a checkbox
  // is full along its edges and empty inside; a corner bracket is full along its top and left edges only.
  TEST_METHOD(DrawsTheSprites)
  {
    const Neuron::GlyphBitmap gem = Neuron::DrawSprite(Neuron::SpriteShape::Gem, 24);
    Assert::AreEqual(24u, gem.width);
    Assert::AreEqual(std::uint8_t{255}, At(gem, 12, 12));
    Assert::AreEqual(std::uint8_t{0}, At(gem, 0, 0));
    Assert::AreEqual(std::uint8_t{0}, At(gem, 23, 23));
    Assert::AreEqual(At(gem, 5, 10), At(gem, 18, 10), L"symmetric");
    Assert::IsTrue(At(gem, 12, 4) == 255 && At(gem, 12, 2) == 0, L"a flat top");
    Assert::AreEqual(std::uint8_t{255}, At(gem, 6, 5), L"a shoulder");
    Assert::IsTrue(At(gem, 12, 19) == 255 && At(gem, 6, 18) == 0, L"narrowing to its foot");

    const Neuron::GlyphBitmap box = Neuron::DrawSprite(Neuron::SpriteShape::Checkbox, 24);
    Assert::AreEqual(std::uint8_t{255}, At(box, 0, 12));
    Assert::AreEqual(std::uint8_t{255}, At(box, 23, 12));
    Assert::AreEqual(std::uint8_t{0}, At(box, 12, 12));

    const Neuron::GlyphBitmap corner = Neuron::DrawSprite(Neuron::SpriteShape::CornerBracket, 24);
    Assert::AreEqual(std::uint8_t{255}, At(corner, 0, 20));
    Assert::AreEqual(std::uint8_t{255}, At(corner, 20, 0));
    Assert::AreEqual(std::uint8_t{0}, At(corner, 20, 20));
    Assert::AreEqual(std::uint8_t{0}, At(corner, 23, 12));

    Assert::AreEqual(1u, Neuron::DrawSprite(Neuron::SpriteShape::Gem, 0).width, L"never empty");

    const Neuron::GlyphBitmap disc = Neuron::DrawSprite(Neuron::SpriteShape::Disc, 24);
    Assert::AreEqual(std::uint8_t{255}, At(disc, 12, 12));
    Assert::AreEqual(std::uint8_t{255}, At(disc, 1, 12), L"full to its edge");
    Assert::AreEqual(std::uint8_t{0}, At(disc, 1, 1), L"round");

    const Neuron::GlyphBitmap ring = Neuron::DrawSprite(Neuron::SpriteShape::Ring, 24);
    Assert::IsTrue(At(ring, 0, 12) > 240 && At(ring, 12, 23) > 240, L"a line along its edge");
    Assert::AreEqual(std::uint8_t{0}, At(ring, 12, 12), L"empty inside");
    Assert::AreEqual(std::uint8_t{0}, At(ring, 1, 1), L"round");

    // A rank's chevrons point up and stack, centered: one, two or three runs down the middle, each wider at its foot.
    const std::array<Neuron::SpriteShape, 3> ranks{Neuron::SpriteShape::Chevron, Neuron::SpriteShape::TwoChevrons,
                                                   Neuron::SpriteShape::ThreeChevrons};
    for (std::size_t rank = 0; rank < ranks.size(); ++rank)
    {
      const Neuron::GlyphBitmap chevrons = Neuron::DrawSprite(ranks[rank], 24);
      Assert::AreEqual(static_cast<int>(rank) + 1, RunsDownTheMiddle(chevrons));
      Assert::AreEqual(At(chevrons, 4, 16), At(chevrons, 19, 16), L"symmetric");
      Assert::AreEqual(std::uint8_t{0}, At(chevrons, 0, 0));
    }
    const Neuron::GlyphBitmap chevron = Neuron::DrawSprite(Neuron::SpriteShape::Chevron, 24);
    std::uint32_t point = 0;
    while (At(chevron, 12, point) < 128)
      ++point;
    Assert::IsTrue(At(chevron, 12, point) >= 128 && At(chevron, 5, point) < 128 && At(chevron, 5, point + 4) >= 128,
                   L"its point at the top, its arms lower down");

    const Neuron::GlyphBitmap plus = Neuron::DrawSprite(Neuron::SpriteShape::Plus, 24);
    Assert::AreEqual(std::uint8_t{255}, At(plus, 12, 12));
    Assert::AreEqual(std::uint8_t{255}, At(plus, 5, 12), L"its left bar");
    Assert::AreEqual(std::uint8_t{255}, At(plus, 12, 18), L"its lower bar");
    Assert::AreEqual(std::uint8_t{0}, At(plus, 5, 5), L"empty between its bars");
  }

  // A halo is its glyph widened by its reach on every side and blurred, at the same pen position and advance, so that it
  // stands under the glyph a few pixels round it; a glyph with nothing to draw has none. A font's halos reach a share of its
  // em, a pixel at least, and a font without a halo has none.
  TEST_METHOD(DrawsAHaloRoundEachGlyph)
  {
    const Neuron::GlyphBitmap glyph = Bitmap(4, 6, 255);
    const Neuron::GlyphBitmap halo = Neuron::HaloOf(glyph, 2);
    Assert::AreEqual(8u, halo.width);
    Assert::AreEqual(10u, halo.height);
    Assert::AreEqual(glyph.offsetX - 2, halo.offsetX);
    Assert::AreEqual(glyph.offsetY - 2, halo.offsetY);
    Assert::AreEqual(glyph.advance, halo.advance);
    Assert::AreEqual(std::uint8_t{255}, At(halo, 4, 5), L"full where the glyph is");
    Assert::IsTrue(At(halo, 1, 5) > 128, L"widened past the glyph's edge");
    Assert::IsTrue(At(halo, 0, 5) > 0 && At(halo, 0, 5) < 255, L"soft at its own edge");
    Assert::IsTrue(At(halo, 0, 0) < At(halo, 1, 5), L"round at its corners");

    Assert::AreEqual(0u, Neuron::HaloOf(Neuron::GlyphBitmap{.advance = 5.0f}, 2).width, L"none for a space");
    Assert::AreEqual(5.0f, Neuron::HaloOf(Neuron::GlyphBitmap{.advance = 5.0f}, 2).advance);

    const Neuron::FontBitmaps font = MadeUpFont(0);
    Assert::IsTrue(Neuron::HalosOf(font, 20.0f, 0.0f).empty());
    const std::vector<Neuron::GlyphBitmap> halos = Neuron::HalosOf(font, 20.0f, 0.15f);
    Assert::AreEqual(font.glyphs.size(), halos.size());
    Assert::AreEqual(font.glyphs[1].width + 6u, halos[1].width, L"3 pixels of a 20-pixel em");
    Assert::AreEqual(font.glyphs[1].width + 2u, Neuron::HalosOf(font, 4.0f, 0.15f)[1].width, L"a pixel at least");
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
} // namespace NeuronClientTests

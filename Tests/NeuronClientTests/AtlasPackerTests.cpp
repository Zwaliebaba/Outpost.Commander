#include "pch.h"

#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace NeuronClientTests
{

namespace
{
/// Do two placed slots share a texel? The packer's one absolute obligation.
[[nodiscard]] bool Overlaps(const Neuron::AtlasSlot& _first, const Neuron::AtlasSlot& _second) noexcept
{
  const bool apart = ((_first.left + _first.widthPixels) <= _second.left) || ((_second.left + _second.widthPixels) <= _first.left) ||
                     ((_first.top + _first.heightPixels) <= _second.top) || ((_second.top + _second.heightPixels) <= _first.top);
  return !apart;
}
} // namespace

/// `TechnicalDesign.md` §8 names atlas packing as one of M1.12's two pinned properties. This is the half
/// of the glyph atlas a suite can reach at all: `GlyphAtlas` needs DirectWrite and a graphics device and
/// `AtlasPacker` needs neither, which is why they are two classes (R9).
TEST_CLASS(TheAtlasPacker)
{
public:
  /// **NOTHING EVER OVERLAPS ANYTHING.** Asserted over a hundred rectangles of varying size against
  /// every other, rather than over a handful by inspection -- an off-by-one in the shelf advance shows
  /// as one shared row between two rows of glyphs, which is exactly the defect that would be read as
  /// "the font looks a bit dirty" rather than as a packing bug.
  TEST_METHOD(NoTwoSlotsEverOverlap)
  {
    Neuron::AtlasPacker packer;
    packer.Reset(256, 256);

    std::vector<Neuron::AtlasSlot> placed;
    for (std::uint32_t index = 0; index < 100; ++index)
    {
      // Sizes that vary the way glyphs do -- a narrow `i` against a wide `W` -- and that do not divide
      // the atlas evenly, so a shelf rarely ends flush.
      const std::uint32_t width = 5 + ((index * 7) % 23);
      const std::uint32_t height = 9 + ((index * 5) % 11);

      Neuron::AtlasSlot slot{};
      if (!packer.Place(width, height, slot))
      {
        break;
      }

      Assert::IsTrue((slot.left + slot.widthPixels) <= packer.WidthPixels(), L"a slot left the atlas to the right");
      Assert::IsTrue((slot.top + slot.heightPixels) <= packer.HeightPixels(), L"a slot left the atlas at the bottom");

      for (const Neuron::AtlasSlot& earlier : placed)
      {
        Assert::IsFalse(Overlaps(earlier, slot), L"two slots share a texel");
      }
      placed.push_back(slot);
    }

    Assert::IsTrue(placed.size() > 50, L"the packer gave up far too early to have been exercised");
  }

  /// A row fills left to right, and the tops agree while it does.
  TEST_METHOD(ARowFillsLeftToRightAtOneHeight)
  {
    Neuron::AtlasPacker packer;
    packer.Reset(100, 100);

    Neuron::AtlasSlot first{};
    Neuron::AtlasSlot second{};
    Neuron::AtlasSlot third{};
    Assert::IsTrue(packer.Place(30, 10, first));
    Assert::IsTrue(packer.Place(30, 10, second));
    Assert::IsTrue(packer.Place(30, 10, third));

    Assert::AreEqual(0u, first.left);
    Assert::AreEqual(30u, second.left);
    Assert::AreEqual(60u, third.left);
    Assert::AreEqual(0u, first.top);
    Assert::AreEqual(0u, second.top);
    Assert::AreEqual(0u, third.top);
  }

  /// **A NEW SHELF OPENS AT THE TALLEST POINT OF THE ONE BELOW**, not at the height of the rectangle
  /// that overflowed. Getting that wrong is what makes tall glyphs bleed into the next row.
  TEST_METHOD(ANewShelfClearsTheTallestOnTheOneBelow)
  {
    Neuron::AtlasPacker packer;
    packer.Reset(100, 100);

    Neuron::AtlasSlot shortOne{};
    Neuron::AtlasSlot tallOne{};
    Neuron::AtlasSlot overflow{};
    Assert::IsTrue(packer.Place(40, 8, shortOne));
    Assert::IsTrue(packer.Place(40, 25, tallOne));

    // 40 + 40 + 40 is past 100, so this one opens a shelf.
    Assert::IsTrue(packer.Place(40, 12, overflow));

    Assert::AreEqual(0u, overflow.left, L"the new shelf did not start at the left edge");
    Assert::AreEqual(25u, overflow.top, L"the new shelf did not clear the tallest rectangle below it");
  }

  /// The used height is the bottom of the current shelf, which is the measurement the packing-quality
  /// argument in `AtlasPacker.h` rests on.
  TEST_METHOD(TheUsedHeightIsTheBottomOfTheLastShelf)
  {
    Neuron::AtlasPacker packer;
    packer.Reset(100, 100);
    Assert::AreEqual(0u, packer.UsedHeightPixels(), L"an empty packer has used nothing");

    Neuron::AtlasSlot slot{};
    Assert::IsTrue(packer.Place(40, 8, slot));
    Assert::AreEqual(8u, packer.UsedHeightPixels());

    Assert::IsTrue(packer.Place(40, 25, slot));
    Assert::AreEqual(25u, packer.UsedHeightPixels(), L"the shelf did not grow to its tallest rectangle");

    Assert::IsTrue(packer.Place(40, 12, slot));
    Assert::AreEqual(37u, packer.UsedHeightPixels(), L"a second shelf did not add its own height");
  }

  /// **A RECTANGLE WIDER THAN THE ATLAS IS REFUSED RATHER THAN SHELVED FOREVER.** Without the early
  /// check it opens a fresh shelf, fails, and leaves the packer having consumed a row for nothing.
  TEST_METHOD(SomethingWiderThanTheAtlasIsRefusedWithoutConsumingAShelf)
  {
    Neuron::AtlasPacker packer;
    packer.Reset(100, 100);

    Neuron::AtlasSlot slot{};
    Assert::IsTrue(packer.Place(50, 10, slot));
    const std::uint32_t before = packer.UsedHeightPixels();

    Assert::IsFalse(packer.Place(101, 10, slot), L"a rectangle wider than the atlas was placed");
    Assert::AreEqual(before, packer.UsedHeightPixels(), L"refusing it still cost a shelf");

    // And the packer is still usable afterwards.
    Assert::IsTrue(packer.Place(40, 10, slot));
    Assert::AreEqual(50u, slot.left, L"the refusal disturbed the cursor");
  }

  /// A full atlas refuses rather than writing outside itself.
  TEST_METHOD(AFullAtlasRefuses)
  {
    Neuron::AtlasPacker packer;
    packer.Reset(50, 20);

    Neuron::AtlasSlot slot{};
    Assert::IsTrue(packer.Place(50, 20, slot));
    Assert::IsFalse(packer.Place(1, 1, slot), L"something was placed past the bottom of the atlas");
  }

  /// **A SPACE IS A GLYPH.** It has an advance and no coverage, and refusing to place it would make
  /// every caller special-case the commonest character in any string.
  TEST_METHOD(AZeroSizedRectangleIsPlacedRatherThanRefused)
  {
    Neuron::AtlasPacker packer;
    packer.Reset(100, 100);

    Neuron::AtlasSlot slot{};
    Assert::IsTrue(packer.Place(0, 0, slot), L"a space was refused");
    Assert::AreEqual(0u, slot.widthPixels);
    Assert::AreEqual(0u, packer.UsedHeightPixels(), L"a space consumed height");
  }

  /// Reset empties it, which is the resize and device-removal path ADR-009 names.
  TEST_METHOD(ResetEmptiesIt)
  {
    Neuron::AtlasPacker packer;
    packer.Reset(100, 100);

    Neuron::AtlasSlot slot{};
    Assert::IsTrue(packer.Place(90, 40, slot));

    packer.Reset(200, 80);
    Assert::AreEqual(200u, packer.WidthPixels());
    Assert::AreEqual(80u, packer.HeightPixels());
    Assert::AreEqual(0u, packer.UsedHeightPixels());

    Assert::IsTrue(packer.Place(10, 10, slot));
    Assert::AreEqual(0u, slot.left);
    Assert::AreEqual(0u, slot.top);
  }
};

/// **ADR-009's SECOND TRAP, PINNED AGAINST HAND ARITHMETIC.** The record names it directly: ClearType
/// coverage is gamma-encoded, and averaging the encoded values gives systematically thin or fat stems.
///
/// The expected values below are computed from the sRGB transfer function by hand -- decode, mean,
/// re-encode -- rather than from the implementation, which is the whole point. A test that asserted the
/// function against itself would pass just as happily on the naive average.
TEST_CLASS(TheClearTypeAverage)
{
public:
  /// **TWO SUBPIXELS OF THREE IS 213, NOT 170**, and that 43-of-255 gap is the defect. A stem covering
  /// two thirds of a pixel is two thirds of the *light*, and two thirds of the light encodes to 213.
  TEST_METHOD(TwoSubpixelsOfThreeIsTwoThirdsOfTheLightAndNotOfTheBytes)
  {
    Assert::AreEqual(213, static_cast<int>(Neuron::AverageClearTypeCoverage(255, 255, 0)), L"the average was not taken in linear space");
    Assert::AreEqual(213, static_cast<int>(Neuron::AverageClearTypeCoverage(0, 255, 255)), L"the average depended on which subpixels");

    // The naive answer, stated so the difference is on the record rather than implied.
    const int naive = (255 + 255 + 0) / 3;
    Assert::AreEqual(170, naive);
    Assert::IsTrue(Neuron::AverageClearTypeCoverage(255, 255, 0) > 200, L"the result is the naive one and every stem will be thin");
  }

  /// One subpixel of three: 156 against the naive 85, which is the same error at nearly twice the size.
  TEST_METHOD(OneSubpixelOfThreeIsAThirdOfTheLight)
  {
    Assert::AreEqual(156, static_cast<int>(Neuron::AverageClearTypeCoverage(255, 0, 0)));
    Assert::AreEqual(156, static_cast<int>(Neuron::AverageClearTypeCoverage(0, 0, 255)));
  }

  /// **AN EQUAL TRIPLE COMES BACK UNCHANGED**, which is the property that says the decode and the encode
  /// are actually inverses. If they were not, every glyph would shift in brightness with no pattern
  /// anybody could name.
  TEST_METHOD(AnEqualTripleIsItself)
  {
    for (int value = 0; value <= 255; value += 17)
    {
      const std::uint8_t byte = static_cast<std::uint8_t>(value);
      const int back = static_cast<int>(Neuron::AverageClearTypeCoverage(byte, byte, byte));
      Assert::IsTrue(std::abs(back - value) <= 1, L"a uniform coverage did not survive the round trip");
    }
  }

  /// The two ends, exactly: nothing is nothing and full is full.
  TEST_METHOD(TheEndsAreExact)
  {
    Assert::AreEqual(0, static_cast<int>(Neuron::AverageClearTypeCoverage(0, 0, 0)));
    Assert::AreEqual(255, static_cast<int>(Neuron::AverageClearTypeCoverage(255, 255, 255)));
  }

  /// It rises with every channel, so a partially covered subpixel is never darker than an empty one.
  TEST_METHOD(ItRisesWithEveryChannel)
  {
    int previous = -1;
    for (int value = 0; value <= 255; value += 5)
    {
      const std::uint8_t byte = static_cast<std::uint8_t>(value);
      const int here = static_cast<int>(Neuron::AverageClearTypeCoverage(byte, 0, 0));
      Assert::IsTrue(here >= previous, L"coverage fell as a subpixel filled");
      previous = here;
    }
  }

  /// A mixed triple, hand-computed: 141 against the naive 128.
  TEST_METHOD(AMixedTripleIsPinned)
  {
    Assert::AreEqual(141, static_cast<int>(Neuron::AverageClearTypeCoverage(64, 128, 192)));
  }
};

/// `TechnicalDesign.md` §8's other pinned property: **a glyph's advance width survives the round trip**
/// from the font's design units into physical pixels.
TEST_CLASS(TheAdvanceWidthRoundTrip)
{
public:
  /// Segoe UI's em is 2,048 design units. A glyph one em wide is therefore exactly the em size in
  /// pixels -- 40 at BODY on the target device, 64 at DISPLAY.
  TEST_METHOD(AFullEmAdvanceIsTheEmSize)
  {
    Assert::AreEqual(40.0f, Neuron::AdvancePixels(2048, 2048, 40.0f), 0.0001f);
    Assert::AreEqual(64.0f, Neuron::AdvancePixels(2048, 2048, 64.0f), 0.0001f);
  }

  /// A half-em advance is half the em size, at both sizes, which is the proportionality the layout
  /// depends on.
  TEST_METHOD(ItIsProportionalInBothArguments)
  {
    Assert::AreEqual(20.0f, Neuron::AdvancePixels(1024, 2048, 40.0f), 0.0001f);
    Assert::AreEqual(32.0f, Neuron::AdvancePixels(1024, 2048, 64.0f), 0.0001f);

    // Double the design advance, double the result.
    const float single = Neuron::AdvancePixels(600, 2048, 40.0f);
    const float doubled = Neuron::AdvancePixels(1200, 2048, 40.0f);
    Assert::AreEqual(single * 2.0f, doubled, 0.0001f);
  }

  /// **THE TWO AUTHORED SIZES ARE 20 AND 32, AND THE FIT'S EXACT 2x MAKES THEM 40 AND 64.** Both even,
  /// which is what the handoff's type table says and why it says it -- an odd authored size would land
  /// on a half pixel at some scale.
  TEST_METHOD(TheAuthoredSizesLandOnWholePhysicalPixels)
  {
    Assert::AreEqual(20, Neuron::BODY_AUTHORED_PIXELS);
    Assert::AreEqual(32, Neuron::DISPLAY_AUTHORED_PIXELS);
    Assert::AreEqual(0, Neuron::BODY_AUTHORED_PIXELS % 2, L"an odd authored size cannot double exactly");
    Assert::AreEqual(0, Neuron::DISPLAY_AUTHORED_PIXELS % 2, L"an odd authored size cannot double exactly");

    Assert::AreEqual(40.0f, static_cast<float>(Neuron::BODY_AUTHORED_PIXELS) * 2.0f, 0.0001f);
    Assert::AreEqual(64.0f, static_cast<float>(Neuron::DISPLAY_AUTHORED_PIXELS) * 2.0f, 0.0001f);
  }

  /// A zero em returns zero rather than dividing by it.
  TEST_METHOD(AZeroEmIsZeroRatherThanADivideByZero)
  {
    Assert::AreEqual(0.0f, Neuron::AdvancePixels(1000, 0, 40.0f), 0.0001f);
  }
};

} // namespace NeuronClientTests

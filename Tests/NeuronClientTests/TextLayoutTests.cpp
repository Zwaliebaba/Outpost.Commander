#include "pch.h"

#include <cmath>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace NeuronClientTests
{

namespace
{
inline constexpr std::uint32_t ATLAS_WIDTH = 1024;
inline constexpr std::uint32_t ATLAS_HEIGHT = 512;

/// **A TABLE BUILT BY HAND**, which is the reason `GlyphTable` is separate from `GlyphAtlas`: every
/// advance, bearing and slot here is a number this file chose, so every position below is arithmetic a
/// reader can do rather than a figure copied out of whatever Segoe UI happens to measure.
///
/// Every glyph is 10 pixels wide at _scale 1 with a 12-pixel advance and a 1-pixel left bearing; `i` is
/// narrow; space has an advance and no ink. At _scale 2 everything doubles, which is what a font at twice
/// the em size does.
[[nodiscard]] Neuron::GlyphTable HandTable(float _scale)
{
  Neuron::GlyphTable table{};
  for (std::size_t index = 0; index < table.size(); ++index)
  {
    Neuron::GlyphEntry& entry = table[index];
    entry.present = true;
    entry.advancePixels = 12.0f * _scale;
    entry.bearingXPixels = 1.0f * _scale;
    entry.bearingYPixels = -14.0f * _scale;
    entry.slot = Neuron::AtlasSlot{.left = static_cast<std::uint32_t>(index) * 40,
                                   .top = 8,
                                   .widthPixels = static_cast<std::uint32_t>(10.0f * _scale),
                                   .heightPixels = static_cast<std::uint32_t>(14.0f * _scale)};
  }

  Neuron::GlyphEntry& narrow = table[static_cast<std::size_t>(L'i' - Neuron::FIRST_CHARACTER)];
  narrow.advancePixels = 5.0f * _scale;
  narrow.slot.widthPixels = static_cast<std::uint32_t>(3.0f * _scale);

  Neuron::GlyphEntry& space = table[0];
  space.advancePixels = 6.0f * _scale;
  space.slot = Neuron::AtlasSlot{};
  return table;
}

/// An em of 20 at _scale 1 with the shape of Segoe UI's vertical metrics rounded to whole pixels: ascent
/// 22 and descent 5 against an em of 20, so the content area overhangs a 20-pixel line box.
[[nodiscard]] Neuron::FaceMetrics HandFace(float _scale) noexcept
{
  return Neuron::FaceMetrics{.emSizePixels = 20.0f * _scale, .ascentPixels = 22.0f * _scale, .descentPixels = 5.0f * _scale};
}

[[nodiscard]] Neuron::TextRun Run(const Neuron::PhysicalRect& _box, std::wstring_view _text, Neuron::TextAlign _align,
                                  float _trackingEm = 0.0f)
{
  return Neuron::TextRun{.box = _box, .text = _text, .align = _align, .trackingEm = _trackingEm, .color = {}};
}
} // namespace

/// `M1.13`'s first criterion: **a string's laid-out advance widths are pinned.**
TEST_CLASS(TheLaidOutAdvances)
{
public:
  /// Three glyphs at a 12-pixel advance and a 1-pixel bearing, from a pen at 100: the ink starts at 101,
  /// 113 and 125. Every one of those is a figure this test can state because the table is the test's.
  TEST_METHOD(EachGlyphLandsAtThePenPlusItsBearing)
  {
    const Neuron::GlyphTable table = HandTable(1.0f);
    std::vector<Neuron::GlyphQuad> quads;
    const std::size_t count =
      Neuron::LayoutText(table, HandFace(1.0f), ATLAS_WIDTH, ATLAS_HEIGHT,
                         Run({.left = 100, .top = 50, .right = 400, .bottom = 70}, L"ABC", Neuron::TextAlign::Left), quads);

    Assert::AreEqual(std::size_t{3}, count);
    Assert::AreEqual(101.0f, quads[0].left);
    Assert::AreEqual(113.0f, quads[1].left);
    Assert::AreEqual(125.0f, quads[2].left);
    Assert::AreEqual(111.0f, quads[0].right, L"a quad is not the width of its slot");
  }

  /// A narrow glyph moves the pen by its own advance and not by a fixed cell -- the thing a monospace
  /// shortcut gets wrong and a proportional readout shows immediately.
  TEST_METHOD(AProportionalAdvanceMovesThePenByItsOwnWidth)
  {
    const Neuron::GlyphTable table = HandTable(1.0f);
    std::vector<Neuron::GlyphQuad> quads;
    static_cast<void>(Neuron::LayoutText(table, HandFace(1.0f), ATLAS_WIDTH, ATLAS_HEIGHT,
                                         Run({.left = 0, .top = 0, .right = 400, .bottom = 20}, L"iA", Neuron::TextAlign::Left), quads));

    Assert::AreEqual(std::size_t{2}, quads.size());
    Assert::AreEqual(1.0f, quads[0].left);
    Assert::AreEqual(6.0f, quads[1].left, L"the pen did not move by i's five-pixel advance");
  }

  /// **A SPACE IS AN ADVANCE AND NO QUAD.** It moves the pen and draws nothing.
  TEST_METHOD(ASpaceAdvancesAndDrawsNothing)
  {
    const Neuron::GlyphTable table = HandTable(1.0f);
    std::vector<Neuron::GlyphQuad> quads;
    const std::size_t count =
      Neuron::LayoutText(table, HandFace(1.0f), ATLAS_WIDTH, ATLAS_HEIGHT,
                         Run({.left = 0, .top = 0, .right = 400, .bottom = 20}, L"A B", Neuron::TextAlign::Left), quads);

    Assert::AreEqual(std::size_t{2}, count);
    Assert::AreEqual(19.0f, quads[1].left, L"12 for A, 6 for the space, 1 of bearing");
  }

  /// **TRACKING IS ADDED AFTER EVERY CHARACTER, THE LAST INCLUDED**, which is what a browser's
  /// `letter-spacing` does and what the handoff's widths were measured with. 0.1 em at an em of 20 is 2
  /// pixels a character: three glyphs are 3 x (12 + 2) = 42 wide, not 40.
  TEST_METHOD(TrackingFollowsEveryCharacterIncludingTheLast)
  {
    const Neuron::GlyphTable table = HandTable(1.0f);
    Assert::AreEqual(36.0f, Neuron::MeasureRunPixels(table, HandFace(1.0f), L"ABC", 0.0f), 0.0001f);
    Assert::AreEqual(42.0f, Neuron::MeasureRunPixels(table, HandFace(1.0f), L"ABC", 0.1f), 0.0001f);
  }

  /// A character outside the printable range draws nothing and **moves the pen nowhere**, including by
  /// the tracking -- so it is a gap of zero, not a gap of one space.
  TEST_METHOD(AnUnknownCharacterIsAGapOfZero)
  {
    const Neuron::GlyphTable table = HandTable(1.0f);
    Assert::AreEqual(Neuron::MeasureRunPixels(table, HandFace(1.0f), L"AB", 0.1f),
                     Neuron::MeasureRunPixels(table, HandFace(1.0f),
                                              L"A\x00e9"
                                              L"B",
                                              0.1f),
                     0.0001f);
  }

  /// Right alignment puts the END of the run, trailing tracking included, on the box's right edge. A
  /// cost of `150` right-aligned in a 96-wide rect lands where the handoff drew it only if this holds.
  TEST_METHOD(RightAlignmentEndsTheRunAtTheRightEdge)
  {
    const Neuron::GlyphTable table = HandTable(1.0f);
    std::vector<Neuron::GlyphQuad> quads;
    static_cast<void>(Neuron::LayoutText(table, HandFace(1.0f), ATLAS_WIDTH, ATLAS_HEIGHT,
                                         Run({.left = 0, .top = 0, .right = 200, .bottom = 20}, L"AB", Neuron::TextAlign::Right), quads));

    // 24 wide, so the pen starts at 176: ink at 177 and 189.
    Assert::AreEqual(177.0f, quads[0].left);
    Assert::AreEqual(189.0f, quads[1].left);
  }

  /// Centered: 24 wide in a 100-wide box starting at 50 puts the pen at 88.
  TEST_METHOD(CenterAlignmentSplitsTheSlack)
  {
    const Neuron::GlyphTable table = HandTable(1.0f);
    std::vector<Neuron::GlyphQuad> quads;
    static_cast<void>(Neuron::LayoutText(table, HandFace(1.0f), ATLAS_WIDTH, ATLAS_HEIGHT,
                                         Run({.left = 50, .top = 0, .right = 150, .bottom = 20}, L"AB", Neuron::TextAlign::Center), quads));
    Assert::AreEqual(89.0f, quads[0].left);
  }
};

/// `M1.13`'s second criterion: **the same string at both sizes lands at the same authored origin.**
TEST_CLASS(TheTwoSizes)
{
public:
  /// The box is authored and carried through the interface fit; the size only changes the glyphs. So a
  /// left-aligned run's pen starts at the box's left edge at both sizes, and its first ink is the
  /// bearing away from it -- one pixel at BODY's scale, two at DISPLAY's.
  TEST_METHOD(ALeftAlignedRunStartsAtTheSameOriginAtBothSizes)
  {
    const Neuron::FitTransform fit = Neuron::ComputeInterfaceFit(2880, 1920);
    const Neuron::PhysicalRect box = Neuron::MapAuthoredRect(fit, Neuron::AuthoredRect{.left = 16, .top = 40, .right = 256, .bottom = 72});
    Assert::AreEqual(32, box.left, L"the authored origin did not reach 32 physical pixels through the 2x fit");

    std::vector<Neuron::GlyphQuad> small;
    std::vector<Neuron::GlyphQuad> large;
    static_cast<void>(
      Neuron::LayoutText(HandTable(1.0f), HandFace(1.0f), ATLAS_WIDTH, ATLAS_HEIGHT, Run(box, L"1,000", Neuron::TextAlign::Left), small));
    static_cast<void>(
      Neuron::LayoutText(HandTable(2.0f), HandFace(2.0f), ATLAS_WIDTH, ATLAS_HEIGHT, Run(box, L"1,000", Neuron::TextAlign::Left), large));

    Assert::AreEqual(33.0f, small.front().left);
    Assert::AreEqual(34.0f, large.front().left);
    Assert::AreEqual(small.front().left - 1.0f, large.front().left - 2.0f, L"the two sizes did not share a pen origin");
  }

  /// And a right-aligned run ends on the same edge at both sizes -- the case a cost and a count use.
  TEST_METHOD(ARightAlignedRunEndsAtTheSameEdgeAtBothSizes)
  {
    const Neuron::PhysicalRect box{.left = 0, .top = 0, .right = 300, .bottom = 64};
    const float smallEnd = static_cast<float>(box.right) - Neuron::MeasureRunPixels(HandTable(1.0f), HandFace(1.0f), L"300", 0.0f);
    const float largeEnd = static_cast<float>(box.right) - Neuron::MeasureRunPixels(HandTable(2.0f), HandFace(2.0f), L"300", 0.0f);

    std::vector<Neuron::GlyphQuad> small;
    std::vector<Neuron::GlyphQuad> large;
    static_cast<void>(
      Neuron::LayoutText(HandTable(1.0f), HandFace(1.0f), ATLAS_WIDTH, ATLAS_HEIGHT, Run(box, L"300", Neuron::TextAlign::Right), small));
    static_cast<void>(
      Neuron::LayoutText(HandTable(2.0f), HandFace(2.0f), ATLAS_WIDTH, ATLAS_HEIGHT, Run(box, L"300", Neuron::TextAlign::Right), large));

    Assert::AreEqual(smallEnd + 1.0f, small.front().left);
    Assert::AreEqual(largeEnd + 2.0f, large.front().left);
  }
};

/// Positions snap to whole pixels, and over-long strings clip rather than wrap.
TEST_CLASS(ThePixelGrid)
{
public:
  /// **NO QUAD LANDS BETWEEN PIXELS.** A fractional pen would resample a glyph that was rasterized at
  /// exactly its drawn size -- the one thing ADR-011 bought.
  TEST_METHOD(EveryEdgeIsAWholePixelEvenWithFractionalAdvances)
  {
    Neuron::GlyphTable table = HandTable(1.0f);
    for (Neuron::GlyphEntry& entry : table)
    {
      entry.advancePixels = 11.37f;
    }

    std::vector<Neuron::GlyphQuad> quads;
    static_cast<void>(Neuron::LayoutText(
      table, HandFace(1.0f), ATLAS_WIDTH, ATLAS_HEIGHT,
      Run({.left = 7, .top = 3, .right = 900, .bottom = 23}, L"ENGAGEMENT ENDED", Neuron::TextAlign::Center, 0.14f), quads));

    for (const Neuron::GlyphQuad& quad : quads)
    {
      Assert::AreEqual(std::round(quad.left), quad.left, L"a quad's left edge is between pixels");
      Assert::AreEqual(std::round(quad.top), quad.top, L"a quad's top edge is between pixels");
    }
  }

  /// **FRACTIONAL ADVANCES DO NOT ACCUMULATE ROUNDING.** Ten glyphs at 11.4 end at 114, which rounding
  /// each advance to 11 would put at 110.
  TEST_METHOD(ThePenAdvancesInFloatsAndSnapsPerGlyph)
  {
    Neuron::GlyphTable table = HandTable(1.0f);
    for (Neuron::GlyphEntry& entry : table)
    {
      entry.advancePixels = 11.4f;
      entry.bearingXPixels = 0.0f;
    }

    std::vector<Neuron::GlyphQuad> quads;
    static_cast<void>(Neuron::LayoutText(table, HandFace(1.0f), ATLAS_WIDTH, ATLAS_HEIGHT,
                                         Run({.left = 0, .top = 0, .right = 900, .bottom = 20}, L"AAAAAAAAAAA", Neuron::TextAlign::Left),
                                         quads));

    Assert::AreEqual(114.0f, quads[10].left, L"the tenth advance lost its fractions");
  }

  /// **AN OVER-LONG STRING IS CUT AT THE BOX**, and a glyph straddling the edge is cropped with its
  /// texture coordinate rather than squeezed into the space that is left.
  TEST_METHOD(AnOverLongRunIsCroppedAtTheRightEdge)
  {
    const Neuron::GlyphTable table = HandTable(1.0f);
    std::vector<Neuron::GlyphQuad> quads;
    static_cast<void>(Neuron::LayoutText(table, HandFace(1.0f), ATLAS_WIDTH, ATLAS_HEIGHT,
                                         Run({.left = 0, .top = 0, .right = 30, .bottom = 20}, L"ABCDE", Neuron::TextAlign::Left), quads));

    // A at 1-11, B at 13-23, C at 25-35 cut to 25-30, D and E wholly outside.
    Assert::AreEqual(std::size_t{3}, quads.size(), L"a glyph wholly outside the box was drawn");
    Assert::AreEqual(30.0f, quads[2].right);

    const float texelsKept = (quads[2].u1 - quads[2].u0) * static_cast<float>(ATLAS_WIDTH);
    Assert::AreEqual(5.0f, texelsKept, 0.001f, L"the cropped glyph was squeezed rather than cut");
  }
};

/// **THE BASELINE, WHICH THE HANDOFF'S COORDINATES DEPEND ON WITHOUT STATING.**
TEST_CLASS(TheBaseline)
{
public:
  /// CSS centers ascent plus descent in the line box. An ascent of 22 and a descent of 5 in a 20-pixel
  /// box: the content area is 27, the half-leading is -3.5, and the baseline sits 18.5 below the top.
  TEST_METHOD(ItIsTheAscentBelowTheCenteredContentArea)
  {
    Assert::AreEqual(118.5f, Neuron::BaselinePixels(HandFace(1.0f), 100.0f, 20.0f), 0.0001f);
  }

  /// Hand-computed against Segoe UI's own figures -- ascent 2,210, descent 514, em 2,048 -- at BODY's 40
  /// physical pixels in its 40-pixel box: content area 53.20, half-leading -6.60, ascent 43.16 -- so the
  /// baseline sits 36.56 below the top.
  TEST_METHOD(ItMatchesSegoeUiAtBody)
  {
    const float perUnit = 40.0f / 2048.0f;
    const Neuron::FaceMetrics face{.emSizePixels = 40.0f, .ascentPixels = 2210.0f * perUnit, .descentPixels = 514.0f * perUnit};
    Assert::AreEqual(36.5625f, Neuron::BaselinePixels(face, 0.0f, 40.0f), 0.001f);
  }

  /// And a laid-out glyph's top is the rounded baseline plus its bearing, so text sits on one line.
  TEST_METHOD(AGlyphHangsFromTheRoundedBaseline)
  {
    std::vector<Neuron::GlyphQuad> quads;
    static_cast<void>(Neuron::LayoutText(HandTable(1.0f), HandFace(1.0f), ATLAS_WIDTH, ATLAS_HEIGHT,
                                         Run({.left = 0, .top = 100, .right = 300, .bottom = 120}, L"A", Neuron::TextAlign::Left), quads));
    // 118.5 rounds half away from zero to 119; the bearing is -14.
    Assert::AreEqual(105.0f, quads[0].top);
  }
};

/// A solid plate is a glyph quad over the atlas's block of full coverage.
TEST_CLASS(TheSolidQuad)
{
public:
  /// It covers exactly the rectangle and samples one point in the middle of the block, so point
  /// sampling can never wander onto the block's edge.
  TEST_METHOD(ItCoversTheRectAndSamplesTheMiddleOfTheBlock)
  {
    std::vector<Neuron::GlyphQuad> quads;
    const Neuron::AtlasSlot solid{.left = 1, .top = 1, .widthPixels = 4, .heightPixels = 4};
    Assert::IsTrue(Neuron::AppendSolidQuad(solid, ATLAS_WIDTH, ATLAS_HEIGHT, {.left = 10, .top = 20, .right = 30, .bottom = 25},
                                           Neuron::HexColor(0x38D1F5), quads));

    Assert::AreEqual(10.0f, quads[0].left);
    Assert::AreEqual(25.0f, quads[0].bottom);
    Assert::AreEqual(3.0f / static_cast<float>(ATLAS_WIDTH), quads[0].u0, 0.000001f);
    Assert::AreEqual(quads[0].u0, quads[0].u1, L"a solid quad must sample one texel");
  }

  /// An empty rectangle appends nothing -- a bar at zero percent is a caller's ordinary case.
  TEST_METHOD(AnEmptyRectAppendsNothing)
  {
    std::vector<Neuron::GlyphQuad> quads;
    Assert::IsFalse(Neuron::AppendSolidQuad({.left = 1, .top = 1, .widthPixels = 4, .heightPixels = 4}, ATLAS_WIDTH, ATLAS_HEIGHT,
                                            {.left = 10, .top = 20, .right = 10, .bottom = 25}, Neuron::HexColor(0xFFFFFF), quads));
    Assert::IsTrue(quads.empty());
  }

  /// **THE PALETTE GOES IN AS ITS BYTES**, because the back buffer has no sRGB view (see `GlyphQuad`).
  TEST_METHOD(AHexColorIsItsBytes)
  {
    const Neuron::QuadColor color = Neuron::HexColor(0x38D1F5, 0.5f);
    Assert::AreEqual(56.0f / 255.0f, color.red, 0.000001f);
    Assert::AreEqual(209.0f / 255.0f, color.green, 0.000001f);
    Assert::AreEqual(245.0f / 255.0f, color.blue, 0.000001f);
    Assert::AreEqual(0.5f, color.alpha);
  }
};

} // namespace NeuronClientTests

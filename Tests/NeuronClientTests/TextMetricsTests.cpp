#include "pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace NeuronClientTests
{
namespace
{
// A font of the atlas made by hand, every character _advancePixels wide and a line _lineHeightPixels tall, so that a width
// is a count of characters.
Neuron::GlyphAtlas::Font EvenFont(float _advancePixels, float _lineHeightPixels = 20.0f)
{
  Neuron::GlyphAtlas::Font font;
  font.glyphs.assign(Neuron::GlyphAtlas::CHARACTER_COUNT, Neuron::GlyphAtlas::Glyph{.advance = _advancePixels});
  font.lineHeight = _lineHeightPixels;
  return font;
}
} // namespace

TEST_CLASS(TextMetricsTests)
{
public:
  // ADR-061 decision 1: a line is measured as it is set, each character at its advance and the tracking at a whole pixel, in
  // reference units at the atlas's scale; a font the atlas does not have measures nothing.
  TEST_METHOD(MeasuresALineAsItIsSet)
  {
    const std::array<Neuron::GlyphAtlas::Font, 2> fonts{EvenFont(10.0f), EvenFont(6.0f)};
    const Neuron::TextMetrics atOne(fonts, 1.0f);
    Assert::AreEqual(50.0f, atOne.Width(0, "Hello"));
    Assert::AreEqual(30.0f, atOne.Width(1, "Hello"));
    Assert::AreEqual(58.0f, atOne.Width(0, "Hello", 2.0f), L"four gaps of 2");
    Assert::AreEqual(0.0f, atOne.Width(2, "Hello"), L"no third font");

    // At twice the scale the font's pixels are halved back into units, and the tracking is rounded at the pixel.
    const Neuron::TextMetrics atTwo(fonts, 2.0f);
    Assert::AreEqual(25.0f, atTwo.Width(0, "Hello"));
    Assert::AreEqual(25.0f + 4.0f * 1.5f, atTwo.Width(0, "Hello", 1.4f), L"1.4 units is 3 whole pixels, 1.5 units");
  }

  // ADR-061 decision 3: a line that does not fit is cut short with three dots, never on a space; one that fits is whole.
  TEST_METHOD(FitsALineWithThreeDots)
  {
    const std::array<Neuron::GlyphAtlas::Font, 1> fonts{EvenFont(10.0f)};
    const Neuron::TextMetrics metrics(fonts, 1.0f);
    Assert::AreEqual(std::string("Shipyard"), metrics.Fit(0, "Shipyard", 80.0f));
    Assert::AreEqual(std::string("Ship..."), metrics.Fit(0, "Shipyard", 70.0f));
    Assert::AreEqual(std::string("Big..."), metrics.Fit(0, "Big Shipyard", 70.0f), L"no space before the dots");
    Assert::AreEqual(std::string(), metrics.Fit(0, "Shipyard", 20.0f), L"not even the dots fit");
  }

  // ADR-061 decision 3: text is broken between its words to the width, and a word wider than that is cut short.
  TEST_METHOD(WrapsTextBetweenWords)
  {
    const std::array<Neuron::GlyphAtlas::Font, 1> fonts{EvenFont(10.0f)};
    const Neuron::TextMetrics metrics(fonts, 1.0f);
    Assert::IsTrue(metrics.Wrap(0, "Mass Driver fire rate", 120.0f) == std::vector<std::string>{"Mass Driver", "fire rate"});
    Assert::IsTrue(metrics.Wrap(0, "Calibration", 60.0f) == std::vector<std::string>{"Cal..."});
    Assert::IsTrue(metrics.Wrap(0, "", 60.0f).empty());
  }
};
} // namespace NeuronClientTests

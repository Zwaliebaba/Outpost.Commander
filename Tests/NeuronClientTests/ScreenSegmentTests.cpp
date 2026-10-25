#include "pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace NeuronClientTests
{
namespace
{
// Whether _corner is (_x, _y), to a thousandth of a pixel.
void AssertAt(const DirectX::XMFLOAT2& _corner, float _x, float _y, const wchar_t* _what)
{
  Assert::AreEqual(_x, _corner.x, 0.001f, _what);
  Assert::AreEqual(_y, _corner.y, 0.001f, _what);
}

// The corners of _segment, which the test needs it to have.
std::array<DirectX::XMFLOAT2, 4> CornersOf(const Neuron::ScreenSegment& _segment)
{
  const std::optional<std::array<DirectX::XMFLOAT2, 4>> corners = _segment.Corners();
  Assert::IsTrue(corners.has_value(), L"a quad");
  return corners.value_or(std::array<DirectX::XMFLOAT2, 4>{});
}
} // namespace

TEST_CLASS(ScreenSegmentTests)
{
public:
  // ADR-015: a segment's quad, at 0°, 90° and 45°, half its width to either side and past either end, its corners in the
  // order a rectangle's are drawn, so that one at 0° is the rectangle FillRect would draw.
  TEST_METHOD(LaysAQuadAlongASegment)
  {
    const auto flat = CornersOf({.from = {10.0f, 20.0f}, .to = {30.0f, 20.0f}, .widthPixels = 2.0f});
    AssertAt(flat[0], 9.0f, 19.0f, L"0 degrees: top left");
    AssertAt(flat[1], 31.0f, 19.0f, L"top right");
    AssertAt(flat[2], 31.0f, 21.0f, L"bottom right");
    AssertAt(flat[3], 9.0f, 21.0f, L"bottom left");

    // Down the screen, its left is the screen's right.
    const auto down = CornersOf({.from = {10.0f, 20.0f}, .to = {10.0f, 40.0f}, .widthPixels = 4.0f});
    AssertAt(down[0], 12.0f, 18.0f, L"90 degrees: left of from");
    AssertAt(down[1], 12.0f, 42.0f, L"left of to");
    AssertAt(down[2], 8.0f, 42.0f, L"right of to");
    AssertAt(down[3], 8.0f, 18.0f, L"right of from");

    const float half = std::sqrt(2.0f) / 2.0f;
    const auto slant = CornersOf({.from = {0.0f, 0.0f}, .to = {10.0f, 10.0f}, .widthPixels = 2.0f});
    AssertAt(slant[0], -half + half, -half - half, L"45 degrees: left of from");
    AssertAt(slant[1], 10.0f + half + half, 10.0f + half - half, L"left of to");
    AssertAt(slant[2], 10.0f + half - half, 10.0f + half + half, L"right of to");
    AssertAt(slant[3], -half - half, -half + half, L"right of from");
    // Its sides are as long as the segment and its caps, and as far apart as its width.
    const auto distance = [](const DirectX::XMFLOAT2& _a, const DirectX::XMFLOAT2& _b) { return std::hypot(_a.x - _b.x, _a.y - _b.y); };
    Assert::AreEqual(std::hypot(10.0f, 10.0f) + 2.0f, distance(slant[0], slant[1]), 0.001f);
    Assert::AreEqual(2.0f, distance(slant[1], slant[2]), 0.001f);
  }

  // A segment of no length has no quad, rather than one turned at random.
  TEST_METHOD(DrawsNothingForASegmentOfNoLength)
  {
    Assert::IsFalse(Neuron::ScreenSegment{.from = {5.0f, 5.0f}, .to = {5.0f, 5.0f}, .widthPixels = 3.0f}.Corners().has_value());
  }
};
} // namespace NeuronClientTests

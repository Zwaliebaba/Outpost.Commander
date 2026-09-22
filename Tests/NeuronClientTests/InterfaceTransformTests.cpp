#include "pch.h"

#include <array>
#include <cstdint>
#include <utility>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace NeuronClientTests
{

namespace
{
/// The target device's panel (`Interface.md` section 1). The interface fit against it is an exact
/// 2x, which is what makes the 48-pixel touch floor land on 96 physical pixels with no fractional
/// edge anywhere.
inline constexpr std::int32_t PANEL_WIDTH = 2880;
inline constexpr std::int32_t PANEL_HEIGHT = 1920;

/// The whole authored frame, which every fit must map exactly onto its own fitted rectangle.
inline constexpr Neuron::AuthoredRect WHOLE_FRAME{
  .left = 0, .top = 0, .right = Neuron::INTERFACE_AUTHORED_WIDTH, .bottom = Neuron::INTERFACE_AUTHORED_HEIGHT};

/// `Interface.md` section 1's floor -- 48 x 48 authored -- placed at the bottom left with the 16
/// pixels of clear space the same section requires. It is the rectangle the probe draws, so the
/// number a pair of eyes checks on the device is the number asserted here.
inline constexpr Neuron::AuthoredRect TOUCH_FLOOR{.left = 16, .top = 896, .right = 64, .bottom = 944};

/// Windows this has to be right at. The panel, the panel in portrait, the 1080p monitor a developer
/// actually works on, a square one and a small one -- five aspect ratios and five scales, so a
/// transform that only works at an exact 2x fails here rather than on the device.
inline constexpr std::array<std::pair<std::int32_t, std::int32_t>, 5> WINDOWS{
  {{PANEL_WIDTH, PANEL_HEIGHT}, {1920, 1080}, {1440, 1440}, {1200, 800}, {640, 480}}};
} // namespace

TEST_CLASS(TheInterfaceTransform)
{
public:
  TEST_METHOD(TheWholeAuthoredFrameIsTheFittedRectangle)
  {
    // THE PROPERTY EVERYTHING ELSE RESTS ON, and the reason both edges are mapped rather than an
    // origin and an extent: authored 1440 lands on the fitted rectangle's right edge EXACTLY, at
    // every window size, so nothing in the interface can be off by a pixel at the frame's edge.
    for (const auto& [width, height] : WINDOWS)
    {
      const Neuron::FitTransform fit = Neuron::ComputeInterfaceFit(width, height);
      const Neuron::PhysicalRect placed = Neuron::MapAuthoredRect(fit, WHOLE_FRAME);
      Assert::AreEqual(fit.offsetX, placed.left);
      Assert::AreEqual(fit.offsetY, placed.top);
      Assert::AreEqual(fit.offsetX + fit.width, placed.right);
      Assert::AreEqual(fit.offsetY + fit.height, placed.bottom);
    }
  }

  TEST_METHOD(TheTouchFloorIsNinetySixPhysicalOnThePanel)
  {
    // `Interface.md` section 1: 48 authored pixels is 96 physical and 9.15 mm, and the 16 pixels of
    // clear space is 32. This is that whole derivation as one rectangle, and it is the rectangle
    // M0.17 puts on the glass for a pair of eyes.
    const Neuron::FitTransform fit = Neuron::ComputeInterfaceFit(PANEL_WIDTH, PANEL_HEIGHT);
    const Neuron::PhysicalRect placed = Neuron::MapAuthoredRect(fit, TOUCH_FLOOR);
    Assert::AreEqual(32, placed.left);
    Assert::AreEqual(1792, placed.top);
    Assert::AreEqual(128, placed.right);
    Assert::AreEqual(1888, placed.bottom);
    Assert::AreEqual(96, placed.WidthPixels());
    Assert::AreEqual(96, placed.HeightPixels());
  }

  TEST_METHOD(ItIsPlacedInsideTheFrameAtEveryWindowSize)
  {
    // NOTHING BRANCHES ON THE WINDOW SIZE, said as a property rather than by reading the code: one
    // authored rectangle, five windows, and at every one of them it lands inside the fitted frame
    // and keeps the 16 authored pixels of clear space from the left and bottom edges that
    // `Interface.md` requires.
    for (const auto& [width, height] : WINDOWS)
    {
      const Neuron::FitTransform fit = Neuron::ComputeInterfaceFit(width, height);
      const Neuron::PhysicalRect frame = Neuron::MapAuthoredRect(fit, WHOLE_FRAME);
      const Neuron::PhysicalRect placed = Neuron::MapAuthoredRect(fit, TOUCH_FLOOR);
      Assert::IsTrue(placed.left > frame.left, L"the target crossed the frame's left edge");
      Assert::IsTrue(placed.right < frame.right, L"the target crossed the frame's right edge");
      Assert::IsTrue(placed.top > frame.top, L"the target crossed the frame's top edge");
      Assert::IsTrue(placed.bottom < frame.bottom, L"the target crossed the frame's bottom edge");
      Assert::IsTrue(placed.WidthPixels() > 0);
      Assert::IsTrue(placed.HeightPixels() > 0);
    }
  }

  TEST_METHOD(FlushAuthoredRectanglesStayFlush)
  {
    // What mapping both edges buys, on the window where it is hardest: a bilinear fit whose scale
    // is 1.125. Two panels authored edge to edge must reach the glass edge to edge -- a transform
    // that scaled an origin and then a width would round twice and leave a seam that is one pixel
    // of the back buffer's clear color running down the interface.
    const Neuron::FitTransform fit = Neuron::ComputeInterfaceFit(1920, 1080);
    Assert::AreEqual(1.125f, fit.scale);

    for (std::int32_t edge = 1; edge < Neuron::INTERFACE_AUTHORED_WIDTH; edge += 7)
    {
      const Neuron::PhysicalRect left = Neuron::MapAuthoredRect(fit, {.left = 0, .top = 0, .right = edge, .bottom = 48});
      const Neuron::PhysicalRect right =
        Neuron::MapAuthoredRect(fit, {.left = edge, .top = 0, .right = Neuron::INTERFACE_AUTHORED_WIDTH, .bottom = 48});
      Assert::AreEqual(left.right, right.left);
    }
  }

  TEST_METHOD(ADegenerateFitCollapsesRatherThanDividing)
  {
    // `ComputeInterfaceFit` answers a window of no size with a zeroed transform, and this is what
    // that means downstream: an empty rectangle, which `InterfacePass::Record` draws nothing for.
    const Neuron::FitTransform fit = Neuron::ComputeInterfaceFit(0, 0);
    const Neuron::PhysicalRect placed = Neuron::MapAuthoredRect(fit, TOUCH_FLOOR);
    Assert::AreEqual(0, placed.WidthPixels());
    Assert::AreEqual(0, placed.HeightPixels());
  }
};

TEST_CLASS(TheInterfaceClipSpace)
{
public:
  TEST_METHOD(TheWholeFrameFillsClipSpaceOnThePanel)
  {
    // The panel is 3:2 and so is the authored frame, so there are no bars and the frame is the
    // whole back buffer. Exact rather than near: every one of these four values is a division that
    // lands on a power of two.
    const Neuron::FitTransform fit = Neuron::ComputeInterfaceFit(PANEL_WIDTH, PANEL_HEIGHT);
    const Neuron::ClipRect clip = Neuron::ToClipRect(Neuron::MapAuthoredRect(fit, WHOLE_FRAME), PANEL_WIDTH, PANEL_HEIGHT);
    Assert::AreEqual(-1.0f, clip.left);
    Assert::AreEqual(1.0f, clip.top);
    Assert::AreEqual(1.0f, clip.right);
    Assert::AreEqual(-1.0f, clip.bottom);
  }

  TEST_METHOD(TheYAxisFlipsHereAndTheTopIsTheLargerValue)
  {
    // The one thing in this file that is easy to get backwards and impossible to see afterwards: a
    // back buffer counts rows downwards and clip space counts them upwards, so an interface drawn
    // through an unflipped transform is simply upside down and every layout number still "works".
    const Neuron::FitTransform fit = Neuron::ComputeInterfaceFit(PANEL_WIDTH, PANEL_HEIGHT);
    const Neuron::ClipRect clip = Neuron::ToClipRect(Neuron::MapAuthoredRect(fit, TOUCH_FLOOR), PANEL_WIDTH, PANEL_HEIGHT);
    Assert::IsTrue(clip.top > clip.bottom, L"the top edge must be the larger clip-space y");
    Assert::IsTrue(clip.right > clip.left);

    // Bottom left of the frame, so both clip coordinates are negative and neither is near an edge
    // it should not be near.
    Assert::IsTrue(clip.left < 0.0f);
    Assert::IsTrue(clip.top < 0.0f);
  }

  TEST_METHOD(TheTopLeftQuarterIsTheTopLeftQuarter)
  {
    // A quarter of the authored frame, which lands on exact zeros in clip space -- the cheapest
    // possible assertion that the two halves of this transform compose the way they read.
    const Neuron::FitTransform fit = Neuron::ComputeInterfaceFit(PANEL_WIDTH, PANEL_HEIGHT);
    const Neuron::AuthoredRect quarter{
      .left = 0, .top = 0, .right = Neuron::INTERFACE_AUTHORED_WIDTH / 2, .bottom = Neuron::INTERFACE_AUTHORED_HEIGHT / 2};
    const Neuron::ClipRect clip = Neuron::ToClipRect(Neuron::MapAuthoredRect(fit, quarter), PANEL_WIDTH, PANEL_HEIGHT);
    Assert::AreEqual(-1.0f, clip.left);
    Assert::AreEqual(1.0f, clip.top);
    Assert::AreEqual(0.0f, clip.right);
    Assert::AreEqual(0.0f, clip.bottom);
  }

  TEST_METHOD(ItStaysInsideClipSpaceAtEveryWindowSize)
  {
    // The boxed windows are the interesting ones: the authored frame is letterboxed or pillarboxed
    // inside the back buffer, so the interface must NOT reach the clip-space edges -- a transform
    // that stretched to fill would pass every other test in this file.
    for (const auto& [width, height] : WINDOWS)
    {
      const Neuron::FitTransform fit = Neuron::ComputeInterfaceFit(width, height);
      const Neuron::ClipRect clip = Neuron::ToClipRect(Neuron::MapAuthoredRect(fit, WHOLE_FRAME), width, height);
      Assert::IsTrue(clip.left >= -1.0f);
      Assert::IsTrue(clip.right <= 1.0f);
      Assert::IsTrue(clip.top <= 1.0f);
      Assert::IsTrue(clip.bottom >= -1.0f);

      // Pillarboxed means bars down the sides, so the frame stops short of the left and right
      // edges and reaches the top and bottom exactly. Letterboxed is the same statement turned
      // ninety degrees.
      if (fit.IsPillarboxed())
      {
        Assert::IsTrue(clip.left > -1.0f);
        Assert::AreEqual(1.0f, clip.top);
      }
      if (fit.IsLetterboxed())
      {
        Assert::IsTrue(clip.top < 1.0f);
        Assert::AreEqual(-1.0f, clip.left);
      }
    }
  }

  TEST_METHOD(ABackBufferOfNoSizeIsEmptyRatherThanInfinite)
  {
    const Neuron::ClipRect clip = Neuron::ToClipRect({.left = 0, .top = 0, .right = 96, .bottom = 96}, 0, 0);
    Assert::AreEqual(0.0f, clip.left);
    Assert::AreEqual(0.0f, clip.top);
    Assert::AreEqual(0.0f, clip.right);
    Assert::AreEqual(0.0f, clip.bottom);
  }
};

} // namespace NeuronClientTests

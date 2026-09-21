#pragma once

#include "NeuronCore.h"

#include <cstdint>

namespace Neuron
{

/// THE ONE PLACE IN THE CLIENT THAT ASKS HOW BIG THE WINDOW IS (R13), and what it holds is what a
/// `CoreWindow` actually reports: a size in DEVICE-INDEPENDENT PIXELS, and a factor relating those
/// to physical ones.
///
/// R18 names this conversion specifically and requires a suite over it, because it is the one
/// place this application model can silently produce a worse picture: a swap chain created at DIPs
/// on a 200% display is half the resolution of the panel it is presented on, and nothing fails --
/// the game just looks soft, on the one device it is for.
///
/// R8: a public aggregate, so plain fields and brace initialization.
struct WindowMetrics
{
  /// What `CoreWindow::Bounds()` reports. Floating point because the Windows Runtime says so, and
  /// this is the renderer rather than the simulation -- ADR-002 puts floats here freely.
  float widthDips = 0.0f;
  float heightDips = 0.0f;

  /// `DisplayInformation::RawPixelsPerViewPixel()`. One at 100%, 1.5 at 150%, two at 200% -- and
  /// the Surface Pro this game is for ships at 200%, which is why ADR-007's authored 1440 x 960
  /// and its 2880 x 1920 panel are the same window described two ways.
  float rawPixelsPerViewPixel = 1.0f;
};

/// Device-independent pixels to physical pixels, rounded.
///
/// NEVER ZERO FOR A POSITIVE SIZE. A swap chain of zero width does not fail gracefully, and a
/// window being dragged between monitors can report a size small enough to round to nothing for
/// one frame. A non-positive input is zero, because that is genuinely no window.
[[nodiscard]] std::int32_t DipsToPhysicalPixels(float _dips, float _rawPixelsPerViewPixel) noexcept;

[[nodiscard]] std::int32_t PhysicalWidth(const WindowMetrics& _metrics) noexcept;
[[nodiscard]] std::int32_t PhysicalHeight(const WindowMetrics& _metrics) noexcept;

} // namespace Neuron

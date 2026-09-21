#include "pch.h"

#include "WindowMetrics.h"

#include <cmath>

namespace Neuron
{

std::int32_t DipsToPhysicalPixels(float _dips, float _rawPixelsPerViewPixel) noexcept
{
  if (!(_dips > 0.0f) || !(_rawPixelsPerViewPixel > 0.0f))
  {
    // Written as a negated greater-than so that a NaN -- which a window being torn between two
    // displays can briefly report -- lands here rather than sailing through a `<= 0` comparison
    // that is false for every NaN.
    return 0;
  }

  const float physical = _dips * _rawPixelsPerViewPixel;
  const std::int32_t rounded = static_cast<std::int32_t>(std::lround(physical));

  // A positive window is at least one pixel. See the header: a zero-width swap chain is not a
  // graceful failure, and a window mid-drag can report a size that rounds away.
  return (rounded < 1) ? 1 : rounded;
}

std::int32_t PhysicalWidth(const WindowMetrics& _metrics) noexcept
{
  return DipsToPhysicalPixels(_metrics.widthDips, _metrics.rawPixelsPerViewPixel);
}

std::int32_t PhysicalHeight(const WindowMetrics& _metrics) noexcept
{
  return DipsToPhysicalPixels(_metrics.heightDips, _metrics.rawPixelsPerViewPixel);
}

} // namespace Neuron

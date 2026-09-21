#include "pch.h"

#include "FitTransform.h"

namespace Neuron
{

FitTransform ComputeFit(std::int32_t _sourceWidth, std::int32_t _sourceHeight, std::int32_t _destinationWidth,
                        std::int32_t _destinationHeight) noexcept
{
  if ((_sourceWidth <= 0) || (_sourceHeight <= 0) || (_destinationWidth <= 0) || (_destinationHeight <= 0))
  {
    return FitTransform{};
  }

  // WHICH AXIS LIMITS, DECIDED BY CROSS-MULTIPLICATION RATHER THAN BY DIVIDING. Two divisions and
  // a float comparison would answer this too, and would answer it wrongly for exactly the sizes
  // that matter: a 2880-wide window against a 2880-wide target has to come out equal, not nearly.
  const std::int64_t byWidth = static_cast<std::int64_t>(_destinationWidth) * _sourceHeight;
  const std::int64_t byHeight = static_cast<std::int64_t>(_destinationHeight) * _sourceWidth;
  const bool widthLimits = byWidth <= byHeight;

  // The scale as an exact rational, which is what makes the filter decision integer arithmetic.
  const std::int64_t numerator = widthLimits ? _destinationWidth : _destinationHeight;
  const std::int64_t denominator = widthLimits ? _sourceWidth : _sourceHeight;

  FitTransform fit{};
  if (widthLimits)
  {
    fit.width = _destinationWidth;
    fit.height = static_cast<std::int32_t>(((static_cast<std::int64_t>(_sourceHeight) * numerator) + (denominator / 2)) / denominator);
  }
  else
  {
    fit.height = _destinationHeight;
    fit.width = static_cast<std::int32_t>(((static_cast<std::int64_t>(_sourceWidth) * numerator) + (denominator / 2)) / denominator);
  }

  fit.offsetX = (_destinationWidth - fit.width) / 2;
  fit.offsetY = (_destinationHeight - fit.height) / 2;
  fit.scale = static_cast<float>(numerator) / static_cast<float>(denominator);

  // R13'S THREE CASES, in integers. `numerator == denominator` is 1:1 exactly; a numerator that
  // divides evenly is an exact integer multiple; everything else resamples. No float is compared
  // to 1.0 or to 2.0 anywhere in this decision, which is the point.
  if (numerator == denominator)
  {
    fit.filter = FitFilter::None;
  }
  else if ((numerator % denominator) == 0)
  {
    fit.filter = FitFilter::Point;

    // An exact multiple, so say so exactly rather than through a division that might not be.
    fit.scale = static_cast<float>(numerator / denominator);
  }
  else
  {
    fit.filter = FitFilter::Bilinear;
  }

  return fit;
}

} // namespace Neuron

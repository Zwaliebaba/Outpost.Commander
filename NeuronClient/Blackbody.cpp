#include "pch.h"

#include "Blackbody.h"

#include <cmath>

namespace Neuron
{

namespace
{
/// A piecewise Gaussian: one standard deviation below the peak, another above. Wyman, Sloan and
/// Shirley's fit is built out of these, and the asymmetry is what lets two of them stand in for a
/// tabulated curve with a shoulder on one side.
[[nodiscard]] float PiecewiseGaussian(float _x, float _peak, float _sigmaBelow, float _sigmaAbove) noexcept
{
  const float sigma = (_x < _peak) ? _sigmaBelow : _sigmaAbove;
  const float t = (_x - _peak) / sigma;
  return std::exp(-0.5f * t * t);
}

/// The CIE 1931 two-degree observer, as the multi-lobe analytic fit. Three lobes for x, two each for y
/// and z -- see the header for why this is a fit rather than the tabulated data.
void ColorMatching(float _nanometres, float& _outX, float& _outY, float& _outZ) noexcept
{
  _outX = (1.056f * PiecewiseGaussian(_nanometres, 599.8f, 37.9f, 31.0f)) +
          (0.362f * PiecewiseGaussian(_nanometres, 442.0f, 16.0f, 26.7f)) - (0.065f * PiecewiseGaussian(_nanometres, 501.1f, 20.4f, 26.2f));

  _outY = (0.821f * PiecewiseGaussian(_nanometres, 568.8f, 46.9f, 40.5f)) + (0.286f * PiecewiseGaussian(_nanometres, 530.9f, 16.3f, 31.1f));

  _outZ = (1.217f * PiecewiseGaussian(_nanometres, 437.0f, 11.8f, 36.0f)) + (0.681f * PiecewiseGaussian(_nanometres, 459.0f, 26.0f, 13.8f));
}

/// Planck's law, in wavelength form, up to the constants that cancel when the result is normalized.
///
/// `c1 / (lambda^5 * (exp(c2 / (lambda * T)) - 1))`, with lambda in metres. Only the SHAPE matters
/// here -- every absolute factor divides out when the brightest channel is scaled to one -- so the
/// leading constant is dropped and the second radiation constant is the one that has to be right.
[[nodiscard]] double SpectralRadiance(double _nanometres, double _kelvin) noexcept
{
  // The second radiation constant, hc/k, in metre-kelvin.
  constexpr double SECOND_RADIATION_CONSTANT = 1.4387768775039337e-2;

  const double lambda = _nanometres * 1e-9;
  const double exponent = SECOND_RADIATION_CONSTANT / (lambda * _kelvin);

  // `expm1` rather than `exp(x) - 1`: at the blue end of a cool star the exponent is large and the
  // subtraction is fine, but at the red end of a hot one it is small and the naive form loses every
  // significant digit it has.
  return 1.0 / (std::pow(lambda, 5.0) * std::expm1(exponent));
}
} // namespace

Chromaticity BlackbodyColor(float _kelvin) noexcept
{
  const float clamped =
    (_kelvin < BLACKBODY_STOPS.front()) ? BLACKBODY_STOPS.front() : ((_kelvin > BLACKBODY_STOPS.back()) ? BLACKBODY_STOPS.back() : _kelvin);

  double sumX = 0.0;
  double sumY = 0.0;
  double sumZ = 0.0;
  for (float nanometres = SPECTRUM_START_NANOMETRES; nanometres <= SPECTRUM_END_NANOMETRES; nanometres += SPECTRUM_STEP_NANOMETRES)
  {
    float matchX = 0.0f;
    float matchY = 0.0f;
    float matchZ = 0.0f;
    ColorMatching(nanometres, matchX, matchY, matchZ);

    const double radiance = SpectralRadiance(nanometres, static_cast<double>(clamped));
    sumX += radiance * matchX;
    sumY += radiance * matchY;
    sumZ += radiance * matchZ;
  }

  // XYZ to linear sRGB (Rec. 709 primaries, D65). The step is not multiplied in: it is a constant
  // factor across all three sums and the normalization below removes it.
  const double red = (3.2406 * sumX) + (-1.5372 * sumY) + (-0.4986 * sumZ);
  const double green = (-0.9689 * sumX) + (1.8758 * sumY) + (0.0415 * sumZ);
  const double blue = (0.0557 * sumX) + (-0.2040 * sumY) + (1.0570 * sumZ);

  // **CLAMPED AT ZERO BEFORE NORMALIZING.** The sRGB gamut does not contain the whole Planckian locus,
  // so a cool star's blue and a hot star's red come out slightly negative -- which is the colour being
  // outside what the display can say rather than an error, and clamping is what saying so looks like.
  const double clampedRed = (red > 0.0) ? red : 0.0;
  const double clampedGreen = (green > 0.0) ? green : 0.0;
  const double clampedBlue = (blue > 0.0) ? blue : 0.0;

  double brightest = clampedRed;
  brightest = (clampedGreen > brightest) ? clampedGreen : brightest;
  brightest = (clampedBlue > brightest) ? clampedBlue : brightest;
  if (brightest <= 0.0)
  {
    return Chromaticity{};
  }

  return Chromaticity{.red = static_cast<float>(clampedRed / brightest),
                      .green = static_cast<float>(clampedGreen / brightest),
                      .blue = static_cast<float>(clampedBlue / brightest)};
}

const std::array<Chromaticity, BLACKBODY_STOP_COUNT>& BlackbodyTable() noexcept
{
  // Computed once, on first use. `static` in a free function is a local cache rather than mutable
  // state reaching an outcome -- and this file is the renderer's, which `Scripts/CheckDeterminism.py`
  // does not sweep and which R16 does not reach.
  static const std::array<Chromaticity, BLACKBODY_STOP_COUNT> table = []
  {
    std::array<Chromaticity, BLACKBODY_STOP_COUNT> computed{};
    for (std::size_t stop = 0; stop < BLACKBODY_STOP_COUNT; ++stop)
    {
      computed[stop] = BlackbodyColor(BLACKBODY_STOPS[stop]);
    }
    return computed;
  }();
  return table;
}

Chromaticity Desaturate(const Chromaticity& _color, float _saturation) noexcept
{
  const float amount = (_saturation < 0.0f) ? 0.0f : ((_saturation > 1.0f) ? 1.0f : _saturation);

  // Rec. 709 luminance, which is the grey this pulls toward -- so a desaturated star keeps the
  // brightness it had instead of dimming as it whitens.
  const float luminance = (0.2126f * _color.red) + (0.7152f * _color.green) + (0.0722f * _color.blue);

  return Chromaticity{.red = luminance + ((_color.red - luminance) * amount),
                      .green = luminance + ((_color.green - luminance) * amount),
                      .blue = luminance + ((_color.blue - luminance) * amount)};
}

} // namespace Neuron

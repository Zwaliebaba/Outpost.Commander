#include "pch.h"

#include "StarField.h"

#include <cmath>

namespace Neuron
{

namespace
{
inline constexpr float PI = 3.14159265358979323846f;

/// A float in [0, 1) from one draw. Twenty-four bits of the generator's thirty-two, which is every bit
/// a `float` mantissa can hold -- taking all thirty-two and dividing would throw the low eight away
/// silently and look like it had not.
[[nodiscard]] float NextUnit(Pcg32& _random) noexcept
{
  constexpr std::uint32_t MANTISSA_BITS = 24;
  constexpr float SCALE = 1.0f / static_cast<float>(1u << MANTISSA_BITS);
  return static_cast<float>(_random.Next() >> (32 - MANTISSA_BITS)) * SCALE;
}

/// A float in [-1, 1).
[[nodiscard]] float NextSigned(Pcg32& _random) noexcept
{
  return (NextUnit(_random) * 2.0f) - 1.0f;
}

/// Linear between two ends, at a position along a count.
[[nodiscard]] float Ramp(float _first, float _last, std::size_t _index, std::size_t _count) noexcept
{
  if (_count <= 1)
  {
    return _first;
  }
  const float t = static_cast<float>(_index) / static_cast<float>(_count - 1);
  return _first + ((_last - _first) * t);
}

/// An orthonormal basis whose third axis is the given pole. Used to put a direction sampled in
/// galactic coordinates -- latitude measured from the band -- into world space.
///
/// **THE HELPER AXIS IS CHOSEN AGAINST THE POLE'S SMALLEST COMPONENT**, which is the standard way to
/// avoid a cross product with something nearly parallel; picking a fixed helper gives a degenerate
/// basis exactly when the pole happens to point along it, and the sky would then be a line.
void BasisFromPole(float _poleX, float _poleY, float _poleZ, float (&_outRight)[3], float (&_outForward)[3], float (&_outPole)[3]) noexcept
{
  // **A DEGENERATE POLE BECOMES +Z RATHER THAN STAYING ZERO.** Dividing by a guarded length keeps the
  // arithmetic finite but leaves a zero-length "axis", and every direction built on it comes out zero --
  // a sky of three thousand stars stacked on the origin, which draws as nothing at all and looks like
  // the generator failing rather than like a caller passing a vector it never filled in.
  const float length = std::sqrt((_poleX * _poleX) + (_poleY * _poleY) + (_poleZ * _poleZ));
  if (length > 1e-6f)
  {
    _outPole[0] = _poleX / length;
    _outPole[1] = _poleY / length;
    _outPole[2] = _poleZ / length;
  }
  else
  {
    _outPole[0] = 0.0f;
    _outPole[1] = 0.0f;
    _outPole[2] = 1.0f;
  }

  const float absX = std::fabs(_outPole[0]);
  const float absY = std::fabs(_outPole[1]);
  const float absZ = std::fabs(_outPole[2]);

  float helper[3] = {0.0f, 0.0f, 0.0f};
  if ((absX <= absY) && (absX <= absZ))
  {
    helper[0] = 1.0f;
  }
  else if (absY <= absZ)
  {
    helper[1] = 1.0f;
  }
  else
  {
    helper[2] = 1.0f;
  }

  _outRight[0] = (_outPole[1] * helper[2]) - (_outPole[2] * helper[1]);
  _outRight[1] = (_outPole[2] * helper[0]) - (_outPole[0] * helper[2]);
  _outRight[2] = (_outPole[0] * helper[1]) - (_outPole[1] * helper[0]);

  const float rightLength = std::sqrt((_outRight[0] * _outRight[0]) + (_outRight[1] * _outRight[1]) + (_outRight[2] * _outRight[2]));
  const float safeRight = (rightLength > 1e-6f) ? rightLength : 1.0f;
  _outRight[0] /= safeRight;
  _outRight[1] /= safeRight;
  _outRight[2] /= safeRight;

  _outForward[0] = (_outPole[1] * _outRight[2]) - (_outPole[2] * _outRight[1]);
  _outForward[1] = (_outPole[2] * _outRight[0]) - (_outPole[0] * _outRight[2]);
  _outForward[2] = (_outPole[0] * _outRight[1]) - (_outPole[1] * _outRight[0]);
}
} // namespace

std::array<std::uint32_t, MAGNITUDE_TIER_COUNT> TierCounts(std::uint32_t _starCount) noexcept
{
  std::uint32_t parts = 0;
  for (const std::uint32_t ratio : MAGNITUDE_RATIO)
  {
    parts += ratio;
  }

  // **ROUNDED TO NEAREST RATHER THAN TRUNCATED**, and in 64 bits because `_starCount * 243` leaves a
  // 32-bit product above about 17.6 million stars. Truncating cost tier two a star it had earned --
  // its share of 3,000 is 24.7 and it was given 24 -- and handed the slack to the faintest tier, which
  // is the one place three stars make no visible difference. That is a fine place for a REMAINDER and
  // a poor place for five separate roundings.
  std::array<std::uint32_t, MAGNITUDE_TIER_COUNT> counts{};
  std::uint32_t assigned = 0;
  for (std::size_t tier = 0; (tier + 1) < MAGNITUDE_TIER_COUNT; ++tier)
  {
    const std::uint64_t scaled = (static_cast<std::uint64_t>(_starCount) * MAGNITUDE_RATIO[tier]) + (parts / 2);
    counts[tier] = static_cast<std::uint32_t>(scaled / parts);
    assigned += counts[tier];
  }

  // **THE REMAINDER GOES TO THE FAINTEST TIER.** Spreading it from the top would turn eight standouts
  // into nine or ten, and eight is the effect ADR-019 is buying.
  //
  // The five bright tiers are 121/364 of the field between them, so for any field worth drawing there
  // is a remainder and it is most of the field. A one-star field is the case where the five roundings
  // consume it exactly and the faintest tier gets none; the clamp is for that, and the sum equals
  // `_starCount` either way -- which `StarFieldTests` asserts across a range rather than at 3,000 only.
  counts[MAGNITUDE_TIER_COUNT - 1] = (assigned >= _starCount) ? 0 : (_starCount - assigned);
  return counts;
}

std::vector<Star> GenerateStarField(std::uint64_t _seed, const StarFieldDescription& _description)
{
  std::vector<Star> stars;
  if (_description.starCount == 0)
  {
    return stars;
  }
  stars.reserve(_description.starCount);

  Pcg32 random{_seed, STAR_FIELD_STREAM};

  float right[3] = {1.0f, 0.0f, 0.0f};
  float forward[3] = {0.0f, 1.0f, 0.0f};
  float pole[3] = {0.0f, 0.0f, 1.0f};
  BasisFromPole(_description.poleX, _description.poleY, _description.poleZ, right, forward, pole);

  const std::array<std::uint32_t, MAGNITUDE_TIER_COUNT> counts = TierCounts(_description.starCount);
  const float concentration = (_description.planeConcentration < 1.0f) ? 1.0f : _description.planeConcentration;

  for (std::size_t tier = 0; tier < MAGNITUDE_TIER_COUNT; ++tier)
  {
    const float size = Ramp(_description.brightestSizePixels, _description.faintestSizePixels, tier, MAGNITUDE_TIER_COUNT);
    const float value = Ramp(_description.brightestValue, _description.faintestValue, tier, MAGNITUDE_TIER_COUNT);
    const float kelvin = Ramp(_description.brightestKelvin, _description.faintestKelvin, tier, MAGNITUDE_TIER_COUNT);

    for (std::uint32_t index = 0; index < counts[tier]; ++index)
    {
      // **THE SINE OF THE GALACTIC LATITUDE, PULLED TOWARD THE PLANE.** A uniform sphere wants this
      // uniform on [-1, 1]; raising its magnitude to a power above one concentrates it near zero,
      // which is the band. The sign is kept, so both hemispheres fill.
      const float uniform = NextSigned(random);
      const float magnitude = std::pow(std::fabs(uniform), concentration);
      const float sineLatitude = (uniform < 0.0f) ? -magnitude : magnitude;

      const float cosineLatitude = std::sqrt((1.0f - sineLatitude * sineLatitude) > 0.0f ? (1.0f - sineLatitude * sineLatitude) : 0.0f);
      const float longitude = NextUnit(random) * 2.0f * PI;

      const float alongRight = cosineLatitude * std::cos(longitude);
      const float alongForward = cosineLatitude * std::sin(longitude);

      Star star;
      star.directionX = (right[0] * alongRight) + (forward[0] * alongForward) + (pole[0] * sineLatitude);
      star.directionY = (right[1] * alongRight) + (forward[1] * alongForward) + (pole[1] * sineLatitude);
      star.directionZ = (right[2] * alongRight) + (forward[2] * alongForward) + (pole[2] * sineLatitude);
      star.sizePixels = size;
      star.tier = static_cast<std::uint8_t>(tier);

      const float jitter = NextSigned(random) * _description.temperatureJitterKelvin;
      const Chromaticity color = Desaturate(BlackbodyColor(kelvin + jitter), _description.saturation);

      // **THE VALUE IS FOLDED IN HERE AND NOT IN THE SHADER**, so the sprite multiplies by a radial
      // falloff and nothing else -- and so a suite can add up the lit area without a device.
      star.red = color.red * value;
      star.green = color.green * value;
      star.blue = color.blue * value;

      stars.push_back(star);
    }
  }

  return stars;
}

float LitAreaFraction(const std::vector<Star>& _stars, std::uint32_t _framePixels) noexcept
{
  if (_framePixels == 0)
  {
    return 0.0f;
  }

  // A quarter of the disc, which is roughly what a soft radial falloff integrates to -- see the header
  // for why this is an estimate.
  constexpr float FALLOFF_INTEGRAL = 0.25f;

  double lit = 0.0;
  for (const Star& star : _stars)
  {
    const double radius = static_cast<double>(star.sizePixels) * 0.5;
    const double area = PI * radius * radius * FALLOFF_INTEGRAL;

    // Rec. 709 again, so a red star and a blue one of the same value count the same.
    const double luminance = (0.2126 * star.red) + (0.7152 * star.green) + (0.0722 * star.blue);
    lit += area * luminance;
  }

  return static_cast<float>(lit / static_cast<double>(_framePixels));
}

} // namespace Neuron

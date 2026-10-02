#include "pch.h"
#include "Starfield.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace
{
constexpr float PI = std::numbers::pi_v<float>;

// Presentation, not tuning (design §11). The sky only has to read as deep space behind the battlefield, never compete
// with it: no star is as bright or as saturated as an exhaust, a shot or a team color.

// One seed for the whole sky: the same stars every run.
constexpr std::uint64_t SKY_SEED = 0x5EED'5EED'0F5E'7A25ULL;

// Stars over the whole sky, a few thousand of them on the screen at once, and the brightest of them, which are drawn
// as crosses: a handful on the screen (owner, 2026-10-02, ADR-028).
constexpr int STAR_COUNT = 50000;
constexpr int BURST_COUNT = 25;

// A point's light is its peak color times its Gaussian's area, so a bright point is drawn wider rather than brighter
// than MAX_PEAK. Its square reaches REACH standard deviations, where StarPS.hlsl puts its rim.
constexpr float MIN_SPREAD_PIXELS = 0.7f;
constexpr float MAX_SPREAD_PIXELS = 1.6f;
constexpr float MAX_PEAK = 0.75f;
constexpr float REACH = 3.0f;
// The faintest star's light: a point at the smallest spread with this peak. The count of stars brighter than a light
// falls as the light to the power -3/2, as it does for stars spread evenly through space.
constexpr float MIN_FLUX = 0.018f * MIN_SPREAD_PIXELS * MIN_SPREAD_PIXELS;
constexpr float COUNT_POWER = -1.5f;

// A burst's square, and so its cross's arms, grows from the dimmest burst to the brightest, with the logarithm of its
// light, and its color is the star's tint at this brightness. The cross has its own dot at its center, so a burst is
// never a point as well. Small, so that the crosses are fine marks in the line art rather than flares.
constexpr float MIN_BURST_RADIUS_PIXELS = 6.0f;
constexpr float MAX_BURST_RADIUS_PIXELS = 16.0f;
constexpr float BURST_PEAK = 0.6f;

// Star colors by temperature, as linear colors with their brightest channel at 1. A star keeps this share of its tint
// and is white for the rest, so that no star reads as a team or exhaust color.
constexpr float TINT_SHARE = 0.65f;
struct Tint
{
  DirectX::XMFLOAT3 color;
  // How common the tint is.
  float weight;
};
constexpr std::array<Tint, 7> TINTS{{
  {.color = {1.0f, 0.46f, 0.15f}, .weight = 0.10f}, // 3,000 K
  {.color = {1.0f, 0.64f, 0.37f}, .weight = 0.20f}, // 4,000 K
  {.color = {1.0f, 0.78f, 0.62f}, .weight = 0.22f}, // 5,000 K
  {.color = {1.0f, 0.87f, 0.78f}, .weight = 0.18f}, // 5,800 K
  {.color = {1.0f, 0.95f, 0.98f}, .weight = 0.14f}, // 6,500 K
  {.color = {0.77f, 0.82f, 1.0f}, .weight = 0.10f}, // 8,000 K
  {.color = {0.60f, 0.71f, 1.0f}, .weight = 0.06f}, // 10,000 K
}};

// SplitMix64, written out so that the sky does not hang on a standard library's distributions.
class SkyRandom
{
public:
  explicit SkyRandom(std::uint64_t _seed) noexcept
    : m_state(_seed)
  {
  }

  [[nodiscard]] std::uint64_t Next() noexcept
  {
    m_state += 0x9E37'79B9'7F4A'7C15ULL;
    std::uint64_t mixed = m_state;
    mixed = (mixed ^ (mixed >> 30)) * 0xBF58'476D'1CE4'E5B9ULL;
    mixed = (mixed ^ (mixed >> 27)) * 0x94D0'49BB'1331'11EBULL;
    return mixed ^ (mixed >> 31);
  }

  // A number in [0, 1), from the top 24 bits of one draw.
  [[nodiscard]] float Unit() noexcept
  {
    return static_cast<float>(Next() >> 40) * 0x1.0p-24f;
  }

  // A number in (0, 1], for a power.
  [[nodiscard]] float OpenUnit() noexcept
  {
    return 1.0f - Unit();
  }

private:
  std::uint64_t m_state;
};

// A star before it is drawn: where it is, its light, and its color with the light left out.
struct Candidate
{
  DirectX::XMFLOAT3 direction;
  float flux;
  DirectX::XMFLOAT3 tint;
};

Candidate MakeCandidate(SkyRandom& _random) noexcept
{
  // Even over the sphere: the height is even, and so is the angle round.
  const float height = (2.0f * _random.Unit()) - 1.0f;
  const float around = 2.0f * PI * _random.Unit();
  const float across = std::sqrt(std::max(0.0f, 1.0f - (height * height)));
  DirectX::XMFLOAT3 direction;
  DirectX::XMStoreFloat3(
    &direction, DirectX::XMVector3Normalize(DirectX::XMVectorSet(across * std::cos(around), height, across * std::sin(around), 0.0f)));

  float pick = _random.Unit();
  const Tint* tint = &TINTS.back();
  for (const Tint& candidate : TINTS)
  {
    if (pick < candidate.weight)
    {
      tint = &candidate;
      break;
    }
    pick -= candidate.weight;
  }
  const auto channel = [](float _tinted) { return std::lerp(1.0f, _tinted, TINT_SHARE); };
  const float flux = MIN_FLUX * std::pow(_random.OpenUnit(), 1.0f / COUNT_POWER);
  return {.direction = direction, .flux = flux, .tint = {channel(tint->color.x), channel(tint->color.y), channel(tint->color.z)}};
}

DirectX::XMFLOAT3 Scaled(const DirectX::XMFLOAT3& _color, float _scale) noexcept
{
  return {_color.x * _scale, _color.y * _scale, _color.z * _scale};
}
} // namespace

Outpost::Starfield Outpost::BuildStarfield()
{
  SkyRandom random(SKY_SEED);
  std::vector<Candidate> candidates;
  candidates.reserve(STAR_COUNT);
  for (int index = 0; index < STAR_COUNT; ++index)
    candidates.push_back(MakeCandidate(random));
  // Brightest first. A stable sort keeps the sky the same whatever the standard library.
  std::ranges::stable_sort(candidates, std::ranges::greater{}, &Candidate::flux);

  Starfield sky;
  const float dimmestBurst = std::log(candidates[static_cast<size_t>(BURST_COUNT) - 1].flux);
  const float brightestBurst = std::log(candidates.front().flux);
  for (int index = 0; index < BURST_COUNT; ++index)
  {
    const Candidate& star = candidates[static_cast<size_t>(index)];
    const float share = brightestBurst > dimmestBurst ? (std::log(star.flux) - dimmestBurst) / (brightestBurst - dimmestBurst) : 1.0f;
    sky.bursts.push_back({.direction = star.direction,
                          .radiusPixels = std::lerp(MIN_BURST_RADIUS_PIXELS, MAX_BURST_RADIUS_PIXELS, share),
                          .color = Scaled(star.tint, BURST_PEAK)});
  }

  sky.points.reserve(static_cast<size_t>(STAR_COUNT) - static_cast<size_t>(BURST_COUNT));
  for (size_t index = BURST_COUNT; index < candidates.size(); ++index)
  {
    const Candidate& star = candidates[index];
    const float spread = std::clamp(std::sqrt(star.flux / MAX_PEAK), MIN_SPREAD_PIXELS, MAX_SPREAD_PIXELS);
    const float peak = std::min(star.flux / (spread * spread), MAX_PEAK);
    sky.points.push_back({.direction = star.direction, .radiusPixels = REACH * spread, .color = Scaled(star.tint, peak)});
  }
  return sky;
}

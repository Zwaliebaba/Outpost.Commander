#include "pch.h"
#include "Starfield.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace
{
using Star = Neuron::StarPipeline::Star;

constexpr float PI = std::numbers::pi_v<float>;
constexpr float DEGREES = PI / 180.0f;

// Presentation, not tuning (design §11). The sky only has to read as deep space behind the battlefield, never compete
// with it: no star is as bright or as saturated as an exhaust, a shot or a team color.

// The galaxy's center sits right of and a little below the middle of the default view, which looks along +z and 47
// degrees down (ADR-012), and the band runs from it up to the left. The camera only ever sees the lower half of the
// sky, so turning it with Q and E takes the band out of view for part of the turn (ADR-021).
constexpr DirectX::XMFLOAT3 GALACTIC_CENTER{0.28f, -0.79f, 0.55f};
// A second point on the band, which with the center fixes its plane.
constexpr DirectX::XMFLOAT3 BAND_THROUGH{-0.80f, -0.30f, 0.50f};

// One seed for the whole sky: the same stars every run.
constexpr std::uint64_t SKY_SEED = 0x5EED'5EED'0F5E'7A25ULL;

// Stars scattered over the whole sky, in every direction alike.
constexpr int FIELD_STARS = 14000;
// Candidates for the band: those that fall in a dust lane or between clouds are dropped, so fewer are drawn.
constexpr int DISK_CANDIDATES = 260000;
// The bulge round the center.
constexpr int BULGE_CANDIDATES = 80000;

// The band's stars crowd toward the center along it: this share is spread evenly round the whole circle and the rest
// falls off from the center with this standard deviation.
constexpr float DISK_EVEN_SHARE = 0.55f;
constexpr float DISK_LONGITUDE_SPREAD_RADIANS = 45.0f * DEGREES;
// How far the band's stars lie off its plane: a thin disk, thicker toward the center, and a thick one.
constexpr float THIN_DISK_LATITUDE_RADIANS = 1.8f * DEGREES;
constexpr float THIN_DISK_FLARE_RADIANS = 2.2f * DEGREES;
constexpr float FLARE_LONGITUDE_RADIANS = 25.0f * DEGREES;
constexpr float THICK_DISK_SHARE = 0.12f;
constexpr float THICK_DISK_LATITUDE_RADIANS = 5.0f * DEGREES;
// The bulge is wider along the band than across it.
constexpr float BULGE_LONGITUDE_RADIANS = 6.0f * DEGREES;
constexpr float BULGE_LATITUDE_RADIANS = 4.0f * DEGREES;

// Star clouds: a band star is kept with a chance between CLOUD_FLOOR and 1, following a noise over the sky, so the band
// is clumpy rather than even. The noise counts as no cloud below its low level and as full cloud above its high one.
constexpr float CLOUD_FLOOR = 0.15f;
constexpr float CLOUD_FREQUENCY = 9.0f;
constexpr float CLOUD_LOW = 0.35f;
constexpr float CLOUD_HIGH = 0.65f;
// Dust: where it is thickest, this share of the band's stars is dropped. A rift runs along the plane, strongest toward
// the center and wandering off the plane by up to DUST_WANDER_RADIANS, broken into lengths by noise.
constexpr float DUST_DEPTH = 0.97f;
constexpr float DUST_WIDTH_RADIANS = 1.6f * DEGREES;
constexpr float DUST_WANDER_RADIANS = 1.2f * DEGREES;
constexpr float DUST_REACH_RADIANS = 70.0f * DEGREES;
constexpr float RIFT_FREQUENCY = 5.0f;
constexpr float RIFT_LOW = 0.35f;
constexpr float RIFT_HIGH = 0.55f;
constexpr float RIFT_NEGLIGIBLE = 0.01f;
// Filaments of dust across the whole band, where a finer noise is high, less dark than the rift.
constexpr float FILAMENT_DEPTH = 0.8f;
constexpr float FILAMENT_FREQUENCY = 14.0f;
constexpr float FILAMENT_LOW = 0.55f;
constexpr float FILAMENT_HIGH = 0.7f;
constexpr int NOISE_OCTAVES = 3;

// A star's light is its peak color times its Gaussian's area, so a bright star is drawn wider rather than brighter
// than MAX_PEAK.
constexpr float MIN_SPREAD_PIXELS = 0.7f;
constexpr float MAX_SPREAD_PIXELS = 1.6f;
constexpr float MAX_PEAK = 0.75f;

// What sets a kind of star apart: the light of its faintest, the power of the light that the count of stars brighter
// than it falls as, and whether it is old, which makes it redder.
struct Population
{
  float minimumFlux;
  float countPower;
  bool old;
};
// The field's count falls as the light to the power -3/2, as for stars spread evenly through space. The band and the
// bulge are as far as the galaxy, so their stars are fainter and few of them bright.
constexpr Population FIELD_POPULATION{.minimumFlux = 0.030f * MIN_SPREAD_PIXELS * MIN_SPREAD_PIXELS, .countPower = -1.5f, .old = false};
constexpr Population BAND_POPULATION{.minimumFlux = 0.020f * MIN_SPREAD_PIXELS * MIN_SPREAD_PIXELS, .countPower = -2.0f, .old = false};
constexpr Population BULGE_POPULATION{.minimumFlux = 0.022f * MIN_SPREAD_PIXELS * MIN_SPREAD_PIXELS, .countPower = -2.0f, .old = true};

// Star colors by temperature, as linear colors with their brightest channel at 1. A star keeps this share of its tint
// and is white for the rest, so that no star reads as a team or exhaust color.
constexpr float TINT_SHARE = 0.65f;
struct Tint
{
  DirectX::XMFLOAT3 color;
  // How common the tint is among the field and band stars, and among the bulge's older stars.
  float fieldWeight;
  float bulgeWeight;
};
constexpr std::array<Tint, 7> TINTS{{
  {.color = {1.0f, 0.46f, 0.15f}, .fieldWeight = 0.10f, .bulgeWeight = 0.30f}, // 3,000 K
  {.color = {1.0f, 0.64f, 0.37f}, .fieldWeight = 0.20f, .bulgeWeight = 0.38f}, // 4,000 K
  {.color = {1.0f, 0.78f, 0.62f}, .fieldWeight = 0.22f, .bulgeWeight = 0.22f}, // 5,000 K
  {.color = {1.0f, 0.87f, 0.78f}, .fieldWeight = 0.18f, .bulgeWeight = 0.08f}, // 5,800 K
  {.color = {1.0f, 0.95f, 0.98f}, .fieldWeight = 0.14f, .bulgeWeight = 0.02f}, // 6,500 K
  {.color = {0.77f, 0.82f, 1.0f}, .fieldWeight = 0.10f, .bulgeWeight = 0.0f},  // 8,000 K
  {.color = {0.60f, 0.71f, 1.0f}, .fieldWeight = 0.06f, .bulgeWeight = 0.0f},  // 10,000 K
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

  // A number in (0, 1], for a logarithm or a power.
  [[nodiscard]] float OpenUnit() noexcept
  {
    return 1.0f - Unit();
  }

  // A standard normal number, by Box and Muller.
  [[nodiscard]] float Normal() noexcept
  {
    const float radius = std::sqrt(-2.0f * std::log(OpenUnit()));
    return radius * std::cos(2.0f * PI * Unit());
  }

private:
  std::uint64_t m_state;
};

DirectX::XMFLOAT3 Normalized(const DirectX::XMFLOAT3& _vector) noexcept
{
  DirectX::XMFLOAT3 result;
  DirectX::XMStoreFloat3(&result, DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&_vector)));
  return result;
}

// A number in [0, 1) for a point of a lattice, the same for the same point every time.
float LatticeValue(std::int32_t _x, std::int32_t _y, std::int32_t _z, std::uint64_t _salt) noexcept
{
  std::uint64_t hash = _salt;
  hash ^= static_cast<std::uint64_t>(static_cast<std::uint32_t>(_x)) * 0x9E37'79B9'7F4A'7C15ULL;
  hash ^= static_cast<std::uint64_t>(static_cast<std::uint32_t>(_y)) * 0xC2B2'AE3D'27D4'EB4FULL;
  hash ^= static_cast<std::uint64_t>(static_cast<std::uint32_t>(_z)) * 0x1656'67B1'9E37'79F9ULL;
  return SkyRandom(hash).Unit();
}

float Smooth(float _share) noexcept
{
  return _share * _share * (3.0f - (2.0f * _share));
}

// Smooth noise in [0, 1) over space: lattice values blended across each cell.
float ValueNoise(const DirectX::XMFLOAT3& _point, std::uint64_t _salt) noexcept
{
  const float floorX = std::floor(_point.x);
  const float floorY = std::floor(_point.y);
  const float floorZ = std::floor(_point.z);
  const auto x = static_cast<std::int32_t>(floorX);
  const auto y = static_cast<std::int32_t>(floorY);
  const auto z = static_cast<std::int32_t>(floorZ);
  const float alongX = Smooth(_point.x - floorX);
  const float alongY = Smooth(_point.y - floorY);
  const float alongZ = Smooth(_point.z - floorZ);
  const auto edge = [&](std::int32_t _dy, std::int32_t _dz)
  { return std::lerp(LatticeValue(x, y + _dy, z + _dz, _salt), LatticeValue(x + 1, y + _dy, z + _dz, _salt), alongX); };
  const float front = std::lerp(edge(0, 0), edge(1, 0), alongY);
  const float back = std::lerp(edge(0, 1), edge(1, 1), alongY);
  return std::lerp(front, back, alongZ);
}

// Noise over the sky, in [0, 1): octaves of ValueNoise at a direction, each twice as fine and half as strong.
float SkyNoise(const DirectX::XMFLOAT3& _direction, float _frequency, std::uint64_t _salt) noexcept
{
  float sum = 0.0f;
  float weight = 1.0f;
  float total = 0.0f;
  float frequency = _frequency;
  for (int octave = 0; octave < NOISE_OCTAVES; ++octave)
  {
    const DirectX::XMFLOAT3 point{_direction.x * frequency, _direction.y * frequency, _direction.z * frequency};
    sum += weight * ValueNoise(point, _salt + static_cast<std::uint64_t>(octave));
    total += weight;
    weight *= 0.5f;
    frequency *= 2.0f;
  }
  return sum / total;
}

// The galaxy's axes in the world: toward its center, along its plane a quarter turn from the center, and its pole.
struct GalacticAxes
{
  DirectX::XMVECTOR center;
  DirectX::XMVECTOR along;
  DirectX::XMVECTOR pole;
};

GalacticAxes Axes() noexcept
{
  const Outpost::GalacticFrame frame = Outpost::StarfieldGalaxy();
  const DirectX::XMVECTOR center = DirectX::XMLoadFloat3(&frame.center);
  const DirectX::XMVECTOR pole = DirectX::XMLoadFloat3(&frame.pole);
  return {.center = center, .along = DirectX::XMVector3Cross(pole, center), .pole = pole};
}

// The direction at a galactic longitude, counted along the band from its center, and latitude, off its plane.
DirectX::XMFLOAT3 GalacticDirection(const GalacticAxes& _axes, float _longitudeRadians, float _latitudeRadians) noexcept
{
  const float across = std::cos(_latitudeRadians);
  const DirectX::XMVECTOR direction =
    DirectX::XMVectorAdd(DirectX::XMVectorAdd(DirectX::XMVectorScale(_axes.center, across * std::cos(_longitudeRadians)),
                                              DirectX::XMVectorScale(_axes.along, across * std::sin(_longitudeRadians))),
                         DirectX::XMVectorScale(_axes.pole, std::sin(_latitudeRadians)));
  DirectX::XMFLOAT3 result;
  DirectX::XMStoreFloat3(&result, DirectX::XMVector3Normalize(direction));
  return result;
}

// An angle brought into [-pi, pi].
float Wrapped(float _radians) noexcept
{
  return std::remainder(_radians, 2.0f * PI);
}

// Noise brought from between two levels up to 0 to 1, with smooth ends: below _low is 0 and above _high is 1.
float Contrast(float _noise, float _low, float _high) noexcept
{
  return Smooth(std::clamp((_noise - _low) / (_high - _low), 0.0f, 1.0f));
}

// The share of the band's stars the dust leaves at a point of the band: a rift along the plane toward the center, and
// filaments across the rest of the band.
float DustTransmission(const DirectX::XMFLOAT3& _direction, float _longitudeRadians, float _latitudeRadians) noexcept
{
  const float wander = DUST_WANDER_RADIANS * std::sin(3.0f * _longitudeRadians) * std::cos(1.7f * _longitudeRadians);
  const float offPlane = (_latitudeRadians - wander) / DUST_WIDTH_RADIANS;
  const float alongBand = _longitudeRadians / DUST_REACH_RADIANS;
  // Away from the rift its noise cannot matter, and the noise is most of what building the sky costs.
  const float riftReach = std::exp(-(offPlane * offPlane) - (alongBand * alongBand));
  const float rift =
    riftReach < RIFT_NEGLIGIBLE ? 0.0f : riftReach * Contrast(SkyNoise(_direction, RIFT_FREQUENCY, SKY_SEED + 100), RIFT_LOW, RIFT_HIGH);
  const float filaments = FILAMENT_DEPTH * Contrast(SkyNoise(_direction, FILAMENT_FREQUENCY, SKY_SEED + 300), FILAMENT_LOW, FILAMENT_HIGH);
  return 1.0f - (DUST_DEPTH * std::max(rift, filaments));
}

// The chance a band star is kept between clouds, before the dust: CLOUD_FLOOR there, and 1 in a cloud.
float CloudKeepChance(const DirectX::XMFLOAT3& _direction) noexcept
{
  const float clouds = Contrast(SkyNoise(_direction, CLOUD_FREQUENCY, SKY_SEED + 200), CLOUD_LOW, CLOUD_HIGH);
  return CLOUD_FLOOR + ((1.0f - CLOUD_FLOOR) * clouds);
}

Star MakeStar(SkyRandom& _random, const DirectX::XMFLOAT3& _direction, const Population& _population) noexcept
{
  float pick = _random.Unit();
  const Tint* tint = &TINTS.back();
  for (const Tint& candidate : TINTS)
  {
    const float weight = _population.old ? candidate.bulgeWeight : candidate.fieldWeight;
    if (pick < weight)
    {
      tint = &candidate;
      break;
    }
    pick -= weight;
  }

  const float flux = _population.minimumFlux * std::pow(_random.OpenUnit(), 1.0f / _population.countPower);
  const float spread = std::clamp(std::sqrt(flux / MAX_PEAK), MIN_SPREAD_PIXELS, MAX_SPREAD_PIXELS);
  const float peak = std::min(flux / (spread * spread), MAX_PEAK);
  const auto channel = [peak](float _tinted) { return peak * std::lerp(1.0f, _tinted, TINT_SHARE); };
  return {
    .direction = _direction, .spreadPixels = spread, .color = {channel(tint->color.x), channel(tint->color.y), channel(tint->color.z)}};
}
} // namespace

Outpost::GalacticFrame Outpost::StarfieldGalaxy() noexcept
{
  const DirectX::XMVECTOR center = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&GALACTIC_CENTER));
  const DirectX::XMVECTOR pole = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(center, DirectX::XMLoadFloat3(&BAND_THROUGH)));
  GalacticFrame frame{};
  DirectX::XMStoreFloat3(&frame.center, center);
  DirectX::XMStoreFloat3(&frame.pole, pole);
  return frame;
}

std::vector<Neuron::StarPipeline::Star> Outpost::BuildStarfield()
{
  SkyRandom random(SKY_SEED);
  const GalacticAxes axes = Axes();
  std::vector<Star> stars;
  stars.reserve(static_cast<size_t>(FIELD_STARS + DISK_CANDIDATES + BULGE_CANDIDATES));

  for (int index = 0; index < FIELD_STARS; ++index)
  {
    // Even over the sphere: the height is even, and so is the angle round.
    const float height = (2.0f * random.Unit()) - 1.0f;
    const float around = 2.0f * PI * random.Unit();
    const float across = std::sqrt(std::max(0.0f, 1.0f - (height * height)));
    const DirectX::XMFLOAT3 direction = Normalized({across * std::cos(around), height, across * std::sin(around)});
    stars.push_back(MakeStar(random, direction, FIELD_POPULATION));
  }

  for (int index = 0; index < DISK_CANDIDATES; ++index)
  {
    const float longitude =
      random.Unit() < DISK_EVEN_SHARE ? PI * ((2.0f * random.Unit()) - 1.0f) : Wrapped(DISK_LONGITUDE_SPREAD_RADIANS * random.Normal());
    const float flare = longitude / FLARE_LONGITUDE_RADIANS;
    const float thickness = random.Unit() < THICK_DISK_SHARE
                              ? THICK_DISK_LATITUDE_RADIANS
                              : THIN_DISK_LATITUDE_RADIANS + (THIN_DISK_FLARE_RADIANS * std::exp(-0.5f * flare * flare));
    const float latitude = std::clamp(thickness * random.Normal(), -PI / 2.0f, PI / 2.0f);
    const DirectX::XMFLOAT3 direction = GalacticDirection(axes, longitude, latitude);
    // A star is kept with the chance the clouds and the dust give together. Most candidates fall between clouds, and
    // those need no dust.
    const float draw = random.Unit();
    const float clouds = CloudKeepChance(direction);
    if (draw < clouds && draw < clouds * DustTransmission(direction, longitude, latitude))
      stars.push_back(MakeStar(random, direction, BAND_POPULATION));
  }

  for (int index = 0; index < BULGE_CANDIDATES; ++index)
  {
    const float longitude = Wrapped(BULGE_LONGITUDE_RADIANS * random.Normal());
    const float latitude = std::clamp(BULGE_LATITUDE_RADIANS * random.Normal(), -PI / 2.0f, PI / 2.0f);
    const DirectX::XMFLOAT3 direction = GalacticDirection(axes, longitude, latitude);
    if (random.Unit() < DustTransmission(direction, longitude, latitude))
      stars.push_back(MakeStar(random, direction, BULGE_POPULATION));
  }
  return stars;
}

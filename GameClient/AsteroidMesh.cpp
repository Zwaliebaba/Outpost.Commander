#include "pch.h"

#include "AsteroidMesh.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace Outpost
{

namespace
{
constexpr std::array<std::string_view, ASTEROID_VARIANT_COUNT> VARIANT_NAMES{"AsteroidA", "AsteroidB", "AsteroidC", "AsteroidD",
                                                                             "AsteroidE"};

/// A binary angle to radians. Visual only, so a float is correct here and would be a defect one layer down.
[[nodiscard]] float Radians(Neuron::Angle _angle) noexcept
{
  constexpr float FULL_TURN_RADIANS = 6.28318530717958647692f;
  return static_cast<float>(_angle) * (FULL_TURN_RADIANS / 65536.0f);
}

[[nodiscard]] float Larger(float _a, float _b) noexcept
{
  return (std::fabs(_a) > std::fabs(_b)) ? std::fabs(_a) : std::fabs(_b);
}

/// Roll about X, then pitch about Y, then yaw about Z, applied to one direction.
void Turn(const RockLook& _look, float& _x, float& _y, float& _z) noexcept
{
  const float rollCosine = std::cos(Radians(_look.roll));
  const float rollSine = std::sin(Radians(_look.roll));
  const float pitchCosine = std::cos(Radians(_look.pitch));
  const float pitchSine = std::sin(Radians(_look.pitch));
  const float yawCosine = std::cos(Radians(_look.yaw));
  const float yawSine = std::sin(Radians(_look.yaw));

  // About X.
  const float y1 = (_y * rollCosine) - (_z * rollSine);
  const float z1 = (_y * rollSine) + (_z * rollCosine);
  // About Y.
  const float x2 = (_x * pitchCosine) + (z1 * pitchSine);
  const float z2 = (-_x * pitchSine) + (z1 * pitchCosine);
  // About Z.
  _x = (x2 * yawCosine) - (y1 * yawSine);
  _y = (x2 * yawSine) + (y1 * yawCosine);
  _z = z2;
}
} // namespace

std::span<const std::string_view> AsteroidVariantNames() noexcept
{
  return VARIANT_NAMES;
}

float BoundingRadiusUnits(const MeshEntry& _entry) noexcept
{
  const float x = Larger(_entry.minX, _entry.maxX);
  const float y = Larger(_entry.minY, _entry.maxY);
  const float z = Larger(_entry.minZ, _entry.maxZ);
  return std::sqrt((x * x) + (y * y) + (z * z));
}

float BoundingRadiusUnits(const HullMesh& _mesh) noexcept
{
  float farthestSquared = 0.0f;
  for (const HullVertex& vertex : _mesh.vertices)
  {
    farthestSquared = std::max(farthestSquared, (vertex.x * vertex.x) + (vertex.y * vertex.y) + (vertex.z * vertex.z));
  }
  return std::sqrt(farthestSquared);
}

std::vector<RockLook> RockLooks(std::uint64_t _matchSeed, std::span<const Placement> _rocks,
                                std::span<const float, ASTEROID_VARIANT_COUNT> _radiusUnits)
{
  Neuron::Pcg32 random{_matchSeed, ROCK_LOOK_STREAM};

  std::vector<RockLook> looks;
  looks.reserve(_rocks.size());
  for (std::size_t index = 0; index < _rocks.size(); ++index)
  {
    // SIX DRAWS A ROCK, ALWAYS, IN THIS ORDER -- the clamp below reads no draw, so a rock that is clamped
    // does not shift every look after it.
    RockLook look;
    look.variant = static_cast<std::uint8_t>(random.NextBelow(static_cast<std::uint32_t>(ASTEROID_VARIANT_COUNT)));
    look.yaw = static_cast<Neuron::Angle>(random.NextBelow(65536u));
    look.pitch = static_cast<Neuron::Angle>(random.NextBelow(65536u));
    look.roll = static_cast<Neuron::Angle>(random.NextBelow(65536u));
    look.scalePercent = random.NextInRange(ROCK_SCALE_LOWEST_PERCENT, ROCK_SCALE_HIGHEST_PERCENT);
    look.liftUnits = random.NextInRange(-ROCK_LIFT_UNITS, ROCK_LIFT_UNITS);

    // The nearest neighbor, on the plane, in whole units. Squared in 64 bits and rooted once.
    std::int64_t nearestSquared = std::numeric_limits<std::int64_t>::max();
    const std::int64_t x = _rocks[index].position.x / Neuron::FIXED_ONE;
    const std::int64_t y = _rocks[index].position.y / Neuron::FIXED_ONE;
    for (std::size_t other = 0; other < _rocks.size(); ++other)
    {
      if (other == index)
      {
        continue;
      }
      const std::int64_t dx = (_rocks[other].position.x / Neuron::FIXED_ONE) - x;
      const std::int64_t dy = (_rocks[other].position.y / Neuron::FIXED_ONE) - y;
      nearestSquared = std::min(nearestSquared, (dx * dx) + (dy * dy));
    }

    look.scale = static_cast<float>(look.scalePercent) / 100.0f;
    const float radius = _radiusUnits[look.variant];
    if ((_rocks.size() > 1) && (radius > 0.0f))
    {
      const float fits = (0.5f * std::sqrt(static_cast<float>(nearestSquared))) / radius;
      look.scale = std::min(look.scale, fits);
    }
    looks.push_back(look);
  }
  return looks;
}

HullVertex PlaceRockVertex(const HullVertex& _vertex, const Placement& _rock, const RockLook& _look) noexcept
{
  HullVertex out = _vertex;

  Turn(_look, out.x, out.y, out.z);
  Turn(_look, out.normalX, out.normalY, out.normalZ);

  out.x = (out.x * _look.scale) + (static_cast<float>(_rock.position.x) / static_cast<float>(Neuron::FIXED_ONE));
  out.y = (out.y * _look.scale) + (static_cast<float>(_rock.position.y) / static_cast<float>(Neuron::FIXED_ONE));
  out.z = (out.z * _look.scale) + static_cast<float>(_look.liftUnits);
  return out;
}

std::vector<RockPickPoint> RockPickPoints(std::span<const Placement> _rocks, std::span<const RockLook> _looks)
{
  std::vector<RockPickPoint> points;
  const std::size_t count = std::min(_rocks.size(), _looks.size());
  points.reserve(count);
  for (std::size_t index = 0; index < count; ++index)
  {
    points.push_back(RockPickPoint{.worldX = static_cast<float>(_rocks[index].position.x) / static_cast<float>(Neuron::FIXED_ONE),
                                   .worldY = static_cast<float>(_rocks[index].position.y) / static_cast<float>(Neuron::FIXED_ONE),
                                   .liftUnits = static_cast<float>(_looks[index].liftUnits),
                                   .rock = static_cast<std::uint16_t>(index)});
  }
  return points;
}

bool BuildVariantField(const HullMesh& _variantMesh, std::uint8_t _variant, std::span<const Placement> _rocks,
                       std::span<const RockLook> _looks, HullMesh& _outMesh)
{
  _outMesh = HullMesh{};
  _outMesh.longestUnits = _variantMesh.longestUnits;

  const std::size_t rockCount = std::min(_rocks.size(), _looks.size());
  for (std::size_t index = 0; index < rockCount; ++index)
  {
    if (_looks[index].variant != _variant)
    {
      continue;
    }

    const std::size_t base = _outMesh.vertices.size();
    if ((base + _variantMesh.vertices.size()) > (static_cast<std::size_t>(std::numeric_limits<std::uint16_t>::max()) + 1))
    {
      _outMesh = HullMesh{};
      return false;
    }

    for (const HullVertex& vertex : _variantMesh.vertices)
    {
      _outMesh.vertices.push_back(PlaceRockVertex(vertex, _rocks[index], _looks[index]));
    }
    for (const std::uint16_t vertexIndex : _variantMesh.indices)
    {
      _outMesh.indices.push_back(static_cast<std::uint16_t>(base + vertexIndex));
    }
  }
  return true;
}

void OmitSpentRocks(std::span<const Placement> _rocks, std::span<const RockLook> _looks, std::span<const std::uint8_t> _spent,
                    std::vector<Placement>& _outRocks, std::vector<RockLook>& _outLooks)
{
  _outRocks.clear();
  _outLooks.clear();
  for (std::size_t rock = 0; (rock < _rocks.size()) && (rock < _looks.size()); ++rock)
  {
    if (!IsRockSpent(_spent, rock))
    {
      _outRocks.push_back(_rocks[rock]);
      _outLooks.push_back(_looks[rock]);
    }
  }
}

} // namespace Outpost

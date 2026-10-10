#include "pch.h"
#include "FieldLayout.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace
{
// Mixed into a field's identifier, so that its layout draws apart from any other effect seeded with the same number.
constexpr std::uint64_t FIELD_SEED_SALT = 0xF1E1'D0F0'0C45'7E5Dull;
constexpr float FULL_TURN_RADIANS = 2.0f * std::numbers::pi_v<float>;

// The middle rock: its radius and how far off the center it stands, in shares of the field's radius.
constexpr float CENTER_LEAST_SHARE = 0.36f;
constexpr float CENTER_MOST_SHARE = 0.46f;
constexpr float CENTER_OFFSET_SHARE = 0.06f;
// The ring: how many rocks, each a share of the way round off its even place, at a distance and of a radius in shares
// of the field's, so that the farthest and largest just reaches the field's edge.
constexpr std::uint64_t RING_LEAST_ROCKS = 5;
constexpr std::uint64_t RING_ROCK_CHOICES = 4;
constexpr float RING_JITTER_SHARE = 0.2f;
constexpr float RING_NEAREST_SHARE = 0.56f;
constexpr float RING_FARTHEST_SHARE = 0.68f;
constexpr float RING_LEAST_SHARE = 0.2f;
constexpr float RING_MOST_SHARE = 0.32f;
// The rocks that fill a gap in the ring, smaller than the ring's.
constexpr float FILLER_LEAST_SHARE = 0.08f;
constexpr float FILLER_MOST_SHARE = 0.12f;
// The directions NarrowestReach measures in, a degree apart.
constexpr int REACH_DIRECTIONS = 360;
} // namespace

Outpost::FieldLayout Outpost::FieldLayout::Of(EntityId _field, float _radiusMeters, float _reachShare, float _gapMeters)
{
  EffectRandom random(FIELD_SEED_SALT ^ _field.value);
  FieldLayout layout;
  layout.center = {.xMeters = random.Signed(CENTER_OFFSET_SHARE * _radiusMeters),
                   .zMeters = random.Signed(CENTER_OFFSET_SHARE * _radiusMeters),
                   .radiusMeters = _radiusMeters * random.Between(CENTER_LEAST_SHARE, CENTER_MOST_SHARE),
                   .turnRadians = random.Between(0.0f, FULL_TURN_RADIANS)};

  const std::uint64_t rocks = RING_LEAST_ROCKS + (random.Next() % RING_ROCK_CHOICES);
  const float step = FULL_TURN_RADIANS / static_cast<float>(rocks);
  const float start = random.Between(0.0f, FULL_TURN_RADIANS);
  std::vector<FieldRock> ring;
  ring.reserve(rocks);
  for (std::uint64_t i = 0; i < rocks; ++i)
  {
    const float angle = start + (static_cast<float>(i) * step) + random.Signed(RING_JITTER_SHARE * step);
    const float distance = _radiusMeters * random.Between(RING_NEAREST_SHARE, RING_FARTHEST_SHARE);
    const float radius = std::min(_radiusMeters * random.Between(RING_LEAST_SHARE, RING_MOST_SHARE), _radiusMeters - distance);
    ring.push_back({.xMeters = distance * std::cos(angle),
                    .zMeters = distance * std::sin(angle),
                    .radiusMeters = radius,
                    .turnRadians = random.Between(0.0f, FULL_TURN_RADIANS)});
  }

  // Each rock, then the fillers between it and the next round the ring: as few as leave no gap wider than _gapMeters,
  // spaced evenly along the line between the two, each gap measured between the rocks' least reaches.
  for (std::size_t i = 0; i < ring.size(); ++i)
  {
    const FieldRock& from = ring[i];
    const FieldRock& to = ring[(i + 1) % ring.size()];
    layout.ring.push_back(from);
    const float dx = to.xMeters - from.xMeters;
    const float dz = to.zMeters - from.zMeters;
    const float distance = std::hypot(dx, dz);
    const float open = distance - (_reachShare * (from.radiusMeters + to.radiusMeters));
    if (open <= _gapMeters || distance <= 0.0f)
      continue;
    const float filler = _radiusMeters * random.Between(FILLER_LEAST_SHARE, FILLER_MOST_SHARE);
    const float blocks = 2.0f * _reachShare * filler;
    const auto count = static_cast<int>(std::ceil((open - _gapMeters) / (blocks + _gapMeters)));
    const float gap = (open - (static_cast<float>(count) * blocks)) / static_cast<float>(count + 1);
    for (int k = 0; k < count; ++k)
    {
      const float along = (_reachShare * from.radiusMeters) + gap + (_reachShare * filler) + (static_cast<float>(k) * (blocks + gap));
      layout.ring.push_back({.xMeters = from.xMeters + (dx * along / distance),
                             .zMeters = from.zMeters + (dz * along / distance),
                             .radiusMeters = filler,
                             .turnRadians = random.Between(0.0f, FULL_TURN_RADIANS)});
    }
  }
  return layout;
}

float Outpost::FieldLayout::NarrowestReach(std::span<const Neuron::MeshVertex> _vertices) noexcept
{
  float narrowest = std::numeric_limits<float>::max();
  for (int i = 0; i < REACH_DIRECTIONS; ++i)
  {
    const float angle = static_cast<float>(i) * FULL_TURN_RADIANS / static_cast<float>(REACH_DIRECTIONS);
    const float x = std::cos(angle);
    const float z = std::sin(angle);
    float reach = 0.0f;
    for (const Neuron::MeshVertex& vertex : _vertices)
      reach = std::max(reach, (vertex.position.x * x) + (vertex.position.z * z));
    narrowest = std::min(narrowest, reach);
  }
  return _vertices.empty() ? 0.0f : narrowest;
}

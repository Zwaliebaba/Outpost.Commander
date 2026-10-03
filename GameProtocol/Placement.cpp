#include "pch.h"
#include "Placement.h"

#include <algorithm>
#include <cmath>

namespace
{
float Distance(Outpost::PlanePosition _a, Outpost::PlanePosition _b) noexcept
{
  return std::hypot(_a.xMeters - _b.xMeters, _a.zMeters - _b.zMeters);
}
} // namespace

Outpost::GhostPlacement Outpost::PlaceGhost(const StructureTypeView& _type, PlanePosition _cursor, std::span<const EntityView> _entities,
                                            float _mapSizeMeters)
{
  if (_type.structure == StructureKind::MiningRig)
  {
    // The nearest ore asteroid whose edge is within reach of the cursor, free unless a rig already stands on it.
    const EntityView* nearest = nullptr;
    float nearestGap = RIG_SNAP_METERS;
    for (const EntityView& asteroid : _entities)
    {
      if (asteroid.kind != EntityKind::Asteroid)
        continue;
      const float gap = Distance(asteroid.position, _cursor) - asteroid.radiusMeters;
      if (gap <= nearestGap)
      {
        nearest = &asteroid;
        nearestGap = gap;
      }
    }
    if (nearest == nullptr)
      return {.position = _cursor, .radiusMeters = _type.radiusMeters, .valid = false};
    const bool taken = std::ranges::any_of(_entities,
                                           [nearest](const EntityView& _entity)
                                           {
                                             return _entity.kind == EntityKind::Structure &&
                                                    _entity.structure == StructureKind::MiningRig &&
                                                    Distance(_entity.position, nearest->position) < 1.0f;
                                           });
    return {.position = nearest->position, .radiusMeters = std::max(_type.radiusMeters, nearest->radiusMeters), .valid = !taken};
  }

  const float half = _mapSizeMeters / 2.0f;
  const bool inside = std::abs(_cursor.xMeters) + _type.radiusMeters <= half && std::abs(_cursor.zMeters) + _type.radiusMeters <= half;
  const auto overlaps = [&](const EntityView& _other)
  {
    const bool blocks =
      _other.kind == EntityKind::Asteroid || _other.kind == EntityKind::AsteroidField || _other.kind == EntityKind::Structure;
    return blocks && Distance(_other.position, _cursor) < _other.radiusMeters + _type.radiusMeters;
  };
  // A ghost outside the map, or one overlap, makes it invalid: the search stops at the first overlap, and outside the map
  // it is not made at all.
  return {.position = _cursor, .radiusMeters = _type.radiusMeters, .valid = inside && std::ranges::none_of(_entities, overlaps)};
}
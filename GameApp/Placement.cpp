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
  bool valid = std::abs(_cursor.xMeters) + _type.radiusMeters <= half && std::abs(_cursor.zMeters) + _type.radiusMeters <= half;
  for (const EntityView& other : _entities)
  {
    const bool blocks =
      other.kind == EntityKind::Asteroid || other.kind == EntityKind::AsteroidField || other.kind == EntityKind::Structure;
    if (blocks && Distance(other.position, _cursor) < other.radiusMeters + _type.radiusMeters)
      valid = false;
  }
  return {.position = _cursor, .radiusMeters = _type.radiusMeters, .valid = valid};
}
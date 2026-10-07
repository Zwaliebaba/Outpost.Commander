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
                                            float _mapSizeMeters, std::span<const SectorView> _sectors, PlayerId _player,
                                            std::int32_t _nodeCap)
{
  if (_type.structure == StructureKind::Relay)
  {
    // The node of the sector under the cursor, free, and adjacent to a sector the player holds; without sectors, nowhere.
    const SectorView* sector = FindSector(_sectors, _cursor);
    if (sector == nullptr)
      return {.position = _cursor, .radiusMeters = _type.radiusMeters, .valid = false};
    const auto heldByPlayer = [&](std::int32_t _id)
    {
      const auto found = std::ranges::find(_sectors, _id, &SectorView::id);
      return found != _sectors.end() && found->holder == _player;
    };
    GhostPlacement ghost =
      PlaceGhost({.structure = StructureKind::Shipyard, .radiusMeters = _type.radiusMeters}, sector->node, _entities, _mapSizeMeters);
    ghost.valid = ghost.valid && !sector->holder.IsValid() && std::ranges::any_of(sector->adjacent, heldByPlayer) &&
                  !AtNodeCap(_sectors, _entities, _player, _nodeCap) && !sector->guarded;
    return ghost;
  }
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
    // On a map with sectors, ore belongs to the sector it lies in (Phase 2 design §4).
    const SectorView* sector = FindSector(_sectors, nearest->position);
    const bool held = _sectors.empty() || (sector != nullptr && sector->holder == _player);
    return {.position = nearest->position, .radiusMeters = std::max(_type.radiusMeters, nearest->radiusMeters), .valid = !taken && held};
  }

  const float half = _mapSizeMeters / 2.0f;
  const bool inside = std::abs(_cursor.xMeters) + _type.radiusMeters <= half && std::abs(_cursor.zMeters) + _type.radiusMeters <= half;
  // On a map with sectors, a Shipyard stands only in a sector the player holds, so that production follows territory
  // (Phase 4 design §6).
  if (_type.structure == StructureKind::Shipyard && !_sectors.empty())
  {
    const SectorView* sector = FindSector(_sectors, _cursor);
    if (sector == nullptr || sector->holder != _player)
      return {.position = _cursor, .radiusMeters = _type.radiusMeters, .valid = false};
  }
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

std::int32_t Outpost::NodesTaken(std::span<const SectorView> _sectors, std::span<const EntityView> _entities, PlayerId _player) noexcept
{
  const auto held = std::ranges::count(_sectors, _player, &SectorView::holder);
  const auto sites = std::ranges::count_if(_entities,
                                           [_player](const EntityView& _entity)
                                           {
                                             return _entity.kind == EntityKind::Structure && _entity.structure == StructureKind::Relay &&
                                                    _entity.owner == _player && _entity.builtPermille < PERMILLE;
                                           });
  return static_cast<std::int32_t>(held + sites);
}

bool Outpost::AtNodeCap(std::span<const SectorView> _sectors, std::span<const EntityView> _entities, PlayerId _player,
                        std::int32_t _nodeCap) noexcept
{
  return _nodeCap > 0 && NodesTaken(_sectors, _entities, _player) >= _nodeCap;
}
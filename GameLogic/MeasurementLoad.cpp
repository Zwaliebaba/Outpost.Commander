#include "pch.h"
#include "MeasurementLoad.h"

#include <array>
#include <cmath>

namespace
{
// Lattice points this far apart hold the widest hull, 48 m across, with room to spare.
constexpr float LATTICE_SPACING_METERS = 60.0f;
// Kept clear of a start, so the starting bases stand alone.
constexpr float START_CLEARANCE_METERS = 150.0f;
constexpr float STRUCTURE_RADIUS_METERS = 20.0f;
constexpr std::array<Outpost::StructureKind, 5> STRUCTURE_KINDS{Outpost::StructureKind::CommandStation, Outpost::StructureKind::Shipyard,
                                                                Outpost::StructureKind::ResearchLab, Outpost::StructureKind::MiningRig,
                                                                Outpost::StructureKind::DefensePlatform};

bool IsOpen(const Outpost::Map& _map, Outpost::PlanePosition _point, float _radiusMeters) noexcept
{
  const float half = _map.sizeMeters / 2.0f;
  const float margin = _radiusMeters + _map.minimumGapMeters;
  if (std::abs(_point.xMeters) + margin > half || std::abs(_point.zMeters) + margin > half)
    return false;
  for (const Outpost::OreAsteroidPlacement& asteroid : _map.oreAsteroids)
  {
    if (Outpost::Distance(_point, asteroid.position) < asteroid.radiusMeters + margin)
      return false;
  }
  for (const Outpost::AsteroidFieldPlacement& field : _map.asteroidFields)
  {
    if (Outpost::Distance(_point, field.position) < field.radiusMeters + margin)
      return false;
  }
  for (const Outpost::PlanePosition start : _map.starts)
  {
    if (Outpost::Distance(_point, start) < START_CLEARANCE_METERS)
      return false;
  }
  return true;
}

// The player whose half of the map a point is on: the first start's side of the line between the two starts' halves.
Outpost::PlayerId OwnerOf(const Outpost::Map& _map, Outpost::PlanePosition _point) noexcept
{
  if (_map.starts.size() < 2)
    return Outpost::PlayerId{1};
  const bool nearerFirst = Outpost::Distance(_point, _map.starts[0]) <= Outpost::Distance(_point, _map.starts[1]);
  return Outpost::PlayerId{nearerFirst ? 1u : 2u};
}
} // namespace

void Outpost::PlaceMeasurementLoad(Simulation& _simulation, const Map& _map, const Tuning& _tuning)
{
  if (_tuning.hulls.empty() || _tuning.drives.empty())
    throw Neuron::Exception("The measurement load needs at least one hull and one drive in the tuning data.");

  size_t ships = 0;
  size_t structures = 0;
  for (const EntityView& entity : _simulation.BuildSnapshot(PlayerId{}).entities)
  {
    if (entity.kind == EntityKind::Ship)
      ++ships;
    else if (entity.kind == EntityKind::Structure)
      ++structures;
  }

  // The lattice, in rows along x, from the map's corner.
  std::vector<PlanePosition> open;
  const float half = _map.sizeMeters / 2.0f;
  const auto points = static_cast<int>(_map.sizeMeters / LATTICE_SPACING_METERS);
  const auto at = [half](int _index)
  {
    return -half + ((static_cast<float>(_index) + 0.5f) * LATTICE_SPACING_METERS);
  };
  for (int row = 0; row < points; ++row)
  {
    for (int column = 0; column < points; ++column)
    {
      const PlanePosition point{.xMeters = at(column), .zMeters = at(row)};
      if (IsOpen(_map, point, static_cast<float>(_tuning.hulls.back().footprintRadiusMeters)))
        open.push_back(point);
    }
  }
  const size_t shipsToAdd = ships < MEASUREMENT_SHIPS ? MEASUREMENT_SHIPS - ships : 0;
  const size_t structuresToAdd = structures < MEASUREMENT_STRUCTURES ? MEASUREMENT_STRUCTURES - structures : 0;
  if (open.size() < shipsToAdd + structuresToAdd)
  {
    throw Neuron::Exception(std::format("The map has room for {} of the measurement load's {} ships and structures.", open.size(),
                                        shipsToAdd + structuresToAdd));
  }

  // Every other lattice point, so ships and structures mix across the map rather than filling it from one corner.
  const size_t stride = open.size() / (shipsToAdd + structuresToAdd);
  size_t next = 0;
  for (size_t i = 0; i < shipsToAdd; ++i, next += stride)
  {
    const HullTuning& hull = _tuning.hulls[i % _tuning.hulls.size()];
    const ShipMovement movement = MovementFor(_tuning, hull.id, _tuning.drives.front().id);
    (void)_simulation.SpawnShip(OwnerOf(_map, open[next]), DesignId{}, movement, open[next], hull.id);
  }
  for (size_t i = 0; i < structuresToAdd; ++i, next += stride)
  {
    (void)_simulation.SpawnStructure(OwnerOf(_map, open[next]), STRUCTURE_KINDS[i % STRUCTURE_KINDS.size()], open[next],
                                     STRUCTURE_RADIUS_METERS);
  }
}
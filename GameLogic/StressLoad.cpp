#include "pch.h"
#include "StressLoad.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace
{
// Lattice points this far apart hold the widest hull with room to spare.
constexpr float LATTICE_SPACING_METERS = 50.0f;
// Where a player's ships gather and its structures stand: on the line from the middle to its start, this far from the
// middle, where a third and 55% of the way from the start to the middle put them on the 5 km map. In meters rather than
// shares, so that the fight is as near on a larger map and the fleets meet soon after the run starts.
constexpr float RALLY_METERS_FROM_MIDDLE = 1650.0f;
constexpr float STRUCTURES_METERS_FROM_MIDDLE = 1100.0f;
// How often ships standing idle are sent back into the fight: once a second at 20 Hz.
constexpr std::uint64_t REORDER_TICKS = 20;
constexpr std::array<Outpost::StructureKind, 5> STRUCTURE_KINDS{Outpost::StructureKind::CommandStation, Outpost::StructureKind::Shipyard,
                                                                Outpost::StructureKind::ResearchLab, Outpost::StructureKind::MiningRig,
                                                                Outpost::StructureKind::DefensePlatform};

// The point _meters from the map's middle toward _start, or _start itself when it is nearer the middle than that.
Outpost::PlanePosition TowardStart(Outpost::PlanePosition _start, float _meters) noexcept
{
  const float startMeters = Outpost::Distance(_start, Outpost::PlanePosition{});
  const float share = startMeters > _meters ? _meters / startMeters : 1.0f;
  return {.xMeters = _start.xMeters * share, .zMeters = _start.zMeters * share};
}

// Clear of the edge and of every obstacle by _clearanceMeters.
bool IsOpen(const Outpost::Map& _map, Outpost::PlanePosition _point, float _clearanceMeters) noexcept
{
  const float half = _map.sizeMeters / 2.0f;
  if (std::abs(_point.xMeters) + _clearanceMeters > half || std::abs(_point.zMeters) + _clearanceMeters > half)
    return false;
  for (const Outpost::OreAsteroidPlacement& asteroid : _map.oreAsteroids)
  {
    if (Outpost::Distance(_point, asteroid.position) < asteroid.radiusMeters + _clearanceMeters)
      return false;
  }
  for (const Outpost::AsteroidFieldPlacement& field : _map.asteroidFields)
  {
    if (Outpost::Distance(_point, field.position) < field.radiusMeters + _clearanceMeters)
      return false;
  }
  return true;
}

// Every open lattice point of the map, nearest _to first; ties go to the lattice's order, so the result is the same on
// every run.
std::vector<Outpost::PlanePosition> OpenPointsNear(const Outpost::Map& _map, Outpost::PlanePosition _to, float _clearanceMeters)
{
  std::vector<Outpost::PlanePosition> points;
  const float half = _map.sizeMeters / 2.0f;
  const auto count = static_cast<int>(_map.sizeMeters / LATTICE_SPACING_METERS);
  for (int row = 0; row < count; ++row)
  {
    for (int column = 0; column < count; ++column)
    {
      const Outpost::PlanePosition point{.xMeters = -half + ((static_cast<float>(column) + 0.5f) * LATTICE_SPACING_METERS),
                                         .zMeters = -half + ((static_cast<float>(row) + 0.5f) * LATTICE_SPACING_METERS)};
      if (IsOpen(_map, point, _clearanceMeters))
        points.push_back(point);
    }
  }
  std::ranges::stable_sort(points, {}, [_to](Outpost::PlanePosition _point) { return Outpost::Distance(_point, _to); });
  return points;
}
} // namespace

Outpost::StressLoad::StressLoad(Simulation& _simulation, const Map& _map, const Tuning& _tuning)
{
  if (_map.starts.size() != m_sides.size())
    throw Neuron::Exception("The stress load needs a map with two starts.");
  float widestMeters = 0.0f;
  for (const HullTuning& hull : _tuning.hulls)
    widestMeters = std::max(widestMeters, static_cast<float>(hull.footprintRadiusMeters));

  for (size_t index = 0; index < m_sides.size(); ++index)
  {
    Side& side = m_sides[index];
    side.player = PlayerId{static_cast<std::uint32_t>(index + 1)};
    side.start = _map.starts[index];
    // The fleets head for each other's rally, where replacements appear, so the fight stays in one place.
    side.enemyRally = TowardStart(_map.starts[1 - index], RALLY_METERS_FROM_MIDDLE);
    for (const DesignComponents& components : StartingDesigns(_tuning))
    {
      if (const ShipDesign* design = _simulation.FindDesign(side.player, components))
        side.designs.push_back(design->id);
    }
    if (side.designs.empty())
      throw Neuron::Exception(std::format("Player {} has no starting design for the stress load.", side.player.value));
    const PlanePosition rally = TowardStart(side.start, RALLY_METERS_FROM_MIDDLE);
    side.berths = OpenPointsNear(_map, rally, widestMeters);
    side.berths.resize(std::min(side.berths.size(), STRESS_SHIPS_PER_PLAYER));
    if (side.berths.size() < STRESS_SHIPS_PER_PLAYER)
      throw Neuron::Exception("The map has too little open ground for the stress load's ships.");

    // The structures the player already has, its Command Station, count toward the scene's.
    const auto existing =
      static_cast<size_t>(std::ranges::count_if(_simulation.Entities(), [&side](const Entity& _entity)
                                                { return _entity.kind == EntityKind::Structure && _entity.owner == side.player; }));
    const PlanePosition structuresAt = TowardStart(side.start, STRUCTURES_METERS_FROM_MIDDLE);
    float widestStructureMeters = 0.0f;
    for (const StructureTuning& structure : _tuning.structures)
      widestStructureMeters = std::max(widestStructureMeters, static_cast<float>(structure.footprintRadiusMeters));
    // Structures block movement (ADR-016), so each keeps the map's narrowest passage from the others, as from obstacles.
    std::vector<PlanePosition> sites;
    for (const PlanePosition point : OpenPointsNear(_map, structuresAt, widestStructureMeters + _map.minimumGapMeters))
    {
      if (sites.size() + existing >= STRESS_STRUCTURES_PER_PLAYER)
        break;
      const bool clear = std::ranges::all_of(sites, [&](PlanePosition _site)
                                             { return Distance(_site, point) >= (2.0f * widestStructureMeters) + _map.minimumGapMeters; });
      if (clear)
        sites.push_back(point);
    }
    if (sites.size() + existing < STRESS_STRUCTURES_PER_PLAYER)
      throw Neuron::Exception("The map has too little open ground for the stress load's structures.");
    // A Mining Rig stands on an ore asteroid, as one built in a match does (design §6): on the free ones nearest the
    // structures, in the map's order where they tie (owner, 2026-10-03).
    std::vector<OreAsteroidPlacement> ores = _map.oreAsteroids;
    std::ranges::stable_sort(ores, {}, [structuresAt](const OreAsteroidPlacement& _ore) { return Distance(_ore.position, structuresAt); });
    const auto taken = [&_simulation](const OreAsteroidPlacement& _ore)
    {
      return std::ranges::any_of(_simulation.Entities(),
                                 [&_ore](const Entity& _entity) {
                                   return _entity.kind == EntityKind::Structure && _entity.structure == StructureKind::MiningRig &&
                                          _entity.position == _ore.position;
                                 });
    };
    size_t nextOre = 0;
    for (size_t i = 0; i < sites.size(); ++i)
    {
      const StructureKind kind = STRUCTURE_KINDS[i % STRUCTURE_KINDS.size()];
      const auto tuning = std::ranges::find(_tuning.structures, kind, &StructureTuning::kind);
      if (tuning == _tuning.structures.end())
        throw Neuron::Exception("The tuning data lacks a structure kind the stress load places.");
      PlanePosition at = sites[i];
      if (kind == StructureKind::MiningRig)
      {
        while (nextOre < ores.size() && taken(ores[nextOre]))
          ++nextOre;
        if (nextOre == ores.size())
          throw Neuron::Exception("The map has too few free ore asteroids for the stress load's Mining Rigs.");
        at = ores[nextOre++].position;
      }
      (void)_simulation.SpawnStructure(side.player, kind, at, static_cast<float>(tuning->footprintRadiusMeters),
                                       tuning->hitPoints * HUNDREDTHS, tuning->armor * HUNDREDTHS);
    }
  }
}

Outpost::EntityId Outpost::StressLoad::Launch(Simulation& _simulation, Side& _side)
{
  const DesignId design = _side.designs[_side.nextDesign++ % _side.designs.size()];
  const PlanePosition at = _side.berths[_side.nextBerth++ % _side.berths.size()];
  const PlaneVector toEnemy = _side.enemyRally - at;
  return _simulation.SpawnShip(_side.player, design, at, std::atan2(toEnemy.zMeters, toEnemy.xMeters));
}

std::vector<Outpost::Command> Outpost::StressLoad::TopUp(Simulation& _simulation)
{
  std::array<size_t, 2> ships{};
  std::array<std::vector<EntityId>, 2> fleets;
  std::array<std::vector<EntityId>, 2> idle;
  std::array<PlaneVector, 2> sums{};
  for (const Entity& entity : _simulation.Entities())
  {
    // Warships only: the Constructors stay at home.
    if (entity.kind != EntityKind::Ship || entity.role != ShipRole::Warship)
      continue;
    for (size_t index = 0; index < m_sides.size(); ++index)
    {
      if (entity.owner != m_sides[index].player)
        continue;
      ++ships[index];
      fleets[index].push_back(entity.id);
      sums[index] = sums[index] + (entity.position - PlanePosition{});
      if (entity.order == ShipOrder::None && !entity.target.IsValid())
        idle[index].push_back(entity.id);
    }
  }
  const bool reorderIdle = (_simulation.CurrentTick() % REORDER_TICKS) == 0;

  std::vector<Command> commands;
  for (size_t index = 0; index < m_sides.size(); ++index)
  {
    Side& side = m_sides[index];
    std::vector<EntityId> launched;
    for (size_t count = ships[index]; count < STRESS_SHIPS_PER_PLAYER; ++count)
      launched.push_back(Launch(_simulation, side));
    // The first time, the whole fleet sets off; after that, only the replacements.
    std::vector<EntityId> ordered = m_ordered ? std::move(launched) : [&]
    {
      std::vector<EntityId> all = fleets[index];
      all.insert(all.end(), launched.begin(), launched.end());
      return all;
    }();
    if (!ordered.empty())
      commands.push_back({.player = side.player, .order = AttackMoveCommand{.ships = std::move(ordered), .destination = side.enemyRally}});
    // Ships that won through and stand idle go after what is left of the enemy, so the whole of both fleets keeps
    // fighting.
    const size_t enemy = 1 - index;
    if (m_ordered && reorderIdle && !idle[index].empty() && ships[enemy] > 0)
    {
      const PlanePosition enemyCenter = PlanePosition{} + sums[enemy] * (1.0f / static_cast<float>(ships[enemy]));
      commands.push_back({.player = side.player, .order = AttackMoveCommand{.ships = std::move(idle[index]), .destination = enemyCenter}});
    }
  }
  m_ordered = true;
  return commands;
}
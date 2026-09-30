#include "pch.h"
#include "Simulation.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace
{
bool IsFinite(Outpost::PlanePosition _position) noexcept
{
  return std::isfinite(_position.xMeters) && std::isfinite(_position.zMeters);
}
} // namespace

Outpost::Simulation::Simulation(std::uint64_t _seed) noexcept
  : m_random(_seed)
{
}

void Outpost::Simulation::PlaceMap(const Map& _map)
{
  for (const OreAsteroidPlacement& asteroid : _map.oreAsteroids)
  {
    m_entities.push_back({.id = EntityId{++m_lastEntityId},
                          .kind = EntityKind::Asteroid,
                          .position = asteroid.position,
                          .radiusMeters = asteroid.radiusMeters,
                          .oreYield = asteroid.yield});
  }
  for (const AsteroidFieldPlacement& field : _map.asteroidFields)
  {
    m_entities.push_back({.id = EntityId{++m_lastEntityId},
                          .kind = EntityKind::AsteroidField,
                          .position = field.position,
                          .radiusMeters = field.radiusMeters});
  }
}

Outpost::EntityId Outpost::Simulation::SpawnShip(PlayerId _owner, DesignId _design, PlanePosition _position)
{
  const EntityId id{++m_lastEntityId};
  m_entities.push_back({.id = id, .kind = EntityKind::Ship, .owner = _owner, .design = _design, .position = _position});
  return id;
}

std::vector<Outpost::CommandResult> Outpost::Simulation::Tick(const std::vector<Command>& _commands)
{
  std::vector<CommandResult> results;
  results.reserve(_commands.size());
  for (const Command& command : _commands)
  {
    results.push_back(std::visit(
      [this, &command]<typename OrderType>(const OrderType& _order)
      {
        if constexpr (std::is_same_v<OrderType, MoveCommand> || std::is_same_v<OrderType, StopCommand>)
          return Apply(command.player, _order);
        else
          return CommandResult::NotYetSupported;
      },
      command.order));
  }

  // The world advances here: movement is task 2.4 and combat 3.3.
  ++m_tick;
  return results;
}

Outpost::Snapshot Outpost::Simulation::BuildSnapshot(PlayerId _player) const
{
  Snapshot snapshot{.tick = m_tick, .player = _player};
  snapshot.entities.reserve(m_entities.size());
  for (const Entity& entity : m_entities)
  {
    snapshot.entities.push_back({.id = entity.id,
                                 .kind = entity.kind,
                                 .owner = entity.owner,
                                 .design = entity.design,
                                 .structure = entity.structure,
                                 .position = entity.position,
                                 .headingRadians = entity.headingRadians,
                                 .radiusMeters = entity.radiusMeters});
  }
  return snapshot;
}

const Outpost::Entity* Outpost::Simulation::FindEntity(EntityId _id) const noexcept
{
  const auto found = std::ranges::lower_bound(m_entities, _id, {}, &Entity::id);
  return (found != m_entities.end() && found->id == _id) ? &*found : nullptr;
}

Outpost::Entity* Outpost::Simulation::FindMutableEntity(EntityId _id) noexcept
{
  return const_cast<Entity*>(std::as_const(*this).FindEntity(_id));
}

Outpost::CommandResult Outpost::Simulation::ValidateShips(PlayerId _player, const std::vector<EntityId>& _ships) const noexcept
{
  if (_ships.empty())
    return CommandResult::NoShips;
  for (const EntityId id : _ships)
  {
    const Entity* entity = FindEntity(id);
    if (entity == nullptr)
      return CommandResult::UnknownEntity;
    if (entity->kind != EntityKind::Ship)
      return CommandResult::NotAShip;
    if (entity->owner != _player)
      return CommandResult::NotOwned;
  }
  return CommandResult::Applied;
}

Outpost::CommandResult Outpost::Simulation::Apply(PlayerId _player, const MoveCommand& _move)
{
  if (!IsFinite(_move.destination))
    return CommandResult::InvalidPosition;
  if (const CommandResult result = ValidateShips(_player, _move.ships); result != CommandResult::Applied)
    return result;
  for (const EntityId id : _move.ships)
    FindMutableEntity(id)->destination = _move.destination;
  return CommandResult::Applied;
}

Outpost::CommandResult Outpost::Simulation::Apply(PlayerId _player, const StopCommand& _stop)
{
  if (const CommandResult result = ValidateShips(_player, _stop.ships); result != CommandResult::Applied)
    return result;
  for (const EntityId id : _stop.ships)
    FindMutableEntity(id)->destination.reset();
  return CommandResult::Applied;
}

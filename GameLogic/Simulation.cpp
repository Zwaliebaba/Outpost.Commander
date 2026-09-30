#include "pch.h"
#include "Simulation.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <utility>

namespace
{
using Outpost::PlanePosition;
using Outpost::PlaneVector;

// A ship that has come no closer to its next waypoint for this long gives it up: it passes a corner, or counts itself
// arrived at its slot. Ships in a crowd, or a slot pushed onto another by an obstacle or the edge, otherwise wait forever.
constexpr float STALL_SECONDS = 1.0f;
// Getting closer by less than this does not count as progress.
constexpr float PROGRESS_METERS = 0.1f;

// Room between neighbors in a formation, in footprint radii of the group's widest ship: two radii for the ships
// themselves, one for the gap. Loose, as design §9 asks.
constexpr float FORMATION_SPACING_RADII = 3.0f;

bool IsFinite(PlanePosition _position) noexcept
{
  return std::isfinite(_position.xMeters) && std::isfinite(_position.zMeters);
}

// _angle brought into (-pi, pi].
float WrapAngle(float _angle) noexcept
{
  constexpr float PI = std::numbers::pi_v<float>;
  while (_angle > PI)
    _angle -= 2.0f * PI;
  while (_angle <= -PI)
    _angle += 2.0f * PI;
  return _angle;
}

float PathLength(PlanePosition _from, const std::vector<PlanePosition>& _path) noexcept
{
  float length = 0.0f;
  PlanePosition previous = _from;
  for (const PlanePosition waypoint : _path)
  {
    length += Outpost::Distance(previous, waypoint);
    previous = waypoint;
  }
  return length;
}
} // namespace

Outpost::ShipMovement Outpost::MovementFor(const Tuning& _tuning, HullId _hull, DriveId _drive)
{
  const auto hull = std::ranges::find(_tuning.hulls, _hull, &HullTuning::id);
  const auto drive = std::ranges::find(_tuning.drives, _drive, &DriveTuning::id);
  if (hull == _tuning.hulls.end() || drive == _tuning.drives.end())
    throw Neuron::Exception(std::format("MovementFor: no hull {} or no drive {} in the tuning data", _hull.value, _drive.value));
  constexpr double RADIANS_PER_DEGREE = std::numbers::pi / 180.0;
  return {.speedMetersPerSecond = static_cast<float>(hull->speedMetersPerSecond * drive->speedFactor),
          .turnRateRadiansPerSecond = static_cast<float>(hull->turnRateDegreesPerSecond * drive->turnRateFactor * RADIANS_PER_DEGREE),
          .radiusMeters = static_cast<float>(hull->footprintRadiusMeters)};
}

Outpost::Simulation::Simulation(std::uint64_t _seed, std::uint32_t _ticksPerSecond)
  : m_random(_seed)
{
  if (_ticksPerSecond == 0)
    throw Neuron::Exception("Simulation: the tick rate must be at least 1");
  m_secondsPerTick = 1.0f / static_cast<float>(_ticksPerSecond);
  m_stallLimitTicks = static_cast<std::uint32_t>(std::ceil(STALL_SECONDS * static_cast<float>(_ticksPerSecond)));
}

void Outpost::Simulation::PlaceMap(const Map& _map)
{
  std::vector<Obstacle> obstacles;
  obstacles.reserve(_map.oreAsteroids.size() + _map.asteroidFields.size());
  for (const OreAsteroidPlacement& asteroid : _map.oreAsteroids)
  {
    m_entities.push_back({.id = EntityId{++m_lastEntityId},
                          .kind = EntityKind::Asteroid,
                          .position = asteroid.position,
                          .radiusMeters = asteroid.radiusMeters,
                          .oreYield = asteroid.yield});
    obstacles.push_back({asteroid.position, asteroid.radiusMeters});
  }
  for (const AsteroidFieldPlacement& field : _map.asteroidFields)
  {
    m_entities.push_back({.id = EntityId{++m_lastEntityId},
                          .kind = EntityKind::AsteroidField,
                          .position = field.position,
                          .radiusMeters = field.radiusMeters});
    obstacles.push_back({field.position, field.radiusMeters});
  }
  m_pathfinder.SetObstacles(std::move(obstacles), _map.sizeMeters / 2.0f);
}

Outpost::EntityId Outpost::Simulation::SpawnShip(PlayerId _owner, DesignId _design, const ShipMovement& _movement, PlanePosition _position)
{
  const EntityId id{++m_lastEntityId};
  m_entities.push_back({.id = id,
                        .kind = EntityKind::Ship,
                        .owner = _owner,
                        .design = _design,
                        .position = _position,
                        .radiusMeters = _movement.radiusMeters,
                        .speedMetersPerSecond = _movement.speedMetersPerSecond,
                        .turnRateRadiansPerSecond = _movement.turnRateRadiansPerSecond});
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

  // The world advances: ships steer along their paths, then make room for each other, then leave any obstacle they were
  // pushed into. Combat is task 3.3.
  MoveShips();
  SeparateShips();
  KeepShipsClear();
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

// A group ordered as one keeps a loose formation (design §9): a grid of slots around the destination, facing the way the
// group travels, with the ships that are ahead now taking the front slots so that few paths cross. Each ship paths to its
// own slot, and each goes at the speed that brings the whole group in at the same moment, which is no faster than its
// slowest ship allows.
Outpost::CommandResult Outpost::Simulation::Apply(PlayerId _player, const MoveCommand& _move)
{
  if (!IsFinite(_move.destination))
    return CommandResult::InvalidPosition;
  if (const CommandResult result = ValidateShips(_player, _move.ships); result != CommandResult::Applied)
    return result;

  std::vector<Entity*> ships;
  for (const EntityId id : _move.ships)
  {
    Entity* ship = FindMutableEntity(id);
    if (std::ranges::find(ships, ship) == ships.end())
      ships.push_back(ship);
  }

  PlaneVector sum{};
  float widestRadius = 0.0f;
  for (const Entity* ship : ships)
  {
    sum = sum + (ship->position - PlanePosition{});
    widestRadius = std::max(widestRadius, ship->radiusMeters);
  }
  const PlanePosition center = PlanePosition{} + sum * (1.0f / static_cast<float>(ships.size()));
  const PlaneVector forward = Normalized(_move.destination - center, {1.0f, 0.0f});
  const PlaneVector side = Perpendicular(forward);

  std::ranges::sort(ships,
                    [&](const Entity* _a, const Entity* _b)
                    {
                      const float aheadA = Dot(_a->position - center, forward);
                      const float aheadB = Dot(_b->position - center, forward);
                      if (aheadA != aheadB)
                        return aheadA > aheadB;
                      const float sideA = Dot(_a->position - center, side);
                      const float sideB = Dot(_b->position - center, side);
                      if (sideA != sideB)
                        return sideA < sideB;
                      return _a->id < _b->id;
                    });

  const auto columns = static_cast<std::size_t>(std::ceil(std::sqrt(static_cast<float>(ships.size()))));
  const float spacing = FORMATION_SPACING_RADII * widestRadius;
  float slowestArrivalSeconds = 0.0f;
  for (std::size_t index = 0; index < ships.size(); ++index)
  {
    const std::size_t row = index / columns;
    const std::size_t inRow = std::min(columns, ships.size() - row * columns);
    const float across = (static_cast<float>(index % columns) - static_cast<float>(inRow - 1) / 2.0f) * spacing;
    const PlanePosition slot = _move.destination + side * across - forward * (static_cast<float>(row) * spacing);

    Entity& ship = *ships[index];
    ship.destination = _move.destination;
    ship.path = m_pathfinder.FindPath(ship.position, slot, ship.radiusMeters);
    ship.closestMeters = std::numeric_limits<float>::infinity();
    ship.stalledTicks = 0;
    if (ship.speedMetersPerSecond > 0.0f)
      slowestArrivalSeconds = std::max(slowestArrivalSeconds, PathLength(ship.position, ship.path) / ship.speedMetersPerSecond);
  }

  for (Entity* ship : ships)
  {
    const float length = PathLength(ship->position, ship->path);
    ship->cruiseSpeedMetersPerSecond = slowestArrivalSeconds > 0.0f ? length / slowestArrivalSeconds : ship->speedMetersPerSecond;
  }
  return CommandResult::Applied;
}

Outpost::CommandResult Outpost::Simulation::Apply(PlayerId _player, const StopCommand& _stop)
{
  if (const CommandResult result = ValidateShips(_player, _stop.ships); result != CommandResult::Applied)
    return result;
  for (const EntityId id : _stop.ships)
  {
    Entity* ship = FindMutableEntity(id);
    ship->destination.reset();
    ship->path.clear();
  }
  return CommandResult::Applied;
}

// A ship turns toward its next waypoint at its turn rate and moves the way it faces, slower the further it is from facing
// the waypoint and not at all while it faces away, so it turns before it sets off rather than circling.
//
// A corner of the path is only a way round an obstacle, so a ship lets it go as soon as it is within its own radius of it,
// or can see the waypoint after it: ships crowding round the same corner then do not have to queue for its exact point.
// Only the last waypoint, the ship's slot, has to be reached.
void Outpost::Simulation::MoveShips()
{
  for (Entity& ship : m_entities)
  {
    if (ship.kind != EntityKind::Ship || ship.path.empty())
      continue;
    while (ship.path.size() > 1 && (Distance(ship.position, ship.path.front()) <= ship.radiusMeters ||
                                    m_pathfinder.IsStraightPathClear(ship.position, ship.path[1], ship.radiusMeters)))
    {
      ship.path.erase(ship.path.begin());
      ship.closestMeters = std::numeric_limits<float>::infinity();
      ship.stalledTicks = 0;
    }

    const PlanePosition waypoint = ship.path.front();
    const PlaneVector toWaypoint = waypoint - ship.position;
    const float remaining = Length(toWaypoint);
    if (remaining > 0.0f)
    {
      const float wanted = std::atan2(toWaypoint.zMeters, toWaypoint.xMeters);
      const float maxTurn = ship.turnRateRadiansPerSecond * m_secondsPerTick;
      ship.headingRadians = WrapAngle(ship.headingRadians + std::clamp(WrapAngle(wanted - ship.headingRadians), -maxTurn, maxTurn));
    }

    const float offCourse = remaining > 0.0f ? WrapAngle(std::atan2(toWaypoint.zMeters, toWaypoint.xMeters) - ship.headingRadians) : 0.0f;
    const float step = ship.cruiseSpeedMetersPerSecond * std::max(0.0f, std::cos(offCourse)) * m_secondsPerTick;
    if (remaining < ship.closestMeters - PROGRESS_METERS)
    {
      ship.closestMeters = remaining;
      ship.stalledTicks = 0;
    }
    else if (step > 0.0f)
    {
      // Turning on the spot is not stalling: only a tick the ship tried to move and got no closer counts.
      ++ship.stalledTicks;
    }

    if (step >= remaining || ship.stalledTicks >= m_stallLimitTicks)
    {
      if (step >= remaining)
        ship.position = waypoint;
      ship.path.erase(ship.path.begin());
      ship.closestMeters = std::numeric_limits<float>::infinity();
      ship.stalledTicks = 0;
      if (ship.path.empty())
        ship.destination.reset();
    }
    else
    {
      ship.position = ship.position + PlaneVector{std::cos(ship.headingRadians), std::sin(ship.headingRadians)} * step;
    }
  }
}

// Ships do not collide, but they do not overlap either (design §9): each overlapping pair is pushed apart along the line
// between them, half each, in identifier order.
void Outpost::Simulation::SeparateShips()
{
  for (std::size_t a = 0; a < m_entities.size(); ++a)
  {
    Entity& first = m_entities[a];
    if (first.kind != EntityKind::Ship)
      continue;
    for (std::size_t b = a + 1; b < m_entities.size(); ++b)
    {
      Entity& second = m_entities[b];
      if (second.kind != EntityKind::Ship)
        continue;
      const PlaneVector between = second.position - first.position;
      const float overlap = first.radiusMeters + second.radiusMeters - Length(between);
      if (overlap <= 0.0f)
        continue;
      // Ships on the same spot part along +x, the earlier one to the left, so that the result does not depend on chance.
      const PlaneVector apart = Normalized(between, {1.0f, 0.0f}) * (overlap / 2.0f);
      first.position = first.position - apart;
      second.position = second.position + apart;
    }
  }
}

// Asteroids and fields block movement (design §4), and the map has an edge: a ship pushed into either leaves it by the
// shortest way.
void Outpost::Simulation::KeepShipsClear()
{
  for (Entity& ship : m_entities)
  {
    if (ship.kind != EntityKind::Ship)
      continue;
    for (const Obstacle& obstacle : m_pathfinder.Obstacles())
    {
      const float reach = obstacle.radiusMeters + ship.radiusMeters;
      if (Distance(ship.position, obstacle.center) < reach)
        ship.position = obstacle.center + Normalized(ship.position - obstacle.center, {1.0f, 0.0f}) * reach;
    }
    ship.position = m_pathfinder.InsideEdge(ship.position, ship.radiusMeters);
  }
}

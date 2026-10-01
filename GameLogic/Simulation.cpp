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

// A tick in thousandths, the unit a weapon's reload counts in (ADR-014).
constexpr std::int32_t MILLITICKS_PER_TICK = 1000;

// A ship on an attack order paths to its target again when the target has moved this far since it last did, at most once
// a second.
constexpr float CHASE_REPATH_METERS = 40.0f;

// Room between neighbors in a starting fleet beyond the widest ship's footprint.
constexpr float FLEET_SPACING_MARGIN_METERS = 8.0f;

// A starting ship's footprint must stay inside the map and clear of every obstacle. The map's own gap check keeps a
// margin around the start, and a fleet too large for it is a data error, reported rather than overlapped.
void CheckStartingSlot(const Outpost::Map& _map, size_t _player, PlanePosition _position, float _radiusMeters)
{
  const auto fail = [_player](std::string_view _what) {
    throw Neuron::Exception(std::format("Player {}'s starting fleet does not fit around its start: a ship would {}.", _player + 1, _what));
  };
  const float half = _map.sizeMeters / 2.0f;
  if (std::abs(_position.xMeters) + _radiusMeters > half || std::abs(_position.zMeters) + _radiusMeters > half)
    fail("cross the map's edge");
  for (const Outpost::OreAsteroidPlacement& asteroid : _map.oreAsteroids)
  {
    if (Outpost::Distance(_position, asteroid.position) < asteroid.radiusMeters + _radiusMeters)
      fail("overlap an ore asteroid");
  }
  for (const Outpost::AsteroidFieldPlacement& field : _map.asteroidFields)
  {
    if (Outpost::Distance(_position, field.position) < field.radiusMeters + _radiusMeters)
      fail("overlap an asteroid field");
  }
}

// Range is measured between centers, as the Q2 model measures it between clumps (ADR-014).
bool IsInRange(const Outpost::Entity& _ship, const Outpost::Entity& _target, float _rangeMeters) noexcept
{
  const PlaneVector between = _target.position - _ship.position;
  return Outpost::Dot(between, between) <= _rangeMeters * _rangeMeters;
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

Outpost::Simulation::Simulation(std::uint64_t _seed, std::uint32_t _ticksPerSecond)
  : m_ticksPerSecond(_ticksPerSecond),
    m_random(_seed)
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

void Outpost::Simulation::AddPlayer(PlayerId _player, std::int32_t _ore)
{
  if (std::ranges::any_of(m_players, [_player](const PlayerState& _state) { return _state.id == _player; }))
    throw Neuron::Exception(std::format("Simulation: player {} is already added", _player.value));
  m_players.push_back({_player, _ore});
}

Outpost::DesignId Outpost::Simulation::SaveDesign(PlayerId _owner, std::string _name, const DesignComponents& _components,
                                                  const DesignStats& _stats)
{
  const DesignId id{++m_lastDesignId};
  m_designs.push_back({.id = id, .owner = _owner, .name = std::move(_name), .components = _components, .stats = _stats});
  return id;
}

void Outpost::Simulation::SaveStartingDesigns(PlayerId _owner, const Tuning& _tuning)
{
  const std::vector<DesignComponents> designs = StartingDesigns(_tuning);
  if (designs.empty())
    throw Neuron::Exception("The tuning data has no starting design: every hull, drive or weapon is unlocked by research.");
  for (const DesignComponents& components : designs)
  {
    if (FindDesign(_owner, components) == nullptr)
      (void)SaveDesign(_owner, DesignName(_tuning, components), components,
                       DesignStatsFor(_tuning, components.hull, components.drive, components.weapon));
  }
}

const Outpost::ShipDesign* Outpost::Simulation::FindDesign(DesignId _id) const noexcept
{
  const auto found = std::ranges::lower_bound(m_designs, _id, {}, &ShipDesign::id);
  return (found != m_designs.end() && found->id == _id) ? &*found : nullptr;
}

const Outpost::ShipDesign* Outpost::Simulation::FindDesign(PlayerId _owner, const DesignComponents& _components) const noexcept
{
  const auto found = std::ranges::find_if(m_designs, [&](const ShipDesign& _design)
                                          { return _design.owner == _owner && _design.components == _components; });
  return found != m_designs.end() ? &*found : nullptr;
}

Outpost::EntityId Outpost::Simulation::SpawnShip(PlayerId _owner, DesignId _design, PlanePosition _position, float _headingRadians)
{
  const ShipDesign* design = FindDesign(_design);
  if (design == nullptr || design->owner != _owner)
    throw Neuron::Exception(std::format("SpawnShip: player {} has no design {}", _owner.value, _design.value));
  const EntityId id = SpawnShip(_owner, _design, design->stats.movement, _position, design->components.hull, _headingRadians);
  Entity& ship = m_entities.back();
  ship.hitPointsHundredths = design->stats.hitPointsHundredths;
  ship.maxHitPointsHundredths = design->stats.hitPointsHundredths;
  ship.armor = design->stats.armor;
  return id;
}

Outpost::EntityId Outpost::Simulation::SpawnShip(PlayerId _owner, DesignId _design, const ShipMovement& _movement, PlanePosition _position,
                                                 HullId _hull, float _headingRadians)
{
  const EntityId id{++m_lastEntityId};
  m_entities.push_back({.id = id,
                        .kind = EntityKind::Ship,
                        .owner = _owner,
                        .design = _design,
                        .hull = _hull,
                        .position = _position,
                        .headingRadians = _headingRadians,
                        .radiusMeters = _movement.radiusMeters,
                        .speedMetersPerSecond = _movement.speedMetersPerSecond,
                        .turnRateRadiansPerSecond = _movement.turnRateRadiansPerSecond});
  return id;
}

Outpost::EntityId Outpost::Simulation::SpawnStructure(PlayerId _owner, StructureKind _kind, PlanePosition _position, float _radiusMeters,
                                                      std::int32_t _hitPointsHundredths, std::int32_t _armor)
{
  const EntityId id{++m_lastEntityId};
  m_entities.push_back({.id = id,
                        .kind = EntityKind::Structure,
                        .owner = _owner,
                        .structure = _kind,
                        .position = _position,
                        .radiusMeters = _radiusMeters,
                        .hitPointsHundredths = _hitPointsHundredths,
                        .maxHitPointsHundredths = _hitPointsHundredths,
                        .armor = _armor});
  return id;
}

void Outpost::Simulation::PlaceStartingFleets(const Map& _map, const Tuning& _tuning)
{
  struct Ship
  {
    DesignComponents components;
    ShipMovement movement;
  };
  std::vector<Ship> fleet;
  float widestMeters = 0.0f;
  for (const StartingShips& group : _map.startingFleet)
  {
    const DesignComponents components{group.hull, group.drive, group.weapon};
    // Checks every component exists before anything is placed.
    const ShipMovement movement = DesignStatsFor(_tuning, group.hull, group.drive, group.weapon).movement;
    widestMeters = std::max(widestMeters, movement.radiusMeters);
    fleet.insert(fleet.end(), group.count, {components, movement});
  }
  if (fleet.empty())
    return;

  // A square-ish grid, front row first, with room for the widest ship in every slot.
  const float spacing = (2.0f * widestMeters) + FLEET_SPACING_MARGIN_METERS;
  const auto columns = static_cast<size_t>(std::ceil(std::sqrt(static_cast<float>(fleet.size()))));
  const size_t rows = (fleet.size() + columns - 1) / columns;

  for (size_t player = 0; player < _map.starts.size(); ++player)
  {
    const PlanePosition start = _map.starts[player];
    const float heading = (start.xMeters == 0.0f && start.zMeters == 0.0f) ? 0.0f : std::atan2(-start.zMeters, -start.xMeters);
    const float forwardX = std::cos(heading);
    const float forwardZ = std::sin(heading);
    const PlayerId owner{static_cast<std::uint32_t>(player + 1)};
    for (size_t i = 0; i < fleet.size(); ++i)
    {
      // The grid's row and column, whole numbers, then offsets from the start.
      const size_t row = i / columns;
      const size_t column = i % columns;
      const float ahead = ((static_cast<float>(rows - 1) / 2.0f) - static_cast<float>(row)) * spacing;
      const float across = (static_cast<float>(column) - (static_cast<float>(columns - 1) / 2.0f)) * spacing;
      // Across is to the right of forward, a quarter turn clockwise seen from above.
      const PlanePosition position{.xMeters = start.xMeters + (forwardX * ahead) + (forwardZ * across),
                                   .zMeters = start.zMeters + (forwardZ * ahead) - (forwardX * across)};
      CheckStartingSlot(_map, player, position, fleet[i].movement.radiusMeters);
      const DesignComponents& components = fleet[i].components;
      const ShipDesign* design = FindDesign(owner, components);
      const DesignId designId = design != nullptr
                                  ? design->id
                                  : SaveDesign(owner, DesignName(_tuning, components), components,
                                               DesignStatsFor(_tuning, components.hull, components.drive, components.weapon));
      (void)SpawnShip(owner, designId, position, heading);
    }
  }
}

std::vector<Outpost::CommandResult> Outpost::Simulation::Tick(const std::vector<Command>& _commands)
{
  std::vector<CommandResult> results;
  results.reserve(_commands.size());
  m_shots.clear();
  m_destroyed.clear();
  for (const Command& command : _commands)
  {
    results.push_back(std::visit(
      [this, &command]<typename OrderType>(const OrderType& _order)
      {
        if constexpr (std::is_same_v<OrderType, MoveCommand> || std::is_same_v<OrderType, AttackMoveCommand> ||
                      std::is_same_v<OrderType, AttackCommand> || std::is_same_v<OrderType, StopCommand>)
          return Apply(command.player, _order);
        else
          return CommandResult::NotYetSupported;
      },
      command.order));
  }

  // The world advances: ships fire from where they stand, and the destroyed leave; ships on attack orders head for their
  // targets; ships steer along their paths, then make room for each other, then leave any obstacle they were pushed into.
  Fight();
  ChaseTargets();
  MoveShips();
  SeparateShips();
  KeepShipsClear();
  ++m_tick;
  return results;
}

Outpost::Snapshot Outpost::Simulation::BuildSnapshot(PlayerId _player) const
{
  Snapshot snapshot{.tick = m_tick, .player = _player, .shots = m_shots, .destroyed = m_destroyed};
  if (const auto state = std::ranges::find(m_players, _player, &PlayerState::id); state != m_players.end())
    snapshot.ore = state->ore;
  for (const ShipDesign& design : m_designs)
  {
    if (design.owner == _player)
      snapshot.designs.push_back({.id = design.id,
                                  .nameUtf8 = design.name,
                                  .hull = design.components.hull,
                                  .drive = design.components.drive,
                                  .weapon = design.components.weapon});
  }
  snapshot.entities.reserve(m_entities.size());
  for (const Entity& entity : m_entities)
  {
    snapshot.entities.push_back({.id = entity.id,
                                 .kind = entity.kind,
                                 .owner = entity.owner,
                                 .design = entity.design,
                                 .hull = entity.hull,
                                 .structure = entity.structure,
                                 .position = entity.position,
                                 .headingRadians = entity.headingRadians,
                                 .radiusMeters = entity.radiusMeters,
                                 .hitPointsHundredths = entity.hitPointsHundredths,
                                 .maxHitPointsHundredths = entity.maxHitPointsHundredths});
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
// group travels, with the ships that are ahead now taking the front slots so that few paths cross. The group searches one
// route, and each ship joins it on the way to its own slot (ADR-010). Each goes at the speed that brings the whole group
// in at the same moment, which is no faster than its slowest ship allows.
Outpost::CommandResult Outpost::Simulation::Apply(PlayerId _player, const MoveCommand& _move)
{
  return OrderMove(_player, _move.ships, _move.destination, ShipOrder::Move);
}

// An attack-move is a move whose ships stand to fire while an enemy is in range (design §7).
Outpost::CommandResult Outpost::Simulation::Apply(PlayerId _player, const AttackMoveCommand& _attackMove)
{
  return OrderMove(_player, _attackMove.ships, _attackMove.destination, ShipOrder::AttackMove);
}

Outpost::CommandResult Outpost::Simulation::OrderMove(PlayerId _player, const std::vector<EntityId>& _shipIds, PlanePosition _destination,
                                                      ShipOrder _order)
{
  if (!IsFinite(_destination))
    return CommandResult::InvalidPosition;
  if (const CommandResult result = ValidateShips(_player, _shipIds); result != CommandResult::Applied)
    return result;

  std::vector<Entity*> ships;
  for (const EntityId id : _shipIds)
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
  const PlaneVector forward = Normalized(_destination - center, {1.0f, 0.0f});
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

  GroupRoutes routes(m_pathfinder, _destination, widestRadius);
  if (ships.size() > 1)
    routes.SearchFrom(center);

  const auto columns = static_cast<std::size_t>(std::ceil(std::sqrt(static_cast<float>(ships.size()))));
  const float spacing = FORMATION_SPACING_RADII * widestRadius;
  float slowestArrivalSeconds = 0.0f;
  for (std::size_t index = 0; index < ships.size(); ++index)
  {
    const std::size_t row = index / columns;
    const std::size_t inRow = std::min(columns, ships.size() - row * columns);
    const float across = (static_cast<float>(index % columns) - static_cast<float>(inRow - 1) / 2.0f) * spacing;
    const PlanePosition slot = routes.Destination() + side * across - forward * (static_cast<float>(row) * spacing);

    Entity& ship = *ships[index];
    ship.order = _order;
    ship.attackTarget = {};
    ship.destination = _destination;
    ship.path = routes.PathFor(ship.position, slot, ship.radiusMeters);
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

// Each ship closes on the target, along a route the group searched once, and fires at it once it is in range (design §7).
// Each goes at its own top speed: a chase is not a formation.
Outpost::CommandResult Outpost::Simulation::Apply(PlayerId _player, const AttackCommand& _attack)
{
  if (const CommandResult result = ValidateShips(_player, _attack.ships); result != CommandResult::Applied)
    return result;
  const Entity* target = FindEntity(_attack.target);
  if (target == nullptr)
    return CommandResult::UnknownTarget;
  if (!target->owner.IsValid() || target->owner == _player || target->maxHitPointsHundredths <= 0)
    return CommandResult::NotAnEnemy;
  const PlanePosition targetPosition = target->position;

  PlaneVector sum{};
  float widestRadius = 0.0f;
  for (const EntityId id : _attack.ships)
  {
    const Entity* ship = FindEntity(id);
    sum = sum + (ship->position - PlanePosition{});
    widestRadius = std::max(widestRadius, ship->radiusMeters);
  }
  GroupRoutes routes(m_pathfinder, targetPosition, widestRadius);
  if (_attack.ships.size() > 1)
    routes.SearchFrom(PlanePosition{} + sum * (1.0f / static_cast<float>(_attack.ships.size())));

  for (const EntityId id : _attack.ships)
  {
    Entity& ship = *FindMutableEntity(id);
    ship.order = ShipOrder::Attack;
    ship.attackTarget = _attack.target;
    ship.destination.reset();
    ship.path = routes.PathFor(ship.position, targetPosition, ship.radiusMeters);
    ship.cruiseSpeedMetersPerSecond = ship.speedMetersPerSecond;
    ship.closestMeters = std::numeric_limits<float>::infinity();
    ship.stalledTicks = 0;
    ship.chasedPosition = targetPosition;
    ship.chaseTick = m_tick + m_ticksPerSecond;
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
    ship->order = ShipOrder::None;
    ship->attackTarget = {};
    ship->destination.reset();
    ship->path.clear();
  }
  return CommandResult::Applied;
}

// Design §7's rule, unless the Q2 check forces one of its extremes. Ships come before structures: a structure is chosen
// only when no enemy ship is in range.
Outpost::EntityId Outpost::Simulation::ChooseTarget(const Entity& _ship, float _rangeMeters)
{
  const auto isEnemy = [&_ship](const Entity& _other)
  { return _other.maxHitPointsHundredths > 0 && _other.owner.IsValid() && _other.owner != _ship.owner; };

  if (m_targetRule == TargetRule::Nearest && _ship.target.IsValid())
  {
    // Kept until it dies or leaves range. A dead target has already left the entities.
    if (const Entity* current = FindEntity(_ship.target);
        current != nullptr && isEnemy(*current) && IsInRange(_ship, *current, _rangeMeters))
      return _ship.target;
  }

  for (const EntityKind kind : {EntityKind::Ship, EntityKind::Structure})
  {
    std::vector<const Entity*> inRange;
    for (const Entity& other : m_entities)
    {
      if (other.kind == kind && isEnemy(other) && IsInRange(_ship, other, _rangeMeters))
        inRange.push_back(&other);
    }
    if (inRange.empty())
      continue;
    switch (m_targetRule)
    {
    case TargetRule::Random:
      return inRange[m_random.NextBelow(static_cast<std::uint32_t>(inRange.size()))]->id;
    case TargetRule::Weakest:
      return (*std::ranges::min_element(inRange, {}, &Entity::hitPointsHundredths))->id;
    case TargetRule::Nearest:
      break;
    }
    const auto distanceSquared = [&_ship](const Entity* _other)
    {
      const PlaneVector between = _other->position - _ship.position;
      return Dot(between, between);
    };
    return (*std::ranges::min_element(inRange, {}, distanceSquared))->id;
  }
  return {};
}

// Every armed ship picks its target from where everything stands at the start of the tick and fires if its weapon is
// ready. The hits land together at the end, so no shot depends on which ship was handled first, and a target can be hit
// more than it needs: focus fire overkills, as in the Q2 model. Then the destroyed leave the world, and every order on
// them ends.
void Outpost::Simulation::Fight()
{
  struct Hit
  {
    EntityId target;
    std::int32_t hundredths = 0;
  };
  std::vector<Hit> hits;

  for (Entity& ship : m_entities)
  {
    if (ship.kind != EntityKind::Ship || ship.maxHitPointsHundredths <= 0)
      continue;
    const ShipDesign* design = FindDesign(ship.design);
    if (design == nullptr)
      continue;

    // The attack order's target when it is in range, and otherwise the targeting rule's choice, which keeps the target
    // the ship had last tick when it can.
    const EntityId previous = ship.target;
    EntityId chosen;
    if (ship.order == ShipOrder::Attack)
    {
      if (const Entity* ordered = FindEntity(ship.attackTarget); ordered != nullptr && IsInRange(ship, *ordered, design->stats.rangeMeters))
        chosen = ship.attackTarget;
    }
    if (!chosen.IsValid())
      chosen = ChooseTarget(ship, design->stats.rangeMeters);
    ship.target = chosen;

    const auto intervalMilliticks =
      static_cast<std::int32_t>(std::llround(design->stats.fireIntervalSeconds * m_ticksPerSecond * MILLITICKS_PER_TICK));
    if (!ship.target.IsValid())
    {
      // Idle: the weapon finishes reloading and waits.
      ship.reloadMilliticks = std::max(0, ship.reloadMilliticks - MILLITICKS_PER_TICK);
      continue;
    }
    // A ship that has just found a target fires its first shot at a random moment within its interval, so that a group's
    // volleys do not land together (design §7, the Q2 model).
    if (!previous.IsValid() && intervalMilliticks > 0)
      ship.reloadMilliticks =
        std::max(ship.reloadMilliticks, static_cast<std::int32_t>(m_random.NextBelow(static_cast<std::uint32_t>(intervalMilliticks))));
    ship.reloadMilliticks -= MILLITICKS_PER_TICK;
    if (ship.reloadMilliticks > 0)
      continue;
    ship.reloadMilliticks += intervalMilliticks;

    const Entity& target = *FindEntity(ship.target);
    hits.push_back({target.id, HitHundredths(design->stats.damage, target.armor)});
    m_shots.push_back(
      {.shooter = ship.id, .target = target.id, .weapon = design->components.weapon, .from = ship.position, .to = target.position});
  }

  bool anyDestroyed = false;
  for (const Hit& hit : hits)
  {
    Entity& target = *FindMutableEntity(hit.target);
    target.hitPointsHundredths -= hit.hundredths;
    anyDestroyed |= target.hitPointsHundredths <= 0;
  }
  if (!anyDestroyed)
    return;

  const auto isDestroyed = [](const Entity& _entity) { return _entity.maxHitPointsHundredths > 0 && _entity.hitPointsHundredths <= 0; };
  for (const Entity& entity : m_entities)
  {
    if (isDestroyed(entity))
      m_destroyed.push_back({.id = entity.id,
                             .kind = entity.kind,
                             .owner = entity.owner,
                             .hull = entity.hull,
                             .position = entity.position,
                             .headingRadians = entity.headingRadians,
                             .radiusMeters = entity.radiusMeters});
  }
  std::erase_if(m_entities, isDestroyed);
  for (Entity& ship : m_entities)
  {
    if (ship.target.IsValid() && FindEntity(ship.target) == nullptr)
      ship.target = {};
    if (ship.order == ShipOrder::Attack && FindEntity(ship.attackTarget) == nullptr)
    {
      ship.order = ShipOrder::None;
      ship.attackTarget = {};
      ship.path.clear();
    }
  }
}

// A ship on an attack order stands while its target is in range, and otherwise heads for it, pathing again when the
// target has moved away from where it last pathed to, at most once a second.
void Outpost::Simulation::ChaseTargets()
{
  for (Entity& ship : m_entities)
  {
    if (ship.order != ShipOrder::Attack)
      continue;
    const Entity& target = *FindEntity(ship.attackTarget);
    const ShipDesign* design = FindDesign(ship.design);
    if (design != nullptr && IsInRange(ship, target, design->stats.rangeMeters))
    {
      ship.path.clear();
      continue;
    }
    const bool moved = Distance(target.position, ship.chasedPosition) > CHASE_REPATH_METERS && m_tick >= ship.chaseTick;
    if (!ship.path.empty() && !moved)
      continue;
    ship.path = m_pathfinder.IsStraightPathClear(ship.position, target.position, ship.radiusMeters)
                  ? std::vector<PlanePosition>{target.position}
                  : m_pathfinder.FindPath(ship.position, target.position, ship.radiusMeters);
    ship.cruiseSpeedMetersPerSecond = ship.speedMetersPerSecond;
    ship.closestMeters = std::numeric_limits<float>::infinity();
    ship.stalledTicks = 0;
    ship.chasedPosition = target.position;
    ship.chaseTick = m_tick + m_ticksPerSecond;
  }
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
    // An attack-moving ship stands to fire while an enemy is in range (design §7).
    if (ship.order == ShipOrder::AttackMove && ship.target.IsValid())
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
      {
        ship.destination.reset();
        if (ship.order != ShipOrder::Attack)
          ship.order = ShipOrder::None;
      }
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

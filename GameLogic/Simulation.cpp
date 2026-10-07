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
// Ships turn in arcs, as aircraft do (owner, 2026-10-03, ADR-039): a ship whose waypoint is abeam or behind it flies this
// share of its cruise speed, so that it comes about in a tight loop rather than a wide one.
constexpr float TURN_SPEED_SHARE = 0.5f;
// A ship heading further off its waypoint than this is coming about, not stalled (MoveShips).
constexpr float STALL_HEADING_RADIANS = std::numbers::pi_v<float> / 4.0f;

// A ship standing to fire, pushed almost straight at its target, steps sideways instead (SeparateShips): below this share
// of the push left over sideways.
constexpr float SIDESTEP_SHARE = 0.2f;

// Room between neighbors in a formation, in footprint radii of the group's widest ship: two radii for the ships
// themselves, one for the gap. Loose, as design §9 asks.
constexpr float FORMATION_SPACING_RADII = 3.0f;

// The component of kind T a research topic unlocks, or none.
template <typename T> T UnlockedBy(const Outpost::ResearchTopicTuning& _topic) noexcept
{
  const T* unlocked = std::get_if<T>(&_topic.effect);
  return unlocked != nullptr ? *unlocked : T{};
}

bool IsFinite(PlanePosition _position) noexcept
{
  return std::isfinite(_position.xMeters) && std::isfinite(_position.zMeters);
}

// _angle brought into (-pi, pi].
float WrapAngle(float _angle) noexcept
{
  constexpr float PI = std::numbers::pi_v<float>;
  while (_angle > PI)
  {
    _angle -= 2.0f * PI;
  }
  while (_angle <= -PI)
  {
    _angle += 2.0f * PI;
  }
  return _angle;
}

// A tick in thousandths, the unit a weapon's reload counts in (ADR-014).
constexpr std::int32_t MILLITICKS_PER_TICK = 1000;

// A ship on an attack order paths to its target again when the target has moved this far since it last did, at most once
// a second.
constexpr float CHASE_REPATH_METERS = 40.0f;
// A group holding a sector is sent at an enemy again once the enemy is this far from where it was sent, and goes back to
// its sector's node once no enemy is left and it stands further than this from the node (ADR-059).
constexpr float STANDING_RESEND_METERS = 150.0f;
constexpr float HOLD_RETURN_METERS = 200.0f;

// A Constructor works on what it is ordered to once its footprint comes this close to the target's (ADR-016).
constexpr float WORK_REACH_METERS = 20.0f;
// A structure under construction starts with this share of its hit points and gains the rest as it is built.
constexpr std::int32_t SITE_STARTING_HIT_POINTS_DIVISOR = 10;
// A new ship appears this far beyond its producer's footprint, on the side facing the map's center.
constexpr float SPAWN_GAP_METERS = 5.0f;
// A Command Station or Relay this close to a sector's node stands on it: a Relay is placed on the node and a station on
// its start, which is its home sector's node (ADR-036, ADR-056); this allows for float rounding.
constexpr float NODE_REACH_METERS = 1.0f;
// The starting Constructors stand in a row this far in front of the Command Station's footprint, this far apart.
constexpr float BASE_ROW_GAP_METERS = 25.0f;
constexpr float BASE_ROW_SPACING_METERS = 10.0f;

// A starting Command Station or Constructor must stay inside the map and clear of every obstacle. A base too large for its
// start is a data error, reported rather than overlapped.
void CheckStartingSlot(const Outpost::Map& _map, size_t _player, PlanePosition _position, float _radiusMeters)
{
  const auto fail = [_player](std::string_view _what)
  { throw Neuron::Exception(std::format("Player {}'s starting base does not fit around its start: it would {}.", _player + 1, _what)); };
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

// Range is measured between centers, as the balance check's model measures it between clumps (ADR-014).
bool IsInRange(const Outpost::Entity& _ship, const Outpost::Entity& _target, float _rangeMeters) noexcept
{
  const PlaneVector between = _target.position - _ship.position;
  return Outpost::Dot(between, between) <= _rangeMeters * _rangeMeters;
}

// A group ordered as one keeps a loose formation (design §9, ADR-010): the ships ahead now take its front rows, each row
// from one side to the other, so that few paths cross. The group keeps the formation's width as a band of lanes along its
// route (ADR-047).
void SortForFormation(std::vector<Outpost::Entity*>& _ships, PlanePosition _center, PlaneVector _forward)
{
  const PlaneVector side = Outpost::Perpendicular(_forward);
  std::ranges::sort(_ships,
                    [&](const Outpost::Entity* _a, const Outpost::Entity* _b)
                    {
                      const float aheadA = Outpost::Dot(_a->position - _center, _forward);
                      const float aheadB = Outpost::Dot(_b->position - _center, _forward);
                      if (aheadA != aheadB)
                        return aheadA > aheadB;
                      const float sideA = Outpost::Dot(_a->position - _center, side);
                      const float sideB = Outpost::Dot(_b->position - _center, side);
                      if (sideA != sideB)
                        return sideA < sideB;
                      return _a->id < _b->id;
                    });
}

// The rows of a formation of _ships are this many wide.
std::size_t FormationColumns(std::size_t _ships) noexcept
{
  return static_cast<std::size_t>(std::ceil(std::sqrt(static_cast<float>(_ships))));
}

// How far across the formation the _index-th of _ships keeps, in formation order: its row is centered.
float FormationAcross(std::size_t _index, std::size_t _ships, std::size_t _columns, float _spacing) noexcept
{
  const std::size_t row = _index / _columns;
  const std::size_t inRow = std::min(_columns, _ships - (row * _columns));
  return (static_cast<float>(_index % _columns) - (static_cast<float>(inRow - 1) / 2.0f)) * _spacing;
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
                          .oreYield = asteroid.yield,
                          .oreReserveHundredths = asteroid.reserveOre.has_value()
                                                    ? std::optional<std::int64_t>(std::int64_t{*asteroid.reserveOre} * HUNDREDTHS)
                                                    : std::nullopt});
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
  m_mapObstacles = std::move(obstacles);
  m_mapHalfSizeMeters = _map.sizeMeters / 2.0f;
  m_sectors.clear();
  for (const SectorPlacement& sector : _map.sectors)
    m_sectors.push_back({.placement = sector});
  UpdateObstacles();
}

void Outpost::Simulation::UseTuning(const Tuning& _tuning)
{
  m_tuning = std::make_shared<const Tuning>(_tuning);
  m_unresearched = EffectsFrom(*m_tuning, {});
  for (PlayerState& player : m_players)
    player.researchEffects = EffectsFrom(*m_tuning, player.researched);
}

void Outpost::Simulation::AddPlayer(PlayerId _player, std::int32_t _ore)
{
  if (std::ranges::any_of(m_players, [_player](const PlayerState& _state) { return _state.id == _player; }))
    throw Neuron::Exception(std::format("Simulation: player {} is already added", _player.value));
  m_players.push_back({_player, std::int64_t{_ore} * HUNDREDTHS});
  // It has researched nothing yet.
  m_players.back().researchEffects = m_unresearched;
}

void Outpost::Simulation::UseFog()
{
  if (!m_tuning)
    throw Neuron::Exception("Simulation: fog of war needs the tuning data's sight");
  m_fog = true;
  UpdateVision();
}

bool Outpost::Simulation::Sees(PlayerId _player, const Entity& _entity) const noexcept
{
  if (!m_fog || !_entity.owner.IsValid() || _entity.owner == _player)
    return true;
  const PlayerState* state = FindPlayer(_player);
  return state != nullptr && std::ranges::binary_search(state->seen, _entity.id);
}

float Outpost::Simulation::SightMetersOf(const Entity& _entity) const noexcept
{
  if (!m_tuning)
    return 0.0f;
  // A ship with a module that sees further sees that far, whatever its weapon (Phase 2 design §10, ADR-058).
  const ShipDesign* design = _entity.kind == EntityKind::Ship ? FindDesign(_entity.design) : nullptr;
  const float moduleSight = design != nullptr ? design->stats.moduleSightMeters : 0.0f;
  if (const std::optional<Armament> armament = ArmamentOf(_entity))
    return std::max(moduleSight, armament->rangeMeters + static_cast<float>(m_tuning->sight.weaponMarginMeters));
  return std::max(moduleSight, static_cast<float>(m_tuning->sight.unarmedMeters));
}

std::vector<Outpost::Simulation::Observer> Outpost::Simulation::ObserversOf(PlayerId _player) const
{
  std::vector<Observer> observers;
  for (const Entity& entity : m_entities)
  {
    if (entity.owner == _player && (entity.kind == EntityKind::Ship || entity.kind == EntityKind::Structure))
      observers.push_back({.position = entity.position, .sightMeters = SightMetersOf(entity)});
  }
  return observers;
}

bool Outpost::Simulation::InSight(std::span<const Observer> _observers, PlanePosition _position, float _radiusMeters) noexcept
{
  return std::ranges::any_of(_observers,
                             [&](const Observer& _observer)
                             {
                               const float reach = _observer.sightMeters + _radiusMeters;
                               const PlaneVector between = _position - _observer.position;
                               return Dot(between, between) <= reach * reach;
                             });
}

// A player sees an enemy entity that is within the sight of one of its own, measured from center to the entity's edge, or
// that hit it in the last few seconds (ADR-024). Every armed entity sees beyond its weapon's range, so whatever it can
// shoot it sees: fog never changes what a ship fires at, only what its player knows.
void Outpost::Simulation::UpdateVision()
{
  const std::vector<PlayerId> exposed = PlayersWithoutStation();
  for (PlayerState& player : m_players)
  {
    const std::vector<Observer> observers = ObserversOf(player.id);
    std::erase_if(player.revealedUntil,
                  [this](const auto& _reveal) { return _reveal.second <= m_tick || FindEntity(_reveal.first) == nullptr; });
    player.seen.clear();
    for (const Entity& entity : m_entities)
    {
      if (!entity.owner.IsValid() || entity.owner == player.id)
        continue;
      const bool revealed =
        std::ranges::find(player.revealedUntil, entity.id, &std::pair<EntityId, std::uint64_t>::first) != player.revealedUntil.end();
      if (!revealed && !InSight(observers, entity.position, entity.radiusMeters) && !InSectorSight(player.id, entity.position))
        continue;
      player.seen.push_back(entity.id);
      if (entity.kind != EntityKind::Structure)
        continue;
      EntityView view = EntityViewOf(entity, false);
      if (const auto known = std::ranges::find(player.remembered, entity.id, &EntityView::id); known != player.remembered.end())
        *known = std::move(view);
      else
        player.remembered.push_back(std::move(view));
    }
    // A player that has lost its Command Station has its finished Shipyards shown to its opponents, as remembered
    // structures, so that the end of a match is not a search of the map for one building (Phase 1 design §4, ADR-037).
    for (const Entity& entity : m_entities)
    {
      if (entity.kind != EntityKind::Structure || entity.structure != StructureKind::Shipyard || !entity.IsBuilt() ||
          entity.owner == player.id || std::ranges::find(exposed, entity.owner) == exposed.end() ||
          std::ranges::binary_search(player.seen, entity.id))
        continue;
      EntityView view = EntityViewOf(entity, false);
      if (const auto known = std::ranges::find(player.remembered, entity.id, &EntityView::id); known != player.remembered.end())
        *known = std::move(view);
      else
        player.remembered.push_back(std::move(view));
    }
    // The Ore left in each ore asteroid in sight, which the player remembers once it is out of sight (Phase 1 design §8).
    for (const Entity& asteroid : m_entities)
    {
      if (asteroid.kind != EntityKind::Asteroid || !asteroid.oreReserveHundredths.has_value() ||
          (!InSight(observers, asteroid.position, asteroid.radiusMeters) && !InSectorSight(player.id, asteroid.position)))
        continue;
      const auto known = std::ranges::find(player.knownReserves, asteroid.id, &std::pair<EntityId, std::int64_t>::first);
      if (known != player.knownReserves.end())
        known->second = *asteroid.oreReserveHundredths;
      else
        player.knownReserves.emplace_back(asteroid.id, *asteroid.oreReserveHundredths);
    }
    // A remembered structure whose place is in sight but that is not seen there is gone.
    std::erase_if(player.remembered,
                  [&](const EntityView& _known)
                  {
                    return !std::ranges::binary_search(player.seen, _known.id) &&
                           (InSight(observers, _known.position, _known.radiusMeters) || InSectorSight(player.id, _known.position));
                  });

    // An attack on a ship ends once the ship is out of sight; a structure stays where it was seen.
    for (Entity& ship : m_entities)
    {
      if (ship.owner != player.id || ship.order != ShipOrder::Attack)
        continue;
      const Entity* target = FindEntity(ship.attackTarget);
      if (target != nullptr && target->kind == EntityKind::Ship && !std::ranges::binary_search(player.seen, target->id))
      {
        ship.order = ShipOrder::None;
        ship.attackTarget = {};
        ship.path.clear();
      }
    }
  }
}

std::vector<Outpost::PlayerId> Outpost::Simulation::PlayersWithoutStation() const
{
  std::vector<PlayerId> players;
  for (const PlayerId player : m_basePlayers)
  {
    const bool hasStation = std::ranges::any_of(
      m_entities, [player](const Entity& _entity)
      { return _entity.kind == EntityKind::Structure && _entity.structure == StructureKind::CommandStation && _entity.owner == player; });
    if (!hasStation)
      players.push_back(player);
  }
  return players;
}

void Outpost::Simulation::RevealShooter(PlayerId _hitPlayer, EntityId _shooter)
{
  PlayerState* player = FindPlayer(_hitPlayer);
  if (!m_fog || player == nullptr)
    return;
  const auto ticks = static_cast<std::uint64_t>(std::ceil(m_tuning->sight.shotRevealSeconds * m_ticksPerSecond));
  const auto known = std::ranges::find(player->revealedUntil, _shooter, &std::pair<EntityId, std::uint64_t>::first);
  if (known != player->revealedUntil.end())
    known->second = m_tick + ticks;
  else
    player->revealedUntil.emplace_back(_shooter, m_tick + ticks);
}

Outpost::EntityView Outpost::Simulation::EntityViewOf(const Entity& _entity, bool _detailed) const
{
  EntityView view{.id = _entity.id,
                  .kind = _entity.kind,
                  .owner = _entity.owner,
                  .design = _entity.design,
                  .hull = _entity.hull,
                  .drive = {},
                  .weapon = {},
                  .role = _entity.role,
                  .structure = _entity.structure,
                  .position = _entity.position,
                  .headingRadians = _entity.headingRadians,
                  .radiusMeters = _entity.radiusMeters,
                  .hitPointsHundredths = _entity.hitPointsHundredths,
                  .maxHitPointsHundredths = _entity.maxHitPointsHundredths};
  if (const ShipDesign* design = _entity.kind == EntityKind::Ship ? FindDesign(_entity.design) : nullptr)
  {
    view.drive = design->components.drive;
    view.weapon = design->components.weapon;
    view.module = design->components.module;
  }
  if (!_entity.IsBuilt())
    view.builtPermille = static_cast<std::int32_t>(std::int64_t{_entity.buildWorkDone} * PERMILLE / _entity.buildWorkNeeded);
  view.level = _entity.level;
  if (_entity.IsUpgrading())
    view.upgradePermille = static_cast<std::int32_t>(std::int64_t{_entity.upgradeWorkDone} * PERMILLE / _entity.upgradeWorkNeeded);
  if (_detailed && (!_entity.queue.empty() || !_entity.researchQueue.empty()))
  {
    view.queue = _entity.queue;
    view.research = _entity.researchQueue;
    if (_entity.jobWorkNeeded > 0)
      view.jobPermille = static_cast<std::int32_t>(std::int64_t{_entity.jobWorkDone} * PERMILLE / _entity.jobWorkNeeded);
    if (_entity.secondJobWorkNeeded > 0)
      view.secondJobPermille = static_cast<std::int32_t>(std::int64_t{_entity.secondJobWorkDone} * PERMILLE / _entity.secondJobWorkNeeded);
  }
  return view;
}

std::int64_t Outpost::Simulation::OreHundredths(PlayerId _player) const noexcept
{
  const auto state = std::ranges::find(m_players, _player, &PlayerState::id);
  return state != m_players.end() ? state->oreHundredths : 0;
}

Outpost::Simulation::PlayerState* Outpost::Simulation::FindPlayer(PlayerId _player) noexcept
{
  return const_cast<PlayerState*>(std::as_const(*this).FindPlayer(_player));
}

const Outpost::Simulation::PlayerState* Outpost::Simulation::FindPlayer(PlayerId _player) const noexcept
{
  const auto state = std::ranges::find(m_players, _player, &PlayerState::id);
  return state != m_players.end() ? &*state : nullptr;
}

std::span<const Outpost::ResearchTopicId> Outpost::Simulation::Researched(PlayerId _player) const noexcept
{
  const PlayerState* state = FindPlayer(_player);
  return state != nullptr ? std::span<const ResearchTopicId>(state->researched) : std::span<const ResearchTopicId>();
}

Outpost::Upgrades Outpost::Simulation::UpgradesOf(PlayerId _player) const
{
  return EffectsOf(_player).upgrades;
}

Outpost::Simulation::ResearchEffects Outpost::Simulation::EffectsFrom(const Tuning& _tuning, std::span<const ResearchTopicId> _researched)
{
  ResearchEffects effects{.upgrades = UpgradesFrom(_tuning, _researched)};
  effects.hulls.reserve(_tuning.hulls.size());
  for (const HullTuning& hull : _tuning.hulls)
  {
    effects.hulls.push_back(ViewOf(hull, effects.upgrades, IsAvailable(_tuning, _researched, hull.id)));
    effects.hulls.back().shipyardLevel = ShipyardLevelFor(_tuning, hull.id);
  }
  effects.drives.reserve(_tuning.drives.size());
  for (const DriveTuning& drive : _tuning.drives)
    effects.drives.push_back(ViewOf(drive, IsAvailable(_tuning, _researched, drive.id)));
  effects.weapons.reserve(_tuning.weapons.size());
  for (const WeaponTuning& weapon : _tuning.weapons)
    effects.weapons.push_back(ViewOf(weapon, effects.upgrades, IsAvailable(_tuning, _researched, weapon.id)));
  effects.modules.reserve(_tuning.modules.size());
  for (const ModuleTuning& module : _tuning.modules)
    effects.modules.push_back(ViewOf(module));
  effects.topics.reserve(_tuning.research.size());
  for (const ResearchTopicTuning& topic : _tuning.research)
  {
    effects.topics.push_back({.id = topic.id,
                              .nameUtf8 = topic.name,
                              .effectUtf8 = EffectText(_tuning, topic),
                              .cost = topic.cost,
                              .researchSeconds = topic.researchSeconds,
                              .prerequisites = topic.prerequisites,
                              .researched = std::ranges::find(_researched, topic.id) != _researched.end(),
                              .unlocksHull = UnlockedBy<HullId>(topic),
                              .unlocksDrive = UnlockedBy<DriveId>(topic),
                              .unlocksWeapon = UnlockedBy<WeaponId>(topic),
                              .tier = topic.tier});
  }
  return effects;
}

const Outpost::Simulation::ResearchEffects& Outpost::Simulation::EffectsOf(PlayerId _player) const noexcept
{
  const PlayerState* state = FindPlayer(_player);
  return state != nullptr ? state->researchEffects : m_unresearched;
}

const Outpost::StructureTuning* Outpost::Simulation::StructureTuningFor(StructureKind _kind) const noexcept
{
  if (!m_tuning)
    return nullptr;
  const auto found = std::ranges::find(m_tuning->structures, _kind, &StructureTuning::kind);
  return found != m_tuning->structures.end() ? &*found : nullptr;
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
    {
      (void)SaveDesign(_owner, StartingDesignName(_tuning, components), components, DesignStatsFor(_tuning, components));
    }
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
  ship.armorHundredths = design->stats.armorHundredths;
  // A new ship's weapon is cold: its first shot comes at a random moment once it finds a target (Fight).
  ship.reloadMilliticks = std::numeric_limits<std::int32_t>::min() / 2;
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

Outpost::EntityId Outpost::Simulation::SpawnConstructor(PlayerId _owner, PlanePosition _position, float _headingRadians)
{
  if (!m_tuning)
    throw Neuron::Exception("SpawnConstructor: the simulation has no tuning data");
  const ConstructorTuning& constructor = m_tuning->constructor;
  const ShipMovement movement{.speedMetersPerSecond = ConstructorSpeed(_owner),
                              .turnRateRadiansPerSecond =
                                static_cast<float>(constructor.turnRateDegreesPerSecond * std::numbers::pi / 180.0),
                              .radiusMeters = static_cast<float>(constructor.footprintRadiusMeters)};
  const EntityId id = SpawnShip(_owner, DesignId{}, movement, _position, HullId{}, _headingRadians);
  Entity& ship = m_entities.back();
  ship.role = ShipRole::Constructor;
  ship.hitPointsHundredths = constructor.hitPoints * HUNDREDTHS;
  ship.maxHitPointsHundredths = ship.hitPointsHundredths;
  ship.armorHundredths = constructor.armor * HUNDREDTHS;
  return id;
}

Outpost::EntityId Outpost::Simulation::SpawnStructure(PlayerId _owner, StructureKind _kind, PlanePosition _position, float _radiusMeters,
                                                      std::int32_t _hitPointsHundredths, std::int32_t _armorHundredths, std::int32_t _level)
{
  const EntityId id{++m_lastEntityId};
  const StructureTuning* tuning = StructureTuningFor(_kind);
  m_entities.push_back({.id = id,
                        .kind = EntityKind::Structure,
                        .owner = _owner,
                        .structure = _kind,
                        .position = _position,
                        .radiusMeters = _radiusMeters,
                        .hitPointsHundredths = _hitPointsHundredths,
                        .maxHitPointsHundredths = _hitPointsHundredths,
                        .armorHundredths = _armorHundredths,
                        .level = _level,
                        .structureWeapon = tuning != nullptr ? tuning->structureWeapon : StructureWeaponId{}});
  if (_kind == StructureKind::MiningRig)
  {
    // It stands on the ore asteroid under it, if there is one, mines it, and covers it: Constructors reach it from the
    // asteroid's edge, as ships do.
    for (const Entity& asteroid : m_entities)
    {
      if (asteroid.kind == EntityKind::Asteroid && Distance(asteroid.position, _position) <= asteroid.radiusMeters)
      {
        Entity& rig = m_entities.back();
        rig.site = asteroid.id;
        rig.oreYield = asteroid.oreYield;
        rig.radiusMeters = std::max(rig.radiusMeters, asteroid.radiusMeters);
        break;
      }
    }
  }
  else
    UpdateObstacles();
  // A Command Station or a Relay placed whole holds its sector at once.
  if (_kind == StructureKind::CommandStation || _kind == StructureKind::Relay)
    UpdateTerritory();
  return id;
}

void Outpost::Simulation::PlaceStartingBases(const Map& _map)
{
  const StructureTuning* station = StructureTuningFor(StructureKind::CommandStation);
  if (station == nullptr)
    throw Neuron::Exception("PlaceStartingBases: the simulation has no tuning data");
  const auto stationRadius = static_cast<float>(station->footprintRadiusMeters);
  const auto constructorRadius = static_cast<float>(m_tuning->constructor.footprintRadiusMeters);
  const auto constructors = static_cast<size_t>(m_tuning->rules.startingConstructors);

  for (size_t player = 0; player < _map.starts.size(); ++player)
  {
    const PlanePosition start = _map.starts[player];
    const float heading = (start.xMeters == 0.0f && start.zMeters == 0.0f) ? 0.0f : std::atan2(-start.zMeters, -start.xMeters);
    const PlaneVector forward{std::cos(heading), std::sin(heading)};
    // Across is to the right of forward, a quarter turn clockwise seen from above.
    const PlaneVector across{forward.zMeters, -forward.xMeters};
    const PlayerId owner{static_cast<std::uint32_t>(player + 1)};
    if (std::ranges::find(m_basePlayers, owner) == m_basePlayers.end())
      m_basePlayers.push_back(owner);

    CheckStartingSlot(_map, player, start, stationRadius);
    for (size_t i = 0; i < constructors; ++i)
    {
      const float offset =
        (static_cast<float>(i) - (static_cast<float>(constructors - 1) / 2.0f)) * ((2.0f * constructorRadius) + BASE_ROW_SPACING_METERS);
      const PlanePosition position = start + forward * (stationRadius + BASE_ROW_GAP_METERS + constructorRadius) + across * offset;
      CheckStartingSlot(_map, player, position, constructorRadius);
    }

    (void)SpawnStructure(owner, StructureKind::CommandStation, start, stationRadius, StructureHitPoints(owner, *station),
                         station->armor * HUNDREDTHS);
    for (size_t i = 0; i < constructors; ++i)
    {
      const float offset =
        (static_cast<float>(i) - (static_cast<float>(constructors - 1) / 2.0f)) * ((2.0f * constructorRadius) + BASE_ROW_SPACING_METERS);
      (void)SpawnConstructor(owner, start + forward * (stationRadius + BASE_ROW_GAP_METERS + constructorRadius) + across * offset, heading);
    }
  }
  // On a map with territory every player starts with its tickets, counted in shares of a ticket, as many to a ticket as the
  // map has nodes (ADR-057).
  if (HasTerritory())
  {
    for (const PlayerId owner : m_basePlayers)
    {
      if (PlayerState* player = FindPlayer(owner))
        player->ticketShares = std::int64_t{m_tuning->territory.tickets} * static_cast<std::int64_t>(m_sectors.size());
    }
  }
}

std::vector<Outpost::CommandResult> Outpost::Simulation::Tick(const std::vector<Command>& _commands, TickObserver* _observer)
{
  // The observer watches this tick only, so that it is never left behind in a copy, even when the tick throws.
  struct Watch
  {
    Simulation& simulation;
    Watch(Simulation& _simulation, TickObserver* _observer) noexcept
      : simulation(_simulation)
    {
      simulation.m_observer = _observer;
      simulation.m_pathfinder.Observe(_observer);
    }
    Watch(const Watch&) = delete;
    Watch& operator=(const Watch&) = delete;
    Watch(Watch&&) = delete;
    Watch& operator=(Watch&&) = delete;
    ~Watch()
    {
      simulation.m_observer = nullptr;
      simulation.m_pathfinder.Observe(nullptr);
    }
  };
  const Watch watch(*this, _observer);

  std::vector<CommandResult> results;
  results.reserve(_commands.size());
  m_shots.clear();
  m_destroyed.clear();
  {
    const ObservedPart part(m_observer, TickPart::Commands);
    // The large orders of the last tick plan the rest of their paths and set off before this tick's orders, which may
    // order their ships again (ADR-032).
    FinishPlannedOrders();
    for (const Command& command : _commands)
    {
      const std::size_t plannedBefore = m_plannedOrders.size();
      results.push_back(std::visit(
        [this, &command]<typename OrderType>(const OrderType& _order)
        {
          if constexpr (std::is_same_v<OrderType, MoveCommand> || std::is_same_v<OrderType, AttackMoveCommand> ||
                        std::is_same_v<OrderType, AttackCommand> || std::is_same_v<OrderType, StopCommand> ||
                        std::is_same_v<OrderType, HoldSectorCommand> || std::is_same_v<OrderType, PatrolCommand>)
            return Apply(command.player, _order);
          else
            return m_tuning ? Apply(command.player, _order) : CommandResult::NotYetSupported;
        },
        command.order));
      // A ship given another order leaves the group it was planning with.
      if (results.back() == CommandResult::Applied)
      {
        std::visit(
          [this, plannedBefore]<typename OrderType>(const OrderType& _order)
          {
            if constexpr (requires { _order.ships; })
            {
              for (std::size_t index = 0; index < plannedBefore; ++index)
                std::erase_if(m_plannedOrders[index].members, [&_order](const PlannedOrder::Member& _member)
                              { return std::ranges::find(_order.ships, _member.ship) != _order.ships.end(); });
            }
            // Any other order a player gives a ship ends its standing order (ADR-059).
            if constexpr (std::is_same_v<OrderType, MoveCommand> || std::is_same_v<OrderType, AttackMoveCommand> ||
                          std::is_same_v<OrderType, AttackCommand> || std::is_same_v<OrderType, StopCommand>)
            {
              for (const EntityId id : _order.ships)
              {
                if (Entity* ship = FindMutableEntity(id))
                  ship->standing = StandingOrder::None;
              }
            }
          },
          command.order);
      }
    }
  }

  // The world advances: ships and armed structures fire from where they stand, and the destroyed leave; ships on attack
  // and work orders head for their targets, and Constructors in reach build and repair; levels build, queues produce,
  // labs research and Mining Rigs earn; ships steer along their paths, then make room for each other, then leave any
  // obstacle they were pushed into. Each part is told to the observer, if there is one (task 8.1).
  {
    const ObservedPart part(m_observer, TickPart::Fight);
    Fight();
  }
  {
    const ObservedPart part(m_observer, TickPart::Targets);
    DecideMatch();
    KeepStandingOrders();
    ChaseTargets();
    ApproachWork();
  }
  if (m_tuning)
  {
    const ObservedPart part(m_observer, TickPart::Economy);
    Work();
    BuildLevels();
    Produce();
    Research();
    Mine();
  }
  {
    const ObservedPart part(m_observer, TickPart::Move);
    MoveShips();
  }
  {
    const ObservedPart part(m_observer, TickPart::Separate);
    SeparateShips();
    KeepShipsClear();
  }
  // Who holds what, once every structure built this tick stands and every ship is where the tick leaves it (ADR-056), and
  // what holding it costs the other side (ADR-057).
  UpdateTerritory();
  Dominate();
  if (m_fog)
  {
    const ObservedPart part(m_observer, TickPart::Vision);
    UpdateVision();
  }
  // A tick that planned no paths builds again one graph the obstacles' last change dropped, so that the next order finds
  // it built (ADR-032). The graphs are a cache: when they are built changes no path.
  if (!m_plannedThisTick)
    (void)m_pathfinder.PrepareNext();
  m_plannedThisTick = false;
  ++m_tick;
  return results;
}

Outpost::Snapshot Outpost::Simulation::BuildSnapshot(PlayerId _player) const
{
  Snapshot snapshot{.tick = m_tick, .player = _player};
  if (m_fog)
  {
    // A destruction is seen where it happened; a shot when its shooter or its target is.
    const std::vector<Observer> observers = ObserversOf(_player);
    for (const DestroyedView& destroyed : m_destroyed)
    {
      if (!destroyed.owner.IsValid() || destroyed.owner == _player || InSight(observers, destroyed.position, destroyed.radiusMeters) ||
          InSectorSight(_player, destroyed.position))
        snapshot.destroyed.push_back(destroyed);
    }
    const auto shown = [&](EntityId _id)
    {
      if (const Entity* entity = FindEntity(_id))
        return Sees(_player, *entity);
      return std::ranges::find(snapshot.destroyed, _id, &DestroyedView::id) != snapshot.destroyed.end();
    };
    for (const ShotView& shot : m_shots)
    {
      if (shown(shot.shooter) || shown(shot.target))
        snapshot.shots.push_back(shot);
    }
  }
  else
  {
    snapshot.shots = m_shots;
    snapshot.destroyed = m_destroyed;
  }
  if (const auto state = std::ranges::find(m_players, _player, &PlayerState::id); state != m_players.end())
    snapshot.ore = static_cast<std::int32_t>(state->oreHundredths / HUNDREDTHS);
  if (m_tuning)
    snapshot.oreIncomeHundredthsPerSecond = static_cast<std::int32_t>(IncomeHundredthsPerSecond(_player));
  for (const ShipDesign& design : m_designs)
  {
    if (design.owner == _player)
    {
      snapshot.designs.push_back({.id = design.id,
                                  .nameUtf8 = design.name,
                                  .hull = design.components.hull,
                                  .drive = design.components.drive,
                                  .weapon = design.components.weapon,
                                  .module = design.components.module,
                                  .cost = design.stats.cost});
    }
  }
  snapshot.mapSizeMeters = 2.0f * m_mapHalfSizeMeters;
  if (m_tuning)
  {
    for (const StructureTuning& structure : m_tuning->structures)
    {
      StructureTypeView& type =
        snapshot.structureTypes.emplace_back(StructureTypeView{.structure = structure.kind,
                                                               .nameUtf8 = structure.name,
                                                               .radiusMeters = static_cast<float>(structure.footprintRadiusMeters),
                                                               .buildable = structure.cost.has_value(),
                                                               .cost = structure.cost.value_or(0)});
      for (std::int32_t level = 2; level <= structure.TopLevel(); ++level)
      {
        const StructureLevelTuning& tuning = structure.levels[static_cast<size_t>(level - 2)];
        type.levels.push_back({.cost = tuning.cost,
                               .buildSeconds = tuning.buildSeconds,
                               .maxHitPointsHundredths = StructureHitPoints(_player, structure, level),
                               .opensTier = tuning.opensTier,
                               .researchSlots = tuning.researchSlots,
                               .prerequisites = tuning.prerequisites,
                               .nodes = tuning.nodes,
                               .guns = tuning.guns,
                               .commandPoints = tuning.commandPoints});
      }
    }
    snapshot.constructorCost = m_tuning->constructor.cost;
    snapshot.constructorBuildSeconds = m_tuning->constructor.buildSeconds;

    // The components and topics as the player has them, kept from when its research or the tuning data last changed.
    const ResearchEffects& effects = EffectsOf(_player);
    snapshot.hulls = effects.hulls;
    snapshot.drives = effects.drives;
    snapshot.weapons = effects.weapons;
    snapshot.modules = effects.modules;
    snapshot.research = effects.topics;
    snapshot.shipyardBuildSpeedFactor = effects.upgrades.shipyardBuildSpeedFactor;
    // The tier its finished Research Lab's level has opened (Phase 3 design §6).
    for (const Entity& entity : m_entities)
    {
      if (entity.kind == EntityKind::Structure && entity.structure == StructureKind::ResearchLab && entity.owner == _player &&
          entity.IsBuilt())
        snapshot.researchTier = std::max(snapshot.researchTier, OpenTier(*m_tuning, entity.level));
    }
  }
  snapshot.matchOver = m_matchOver;
  snapshot.winner = m_winner;
  snapshot.matchEndedTick = m_matchEndedTick;
  snapshot.ending = m_ending;
  if (HasTerritory())
    snapshot.nodeCap = NodeCapOf(_player);
  if (m_tuning)
  {
    snapshot.commandPoints = CommandPointsOf(_player);
    snapshot.fleetCap = FleetCapOf(_player);
  }
  if (HasTerritory() && !m_basePlayers.empty())
  {
    const auto nodes = static_cast<std::int64_t>(m_sectors.size());
    snapshot.startingTickets = m_tuning->territory.tickets;
    for (const PlayerId player : m_basePlayers)
      snapshot.tickets.push_back({.player = player, .tickets = static_cast<std::int32_t>((TicketShares(player) + nodes - 1) / nodes)});
  }
  snapshot.fogOfWar = m_fog;
  snapshot.sectors.reserve(m_sectors.size());
  for (const Sector& sector : m_sectors)
  {
    const SectorPlacement& placement = sector.placement;
    snapshot.sectors.push_back({.id = placement.id,
                                .nameUtf8 = placement.name,
                                .minXMeters = placement.minXMeters,
                                .maxXMeters = placement.maxXMeters,
                                .minZMeters = placement.minZMeters,
                                .maxZMeters = placement.maxZMeters,
                                .node = placement.node,
                                .adjacent = placement.adjacent,
                                .holder = sector.holder,
                                .suppressed = sector.suppressed,
                                .cutOff = sector.cutOff});
  }
  snapshot.entities.reserve(m_entities.size());
  for (const Entity& entity : m_entities)
  {
    if (!Sees(_player, entity))
      continue;
    EntityView& view = snapshot.entities.emplace_back(EntityViewOf(entity, !m_fog || entity.owner == _player));
    // A Shipyard's number and the ships it has built are its owner's alone, fog of war or not (Phase 1 design §11).
    if (entity.owner == _player)
    {
      view.shipyardNumber = entity.shipyardNumber;
      view.shipsBuilt = entity.shipsBuilt;
    }
    if (m_fog && entity.owner == _player)
      view.sightMeters = SightMetersOf(entity);
    if (entity.owner == _player)
      view.standing = entity.standing;
    // An ore asteroid's Ore left, and a rig's asteroid's, as far as the player knows it (Phase 1 design §8).
    if (entity.kind == EntityKind::Asteroid)
      view.oreReserveHundredths = ReserveKnownTo(_player, entity.id);
    else if (entity.kind == EntityKind::Structure && entity.structure == StructureKind::MiningRig && entity.site.IsValid())
      view.oreReserveHundredths = ReserveKnownTo(_player, entity.site);
  }
  // The enemy structures it remembers and does not see now, as it last saw them, in identifier order with the rest, as a
  // client pairs two snapshots' entities by it (SnapshotInterpolator).
  if (const PlayerState* state = m_fog ? FindPlayer(_player) : nullptr)
  {
    const auto seenEnd = static_cast<std::ptrdiff_t>(snapshot.entities.size());
    for (const EntityView& known : state->remembered)
    {
      if (std::ranges::binary_search(state->seen, known.id))
        continue;
      EntityView& view = snapshot.entities.emplace_back(known);
      view.remembered = true;
    }
    std::ranges::sort(snapshot.entities.begin() + seenEnd, snapshot.entities.end(), {}, &EntityView::id);
    std::ranges::inplace_merge(snapshot.entities, snapshot.entities.begin() + seenEnd, {}, &EntityView::id);
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
  SortForFormation(ships, center, forward);

  // A grid of slots round the destination, facing the way the group travels, the ships ahead in front (ADR-010). The
  // group keeps the grid's width as a band of lanes along its route (ADR-047).
  const std::size_t columns = FormationColumns(ships.size());
  const float spacing = FORMATION_SPACING_RADII * widestRadius;
  const float bandHalfWidth = static_cast<float>(columns - 1) * spacing / 2.0f;

  GroupRoutes routes(m_pathfinder, _destination, widestRadius, bandHalfWidth);
  if (ships.size() > 1)
  {
    const ObservedPart part(m_observer, TickPart::GroupRoute);
    routes.SearchFrom(center);
  }

  PlannedOrder planned{
    .order = _order, .destination = _destination, .widestRadiusMeters = widestRadius, .bandHalfWidthMeters = bandHalfWidth};
  planned.members.reserve(ships.size());
  for (std::size_t index = 0; index < ships.size(); ++index)
  {
    const std::size_t row = index / columns;
    const float across = FormationAcross(index, ships.size(), columns, spacing);
    planned.members.push_back({.ship = ships[index]->id,
                               .goal = routes.Destination() + side * across - forward * (static_cast<float>(row) * spacing),
                               .laneMeters = across});
  }
  planned.routes = routes.TakeRoutes();
  Plan(std::move(planned));
  return CommandResult::Applied;
}

void Outpost::Simulation::Plan(PlannedOrder _order)
{
  m_plannedThisTick = true;
  if (_order.members.size() <= SPLIT_ORDER_SHIPS)
  {
    PlanPaths(_order, 1);
    SetOff(_order);
    return;
  }
  // A large group plans every other ship now and the rest in the next tick, so that each tick plans as many of the ships
  // ahead as of those behind, which search more often (ADR-032). Its ships hold until then.
  for (const PlannedOrder::Member& member : _order.members)
  {
    Entity& ship = *FindMutableEntity(member.ship);
    ship.order = ShipOrder::None;
    ship.attackTarget = {};
    ship.workTarget = {};
    ship.destination.reset();
    ship.path.clear();
  }
  PlanPaths(_order, 2);
  m_plannedOrders.push_back(std::move(_order));
}

void Outpost::Simulation::PlanPaths(PlannedOrder& _order, std::size_t _stride)
{
  const ObservedPart part(m_observer, TickPart::ShipPaths);
  GroupRoutes routes(m_pathfinder, _order.destination, _order.widestRadiusMeters, _order.bandHalfWidthMeters, std::move(_order.routes));
  for (std::size_t index = 0; index < _order.members.size(); index += _stride)
  {
    PlannedOrder::Member& member = _order.members[index];
    if (member.path.has_value())
      continue;
    // A ship destroyed while its group planned has no path to plan.
    const Entity* ship = FindEntity(member.ship);
    if (ship == nullptr)
    {
      member.path = std::vector<PlanePosition>{};
      continue;
    }
    GroupRoutes::Way way = routes.PathFor(ship->position, member.goal, ship->radiusMeters, member.laneMeters);
    member.path = std::move(way.waypoints);
    member.laneEnd = way.laneEnd;
  }
  _order.routes = routes.TakeRoutes();
}

void Outpost::Simulation::SetOff(const PlannedOrder& _order)
{
  if (_order.order == ShipOrder::Attack)
  {
    // A target destroyed while the group planned leaves its ships holding.
    if (FindEntity(_order.attackTarget) == nullptr)
      return;
    for (const PlannedOrder::Member& member : _order.members)
    {
      Entity* ship = FindMutableEntity(member.ship);
      if (ship == nullptr)
        continue;
      ship->order = ShipOrder::Attack;
      ship->attackTarget = _order.attackTarget;
      ship->workTarget = {};
      ship->destination.reset();
      ship->path = member.path.value_or(std::vector<PlanePosition>{});
      ship->laneEnd = member.laneEnd;
      ship->cruiseSpeedMetersPerSecond = ship->speedMetersPerSecond;
      ship->closestMeters = std::numeric_limits<float>::infinity();
      ship->stalledTicks = 0;
      ship->chasedPosition = _order.destination;
      ship->chaseTick = m_tick + m_ticksPerSecond;
    }
    return;
  }

  // Every ship cruises at its path's length over the time the slowest needs for its own, so that they arrive together.
  float slowestArrivalSeconds = 0.0f;
  for (const PlannedOrder::Member& member : _order.members)
  {
    Entity* ship = FindMutableEntity(member.ship);
    if (ship == nullptr)
      continue;
    ship->order = _order.order;
    ship->attackTarget = {};
    ship->workTarget = {};
    ship->destination = _order.destination;
    ship->path = member.path.value_or(std::vector<PlanePosition>{});
    ship->laneEnd = member.laneEnd;
    ship->closestMeters = std::numeric_limits<float>::infinity();
    ship->stalledTicks = 0;
    if (ship->speedMetersPerSecond > 0.0f)
      slowestArrivalSeconds = std::max(slowestArrivalSeconds, PathLength(ship->position, ship->path) / ship->speedMetersPerSecond);
  }
  for (const PlannedOrder::Member& member : _order.members)
  {
    Entity* ship = FindMutableEntity(member.ship);
    if (ship == nullptr)
      continue;
    const float length = PathLength(ship->position, ship->path);
    ship->cruiseSpeedMetersPerSecond = slowestArrivalSeconds > 0.0f ? length / slowestArrivalSeconds : ship->speedMetersPerSecond;
  }
}

void Outpost::Simulation::FinishPlannedOrders()
{
  std::vector<PlannedOrder> planned = std::exchange(m_plannedOrders, {});
  for (PlannedOrder& order : planned)
  {
    m_plannedThisTick = true;
    PlanPaths(order, 1);
    SetOff(order);
  }
}

// Each ship closes on the target, along a route the group searched once, and fires at it once it is in range (design §7).
// Each goes at its own top speed: a chase is not a formation, but the group keeps a band of lanes round obstacles as a
// moving group does, and paths again as a group when its target moves (ADR-047).
Outpost::CommandResult Outpost::Simulation::Apply(PlayerId _player, const AttackCommand& _order)
{
  if (const CommandResult result = ValidateShips(_player, _order.ships); result != CommandResult::Applied)
    return result;
  // A Constructor has nothing to attack with, so a selection that includes one attacks without it.
  AttackCommand attack{.target = _order.target};
  for (const EntityId id : _order.ships)
  {
    if (FindEntity(id)->role == ShipRole::Warship)
      attack.ships.push_back(id);
  }
  if (attack.ships.empty())
    return CommandResult::NoShips;
  const Entity* target = FindEntity(attack.target);
  if (target == nullptr)
    return CommandResult::UnknownTarget;
  if (!target->owner.IsValid() || target->owner == _player || target->maxHitPointsHundredths <= 0)
    return CommandResult::NotAnEnemy;
  if (m_fog && !Sees(_player, *target))
  {
    const PlayerState* state = FindPlayer(_player);
    const bool remembered =
      state != nullptr && std::ranges::find(state->remembered, target->id, &EntityView::id) != state->remembered.end();
    if (!remembered)
      return CommandResult::NotVisible;
  }
  const PlanePosition targetPosition = target->position;

  std::vector<Entity*> ships;
  for (const EntityId id : attack.ships)
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
  SortForFormation(ships, center, Normalized(targetPosition - center, {1.0f, 0.0f}));
  const std::size_t columns = FormationColumns(ships.size());
  const float spacing = FORMATION_SPACING_RADII * widestRadius;
  const float bandHalfWidth = static_cast<float>(columns - 1) * spacing / 2.0f;
  GroupRoutes routes(m_pathfinder, targetPosition, widestRadius, bandHalfWidth);
  if (ships.size() > 1)
  {
    const ObservedPart part(m_observer, TickPart::GroupRoute);
    routes.SearchFrom(center);
  }

  PlannedOrder planned{.order = ShipOrder::Attack,
                       .destination = targetPosition,
                       .attackTarget = attack.target,
                       .widestRadiusMeters = widestRadius,
                       .bandHalfWidthMeters = bandHalfWidth};
  planned.members.reserve(ships.size());
  for (std::size_t index = 0; index < ships.size(); ++index)
  {
    planned.members.push_back(
      {.ship = ships[index]->id, .goal = targetPosition, .laneMeters = FormationAcross(index, ships.size(), columns, spacing)});
  }
  planned.routes = routes.TakeRoutes();
  Plan(std::move(planned));
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
    ship->workTarget = {};
    ship->destination.reset();
    ship->path.clear();
  }
  return CommandResult::Applied;
}

// Design §7's rule, or one of the balance check's extremes. Ships come before structures: a structure is chosen only
// when no enemy ship is in range.
Outpost::EntityId Outpost::Simulation::ChooseTarget(const Entity& _ship, float _rangeMeters, TargetRule _rule)
{
  const auto isEnemy = [&_ship](const Entity& _other)
  { return _other.maxHitPointsHundredths > 0 && _other.owner.IsValid() && _other.owner != _ship.owner; };

  if (_rule == TargetRule::Nearest)
  {
    // Kept until it dies or leaves range. A dead target has already left the entities.
    if (_ship.target.IsValid())
    {
      if (const Entity* current = FindEntity(_ship.target);
          current != nullptr && isEnemy(*current) && IsInRange(_ship, *current, _rangeMeters))
        return _ship.target;
    }
    // One pass, keeping the nearest ship and the nearest structure in range.
    const float rangeSquared = _rangeMeters * _rangeMeters;
    std::array<const Entity*, 2> nearest{};
    std::array<float, 2> nearestSquared{rangeSquared, rangeSquared};
    for (const Entity& other : m_entities)
    {
      if (!isEnemy(other))
        continue;
      const size_t kind = other.kind == EntityKind::Ship ? 0 : 1;
      const PlaneVector between = other.position - _ship.position;
      const float distanceSquared = Dot(between, between);
      if (distanceSquared <= nearestSquared[kind] && (nearest[kind] == nullptr || distanceSquared < nearestSquared[kind]))
      {
        nearest[kind] = &other;
        nearestSquared[kind] = distanceSquared;
      }
    }
    const Entity* chosen = nearest[0] != nullptr ? nearest[0] : nearest[1];
    return chosen != nullptr ? chosen->id : EntityId{};
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
    if (_rule == TargetRule::Random)
      return inRange[m_random.NextBelow(static_cast<std::uint32_t>(inRange.size()))]->id;
    return (*std::ranges::min_element(inRange, {}, &Entity::hitPointsHundredths))->id;
  }
  return {};
}

// Every armed ship picks its target from where everything stands at the start of the tick and fires if its weapon is
// ready. The hits land together at the end, so no shot depends on which ship was handled first, and a target can be hit
// more than it needs: focus fire overkills, as in the balance check's model. Then the destroyed leave the world, and
// every order on them ends.
void Outpost::Simulation::Fight()
{
  struct Hit
  {
    EntityId target;
    std::int32_t hundredths = 0;
  };
  std::vector<Hit> hits;

  // One gun of _shooter: it reloads, waits while it has no target, and fires at _target when it is ready (ADR-014). The
  // shot names the gun, so that the client draws a structure's guns from their own hardpoints (ADR-064).
  const auto fire = [&](Entity& _shooter, const Armament& _armament, EntityId _target, std::int32_t& _reloadMilliticks, std::uint8_t _gun)
  {
    const auto intervalMilliticks =
      static_cast<std::int32_t>(std::llround(_armament.fireIntervalSeconds * m_ticksPerSecond * MILLITICKS_PER_TICK));
    if (!_target.IsValid())
    {
      // Idle: the weapon finishes reloading, waits, and an interval later goes cold.
      _reloadMilliticks = std::max(-intervalMilliticks, _reloadMilliticks - MILLITICKS_PER_TICK);
      return;
    }
    // A cold weapon fires its first shot at a random moment within its interval, so that a group's volleys do not land
    // together (design §7, the balance check's model). A ship that lost its target only for a moment keeps its rhythm.
    if (_reloadMilliticks <= -intervalMilliticks && intervalMilliticks > 0)
      _reloadMilliticks = static_cast<std::int32_t>(m_random.NextBelow(static_cast<std::uint32_t>(intervalMilliticks)));
    _reloadMilliticks -= MILLITICKS_PER_TICK;
    if (_reloadMilliticks > 0)
      return;
    // A weapon that was ready and waiting fires now, and its next shot comes a whole interval later; one firing steadily
    // carries the fraction of a tick over, so an interval that is not a whole number of ticks keeps its average.
    _reloadMilliticks = std::max(_reloadMilliticks, -MILLITICKS_PER_TICK) + intervalMilliticks;

    // The balance check's spread and focus fire choose again for every shot; the target the ship keeps is only what tells
    // it that an enemy is in range.
    EntityId shotTarget = _target;
    if (m_targetRule != TargetRule::Nearest && _target != _shooter.attackTarget)
      shotTarget = ChooseTarget(_shooter, _armament.rangeMeters, m_targetRule);
    const Entity& target = *FindEntity(shotTarget);
    hits.push_back({target.id, HitHundredths(_armament.damageHundredths, target.armorHundredths)});
    RevealShooter(target.owner, _shooter.id);
    m_shots.push_back({.shooter = _shooter.id,
                       .target = target.id,
                       .weapon = _armament.weapon,
                       .from = _shooter.position,
                       .to = target.position,
                       .splashRadiusMeters = _armament.splashRadiusMeters,
                       .gun = _gun});
    // Splash: every other enemy ship or structure whose center is within the radius of the target's takes the same hit,
    // after its own armor. Never the shooter's own side (ADR-014).
    if (_armament.splashRadiusMeters > 0.0f)
    {
      const float reach = _armament.splashRadiusMeters * _armament.splashRadiusMeters;
      const std::int32_t damageHundredths = _armament.damageHundredths;
      for (const Entity& other : m_entities)
      {
        if (other.id == target.id || other.maxHitPointsHundredths <= 0 || !other.owner.IsValid() || other.owner == _shooter.owner)
          continue;
        const PlaneVector between = other.position - target.position;
        if (Dot(between, between) <= reach)
        {
          hits.push_back({other.id, HitHundredths(damageHundredths, other.armorHundredths)});
          RevealShooter(other.owner, _shooter.id);
        }
      }
    }
  };

  for (Entity& ship : m_entities)
  {
    if (ship.maxHitPointsHundredths <= 0)
      continue;
    const std::optional<Armament> armament = ArmamentOf(ship);
    if (!armament)
      continue;

    // The attack order's target when it is in range, and otherwise the targeting rule's choice, which keeps the target
    // the ship had last tick when it can.
    EntityId chosen;
    if (ship.order == ShipOrder::Attack)
    {
      if (const Entity* ordered = FindEntity(ship.attackTarget); ordered != nullptr && IsInRange(ship, *ordered, armament->rangeMeters))
        chosen = ship.attackTarget;
    }
    if (!chosen.IsValid())
      chosen = ChooseTarget(ship, armament->rangeMeters, TargetRule::Nearest);
    ship.target = chosen;
    const Armament& gun = *armament;
    fire(ship, gun, ship.target, ship.reloadMilliticks, 0);

    // A Command Station's further Defence guns, which its level gives (Phase 3 design §7), each the gun it carries, at the
    // target the station keeps, on a rhythm of its own.
    if (ship.kind == EntityKind::Structure)
    {
      ship.extraGunReloadMilliticks.resize(static_cast<std::size_t>(std::max(0, StationGuns(*m_tuning, ship.structure, ship.level) - 1)));
      for (std::size_t extra = 0; extra < ship.extraGunReloadMilliticks.size(); ++extra)
        fire(ship, gun, ship.target, ship.extraGunReloadMilliticks[extra], static_cast<std::uint8_t>(extra + 1));
    }
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
    {
      m_destroyed.push_back({.id = entity.id,
                             .kind = entity.kind,
                             .structure = entity.structure,
                             .owner = entity.owner,
                             .hull = entity.hull,
                             .position = entity.position,
                             .headingRadians = entity.headingRadians,
                             .radiusMeters = entity.radiusMeters});
    }
  }
  const bool blockerDestroyed = std::ranges::any_of(
    m_entities, [&](const Entity& _entity)
    { return isDestroyed(_entity) && _entity.kind == EntityKind::Structure && _entity.structure != StructureKind::MiningRig; });
  std::erase_if(m_entities, isDestroyed);
  for (Entity& ship : m_entities)
  {
    if (ship.target.IsValid() && FindEntity(ship.target) == nullptr)
      ship.target = {};
    if ((ship.order == ShipOrder::Attack && FindEntity(ship.attackTarget) == nullptr) ||
        (ship.order == ShipOrder::Work && FindEntity(ship.workTarget) == nullptr))
    {
      ship.order = ShipOrder::None;
      ship.attackTarget = {};
      ship.workTarget = {};
      ship.path.clear();
    }
  }
  if (blockerDestroyed)
    UpdateObstacles();
}

// A ship on an attack order stands while its target is in range, and otherwise heads for it, pathing again when the
// target has moved away from where it last pathed to, at most once a second. The ships of one player that path again to
// one target in the same tick path as a group, along one route in a band of lanes, as they set out (ADR-047).
void Outpost::Simulation::ChaseTargets()
{
  // The ships to path again, by their owner and target, in the order their groups' first ships come.
  struct Chase
  {
    PlayerId owner;
    EntityId target;
    std::vector<Entity*> ships;
  };
  std::vector<Chase> chases;
  for (Entity& ship : m_entities)
  {
    if (ship.order != ShipOrder::Attack)
      continue;
    const Entity& target = *FindEntity(ship.attackTarget);
    const std::optional<Armament> armament = ArmamentOf(ship);
    if (armament && IsInRange(ship, target, armament->rangeMeters))
    {
      ship.path.clear();
      continue;
    }
    const bool moved = Distance(target.position, ship.chasedPosition) > CHASE_REPATH_METERS && m_tick >= ship.chaseTick;
    if (!ship.path.empty() && !moved)
      continue;
    const auto chase = std::ranges::find_if(chases, [&ship](const Chase& _chase)
                                            { return _chase.owner == ship.owner && _chase.target == ship.attackTarget; });
    if (chase != chases.end())
      chase->ships.push_back(&ship);
    else
      chases.push_back({.owner = ship.owner, .target = ship.attackTarget, .ships = {&ship}});
  }

  for (Chase& chase : chases)
  {
    const PlanePosition target = FindEntity(chase.target)->position;
    if (chase.ships.size() == 1)
    {
      Entity& ship = *chase.ships.front();
      ship.path = m_pathfinder.IsStraightPathClear(ship.position, target, ship.radiusMeters)
                    ? std::vector<PlanePosition>{target}
                    : m_pathfinder.FindPath(ship.position, target, ship.radiusMeters);
      ship.laneEnd.reset();
    }
    else
    {
      PlaneVector sum{};
      float widestRadius = 0.0f;
      for (const Entity* ship : chase.ships)
      {
        sum = sum + (ship->position - PlanePosition{});
        widestRadius = std::max(widestRadius, ship->radiusMeters);
      }
      const PlanePosition center = PlanePosition{} + sum * (1.0f / static_cast<float>(chase.ships.size()));
      SortForFormation(chase.ships, center, Normalized(target - center, {1.0f, 0.0f}));
      const std::size_t columns = FormationColumns(chase.ships.size());
      const float spacing = FORMATION_SPACING_RADII * widestRadius;
      GroupRoutes routes(m_pathfinder, target, widestRadius, static_cast<float>(columns - 1) * spacing / 2.0f);
      {
        const ObservedPart part(m_observer, TickPart::GroupRoute);
        routes.SearchFrom(center);
      }
      const ObservedPart part(m_observer, TickPart::ShipPaths);
      for (std::size_t index = 0; index < chase.ships.size(); ++index)
      {
        Entity& ship = *chase.ships[index];
        GroupRoutes::Way way =
          routes.PathFor(ship.position, target, ship.radiusMeters, FormationAcross(index, chase.ships.size(), columns, spacing));
        ship.path = std::move(way.waypoints);
        ship.laneEnd = way.laneEnd;
      }
    }
    for (Entity* ship : chase.ships)
    {
      ship->cruiseSpeedMetersPerSecond = ship->speedMetersPerSecond;
      ship->closestMeters = std::numeric_limits<float>::infinity();
      ship->stalledTicks = 0;
      ship->chasedPosition = target;
      ship->chaseTick = m_tick + m_ticksPerSecond;
    }
  }
}

// A ship turns toward its next waypoint at its turn rate as it flies the way it faces, and never on the spot: slower the
// further it is from facing the waypoint, down to half speed, and never too fast to reach it on an arc (ADR-039).
//
// A corner of the path is only a way round an obstacle, so a ship lets it go as soon as it is within its own radius of it,
// or can see the waypoint after it: ships crowding round the same corner then do not have to queue for its exact point.
// Only the last waypoint, the ship's slot, has to be reached. A corner of the ship's lane is its place in its group's band
// as well (ADR-047), so the ship lets it go at sight of the next only from within a place in the formation of it: a ship
// that cut to its slot as soon as it saw it past an obstacle would pass the obstacle where every other ship does.
void Outpost::Simulation::MoveShips()
{
  const auto mayLetGo = [this](const Entity& _ship)
  {
    const float meters = Distance(_ship.position, _ship.path.front());
    if (meters <= _ship.radiusMeters)
      return true;
    const bool inLane = _ship.laneEnd.has_value() && std::ranges::find(_ship.path, *_ship.laneEnd) != _ship.path.end();
    if (inLane && meters > FORMATION_SPACING_RADII * _ship.radiusMeters)
      return false;
    return m_pathfinder.IsStraightPathClear(_ship.position, _ship.path[1], _ship.radiusMeters);
  };
  for (Entity& ship : m_entities)
  {
    if (ship.kind != EntityKind::Ship || ship.path.empty())
      continue;
    // An attack-moving ship stands to fire while an enemy is in range (design §7).
    if (ship.order == ShipOrder::AttackMove && ship.target.IsValid())
      continue;
    while (ship.path.size() > 1 && mayLetGo(ship))
    {
      ship.path.erase(ship.path.begin());
      ship.closestMeters = std::numeric_limits<float>::infinity();
      ship.stalledTicks = 0;
    }

    // A ship flies the way it faces and turns as it goes, as an aircraft does: it never turns on the spot (owner,
    // 2026-10-03, ADR-039). It slows into a turn, to half its cruise speed with its waypoint abeam or behind. It also flies
    // no faster than lets it reach its waypoint on the arc that leaves along its heading, of radius half the distance over
    // the sine of the bearing, so that it never circles a waypoint inside its turn.
    const PlanePosition waypoint = ship.path.front();
    const PlaneVector toWaypoint = waypoint - ship.position;
    const float remaining = Length(toWaypoint);
    const float bearing = remaining > 0.0f ? WrapAngle(std::atan2(toWaypoint.zMeters, toWaypoint.xMeters) - ship.headingRadians) : 0.0f;
    float speed = ship.cruiseSpeedMetersPerSecond * (TURN_SPEED_SHARE + ((1.0f - TURN_SPEED_SHARE) * std::max(0.0f, std::cos(bearing))));
    if (const float abeam = std::abs(std::sin(bearing)); abeam > 0.0f)
      speed = std::min(speed, ship.turnRateRadiansPerSecond * remaining / (2.0f * abeam));
    const float maxTurn = ship.turnRateRadiansPerSecond * m_secondsPerTick;
    ship.headingRadians = WrapAngle(ship.headingRadians + std::clamp(bearing, -maxTurn, maxTurn));
    const float step = speed * m_secondsPerTick;

    // Coming about is not stalling, and progress counts from where the ship has come about: only a tick it heads within
    // 45 degrees of its waypoint and gets no closer counts.
    const float offCourse = remaining > 0.0f ? WrapAngle(std::atan2(toWaypoint.zMeters, toWaypoint.xMeters) - ship.headingRadians) : 0.0f;
    if (std::abs(offCourse) > STALL_HEADING_RADIANS || remaining < ship.closestMeters - PROGRESS_METERS)
    {
      ship.closestMeters = remaining;
      ship.stalledTicks = 0;
    }
    else
      ++ship.stalledTicks;

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
        if (ship.order != ShipOrder::Attack && ship.order != ShipOrder::Work)
          ship.order = ShipOrder::None;
      }
    }
    else
      ship.position = ship.position + PlaneVector{std::cos(ship.headingRadians), std::sin(ship.headingRadians)} * step;
  }
}

// Ships do not collide, but they do not overlap either (design §9): each overlapping pair is pushed apart along the line
// between them, in identifier order. Two that both move, or both stand idle, part half each. A ship standing to fire
// keeps its range (design §7, decided by the owner on 2026-10-01): pushed by a ship that is still moving up, it gives way
// only sideways round its target, never toward it or away, and the mover takes the rest. A group closing on an enemy so
// spreads its front into an arc at range, and its rear ships find their own places on it rather than shoving the front
// into the enemy. An idle ship holds its ground against a moving one, which slides round it.
void Outpost::Simulation::SeparateShips()
{
  const auto moving = [](const Entity& _ship)
  { return !_ship.path.empty() && !(_ship.order == ShipOrder::AttackMove && _ship.target.IsValid()); };
  // How far _stander gives way when _mover pushes it _overlap along _push: half of it, sideways round its target. When the
  // push is straight at the target, it steps to the side its identifier picks, so that a ship right behind another is not
  // stuck there.
  const auto giveWay = [this](const Entity& _stander, const Entity& _mover, PlaneVector _push, float _overlap) -> PlaneVector
  {
    const Entity* target = _stander.target.IsValid() ? FindEntity(_stander.target) : nullptr;
    if (target == nullptr)
      return {};
    const PlaneVector toTarget = Normalized(target->position - _stander.position, {1.0f, 0.0f});
    PlaneVector sideways = _push + toTarget * -Dot(_push, toTarget);
    if (Length(sideways) < SIDESTEP_SHARE)
      sideways = Perpendicular(toTarget) * (_stander.id < _mover.id ? 1.0f : -1.0f);
    return Normalized(sideways, {}) * (_overlap / 2.0f);
  };

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
      const PlaneVector direction = Normalized(between, {1.0f, 0.0f});
      const bool firstMoves = moving(first);
      const bool secondMoves = moving(second);
      if (firstMoves && secondMoves)
      {
        first.position = first.position - direction * (overlap / 2.0f);
        second.position = second.position + direction * (overlap / 2.0f);
        continue;
      }
      if (!firstMoves && !secondMoves)
      {
        // Two standing: each gives way half, and one standing to fire only sideways round its target.
        const auto standAside = [&](const Entity& _ship, const Entity& _other, PlaneVector _push)
        { return _ship.target.IsValid() ? giveWay(_ship, _other, _push, overlap) : _push * (overlap / 2.0f); };
        const PlaneVector firstStep = standAside(first, second, direction * -1.0f);
        const PlaneVector secondStep = standAside(second, first, direction);
        first.position = first.position + firstStep;
        second.position = second.position + secondStep;
        continue;
      }
      // One stands and one moves: the stander gives way sideways at most, and the mover takes what is left of the push
      // along the line between them.
      Entity& stander = firstMoves ? second : first;
      Entity& mover = firstMoves ? first : second;
      const PlaneVector push = firstMoves ? direction : direction * -1.0f;
      const PlaneVector step = giveWay(stander, mover, push, overlap);
      stander.position = stander.position + step;
      mover.position = mover.position - push * (overlap - Dot(step, push));
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

std::optional<Outpost::Simulation::Armament> Outpost::Simulation::ArmamentOf(const Entity& _entity) const noexcept
{
  if (_entity.kind == EntityKind::Ship)
  {
    const ShipDesign* design = FindDesign(_entity.design);
    if (design == nullptr)
      return std::nullopt;
    return Armament{.damageHundredths = design->stats.damageHundredths,
                    .fireIntervalSeconds = design->stats.fireIntervalSeconds,
                    .rangeMeters = design->stats.rangeMeters,
                    .splashRadiusMeters = design->stats.splashRadiusMeters,
                    .weapon = design->components.weapon};
  }
  // A structure fires once it is built (design §6).
  if (_entity.kind != EntityKind::Structure || !_entity.structureWeapon.IsValid() || !_entity.IsBuilt() || !m_tuning)
    return std::nullopt;
  const auto gun = std::ranges::find(m_tuning->structureWeapons, _entity.structureWeapon, &StructureWeaponTuning::id);
  if (gun == m_tuning->structureWeapons.end())
    return std::nullopt;
  return Armament{.damageHundredths = gun->damage * HUNDREDTHS,
                  .fireIntervalSeconds = gun->fireIntervalSeconds / EffectsOf(_entity.owner).upgrades.FireRateFactor(gun->id),
                  .rangeMeters = static_cast<float>(gun->rangeMeters)};
}

Outpost::CommandResult Outpost::Simulation::ValidateConstructors(PlayerId _player,
                                                                 const std::vector<EntityId>& _constructors) const noexcept
{
  if (const CommandResult result = ValidateShips(_player, _constructors); result != CommandResult::Applied)
    return result;
  for (const EntityId id : _constructors)
  {
    if (FindEntity(id)->role != ShipRole::Constructor)
      return CommandResult::NotAConstructor;
  }
  return CommandResult::Applied;
}

// A structure stands on the plane with a circular footprint that overlaps nothing and stays inside the map's edge. A
// Mining Rig instead snaps to the free ore asteroid it was placed on or beside, and stands at its center (design §6); on
// a map with territory the asteroid must be in a sector the player holds. A Relay snaps to the node of the sector it was
// placed in, which must be free and adjacent to a sector the player holds (Phase 2 design §4–§6, ADR-056).
Outpost::CommandResult Outpost::Simulation::CheckPlacement(PlayerId _player, StructureKind _kind, float _radiusMeters,
                                                           PlanePosition& _position) const
{
  if (!IsFinite(_position))
    return CommandResult::InvalidPosition;
  const Sector* relaySector = nullptr;
  if (_kind == StructureKind::Relay)
  {
    relaySector = SectorAt(_position);
    if (relaySector == nullptr)
      return CommandResult::InvalidPlacement;
    _position = relaySector->placement.node;
  }
  if (_kind == StructureKind::MiningRig)
  {
    const Entity* nearest = nullptr;
    float nearestGap = RIG_SNAP_METERS;
    for (const Entity& asteroid : m_entities)
    {
      if (asteroid.kind != EntityKind::Asteroid)
        continue;
      const float gap = Distance(asteroid.position, _position) - asteroid.radiusMeters;
      if (gap <= nearestGap)
      {
        nearest = &asteroid;
        nearestGap = gap;
      }
    }
    if (nearest == nullptr)
      return CommandResult::InvalidPlacement;
    const bool taken = std::ranges::any_of(
      m_entities, [nearest](const Entity& _entity)
      { return _entity.kind == EntityKind::Structure && _entity.structure == StructureKind::MiningRig && _entity.site == nearest->id; });
    if (taken)
      return CommandResult::InvalidPlacement;
    if (HasTerritory())
    {
      const Sector* sector = SectorAt(nearest->position);
      if (sector == nullptr || sector->holder != _player)
        return CommandResult::SectorNotHeld;
    }
    _position = nearest->position;
    return CommandResult::Applied;
  }

  if (m_mapHalfSizeMeters > 0.0f && (std::abs(_position.xMeters) + _radiusMeters > m_mapHalfSizeMeters ||
                                     std::abs(_position.zMeters) + _radiusMeters > m_mapHalfSizeMeters))
    return CommandResult::InvalidPlacement;
  // A Shipyard stands only in a sector its player holds, so that production follows territory (Phase 4 design §6).
  if (_kind == StructureKind::Shipyard && HasTerritory())
  {
    const Sector* sector = SectorAt(_position);
    if (sector == nullptr || sector->holder != _player)
      return CommandResult::SectorNotHeld;
  }
  for (const Entity& other : m_entities)
  {
    const bool blocks =
      other.kind == EntityKind::Asteroid || other.kind == EntityKind::AsteroidField || other.kind == EntityKind::Structure;
    if (blocks && Distance(other.position, _position) < other.radiusMeters + _radiusMeters)
      return CommandResult::InvalidPlacement;
  }
  // A Relay's node is free now, since nothing stands on it; it must be next to the player's territory (Phase 2 design §6).
  if (relaySector != nullptr && std::ranges::none_of(relaySector->placement.adjacent,
                                                     [this, _player](std::int32_t _id)
                                                     {
                                                       const Sector* adjacent = SectorById(_id);
                                                       return adjacent != nullptr && adjacent->holder == _player;
                                                     }))
    return CommandResult::NotAdjacent;
  return CommandResult::Applied;
}

// The structure is paid for and its site placed at once, blocking from then on; the Constructors head for it and build it
// (design §5, §6).
Outpost::CommandResult Outpost::Simulation::Apply(PlayerId _player, const BuildStructureCommand& _build)
{
  if (const CommandResult result = ValidateConstructors(_player, _build.constructors); result != CommandResult::Applied)
    return result;
  const StructureTuning* tuning = StructureTuningFor(_build.structure);
  if (tuning == nullptr || !tuning->cost.has_value() || !tuning->buildConstructorSeconds.has_value())
    return CommandResult::NotBuildable;
  const std::int32_t costOre = tuning->cost.value_or(0);
  const double buildSeconds = tuning->buildConstructorSeconds.value_or(0.0);
  if (_build.structure == StructureKind::ResearchLab &&
      std::ranges::any_of(
        m_entities, [_player](const Entity& _entity)
        { return _entity.kind == EntityKind::Structure && _entity.structure == StructureKind::ResearchLab && _entity.owner == _player; }))
    return CommandResult::LimitReached;

  // A Relay holds a sector, and a map without them has none to hold (ADR-056).
  if (_build.structure == StructureKind::Relay && !HasTerritory())
    return CommandResult::NotBuildable;

  const auto radius = static_cast<float>(tuning->footprintRadiusMeters);
  PlanePosition position = _build.position;
  if (const CommandResult result = CheckPlacement(_player, _build.structure, radius, position); result != CommandResult::Applied)
    return result;
  // A Relay past the nodes the player's Command Station lets it hold is refused; one under construction counts, as it
  // takes its node (Phase 3 design §7).
  if (const std::int32_t cap = NodeCapOf(_player); _build.structure == StructureKind::Relay && cap > 0 && NodesTaken(_player) >= cap)
    return CommandResult::CapReached;
  PlayerState* player = FindPlayer(_player);
  const std::int64_t cost = std::int64_t{costOre} * HUNDREDTHS;
  if (player == nullptr || player->oreHundredths < cost)
    return CommandResult::NotEnoughOre;
  player->oreHundredths -= cost;

  const std::int32_t hitPoints = StructureHitPoints(_player, *tuning);
  const EntityId id = SpawnStructure(_player, _build.structure, position, radius, hitPoints, tuning->armor * HUNDREDTHS);
  Entity& structure = *FindMutableEntity(id);
  structure.buildWorkNeeded = static_cast<std::int32_t>(std::llround(buildSeconds * m_ticksPerSecond * MILLITICKS_PER_TICK));
  structure.hitPointsHundredths = hitPoints / SITE_STARTING_HIT_POINTS_DIVISOR;
  OrderWork(_build.constructors, id);
  return CommandResult::Applied;
}

Outpost::CommandResult Outpost::Simulation::Apply(PlayerId _player, const RepairCommand& _repair)
{
  if (const CommandResult result = ValidateConstructors(_player, _repair.constructors); result != CommandResult::Applied)
    return result;
  const Entity* target = FindEntity(_repair.target);
  if (target == nullptr)
    return CommandResult::UnknownTarget;
  const bool needsWork = !target->IsBuilt() || target->hitPointsHundredths < target->maxHitPointsHundredths;
  const bool own = target->owner == _player && (target->kind == EntityKind::Ship || target->kind == EntityKind::Structure);
  if (!own || target->maxHitPointsHundredths <= 0 || !needsWork ||
      std::ranges::find(_repair.constructors, _repair.target) != _repair.constructors.end())
    return CommandResult::NotRepairable;
  OrderWork(_repair.constructors, _repair.target);
  return CommandResult::Applied;
}

// A job joins the back of the queue; it is paid for when it reaches the front and starts (Produce).
Outpost::CommandResult Outpost::Simulation::Apply(PlayerId _player, const QueueShipCommand& _queue)
{
  Entity* producer = FindMutableEntity(_queue.producer);
  if (producer == nullptr)
    return CommandResult::UnknownEntity;
  const bool producerKind = producer->structure == StructureKind::Shipyard || producer->structure == StructureKind::CommandStation;
  if (producer->kind != EntityKind::Structure || !producerKind || producer->owner != _player || !producer->IsBuilt())
    return CommandResult::NotAProducer;
  JobView job{.role = ShipRole::Constructor};
  if (producer->structure == StructureKind::Shipyard)
  {
    const ShipDesign* design = FindDesign(_queue.design);
    if (design == nullptr || design->owner != _player)
      return CommandResult::UnknownDesign;
    // A Shipyard builds the hulls of its level and below (Phase 3 design §5).
    if (producer->level < ShipyardLevelFor(*m_tuning, design->components.hull))
      return CommandResult::LevelTooLow;
    job = {.role = ShipRole::Warship, .design = _queue.design};
  }
  if (producer->queue.size() >= QUEUE_LIMIT)
    return CommandResult::QueueFull;
  producer->queue.push_back(job);
  return CommandResult::Applied;
}

// The upgrade is paid for now and builds itself over the level's time (BuildLevels); the structure keeps working
// meanwhile (Phase 3 design §4, ADR-064).
Outpost::CommandResult Outpost::Simulation::Apply(PlayerId _player, const UpgradeStructureCommand& _upgrade)
{
  Entity* structure = FindMutableEntity(_upgrade.structure);
  if (structure == nullptr)
    return CommandResult::UnknownEntity;
  const StructureTuning* tuning = StructureTuningFor(structure->structure);
  if (structure->kind != EntityKind::Structure || structure->owner != _player || tuning == nullptr)
    return CommandResult::NotUpgradable;
  if (!structure->IsBuilt())
    return CommandResult::UnderConstruction;
  if (structure->IsUpgrading())
    return CommandResult::AlreadyUpgrading;
  if (structure->level >= tuning->TopLevel())
    return CommandResult::TopLevel;
  const StructureLevelTuning& next = tuning->levels[static_cast<size_t>(structure->level - 1)];
  // A Research Lab's level may need topics researched first (Phase 3 design §6).
  const std::span<const ResearchTopicId> researched = Researched(_player);
  if (!std::ranges::all_of(next.prerequisites,
                           [&researched](ResearchTopicId _topic) { return std::ranges::find(researched, _topic) != researched.end(); }))
    return CommandResult::PrerequisiteMissing;
  PlayerState* player = FindPlayer(_player);
  const std::int64_t cost = std::int64_t{next.cost} * HUNDREDTHS;
  if (player == nullptr || player->oreHundredths < cost)
    return CommandResult::NotEnoughOre;
  player->oreHundredths -= cost;
  structure->upgradeWorkDone = 0;
  structure->upgradeWorkNeeded =
    std::max(1, static_cast<std::int32_t>(std::llround(next.buildSeconds * m_ticksPerSecond * MILLITICKS_PER_TICK)));
  return CommandResult::Applied;
}

void Outpost::Simulation::OrderWork(const std::vector<EntityId>& _constructors, EntityId _target)
{
  const Entity& target = *FindEntity(_target);
  PlaneVector sum{};
  float widestRadius = 0.0f;
  for (const EntityId id : _constructors)
  {
    const Entity* ship = FindEntity(id);
    sum = sum + (ship->position - PlanePosition{});
    widestRadius = std::max(widestRadius, ship->radiusMeters);
  }
  GroupRoutes routes(m_pathfinder, target.position, widestRadius);
  if (_constructors.size() > 1)
  {
    const ObservedPart part(m_observer, TickPart::GroupRoute);
    routes.SearchFrom(PlanePosition{} + sum * (1.0f / static_cast<float>(_constructors.size())));
  }
  const ObservedPart shipPaths(m_observer, TickPart::ShipPaths);
  for (const EntityId id : _constructors)
  {
    Entity& ship = *FindMutableEntity(id);
    ship.order = ShipOrder::Work;
    ship.workTarget = _target;
    ship.attackTarget = {};
    ship.destination.reset();
    GroupRoutes::Way way = routes.PathFor(ship.position, target.position, ship.radiusMeters);
    ship.path = std::move(way.waypoints);
    ship.laneEnd = way.laneEnd;
    ship.cruiseSpeedMetersPerSecond = ship.speedMetersPerSecond;
    ship.closestMeters = std::numeric_limits<float>::infinity();
    ship.stalledTicks = 0;
    ship.chasedPosition = target.position;
    ship.chaseTick = m_tick + m_ticksPerSecond;
  }
}

void Outpost::Simulation::UpdateObstacles()
{
  // The map's obstacles, then the structures in identifier order. A structure placed is the newest, so its placement only
  // adds an obstacle after the others, which extends the path graphs rather than dropping them (ADR-054).
  std::vector<Obstacle> obstacles = m_mapObstacles;
  for (const Entity& entity : m_entities)
  {
    if (entity.kind == EntityKind::Structure && entity.structure != StructureKind::MiningRig)
      obstacles.push_back({entity.position, entity.radiusMeters});
  }
  const size_t before = m_pathfinder.Obstacles().size();
  m_pathfinder.SetObstacles(std::move(obstacles), m_mapHalfSizeMeters);
  if (m_pathfinder.Obstacles().size() <= before)
    return;
  // A new structure may stand across a ship's way: a ship whose next leg it now blocks looks for another way to the end of
  // its path.
  for (Entity& ship : m_entities)
  {
    if (ship.kind != EntityKind::Ship || ship.path.empty())
      continue;
    PlanePosition from = ship.position;
    bool blocked = false;
    for (const PlanePosition waypoint : ship.path)
    {
      if (!m_pathfinder.IsStraightPathClear(from, waypoint, ship.radiusMeters))
      {
        blocked = true;
        break;
      }
      from = waypoint;
    }
    if (blocked)
    {
      ship.path = m_pathfinder.FindPath(ship.position, ship.path.back(), ship.radiusMeters);
      ship.laneEnd.reset();
      ship.closestMeters = std::numeric_limits<float>::infinity();
      ship.stalledTicks = 0;
    }
  }
}

namespace
{
// Whether a Constructor is close enough to work on its target (ADR-016).
bool IsInReach(const Outpost::Entity& _constructor, const Outpost::Entity& _target) noexcept
{
  return Outpost::Distance(_constructor.position, _target.position) <= _constructor.radiusMeters + _target.radiusMeters + WORK_REACH_METERS;
}
} // namespace

// A Constructor on a work order stands while its target is in reach, and otherwise heads for it, pathing again when the
// target has moved, as a repaired ship may, at most once a second.
void Outpost::Simulation::ApproachWork()
{
  for (Entity& ship : m_entities)
  {
    if (ship.order != ShipOrder::Work)
      continue;
    const Entity& target = *FindEntity(ship.workTarget);
    if (IsInReach(ship, target))
    {
      ship.path.clear();
      continue;
    }
    const bool moved = Distance(target.position, ship.chasedPosition) > CHASE_REPATH_METERS && m_tick >= ship.chaseTick;
    if (!ship.path.empty() && !moved)
      continue;
    ship.path = m_pathfinder.FindPath(ship.position, target.position, ship.radiusMeters);
    ship.laneEnd.reset();
    ship.cruiseSpeedMetersPerSecond = ship.speedMetersPerSecond;
    ship.closestMeters = std::numeric_limits<float>::infinity();
    ship.stalledTicks = 0;
    ship.chasedPosition = target.position;
    ship.chaseTick = m_tick + m_ticksPerSecond;
  }
}

// Constructors in reach of their target build it while it is under construction, and repair it once it is built (design
// §6, gate G8). Building: one Constructor does a tick's work each tick, and each further one adds the tuning data's
// share of one more; a site's hit points rise with its progress from a tenth to full. Repair: each Constructor restores
// the tuning data's percentage of the target's maximum each second, for nothing. A finished job ends the order. A level
// being built is no work of theirs (BuildLevels).
void Outpost::Simulation::Work()
{
  std::vector<std::pair<EntityId, std::int32_t>> crews;
  for (const Entity& ship : m_entities)
  {
    if (ship.order != ShipOrder::Work || !IsInReach(ship, *FindEntity(ship.workTarget)))
      continue;
    const auto crew = std::ranges::find(crews, ship.workTarget, &std::pair<EntityId, std::int32_t>::first);
    if (crew != crews.end())
      ++crew->second;
    else
      crews.emplace_back(ship.workTarget, 1);
  }
  // In identifier order, so that the result does not depend on which Constructor came first.
  std::ranges::sort(crews);

  const ConstructorTuning& tuning = m_tuning->constructor;
  std::vector<EntityId> finished;
  for (const auto& [id, constructors] : crews)
  {
    Entity& target = *FindMutableEntity(id);
    // A player's Constructors build and repair only its own, so the target's owner's research sets their rate.
    const double rate = EffectsOf(target.owner).upgrades.constructorRateFactor;
    const auto work = static_cast<std::int32_t>(
      std::llround(MILLITICKS_PER_TICK * (1.0 + (tuning.extraConstructorBuildShare * static_cast<double>(constructors - 1))) * rate));
    if (!target.IsBuilt())
    {
      const std::int64_t maximum = target.maxHitPointsHundredths;
      const std::int64_t starting = maximum / SITE_STARTING_HIT_POINTS_DIVISOR;
      const auto granted = [&](std::int64_t _done) { return starting + ((maximum - starting) * _done / target.buildWorkNeeded); };
      const std::int32_t done = std::min(target.buildWorkNeeded, target.buildWorkDone + work);
      target.hitPointsHundredths += static_cast<std::int32_t>(granted(done) - granted(target.buildWorkDone));
      target.buildWorkDone = done;
      if (target.IsBuilt() && target.hitPointsHundredths >= target.maxHitPointsHundredths)
        finished.push_back(id);
      continue;
    }
    const auto perConstructor = static_cast<std::int32_t>(
      std::llround(static_cast<double>(target.maxHitPointsHundredths) * tuning.repairPercentPerSecond / 100.0 / m_ticksPerSecond * rate));
    target.hitPointsHundredths = std::min(target.maxHitPointsHundredths, target.hitPointsHundredths + (perConstructor * constructors));
    if (target.hitPointsHundredths >= target.maxHitPointsHundredths)
      finished.push_back(id);
  }
  for (Entity& ship : m_entities)
  {
    if (ship.order == ShipOrder::Work && std::ranges::find(finished, ship.workTarget) != finished.end())
    {
      ship.order = ShipOrder::None;
      ship.workTarget = {};
      ship.path.clear();
    }
  }
}

// A structure being upgraded builds its next level itself, a tick of the level's time each tick, which no Constructor
// and no research shortens (owner, 2026-10-04). The level lands whole when it is in, and the structure keeps the share
// of its hit points it had (Phase 3 design §4).
void Outpost::Simulation::BuildLevels()
{
  for (Entity& structure : m_entities)
  {
    if (!structure.IsUpgrading())
      continue;
    structure.upgradeWorkDone = std::min(structure.upgradeWorkNeeded, structure.upgradeWorkDone + MILLITICKS_PER_TICK);
    if (structure.upgradeWorkDone < structure.upgradeWorkNeeded)
      continue;
    structure.upgradeWorkDone = 0;
    structure.upgradeWorkNeeded = 0;
    ++structure.level;
    if (const StructureTuning* tuning = StructureTuningFor(structure.structure))
      RescaleHitPoints(structure, StructureHitPoints(structure.owner, *tuning, structure.level));
  }
}

// Each built Shipyard and Command Station works on the front of its queue. A job starts, and is paid for, once the
// player has the Ore (design §5), and a warship's once it fits under its player's fleet cap (Phase 4 design §5); until
// then it waits. A finished ship appears beside its producer, on the side facing
// the map's center.
void Outpost::Simulation::Produce()
{
  // A Shipyard finished since the last tick, built or placed whole, takes its owner's next number, in identifier order.
  for (Entity& yard : m_entities)
  {
    if (yard.kind != EntityKind::Structure || yard.structure != StructureKind::Shipyard || yard.shipyardNumber != 0 || !yard.IsBuilt())
      continue;
    if (PlayerState* player = FindPlayer(yard.owner))
      yard.shipyardNumber = ++player->shipyardsFinished;
  }

  // Ships made this tick join the entities after the loop, which must not see the vector grow under it.
  struct Delivery
  {
    PlayerId owner;
    JobView job;
    PlanePosition producerPosition;
    float producerRadiusMeters = 0.0f;
  };
  std::vector<Delivery> deliveries;
  // A warship finished earlier in this loop has left its producer's job and is not yet an entity, so its points are counted
  // from the deliveries.
  const auto delivered = [this, &deliveries](PlayerId _owner)
  {
    std::int32_t points = 0;
    for (const Delivery& delivery : deliveries)
    {
      const ShipDesign* design = delivery.job.role == ShipRole::Warship ? FindDesign(delivery.job.design) : nullptr;
      if (delivery.owner == _owner && design != nullptr)
        points += CommandPointsOfHull(design->components.hull);
    }
    return points;
  };
  for (Entity& producer : m_entities)
  {
    if (producer.kind != EntityKind::Structure || producer.queue.empty() || !producer.IsBuilt())
      continue;
    const JobView job = producer.queue.front();
    if (producer.jobWorkNeeded == 0)
    {
      std::int32_t cost = m_tuning->constructor.cost;
      double seconds = m_tuning->constructor.buildSeconds;
      if (job.role == ShipRole::Warship)
      {
        const ShipDesign* design = FindDesign(job.design);
        if (design == nullptr)
        {
          producer.queue.erase(producer.queue.begin());
          continue;
        }
        cost = design->stats.cost;
        seconds = design->stats.buildSeconds;
        const std::int32_t cap = FleetCapOf(producer.owner);
        if (cap > 0 && CommandPointsOf(producer.owner) + delivered(producer.owner) + CommandPointsOfHull(design->components.hull) > cap)
          continue;
      }
      PlayerState* player = FindPlayer(producer.owner);
      if (player == nullptr || player->oreHundredths < std::int64_t{cost} * HUNDREDTHS)
        continue;
      player->oreHundredths -= std::int64_t{cost} * HUNDREDTHS;
      producer.jobWorkNeeded = std::max(1, static_cast<std::int32_t>(std::llround(seconds * m_ticksPerSecond * MILLITICKS_PER_TICK)));
      producer.jobWorkDone = 0;
    }
    // Automated Shipyards speed a Shipyard's work, the job under way included (design §8); not the Command Station's.
    producer.jobWorkDone +=
      producer.structure == StructureKind::Shipyard
        ? static_cast<std::int32_t>(std::llround(MILLITICKS_PER_TICK * EffectsOf(producer.owner).upgrades.shipyardBuildSpeedFactor))
        : MILLITICKS_PER_TICK;
    if (producer.jobWorkDone < producer.jobWorkNeeded)
      continue;
    deliveries.push_back({producer.owner, job, producer.position, producer.radiusMeters});
    if (job.role == ShipRole::Warship)
      ++producer.shipsBuilt;
    producer.queue.erase(producer.queue.begin());
    producer.jobWorkDone = 0;
    producer.jobWorkNeeded = 0;
  }

  for (const Delivery& delivery : deliveries)
  {
    const PlaneVector out = Normalized(PlanePosition{} - delivery.producerPosition, {1.0f, 0.0f});
    const float heading = std::atan2(out.zMeters, out.xMeters);
    const float radius = delivery.job.role == ShipRole::Constructor ? static_cast<float>(m_tuning->constructor.footprintRadiusMeters)
                                                                    : FindDesign(delivery.job.design)->stats.movement.radiusMeters;
    const PlanePosition position =
      m_pathfinder.Clear(delivery.producerPosition + out * (delivery.producerRadiusMeters + radius + SPAWN_GAP_METERS), radius);
    if (delivery.job.role == ShipRole::Constructor)
      (void)SpawnConstructor(delivery.owner, position, heading);
    else
      (void)SpawnShip(delivery.owner, delivery.job.design, position, heading);
  }
}

// Each built Mining Rig adds its asteroid's income to its owner's Ore every tick (design §5), with the owner's research
// applied (design §8). The income is counted per second in hundredths and paid a tick's share at a time, the remainder
// carried, so that a rate that is not a whole number of hundredths a tick is still paid in full (ADR-017).
std::int64_t Outpost::Simulation::IncomeHundredthsPerSecond(PlayerId _player) const
{
  const double factor = EffectsOf(_player).upgrades.miningIncomeFactor;
  std::int64_t income = 0;
  for (const Entity& rig : m_entities)
  {
    if (rig.kind == EntityKind::Structure && rig.structure == StructureKind::MiningRig && rig.owner == _player && rig.site.IsValid() &&
        rig.IsBuilt())
      income += RigIncomeHundredthsPerSecond(rig, factor);
  }
  return income;
}

std::int64_t Outpost::Simulation::RigIncomeHundredthsPerSecond(const Entity& _rig, double _incomeFactor) const
{
  const RulesTuning& rules = m_tuning->rules;
  const double rate = _rig.oreYield == OreYield::Home        ? rules.miningRigOrePerSecondHome
                      : _rig.oreYield == OreYield::Near      ? rules.miningRigOrePerSecondNear
                      : _rig.oreYield == OreYield::Contested ? rules.miningRigOrePerSecondContested
                                                             : rules.miningRigOrePerSecondRich;
  const Entity* asteroid = FindEntity(_rig.site);
  const bool dry = asteroid != nullptr && asteroid->oreReserveHundredths == 0;
  const double share = dry ? m_tuning->rules.exhaustedYieldPercent / 100.0 : 1.0;
  return std::llround(rate * HUNDREDTHS * _incomeFactor * share * TerritoryShare(_rig));
}

const Outpost::Simulation::Sector* Outpost::Simulation::SectorAt(PlanePosition _position) const noexcept
{
  const auto found = std::ranges::find_if(m_sectors, [_position](const Sector& _sector) { return _sector.placement.Contains(_position); });
  return found != m_sectors.end() ? &*found : nullptr;
}

const Outpost::Simulation::Sector* Outpost::Simulation::SectorById(std::int32_t _id) const noexcept
{
  const auto found = std::ranges::find_if(m_sectors, [_id](const Sector& _sector) { return _sector.placement.id == _id; });
  return found != m_sectors.end() ? &*found : nullptr;
}

// A sector is held by the owner of the finished Relay or the Command Station on its node; a site under construction holds
// nothing yet. A Relay is suppressed while an enemy warship is within the tuning data's radius of it and none of its
// owner's is; a Command Station never is. A held sector is linked when its holder holds a path of adjacent sectors to one
// of its home sectors, the ones its Command Station holds, suppressed ones included; one that is not is cut off (Phase 2
// design §4–§6, ADR-056).
void Outpost::Simulation::UpdateTerritory()
{
  if (!HasTerritory())
    return;
  const auto suppressionRadius = static_cast<float>(m_tuning->territory.suppressionRadiusMeters);
  for (Sector& sector : m_sectors)
  {
    sector.holder = {};
    sector.home = false;
    sector.suppressed = false;
    sector.cutOff = false;
    const auto onNode = std::ranges::find_if(m_entities,
                                             [&sector](const Entity& _entity)
                                             {
                                               const bool holds = _entity.structure == StructureKind::CommandStation ||
                                                                  _entity.structure == StructureKind::Relay;
                                               return _entity.kind == EntityKind::Structure && holds && _entity.owner.IsValid() &&
                                                      _entity.IsBuilt() &&
                                                      Distance(_entity.position, sector.placement.node) <= NODE_REACH_METERS;
                                             });
    if (onNode == m_entities.end())
      continue;
    sector.holder = onNode->owner;
    sector.home = onNode->structure == StructureKind::CommandStation;
    if (sector.home)
      continue;
    bool enemyNear = false;
    bool ownNear = false;
    for (const Entity& ship : m_entities)
    {
      if (ship.kind != EntityKind::Ship || ship.role != ShipRole::Warship || ship.maxHitPointsHundredths <= 0 || !ship.owner.IsValid() ||
          !IsInRange(ship, *onNode, suppressionRadius))
        continue;
      (ship.owner == sector.holder ? ownNear : enemyNear) = true;
    }
    sector.suppressed = enemyNear && !ownNear;
  }

  // Outward from each home sector through the sectors its holder holds.
  std::vector<std::uint8_t> linked(m_sectors.size(), 0);
  std::vector<size_t> frontier;
  for (size_t index = 0; index < m_sectors.size(); ++index)
  {
    if (m_sectors[index].home)
    {
      linked[index] = 1;
      frontier.push_back(index);
    }
  }
  while (!frontier.empty())
  {
    const size_t from = frontier.back();
    frontier.pop_back();
    for (const std::int32_t id : m_sectors[from].placement.adjacent)
    {
      const auto next = std::ranges::find_if(m_sectors, [id](const Sector& _sector) { return _sector.placement.id == id; });
      if (next == m_sectors.end())
        continue;
      const auto index = static_cast<size_t>(next - m_sectors.begin());
      if (linked[index] != 0 || next->holder != m_sectors[from].holder)
        continue;
      linked[index] = 1;
      frontier.push_back(index);
    }
  }
  for (size_t index = 0; index < m_sectors.size(); ++index)
    m_sectors[index].cutOff = m_sectors[index].holder.IsValid() && linked[index] == 0;
}

std::int32_t Outpost::Simulation::NodeCapOf(PlayerId _player) const noexcept
{
  if (!m_tuning)
    return 0;
  const auto station = std::ranges::find_if(
    m_entities, [_player](const Entity& _entity)
    { return _entity.kind == EntityKind::Structure && _entity.structure == StructureKind::CommandStation && _entity.owner == _player; });
  return NodeCap(*m_tuning, station != m_entities.end() ? station->level : 1);
}

std::int32_t Outpost::Simulation::FleetCapOf(PlayerId _player) const noexcept
{
  if (!m_tuning)
    return 0;
  const auto station = std::ranges::find_if(
    m_entities, [_player](const Entity& _entity)
    { return _entity.kind == EntityKind::Structure && _entity.structure == StructureKind::CommandStation && _entity.owner == _player; });
  return FleetCap(*m_tuning, station != m_entities.end() ? station->level : 1);
}

std::int32_t Outpost::Simulation::CommandPointsOf(PlayerId _player) const noexcept
{
  std::int32_t points = 0;
  for (const Entity& entity : m_entities)
  {
    if (entity.owner != _player)
      continue;
    if (entity.kind == EntityKind::Ship && entity.role == ShipRole::Warship)
      points += CommandPointsOfHull(entity.hull);
    else if (entity.kind == EntityKind::Structure && entity.jobWorkNeeded > 0 && !entity.queue.empty() &&
             entity.queue.front().role == ShipRole::Warship)
    {
      if (const ShipDesign* design = FindDesign(entity.queue.front().design))
        points += CommandPointsOfHull(design->components.hull);
    }
  }
  return points;
}

std::int32_t Outpost::Simulation::CommandPointsOfHull(HullId _hull) const noexcept
{
  if (!m_tuning)
    return 0;
  const auto hull = std::ranges::find(m_tuning->hulls, _hull, &HullTuning::id);
  return hull != m_tuning->hulls.end() ? hull->commandPoints : 0;
}

std::int32_t Outpost::Simulation::NodesTaken(PlayerId _player) const noexcept
{
  const auto held = std::ranges::count(m_sectors, _player, &Sector::holder);
  const auto sites = std::ranges::count_if(m_entities,
                                           [_player](const Entity& _entity)
                                           {
                                             return _entity.kind == EntityKind::Structure && _entity.structure == StructureKind::Relay &&
                                                    _entity.owner == _player && !_entity.IsBuilt();
                                           });
  return static_cast<std::int32_t>(held + sites);
}

double Outpost::Simulation::TerritoryShare(const Entity& _rig) const noexcept
{
  if (!HasTerritory())
    return 1.0;
  const Sector* sector = SectorAt(_rig.position);
  if (sector == nullptr || sector->holder != _rig.owner || sector->suppressed)
    return 0.0;
  return sector->cutOff ? m_tuning->territory.cutOffIncomePercent / 100.0 : 1.0;
}

bool Outpost::Simulation::InSectorSight(PlayerId _player, PlanePosition _position) const noexcept
{
  if (!HasTerritory())
    return false;
  const Sector* sector = SectorAt(_position);
  return sector != nullptr && sector->holder == _player && !sector->suppressed;
}

std::optional<std::int64_t> Outpost::Simulation::ReserveKnownTo(PlayerId _player, EntityId _asteroid) const
{
  const Entity* asteroid = FindEntity(_asteroid);
  if (asteroid == nullptr || !asteroid->oreReserveHundredths.has_value())
    return std::nullopt;
  if (!m_fog)
    return asteroid->oreReserveHundredths;
  const PlayerState* state = FindPlayer(_player);
  if (state == nullptr)
    return std::nullopt;
  const auto known = std::ranges::find(state->knownReserves, _asteroid, &std::pair<EntityId, std::int64_t>::first);
  return known != state->knownReserves.end() ? std::optional<std::int64_t>(known->second) : std::nullopt;
}

void Outpost::Simulation::Mine()
{
  for (PlayerState& player : m_players)
  {
    player.oreRemainder += IncomeHundredthsPerSecond(player.id);
    player.oreHundredths += player.oreRemainder / m_ticksPerSecond;
    player.oreRemainder %= m_ticksPerSecond;
  }

  // Each built rig draws what it earns from its asteroid's reserve, so Improved Extraction drains it faster, and a player
  // with Deep Core Survey draws less for the same Ore (Phase 1 design §8). The trickle of a dry asteroid draws nothing,
  // and the last tick's draw takes what was left.
  for (Entity& rig : m_entities)
  {
    if (rig.kind != EntityKind::Structure || rig.structure != StructureKind::MiningRig || !rig.site.IsValid() || !rig.IsBuilt())
      continue;
    Entity* asteroid = FindMutableEntity(rig.site);
    if (asteroid == nullptr || !asteroid->oreReserveHundredths.has_value() || *asteroid->oreReserveHundredths == 0)
      continue;
    const Upgrades& upgrades = EffectsOf(rig.owner).upgrades;
    rig.reserveRemainder +=
      std::llround(static_cast<double>(RigIncomeHundredthsPerSecond(rig, upgrades.miningIncomeFactor)) / upgrades.oreReserveFactor);
    asteroid->oreReserveHundredths = std::max<std::int64_t>(0, *asteroid->oreReserveHundredths - (rig.reserveRemainder / m_ticksPerSecond));
    rig.reserveRemainder %= m_ticksPerSecond;
  }
}

// Each built Research Lab works on the front of its queue, and from the level that gives it a second slot on the topic
// behind it too (Phase 3 design §6). A topic starts, and is paid for, once the player has the Ore (design §5), and the
// second once the first has started and what it requires is researched; until then it waits. A finished topic applies at
// once (design §8).
void Outpost::Simulation::Research()
{
  std::vector<std::pair<PlayerId, ResearchTopicId>> finished;
  for (Entity& lab : m_entities)
  {
    if (lab.kind != EntityKind::Structure || lab.researchQueue.empty() || !lab.IsBuilt())
      continue;
    PlayerState* player = FindPlayer(lab.owner);
    if (player == nullptr)
      continue;
    // The front topic, and from the level that gives a second slot the one behind it, once what it requires is researched,
    // not only queued ahead of it (Phase 3 design §6). Each is paid for when it starts.
    const std::int32_t slots =
      std::min<std::int32_t>(ResearchSlots(*m_tuning, lab.level), static_cast<std::int32_t>(lab.researchQueue.size()));
    const auto work = [&](std::size_t _slot) -> std::pair<std::int32_t&, std::int32_t&>
    {
      return _slot == 0 ? std::pair<std::int32_t&, std::int32_t&>{lab.jobWorkDone, lab.jobWorkNeeded}
                        : std::pair<std::int32_t&, std::int32_t&>{lab.secondJobWorkDone, lab.secondJobWorkNeeded};
    };
    std::vector<std::size_t> done;
    for (std::size_t slot = 0; std::cmp_less(slot, slots); ++slot)
    {
      auto [workDone, workNeeded] = work(slot);
      const auto topic = std::ranges::find(m_tuning->research, lab.researchQueue[slot], &ResearchTopicTuning::id);
      if (workNeeded == 0)
      {
        if (slot > 0 && lab.jobWorkNeeded == 0)
          break;
        const bool ready = std::ranges::all_of(topic->prerequisites, [player](ResearchTopicId _id)
                                               { return std::ranges::find(player->researched, _id) != player->researched.end(); });
        if (!ready || player->oreHundredths < std::int64_t{topic->cost} * HUNDREDTHS)
          continue;
        player->oreHundredths -= std::int64_t{topic->cost} * HUNDREDTHS;
        workNeeded = std::max(1, static_cast<std::int32_t>(std::llround(topic->researchSeconds * m_ticksPerSecond * MILLITICKS_PER_TICK)));
        workDone = 0;
      }
      workDone += MILLITICKS_PER_TICK;
      if (workDone >= workNeeded)
        done.push_back(slot);
    }
    // The finished leave the queue, the second's progress moving to the front when the first is done.
    for (auto slot = done.rbegin(); slot != done.rend(); ++slot)
    {
      finished.emplace_back(lab.owner, lab.researchQueue[*slot]);
      lab.researchQueue.erase(lab.researchQueue.begin() + static_cast<std::ptrdiff_t>(*slot));
      if (*slot == 0)
      {
        lab.jobWorkDone = std::exchange(lab.secondJobWorkDone, 0);
        lab.jobWorkNeeded = std::exchange(lab.secondJobWorkNeeded, 0);
      }
      else
      {
        lab.secondJobWorkDone = 0;
        lab.secondJobWorkNeeded = 0;
      }
    }
  }
  for (const auto& [player, topic] : finished)
  {
    if (PlayerState* state = FindPlayer(player))
      CompleteResearch(*state, topic);
  }
}

void Outpost::Simulation::CompleteResearch(PlayerState& _player, ResearchTopicId _topic)
{
  const double structuresBefore = _player.researchEffects.upgrades.structureHitPointsFactor;
  _player.researched.push_back(_topic);
  _player.researchEffects = EffectsFrom(*m_tuning, _player.researched);
  const Upgrades& upgrades = _player.researchEffects.upgrades;
  for (ShipDesign& design : m_designs)
  {
    if (design.owner == _player.id)
      design.stats = DesignStatsFor(*m_tuning, design.components, upgrades);
  }
  // A ship or structure keeps the share of its hit points it had, so an undamaged one gains the whole upgrade (owner,
  // 2026-10-01). A ship's speed follows its design's, and a Constructor's the rule's; a move under way keeps its pace.
  for (Entity& entity : m_entities)
  {
    if (entity.owner != _player.id)
      continue;
    if (entity.kind == EntityKind::Ship && entity.role == ShipRole::Warship)
    {
      const ShipDesign* design = FindDesign(entity.design);
      if (design == nullptr)
        continue;
      RescaleHitPoints(entity, design->stats.hitPointsHundredths);
      entity.speedMetersPerSecond = design->stats.movement.speedMetersPerSecond;
    }
    else if (entity.kind == EntityKind::Ship && entity.role == ShipRole::Constructor)
      entity.speedMetersPerSecond = ConstructorSpeed(_player.id);
    else if (entity.kind == EntityKind::Structure)
    {
      // Only a structure with the hit points its kind had before is upgraded: one placed with numbers of its own, as a
      // test or a measurement load places them, keeps them.
      const StructureTuning* tuning = StructureTuningFor(entity.structure);
      if (tuning != nullptr && entity.maxHitPointsHundredths == StructureHitPointsAt(*tuning, structuresBefore, entity.level))
        RescaleHitPoints(entity, StructureHitPoints(_player.id, *tuning, entity.level));
    }
  }
}

void Outpost::Simulation::RescaleHitPoints(Entity& _entity, std::int64_t _after) noexcept
{
  const std::int64_t before = _entity.maxHitPointsHundredths;
  if (before <= 0 || before == _after)
    return;
  _entity.hitPointsHundredths =
    static_cast<std::int32_t>(std::max<std::int64_t>(1, ((_entity.hitPointsHundredths * _after) + (before / 2)) / before));
  _entity.maxHitPointsHundredths = static_cast<std::int32_t>(_after);
}

std::int32_t Outpost::Simulation::StructureHitPointsAt(const StructureTuning& _tuning, double _researchFactor, std::int32_t _level) const
{
  const double levels = m_tuning->rules.levelHitPointsPercent * static_cast<double>(_level - 1) / 100.0;
  return static_cast<std::int32_t>(std::llround(_tuning.hitPoints * HUNDREDTHS * (_researchFactor + levels)));
}

std::int32_t Outpost::Simulation::StructureHitPoints(PlayerId _owner, const StructureTuning& _tuning, std::int32_t _level) const
{
  return StructureHitPointsAt(_tuning, EffectsOf(_owner).upgrades.structureHitPointsFactor, _level);
}

float Outpost::Simulation::ConstructorSpeed(PlayerId _owner) const
{
  return static_cast<float>(m_tuning->constructor.speedMetersPerSecond * EffectsOf(_owner).upgrades.shipSpeedFactor);
}

// A topic joins the back of the lab's queue; it is paid for when it reaches the front and starts (Research). It may
// follow its prerequisites there, since one lab researches them in order (design §8).
Outpost::CommandResult Outpost::Simulation::Apply(PlayerId _player, const StartResearchCommand& _research)
{
  Entity* lab = FindMutableEntity(_research.lab);
  if (lab == nullptr)
    return CommandResult::UnknownEntity;
  if (lab->kind != EntityKind::Structure || lab->structure != StructureKind::ResearchLab || lab->owner != _player || !lab->IsBuilt())
    return CommandResult::NotALab;
  const auto topic = std::ranges::find(m_tuning->research, _research.topic, &ResearchTopicTuning::id);
  if (topic == m_tuning->research.end())
    return CommandResult::UnknownTopic;
  const std::span<const ResearchTopicId> researched = Researched(_player);
  const auto known = [&](ResearchTopicId _id)
  {
    return std::ranges::find(researched, _id) != researched.end() || std::ranges::find(lab->researchQueue, _id) != lab->researchQueue.end();
  };
  if (known(topic->id))
    return CommandResult::AlreadyResearched;
  if (!std::ranges::all_of(topic->prerequisites, known))
    return CommandResult::PrerequisiteMissing;
  // A tier is researched only once the Lab's level has opened it (Phase 3 design §6).
  if (lab->level < LabLevelFor(*m_tuning, topic->tier))
    return CommandResult::LevelTooLow;
  if (lab->researchQueue.size() >= QUEUE_LIMIT)
    return CommandResult::QueueFull;
  lab->researchQueue.push_back(topic->id);
  return CommandResult::Applied;
}

std::vector<Outpost::EntityId> Outpost::Simulation::WarshipsOf(const std::vector<EntityId>& _ships) const
{
  std::vector<EntityId> warships;
  for (const EntityId id : _ships)
  {
    if (FindEntity(id)->role == ShipRole::Warship && std::ranges::find(warships, id) == warships.end())
      warships.push_back(id);
  }
  return warships;
}

// Phase 2 design §9: the warships hold the sector that holds the point. They go to its node, answer any enemy ship they
// see in the sector, and go back to the node once none is left (KeepStandingOrders, ADR-059).
Outpost::CommandResult Outpost::Simulation::Apply(PlayerId _player, const HoldSectorCommand& _hold)
{
  if (const CommandResult result = ValidateShips(_player, _hold.ships); result != CommandResult::Applied)
    return result;
  if (!IsFinite(_hold.position))
    return CommandResult::InvalidPosition;
  const std::vector<EntityId> warships = WarshipsOf(_hold.ships);
  if (warships.empty())
    return CommandResult::NoShips;
  const Sector* sector = HasTerritory() ? SectorAt(_hold.position) : nullptr;
  if (sector == nullptr)
    return CommandResult::NoSector;
  const std::uint32_t group = ++m_lastStandingGroup;
  for (const EntityId id : warships)
  {
    Entity& ship = *FindMutableEntity(id);
    ship.standing = StandingOrder::HoldSector;
    ship.standingGroup = group;
    ship.holdSector = sector->placement.id;
    ship.standingFrom = sector->placement.node;
  }
  return OrderMove(_player, warships, sector->placement.node, ShipOrder::AttackMove);
}

// Phase 2 design §9: the warships attack-move to the point, then back to the middle of where they were ordered from, and
// so on (KeepStandingOrders, ADR-059).
Outpost::CommandResult Outpost::Simulation::Apply(PlayerId _player, const PatrolCommand& _patrol)
{
  if (const CommandResult result = ValidateShips(_player, _patrol.ships); result != CommandResult::Applied)
    return result;
  if (!IsFinite(_patrol.destination))
    return CommandResult::InvalidPosition;
  const std::vector<EntityId> warships = WarshipsOf(_patrol.ships);
  if (warships.empty())
    return CommandResult::NoShips;
  PlaneVector sum{};
  for (const EntityId id : warships)
    sum = sum + (FindEntity(id)->position - PlanePosition{});
  const PlanePosition from = PlanePosition{} + sum * (1.0f / static_cast<float>(warships.size()));
  const std::uint32_t group = ++m_lastStandingGroup;
  for (const EntityId id : warships)
  {
    Entity& ship = *FindMutableEntity(id);
    ship.standing = StandingOrder::Patrol;
    ship.standingGroup = group;
    ship.standingFrom = from;
    ship.standingTo = _patrol.destination;
    ship.standingOutward = true;
  }
  return OrderMove(_player, warships, _patrol.destination, ShipOrder::AttackMove);
}

// Once a second, each group on a standing order, in the order it was given, acts as one (ADR-059). A group holding a
// sector attack-moves on the nearest enemy ship its player sees in the sector, unless it is already headed there or
// firing, and once none is left and it stands idle away from the sector's node, attack-moves back to it. A patrolling
// group, once all of it stands idle at one end, attack-moves to the other. A group still planning its paths waits.
void Outpost::Simulation::KeepStandingOrders()
{
  if (m_tick % m_ticksPerSecond != 0)
    return;
  std::vector<std::uint32_t> groups;
  for (const Entity& ship : m_entities)
  {
    if (ship.standing != StandingOrder::None && std::ranges::find(groups, ship.standingGroup) == groups.end())
      groups.push_back(ship.standingGroup);
  }
  std::ranges::sort(groups);
  const auto planning = [this](EntityId _ship)
  {
    return std::ranges::any_of(m_plannedOrders, [_ship](const PlannedOrder& _order)
                               { return std::ranges::find(_order.members, _ship, &PlannedOrder::Member::ship) != _order.members.end(); });
  };
  for (const std::uint32_t group : groups)
  {
    std::vector<EntityId> ships;
    PlaneVector sum{};
    for (const Entity& ship : m_entities)
    {
      if (ship.standing != StandingOrder::None && ship.standingGroup == group)
      {
        ships.push_back(ship.id);
        sum = sum + (ship.position - PlanePosition{});
      }
    }
    if (std::ranges::any_of(ships, planning))
      continue;
    const Entity& first = *FindEntity(ships.front());
    const PlanePosition center = PlanePosition{} + sum * (1.0f / static_cast<float>(ships.size()));
    const bool idle = std::ranges::all_of(ships,
                                          [this](EntityId _id)
                                          {
                                            const Entity& ship = *FindEntity(_id);
                                            return ship.order == ShipOrder::None && ship.path.empty();
                                          });
    if (first.standing == StandingOrder::Patrol)
    {
      if (!idle)
        continue;
      const bool outward = !first.standingOutward;
      for (const EntityId id : ships)
        FindMutableEntity(id)->standingOutward = outward;
      (void)OrderMove(first.owner, ships, outward ? first.standingTo : first.standingFrom, ShipOrder::AttackMove);
      continue;
    }

    const Sector* sector = SectorById(first.holdSector);
    if (sector == nullptr)
      continue;
    const Entity* enemy = nullptr;
    for (const Entity& other : m_entities)
    {
      if (other.kind != EntityKind::Ship || !other.owner.IsValid() || other.owner == first.owner || other.maxHitPointsHundredths <= 0 ||
          !sector->placement.Contains(other.position) || !Sees(first.owner, other))
        continue;
      if (enemy == nullptr || Distance(other.position, center) < Distance(enemy->position, center))
        enemy = &other;
    }
    if (enemy != nullptr)
    {
      const bool engaged =
        std::ranges::any_of(ships,
                            [&](EntityId _id)
                            {
                              const Entity& ship = *FindEntity(_id);
                              return ship.target.IsValid() || (ship.order == ShipOrder::AttackMove && ship.destination.has_value() &&
                                                               Distance(*ship.destination, enemy->position) <= STANDING_RESEND_METERS);
                            });
      if (!engaged)
        (void)OrderMove(first.owner, ships, enemy->position, ShipOrder::AttackMove);
    }
    else if (idle && Distance(center, sector->placement.node) > HOLD_RETURN_METERS)
      (void)OrderMove(first.owner, ships, sector->placement.node, ShipOrder::AttackMove);
  }
}

// A new design is of components the player has, and is not one it already has; a saved one is renamed (ADR-017).
Outpost::CommandResult Outpost::Simulation::Apply(PlayerId _player, const SaveDesignCommand& _save)
{
  if (!IsValidDesignName(_save.nameUtf8))
    return CommandResult::InvalidName;
  const DesignComponents components{_save.hull, _save.drive, _save.weapon, _save.module};
  const bool known =
    std::ranges::find(m_tuning->hulls, components.hull, &HullTuning::id) != m_tuning->hulls.end() &&
    std::ranges::find(m_tuning->drives, components.drive, &DriveTuning::id) != m_tuning->drives.end() &&
    std::ranges::find(m_tuning->weapons, components.weapon, &WeaponTuning::id) != m_tuning->weapons.end() &&
    (!components.module.IsValid() || std::ranges::find(m_tuning->modules, components.module, &ModuleTuning::id) != m_tuning->modules.end());
  if (!known)
    return CommandResult::UnknownComponent;

  if (_save.design.IsValid())
  {
    const auto design = std::ranges::lower_bound(m_designs, _save.design, {}, &ShipDesign::id);
    if (design == m_designs.end() || design->id != _save.design || design->owner != _player)
      return CommandResult::UnknownDesign;
    if (design->components != components)
      return CommandResult::ComponentsFixed;
    design->name = _save.nameUtf8;
    return CommandResult::Applied;
  }

  const std::span<const ResearchTopicId> researched = Researched(_player);
  if (!IsAvailable(*m_tuning, researched, components.hull) || !IsAvailable(*m_tuning, researched, components.drive) ||
      !IsAvailable(*m_tuning, researched, components.weapon))
    return CommandResult::ComponentLocked;
  if (FindDesign(_player, components) != nullptr)
    return CommandResult::DuplicateDesign;
  (void)SaveDesign(_player, _save.nameUtf8, components, DesignStatsFor(*m_tuning, components, EffectsOf(_player).upgrades));
  return CommandResult::Applied;
}

// A player whose base was placed and who has neither a Command Station nor a finished Shipyard has lost (Phase 1 design
// §4, ADR-037). The match ends then, once: the world runs on, but the outcome stands (owner, 2026-10-01).
void Outpost::Simulation::DecideMatch()
{
  if (m_matchOver || m_basePlayers.empty())
    return;
  std::vector<PlayerId> standing;
  for (const PlayerId player : m_basePlayers)
  {
    const bool producing = std::ranges::any_of(m_entities,
                                               [player](const Entity& _entity)
                                               {
                                                 const bool production =
                                                   _entity.structure == StructureKind::CommandStation ||
                                                   (_entity.structure == StructureKind::Shipyard && _entity.IsBuilt());
                                                 return _entity.kind == EntityKind::Structure && _entity.owner == player && production;
                                               });
    if (producing)
      standing.push_back(player);
  }
  if (standing.size() < m_basePlayers.size())
    EndMatch(standing, MatchEnding::LostProduction);
}

// Phase 2 design §8: every drain interval, a player that holds fewer nodes than the most any player holds loses the
// tuning data's tickets for each node it is behind, divided by the map's nodes; a suppressed Relay counts for its owner,
// and a free node for no one. Tickets are kept in shares, as many to a ticket as the map has nodes, so the drain is whole
// and a lead of one node on nine drains a side in exactly 50 minutes (ADR-057). A player out of tickets loses.
void Outpost::Simulation::Dominate()
{
  if (m_matchOver || m_basePlayers.empty() || !HasTerritory())
    return;
  const auto interval =
    std::max<std::uint64_t>(1, static_cast<std::uint64_t>(std::llround(m_tuning->territory.drainIntervalSeconds * m_ticksPerSecond)));
  if ((m_tick + 1) % interval != 0)
    return;
  std::vector<std::int64_t> nodes;
  nodes.reserve(m_basePlayers.size());
  for (const PlayerId player : m_basePlayers)
    nodes.push_back(std::ranges::count(m_sectors, player, &Sector::holder));
  const std::int64_t most = std::ranges::max(nodes);
  std::vector<PlayerId> standing;
  for (size_t index = 0; index < m_basePlayers.size(); ++index)
  {
    PlayerState* player = FindPlayer(m_basePlayers[index]);
    if (player != nullptr)
    {
      const std::int64_t drain = std::int64_t{m_tuning->territory.drainTicketsPerNodeDifference} * (most - nodes[index]);
      player->ticketShares = std::max<std::int64_t>(0, player->ticketShares - drain);
    }
    if (player == nullptr || player->ticketShares > 0)
      standing.push_back(m_basePlayers[index]);
  }
  if (standing.size() < m_basePlayers.size())
    EndMatch(standing, MatchEnding::Domination);
}

void Outpost::Simulation::EndMatch(const std::vector<PlayerId>& _standing, MatchEnding _ending)
{
  m_matchOver = true;
  m_winner = _standing.size() == 1 ? _standing.front() : PlayerId{};
  m_matchEndedTick = m_tick + 1;
  m_ending = _ending;
}

std::int64_t Outpost::Simulation::TicketShares(PlayerId _player) const noexcept
{
  const PlayerState* player = FindPlayer(_player);
  return player != nullptr ? player->ticketShares : 0;
}

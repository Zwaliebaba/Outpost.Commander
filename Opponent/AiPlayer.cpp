#include "pch.h"
#include "AiPlayer.h"

namespace
{
using Outpost::EntityId;
using Outpost::EntityKind;
using Outpost::EntityView;
using Outpost::PlanePosition;
using Outpost::Snapshot;
using Outpost::StructureKind;

// A build order takes at most this many idle Constructors, and leaves the rest for the next order.
constexpr size_t CONSTRUCTORS_PER_BUILD = 2;
// How long the AI waits to see a site it ordered before it takes the order as refused: the server says nothing of a
// refusal (ADR-002), but a site it places is in the next snapshot.
constexpr std::uint64_t SITE_WAIT_SECONDS = 3;
// How long it waits to see a design it saved before it saves it again.
constexpr std::uint64_t DESIGN_WAIT_SECONDS = 5;
// A structure stands where it was ordered, and a Mining Rig on its asteroid's center; this allows for float rounding.
constexpr float SAME_PLACE_METERS = 1.0f;
// The search for a free place: rings this far apart around the preferred one, out to this many.
constexpr float SEARCH_STEP_METERS = 20.0f;
constexpr int SEARCH_RINGS = 20;
// The clear circle the reserve gathers in.
constexpr float RALLY_RADIUS_METERS = 40.0f;
// A reserve warship is sent again only when where it should be has moved this far.
constexpr float RESEND_METERS = 50.0f;

float Distance(PlanePosition _a, PlanePosition _b) noexcept
{
  return std::hypot(_a.xMeters - _b.xMeters, _a.zMeters - _b.zMeters);
}

PlanePosition Along(PlanePosition _from, float _directionX, float _directionZ, float _meters) noexcept
{
  return {.xMeters = _from.xMeters + (_directionX * _meters), .zMeters = _from.zMeters + (_directionZ * _meters)};
}

const EntityView* FindEntity(const Snapshot& _snapshot, EntityId _id) noexcept
{
  const auto found = std::ranges::find(_snapshot.entities, _id, &EntityView::id);
  return found != _snapshot.entities.end() ? &*found : nullptr;
}

bool IsStructure(const EntityView& _entity, StructureKind _kind) noexcept
{
  return _entity.kind == EntityKind::Structure && _entity.structure == _kind;
}

bool IsBuilt(const EntityView& _entity) noexcept
{
  return _entity.builtPermille >= Outpost::PERMILLE;
}

const Outpost::StructureTypeView* FindType(const Snapshot& _snapshot, StructureKind _kind) noexcept
{
  const auto found = std::ranges::find(_snapshot.structureTypes, _kind, &Outpost::StructureTypeView::structure);
  return found != _snapshot.structureTypes.end() ? &*found : nullptr;
}

// The nearest place to _preferred, in rings around it, where a structure of _type keeps _gapMeters clear of every blocker
// and of the map's edge.
std::optional<PlanePosition> FindPlace(const Outpost::StructureTypeView& _type, PlanePosition _preferred,
                                       std::span<const EntityView> _blockers, float _mapSizeMeters, float _gapMeters)
{
  Outpost::StructureTypeView padded = _type;
  padded.radiusMeters += _gapMeters;
  for (int ring = 0; ring <= SEARCH_RINGS; ++ring)
  {
    const int samples = ring == 0 ? 1 : 8 * ring;
    for (int sample = 0; sample < samples; ++sample)
    {
      const float angle = 2.0f * std::numbers::pi_v<float> * static_cast<float>(sample) / static_cast<float>(samples);
      const float meters = static_cast<float>(ring) * SEARCH_STEP_METERS;
      const PlanePosition place = Along(_preferred, std::cos(angle), std::sin(angle), meters);
      if (Outpost::PlaceGhost(padded, place, _blockers, _mapSizeMeters).valid)
        return place;
    }
  }
  return std::nullopt;
}

template <typename Order> Outpost::Command MakeCommand(Outpost::PlayerId _player, Order _order)
{
  return {.player = _player, .order = std::move(_order)};
}

bool IsAvailable(const Snapshot& _snapshot, const Outpost::DesignComponents& _design)
{
  const auto hull = std::ranges::find(_snapshot.hulls, _design.hull, &Outpost::HullView::id);
  const auto drive = std::ranges::find(_snapshot.drives, _design.drive, &Outpost::DriveView::id);
  const auto weapon = std::ranges::find(_snapshot.weapons, _design.weapon, &Outpost::WeaponView::id);
  return hull != _snapshot.hulls.end() && hull->available && drive != _snapshot.drives.end() && drive->available &&
         weapon != _snapshot.weapons.end() && weapon->available;
}

// "Medium+Ion+Mass Driver", as the starting designs are named, or the identifiers when that is not a valid name.
std::string DesignName(const Snapshot& _snapshot, const Outpost::DesignComponents& _design)
{
  const auto hull = std::ranges::find(_snapshot.hulls, _design.hull, &Outpost::HullView::id);
  const auto drive = std::ranges::find(_snapshot.drives, _design.drive, &Outpost::DriveView::id);
  const auto weapon = std::ranges::find(_snapshot.weapons, _design.weapon, &Outpost::WeaponView::id);
  if (hull != _snapshot.hulls.end() && drive != _snapshot.drives.end() && weapon != _snapshot.weapons.end())
  {
    std::string name = std::format("{}+{}+{}", hull->nameUtf8, drive->nameUtf8, weapon->nameUtf8);
    if (Outpost::IsValidDesignName(name))
      return name;
  }
  return std::format("Design {}-{}-{}", _design.hull.value, _design.drive.value, _design.weapon.value);
}

auto ComponentOrder(const Outpost::DesignComponents& _design) noexcept
{
  return std::tuple(_design.hull, _design.drive, _design.weapon);
}
} // namespace

Outpost::DesignComponents Outpost::ChooseAnswer(const AiSettings& _settings, const Snapshot& _snapshot)
{
  std::vector<DesignComponents> warships;
  for (const EntityView& ship : _snapshot.entities)
  {
    if (ship.kind == EntityKind::Ship && ship.role == ShipRole::Warship && ship.owner.IsValid() && ship.owner != _snapshot.player)
      warships.push_back({.hull = ship.hull, .drive = ship.drive, .weapon = ship.weapon});
  }
  return ChooseAnswer(_settings, warships, _snapshot);
}

Outpost::DesignComponents Outpost::ChooseAnswer(const AiSettings& _settings, std::span<const DesignComponents> _enemyWarships,
                                                const Snapshot& _snapshot)
{
  std::vector<std::pair<DesignComponents, size_t>> fleet;
  for (const DesignComponents& design : _enemyWarships)
  {
    const auto counted = std::ranges::find(fleet, design, &std::pair<DesignComponents, size_t>::first);
    if (counted != fleet.end())
      ++counted->second;
    else
      fleet.emplace_back(design, 1);
  }
  if (fleet.empty())
    return _settings.defaultDesign;

  const auto most = std::ranges::min_element(fleet,
                                             [](const auto& _a, const auto& _b)
                                             {
                                               if (_a.second != _b.second)
                                                 return _a.second > _b.second;
                                               return ComponentOrder(_a.first) < ComponentOrder(_b.first);
                                             });
  const auto rule = std::ranges::find_if(_settings.counters, [&](const CounterRule& _rule)
                                         { return _rule.enemy == most->first && IsAvailable(_snapshot, _rule.answer); });
  return rule != _settings.counters.end() ? rule->answer : _settings.defaultDesign;
}

Outpost::AiPlayer::AiPlayer(AiSettings _settings, std::uint32_t _ticksPerSecond)
  : m_settings(std::move(_settings)),
    m_ticksPerSecond(_ticksPerSecond),
    m_productionDesign(m_settings.defaultDesign)
{
  if (m_ticksPerSecond == 0)
    throw Neuron::Exception("AiPlayer: the server runs no ticks");
}

std::vector<Outpost::Command> Outpost::AiPlayer::Update(const Snapshot& _snapshot)
{
  m_player = _snapshot.player;
  Watch(_snapshot);
  std::vector<Command> orders;
  if (_snapshot.tick >= m_nextDecisionTick)
  {
    m_nextDecisionTick = _snapshot.tick + m_ticksPerSecond;
    Decide(_snapshot, orders);
  }
  return orders;
}

// Every tick, since a shot is in the snapshot of its tick only. It defends every structure of its, built or a site (owner,
// 2026-10-01). Those of the tick before are remembered, since the shot that destroys one names a structure that has
// already left the snapshot.
void Outpost::AiPlayer::Watch(const Snapshot& _snapshot)
{
  std::vector<std::pair<EntityId, PlanePosition>> structures;
  for (const EntityView& entity : _snapshot.entities)
  {
    if (entity.kind == EntityKind::Structure && entity.owner == m_player)
      structures.emplace_back(entity.id, entity.position);
    else if (entity.kind == EntityKind::Ship && entity.role == ShipRole::Warship && entity.owner.IsValid() && entity.owner != m_player)
      m_seenWarships[entity.id] = {.hull = entity.hull, .drive = entity.drive, .weapon = entity.weapon};
  }
  for (const ShotView& shot : _snapshot.shots)
  {
    for (const auto* known : {&structures, &m_structures})
    {
      const auto target = std::ranges::find(*known, shot.target, &std::pair<EntityId, PlanePosition>::first);
      if (target == known->end())
        continue;
      m_lastDefenseShotTick = _snapshot.tick;
      m_defensePosition = target->second;
      break;
    }
  }
  m_structures = std::move(structures);
}

void Outpost::AiPlayer::Decide(const Snapshot& _snapshot, std::vector<Command>& _orders)
{
  const auto station = std::ranges::find_if(_snapshot.entities, [this](const EntityView& _entity)
                                            { return IsStructure(_entity, StructureKind::CommandStation) && _entity.owner == m_player; });
  if (station == _snapshot.entities.end())
    return;
  if (!m_planned)
    Plan(_snapshot, *station);
  PlanShipyards(_snapshot, *station);
  if (_snapshot.tick >= m_nextReviewTick)
  {
    // What it has seen since the last review; with nothing seen it keeps the design it has.
    if (!m_seenWarships.empty())
    {
      std::vector<DesignComponents> seen;
      seen.reserve(m_seenWarships.size());
      for (const auto& [id, design] : m_seenWarships)
        seen.push_back(design);
      m_productionDesign = ChooseAnswer(m_settings, seen, _snapshot);
      m_seenWarships.clear();
    }
    m_nextReviewTick = _snapshot.tick + static_cast<std::uint64_t>(std::llround(m_settings.reviewIntervalSeconds * m_ticksPerSecond));
  }

  // Constructors: the ones it has and the ones it has queued.
  const auto constructors = std::ranges::count_if(
    _snapshot.entities, [this](const EntityView& _entity)
    { return _entity.kind == EntityKind::Ship && _entity.role == ShipRole::Constructor && _entity.owner == m_player; });
  const auto queued = std::ranges::count(station->queue, ShipRole::Constructor, &JobView::role);
  if (constructors + queued < m_settings.constructors && station->queue.size() < QUEUE_LIMIT)
    _orders.push_back(MakeCommand(m_player, QueueShipCommand{.producer = station->id, .design = {}}));

  // Research, one topic at a time, in its order.
  const auto lab = std::ranges::find_if(_snapshot.entities, [this](const EntityView& _entity)
                                        { return IsStructure(_entity, StructureKind::ResearchLab) && _entity.owner == m_player; });
  if (lab != _snapshot.entities.end() && IsBuilt(*lab) && lab->research.empty())
  {
    const auto researched = [&_snapshot](ResearchTopicId _id)
    {
      const auto topic = std::ranges::find(_snapshot.research, _id, &ResearchTopicView::id);
      return topic != _snapshot.research.end() && topic->researched;
    };
    for (const ResearchTopicId id : m_settings.researchOrder)
    {
      const auto topic = std::ranges::find(_snapshot.research, id, &ResearchTopicView::id);
      if (topic == _snapshot.research.end() || topic->researched || !std::ranges::all_of(topic->prerequisites, researched))
        continue;
      _orders.push_back(MakeCommand(m_player, StartResearchCommand{.lab = lab->id, .topic = id}));
      break;
    }
  }

  std::vector<EntityId> idle;
  TendWork(_snapshot, idle, _orders);
  const bool structureWaiting = Build(_snapshot, idle, _orders);

  // Constructors left over repair what is damaged, one each.
  for (const EntityView& structure : _snapshot.entities)
  {
    if (idle.empty())
      break;
    const bool damaged = structure.kind == EntityKind::Structure && structure.owner == m_player && IsBuilt(structure) &&
                         structure.hitPointsHundredths < structure.maxHitPointsHundredths;
    if (!damaged || std::ranges::any_of(m_work, [&structure](const Work& _work) { return _work.target == structure.id; }))
      continue;
    const EntityId constructor = idle.back();
    idle.pop_back();
    _orders.push_back(MakeCommand(m_player, RepairCommand{.constructors = {constructor}, .target = structure.id}));
    m_work.push_back({.constructors = {constructor}, .target = structure.id, .slot = std::nullopt, .orderedTick = _snapshot.tick});
  }

  Produce(_snapshot, structureWaiting, _orders);
  CommandFleet(_snapshot, _orders);
}

// The plan of the base (owner, 2026-10-01): rigs on the home asteroids; a Shipyard, a Research Lab and a Defence Platform
// beside the Command Station; rigs on the contested asteroids nearest it, each with a platform beside it on the side of
// the base. Each place is chosen once, here, and searched again only if something has taken it by the time the structure
// is ordered. More Shipyards join the plan as the income grows (PlanShipyards).
void Outpost::AiPlayer::Plan(const Snapshot& _snapshot, const EntityView& _station)
{
  m_planned = true;
  const PlanePosition home = _station.position;
  m_home = home;
  const float fromCenter = std::hypot(home.xMeters, home.zMeters);
  // Toward the map's center, and across that to the right seen from above.
  const float forwardX = fromCenter > 0.0f ? -home.xMeters / fromCenter : 1.0f;
  const float forwardZ = fromCenter > 0.0f ? -home.zMeters / fromCenter : 0.0f;
  const float acrossX = forwardZ;
  const float acrossZ = -forwardX;
  const auto gap = static_cast<float>(m_settings.structureGapMeters);

  std::vector<const EntityView*> asteroids;
  std::vector<PlanePosition> enemyStations;
  for (const EntityView& entity : _snapshot.entities)
  {
    if (entity.kind == EntityKind::Asteroid)
      asteroids.push_back(&entity);
    else if (IsStructure(entity, StructureKind::CommandStation) && entity.owner != m_player)
      enemyStations.push_back(entity.position);
  }
  // Under fog of war the enemy's base is out of sight at the start (ADR-024): the map is point-symmetric, so it is across
  // the center from this one.
  if (enemyStations.empty())
    enemyStations.push_back({.xMeters = -home.xMeters, .zMeters = -home.zMeters});
  std::ranges::sort(asteroids,
                    [home](const EntityView* _a, const EntityView* _b)
                    {
                      const float a = Distance(_a->position, home);
                      const float b = Distance(_b->position, home);
                      return a != b ? a < b : _a->id < _b->id;
                    });
  std::vector<const EntityView*> homeAsteroids;
  std::vector<const EntityView*> contestedAsteroids;
  for (const EntityView* asteroid : asteroids)
  {
    if (std::cmp_less(homeAsteroids.size(), m_settings.homeAsteroids))
    {
      homeAsteroids.push_back(asteroid);
      continue;
    }
    if (std::cmp_greater_equal(contestedAsteroids.size(), m_settings.contestedAsteroids))
      break;
    // Not one nearer another player's base: that is the other player's home.
    const float ours = Distance(asteroid->position, home);
    if (std::ranges::none_of(enemyStations, [&](PlanePosition _enemy) { return Distance(asteroid->position, _enemy) < ours; }))
      contestedAsteroids.push_back(asteroid);
  }

  const auto addRig = [&](const EntityView& _asteroid)
  {
    const StructureTypeView* type = FindType(_snapshot, StructureKind::MiningRig);
    const float radius = type != nullptr ? std::max(type->radiusMeters, _asteroid.radiusMeters) : _asteroid.radiusMeters;
    m_slots.push_back({.structure = StructureKind::MiningRig,
                       .position = _asteroid.position,
                       .radiusMeters = radius,
                       .asteroid = _asteroid.id,
                       .abandoned = type == nullptr || !type->buildable});
  };
  // A structure placed as near _preferred as it can be, clear of everything and of the structures planned before it.
  const auto addStructure = [&](StructureKind _kind, const auto& _preferredFor, std::optional<size_t> _besideRig, std::int32_t _income)
  {
    const StructureTypeView* type = FindType(_snapshot, _kind);
    Slot slot{.structure = _kind, .besideRig = _besideRig, .minimumIncomeHundredthsPerSecond = _income};
    if (type == nullptr || !type->buildable)
    {
      slot.abandoned = true;
      m_slots.push_back(slot);
      return;
    }
    slot.radiusMeters = type->radiusMeters;
    const PlanePosition preferred = _preferredFor(type->radiusMeters);
    const std::optional<PlanePosition> place = FindPlace(*type, preferred, Blockers(_snapshot, std::nullopt), _snapshot.mapSizeMeters, gap);
    slot.position = place.value_or(preferred);
    slot.abandoned = !place.has_value();
    m_slots.push_back(slot);
  };

  for (const EntityView* asteroid : homeAsteroids)
    addRig(*asteroid);
  const float stationRadius = _station.radiusMeters;
  addStructure(
    StructureKind::Shipyard, [&](float _radius) { return Along(home, acrossX, acrossZ, stationRadius + _radius + gap); }, std::nullopt, 0);
  addStructure(
    StructureKind::ResearchLab, [&](float _radius) { return Along(home, -acrossX, -acrossZ, stationRadius + _radius + gap); }, std::nullopt,
    0);
  addStructure(
    StructureKind::DefensePlatform, [&](float _radius) { return Along(home, forwardX, forwardZ, stationRadius + _radius + gap); },
    std::nullopt, 0);
  for (const EntityView* asteroid : contestedAsteroids)
  {
    addRig(*asteroid);
    const size_t rig = m_slots.size() - 1;
    const float toHome = std::max(Distance(asteroid->position, home), 1.0f);
    const float towardX = (home.xMeters - asteroid->position.xMeters) / toHome;
    const float towardZ = (home.zMeters - asteroid->position.zMeters) / toHome;
    const float rigRadius = m_slots[rig].radiusMeters;
    addStructure(
      StructureKind::DefensePlatform, [&](float _radius) { return Along(asteroid->position, towardX, towardZ, rigRadius + _radius + gap); },
      rig, 0);
  }
  const StructureTypeView rally{.structure = StructureKind::Shipyard, .radiusMeters = RALLY_RADIUS_METERS};
  const PlanePosition preferred = Along(home, forwardX, forwardZ, static_cast<float>(m_settings.rallyDistanceMeters));
  m_rally = FindPlace(rally, preferred, Blockers(_snapshot, std::nullopt), _snapshot.mapSizeMeters, 0.0f).value_or(preferred);
}

// Owner, 2026-10-01: the AI builds its N-th Shipyard once its income reaches N times the settings' income per Shipyard,
// since one Shipyard spends about as much as that. A Shipyard joins the plan once, and keeps waiting for the income if
// the income drops again, as when a rig is lost. They stand behind the Command Station, then behind it to either side,
// each round of three farther out; the search finds the nearest clear place to each.
void Outpost::AiPlayer::PlanShipyards(const Snapshot& _snapshot, const EntityView& _station)
{
  const auto shipyards = std::ranges::count(m_slots, StructureKind::Shipyard, &Slot::structure);
  const auto income = static_cast<std::int64_t>(std::llround(m_settings.incomePerShipyardOrePerSecond * HUNDREDTHS)) * (shipyards + 1);
  if (shipyards == 0 || _snapshot.oreIncomeHundredthsPerSecond < income)
    return;

  Slot slot{.structure = StructureKind::Shipyard,
            .besideRig = std::nullopt,
            .minimumIncomeHundredthsPerSecond =
              static_cast<std::int32_t>(std::min<std::int64_t>(income, std::numeric_limits<std::int32_t>::max()))};
  const StructureTypeView* type = FindType(_snapshot, StructureKind::Shipyard);
  if (type == nullptr || !type->buildable)
  {
    slot.abandoned = true;
    m_slots.push_back(slot);
    return;
  }
  // Turns from the direction toward the map's center, which the first Shipyard, the lab and the platform stand around.
  constexpr std::array<float, 3> TURNS_RADIANS{std::numbers::pi_v<float>, 0.75f * std::numbers::pi_v<float>,
                                               -0.75f * std::numbers::pi_v<float>};
  const auto extra = static_cast<size_t>(shipyards - 1);
  const PlanePosition home = _station.position;
  const float fromCenter = std::hypot(home.xMeters, home.zMeters);
  const float forwardX = fromCenter > 0.0f ? -home.xMeters / fromCenter : 1.0f;
  const float forwardZ = fromCenter > 0.0f ? -home.zMeters / fromCenter : 0.0f;
  const float turn = TURNS_RADIANS[extra % TURNS_RADIANS.size()];
  const float directionX = (forwardX * std::cos(turn)) - (forwardZ * std::sin(turn));
  const float directionZ = (forwardX * std::sin(turn)) + (forwardZ * std::cos(turn));
  const auto gap = static_cast<float>(m_settings.structureGapMeters);
  const size_t roundIndex = extra / TURNS_RADIANS.size();
  const auto round = static_cast<float>(roundIndex);
  const float meters = _station.radiusMeters + type->radiusMeters + gap + (round * ((2.0f * type->radiusMeters) + gap));
  const PlanePosition preferred = Along(home, directionX, directionZ, meters);

  slot.radiusMeters = type->radiusMeters;
  const std::optional<PlanePosition> place = FindPlace(*type, preferred, Blockers(_snapshot, std::nullopt), _snapshot.mapSizeMeters, gap);
  slot.position = place.value_or(preferred);
  slot.abandoned = !place.has_value();
  m_slots.push_back(slot);
}

bool Outpost::AiPlayer::IsDone(const Slot& _slot, const Snapshot& _snapshot) const
{
  return std::ranges::any_of(_snapshot.entities,
                             [&](const EntityView& _entity)
                             {
                               if (!IsStructure(_entity, _slot.structure) || _entity.owner != m_player)
                                 return false;
                               // A player has one Research Lab, wherever it stands.
                               return _slot.structure == StructureKind::ResearchLab ||
                                      Distance(_entity.position, _slot.position) <= SAME_PLACE_METERS;
                             });
}

bool Outpost::AiPlayer::IsBlocked(const Slot& _slot, const Snapshot& _snapshot) const
{
  if (_slot.abandoned)
    return true;
  if (_slot.besideRig.has_value())
    return IsBlocked(m_slots[*_slot.besideRig], _snapshot);
  if (_slot.structure != StructureKind::MiningRig)
    return false;
  return std::ranges::any_of(_snapshot.entities,
                             [&](const EntityView& _entity)
                             {
                               return IsStructure(_entity, StructureKind::MiningRig) && _entity.owner != m_player &&
                                      Distance(_entity.position, _slot.position) <= SAME_PLACE_METERS;
                             });
}

std::vector<Outpost::EntityView> Outpost::AiPlayer::Blockers(const Snapshot& _snapshot, std::optional<size_t> _skippedSlot) const
{
  std::vector<EntityView> blockers;
  for (const EntityView& entity : _snapshot.entities)
  {
    if (entity.kind != EntityKind::Ship)
      blockers.push_back(entity);
  }
  for (size_t i = 0; i < m_slots.size(); ++i)
  {
    const Slot& slot = m_slots[i];
    // A rig stands on its asteroid, which already blocks.
    if (i == _skippedSlot || slot.abandoned || slot.structure == StructureKind::MiningRig || IsDone(slot, _snapshot))
      continue;
    blockers.push_back(
      {.kind = EntityKind::Structure, .structure = slot.structure, .position = slot.position, .radiusMeters = slot.radiusMeters});
  }
  return blockers;
}

// Keeps track of where its Constructors are, since a snapshot shows no ship's orders: a work ends when its structure is
// finished and at full strength, or gone, or when the site it ordered has not appeared. Sites with nobody on them, such
// as one whose Constructors died, get the idle ones.
void Outpost::AiPlayer::TendWork(const Snapshot& _snapshot, std::vector<EntityId>& _idle, std::vector<Command>& _orders)
{
  const std::uint64_t siteWaitTicks = SITE_WAIT_SECONDS * m_ticksPerSecond;
  std::erase_if(m_work,
                [&](Work& _work)
                {
                  std::erase_if(_work.constructors, [&_snapshot](EntityId _id) { return FindEntity(_snapshot, _id) == nullptr; });
                  if (_work.constructors.empty())
                    return true;
                  if (!_work.target.IsValid() && _work.slot.has_value())
                  {
                    const Slot& slot = m_slots[*_work.slot];
                    const auto site = std::ranges::find_if(_snapshot.entities,
                                                           [&](const EntityView& _entity)
                                                           {
                                                             return IsStructure(_entity, slot.structure) && _entity.owner == m_player &&
                                                                    Distance(_entity.position, slot.position) <= SAME_PLACE_METERS;
                                                           });
                    if (site == _snapshot.entities.end())
                      return _snapshot.tick > _work.orderedTick + siteWaitTicks;
                    _work.target = site->id;
                  }
                  const EntityView* target = FindEntity(_snapshot, _work.target);
                  return target == nullptr || (IsBuilt(*target) && target->hitPointsHundredths >= target->maxHitPointsHundredths);
                });

  for (const EntityView& ship : _snapshot.entities)
  {
    if (ship.kind != EntityKind::Ship || ship.role != ShipRole::Constructor || ship.owner != m_player)
      continue;
    const bool busy = std::ranges::any_of(m_work, [&ship](const Work& _work)
                                          { return std::ranges::find(_work.constructors, ship.id) != _work.constructors.end(); });
    if (!busy)
      _idle.push_back(ship.id);
  }

  for (const EntityView& site : _snapshot.entities)
  {
    if (_idle.empty())
      break;
    if (site.kind != EntityKind::Structure || site.owner != m_player || IsBuilt(site) ||
        std::ranges::any_of(m_work, [&site](const Work& _work) { return _work.target == site.id; }))
      continue;
    Work work{.constructors = {}, .target = site.id, .slot = std::nullopt, .orderedTick = _snapshot.tick};
    while (!_idle.empty() && work.constructors.size() < CONSTRUCTORS_PER_BUILD)
    {
      work.constructors.push_back(_idle.back());
      _idle.pop_back();
    }
    _orders.push_back(MakeCommand(m_player, RepairCommand{.constructors = work.constructors, .target = site.id}));
    m_work.push_back(std::move(work));
  }
}

bool Outpost::AiPlayer::Build(const Snapshot& _snapshot, std::vector<EntityId>& _idle, std::vector<Command>& _orders)
{
  std::int32_t ore = _snapshot.ore;
  const auto gap = static_cast<float>(m_settings.structureGapMeters);
  for (size_t i = 0; i < m_slots.size(); ++i)
  {
    Slot& slot = m_slots[i];
    if (IsBlocked(slot, _snapshot) || IsDone(slot, _snapshot) ||
        _snapshot.oreIncomeHundredthsPerSecond < slot.minimumIncomeHundredthsPerSecond ||
        std::ranges::any_of(m_work, [i](const Work& _work) { return _work.slot == i; }))
      continue;
    const StructureTypeView* type = FindType(_snapshot, slot.structure);
    if (type == nullptr)
      continue;
    if (_idle.empty())
      return false;
    if (ore < type->cost)
      return true;

    if (slot.structure != StructureKind::MiningRig)
    {
      // Something may have taken the place since it was planned.
      StructureTypeView padded = *type;
      padded.radiusMeters += gap;
      const std::vector<EntityView> blockers = Blockers(_snapshot, i);
      if (!PlaceGhost(padded, slot.position, blockers, _snapshot.mapSizeMeters).valid)
      {
        const std::optional<PlanePosition> place = FindPlace(*type, slot.position, blockers, _snapshot.mapSizeMeters, gap);
        if (!place.has_value())
        {
          slot.abandoned = true;
          continue;
        }
        slot.position = *place;
      }
    }

    // The nearest idle Constructors go.
    std::ranges::sort(_idle,
                      [&](EntityId _a, EntityId _b)
                      {
                        const float a = Distance(FindEntity(_snapshot, _a)->position, slot.position);
                        const float b = Distance(FindEntity(_snapshot, _b)->position, slot.position);
                        return a != b ? a < b : _a < _b;
                      });
    const auto crew = static_cast<std::ptrdiff_t>(std::min(CONSTRUCTORS_PER_BUILD, _idle.size()));
    Work work{.constructors = {_idle.begin(), _idle.begin() + crew}, .target = {}, .slot = i, .orderedTick = _snapshot.tick};
    _idle.erase(_idle.begin(), _idle.begin() + crew);
    _orders.push_back(MakeCommand(
      m_player, BuildStructureCommand{.constructors = work.constructors, .structure = slot.structure, .position = slot.position}));
    m_work.push_back(std::move(work));
    ore -= type->cost;
  }
  return false;
}

// Keeps every built Shipyard's queue at the settings' length with the chosen design, saving the design first if the AI
// does not have it. Not while a structure waits for Ore, since a ship is paid for when it starts and would take it.
void Outpost::AiPlayer::Produce(const Snapshot& _snapshot, bool _structureWaiting, std::vector<Command>& _orders)
{
  if (_structureWaiting)
    return;
  const auto design = std::ranges::find_if(_snapshot.designs, [this](const DesignView& _design)
                                           { return DesignComponents{_design.hull, _design.drive, _design.weapon} == m_productionDesign; });
  if (design == _snapshot.designs.end())
  {
    if (!m_designSaveTick.has_value() || _snapshot.tick >= *m_designSaveTick + (DESIGN_WAIT_SECONDS * m_ticksPerSecond))
    {
      _orders.push_back(MakeCommand(m_player, SaveDesignCommand{.design = {},
                                                                .nameUtf8 = DesignName(_snapshot, m_productionDesign),
                                                                .hull = m_productionDesign.hull,
                                                                .drive = m_productionDesign.drive,
                                                                .weapon = m_productionDesign.weapon}));
      m_designSaveTick = _snapshot.tick;
    }
    return;
  }
  for (const EntityView& yard : _snapshot.entities)
  {
    if (!IsStructure(yard, StructureKind::Shipyard) || yard.owner != m_player || !IsBuilt(yard))
      continue;
    for (size_t jobs = yard.queue.size(); std::cmp_less(jobs, m_settings.shipyardQueueJobs); ++jobs)
      _orders.push_back(MakeCommand(m_player, QueueShipCommand{.producer = yard.id, .design = design->id}));
  }
}

// The reserve gathers at the rally, or goes to where the base is under fire; once it is large enough it joins the attack
// group, which attack-moves on the nearest enemy structure and then the next.
void Outpost::AiPlayer::CommandFleet(const Snapshot& _snapshot, std::vector<Command>& _orders)
{
  std::vector<const EntityView*> warships;
  for (const EntityView& ship : _snapshot.entities)
  {
    if (ship.kind == EntityKind::Ship && ship.role == ShipRole::Warship && ship.owner == m_player)
      warships.push_back(&ship);
  }
  std::erase_if(m_attackGroup, [&_snapshot](EntityId _id) { return FindEntity(_snapshot, _id) == nullptr; });
  std::vector<const EntityView*> reserve;
  for (const EntityView* ship : warships)
  {
    if (std::ranges::find(m_attackGroup, ship->id) == m_attackGroup.end())
      reserve.push_back(ship);
  }
  std::erase_if(m_reserveDestinations, [&reserve](const auto& _entry)
                { return std::ranges::none_of(reserve, [&_entry](const EntityView* _ship) { return _ship->id == _entry.first; }); });

  const EntityView* target = m_attackGroup.empty() ? nullptr : FindEntity(_snapshot, m_attackTarget);
  bool grown = false;
  if (std::cmp_greater_equal(reserve.size(), m_settings.attackGroupShips))
  {
    grown = true;
    std::vector<EntityId> joined;
    for (const EntityView* ship : reserve)
    {
      joined.push_back(ship->id);
      m_attackGroup.push_back(ship->id);
      m_reserveDestinations.erase(ship->id);
    }
    reserve.clear();
    if (target != nullptr)
      _orders.push_back(MakeCommand(m_player, AttackMoveCommand{.ships = std::move(joined), .destination = target->position}));
  }

  if (!m_attackGroup.empty() && target == nullptr)
  {
    float sumX = 0.0f;
    float sumZ = 0.0f;
    for (const EntityId id : m_attackGroup)
    {
      const PlanePosition position = FindEntity(_snapshot, id)->position;
      sumX += position.xMeters;
      sumZ += position.zMeters;
    }
    const auto count = static_cast<float>(m_attackGroup.size());
    const PlanePosition center{.xMeters = sumX / count, .zMeters = sumZ / count};
    const EntityView* nearest = nullptr;
    for (const EntityView& entity : _snapshot.entities)
    {
      if (entity.kind != EntityKind::Structure || !entity.owner.IsValid() || entity.owner == m_player)
        continue;
      if (nearest == nullptr || Distance(entity.position, center) < Distance(nearest->position, center))
        nearest = &entity;
    }
    m_attackTarget = nearest != nullptr ? nearest->id : EntityId{};
    if (nearest != nullptr)
      _orders.push_back(MakeCommand(m_player, AttackMoveCommand{.ships = m_attackGroup, .destination = nearest->position}));
    else if (!m_searching || grown)
      _orders.push_back(MakeCommand(
        m_player, AttackMoveCommand{.ships = m_attackGroup, .destination = {.xMeters = -m_home.xMeters, .zMeters = -m_home.zMeters}}));
    m_searching = nearest == nullptr;
  }

  const bool defending =
    m_lastDefenseShotTick.has_value() &&
    _snapshot.tick < *m_lastDefenseShotTick + static_cast<std::uint64_t>(std::llround(m_settings.defenseHoldSeconds * m_ticksPerSecond));
  const PlanePosition destination = defending ? m_defensePosition : m_rally;
  std::vector<EntityId> sent;
  for (const EntityView* ship : reserve)
  {
    const auto last = m_reserveDestinations.find(ship->id);
    if (last != m_reserveDestinations.end() && Distance(last->second, destination) <= RESEND_METERS)
      continue;
    sent.push_back(ship->id);
    m_reserveDestinations[ship->id] = destination;
  }
  if (!sent.empty())
    _orders.push_back(MakeCommand(m_player, AttackMoveCommand{.ships = std::move(sent), .destination = destination}));
}

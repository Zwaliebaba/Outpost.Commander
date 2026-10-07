#include "pch.h"
#include "AiPlayer.h"

namespace
{
using Outpost::EntityId;
using Outpost::EntityKind;
using Outpost::EntityView;
using Outpost::PlanePosition;
using Outpost::SectorView;
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
// An attack-move ends at each ship's place in the group's formation round the target, which for a large group or a
// large structure can be out of the ship's range; a ship of the attack group this close to its target structure is
// ordered to attack it, which closes it into range.
constexpr float CLOSE_IN_METERS = 500.0f;
// The Defence Platforms planned round the base for its Shipyards (task 12.2) stand in rings toward the map's center: the
// first this far from the Command Station, beyond the rally, then each ring this much further out, at these turns from
// the way to the center.
constexpr float HOME_PLATFORM_RING_METERS = 260.0f;
constexpr float HOME_PLATFORM_RING_STEP_METERS = 70.0f;
constexpr std::array<float, 5> HOME_PLATFORM_TURNS_RADIANS{0.0f, 0.5f, -0.5f, 1.0f, -1.0f};
// A scout this close to its waypoint goes on to the next (ADR-020 decision 13).
constexpr float SCOUT_ARRIVED_METERS = 150.0f;
// An enemy sector is raided only when the AI sees no enemy warship this close to its node (ADR-020 decision 13).
constexpr float RAID_GUARD_METERS = 600.0f;

float Distance(PlanePosition _a, PlanePosition _b) noexcept
{
  return std::hypot(_a.xMeters - _b.xMeters, _a.zMeters - _b.zMeters);
}

PlanePosition Along(PlanePosition _from, float _directionX, float _directionZ, float _meters) noexcept
{
  return {.xMeters = _from.xMeters + (_directionX * _meters), .zMeters = _from.zMeters + (_directionZ * _meters)};
}

// The server lists a snapshot's entities in identifier order, each once (Simulation::BuildSnapshot), so one is found by a
// binary search.
const EntityView* FindEntity(const Snapshot& _snapshot, EntityId _id) noexcept
{
  const auto found = std::ranges::lower_bound(_snapshot.entities, _id, {}, &EntityView::id);
  return found != _snapshot.entities.end() && found->id == _id ? &*found : nullptr;
}

const SectorView* FindSectorById(const Snapshot& _snapshot, std::int32_t _id) noexcept
{
  const auto found = std::ranges::find(_snapshot.sectors, _id, &SectorView::id);
  return found != _snapshot.sectors.end() ? &*found : nullptr;
}

bool IsStructure(const EntityView& _entity, StructureKind _kind) noexcept
{
  return _entity.kind == EntityKind::Structure && _entity.structure == _kind;
}

bool IsBuilt(const EntityView& _entity) noexcept
{
  return _entity.builtPermille >= Outpost::PERMILLE;
}

// The highest tier the player's Research Lab has opened by its level (Phase 3 design §6): 1 before any.
std::int32_t OpenedTier(const Snapshot& _snapshot) noexcept
{
  return _snapshot.researchTier;
}

// What one warship of _hull takes of its player's fleet cap (Phase 4 design §5), from the snapshot's hulls; 0 when the
// match sets no cap.
std::int32_t CommandPointsOf(const Snapshot& _snapshot, Outpost::HullId _hull) noexcept
{
  const auto hull = std::ranges::find(_snapshot.hulls, _hull, &Outpost::HullView::id);
  return hull != _snapshot.hulls.end() ? hull->commandPoints : 0;
}

// How many topics a Research Lab at _level researches at once (Phase 3 design §6), from the snapshot's levels.
std::int32_t ResearchSlotsOf(const Outpost::StructureTypeView* _lab, std::int32_t _level) noexcept
{
  std::int32_t slots = 1;
  for (size_t level = 0; _lab != nullptr && level < _lab->levels.size() && std::cmp_less(level + 1, _level); ++level)
    slots = std::max(slots, _lab->levels[level].researchSlots);
  return slots;
}

// The attack group goes for production first: Shipyards, then the Command Station, then anything else (Phase 1 design
// §4, §13). A lower rank comes first.
int AttackRank(const EntityView& _structure) noexcept
{
  if (_structure.structure == StructureKind::Shipyard)
    return 0;
  return _structure.structure == StructureKind::CommandStation ? 1 : 2;
}

// The level a Shipyard must be at to build a hull of _hull (Phase 3 design §5); 1 for a hull the snapshot does not list.
std::int32_t ShipyardLevelFor(const Snapshot& _snapshot, Outpost::HullId _hull) noexcept
{
  const auto hull = std::ranges::find(_snapshot.hulls, _hull, &Outpost::HullView::id);
  return hull != _snapshot.hulls.end() ? hull->shipyardLevel : 1;
}

const Outpost::StructureTypeView* FindType(const Snapshot& _snapshot, StructureKind _kind) noexcept
{
  const auto found = std::ranges::find(_snapshot.structureTypes, _kind, &Outpost::StructureTypeView::structure);
  return found != _snapshot.structureTypes.end() ? &*found : nullptr;
}

// The nearest place to _preferred, in rings around it, where a structure of _type keeps _gapMeters clear of every blocker
// and of the map's edge, and where the rules of _snapshot's map let its player build one: a Shipyard only in a sector the
// player holds (Phase 4 design §6).
std::optional<PlanePosition> FindPlace(const Outpost::StructureTypeView& _type, PlanePosition _preferred,
                                       std::span<const EntityView> _blockers, const Snapshot& _snapshot, float _gapMeters)
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
      if (Outpost::PlaceGhost(padded, place, _blockers, _snapshot.mapSizeMeters, _snapshot.sectors, _snapshot.player).valid)
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

// "Medium+Ion+Mass Driver", as the starting designs are named, with a module's name or else its initials after it, or the
// identifiers when that is not a valid name.
std::string DesignName(const Snapshot& _snapshot, const Outpost::DesignComponents& _design)
{
  const auto hull = std::ranges::find(_snapshot.hulls, _design.hull, &Outpost::HullView::id);
  const auto drive = std::ranges::find(_snapshot.drives, _design.drive, &Outpost::DriveView::id);
  const auto weapon = std::ranges::find(_snapshot.weapons, _design.weapon, &Outpost::WeaponView::id);
  const auto module = std::ranges::find(_snapshot.modules, _design.module, &Outpost::ModuleView::id);
  if (hull != _snapshot.hulls.end() && drive != _snapshot.drives.end() && weapon != _snapshot.weapons.end())
  {
    std::string name = std::format("{}+{}+{}", hull->nameUtf8, drive->nameUtf8, weapon->nameUtf8);
    if (module != _snapshot.modules.end())
    {
      const std::string whole = std::format("{}+{}", name, module->nameUtf8);
      name = Outpost::IsValidDesignName(whole) ? whole : std::format("{}+{}", name, Outpost::Abbreviation(module->nameUtf8));
    }
    if (Outpost::IsValidDesignName(name))
      return name;
  }
  return std::format("Design {}-{}-{}-{}", _design.hull.value, _design.drive.value, _design.weapon.value, _design.module.value);
}

// The player's saved design of these components, module included, or nullptr.
const Outpost::DesignView* FindDesignOf(const Snapshot& _snapshot, const Outpost::DesignComponents& _design)
{
  const auto found =
    std::ranges::find_if(_snapshot.designs, [&_design](const Outpost::DesignView& _view)
                         { return Outpost::DesignComponents{_view.hull, _view.drive, _view.weapon, _view.module} == _design; });
  return found != _snapshot.designs.end() ? &*found : nullptr;
}

auto ComponentOrder(const Outpost::DesignComponents& _design) noexcept
{
  return std::tuple(_design.hull, _design.drive, _design.weapon);
}

// A ship of _player's going back to be repaired is left to it, since any order would end its retreat (ADR-075): it is
// taken out of every order, and an order left with no ship is dropped.
void LeaveRetreatsAlone(const Snapshot& _snapshot, Outpost::PlayerId _player, std::vector<Outpost::Command>& _orders)
{
  std::vector<EntityId> retreating;
  for (const EntityView& entity : _snapshot.entities)
  {
    if (entity.owner == _player && entity.retreating)
      retreating.push_back(entity.id);
  }
  if (retreating.empty())
    return;
  const auto isRetreating = [&retreating](EntityId _id) { return std::ranges::find(retreating, _id) != retreating.end(); };
  for (Outpost::Command& command : _orders)
  {
    std::visit(
      [&isRetreating](auto& _order)
      {
        if constexpr (requires { _order.ships; })
          std::erase_if(_order.ships, isRetreating);
        else if constexpr (requires { _order.constructors; })
          std::erase_if(_order.constructors, isRetreating);
      },
      command.order);
  }
  std::erase_if(_orders,
                [](const Outpost::Command& _command)
                {
                  return std::visit(
                    [](const auto& _order)
                    {
                      if constexpr (requires { _order.ships; })
                        return _order.ships.empty();
                      else if constexpr (requires { _order.constructors; })
                        return _order.constructors.empty();
                      else
                        return false;
                    },
                    _command.order);
                });
}
} // namespace

Outpost::DesignComponents Outpost::ChooseAnswer(const AiSettings& _settings, const Snapshot& _snapshot)
{
  std::vector<DesignComponents> warships;
  for (const EntityView& ship : _snapshot.entities)
  {
    // The enemy is the other player: pirates are no fleet to answer (ADR-073).
    if (ship.kind == EntityKind::Ship && ship.role == ShipRole::Warship && ship.owner.IsValid() && ship.owner != _snapshot.player &&
        ship.owner != PIRATES)
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
    // The enemy's warships, not the pirates', which are no fleet to answer (ADR-073).
    else if (entity.kind == EntityKind::Ship && entity.role == ShipRole::Warship && entity.owner.IsValid() && entity.owner != m_player &&
             entity.owner != PIRATES)
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
  // What blocks in this snapshot is gathered when a place is first looked for (Blockers).
  m_blockersGathered = false;
  const auto station = std::ranges::find_if(_snapshot.entities, [this](const EntityView& _entity)
                                            { return IsStructure(_entity, StructureKind::CommandStation) && _entity.owner == m_player; });
  const bool hasStation = station != _snapshot.entities.end();
  // A lost Command Station is lost for good, and the AI plays on without it, building no more Constructors (Phase 1
  // design §4).
  if (!m_planned)
  {
    if (!hasStation)
      return;
    Plan(_snapshot, *station);
  }
  PlanShipyards(_snapshot);
  FollowOre(_snapshot);
  ClaimTerritory(_snapshot);
  FortifyFront(_snapshot);
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
  if (hasStation)
  {
    const auto queued = std::ranges::count(station->queue, ShipRole::Constructor, &JobView::role);
    if (constructors + queued < m_settings.constructors && station->queue.size() < QUEUE_LIMIT)
      _orders.push_back(MakeCommand(m_player, QueueShipCommand{.producer = station->id, .design = {}}));
  }

  // Research, one topic at a time, in its order.
  const auto lab = std::ranges::find_if(_snapshot.entities, [this](const EntityView& _entity)
                                        { return IsStructure(_entity, StructureKind::ResearchLab) && _entity.owner == m_player; });
  // As many at once as its Lab researches, each of a tier its Lab has opened (Phase 3 design §6).
  const StructureTypeView* labType = FindType(_snapshot, StructureKind::ResearchLab);
  if (lab != _snapshot.entities.end() && IsBuilt(*lab) && std::cmp_less(lab->research.size(), ResearchSlotsOf(labType, lab->level)))
  {
    const auto researched = [&_snapshot](ResearchTopicId _id)
    {
      const auto topic = std::ranges::find(_snapshot.research, _id, &ResearchTopicView::id);
      return topic != _snapshot.research.end() && topic->researched;
    };
    for (const ResearchTopicId id : m_settings.researchOrder)
    {
      const auto topic = std::ranges::find(_snapshot.research, id, &ResearchTopicView::id);
      if (topic == _snapshot.research.end() || topic->researched || topic->tier > _snapshot.researchTier ||
          std::ranges::find(lab->research, id) != lab->research.end() || !std::ranges::all_of(topic->prerequisites, researched))
        continue;
      _orders.push_back(MakeCommand(m_player, StartResearchCommand{.lab = lab->id, .topic = id}));
      break;
    }
  }

  std::vector<EntityId> idle;
  TendWork(_snapshot, idle, _orders);
  const PlanOre plan = Build(_snapshot, idle, _orders);
  const bool structureWaiting = plan.waiting;
  // An upgrade it wants and cannot yet pay for holds production back, as a structure does, so that the Ore gathers for
  // it. It never takes the Ore of a structure of the plan waiting for a Constructor, since it needs no Constructor
  // itself and would otherwise spend what that structure is due.
  bool upgradeWaiting = false;
  if (!structureWaiting)
  {
    std::int32_t ore = plan.left;
    upgradeWaiting =
      UpgradeShipyards(_snapshot, ore, _orders) || UpgradeLab(_snapshot, ore, _orders) || UpgradeStationForFleet(_snapshot, ore, _orders);
  }

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

  Produce(_snapshot, structureWaiting || upgradeWaiting, _orders);
  CommandFleet(_snapshot, _orders);
  CommandScouts(_snapshot, _orders);
  LeaveRetreatsAlone(_snapshot, m_player, _orders);
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
  m_homeRadiusMeters = _station.radiusMeters;
  const float fromCenter = std::hypot(home.xMeters, home.zMeters);
  // Toward the map's center, and across that to the right seen from above.
  const float forwardX = fromCenter > 0.0f ? -home.xMeters / fromCenter : 1.0f;
  const float forwardZ = fromCenter > 0.0f ? -home.zMeters / fromCenter : 0.0f;
  const float acrossX = forwardZ;
  const float acrossZ = -forwardX;
  const auto gap = static_cast<float>(m_settings.structureGapMeters);

  std::vector<const EntityView*> asteroids;
  for (const EntityView& entity : _snapshot.entities)
  {
    if (entity.kind == EntityKind::Asteroid)
      asteroids.push_back(&entity);
  }
  const std::vector<PlanePosition> enemyStations = EnemyStations(_snapshot);
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
    if (!IsEnemyHome(asteroid->position, enemyStations))
      contestedAsteroids.push_back(asteroid);
  }

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
    const std::optional<PlanePosition> place = FindPlace(*type, preferred, Blockers(_snapshot, std::nullopt), _snapshot, gap);
    slot.position = place.value_or(preferred);
    slot.abandoned = !place.has_value();
    m_slots.push_back(slot);
  };

  for (const EntityView* asteroid : homeAsteroids)
    AddRigSlot(_snapshot, *asteroid);
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
    AddRigSlot(_snapshot, *asteroid);
    AddPlatformBesideRig(_snapshot, m_slots.size() - 1);
  }
  for (std::int32_t platform = 0; platform < m_settings.homePlatformsPerShipyard; ++platform)
    AddHomePlatform(_snapshot, 0);
  const StructureTypeView rally{.structure = StructureKind::Shipyard, .radiusMeters = RALLY_RADIUS_METERS};
  const PlanePosition preferred = Along(home, forwardX, forwardZ, static_cast<float>(m_settings.rallyDistanceMeters));
  m_rally = FindPlace(rally, preferred, Blockers(_snapshot, std::nullopt), _snapshot, 0.0f).value_or(preferred);
}

void Outpost::AiPlayer::AddRigSlot(const Snapshot& _snapshot, const EntityView& _asteroid)
{
  if (const SectorView* sector = FindSector(_snapshot.sectors, _asteroid.position);
      sector != nullptr && sector->holder != m_player &&
      std::ranges::none_of(m_slots, [sector](const Slot& _slot)
                           { return _slot.structure == StructureKind::Relay && _slot.position == sector->node; }))
    AddRelaySlot(_snapshot, *sector);
  const StructureTypeView* type = FindType(_snapshot, StructureKind::MiningRig);
  const float radius = type != nullptr ? std::max(type->radiusMeters, _asteroid.radiusMeters) : _asteroid.radiusMeters;
  m_slots.push_back({.structure = StructureKind::MiningRig,
                     .position = _asteroid.position,
                     .radiusMeters = radius,
                     .asteroid = _asteroid.id,
                     .abandoned = type == nullptr || !type->buildable});
}

// A Relay stands on its sector's node, which is fixed, so its place is never searched for (ADR-056).
void Outpost::AiPlayer::AddRelaySlot(const Snapshot& _snapshot, const SectorView& _sector)
{
  const StructureTypeView* type = FindType(_snapshot, StructureKind::Relay);
  m_slots.push_back({.structure = StructureKind::Relay,
                     .position = _sector.node,
                     .radiusMeters = type != nullptr ? type->radiusMeters : 0.0f,
                     .abandoned = type == nullptr || !type->buildable});
}

void Outpost::AiPlayer::AddPlatformBesideRig(const Snapshot& _snapshot, size_t _rig)
{
  AddPlatformBeside(_snapshot, m_slots[_rig].position, m_slots[_rig].radiusMeters, _rig);
}

void Outpost::AiPlayer::AddPlatformBeside(const Snapshot& _snapshot, PlanePosition _at, float _radiusMeters,
                                          std::optional<size_t> _besideRig)
{
  const PlanePosition at = _at;
  const float rigRadius = _radiusMeters;
  Slot slot{.structure = StructureKind::DefensePlatform, .besideRig = _besideRig, .minimumIncomeHundredthsPerSecond = 0};
  const StructureTypeView* type = FindType(_snapshot, StructureKind::DefensePlatform);
  if (type == nullptr || !type->buildable)
  {
    slot.abandoned = true;
    m_slots.push_back(slot);
    return;
  }
  const float toHome = std::max(Distance(at, m_home), 1.0f);
  const PlanePosition preferred = Along(at, (m_home.xMeters - at.xMeters) / toHome, (m_home.zMeters - at.zMeters) / toHome,
                                        rigRadius + type->radiusMeters + static_cast<float>(m_settings.structureGapMeters));
  slot.radiusMeters = type->radiusMeters;
  const std::optional<PlanePosition> place =
    FindPlace(*type, preferred, Blockers(_snapshot, std::nullopt), _snapshot, static_cast<float>(m_settings.structureGapMeters));
  slot.position = place.value_or(preferred);
  slot.abandoned = !place.has_value();
  m_slots.push_back(slot);
}

std::vector<Outpost::PlanePosition> Outpost::AiPlayer::EnemyStations(const Snapshot& _snapshot) const
{
  std::vector<PlanePosition> stations;
  for (const EntityView& entity : _snapshot.entities)
  {
    if (IsStructure(entity, StructureKind::CommandStation) && entity.owner != m_player)
      stations.push_back(entity.position);
  }
  // Under fog of war the enemy's base is out of sight at the start (ADR-024): the map is point-symmetric, so it is across
  // the center from this one.
  if (stations.empty())
    stations.push_back({.xMeters = -m_home.xMeters, .zMeters = -m_home.zMeters});
  return stations;
}

bool Outpost::AiPlayer::IsEnemyHome(PlanePosition _asteroid, const std::vector<PlanePosition>& _enemyStations) const
{
  const float ours = Distance(_asteroid, m_home);
  return std::ranges::any_of(_enemyStations, [&](PlanePosition _enemy) { return Distance(_asteroid, _enemy) < ours; });
}

void Outpost::AiPlayer::FollowOre(const Snapshot& _snapshot)
{
  // An asteroid it does not see now holds what it last saw, or, never seen, ore enough to go and look.
  const auto hasOre = [&_snapshot](EntityId _asteroid)
  {
    const EntityView* asteroid = FindEntity(_snapshot, _asteroid);
    return asteroid != nullptr && asteroid->oreReserveHundredths.value_or(1) > 0;
  };
  const auto mining = std::ranges::count_if(
    m_slots, [&](const Slot& _slot)
    { return _slot.structure == StructureKind::MiningRig && !_slot.abandoned && !IsBlocked(_slot, _snapshot) && hasOre(_slot.asteroid); });
  if (mining >= m_settings.homeAsteroids + m_settings.contestedAsteroids)
    return;

  const std::vector<PlanePosition> enemyStations = EnemyStations(_snapshot);
  const EntityView* nearest = nullptr;
  for (const EntityView& asteroid : _snapshot.entities)
  {
    if (asteroid.kind != EntityKind::Asteroid || !hasOre(asteroid.id) || IsEnemyHome(asteroid.position, enemyStations) ||
        std::ranges::find(m_slots, asteroid.id, &Slot::asteroid) != m_slots.end())
      continue;
    const bool taken = std::ranges::any_of(
      _snapshot.entities, [&asteroid](const EntityView& _rig)
      { return IsStructure(_rig, StructureKind::MiningRig) && Distance(_rig.position, asteroid.position) <= SAME_PLACE_METERS; });
    const float distance = Distance(asteroid.position, m_home);
    if (!taken && (nearest == nullptr || distance < Distance(nearest->position, m_home) ||
                   (distance == Distance(nearest->position, m_home) && asteroid.id < nearest->id)))
      nearest = &asteroid;
  }
  if (nearest == nullptr)
    return;
  AddRigSlot(_snapshot, *nearest);
  AddPlatformBesideRig(_snapshot, m_slots.size() - 1);
}

// Owner, 2026-10-01: the AI builds its N-th Shipyard once its income reaches N times the settings' income per Shipyard,
// since one Shipyard spends about as much as that. A Shipyard joins the plan once, and keeps waiting for the income if
// the income drops again, as when a rig is lost. They stand behind the Command Station, then behind it to either side,
// each round of three farther out; the search finds the nearest clear place to each.
void Outpost::AiPlayer::PlanShipyards(const Snapshot& _snapshot)
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
  const PlanePosition home = m_home;
  const float fromCenter = std::hypot(home.xMeters, home.zMeters);
  const float forwardX = fromCenter > 0.0f ? -home.xMeters / fromCenter : 1.0f;
  const float forwardZ = fromCenter > 0.0f ? -home.zMeters / fromCenter : 0.0f;
  const float turn = TURNS_RADIANS[extra % TURNS_RADIANS.size()];
  const float directionX = (forwardX * std::cos(turn)) - (forwardZ * std::sin(turn));
  const float directionZ = (forwardX * std::sin(turn)) + (forwardZ * std::cos(turn));
  const auto gap = static_cast<float>(m_settings.structureGapMeters);
  const size_t roundIndex = extra / TURNS_RADIANS.size();
  const auto round = static_cast<float>(roundIndex);
  const float meters = m_homeRadiusMeters + type->radiusMeters + gap + (round * ((2.0f * type->radiusMeters) + gap));
  const PlanePosition preferred = Along(home, directionX, directionZ, meters);

  slot.radiusMeters = type->radiusMeters;
  const std::optional<PlanePosition> place = FindPlace(*type, preferred, Blockers(_snapshot, std::nullopt), _snapshot, gap);
  slot.position = place.value_or(preferred);
  slot.abandoned = !place.has_value();
  m_slots.push_back(slot);
  for (std::int32_t platform = 0; platform < m_settings.homePlatformsPerShipyard; ++platform)
    AddHomePlatform(_snapshot, slot.minimumIncomeHundredthsPerSecond);
}

void Outpost::AiPlayer::AddHomePlatform(const Snapshot& _snapshot, std::int32_t _minimumIncomeHundredthsPerSecond)
{
  Slot slot{.structure = StructureKind::DefensePlatform,
            .besideRig = std::nullopt,
            .minimumIncomeHundredthsPerSecond = _minimumIncomeHundredthsPerSecond};
  const StructureTypeView* type = FindType(_snapshot, StructureKind::DefensePlatform);
  if (type == nullptr || !type->buildable)
  {
    slot.abandoned = true;
    m_slots.push_back(slot);
    return;
  }
  const size_t index = m_homePlatforms++;
  const float fromCenter = std::hypot(m_home.xMeters, m_home.zMeters);
  const float forwardX = fromCenter > 0.0f ? -m_home.xMeters / fromCenter : 1.0f;
  const float forwardZ = fromCenter > 0.0f ? -m_home.zMeters / fromCenter : 0.0f;
  const float turn = HOME_PLATFORM_TURNS_RADIANS[index % HOME_PLATFORM_TURNS_RADIANS.size()];
  const size_t ringIndex = index / HOME_PLATFORM_TURNS_RADIANS.size();
  const auto ring = static_cast<float>(ringIndex);
  const float directionX = (forwardX * std::cos(turn)) - (forwardZ * std::sin(turn));
  const float directionZ = (forwardX * std::sin(turn)) + (forwardZ * std::cos(turn));
  const PlanePosition preferred =
    Along(m_home, directionX, directionZ, HOME_PLATFORM_RING_METERS + (ring * HOME_PLATFORM_RING_STEP_METERS));
  slot.radiusMeters = type->radiusMeters;
  const std::optional<PlanePosition> place =
    FindPlace(*type, preferred, Blockers(_snapshot, std::nullopt), _snapshot, static_cast<float>(m_settings.structureGapMeters));
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
  // A rig or Relay in a sector the enemy holds waits for the enemy to lose it, and one the pirates guard waits for them
  // to fall: milestone 33 teaches it to clear them (ADR-073).
  if (_slot.structure == StructureKind::MiningRig || _slot.structure == StructureKind::Relay)
  {
    const SectorView* sector = FindSector(_snapshot.sectors, _slot.position);
    if (sector != nullptr && ((sector->holder.IsValid() && sector->holder != m_player) || sector->guarded))
      return true;
  }
  // A Relay whose node a wreck covers waits for it to be salvaged (ADR-074), which the AI leaves until milestone 33.
  if (_slot.structure == StructureKind::Relay && std::ranges::any_of(_snapshot.entities,
                                                                     [&](const EntityView& _entity) {
                                                                       return _entity.kind == EntityKind::Derelict &&
                                                                              Distance(_entity.position, _slot.position) <
                                                                                _entity.radiusMeters + _slot.radiusMeters;
                                                                     }))
    return true;
  if (_slot.structure != StructureKind::MiningRig)
    return false;
  return std::ranges::any_of(_snapshot.entities,
                             [&](const EntityView& _entity)
                             {
                               return IsStructure(_entity, StructureKind::MiningRig) && _entity.owner != m_player &&
                                      Distance(_entity.position, _slot.position) <= SAME_PLACE_METERS;
                             });
}

bool Outpost::AiPlayer::IsReady(const Slot& _slot, const Snapshot& _snapshot) const
{
  if (_snapshot.sectors.empty())
    return true;
  const SectorView* sector = FindSector(_snapshot.sectors, _slot.position);
  if (_slot.structure == StructureKind::MiningRig)
    return sector != nullptr && sector->holder == m_player;
  if (_slot.structure != StructureKind::Relay)
    return true;
  // A Relay past its Command Station's cap waits for the station's next level (Phase 3 design §7).
  return IsClaimable(_slot, _snapshot) && !AtNodeCap(_snapshot.sectors, _snapshot.entities, m_player, _snapshot.nodeCap);
}

bool Outpost::AiPlayer::IsClaimable(const Slot& _slot, const Snapshot& _snapshot) const
{
  const SectorView* sector = FindSector(_snapshot.sectors, _slot.position);
  return sector != nullptr && !sector->holder.IsValid() && !sector->guarded &&
         std::ranges::any_of(sector->adjacent,
                             [&](std::int32_t _id)
                             {
                               const SectorView* adjacent = FindSectorById(_snapshot, _id);
                               return adjacent != nullptr && adjacent->holder == m_player;
                             });
}

std::span<const Outpost::EntityView> Outpost::AiPlayer::Blockers(const Snapshot& _snapshot, std::optional<size_t> _skippedSlot)
{
  // The snapshot's part is gathered at the decision's first call. The plan's part changes as the plan grows, so it follows
  // afresh at every call.
  if (!m_blockersGathered)
  {
    m_blockers.clear();
    for (const EntityView& entity : _snapshot.entities)
    {
      if (entity.kind != EntityKind::Ship)
        m_blockers.push_back(entity);
    }
    m_snapshotBlockers = m_blockers.size();
    m_blockersGathered = true;
  }
  m_blockers.resize(m_snapshotBlockers);
  for (size_t i = 0; i < m_slots.size(); ++i)
  {
    const Slot& slot = m_slots[i];
    // A rig stands on its asteroid, which already blocks.
    if (i == _skippedSlot || slot.abandoned || slot.structure == StructureKind::MiningRig || IsDone(slot, _snapshot))
      continue;
    m_blockers.push_back(
      {.kind = EntityKind::Structure, .structure = slot.structure, .position = slot.position, .radiusMeters = slot.radiusMeters});
  }
  return m_blockers;
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
                  // A Constructor gone, or going back to be repaired, which ended its work order, leaves the crew (ADR-075).
                  std::erase_if(_work.constructors,
                                [&_snapshot](EntityId _id)
                                {
                                  const EntityView* constructor = FindEntity(_snapshot, _id);
                                  return constructor == nullptr || constructor->retreating;
                                });
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
    // One going back to be repaired is left to it until it is whole.
    if (ship.kind != EntityKind::Ship || ship.role != ShipRole::Constructor || ship.owner != m_player || ship.retreating)
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

Outpost::AiPlayer::PlanOre Outpost::AiPlayer::Build(const Snapshot& _snapshot, std::vector<EntityId>& _idle, std::vector<Command>& _orders)
{
  std::int32_t ore = _snapshot.ore;
  const auto gap = static_cast<float>(m_settings.structureGapMeters);
  // The station's next level is ordered once a decision, however many Relays the cap holds back.
  bool stationOrdered = false;
  for (size_t i = 0; i < m_slots.size(); ++i)
  {
    Slot& slot = m_slots[i];
    // A Relay its Command Station's cap holds back has the station's next level take its place in the plan's order (Phase 3
    // design §7, plan task 23.1). Plan task 24.1 makes this play.
    const bool capped =
      slot.structure == StructureKind::Relay && AtNodeCap(_snapshot.sectors, _snapshot.entities, m_player, _snapshot.nodeCap);
    if (capped && !slot.abandoned && !IsBlocked(slot, _snapshot) && !IsDone(slot, _snapshot) && IsClaimable(slot, _snapshot))
    {
      if (const EntityView* station = stationOrdered ? nullptr : UpgradableStation(_snapshot))
      {
        const std::int32_t cost = FindType(_snapshot, StructureKind::CommandStation)->levels[static_cast<size_t>(station->level - 1)].cost;
        if (ore < cost)
          return {.waiting = true, .left = ore};
        OrderUpgrade(*station, _orders);
        ore -= cost;
        stationOrdered = true;
      }
      continue;
    }
    if (IsBlocked(slot, _snapshot) || IsDone(slot, _snapshot) || !IsReady(slot, _snapshot) ||
        _snapshot.oreIncomeHundredthsPerSecond < slot.minimumIncomeHundredthsPerSecond ||
        std::ranges::any_of(m_work, [i](const Work& _work) { return _work.slot == i; }))
      continue;
    const StructureTypeView* type = FindType(_snapshot, slot.structure);
    if (type == nullptr)
      continue;
    if (_idle.empty())
      return {.waiting = false, .left = ore - type->cost};
    if (ore < type->cost)
      return {.waiting = true, .left = ore};

    if (slot.structure != StructureKind::MiningRig && slot.structure != StructureKind::Relay)
    {
      // Something may have taken the place since it was planned.
      StructureTypeView padded = *type;
      padded.radiusMeters += gap;
      const std::span<const EntityView> blockers = Blockers(_snapshot, i);
      if (!PlaceGhost(padded, slot.position, blockers, _snapshot.mapSizeMeters, _snapshot.sectors, m_player).valid)
      {
        const std::optional<PlanePosition> place = FindPlace(*type, slot.position, blockers, _snapshot, gap);
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
  return {.waiting = false, .left = ore};
}

// Keeps every built Shipyard's queue at the settings' length with the chosen design, saving the design first if the AI
// does not have it. Not while a structure waits for Ore, since a ship is paid for when it starts and would take it.
void Outpost::AiPlayer::Produce(const Snapshot& _snapshot, bool _structureWaiting, std::vector<Command>& _orders)
{
  // Its scouts come first, ahead of a structure waiting for Ore, as they must set out in the first minutes (ADR-020
  // decision 13).
  if (m_settings.scouts > 0 && !_snapshot.sectors.empty())
  {
    const DesignView* scout = FindDesignOf(_snapshot, m_settings.scoutDesign);
    std::int32_t scouts = 0;
    for (const EntityView& entity : _snapshot.entities)
    {
      if (entity.owner != m_player)
        continue;
      if (entity.kind == EntityKind::Ship && IsScout(entity))
        ++scouts;
      else if (scout != nullptr && IsStructure(entity, StructureKind::Shipyard))
        scouts += static_cast<std::int32_t>(std::ranges::count(entity.queue, scout->id, &JobView::design));
    }
    const auto yard =
      std::ranges::find_if(_snapshot.entities, [this](const EntityView& _entity)
                           { return IsStructure(_entity, StructureKind::Shipyard) && _entity.owner == m_player && IsBuilt(_entity); });
    if (scouts < m_settings.scouts && yard != _snapshot.entities.end() && yard->queue.size() < QUEUE_LIMIT)
    {
      if (scout != nullptr)
        _orders.push_back(MakeCommand(m_player, QueueShipCommand{.producer = yard->id, .design = scout->id}));
      else if (!m_scoutSaveTick.has_value() || _snapshot.tick >= *m_scoutSaveTick + (DESIGN_WAIT_SECONDS * m_ticksPerSecond))
      {
        const DesignComponents& design = m_settings.scoutDesign;
        _orders.push_back(MakeCommand(m_player, SaveDesignCommand{.design = {},
                                                                  .nameUtf8 = DesignName(_snapshot, design),
                                                                  .hull = design.hull,
                                                                  .drive = design.drive,
                                                                  .weapon = design.weapon,
                                                                  .module = design.module}));
        m_scoutSaveTick = _snapshot.tick;
      }
    }
  }
  if (_structureWaiting)
    return;
  const auto design =
    std::ranges::find_if(_snapshot.designs, [this](const DesignView& _design)
                         { return DesignComponents{_design.hull, _design.drive, _design.weapon, _design.module} == m_productionDesign; });
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
  const std::int32_t level = ShipyardLevelFor(_snapshot, m_productionDesign.hull);
  for (const EntityView& yard : _snapshot.entities)
  {
    // A Shipyard below the design's level is upgraded first (UpgradeShipyards).
    if (!IsStructure(yard, StructureKind::Shipyard) || yard.owner != m_player || !IsBuilt(yard) || yard.level < level)
      continue;
    for (size_t jobs = yard.queue.size(); std::cmp_less(jobs, m_settings.shipyardQueueJobs); ++jobs)
      _orders.push_back(MakeCommand(m_player, QueueShipCommand{.producer = yard.id, .design = design->id}));
  }
}

// A Shipyard below the level its production design's hull needs is upgraded, one level and one Shipyard at a time, once
// the Ore is there (Phase 3 design §5, plan task 21.1). Plan task 24.1 makes this play.
bool Outpost::AiPlayer::UpgradeShipyards(const Snapshot& _snapshot, std::int32_t& _ore, std::vector<Command>& _orders)
{
  const StructureTypeView* type = FindType(_snapshot, StructureKind::Shipyard);
  const std::int32_t needed = ShipyardLevelFor(_snapshot, m_productionDesign.hull);
  if (type == nullptr)
    return false;
  for (const EntityView& yard : _snapshot.entities)
  {
    if (!IsStructure(yard, StructureKind::Shipyard) || yard.owner != m_player || !IsBuilt(yard) || yard.upgradePermille.has_value() ||
        yard.level >= needed || std::cmp_greater_equal(yard.level - 1, type->levels.size()))
      continue;
    const std::int32_t cost = type->levels[static_cast<size_t>(yard.level - 1)].cost;
    if (_ore < cost)
      return true;
    OrderUpgrade(yard, _orders);
    _ore -= cost;
    return false;
  }
  return false;
}

// The Research Lab is upgraded when the next topic of its research order is of a tier it has not opened, once what the
// level requires is researched and the Ore is there (Phase 3 design §6, plan task 22.1). Plan task 24.1 makes this play.
bool Outpost::AiPlayer::UpgradeLab(const Snapshot& _snapshot, std::int32_t& _ore, std::vector<Command>& _orders)
{
  const StructureTypeView* type = FindType(_snapshot, StructureKind::ResearchLab);
  const auto lab = std::ranges::find_if(_snapshot.entities, [this](const EntityView& _entity)
                                        { return IsStructure(_entity, StructureKind::ResearchLab) && _entity.owner == m_player; });
  if (type == nullptr || lab == _snapshot.entities.end() || !IsBuilt(*lab) || lab->upgradePermille.has_value() ||
      std::cmp_greater_equal(lab->level - 1, type->levels.size()))
    return false;
  const auto researched = [&_snapshot](ResearchTopicId _id)
  {
    const auto topic = std::ranges::find(_snapshot.research, _id, &ResearchTopicView::id);
    return topic != _snapshot.research.end() && topic->researched;
  };
  // The next topic of its order not yet researched.
  const auto next =
    std::ranges::find_if(m_settings.researchOrder, [&](ResearchTopicId _id)
                         { return std::ranges::contains(_snapshot.research, _id, &ResearchTopicView::id) && !researched(_id); });
  if (next == m_settings.researchOrder.end())
    return false;
  const auto topic = std::ranges::find(_snapshot.research, *next, &ResearchTopicView::id);
  const StructureLevelView& level = type->levels[static_cast<size_t>(lab->level - 1)];
  // A level that opens the tier of its next topic, or once its open tier is the settings', the one that gives a second
  // slot while two topics of its order are left (Phase 3 design §8).
  const auto left =
    std::ranges::count_if(m_settings.researchOrder, [&](ResearchTopicId _id)
                          { return std::ranges::contains(_snapshot.research, _id, &ResearchTopicView::id) && !researched(_id); });
  const bool opensTier = level.opensTier > 0 && topic->tier > _snapshot.researchTier;
  const bool addsSlot = level.researchSlots > 1 && _snapshot.researchTier >= m_settings.secondSlotTier && left >= 2;
  if ((!opensTier && !addsSlot) || !std::ranges::all_of(level.prerequisites, researched))
    return false;
  if (_ore < level.cost)
    return true;
  OrderUpgrade(*lab, _orders);
  _ore -= level.cost;
  return false;
}

bool Outpost::AiPlayer::UpgradeStationForFleet(const Snapshot& _snapshot, std::int32_t& _ore, std::vector<Command>& _orders)
{
  // A level builds itself for most of a minute, so it looks one round of its Shipyards ahead: a ship of its production
  // design from each.
  const auto shipyards =
    std::ranges::count_if(_snapshot.entities, [this](const EntityView& _entity)
                          { return IsStructure(_entity, StructureKind::Shipyard) && _entity.owner == m_player && IsBuilt(_entity); });
  const std::int64_t round = std::max<std::int64_t>(1, shipyards) * CommandPointsOf(_snapshot, m_productionDesign.hull);
  if (_snapshot.fleetCap <= 0 || _snapshot.commandPoints + round <= _snapshot.fleetCap)
    return false;
  const EntityView* station = UpgradableStation(_snapshot);
  const auto ordered = [station](const Command& _order)
  {
    const auto* upgrade = std::get_if<UpgradeStructureCommand>(&_order.order);
    return upgrade != nullptr && upgrade->structure == station->id;
  };
  // Build may have ordered it this decision, for a Relay the node cap holds back.
  if (station == nullptr || std::ranges::any_of(_orders, ordered))
    return false;
  const std::int32_t cost = FindType(_snapshot, StructureKind::CommandStation)->levels[static_cast<size_t>(station->level - 1)].cost;
  if (_ore < cost)
    return true;
  OrderUpgrade(*station, _orders);
  _ore -= cost;
  return false;
}

// The AI's Command Station, when it can be upgraded now: finished, not being upgraded, and below its top level (Phase 3
// design §7).
const Outpost::EntityView* Outpost::AiPlayer::UpgradableStation(const Snapshot& _snapshot) const
{
  const StructureTypeView* type = FindType(_snapshot, StructureKind::CommandStation);
  const auto station = std::ranges::find_if(_snapshot.entities, [this](const EntityView& _entity)
                                            { return IsStructure(_entity, StructureKind::CommandStation) && _entity.owner == m_player; });
  if (type == nullptr || station == _snapshot.entities.end() || !IsBuilt(*station) || station->upgradePermille.has_value() ||
      std::cmp_greater_equal(station->level - 1, type->levels.size()))
    return nullptr;
  return &*station;
}

void Outpost::AiPlayer::OrderUpgrade(const EntityView& _structure, std::vector<Command>& _orders) const
{
  _orders.push_back(MakeCommand(m_player, UpgradeStructureCommand{.structure = _structure.id}));
}

bool Outpost::AiPlayer::IsScout(const EntityView& _ship) const noexcept
{
  return m_settings.scoutDesign.module.IsValid() && _ship.kind == EntityKind::Ship && _ship.role == ShipRole::Warship &&
         _ship.module == m_settings.scoutDesign.module;
}

// Once its first Shipyard stands, and every Relay it has planned stands or is blocked, it claims the free sector next to
// its territory nearest its base: a rig on each of its asteroids, each after the sector's Relay, or the Relay alone in a
// sector without ore (ADR-020 decision 13).
void Outpost::AiPlayer::ClaimTerritory(const Snapshot& _snapshot)
{
  if (_snapshot.sectors.empty())
    return;
  const bool shipyard =
    std::ranges::any_of(_snapshot.entities, [this](const EntityView& _entity)
                        { return IsStructure(_entity, StructureKind::Shipyard) && _entity.owner == m_player && IsBuilt(_entity); });
  if (!shipyard)
    return;
  const bool claiming =
    std::ranges::any_of(m_slots, [&](const Slot& _slot)
                        { return _slot.structure == StructureKind::Relay && !IsDone(_slot, _snapshot) && !IsBlocked(_slot, _snapshot); });
  // A claim the pirates guard is no claim, and it claims another in its place (ADR-073).
  std::erase_if(m_claimed,
                [&_snapshot](std::int32_t _id)
                {
                  const SectorView* sector = FindSectorById(_snapshot, _id);
                  return sector != nullptr && sector->guarded;
                });
  if (claiming || std::cmp_greater_equal(m_claimed.size(), m_settings.claimSectors))
    return;
  const SectorView* claimed = nullptr;
  for (const SectorView& sector : _snapshot.sectors)
  {
    // A sector its plan already takes, by a rig's Relay, is no claim: the claims are beyond those (Phase 3 design §8).
    const bool planned = std::ranges::any_of(m_slots, [&sector](const Slot& _slot)
                                             { return _slot.structure == StructureKind::Relay && _slot.position == sector.node; });
    if (sector.holder.IsValid() || sector.guarded || planned || std::ranges::find(m_claimed, sector.id) != m_claimed.end())
      continue;
    const bool adjacent = std::ranges::any_of(sector.adjacent,
                                              [&](std::int32_t _id)
                                              {
                                                const SectorView* other = FindSectorById(_snapshot, _id);
                                                return other != nullptr && other->holder == m_player;
                                              });
    if (adjacent && (claimed == nullptr || IsNearer(sector.node, claimed->node, m_home)))
      claimed = &sector;
  }
  if (claimed == nullptr)
    return;
  m_claimed.push_back(claimed->id);
  std::vector<const EntityView*> asteroids;
  for (const EntityView& asteroid : _snapshot.entities)
  {
    if (asteroid.kind == EntityKind::Asteroid && claimed->Contains(asteroid.position) &&
        std::ranges::find(m_slots, asteroid.id, &Slot::asteroid) == m_slots.end())
      asteroids.push_back(&asteroid);
  }
  std::ranges::sort(asteroids, [this](const EntityView* _a, const EntityView* _b) { return IsNearer(_a->position, _b->position, m_home); });
  for (const EntityView* asteroid : asteroids)
    AddRigSlot(_snapshot, *asteroid);
  if (std::ranges::none_of(m_slots, [claimed](const Slot& _slot)
                           { return _slot.structure == StructureKind::Relay && _slot.position == claimed->node; }))
    AddRelaySlot(_snapshot, *claimed);
}

// A sector it holds next to one the enemy holds is its front: its Relay gets the settings' Defence Platforms, once.
void Outpost::AiPlayer::FortifyFront(const Snapshot& _snapshot)
{
  const StructureTypeView* relay = FindType(_snapshot, StructureKind::Relay);
  for (const SectorView& sector : _snapshot.sectors)
  {
    if (sector.holder != m_player || sector.Contains(m_home) || std::ranges::find(m_fortified, sector.id) != m_fortified.end())
      continue;
    const bool front = std::ranges::any_of(sector.adjacent,
                                           [&](std::int32_t _id)
                                           {
                                             const SectorView* other = FindSectorById(_snapshot, _id);
                                             return other != nullptr && other->holder.IsValid() && other->holder != m_player;
                                           });
    if (!front)
      continue;
    m_fortified.push_back(sector.id);
    for (std::int32_t platform = 0; platform < m_settings.frontPlatforms; ++platform)
      AddPlatformBeside(_snapshot, sector.node, relay != nullptr ? relay->radiusMeters : 0.0f, std::nullopt);
  }
}

// Each scout goes from one of the sectors next to the enemy's home to the next, attack-moving, so that it meets what is
// there and suppresses its Relay while it lives (ADR-020 decision 13). The enemy's home is across the map's center from
// its own.
void Outpost::AiPlayer::CommandScouts(const Snapshot& _snapshot, std::vector<Command>& _orders)
{
  const SectorView* enemyHome = FindSector(_snapshot.sectors, {.xMeters = -m_home.xMeters, .zMeters = -m_home.zMeters});
  if (enemyHome == nullptr || enemyHome->adjacent.empty())
    return;
  std::vector<PlanePosition> waypoints;
  for (const std::int32_t id : enemyHome->adjacent)
  {
    if (const SectorView* sector = FindSectorById(_snapshot, id))
      waypoints.push_back(sector->node);
  }
  std::ranges::sort(waypoints, [this](PlanePosition _a, PlanePosition _b) { return IsNearer(_a, _b, m_home); });
  std::erase_if(m_scoutWaypoints, [&_snapshot](const auto& _entry) { return FindEntity(_snapshot, _entry.first) == nullptr; });
  for (const EntityView& ship : _snapshot.entities)
  {
    if (ship.owner != m_player || !IsScout(ship))
      continue;
    const auto known = m_scoutWaypoints.find(ship.id);
    size_t next = known != m_scoutWaypoints.end() ? known->second : 0;
    const bool arrived =
      known != m_scoutWaypoints.end() && Distance(ship.position, waypoints[next % waypoints.size()]) <= SCOUT_ARRIVED_METERS;
    if (known != m_scoutWaypoints.end() && !arrived)
      continue;
    if (arrived)
      next = (next + 1) % waypoints.size();
    m_scoutWaypoints[ship.id] = next;
    _orders.push_back(MakeCommand(m_player, AttackMoveCommand{.ships = {ship.id}, .destination = waypoints[next % waypoints.size()]}));
  }
}

// The reserve gathers at the rally, or goes to where the base is under fire; once it is large enough it joins the attack
// group, which attack-moves on the enemy's production first, the nearest of it, and then on the next structure.
void Outpost::AiPlayer::CommandFleet(const Snapshot& _snapshot, std::vector<Command>& _orders)
{
  // Its scouts go their own way (CommandScouts). A ship going back to be repaired leaves its group, and rejoins the reserve
  // once it is whole (ADR-075).
  std::vector<const EntityView*> warships;
  for (const EntityView& ship : _snapshot.entities)
  {
    if (ship.kind == EntityKind::Ship && ship.role == ShipRole::Warship && ship.owner == m_player && !IsScout(ship) && !ship.retreating)
      warships.push_back(&ship);
  }
  const auto gone = [&_snapshot](EntityId _id)
  {
    const EntityView* ship = FindEntity(_snapshot, _id);
    return ship == nullptr || ship->retreating;
  };
  std::erase_if(m_attackGroup, gone);

  // A raid ends once its sector is no longer the enemy's, or once it has lost the settings' share of its ships; its ships
  // then rejoin the reserve, and the next raid waits (ADR-020 decision 13).
  std::erase_if(m_raidGroup, gone);
  if (!m_raidGroup.empty())
  {
    const SectorView* sector = FindSectorById(_snapshot, m_raidSector);
    const bool held = sector != nullptr && sector->holder.IsValid() && sector->holder != m_player;
    const bool lost = m_settings.raidLossShare > 0.0 &&
                      static_cast<double>(m_raidGroup.size()) <= static_cast<double>(m_raidLaunch) * (1.0 - m_settings.raidLossShare);
    if (!held || lost)
    {
      m_raidGroup.clear();
      m_raidClosing.clear();
      m_nextRaidTick = _snapshot.tick + static_cast<std::uint64_t>(std::llround(m_settings.raidIntervalSeconds * m_ticksPerSecond));
    }
    else
    {
      // Its ships near the Relay attack it, once each, so that none stands idle out of its range.
      const auto relay = std::ranges::find_if(_snapshot.entities,
                                              [&](const EntityView& _entity)
                                              {
                                                return IsStructure(_entity, StructureKind::Relay) && _entity.owner != m_player &&
                                                       Distance(_entity.position, sector->node) <= SAME_PLACE_METERS;
                                              });
      if (relay != _snapshot.entities.end())
      {
        std::vector<EntityId> closing;
        for (const EntityId id : m_raidGroup)
        {
          const EntityView* ship = FindEntity(_snapshot, id);
          if (ship != nullptr && std::ranges::find(m_raidClosing, id) == m_raidClosing.end() &&
              Distance(ship->position, relay->position) <= CLOSE_IN_METERS)
            closing.push_back(id);
        }
        if (!closing.empty())
        {
          m_raidClosing.insert(m_raidClosing.end(), closing.begin(), closing.end());
          _orders.push_back(MakeCommand(m_player, AttackCommand{.ships = std::move(closing), .target = relay->id}));
        }
      }
    }
  }
  std::erase_if(warships, [this](const EntityView* _ship) { return std::ranges::find(m_raidGroup, _ship->id) != m_raidGroup.end(); });
  // An attack that has lost the settings' share of the ships it set out with falls back to the rally, out of the fight,
  // and rejoins the reserve, which waits before it attacks again (task 12.2): a lost battle costs part of a fleet, not
  // all of it.
  const bool fallBack =
    !m_attackGroup.empty() && m_settings.retreatLossShare > 0.0 &&
    static_cast<double>(m_attackGroup.size()) <= static_cast<double>(m_launchShips) * (1.0 - m_settings.retreatLossShare);
  if (fallBack)
  {
    _orders.push_back(MakeCommand(m_player, MoveCommand{.ships = m_attackGroup, .destination = m_rally}));
    for (const EntityId id : m_attackGroup)
      m_reserveDestinations[id] = {.destination = m_rally};
    m_attackGroup.clear();
    m_attackTarget = {};
    m_closingIn.clear();
    m_searching = false;
    m_launchShips = 0;
    m_regroupUntilTick = _snapshot.tick + static_cast<std::uint64_t>(std::llround(m_settings.regroupSeconds * m_ticksPerSecond));
  }
  std::vector<const EntityView*> reserve;
  for (const EntityView* ship : warships)
  {
    if (std::ranges::find(m_attackGroup, ship->id) == m_attackGroup.end())
      reserve.push_back(ship);
  }
  std::erase_if(m_reserveDestinations, [&reserve](const auto& _entry)
                { return std::ranges::none_of(reserve, [&_entry](const EntityView* _ship) { return _ship->id == _entry.first; }); });

  // A raid: a few of the reserve at the nearest enemy sector it sees no enemy warship guarding, not the enemy's home,
  // while the reserve holds at least twice as many (ADR-020 decision 13).
  if (m_raidGroup.empty() && m_settings.raidShips > 0 && _snapshot.tick >= m_nextRaidTick &&
      std::cmp_greater_equal(reserve.size(), 2 * m_settings.raidShips))
  {
    const SectorView* raided = nullptr;
    for (const SectorView& sector : _snapshot.sectors)
    {
      if (!sector.holder.IsValid() || sector.holder == m_player || sector.Contains(EnemyStations(_snapshot).front()))
        continue;
      const bool guarded = std::ranges::any_of(_snapshot.entities,
                                               [&](const EntityView& _entity)
                                               {
                                                 return _entity.kind == EntityKind::Ship && _entity.role == ShipRole::Warship &&
                                                        _entity.owner.IsValid() && _entity.owner != m_player &&
                                                        Distance(_entity.position, sector.node) <= RAID_GUARD_METERS;
                                               });
      if (!guarded && (raided == nullptr || IsNearer(sector.node, raided->node, m_home)))
        raided = &sector;
    }
    if (raided != nullptr)
    {
      // The smallest hulls, which are the fastest, go.
      std::vector<const EntityView*> fastest = reserve;
      std::ranges::sort(fastest, [](const EntityView* _a, const EntityView* _b)
                        { return _a->hull != _b->hull ? _a->hull < _b->hull : _a->id < _b->id; });
      fastest.resize(static_cast<size_t>(m_settings.raidShips));
      for (const EntityView* ship : fastest)
      {
        m_raidGroup.push_back(ship->id);
        m_reserveDestinations.erase(ship->id);
      }
      std::erase_if(reserve, [this](const EntityView* _ship) { return std::ranges::find(m_raidGroup, _ship->id) != m_raidGroup.end(); });
      m_raidSector = raided->id;
      m_raidLaunch = m_raidGroup.size();
      m_raidClosing.clear();
      _orders.push_back(MakeCommand(m_player, AttackMoveCommand{.ships = m_raidGroup, .destination = raided->node}));
    }
  }

  const EntityView* target = m_attackGroup.empty() ? nullptr : FindEntity(_snapshot, m_attackTarget);
  bool grown = false;
  std::int32_t groupShips = m_settings.attackGroupShips + ((OpenedTier(_snapshot) - 1) * m_settings.attackGroupGrowthPerTier);
  // The fleet cap may leave room for fewer ships of its production design's hull than that (Phase 4 design §5): the attack
  // then goes once the reserve holds attackCapShare of the room its scouts and its raid leave it under the cap.
  std::int64_t capShips = std::numeric_limits<std::int64_t>::max();
  if (const std::int32_t hullPoints = CommandPointsOf(_snapshot, m_productionDesign.hull); _snapshot.fleetCap > 0 && hullPoints > 0)
  {
    std::int32_t elsewhere = 0;
    for (const EntityView& ship : _snapshot.entities)
    {
      if (ship.kind == EntityKind::Ship && ship.role == ShipRole::Warship && ship.owner == m_player &&
          (IsScout(ship) || std::ranges::find(m_raidGroup, ship.id) != m_raidGroup.end()))
        elsewhere += CommandPointsOf(_snapshot, ship.hull);
    }
    capShips = std::max(1, (_snapshot.fleetCap - elsewhere) / hullPoints);
    const auto share = static_cast<std::int32_t>(std::floor(static_cast<double>(capShips) * m_settings.attackCapShare));
    groupShips = std::min(groupShips, std::max(1, share));
  }
  // On a map with territory the main attack waits for a lead in nodes, or for a larger reserve (ADR-041 decision 5).
  std::int32_t nodeLead = 0;
  for (const SectorView& sector : _snapshot.sectors)
    nodeLead += sector.holder == m_player ? 1 : sector.holder.IsValid() ? -1 : 0;
  const bool lead = _snapshot.sectors.empty() || nodeLead >= m_settings.attackNodeLead;
  // Without a lead it waits for more, but never for more than the cap has room for.
  const auto withoutLead = std::min(static_cast<std::int64_t>(std::ceil(groupShips * m_settings.attackWithoutLeadShare)),
                                    std::max<std::int64_t>(groupShips, capShips));
  if (std::cmp_greater_equal(reserve.size(), groupShips) && _snapshot.tick >= m_regroupUntilTick &&
      (lead || std::cmp_greater_equal(reserve.size(), withoutLead)))
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
    m_launchShips = m_attackGroup.size();
    if (target != nullptr)
      _orders.push_back(MakeCommand(m_player, AttackMoveCommand{.ships = std::move(joined), .destination = target->position}));
  }

  if (!m_attackGroup.empty())
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
    const EntityView* best = nullptr;
    for (const EntityView& entity : _snapshot.entities)
    {
      // It attacks the enemy, not the pirates (ADR-073).
      if (entity.kind != EntityKind::Structure || !entity.owner.IsValid() || entity.owner == m_player || entity.owner == PIRATES)
        continue;
      // Within a rank, a structure's levels bring it nearer: a Shipyard at level 3 is worth more than one at level 1
      // (Phase 3 design §8).
      const auto worthMeters = [&](const EntityView& _structure)
      {
        return Distance(_structure.position, center) -
               (static_cast<float>(m_settings.attackLevelMeters) * static_cast<float>(std::max(0, _structure.level - 1)));
      };
      if (best == nullptr || AttackRank(entity) < AttackRank(*best) ||
          (AttackRank(entity) == AttackRank(*best) && worthMeters(entity) < worthMeters(*best)))
        best = &entity;
    }
    // A target is kept until it is gone, or until production comes to light ahead of it.
    if (target == nullptr || (best != nullptr && AttackRank(*best) < AttackRank(*target)))
    {
      m_attackTarget = best != nullptr ? best->id : EntityId{};
      m_closingIn.clear();
      if (best != nullptr)
        _orders.push_back(MakeCommand(m_player, AttackMoveCommand{.ships = m_attackGroup, .destination = best->position}));
      else if (!m_searching || grown)
        _orders.push_back(MakeCommand(
          m_player, AttackMoveCommand{.ships = m_attackGroup, .destination = {.xMeters = -m_home.xMeters, .zMeters = -m_home.zMeters}}));
      m_searching = best == nullptr;
    }
  }

  // The attack group's ships near their target attack it, so that none stands idle out of range of it.
  if (const EntityView* attacked = m_attackGroup.empty() ? nullptr : FindEntity(_snapshot, m_attackTarget))
  {
    std::erase_if(m_closingIn, [this](EntityId _id) { return std::ranges::find(m_attackGroup, _id) == m_attackGroup.end(); });
    std::vector<EntityId> closing;
    for (const EntityId id : m_attackGroup)
    {
      const EntityView* ship = FindEntity(_snapshot, id);
      if (ship != nullptr && std::ranges::find(m_closingIn, id) == m_closingIn.end() &&
          Distance(ship->position, attacked->position) <= CLOSE_IN_METERS + attacked->radiusMeters)
        closing.push_back(id);
    }
    if (!closing.empty())
    {
      m_closingIn.insert(m_closingIn.end(), closing.begin(), closing.end());
      _orders.push_back(MakeCommand(m_player, AttackCommand{.ships = std::move(closing), .target = attacked->id}));
    }
  }

  const bool defending =
    m_lastDefenseShotTick.has_value() &&
    _snapshot.tick < *m_lastDefenseShotTick + static_cast<std::uint64_t>(std::llround(m_settings.defenseHoldSeconds * m_ticksPerSecond));
  // On a map with territory the reserve holds its front with a standing order: the sector it holds next to the enemy's,
  // nearest the rally, or the rally's own sector while it has no front (ADR-020 decision 13).
  const SectorView* held = defending ? nullptr : FrontSector(_snapshot);
  const PlanePosition destination = defending ? m_defensePosition : held != nullptr ? held->node : m_rally;
  std::vector<EntityId> sent;
  for (const EntityView* ship : reserve)
  {
    const auto last = m_reserveDestinations.find(ship->id);
    if (last != m_reserveDestinations.end() && last->second.holds == (held != nullptr) &&
        Distance(last->second.destination, destination) <= RESEND_METERS)
      continue;
    sent.push_back(ship->id);
    m_reserveDestinations[ship->id] = {.destination = destination, .holds = held != nullptr};
  }
  if (sent.empty())
    return;
  if (held != nullptr)
    _orders.push_back(MakeCommand(m_player, HoldSectorCommand{.ships = std::move(sent), .position = held->node}));
  else
    _orders.push_back(MakeCommand(m_player, AttackMoveCommand{.ships = std::move(sent), .destination = destination}));
}

// Two places as near are told apart by the side of the line from its base to the map's center each lies on, the same
// side for either seat of the point-symmetric map, so that both seats choose mirrored places where a sector's or an
// entity's identifier would have them choose differently (ADR-020 decision 13).
bool Outpost::AiPlayer::IsNearer(PlanePosition _a, PlanePosition _b, PlanePosition _from) const noexcept
{
  const float a = Distance(_a, _from);
  const float b = Distance(_b, _from);
  if (a != b)
    return a < b;
  const auto side = [this](PlanePosition _at) { return (m_home.zMeters * _at.xMeters) - (m_home.xMeters * _at.zMeters); };
  return side(_a) < side(_b);
}

const Outpost::SectorView* Outpost::AiPlayer::FrontSector(const Snapshot& _snapshot) const
{
  const SectorView* front = nullptr;
  for (const SectorView& sector : _snapshot.sectors)
  {
    if (sector.holder != m_player)
      continue;
    const bool facing = std::ranges::any_of(sector.adjacent,
                                            [&](std::int32_t _id)
                                            {
                                              const SectorView* other = FindSectorById(_snapshot, _id);
                                              return other != nullptr && other->holder.IsValid() && other->holder != m_player;
                                            });
    if (facing && (front == nullptr || IsNearer(sector.node, front->node, m_rally)))
      front = &sector;
  }
  if (front != nullptr)
    return front;
  const SectorView* rally = FindSector(_snapshot.sectors, m_rally);
  return rally != nullptr && rally->holder == m_player ? rally : nullptr;
}

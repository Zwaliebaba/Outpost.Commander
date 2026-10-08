#include "pch.h"
#include "Deputy.h"

namespace
{
using Outpost::EntityId;
using Outpost::EntityKind;
using Outpost::EntityView;
using Outpost::PlanePosition;
using Outpost::Snapshot;
using Outpost::StructureKind;

// A rig stands on its asteroid's center, and a structure where it was ordered; this allows for float rounding.
constexpr float SAME_PLACE_METERS = 1.0f;
// How long it waits to see a rig's site it ordered before it orders the rig again: the server says nothing of a refusal.
constexpr std::uint64_t SITE_WAIT_SECONDS = 3;
// A rig is rebuilt by at most this many Constructors.
constexpr size_t CONSTRUCTORS_PER_RIG = 2;
// A defender is sent again at the attack only once the attack has moved this far from where it went.
constexpr float RESEND_METERS = 150.0f;

float Distance(PlanePosition _a, PlanePosition _b) noexcept
{
  return std::hypot(_a.xMeters - _b.xMeters, _a.zMeters - _b.zMeters);
}

const EntityView* FindEntity(const Snapshot& _snapshot, EntityId _id) noexcept
{
  const auto found = std::ranges::lower_bound(_snapshot.entities, _id, {}, &EntityView::id);
  return found != _snapshot.entities.end() && found->id == _id ? &*found : nullptr;
}

bool IsStructure(const EntityView& _entity, StructureKind _kind) noexcept
{
  return _entity.kind == EntityKind::Structure && _entity.structure == _kind;
}

bool IsBuilt(const EntityView& _entity) noexcept
{
  return _entity.builtPermille >= Outpost::PERMILLE;
}

bool IsConstructor(const EntityView& _entity, Outpost::PlayerId _player) noexcept
{
  return _entity.kind == EntityKind::Ship && _entity.role == Outpost::ShipRole::Constructor && _entity.owner == _player;
}

bool IsWarship(const EntityView& _entity, Outpost::PlayerId _player) noexcept
{
  return _entity.kind == EntityKind::Ship && _entity.role == Outpost::ShipRole::Warship && _entity.owner == _player;
}

// A ship with nothing to do: no order, no standing order, not going back to be repaired (ADR-075), and waiting on no
// scheduled order of its player's, which an order of the deputy's would take it out of (ADR-080).
bool IsIdle(const EntityView& _ship) noexcept
{
  return _ship.order == Outpost::ShipOrder::None && _ship.standing == Outpost::StandingOrder::None && !_ship.retreating &&
         _ship.scheduledOrder == 0;
}

const Outpost::StructureTypeView* FindType(const Snapshot& _snapshot, StructureKind _kind) noexcept
{
  const auto found = std::ranges::find(_snapshot.structureTypes, _kind, &Outpost::StructureTypeView::structure);
  return found != _snapshot.structureTypes.end() ? &*found : nullptr;
}

const EntityView* FindOwned(const Snapshot& _snapshot, StructureKind _kind, Outpost::PlayerId _player) noexcept
{
  const auto found = std::ranges::find_if(_snapshot.entities, [&](const EntityView& _entity)
                                          { return IsStructure(_entity, _kind) && _entity.owner == _player; });
  return found != _snapshot.entities.end() ? &*found : nullptr;
}

std::int32_t CommandPointsOf(const Snapshot& _snapshot, Outpost::HullId _hull) noexcept
{
  const auto hull = std::ranges::find(_snapshot.hulls, _hull, &Outpost::HullView::id);
  return hull != _snapshot.hulls.end() ? hull->commandPoints : 0;
}

std::int32_t ShipyardLevelFor(const Snapshot& _snapshot, Outpost::HullId _hull) noexcept
{
  const auto hull = std::ranges::find(_snapshot.hulls, _hull, &Outpost::HullView::id);
  return hull != _snapshot.hulls.end() ? hull->shipyardLevel : 1;
}

// How many topics a Research Lab at _level researches at once (Phase 3 design §6), from the snapshot's levels.
std::int32_t ResearchSlotsOf(const Outpost::StructureTypeView* _lab, std::int32_t _level) noexcept
{
  std::int32_t slots = 1;
  for (size_t level = 0; _lab != nullptr && level < _lab->levels.size() && std::cmp_less(level + 1, _level); ++level)
    slots = std::max(slots, _lab->levels[level].researchSlots);
  return slots;
}

// The Constructors _player has, and those its Command Station has queued.
std::int32_t ConstructorsOf(const Snapshot& _snapshot, Outpost::PlayerId _player)
{
  auto count = std::ranges::count_if(_snapshot.entities, [_player](const EntityView& _entity) { return IsConstructor(_entity, _player); });
  if (const EntityView* station = FindOwned(_snapshot, StructureKind::CommandStation, _player))
    count += std::ranges::count(station->queue, Outpost::ShipRole::Constructor, &Outpost::JobView::role);
  return static_cast<std::int32_t>(count);
}

// The hull of the front job in a Shipyard's queue, which waits for the fleet cap; none for an empty queue, or a job of a
// design the player no longer has.
Outpost::HullId HullOfJob(const Snapshot& _snapshot, const EntityView& _yard)
{
  if (_yard.queue.empty())
    return {};
  const auto design = std::ranges::find(_snapshot.designs, _yard.queue.front().design, &Outpost::DesignView::id);
  return design != _snapshot.designs.end() ? design->hull : Outpost::HullId{};
}

template <typename Order> Outpost::Command MakeCommand(Outpost::PlayerId _player, Order _order)
{
  return {.player = _player, .order = std::move(_order)};
}
} // namespace

Outpost::Deputy::Deputy(AiSettings _settings, std::uint32_t _ticksPerSecond)
  : m_settings(std::move(_settings)),
    m_ticksPerSecond(_ticksPerSecond)
{
  if (m_ticksPerSecond == 0)
    throw Neuron::Exception("Deputy: the server runs no ticks");
}

void Outpost::Deputy::Watch(const Snapshot& _snapshot)
{
  m_player = _snapshot.player;
  m_watching = true;
  // Once a second, as it decides when it plays: a queue's last job and a rig stand for many seconds.
  if (_snapshot.tick >= m_nextDecisionTick)
  {
    m_nextDecisionTick = _snapshot.tick + m_ticksPerSecond;
    Learn(_snapshot);
  }
}

std::vector<Outpost::Command> Outpost::Deputy::Play(const Snapshot& _snapshot)
{
  m_player = _snapshot.player;
  if (m_watching)
  {
    // Its turn begins. It keeps the Constructors its player has now, and answers what it sees from here on, not what it
    // was doing at its last turn.
    m_watching = false;
    m_constructors = ConstructorsOf(_snapshot, m_player);
    m_repairs.clear();
    m_defenders.clear();
    m_threatTick.reset();
    m_structures.clear();
    m_nextDecisionTick = _snapshot.tick;
  }

  // Every tick, since a shot is in the snapshot of its tick only. The structures of the tick before are remembered too,
  // since the shot that destroys one names a structure that has already left the snapshot. On a map with territory only a
  // structure in a sector its player holds is defended.
  std::vector<std::pair<EntityId, PlanePosition>> structures;
  for (const EntityView& entity : _snapshot.entities)
  {
    const SectorView* sector = FindSector(_snapshot.sectors, entity.position);
    if (entity.kind == EntityKind::Structure && entity.owner == m_player &&
        (_snapshot.sectors.empty() || (sector != nullptr && sector->holder == m_player)))
      structures.emplace_back(entity.id, entity.position);
  }
  for (const ShotView& shot : _snapshot.shots)
  {
    for (const auto* known : {&structures, &m_structures})
    {
      const auto target = std::ranges::find(*known, shot.target, &std::pair<EntityId, PlanePosition>::first);
      if (target == known->end())
        continue;
      m_threat = target->second;
      m_threatTick = _snapshot.tick;
      break;
    }
  }
  m_structures = std::move(structures);

  std::vector<Command> orders;
  if (_snapshot.tick >= m_nextDecisionTick)
  {
    m_nextDecisionTick = _snapshot.tick + m_ticksPerSecond;
    Learn(_snapshot);
    Decide(_snapshot, orders);
  }
  return orders;
}

void Outpost::Deputy::Learn(const Snapshot& _snapshot)
{
  std::erase_if(m_lastBuilt, [&_snapshot](const auto& _entry) { return FindEntity(_snapshot, _entry.first) == nullptr; });
  for (const EntityView& entity : _snapshot.entities)
  {
    if (entity.owner != m_player || entity.kind != EntityKind::Structure)
      continue;
    // The last job queued is the one a queue builds last before it empties.
    if (entity.structure == StructureKind::Shipyard && !entity.queue.empty() && entity.queue.back().role == ShipRole::Warship)
      m_lastBuilt[entity.id] = entity.queue.back().design;
    else if (entity.structure == StructureKind::MiningRig &&
             std::ranges::none_of(m_rigSites,
                                  [&entity](PlanePosition _site) { return Distance(_site, entity.position) <= SAME_PLACE_METERS; }))
      m_rigSites.push_back(entity.position);
  }
}

void Outpost::Deputy::Decide(const Snapshot& _snapshot, std::vector<Command>& _orders)
{
  std::int32_t ore = _snapshot.ore;
  Research(_snapshot, _orders);

  // Its idle Constructors, nearest the work first as each job takes them.
  std::erase_if(m_repairs,
                [&_snapshot](const auto& _repair)
                {
                  const EntityView* constructor = FindEntity(_snapshot, _repair.first);
                  return constructor == nullptr || constructor->order != ShipOrder::Work;
                });
  std::vector<EntityId> idle;
  for (const EntityView& ship : _snapshot.entities)
  {
    if (IsConstructor(ship, m_player) && IsIdle(ship) && !m_repairs.contains(ship.id))
      idle.push_back(ship.id);
  }
  RebuildRigs(_snapshot, idle, ore, _orders);
  Repair(_snapshot, idle, _orders);
  KeepConstructors(_snapshot, _orders);
  if (Produce(_snapshot, _orders))
    UpgradeStation(_snapshot, ore, _orders);
  Defend(_snapshot, _orders);
}

// One topic at a time while its Lab has a slot free: the next of the AI's order, and once that order is done, the first
// topic of the tuning data's that the Lab can take (Phase 3 design §6).
void Outpost::Deputy::Research(const Snapshot& _snapshot, std::vector<Command>& _orders) const
{
  const EntityView* lab = FindOwned(_snapshot, StructureKind::ResearchLab, m_player);
  if (lab == nullptr || !IsBuilt(*lab) ||
      std::cmp_greater_equal(lab->research.size(), ResearchSlotsOf(FindType(_snapshot, StructureKind::ResearchLab), lab->level)))
    return;
  const auto researched = [&_snapshot](ResearchTopicId _id)
  {
    const auto topic = std::ranges::find(_snapshot.research, _id, &ResearchTopicView::id);
    return topic != _snapshot.research.end() && topic->researched;
  };
  const auto takes = [&](const ResearchTopicView& _topic)
  {
    return !_topic.researched && _topic.tier <= _snapshot.researchTier && !std::ranges::contains(lab->research, _topic.id) &&
           std::ranges::all_of(_topic.prerequisites, researched);
  };
  for (const ResearchTopicId id : m_settings.researchOrder)
  {
    const auto topic = std::ranges::find(_snapshot.research, id, &ResearchTopicView::id);
    if (topic != _snapshot.research.end() && takes(*topic))
    {
      _orders.push_back(MakeCommand(m_player, StartResearchCommand{.lab = lab->id, .topic = id}));
      return;
    }
  }
  const auto any = std::ranges::find_if(_snapshot.research, takes);
  if (any != _snapshot.research.end())
    _orders.push_back(MakeCommand(m_player, StartResearchCommand{.lab = lab->id, .topic = any->id}));
}

const Outpost::DesignView* Outpost::Deputy::NextDesign(const Snapshot& _snapshot, const EntityView& _yard) const
{
  const auto buildable = [&](DesignId _id) -> const DesignView*
  {
    const auto design = std::ranges::find(_snapshot.designs, _id, &DesignView::id);
    return design != _snapshot.designs.end() && ShipyardLevelFor(_snapshot, design->hull) <= _yard.level ? &*design : nullptr;
  };
  if (const auto last = m_lastBuilt.find(_yard.id); last != m_lastBuilt.end())
    return buildable(last->second);
  // A Shipyard it has never seen build, as after the server restarted: the design most of its player's warships are of,
  // the oldest of those as many.
  std::map<DesignId, std::int32_t> fleet;
  for (const EntityView& ship : _snapshot.entities)
  {
    if (IsWarship(ship, m_player) && buildable(ship.design) != nullptr)
      ++fleet[ship.design];
  }
  const auto most = std::ranges::max_element(fleet, [](const auto& _a, const auto& _b)
                                             { return _a.second != _b.second ? _a.second < _b.second : _b.first < _a.first; });
  return most != fleet.end() ? buildable(most->first) : nullptr;
}

// A job waits in its queue until the Ore pays for it and the fleet cap has room for it (Phase 1 design §5, Phase 4 design §5),
// so a Shipyard kept one job ahead starts its next ship as soon as it can, and spends nothing until then.
bool Outpost::Deputy::Produce(const Snapshot& _snapshot, std::vector<Command>& _orders) const
{
  bool capped = false;
  for (const EntityView& yard : _snapshot.entities)
  {
    if (!IsStructure(yard, StructureKind::Shipyard) || yard.owner != m_player || !IsBuilt(yard))
      continue;
    const DesignView* design = yard.queue.empty() ? NextDesign(_snapshot, yard) : nullptr;
    if (design != nullptr)
      _orders.push_back(MakeCommand(m_player, QueueShipCommand{.producer = yard.id, .design = design->id}));
    // What it builds next waits for the cap.
    const auto front = yard.queue.empty() ? design : nullptr;
    const HullId hull = front != nullptr ? front->hull : HullOfJob(_snapshot, yard);
    capped = capped ||
             (_snapshot.fleetCap > 0 && hull.IsValid() && _snapshot.commandPoints + CommandPointsOf(_snapshot, hull) > _snapshot.fleetCap);
  }
  return capped;
}

// The Command Station's next level, once it can be upgraded and the Ore is there (Phase 4 design §12), as the AI upgrades
// its own when the cap holds its Shipyards back.
void Outpost::Deputy::UpgradeStation(const Snapshot& _snapshot, std::int32_t& _ore, std::vector<Command>& _orders) const
{
  const StructureTypeView* type = FindType(_snapshot, StructureKind::CommandStation);
  const EntityView* station = FindOwned(_snapshot, StructureKind::CommandStation, m_player);
  if (type == nullptr || station == nullptr || !IsBuilt(*station) || station->upgradePermille.has_value() ||
      std::cmp_greater_equal(station->level - 1, type->levels.size()))
    return;
  const std::int32_t cost = type->levels[static_cast<size_t>(station->level - 1)].cost;
  if (_ore < cost)
    return;
  _orders.push_back(MakeCommand(m_player, UpgradeStructureCommand{.structure = station->id}));
  _ore -= cost;
}

void Outpost::Deputy::KeepConstructors(const Snapshot& _snapshot, std::vector<Command>& _orders) const
{
  const EntityView* station = FindOwned(_snapshot, StructureKind::CommandStation, m_player);
  if (station == nullptr || !IsBuilt(*station) || station->queue.size() >= QUEUE_LIMIT ||
      ConstructorsOf(_snapshot, m_player) >= m_constructors)
    return;
  _orders.push_back(MakeCommand(m_player, QueueShipCommand{.producer = station->id, .design = {}}));
}

void Outpost::Deputy::RebuildRigs(const Snapshot& _snapshot, std::vector<EntityId>& _idle, std::int32_t& _ore,
                                  std::vector<Command>& _orders)
{
  const StructureTypeView* rig = FindType(_snapshot, StructureKind::MiningRig);
  if (rig == nullptr || !rig->buildable)
    return;
  const std::uint64_t waitTicks = SITE_WAIT_SECONDS * m_ticksPerSecond;
  std::erase_if(m_rigOrders, [&](const auto& _order) { return _snapshot.tick > _order.second + waitTicks; });
  for (const PlanePosition site : m_rigSites)
  {
    if (_idle.empty() || _ore < rig->cost)
      return;
    const auto at = [site](const EntityView& _entity) { return Distance(_entity.position, site) <= SAME_PLACE_METERS; };
    const bool standing = std::ranges::any_of(_snapshot.entities, [&](const EntityView& _entity)
                                              { return IsStructure(_entity, StructureKind::MiningRig) && at(_entity); });
    const auto asteroid = std::ranges::find_if(_snapshot.entities, [&](const EntityView& _entity)
                                               { return _entity.kind == EntityKind::Asteroid && at(_entity); });
    if (standing || asteroid == _snapshot.entities.end() || asteroid->oreReserveHundredths.value_or(1) <= 0 ||
        std::ranges::contains(m_rigOrders, site, &std::pair<PlanePosition, std::uint64_t>::first))
      continue;
    // A rig stands only in a sector its player holds, on a map with territory (ADR-056).
    if (const SectorView* sector = FindSector(_snapshot.sectors, site);
        !_snapshot.sectors.empty() && (sector == nullptr || sector->holder != m_player || sector->guarded))
      continue;
    std::ranges::sort(_idle,
                      [&](EntityId _a, EntityId _b)
                      {
                        const float a = Distance(FindEntity(_snapshot, _a)->position, site);
                        const float b = Distance(FindEntity(_snapshot, _b)->position, site);
                        return a != b ? a < b : _a < _b;
                      });
    const auto crew = static_cast<std::ptrdiff_t>(std::min(CONSTRUCTORS_PER_RIG, _idle.size()));
    _orders.push_back(MakeCommand(m_player, BuildStructureCommand{.constructors = {_idle.begin(), _idle.begin() + crew},
                                                                  .structure = StructureKind::MiningRig,
                                                                  .position = site}));
    _idle.erase(_idle.begin(), _idle.begin() + crew);
    _ore -= rig->cost;
    m_rigOrders.emplace_back(site, _snapshot.tick);
  }
}

// A site nobody builds, and a damaged structure nobody repairs, each get the nearest idle Constructor.
void Outpost::Deputy::Repair(const Snapshot& _snapshot, std::vector<EntityId>& _idle, std::vector<Command>& _orders)
{
  for (const EntityView& structure : _snapshot.entities)
  {
    if (_idle.empty())
      return;
    if (structure.kind != EntityKind::Structure || structure.owner != m_player ||
        (IsBuilt(structure) && structure.hitPointsHundredths >= structure.maxHitPointsHundredths) ||
        std::ranges::any_of(m_repairs, [&structure](const auto& _repair) { return _repair.second == structure.id; }))
      continue;
    const auto nearest = std::ranges::min_element(_idle,
                                                  [&](EntityId _a, EntityId _b)
                                                  {
                                                    const float a = Distance(FindEntity(_snapshot, _a)->position, structure.position);
                                                    const float b = Distance(FindEntity(_snapshot, _b)->position, structure.position);
                                                    return a != b ? a < b : _a < _b;
                                                  });
    const EntityId constructor = *nearest;
    _idle.erase(nearest);
    _orders.push_back(MakeCommand(m_player, RepairCommand{.constructors = {constructor}, .target = structure.id}));
    m_repairs[constructor] = structure.id;
  }
}

// An attack is enemy warships, pirates' among them, in a sector its player holds, the sector with the most of them first;
// or, out of sight or on a map without territory, whatever fired on one of its player's structures there. Its idle warships go
// at it, and once it has been over for the AI's defence hold, each goes back to where it stood.
void Outpost::Deputy::Defend(const Snapshot& _snapshot, std::vector<Command>& _orders)
{
  const SectorView* attacked = nullptr;
  size_t attackers = 0;
  PlanePosition center;
  for (const SectorView& sector : _snapshot.sectors)
  {
    if (sector.holder != m_player)
      continue;
    float sumX = 0.0f;
    float sumZ = 0.0f;
    size_t count = 0;
    for (const EntityView& ship : _snapshot.entities)
    {
      if (ship.kind == EntityKind::Ship && ship.role == ShipRole::Warship && ship.owner.IsValid() && ship.owner != m_player &&
          sector.Contains(ship.position))
      {
        sumX += ship.position.xMeters;
        sumZ += ship.position.zMeters;
        ++count;
      }
    }
    if (count > attackers)
    {
      attacked = &sector;
      attackers = count;
      center = {.xMeters = sumX / static_cast<float>(count), .zMeters = sumZ / static_cast<float>(count)};
    }
  }
  if (attacked != nullptr)
  {
    m_threat = center;
    m_threatTick = _snapshot.tick;
  }
  // A sector its player has lost since is defended no more: going on would be attacking.
  if (const SectorView* sector = FindSector(_snapshot.sectors, m_threat);
      m_threatTick.has_value() && !_snapshot.sectors.empty() && (sector == nullptr || sector->holder != m_player))
    m_threatTick.reset();

  std::erase_if(m_defenders,
                [&_snapshot](const auto& _defender)
                {
                  const EntityView* ship = FindEntity(_snapshot, _defender.first);
                  return ship == nullptr || ship->retreating;
                });
  const auto holdTicks = static_cast<std::uint64_t>(std::llround(m_settings.defenseHoldSeconds * m_ticksPerSecond));
  if (!m_threatTick.has_value() || _snapshot.tick >= *m_threatTick + holdTicks)
  {
    // The attack is over: each defender goes back to where it stood.
    for (const auto& [id, origin] : m_defenders)
      _orders.push_back(MakeCommand(m_player, MoveCommand{.ships = {id}, .destination = origin}));
    m_defenders.clear();
    m_threatTick.reset();
    return;
  }

  std::vector<EntityId> sent;
  for (const EntityView& ship : _snapshot.entities)
  {
    if (!IsWarship(ship, m_player) || ship.retreating)
      continue;
    const auto defender = m_defenders.find(ship.id);
    // A defender that has stopped short of where the attack now is goes again.
    const bool resend = defender != m_defenders.end() && ship.order == ShipOrder::None && Distance(ship.position, m_threat) > RESEND_METERS;
    if (defender == m_defenders.end() && IsIdle(ship))
    {
      m_defenders[ship.id] = ship.position;
      sent.push_back(ship.id);
    }
    else if (resend)
      sent.push_back(ship.id);
  }
  if (!sent.empty())
    _orders.push_back(MakeCommand(m_player, AttackMoveCommand{.ships = std::move(sent), .destination = m_threat}));
}

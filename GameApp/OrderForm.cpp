#include "pch.h"
#include "OrderForm.h"

#include <algorithm>

namespace
{
constexpr std::array TRIGGERS{Outpost::ScheduledTriggerKind::TimeOfDay, Outpost::ScheduledTriggerKind::EnemyInSector,
                              Outpost::ScheduledTriggerKind::RelayThreatened, Outpost::ScheduledTriggerKind::RigLost};
constexpr std::array ACTIONS{Outpost::ScheduledActionKind::Move,   Outpost::ScheduledActionKind::AttackMove,
                             Outpost::ScheduledActionKind::Attack, Outpost::ScheduledActionKind::HoldSector,
                             Outpost::ScheduledActionKind::Patrol, Outpost::ScheduledActionKind::BuildRig};
constexpr int HOURS = 24;
constexpr int MINUTES = 60;

// One step forward, or back for a negative _step.
int Sign(int _step) noexcept
{
  return _step < 0 ? -1 : 1;
}

// The value _step from _value among _count values from zero, round the end.
int Around(int _value, int _step, int _count) noexcept
{
  return (((_value + _step) % _count) + _count) % _count;
}

// The item one step from _current among _items, round the end; the first when _current is not among them, and _none when
// there are no items.
template <class T> T StepAmong(const std::vector<T>& _items, T _current, int _step, T _none)
{
  if (_items.empty())
    return _none;
  const auto at = std::ranges::find(_items, _current);
  if (at == _items.end())
    return _items.front();
  const int index = static_cast<int>(at - _items.begin());
  return _items[static_cast<std::size_t>(Around(index, Sign(_step), static_cast<int>(_items.size())))];
}

// _current, when it is among _items; otherwise the first, or _none when there are no items.
template <class T> T KeptAmong(const std::vector<T>& _items, T _current, T _none)
{
  if (std::ranges::find(_items, _current) != _items.end())
    return _current;
  return _items.empty() ? _none : _items.front();
}

std::vector<std::int32_t> IdsOf(const std::vector<const Outpost::SectorView*>& _sectors)
{
  std::vector<std::int32_t> ids;
  ids.reserve(_sectors.size());
  for (const Outpost::SectorView* sector : _sectors)
    ids.push_back(sector->id);
  return ids;
}

std::vector<std::int32_t> SectorIdsOf(const Outpost::Snapshot& _newest)
{
  std::vector<std::int32_t> ids;
  ids.reserve(_newest.sectors.size());
  for (const Outpost::SectorView& sector : _newest.sectors)
    ids.push_back(sector.id);
  return ids;
}

std::vector<Outpost::EntityId> IdsOf(const std::vector<const Outpost::EntityView*>& _entities)
{
  std::vector<Outpost::EntityId> ids;
  ids.reserve(_entities.size());
  for (const Outpost::EntityView* entity : _entities)
    ids.push_back(entity->id);
  return ids;
}

std::string SectorNameOf(const Outpost::Snapshot& _newest, std::int32_t _sector)
{
  const auto sector = std::ranges::find(_newest.sectors, _sector, &Outpost::SectorView::id);
  return sector != _newest.sectors.end() ? sector->nameUtf8 : std::string("the field");
}
} // namespace

void Outpost::OrderForm::Step(OrderField _field, int _step, const Snapshot& _newest, std::span<const EntityView> _entities)
{
  switch (_field)
  {
  case OrderField::Trigger:
  {
    const auto at = std::ranges::find(TRIGGERS, m_trigger);
    m_trigger =
      TRIGGERS[static_cast<std::size_t>(Around(static_cast<int>(at - TRIGGERS.begin()), Sign(_step), static_cast<int>(TRIGGERS.size())))];
    break;
  }
  case OrderField::Hour:
    m_hour = Around(m_hour, Sign(_step), HOURS);
    break;
  case OrderField::Minute:
    m_minute = Around(m_minute / MINUTE_STEP, Sign(_step), MINUTES / MINUTE_STEP) * MINUTE_STEP;
    break;
  case OrderField::TriggerSector:
    m_triggerSector = StepAmong(IdsOf(WatchedSectors(_newest)), m_triggerSector, _step, 0);
    break;
  case OrderField::Action:
  {
    const auto at = std::ranges::find(ACTIONS, m_action);
    m_action =
      ACTIONS[static_cast<std::size_t>(Around(static_cast<int>(at - ACTIONS.begin()), Sign(_step), static_cast<int>(ACTIONS.size())))];
    break;
  }
  case OrderField::ActionSector:
    m_actionSector = StepAmong(SectorIdsOf(_newest), m_actionSector, _step, 0);
    break;
  case OrderField::Target:
    m_target = StepAmong(IdsOf(Targets(_newest, _entities)), m_target, _step, EntityId{});
    break;
  case OrderField::Condition:
  {
    // From none up to the most and back to none, round the end.
    const std::int32_t next = m_condition.has_value() ? *m_condition + (Sign(_step) * CONDITION_STEP) : _step > 0 ? 0 : MOST_COMMAND_POINTS;
    m_condition = next < 0 || next > MOST_COMMAND_POINTS ? std::nullopt : std::optional<std::int32_t>(next);
    break;
  }
  }
  Update(_newest, _entities);
}

void Outpost::OrderForm::Update(const Snapshot& _newest, std::span<const EntityView> _entities)
{
  m_triggerSector = KeptAmong(IdsOf(WatchedSectors(_newest)), m_triggerSector, 0);
  m_actionSector = KeptAmong(SectorIdsOf(_newest), m_actionSector, 0);
  m_target = TakesTarget() ? KeptAmong(IdsOf(Targets(_newest, _entities)), m_target, EntityId{}) : EntityId{};
}

std::vector<const Outpost::SectorView*> Outpost::OrderForm::WatchedSectors(const Snapshot& _newest)
{
  std::vector<const SectorView*> sectors;
  for (const SectorView& sector : _newest.sectors)
  {
    if (sector.holder == _newest.player)
      sectors.push_back(&sector);
  }
  return sectors;
}

std::vector<const Outpost::EntityView*> Outpost::OrderForm::Targets(const Snapshot& _newest, std::span<const EntityView> _entities) const
{
  std::vector<const EntityView*> targets;
  if (!TakesTarget())
    return targets;
  const auto sector = std::ranges::find(_newest.sectors, m_actionSector, &SectorView::id);
  if (!_newest.sectors.empty() && sector == _newest.sectors.end())
    return targets;
  const auto there = [&](const EntityView& _entity) { return sector == _newest.sectors.end() || sector->Contains(_entity.position); };
  if (m_action == ScheduledActionKind::BuildRig)
  {
    for (const EntityView& entity : _entities)
    {
      if (entity.kind == EntityKind::Asteroid && entity.oreReserveHundredths.value_or(1) > 0 && there(entity))
        targets.push_back(&entity);
    }
    return targets;
  }
  for (const EntityKind kind : {EntityKind::Structure, EntityKind::Ship})
  {
    for (const EntityView& entity : _entities)
    {
      if (entity.kind == kind && entity.owner.IsValid() && entity.owner != _newest.player && there(entity))
        targets.push_back(&entity);
    }
  }
  return targets;
}

std::vector<Outpost::EntityId> Outpost::OrderForm::ShipsFor(std::span<const EntityId> _selected,
                                                            std::span<const EntityView> _entities) const
{
  const ShipRole role = m_action == ScheduledActionKind::BuildRig ? ShipRole::Constructor : ShipRole::Warship;
  std::vector<EntityId> ships;
  for (const EntityId id : _selected)
  {
    const auto entity = std::ranges::find(_entities, id, &EntityView::id);
    if (entity != _entities.end() && entity->kind == EntityKind::Ship && entity->role == role)
      ships.push_back(id);
  }
  return ships;
}

std::string Outpost::OrderForm::Missing(std::span<const EntityId> _selected, const Snapshot& _newest,
                                        std::span<const EntityView> _entities) const
{
  if (ShipsFor(_selected, _entities).empty())
    return m_action == ScheduledActionKind::BuildRig ? "SELECT CONSTRUCTORS" : "SELECT WARSHIPS";
  const std::vector<std::int32_t> watched = IdsOf(WatchedSectors(_newest));
  if (m_trigger != ScheduledTriggerKind::TimeOfDay && std::ranges::find(watched, m_triggerSector) == watched.end())
    return "NO SECTOR HELD";
  if (TakesTarget())
  {
    const std::vector<EntityId> targets = IdsOf(Targets(_newest, _entities));
    if (std::ranges::find(targets, m_target) == targets.end())
      return m_action == ScheduledActionKind::BuildRig ? "NO ASTEROID THERE" : "NO ENEMY SEEN THERE";
  }
  else if (std::ranges::find(_newest.sectors, m_actionSector, &SectorView::id) == _newest.sectors.end())
    return "NO SECTOR";
  return {};
}

std::optional<Outpost::ScheduleOrderCommand> Outpost::OrderForm::Command(std::span<const EntityId> _selected, std::chrono::sys_seconds _now,
                                                                         const PlayerClock& _clock, const Snapshot& _newest,
                                                                         std::span<const EntityView> _entities) const
{
  if (!Missing(_selected, _newest, _entities).empty())
    return std::nullopt;
  ScheduleOrderCommand order{.ships = ShipsFor(_selected, _entities),
                             .trigger = {.kind = m_trigger},
                             .action = {.kind = m_action},
                             .unlessCommandPoints = m_condition};
  if (m_trigger == ScheduledTriggerKind::TimeOfDay)
    order.trigger.utcSeconds = _clock.NextMoment(_now, m_hour, m_minute).time_since_epoch().count();
  else
    order.trigger.sector = m_triggerSector;
  if (TakesTarget())
  {
    // A rig stands on the asteroid at its point; an attack names its target too.
    order.action.position = std::ranges::find(_entities, m_target, &EntityView::id)->position;
    if (m_action == ScheduledActionKind::Attack)
      order.action.target = m_target;
  }
  else
    order.action.position = std::ranges::find(_newest.sectors, m_actionSector, &SectorView::id)->node;
  return order;
}

std::string_view Outpost::OrderForm::NameOf(ScheduledTriggerKind _trigger) noexcept
{
  switch (_trigger)
  {
  case ScheduledTriggerKind::TimeOfDay:
    return "At a time of day";
  case ScheduledTriggerKind::EnemyInSector:
    return "Enemies enter a sector";
  case ScheduledTriggerKind::RelayThreatened:
    return "A Relay is threatened";
  case ScheduledTriggerKind::RigLost:
    return "A Mining Rig is lost";
  }
  return {};
}

std::string_view Outpost::OrderForm::NameOf(ScheduledActionKind _action) noexcept
{
  switch (_action)
  {
  case ScheduledActionKind::Move:
    return "Move";
  case ScheduledActionKind::AttackMove:
    return "Attack-move";
  case ScheduledActionKind::Attack:
    return "Attack";
  case ScheduledActionKind::HoldSector:
    return "Hold the sector";
  case ScheduledActionKind::Patrol:
    return "Patrol";
  case ScheduledActionKind::BuildRig:
    return "Build a Mining Rig";
  }
  return {};
}

std::string Outpost::OrderForm::NameOf(const EntityView& _target, const Snapshot& _newest)
{
  if (_target.kind == EntityKind::Asteroid)
  {
    return _target.oreReserveHundredths.has_value()
             ? std::format("Asteroid, Ore {}", WithThousands((*_target.oreReserveHundredths + HUNDREDTHS - 1) / HUNDREDTHS))
             : std::string("Asteroid");
  }
  std::string name;
  if (_target.kind == EntityKind::Structure)
  {
    const auto type = std::ranges::find(_newest.structureTypes, _target.structure, &StructureTypeView::structure);
    name = type != _newest.structureTypes.end() ? type->nameUtf8 : std::string("Structure");
  }
  else
  {
    const auto hull = std::ranges::find(_newest.hulls, _target.hull, &HullView::id);
    name = std::format("{} ship", hull != _newest.hulls.end() ? hull->nameUtf8 : std::string("Enemy"));
  }
  return _target.owner == PIRATES ? std::format("Pirate {}", name) : name;
}

std::string Outpost::DescribeScheduledOrder(const ScheduledOrderView& _order, const Snapshot& _newest, const PlayerClock& _clock)
{
  std::string when;
  switch (_order.trigger.kind)
  {
  case ScheduledTriggerKind::TimeOfDay:
    when = std::format("at {}", _clock.Reading(std::chrono::sys_seconds{std::chrono::seconds{_order.trigger.utcSeconds}}));
    break;
  case ScheduledTriggerKind::EnemyInSector:
    when = std::format("when enemies enter {}", SectorNameOf(_newest, _order.trigger.sector));
    break;
  case ScheduledTriggerKind::RelayThreatened:
    when = std::format("when the Relay in {} is threatened", SectorNameOf(_newest, _order.trigger.sector));
    break;
  case ScheduledTriggerKind::RigLost:
    when = std::format("when a Mining Rig in {} is lost", SectorNameOf(_newest, _order.trigger.sector));
    break;
  }
  const SectorView* place = FindSector(_newest.sectors, _order.action.position);
  std::string text =
    std::format("{}, {}", when, DescribeScheduledAction(_order.action.kind, place != nullptr ? place->nameUtf8 : std::string("the field")));
  if (_order.unlessCommandPoints.has_value())
    text += std::format(", unless over {} enemy CP", *_order.unlessCommandPoints);
  return text;
}

std::optional<std::string> Outpost::PendingOrderLine(std::span<const EntityId> _selected, std::span<const EntityView> _entities,
                                                     const Snapshot& _newest, const PlayerClock& _clock)
{
  std::vector<std::uint32_t> orders;
  for (const EntityId id : _selected)
  {
    const auto ship = std::ranges::find(_entities, id, &EntityView::id);
    if (ship != _entities.end() && ship->scheduledOrder != 0 && std::ranges::find(orders, ship->scheduledOrder) == orders.end())
      orders.push_back(ship->scheduledOrder);
  }
  if (orders.empty())
    return std::nullopt;
  if (orders.size() > 1)
    return std::format("Scheduled: {} orders", orders.size());
  const auto order = std::ranges::find(_newest.scheduled, orders.front(), &ScheduledOrderView::id);
  return order != _newest.scheduled.end() ? std::format("Scheduled: {}", DescribeScheduledOrder(*order, _newest, _clock))
                                          : std::string("Scheduled: an order");
}

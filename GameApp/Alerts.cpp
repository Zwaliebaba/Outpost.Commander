#include "pch.h"
#include "Alerts.h"

#include <algorithm>

namespace
{
// How many alerts are kept, for the list and the key: more than are ever shown.
constexpr std::size_t KEPT_ALERTS = 16;
} // namespace

std::string Outpost::DescribeScheduledAction(ScheduledActionKind _action, std::string_view _where)
{
  switch (_action)
  {
  case ScheduledActionKind::Move:
    return std::format("move to {}", _where);
  case ScheduledActionKind::AttackMove:
    return std::format("attack-move to {}", _where);
  case ScheduledActionKind::Attack:
    return std::format("attack in {}", _where);
  case ScheduledActionKind::HoldSector:
    return std::format("hold {}", _where);
  case ScheduledActionKind::Patrol:
    return std::format("patrol to {}", _where);
  case ScheduledActionKind::BuildRig:
    return std::format("Mining Rig in {}", _where);
  }
  return std::string(_where);
}

std::string Outpost::DescribeFiredOrder(const EventView& _event, std::string_view _where)
{
  const std::string action = DescribeScheduledAction(_event.action, _where);
  switch (_event.outcome)
  {
  case OrderOutcome::HeldInstead:
    return std::format("Order held back: {}", action);
  case OrderOutcome::Refused:
    return std::format("Order refused: {}", action);
  case OrderOutcome::AsGiven:
    break;
  }
  return std::format("Order fired: {}", action);
}

void Outpost::Alerts::Reset() noexcept
{
  m_alerts.clear();
}

void Outpost::Alerts::Raise(Kind _kind, std::string _text, PlanePosition _position, std::int32_t _sector, std::uint64_t _tick,
                            std::uint32_t _ticksPerSecond)
{
  const std::uint64_t repeatTicks = std::uint64_t{REPEAT_SECONDS} * _ticksPerSecond;
  // Every order fired is told, however close together.
  const bool repeated =
    _kind != Kind::OrderFired &&
    std::ranges::any_of(m_alerts, [&](const Alert& _alert)
                        { return _alert.kind == _kind && _alert.sector == _sector && _tick < _alert.tick + repeatTicks; });
  if (repeated)
    return;
  m_alerts.push_back({.kind = _kind, .text = std::move(_text), .position = _position, .sector = _sector, .tick = _tick});
  if (m_alerts.size() > KEPT_ALERTS)
    m_alerts.erase(m_alerts.begin());
}

void Outpost::Alerts::Observe(const Snapshot& _snapshot, std::uint32_t _ticksPerSecond)
{
  for (const EventView& event : _snapshot.events)
  {
    const auto sector = std::ranges::find(_snapshot.sectors, event.sector, &SectorView::id);
    const std::string where = sector != _snapshot.sectors.end() ? sector->nameUtf8 : std::string("the field");
    const auto raise = [&](Kind _kind, std::string _text)
    { Raise(_kind, std::move(_text), event.position, event.sector, _snapshot.tick, _ticksPerSecond); };
    switch (event.kind)
    {
    case EventKind::RelaySuppressed:
      raise(Kind::RelaySuppressed, std::format("Relay suppressed: {}", where));
      break;
    case EventKind::RelayAttacked:
      raise(Kind::RelayAttacked, std::format("Relay under attack: {}", where));
      break;
    case EventKind::StructureLost:
      if (event.structure == StructureKind::MiningRig)
        raise(Kind::RigLost, std::format("Mining Rig lost: {}", where));
      break;
    case EventKind::EnemyEntered:
      raise(Kind::EnemyEntered, std::format("{} ships in {}", event.other == PIRATES ? "Pirate" : "Enemy", where));
      break;
    case EventKind::ShipRetreating:
      raise(Kind::ShipRetreating, std::format("Ship retreating: {}", where));
      break;
    case EventKind::PiratesCleared:
      raise(Kind::PiratesCleared, std::format("Pirates cleared: {}", where));
      break;
    case EventKind::OrderFired:
      raise(Kind::OrderFired, DescribeFiredOrder(event, where));
      break;
    // What the player built and lost, and the sectors it gained and lost, go to its report of a time away (design §11),
    // not to an alert; and a world's lost and restarted empire is the banner's to tell (Phase 5 design §8).
    case EventKind::ShipBuilt:
    case EventKind::StructureBuilt:
    case EventKind::ShipLost:
    case EventKind::SectorGained:
    case EventKind::SectorLost:
    case EventKind::EmpireLost:
    case EventKind::EmpireRestarted:
      break;
    }
  }
}

std::vector<Outpost::Alerts::Alert> Outpost::Alerts::Shown(std::uint64_t _tick, std::uint32_t _ticksPerSecond) const
{
  const std::uint64_t shownTicks = std::uint64_t{SHOWN_SECONDS} * _ticksPerSecond;
  std::vector<Alert> shown;
  for (auto alert = m_alerts.rbegin(); alert != m_alerts.rend() && shown.size() < SHOWN_ALERTS; ++alert)
  {
    if (_tick < alert->tick + shownTicks)
      shown.push_back(*alert);
  }
  return shown;
}

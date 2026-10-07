#include "pch.h"
#include "Alerts.h"

#include <algorithm>

namespace
{
// How many alerts are kept, for the list and the key: more than are ever shown.
constexpr std::size_t KEPT_ALERTS = 16;
} // namespace

void Outpost::Alerts::Reset() noexcept
{
  m_alerts.clear();
  m_suppressed.clear();
  m_entered.clear();
}

void Outpost::Alerts::Raise(Kind _kind, std::string _text, PlanePosition _position, std::int32_t _sector, std::uint64_t _tick,
                            std::uint32_t _ticksPerSecond)
{
  const std::uint64_t repeatTicks = std::uint64_t{REPEAT_SECONDS} * _ticksPerSecond;
  const bool repeated = std::ranges::any_of(
    m_alerts, [&](const Alert& _alert) { return _alert.kind == _kind && _alert.sector == _sector && _tick < _alert.tick + repeatTicks; });
  if (repeated)
    return;
  m_alerts.push_back({.kind = _kind, .text = std::move(_text), .position = _position, .sector = _sector, .tick = _tick});
  if (m_alerts.size() > KEPT_ALERTS)
    m_alerts.erase(m_alerts.begin());
}

void Outpost::Alerts::Observe(const Snapshot& _snapshot, std::uint32_t _ticksPerSecond)
{
  const PlayerId player = _snapshot.player;
  const auto sectorOf = [&_snapshot](PlanePosition _position) { return FindSector(_snapshot.sectors, _position); };
  const auto sectorName = [](const SectorView* _sector) { return _sector != nullptr ? _sector->nameUtf8 : std::string("the field"); };
  const auto sectorId = [](const SectorView* _sector) { return _sector != nullptr ? _sector->id : 0; };

  // A Relay of the player's newly suppressed.
  std::vector<std::int32_t> suppressed;
  for (const SectorView& sector : _snapshot.sectors)
  {
    if (sector.holder != player || !sector.suppressed)
      continue;
    suppressed.push_back(sector.id);
    if (std::ranges::find(m_suppressed, sector.id) == m_suppressed.end())
      Raise(Kind::RelaySuppressed, std::format("Relay suppressed: {}", sector.nameUtf8), sector.node, sector.id, _snapshot.tick,
            _ticksPerSecond);
  }
  m_suppressed = std::move(suppressed);

  // A Relay of the player's hit by an enemy's shot.
  for (const ShotView& shot : _snapshot.shots)
  {
    const auto target = std::ranges::find(_snapshot.entities, shot.target, &EntityView::id);
    if (target == _snapshot.entities.end() || target->owner != player || target->kind != EntityKind::Structure ||
        target->structure != StructureKind::Relay)
      continue;
    const SectorView* sector = sectorOf(target->position);
    Raise(Kind::RelayAttacked, std::format("Relay under attack: {}", sectorName(sector)), target->position, sectorId(sector),
          _snapshot.tick, _ticksPerSecond);
  }

  // A Mining Rig of the player's destroyed.
  for (const DestroyedView& destroyed : _snapshot.destroyed)
  {
    if (destroyed.owner != player || destroyed.kind != EntityKind::Structure || destroyed.structure != StructureKind::MiningRig)
      continue;
    const SectorView* sector = sectorOf(destroyed.position);
    Raise(Kind::RigLost, std::format("Mining Rig lost: {}", sectorName(sector)), destroyed.position, sectorId(sector), _snapshot.tick,
          _ticksPerSecond);
  }

  // Enemy or pirate warships in a sector the player holds that had none it saw the snapshot before.
  std::vector<std::int32_t> entered;
  for (const EntityView& ship : _snapshot.entities)
  {
    if (ship.kind != EntityKind::Ship || ship.role != ShipRole::Warship || !ship.owner.IsValid() || ship.owner == player)
      continue;
    const SectorView* sector = sectorOf(ship.position);
    if (sector == nullptr || sector->holder != player || std::ranges::find(entered, sector->id) != entered.end())
      continue;
    entered.push_back(sector->id);
    if (std::ranges::find(m_entered, sector->id) == m_entered.end())
      Raise(Kind::EnemyEntered, std::format("{} ships in {}", ship.owner == PIRATES ? "Pirate" : "Enemy", sector->nameUtf8), ship.position,
            sector->id, _snapshot.tick, _ticksPerSecond);
  }
  m_entered = std::move(entered);
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

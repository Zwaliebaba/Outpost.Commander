#include "pch.h"
#include "MatchLog.h"

#include <algorithm>

namespace
{
// How often each player's warships are counted (Phase 1 design §10).
constexpr std::uint64_t FLEET_SAMPLE_SECONDS = 30;
} // namespace

Outpost::MatchLog::MatchLog(std::ostream& _out, std::uint64_t _seed, std::uint32_t _ticksPerSecond)
  : m_out(&_out),
    m_sampleTicks(std::max<std::uint64_t>(FLEET_SAMPLE_SECONDS * _ticksPerSecond, 1))
{
  *m_out << std::format("match seed {} ticks_per_second {}\n", _seed, _ticksPerSecond);
}

void Outpost::MatchLog::Finish()
{
  if (!m_ended)
  {
    WritePeaks();
    *m_out << std::format("left {}\n", m_lastTick);
  }
  m_ended = true;
  m_out->flush();
}

void Outpost::MatchLog::WritePeaks()
{
  std::ranges::sort(m_fleets, {}, [](const Fleet& _fleet) { return _fleet.player.value; });
  for (const Fleet& fleet : m_fleets)
    *m_out << std::format("peak {} player {} warships {}\n", fleet.peakTick, fleet.player.value, fleet.peakWarships);
}

void Outpost::MatchLog::Record(const Snapshot& _snapshot)
{
  m_lastTick = std::max(m_lastTick, _snapshot.tick);

  for (const ResearchTopicView& topic : _snapshot.research)
  {
    const std::pair<PlayerId, ResearchTopicId> key{_snapshot.player, topic.id};
    if (!topic.researched || std::ranges::find(m_researched, key) != m_researched.end())
      continue;
    m_researched.push_back(key);
    *m_out << std::format("research {} player {} topic {} {}\n", _snapshot.tick, _snapshot.player.value, topic.id.value, topic.nameUtf8);
    if (topic.gateway)
      *m_out << std::format("tier {} player {} tier {}\n", _snapshot.tick, _snapshot.player.value, topic.tier);
  }

  // A player sees the components of every ship it sees (design §10, ADR-024), and every snapshot lists every component by
  // name, unlocked or not; one it does not list is written as its identifier.
  const auto nameOf = [](const auto& _views, auto _id)
  {
    const auto found = std::ranges::find_if(_views, [_id](const auto& _view) { return _view.id == _id; });
    return found != _views.end() ? found->nameUtf8 : std::to_string(_id.value);
  };
  size_t warships = 0;
  for (const EntityView& entity : _snapshot.entities)
  {
    if (entity.kind == EntityKind::Asteroid && entity.oreReserveHundredths == 0 && m_dry.insert(entity.id).second)
      *m_out << std::format("dry {} asteroid {}\n", _snapshot.tick, entity.id.value);
    if (entity.kind != EntityKind::Ship || entity.role != ShipRole::Warship)
      continue;
    if (entity.owner == _snapshot.player)
      ++warships;
    if (!m_built.insert(entity.id).second)
      continue;
    *m_out << std::format("built {} player {} hull {} drive {} weapon {} {}+{}+{}\n", _snapshot.tick, entity.owner.value, entity.hull.value,
                          entity.drive.value, entity.weapon.value, nameOf(_snapshot.hulls, entity.hull),
                          nameOf(_snapshot.drives, entity.drive), nameOf(_snapshot.weapons, entity.weapon));
  }

  auto fleet = std::ranges::find(m_fleets, _snapshot.player, &Fleet::player);
  if (fleet == m_fleets.end())
    fleet = m_fleets.insert(m_fleets.end(), Fleet{.player = _snapshot.player, .peakTick = _snapshot.tick});
  if (_snapshot.tick >= fleet->nextSampleTick)
  {
    *m_out << std::format("fleet {} player {} warships {}\n", _snapshot.tick, _snapshot.player.value, warships);
    fleet->nextSampleTick = ((_snapshot.tick / m_sampleTicks) + 1) * m_sampleTicks;
  }
  if (warships > fleet->peakWarships)
  {
    fleet->peakWarships = warships;
    fleet->peakTick = _snapshot.tick;
  }

  if (_snapshot.matchOver && !m_ended)
  {
    m_ended = true;
    WritePeaks();
    *m_out << std::format("end {} winner {}\n", _snapshot.matchEndedTick, _snapshot.winner.value);
    m_out->flush();
  }
}

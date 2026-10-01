#include "pch.h"
#include "MatchLog.h"

#include <algorithm>

Outpost::MatchLog::MatchLog(std::ostream& _out, std::uint64_t _seed, std::uint32_t _ticksPerSecond)
  : m_out(&_out)
{
  *m_out << std::format("match seed {} ticks_per_second {}\n", _seed, _ticksPerSecond);
}

void Outpost::MatchLog::Finish()
{
  if (!m_ended)
    *m_out << std::format("left {}\n", m_lastTick);
  m_ended = true;
  m_out->flush();
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
  }

  // Every player sees every ship's components (design §10), and every snapshot lists every component by name, unlocked
  // or not; one it does not list is written as its identifier.
  const auto nameOf = [](const auto& _views, auto _id)
  {
    const auto found = std::ranges::find_if(_views, [_id](const auto& _view) { return _view.id == _id; });
    return found != _views.end() ? found->nameUtf8 : std::to_string(_id.value);
  };
  for (const EntityView& ship : _snapshot.entities)
  {
    if (ship.kind != EntityKind::Ship || ship.role != ShipRole::Warship || !m_built.insert(ship.id).second)
      continue;
    *m_out << std::format("built {} player {} hull {} drive {} weapon {} {}+{}+{}\n", _snapshot.tick, ship.owner.value, ship.hull.value,
                          ship.drive.value, ship.weapon.value, nameOf(_snapshot.hulls, ship.hull), nameOf(_snapshot.drives, ship.drive),
                          nameOf(_snapshot.weapons, ship.weapon));
  }

  if (_snapshot.matchOver && !m_ended)
  {
    m_ended = true;
    *m_out << std::format("end {} winner {}\n", _snapshot.matchEndedTick, _snapshot.winner.value);
    m_out->flush();
  }
}

#include "pch.h"
#include "MatchLog.h"

#include <algorithm>

namespace
{
// How often each player's warships are counted (Phase 1 design §10).
constexpr std::uint64_t FLEET_SAMPLE_SECONDS = 30;
// An engagement is both sides firing in one sector within this of each other, and ends once neither has fired there for
// the gap (Phase 2 design §2, S2).
constexpr std::uint64_t ENGAGEMENT_WINDOW_SECONDS = 10;
constexpr std::uint64_t ENGAGEMENT_GAP_SECONDS = 30;
} // namespace

Outpost::MatchLog::MatchLog(std::ostream& _out, std::uint64_t _seed, std::uint32_t _ticksPerSecond)
  : m_out(&_out),
    m_sampleTicks(std::max<std::uint64_t>(FLEET_SAMPLE_SECONDS * _ticksPerSecond, 1)),
    m_windowTicks(ENGAGEMENT_WINDOW_SECONDS * _ticksPerSecond),
    m_gapTicks(ENGAGEMENT_GAP_SECONDS * _ticksPerSecond)
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

// A shot is shown to each player who sees its shooter or its target (ADR-024), so a tick's shots arrive in both players'
// snapshots and are counted once. The side that fired is the shooter's owner, or, where the snapshot does not show the
// shooter, the side its target is not on: a match has two players.
void Outpost::MatchLog::RecordShots(const Snapshot& _snapshot)
{
  if (_snapshot.tick != m_shotTick)
  {
    m_shotTick = _snapshot.tick;
    m_shotsCounted.clear();
  }
  const auto ownerOf = [&_snapshot](EntityId _id) -> PlayerId
  {
    if (const auto entity = std::ranges::find(_snapshot.entities, _id, &EntityView::id); entity != _snapshot.entities.end())
      return entity->owner;
    const auto destroyed = std::ranges::find(_snapshot.destroyed, _id, &DestroyedView::id);
    return destroyed != _snapshot.destroyed.end() ? destroyed->owner : PlayerId{};
  };
  for (const ShotView& shot : _snapshot.shots)
  {
    const std::pair<EntityId, EntityId> key{shot.shooter, shot.target};
    if (std::ranges::find(m_shotsCounted, key) != m_shotsCounted.end())
      continue;
    m_shotsCounted.push_back(key);
    PlayerId side = ownerOf(shot.shooter);
    if (!side.IsValid())
    {
      const PlayerId target = ownerOf(shot.target);
      if (target.value == 1 || target.value == 2)
        side = PlayerId{3 - target.value};
    }
    const SectorView* sector = FindSector(_snapshot.sectors, shot.from);
    const std::int32_t sectorId = sector != nullptr ? sector->id : 0;
    if (!m_contact)
    {
      m_contact = true;
      *m_out << std::format("contact {} sector {}\n", _snapshot.tick, sectorId);
    }
    if (!side.IsValid())
      continue;
    auto fire = std::ranges::find(m_fire, sectorId, &SectorFire::sector);
    if (fire == m_fire.end())
      fire = m_fire.insert(m_fire.end(), SectorFire{.sector = sectorId});
    // An engagement ends once the sector has been quiet for the gap.
    std::uint64_t lastAny = 0;
    for (const auto& [player, tick] : fire->lastShot)
      lastAny = std::max(lastAny, tick);
    if (fire->engaged && _snapshot.tick > lastAny + m_gapTicks)
      fire->engaged = false;
    if (const auto last = std::ranges::find(fire->lastShot, side, &std::pair<PlayerId, std::uint64_t>::first); last != fire->lastShot.end())
      last->second = _snapshot.tick;
    else
      fire->lastShot.emplace_back(side, _snapshot.tick);
    const bool bothSides = fire->lastShot.size() >= 2 && std::ranges::all_of(fire->lastShot, [&](const auto& _last)
                                                                             { return _snapshot.tick <= _last.second + m_windowTicks; });
    if (bothSides && !fire->engaged)
    {
      fire->engaged = true;
      *m_out << std::format("engagement {} sector {}\n", _snapshot.tick, sectorId);
    }
  }
}

void Outpost::MatchLog::Record(const Snapshot& _snapshot)
{
  m_lastTick = std::max(m_lastTick, _snapshot.tick);
  RecordShots(_snapshot);

  // Who holds each sector, which both players see alike (ADR-056).
  for (const SectorView& sector : _snapshot.sectors)
  {
    auto known = std::ranges::find(m_holders, sector.id, &std::pair<std::int32_t, PlayerId>::first);
    if (known == m_holders.end())
      known = m_holders.insert(m_holders.end(), {sector.id, PlayerId{}});
    if (known->second == sector.holder)
      continue;
    known->second = sector.holder;
    *m_out << std::format("sector {} sector {} holder {}\n", _snapshot.tick, sector.id, sector.holder.value);
  }

  for (const ResearchTopicView& topic : _snapshot.research)
  {
    const std::pair<PlayerId, ResearchTopicId> key{_snapshot.player, topic.id};
    if (!topic.researched || std::ranges::find(m_researched, key) != m_researched.end())
      continue;
    m_researched.push_back(key);
    *m_out << std::format("research {} player {} topic {} {}\n", _snapshot.tick, _snapshot.player.value, topic.id.value, topic.nameUtf8);
  }
  // Each tier the player's Research Lab opens, once, the first time it opens it (Phase 3 design §6).
  auto tier = std::ranges::find(m_tiers, _snapshot.player, &std::pair<PlayerId, std::int32_t>::first);
  if (tier == m_tiers.end())
    tier = m_tiers.insert(m_tiers.end(), {_snapshot.player, 1});
  for (; tier->second < _snapshot.researchTier; ++tier->second)
    *m_out << std::format("tier {} player {} tier {}\n", _snapshot.tick, _snapshot.player.value, tier->second + 1);

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
    // A module joins the name when the ship has one (Phase 2 design §10), so a scout reads apart from its plain design.
    const std::string module = entity.module.IsValid() ? std::format("+{}", nameOf(_snapshot.modules, entity.module)) : std::string();
    *m_out << std::format("built {} player {} hull {} drive {} weapon {} {}+{}+{}{}\n", _snapshot.tick, entity.owner.value,
                          entity.hull.value, entity.drive.value, entity.weapon.value, nameOf(_snapshot.hulls, entity.hull),
                          nameOf(_snapshot.drives, entity.drive), nameOf(_snapshot.weapons, entity.weapon), module);
  }

  auto fleet = std::ranges::find(m_fleets, _snapshot.player, &Fleet::player);
  if (fleet == m_fleets.end())
    fleet = m_fleets.insert(m_fleets.end(), Fleet{.player = _snapshot.player, .peakTick = _snapshot.tick});
  if (_snapshot.tick >= fleet->nextSampleTick)
  {
    *m_out << std::format("fleet {} player {} warships {}\n", _snapshot.tick, _snapshot.player.value, warships);
    if (const auto tickets = std::ranges::find(_snapshot.tickets, _snapshot.player, &TicketsView::player);
        tickets != _snapshot.tickets.end())
      *m_out << std::format("tickets {} player {} tickets {}\n", _snapshot.tick, _snapshot.player.value, tickets->tickets);
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
    *m_out << std::format("ending {} {}\n", _snapshot.matchEndedTick,
                          _snapshot.ending == MatchEnding::Domination ? "domination" : "production");
    *m_out << std::format("end {} winner {}\n", _snapshot.matchEndedTick, _snapshot.winner.value);
    m_out->flush();
  }
}

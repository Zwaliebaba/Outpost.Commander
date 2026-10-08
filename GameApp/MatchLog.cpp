#include "pch.h"
#include "MatchLog.h"

#include <algorithm>
#include <cmath>

namespace
{
// How often each player's warships are counted (Phase 1 design §10).
constexpr std::uint64_t FLEET_SAMPLE_SECONDS = 30;
// An engagement is both sides firing in one sector within this of each other, and ends once neither has fired there for
// the gap (Phase 2 design §2, S2).
constexpr std::uint64_t ENGAGEMENT_WINDOW_SECONDS = 10;
constexpr std::uint64_t ENGAGEMENT_GAP_SECONDS = 30;
// A Constructor this far beyond its footprint and a derelict's is at the derelict: the server's reach for work, 20 m, and a
// margin, since the log reads positions a snapshot old (ADR-074).
constexpr float SALVAGE_CREW_METERS = 30.0f;

float Distance(Outpost::PlanePosition _a, Outpost::PlanePosition _b) noexcept
{
  return std::hypot(_a.xMeters - _b.xMeters, _a.zMeters - _b.zMeters);
}

// A structure kind as the log writes it, one word.
std::string_view KindWord(Outpost::StructureKind _kind) noexcept
{
  switch (_kind)
  {
  case Outpost::StructureKind::CommandStation:
    return "station";
  case Outpost::StructureKind::Shipyard:
    return "shipyard";
  case Outpost::StructureKind::ResearchLab:
    return "lab";
  case Outpost::StructureKind::MiningRig:
    return "rig";
  case Outpost::StructureKind::DefensePlatform:
    return "platform";
  case Outpost::StructureKind::Relay:
    return "relay";
  case Outpost::StructureKind::RepairBay:
    return "bay";
  }
  return "structure";
}
// How a match ended, as its ending line says it.
std::string_view EndingWord(Outpost::MatchEnding _ending) noexcept
{
  switch (_ending)
  {
  case Outpost::MatchEnding::Domination:
    return "domination";
  case Outpost::MatchEnding::FleetDestroyed:
    return "fleet";
  case Outpost::MatchEnding::TimeLimit:
    return "time";
  case Outpost::MatchEnding::LostProduction:
    break;
  }
  return "production";
}
} // namespace

Outpost::MatchLog::MatchLog(std::ostream& _out, std::uint64_t _seed, std::uint32_t _ticksPerSecond)
  : m_out(&_out),
    m_sampleTicks(std::max<std::uint64_t>(FLEET_SAMPLE_SECONDS * _ticksPerSecond, 1)),
    m_windowTicks(ENGAGEMENT_WINDOW_SECONDS * _ticksPerSecond),
    m_gapTicks(ENGAGEMENT_GAP_SECONDS * _ticksPerSecond)
{
  *m_out << std::format("match seed {} ticks_per_second {}\n", _seed, _ticksPerSecond);
}

void Outpost::MatchLog::Matchup(std::uint32_t _number)
{
  *m_out << std::format("matchup {}\n", _number);
}

void Outpost::MatchLog::Finish()
{
  if (!m_ended)
  {
    WriteTotals();
    *m_out << std::format("left {}\n", m_lastTick);
  }
  m_ended = true;
  m_out->flush();
}

void Outpost::MatchLog::WriteTotals()
{
  std::ranges::sort(m_fleets, {}, [](const Fleet& _fleet) { return _fleet.player.value; });
  for (const Fleet& fleet : m_fleets)
    *m_out << std::format("peak {} player {} warships {}\n", fleet.peakTick, fleet.player.value, fleet.peakWarships);
  if (m_territory)
    *m_out << std::format("stall {} ticks {}\n", m_lastTick, m_stallTicks);
}

// From the player's own snapshots, which show its structures' levels and the work under way (ADR-064).
void Outpost::MatchLog::RecordUpgrades(const Snapshot& _snapshot)
{
  const auto write = [&](const Growth& _growth, std::int32_t _level, std::string_view _what)
  {
    *m_out << std::format("upgrade {} player {} structure {} {} level {} {}\n", _snapshot.tick, _snapshot.player.value, _growth.id.value,
                          KindWord(_growth.kind), _level, _what);
  };
  for (const EntityView& entity : _snapshot.entities)
  {
    if (entity.kind != EntityKind::Structure || entity.owner != _snapshot.player || entity.remembered)
      continue;
    auto growth = std::ranges::find(m_growth, entity.id, &Growth::id);
    if (growth == m_growth.end())
      growth = m_growth.insert(m_growth.end(), Growth{.id = entity.id, .kind = entity.structure, .level = entity.level});
    if (entity.level > growth->level)
    {
      growth->level = entity.level;
      growth->upgrading = false;
      write(*growth, growth->level, "finished");
    }
    if (entity.upgradePermille.has_value() && !growth->upgrading)
    {
      growth->upgrading = true;
      write(*growth, growth->level + 1, "started");
    }
  }
  for (const DestroyedView& destroyed : _snapshot.destroyed)
  {
    const auto growth = std::ranges::find(m_growth, destroyed.id, &Growth::id);
    if (destroyed.owner != _snapshot.player || growth == m_growth.end())
      continue;
    if (growth->upgrading)
      write(*growth, growth->level + 1, "lost");
    m_growth.erase(growth);
  }
}

// Both players' snapshots of a tick carry their own caps, and the holders both see (ADR-056); the tick counts once both
// are in.
void Outpost::MatchLog::RecordStall(const Snapshot& _snapshot)
{
  if (_snapshot.nodeCap <= 0)
    return;
  m_territory = true;
  auto state = std::ranges::find(m_caps, _snapshot.player, &CapState::player);
  if (state == m_caps.end())
    state = m_caps.insert(m_caps.end(), CapState{.player = _snapshot.player});
  state->tick = _snapshot.tick;
  state->atCap = AtNodeCap(_snapshot.sectors, _snapshot.entities, _snapshot.player, _snapshot.nodeCap);
  state->held = std::ranges::count(_snapshot.sectors, _snapshot.player, &SectorView::holder);
  if (m_caps.size() < 2 || m_stallCountedTick == _snapshot.tick ||
      !std::ranges::all_of(m_caps, [&](const CapState& _cap) { return _cap.tick == _snapshot.tick; }))
    return;
  m_stallCountedTick = _snapshot.tick;
  const bool stalled = std::ranges::all_of(m_caps, [](const CapState& _cap) { return _cap.atCap; }) &&
                       std::ranges::all_of(m_caps, [&](const CapState& _cap) { return _cap.held == m_caps.front().held; });
  if (stalled)
    ++m_stallTicks;
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
    // A repaired warship fights again once it fires, at a player or the pirates (Phase 4 U3).
    if (const auto repaired = m_repaired.find(shot.shooter); repaired != m_repaired.end())
    {
      *m_out << std::format("again {} player {} ship {}\n", _snapshot.tick, repaired->second.value, shot.shooter.value);
      m_repaired.erase(repaired);
    }
    // A fight with the pirates is recorded once a player and sector, in the sector of the pirates' end of the shot (U1). The
    // contact and the engagements are of the players' fights, so it counts toward neither.
    const PlayerId shooterOwner = ownerOf(shot.shooter);
    const PlayerId targetOwner = ownerOf(shot.target);
    if (shooterOwner == PIRATES || targetOwner == PIRATES)
    {
      const PlayerId player = shooterOwner == PIRATES ? targetOwner : shooterOwner;
      const SectorView* sector = FindSector(_snapshot.sectors, shooterOwner == PIRATES ? shot.from : shot.to);
      if ((player.value == 1 || player.value == 2) && sector != nullptr && m_piratesFought.insert({player, sector->id}).second)
        *m_out << std::format("pirates {} player {} sector {}\n", _snapshot.tick, player.value, sector->id);
      continue;
    }
    PlayerId side = shooterOwner;
    if (!side.IsValid() && (targetOwner.value == 1 || targetOwner.value == 2))
      side = PlayerId{3 - targetOwner.value};
    // A structure above level 1 under fire, once a structure and level (Phase 3 T4).
    if (const auto target = std::ranges::find(_snapshot.entities, shot.target, &EntityView::id);
        target != _snapshot.entities.end() && target->kind == EntityKind::Structure && target->level > 1 &&
        m_attacked.insert({target->id, target->level}).second)
    {
      *m_out << std::format("attacked {} player {} structure {} {} level {}\n", _snapshot.tick, target->owner.value, target->id.value,
                            KindWord(target->structure), target->level);
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

// A warship's retreat and repair are its owner's to see (ADR-075). A retreat that ends with the ship whole is a repair; one
// that ends by an order is neither.
void Outpost::MatchLog::RecordRetreats(const Snapshot& _snapshot)
{
  for (const EntityView& ship : _snapshot.entities)
  {
    if (ship.kind != EntityKind::Ship || ship.role != ShipRole::Warship || ship.owner != _snapshot.player || ship.remembered)
      continue;
    bool& retreating = m_retreating[ship.id];
    if (ship.retreating && !retreating)
      *m_out << std::format("retreat {} player {} ship {}\n", _snapshot.tick, ship.owner.value, ship.id.value);
    if (!ship.retreating && retreating && ship.hitPointsHundredths == ship.maxHitPointsHundredths)
    {
      *m_out << std::format("repaired {} player {} ship {}\n", _snapshot.tick, ship.owner.value, ship.id.value);
      m_repaired[ship.id] = ship.owner;
    }
    retreating = ship.retreating;
  }
  for (const DestroyedView& destroyed : _snapshot.destroyed)
  {
    m_retreating.erase(destroyed.id);
    m_repaired.erase(destroyed.id);
  }
}

// A derelict leaves only by its salvage (ADR-074), so one the player's own Constructors were at that is gone from the
// player's next snapshot was salvaged by them: while they are there, the player sees it.
void Outpost::MatchLog::RecordSalvage(const Snapshot& _snapshot)
{
  const auto present = [&_snapshot](EntityId _id)
  { return std::ranges::any_of(_snapshot.entities, [_id](const EntityView& _entity) { return _entity.id == _id; }); };
  for (auto salvage = m_salvage.begin(); salvage != m_salvage.end();)
  {
    if (salvage->player != _snapshot.player)
    {
      ++salvage;
      continue;
    }
    if (!present(salvage->derelict))
      *m_out << std::format("salvaged {} player {} derelict {} ore {}\n", _snapshot.tick, salvage->player.value, salvage->derelict.value,
                            salvage->ore);
    salvage = m_salvage.erase(salvage);
  }
  for (const EntityView& derelict : _snapshot.entities)
  {
    if (derelict.kind != EntityKind::Derelict)
      continue;
    const bool crewed = std::ranges::any_of(_snapshot.entities,
                                            [&](const EntityView& _ship)
                                            {
                                              return _ship.kind == EntityKind::Ship && _ship.role == ShipRole::Constructor &&
                                                     _ship.owner == _snapshot.player && !_ship.remembered &&
                                                     Distance(_ship.position, derelict.position) <=
                                                       _ship.radiusMeters + derelict.radiusMeters + SALVAGE_CREW_METERS;
                                            });
    if (crewed)
      m_salvage.push_back({.player = _snapshot.player, .derelict = derelict.id, .ore = derelict.salvageOre});
  }
}

void Outpost::MatchLog::Record(const Snapshot& _snapshot)
{
  m_lastTick = std::max(m_lastTick, _snapshot.tick);
  RecordShots(_snapshot);
  RecordUpgrades(_snapshot);
  RecordStall(_snapshot);
  RecordRetreats(_snapshot);
  RecordSalvage(_snapshot);

  // Who holds each sector, which both players see alike (ADR-056), and which the pirates guard (ADR-073).
  for (const SectorView& sector : _snapshot.sectors)
  {
    auto guarded = std::ranges::find(m_guarded, sector.id, &std::pair<std::int32_t, bool>::first);
    if (guarded == m_guarded.end())
      guarded = m_guarded.insert(m_guarded.end(), {sector.id, sector.guarded});
    if (guarded->second && !sector.guarded)
      *m_out << std::format("cleared {} sector {}\n", _snapshot.tick, sector.id);
    guarded->second = sector.guarded;
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
    WriteTotals();
    *m_out << std::format("ending {} {}\n", _snapshot.matchEndedTick, EndingWord(_snapshot.ending));
    *m_out << std::format("end {} winner {}\n", _snapshot.matchEndedTick, _snapshot.winner.value);
    m_out->flush();
  }
}

#include "pch.h"
#include "WorldLog.h"

namespace
{
std::string_view PlayWord(Outpost::SeatPlay _play) noexcept
{
  switch (_play)
  {
  case Outpost::SeatPlay::Player:
    return "player";
  case Outpost::SeatPlay::Deputy:
    return "deputy";
  case Outpost::SeatPlay::Ai:
    return "ai";
  }
  return "player";
}

// A battle side's play: present once its player played it in any tick, and otherwise whoever last fought it.
Outpost::SeatPlay Fought(Outpost::SeatPlay _before, Outpost::SeatPlay _now) noexcept
{
  return _before == Outpost::SeatPlay::Player ? _before : _now;
}

std::string_view ActionWord(Outpost::ScheduledActionKind _action) noexcept
{
  switch (_action)
  {
  case Outpost::ScheduledActionKind::Move:
    return "move";
  case Outpost::ScheduledActionKind::AttackMove:
    return "attack-move";
  case Outpost::ScheduledActionKind::Attack:
    return "attack";
  case Outpost::ScheduledActionKind::HoldSector:
    return "hold";
  case Outpost::ScheduledActionKind::Patrol:
    return "patrol";
  case Outpost::ScheduledActionKind::BuildRig:
    return "rig";
  }
  return "move";
}

std::string_view OutcomeWord(Outpost::OrderOutcome _outcome) noexcept
{
  switch (_outcome)
  {
  case Outpost::OrderOutcome::AsGiven:
    return "given";
  case Outpost::OrderOutcome::HeldInstead:
    return "held";
  case Outpost::OrderOutcome::Refused:
    return "refused";
  }
  return "given";
}

// Whose an entity is, as a snapshot shows it or the destruction it shows; nobody's when it shows neither.
Outpost::PlayerId OwnerIn(const Outpost::Snapshot& _snapshot, Outpost::EntityId _id)
{
  if (const auto entity = std::ranges::find(_snapshot.entities, _id, &Outpost::EntityView::id); entity != _snapshot.entities.end())
    return entity->owner;
  const auto destroyed = std::ranges::find(_snapshot.destroyed, _id, &Outpost::DestroyedView::id);
  return destroyed != _snapshot.destroyed.end() ? destroyed->owner : Outpost::PlayerId{};
}
} // namespace

Outpost::WorldLog::WorldLog(std::ostream& _out, std::uint32_t _ticksPerSecond)
  : m_out(&_out),
    m_ticksPerSecond(_ticksPerSecond)
{
}

void Outpost::WorldLog::Write(const std::string& _line)
{
  *m_out << _line << '\n';
  m_out->flush();
}

void Outpost::WorldLog::Started(std::uint64_t _tick, std::optional<std::uint64_t> _saveTick, std::size_t _replayed)
{
  // A server made afresh knows nobody's seat, and a battle it did not see the start of is not one it can tell.
  m_seats.clear();
  m_battles.clear();
  Write(_saveTick.has_value() ? std::format("start {} recovered save {} replayed {}", _tick, *_saveTick, _replayed)
                              : std::format("start {} new", _tick));
}

void Outpost::WorldLog::Saved(std::uint64_t _tick, std::size_t _bytes, std::chrono::microseconds _encode)
{
  Write(std::format("save {} bytes {} encode_us {}", _tick, _bytes, _encode.count()));
}

void Outpost::WorldLog::Record(std::uint64_t _tick, std::span<const SeatTick> _seats)
{
  for (const SeatTick& seat : _seats)
    RecordSeat(_tick, seat);
  RecordShots(_tick, _seats);
  EndBattles(_tick);
  const std::uint64_t hour = std::uint64_t{HOUR_SECONDS} * m_ticksPerSecond;
  if (_tick == 0 || hour == 0 || _tick % hour != 0)
    return;
  for (const SeatTick& seat : _seats)
  {
    const Snapshot& snapshot = *seat.snapshot;
    const auto state = std::ranges::find(m_seats, seat.player, &SeatState::player);
    const auto nodes = std::ranges::count(snapshot.sectors, seat.player, &SectorView::holder);
    const auto researched = std::ranges::count_if(snapshot.research, [](const ResearchTopicView& _topic) { return _topic.researched; });
    Write(std::format("hour {} player {} ore {} income {} fleet {} cap {} nodes {} researched {} present {} deputy {}", _tick,
                      seat.player.value, snapshot.ore, snapshot.oreIncomeHundredthsPerSecond, snapshot.commandPoints, snapshot.fleetCap,
                      nodes, researched, state->playerTicks / m_ticksPerSecond, state->deputyTicks / m_ticksPerSecond));
    state->playerTicks = 0;
    state->deputyTicks = 0;
  }
}

void Outpost::WorldLog::RecordSeat(std::uint64_t _tick, const SeatTick& _seat)
{
  auto state = std::ranges::find(m_seats, _seat.player, &SeatState::player);
  if (state == m_seats.end())
  {
    m_seats.push_back({.player = _seat.player});
    state = std::prev(m_seats.end());
  }
  if (state->play != _seat.play)
  {
    state->play = _seat.play;
    Write(std::format("seat {} player {} {}", _tick, _seat.player.value, PlayWord(_seat.play)));
  }
  state->playerTicks += _seat.play == SeatPlay::Player ? 1 : 0;
  state->deputyTicks += _seat.play == SeatPlay::Deputy ? 1 : 0;

  const Snapshot& snapshot = *_seat.snapshot;
  for (const EventView& event : snapshot.events)
  {
    if (event.kind == EventKind::OrderFired)
    {
      Write(std::format("order {} player {} order {} {} {}", _tick, _seat.player.value, event.order, ActionWord(event.action),
                        OutcomeWord(event.outcome)));
    }
    else if (event.kind == EventKind::EmpireLost)
      Write(std::format("lost {} player {}", _tick, _seat.player.value));
    else if (event.kind == EventKind::EmpireRestarted)
      Write(std::format("restart {} player {}", _tick, _seat.player.value));
  }
  // The hour has passed and the seat has not restarted: its start is not free (ADR-084). Written once a wait.
  const bool waiting = snapshot.restartTick.has_value() && _tick > *snapshot.restartTick;
  if (waiting && !state->waiting)
    Write(std::format("waiting {} player {}", _tick, _seat.player.value));
  state->waiting = waiting;
}

void Outpost::WorldLog::RecordShots(std::uint64_t _tick, std::span<const SeatTick> _seats)
{
  const std::uint64_t window = std::uint64_t{BATTLE_WINDOW_SECONDS} * m_ticksPerSecond;
  for (const SeatTick& seat : _seats)
  {
    const Snapshot& snapshot = *seat.snapshot;
    for (const ShotView& shot : snapshot.shots)
    {
      // A seat's own shot at another seat's ship or structure, in the sector it was fired from.
      if (OwnerIn(snapshot, shot.shooter) != seat.player)
        continue;
      const PlayerId target = OwnerIn(snapshot, shot.target);
      if (std::ranges::find(_seats, target, &SeatTick::player) == _seats.end() || target == seat.player)
        continue;
      const SectorView* sector = FindSector(snapshot.sectors, shot.from);
      const std::int32_t sectorId = sector != nullptr ? sector->id : 0;
      auto battle = std::ranges::find(m_battles, sectorId, &Battle::sector);
      if (battle == m_battles.end())
      {
        m_battles.push_back({.sector = sectorId});
        battle = std::prev(m_battles.end());
      }
      if (auto fire = std::ranges::find(battle->lastFire, seat.player, &std::pair<PlayerId, std::uint64_t>::first);
          fire != battle->lastFire.end())
        fire->second = _tick;
      else
        battle->lastFire.emplace_back(seat.player, _tick);
      // Both sides have fired here within the window: a battle, if one is not under way.
      const auto recent = std::ranges::count_if(battle->lastFire, [&](const auto& _fire) { return _fire.second + window >= _tick; });
      if (!battle->start.has_value() && recent >= 2)
      {
        battle->start = _tick;
        // Its sides are every seat that fired there within the window, as each is played now.
        for (const auto& [player, fired] : battle->lastFire)
        {
          const auto side = std::ranges::find(_seats, player, &SeatTick::player);
          if (fired + window >= _tick && side != _seats.end())
            battle->sides.emplace_back(player, side->play);
        }
      }
      if (!battle->start.has_value())
        continue;
      battle->lastShot = _tick;
      if (auto side = std::ranges::find(battle->sides, seat.player, &std::pair<PlayerId, SeatPlay>::first); side != battle->sides.end())
        side->second = Fought(side->second, seat.play);
      else
        battle->sides.emplace_back(seat.player, seat.play);
    }
  }
}

void Outpost::WorldLog::EndBattles(std::uint64_t _tick)
{
  const std::uint64_t gap = std::uint64_t{BATTLE_GAP_SECONDS} * m_ticksPerSecond;
  std::erase_if(m_battles,
                [&](Battle& _battle)
                {
                  const std::uint64_t last = std::ranges::max(_battle.lastFire, {}, &std::pair<PlayerId, std::uint64_t>::second).second;
                  if (last + gap > _tick)
                    return false;
                  if (_battle.start.has_value())
                  {
                    std::string sides;
                    for (const auto& [player, play] : _battle.sides)
                      sides += std::format(" player {} {}", player.value, play == SeatPlay::Player ? "present" : PlayWord(play));
                    Write(std::format("battle {} {} sector {}{}", *_battle.start, _battle.lastShot, _battle.sector, sides));
                  }
                  return true;
                });
}

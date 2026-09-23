#include "pch.h"

#include "StressReport.h"

#include <algorithm>
#include <string>

namespace Outpost
{

StressReport::StressReport(const std::vector<BotRole>& _roles)
{
  m_bots.reserve(_roles.size());
  for (const BotRole role : _roles)
  {
    m_bots.push_back(BotMemory{.role = role});
    ++m_counters[static_cast<std::size_t>(role)].bots;
  }
}

RoleCounters& StressReport::CountersOf(std::uint32_t _bot) noexcept
{
  return m_counters[static_cast<std::size_t>(m_bots[_bot].role)];
}

void StressReport::NoteJoin(std::uint32_t _bot, const JoinState& _join) noexcept
{
  if (_bot >= m_bots.size())
  {
    return;
  }

  BotMemory& memory = m_bots[_bot];
  RoleCounters& counters = CountersOf(_bot);
  const JoinPhase phase = _join.Phase();

  // **ON THE CHANGE, NOT ON THE REPLY.** A client that asked three times hears three answers, and a seat
  // counted three times would report a host seating more players than it has slots.
  if ((phase == JoinPhase::Joined) && (memory.phase != JoinPhase::Joined))
  {
    if (_join.Resumed())
    {
      ++counters.resumed;
    }
    else
    {
      ++counters.seated;
    }
  }
  else if ((phase == JoinPhase::Refused) && (memory.phase != JoinPhase::Refused))
  {
    ++counters.refused;
  }

  memory.phase = phase;
}

void StressReport::NoteDrain(std::uint32_t _bot, const ClientFrame::DrainResult& _drained, const ReplicaStore& _replicas) noexcept
{
  if (_bot >= m_bots.size())
  {
    return;
  }

  BotMemory& memory = m_bots[_bot];
  RoleCounters& counters = CountersOf(_bot);

  counters.updates += _drained.accepted;
  counters.recordsRefused += _drained.refused;
  counters.datagramsFaulted += _drained.faulted;
  counters.refreshes += _drained.refreshed;
  counters.refreshTicksTotal += _drained.refreshTicksTotal;
  counters.refreshTicksMax = std::max(counters.refreshTicksMax, _drained.refreshTicksMax);
  if (_drained.linkLost)
  {
    ++counters.linkLosses;
  }

  // THE STORE'S LOSS COUNT IS CUMULATIVE AND SURVIVES A REJOIN, so the report takes the difference.
  const std::uint64_t lost = _replicas.LostCount();
  if (lost > memory.lostCount)
  {
    counters.updatesLost += lost - memory.lostCount;
  }
  memory.lostCount = lost;

  // **A GAP IS MEASURED ONLY FORWARD, AND NOT ACROSS A CLEARED STORE.** The first tick after a rejoin has no
  // predecessor in this store, and a gap from the tick before the drop would report the churner's own absence
  // as the host falling behind.
  const std::uint32_t newest = _replicas.NewestTick();
  if ((memory.newestTick != 0) && (newest > memory.newestTick))
  {
    const std::uint32_t gap = newest - memory.newestTick;
    ++counters.tickGaps;
    counters.tickGapTotal += gap;
    counters.tickGapMax = std::max(counters.tickGapMax, gap);
  }
  if ((newest == 0) || (newest > memory.newestTick))
  {
    memory.newestTick = newest;
  }
}

void StressReport::NoteSent(std::uint32_t _bot, std::uint32_t _datagrams, std::uint32_t _commands, std::uint32_t _skipped) noexcept
{
  if (_bot >= m_bots.size())
  {
    return;
  }

  RoleCounters& counters = CountersOf(_bot);
  counters.datagramsSent += _datagrams;
  counters.commandsSent += _commands;
  counters.sendsSkipped += _skipped;
}

void StressReport::NoteAcknowledged(std::uint32_t _bot, std::uint64_t _milliseconds) noexcept
{
  if (_bot >= m_bots.size())
  {
    return;
  }

  RoleCounters& counters = CountersOf(_bot);
  ++counters.commandsAcknowledged;
  counters.acknowledgeMillisecondsTotal += _milliseconds;
  counters.acknowledgeMillisecondsMax = std::max(counters.acknowledgeMillisecondsMax, _milliseconds);
}

void StressReport::NoteRejoin(std::uint32_t _bot) noexcept
{
  if (_bot >= m_bots.size())
  {
    return;
  }

  ++CountersOf(_bot).rejoins;

  // **THE BOT IS ASKING AGAIN**, so the next seat is a resume to count, and the update after it has no
  // predecessor: the ticks it was away are its own absence and not a gap the host made.
  m_bots[_bot].phase = JoinPhase::Joining;
  m_bots[_bot].newestTick = 0;
}

std::string StressReport::Format(std::uint64_t _harnessTicks) const
{
  std::string text = "stress report: " + std::to_string(m_bots.size()) + " bots, " + std::to_string(_harnessTicks) + " harness ticks\n";

  for (std::size_t role = 0; role < BOT_ROLE_COUNT; ++role)
  {
    const RoleCounters& c = m_counters[role];
    if (c.bots == 0)
    {
      continue;
    }

    text += std::string{RoleName(static_cast<BotRole>(role))} + ": " + std::to_string(c.bots) + " bots\n";
    text += "  seats: " + std::to_string(c.seated) + " taken, " + std::to_string(c.resumed) + " resumed, " + std::to_string(c.refused) +
            " refused; " + std::to_string(c.rejoins) + " rejoins, " + std::to_string(c.linkLosses) + " link losses\n";
    text += "  updates: " + std::to_string(c.updates) + " received, " + std::to_string(c.updatesLost) + " lost, " +
            std::to_string(c.datagramsFaulted) + " faulted; " + std::to_string(c.recordsRefused) + " records refused\n";
    text += "  update tick gap: mean " + FormatHundredths(c.tickGapTotal, c.tickGaps) + ", max " + std::to_string(c.tickGapMax) + " over " +
            std::to_string(c.tickGaps) + "\n";
    text += "  refresh interval per entity: mean " + FormatHundredths(c.refreshTicksTotal, c.refreshes) + " ticks, max " +
            std::to_string(c.refreshTicksMax) + " over " + std::to_string(c.refreshes) + "\n";
    text += "  commands: " + std::to_string(c.commandsSent) + " sent, " + std::to_string(c.commandsAcknowledged) +
            " acknowledged; acknowledge mean " + FormatHundredths(c.acknowledgeMillisecondsTotal, c.commandsAcknowledged) + " ms, max " +
            std::to_string(c.acknowledgeMillisecondsMax) + " ms\n";
    text += "  datagrams: " + std::to_string(c.datagramsSent) + " sent, " + std::to_string(c.sendsSkipped) + " skipped in flight\n";
  }

  return text;
}

std::string FormatHundredths(std::uint64_t _total, std::uint64_t _count)
{
  if (_count == 0)
  {
    return "0.00";
  }

  // Rounded to nearest, in integers, so the text does not depend on a floating-point format.
  const std::uint64_t hundredths = ((_total * 100) + (_count / 2)) / _count;
  const std::uint64_t fraction = hundredths % 100;
  return std::to_string(hundredths / 100) + ((fraction < 10) ? ".0" : ".") + std::to_string(fraction);
}

const char* RoleName(BotRole _role) noexcept
{
  switch (_role)
  {
  case BotRole::Player:
    return "player";
  case BotRole::Churner:
    return "churner";
  case BotRole::Flooder:
    return "flooder";
  }
  return "unknown";
}

} // namespace Outpost

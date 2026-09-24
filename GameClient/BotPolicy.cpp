#include "pch.h"

#include "BotPolicy.h"
#include "Panels.h"
#include "StressReport.h"

#include <cstddef>
#include <utility>

namespace Outpost
{

namespace
{
/// A record's design byte as a design, or false for a byte this build's table does not hold. A record is
/// what the host sent, and a bot believes it no further than a person's client does.
[[nodiscard]] bool KnownDesign(std::uint8_t _designIdentity, DesignId& _outDesign) noexcept
{
  if (_designIdentity >= Designs().size())
  {
    return false;
  }
  _outDesign = static_cast<DesignId>(_designIdentity);
  return true;
}
} // namespace

BotPolicy::BotPolicy(std::uint64_t _runSeed, std::uint32_t _botIndex) noexcept
  : m_random{_runSeed, BotStream(BotRole::Player, _botIndex)}
{
}

std::vector<Command> BotPolicy::Decide(const ReplicaStore& _replicas, PlayerId _player, std::uint64_t _nowMilliseconds)
{
  std::vector<Command> orders;

  const PlayerBlock* own = _replicas.Own();
  if ((own == nullptr) || (_player == NO_PLAYER))
  {
    return orders;
  }

  // **PACED BY THE UPDATE'S TICK**, so a bot fed the same updates decides the same things on the same ticks
  // however fast the harness loop runs.
  const std::uint32_t tick = _replicas.NewestTick();
  if (m_decided && ((tick - m_lastDecisionTick) < DECISION_INTERVAL_TICKS))
  {
    return orders;
  }
  m_decided = true;
  m_lastDecisionTick = tick;

  if (!m_adoptedSequence)
  {
    m_nextSequence = static_cast<std::uint16_t>(own->lastCommandSequenceApplied + 1);
    m_adoptedSequence = true;
  }

  // THE STATION. Zero in the block is nothing building (`GameLogic/BuildSystem.h`), so a bot builds into an
  // empty slot and never replaces an item -- replacing is a refund and a charge, and the cancel below
  // already exercises the refund.
  if (BuildingDesignOf(own->buildingDesign) == 0)
  {
    std::vector<DesignId> affordable;
    for (const DesignEntry& design : Designs())
    {
      if (design.buildable && (Derive(design).cost <= own->credits))
      {
        affordable.push_back(design.id);
      }
    }
    if (!affordable.empty())
    {
      const DesignId chosen = affordable[m_random.NextBelow(static_cast<std::uint32_t>(affordable.size()))];
      orders.push_back(BuildStationCommand(m_nextSequence++, CommandType::Build, chosen));
    }
  }
  else if (m_random.NextBelow(CANCEL_ONE_IN) == 0)
  {
    orders.push_back(BuildStationCommand(m_nextSequence++, CommandType::CancelBuild, DesignId::Miner));
  }

  // THE SHIPS. **Only what this bot's records say it owns, and only what can move**: a design with a drive,
  // which is a derived stat and not a list of ship names (R24). In index order, so the draw that decides each
  // one is the same draw on every run.
  if (m_random.NextBelow(MOVE_ONE_IN) == 0)
  {
    Command moveOrder{.sequence = 0, .type = CommandType::MoveTo, .targetX = 0, .targetY = 0, .selection = {}};
    for (const EntityRecord& record : _replicas.Entities())
    {
      DesignId design{};
      if ((record.owner != _player) || !KnownDesign(record.designIdentity, design) || (Derive(design).speedUnitsPerSecond == 0))
      {
        continue;
      }
      if ((m_random.NextBelow(2) == 0) && (moveOrder.selection.size() < MAX_MOVE_SELECTION))
      {
        moveOrder.selection.push_back(record.identity);
      }
    }

    if (!moveOrder.selection.empty())
    {
      moveOrder.sequence = m_nextSequence++;
      moveOrder.targetX = static_cast<std::int16_t>(m_random.NextInRange(-MOVE_REACH_WIRE_STEPS, MOVE_REACH_WIRE_STEPS));
      moveOrder.targetY = static_cast<std::int16_t>(m_random.NextInRange(-MOVE_REACH_WIRE_STEPS, MOVE_REACH_WIRE_STEPS));
      orders.push_back(std::move(moveOrder));
    }
  }

  for (const Command& order : orders)
  {
    m_outstanding.push_back(order);
    m_sentMilliseconds.push_back(_nowMilliseconds);
  }
  return orders;
}

std::size_t BotPolicy::Acknowledge(std::uint16_t _lastApplied, std::uint64_t _nowMilliseconds,
                                   std::vector<std::uint64_t>& _outWaitMilliseconds)
{
  // Oldest first and in sequence order, so the acknowledged ones are a prefix. The comparison is the wire's
  // serial-number one, because the sequence wraps.
  std::size_t retired = 0;
  while ((retired < m_outstanding.size()) && !SequenceIsNewer(m_outstanding[retired].sequence, _lastApplied))
  {
    const std::uint64_t sent = m_sentMilliseconds[retired];
    _outWaitMilliseconds.push_back((_nowMilliseconds > sent) ? (_nowMilliseconds - sent) : 0);
    ++retired;
  }

  m_outstanding.erase(m_outstanding.begin(), m_outstanding.begin() + static_cast<std::ptrdiff_t>(retired));
  m_sentMilliseconds.erase(m_sentMilliseconds.begin(), m_sentMilliseconds.begin() + static_cast<std::ptrdiff_t>(retired));
  return retired;
}

void BotPolicy::Reset() noexcept
{
  m_outstanding.clear();
  m_sentMilliseconds.clear();
  m_decided = false;
  m_adoptedSequence = false;
}

} // namespace Outpost

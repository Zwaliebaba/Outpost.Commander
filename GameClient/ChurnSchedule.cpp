#include "pch.h"

#include "ChurnSchedule.h"
#include "StressReport.h"

namespace Outpost
{

ChurnSchedule::ChurnSchedule(std::uint64_t _runSeed, std::uint32_t _botIndex) noexcept
  : m_random{_runSeed, BotStream(BotRole::Churner, _botIndex)}
{
}

ChurnAction ChurnSchedule::Advance(std::uint64_t _harnessTick, bool _seated) noexcept
{
  switch (m_phase)
  {
  case Phase::Unseated:
    if (_seated)
    {
      m_phase = Phase::Seated;
      m_nextTick = _harnessTick + static_cast<std::uint64_t>(m_random.NextInRange(static_cast<std::int32_t>(SEATED_TICKS_MINIMUM),
                                                                                  static_cast<std::int32_t>(SEATED_TICKS_MAXIMUM)));
    }
    return ChurnAction::None;

  case Phase::Seated:
    if (_harnessTick >= m_nextTick)
    {
      m_phase = Phase::Away;
      m_nextTick = _harnessTick + static_cast<std::uint64_t>(m_random.NextInRange(static_cast<std::int32_t>(AWAY_TICKS_MINIMUM),
                                                                                  static_cast<std::int32_t>(AWAY_TICKS_MAXIMUM)));
      return ChurnAction::Drop;
    }
    return ChurnAction::None;

  case Phase::Away:
    if (_harnessTick >= m_nextTick)
    {
      // BACK TO WAITING FOR THE SEAT, not straight to seated: the drop clock starts again only when the host
      // has answered the rejoin.
      m_phase = Phase::Unseated;
      ++m_rejoins;
      return ChurnAction::Rejoin;
    }
    return ChurnAction::None;
  }
  return ChurnAction::None;
}

} // namespace Outpost

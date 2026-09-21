#include "pch.h"

#include "TickSchedule.h"

namespace Neuron
{

TickSchedule::TickSchedule(std::chrono::steady_clock::time_point _start, std::int64_t _periodMilliseconds) noexcept
  : m_start{_start},
    m_period{(_periodMilliseconds > 0) ? _periodMilliseconds : 1}
{
}

std::uint32_t TickSchedule::TicksDue(std::chrono::steady_clock::time_point _now) noexcept
{
  if (_now < m_start)
  {
    return 0;
  }

  // FROM THE START, NOT FROM THE LAST TICK. This division is the whole anti-drift argument: a
  // wake-up that was late still lands on the original grid, so the lateness is spent rather than
  // carried into every tick after it.
  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(_now - m_start);
  const std::uint64_t shouldHaveIssued = static_cast<std::uint64_t>(elapsed.count()) / static_cast<std::uint64_t>(m_period.count());
  if (shouldHaveIssued <= m_consumed)
  {
    return 0;
  }

  std::uint64_t owed = shouldHaveIssued - m_consumed;
  if (owed > MAX_CATCH_UP_TICKS)
  {
    m_abandoned += owed - MAX_CATCH_UP_TICKS;
    owed = MAX_CATCH_UP_TICKS;
  }

  m_issued += owed;

  // PAST THE ABANDONED ONES TOO. Advancing only by what was issued leaves the grid position behind
  // the clock, so the next call owes the same backlog again and the host abandons ticks forever
  // instead of catching up once.
  m_consumed = shouldHaveIssued;
  return static_cast<std::uint32_t>(owed);
}

std::chrono::steady_clock::time_point TickSchedule::NextDeadline() const noexcept
{
  return m_start + (m_period * (m_consumed + 1));
}

} // namespace Neuron

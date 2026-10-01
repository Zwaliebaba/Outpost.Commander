#include "pch.h"
#include "TickHost.h"

#include <algorithm>

namespace
{
constexpr std::uint64_t NANOSECONDS_PER_SECOND = 1'000'000'000;
// A longer gap than this adds nothing, since the backlog is capped anyway, and bounding it keeps the product below in range.
constexpr std::int64_t MAX_ELAPSED_NANOSECONDS = 60 * static_cast<std::int64_t>(NANOSECONDS_PER_SECOND);
constexpr std::uint32_t MAX_TICKS_PER_SECOND = 1000;
} // namespace

Neuron::TickHost::TickHost(std::uint32_t _ticksPerSecond, std::uint32_t _maxTicksPerAdvance)
  : m_ticksPerSecond(_ticksPerSecond),
    m_maxTicksPerAdvance(_maxTicksPerAdvance)
{
  // The upper bound keeps MAX_ELAPSED_NANOSECONDS times the rate within 64 bits, with room to spare.
  if (_ticksPerSecond == 0 || _ticksPerSecond > MAX_TICKS_PER_SECOND || _maxTicksPerAdvance == 0)
  {
    throw Exception(
      std::format("TickHost: a rate of {} ticks per second, up to {} at once, is out of range", _ticksPerSecond, _maxTicksPerAdvance));
  }
}

std::uint32_t Neuron::TickHost::Advance(std::chrono::nanoseconds _elapsed) noexcept
{
  const std::int64_t elapsed = std::clamp<std::int64_t>(_elapsed.count(), 0, MAX_ELAPSED_NANOSECONDS);
  m_pending += static_cast<std::uint64_t>(elapsed) * m_ticksPerSecond;

  const std::uint64_t due = m_pending / NANOSECONDS_PER_SECOND;
  m_pending %= NANOSECONDS_PER_SECOND;
  if (due > m_maxTicksPerAdvance)
    return m_maxTicksPerAdvance;
  return static_cast<std::uint32_t>(due);
}
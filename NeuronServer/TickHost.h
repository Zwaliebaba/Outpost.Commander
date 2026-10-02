#pragma once

namespace Neuron
{
// The one place wall time becomes simulation time (ADR-009). The host is told how much wall time has passed and answers
// how many fixed ticks are due. It keeps the remainder exactly, as nanoseconds times ticks per second, so no rounding
// drifts the rate over a long match.
class TickHost
{
public:
  // _maxTicksPerAdvance bounds the catch-up after a stall: a backlog beyond it is dropped, so the simulation resumes at its
  // own pace rather than running many ticks at once to catch up with the wall clock.
  TickHost(std::uint32_t _ticksPerSecond, std::uint32_t _maxTicksPerAdvance);

  // The ticks due after _elapsed more wall time, from 0 to the maximum. A negative duration counts as none.
  [[nodiscard]] std::uint32_t Advance(std::chrono::nanoseconds _elapsed) noexcept;

  // How much more wall time makes the next tick due, rounded up to a whole nanosecond: what a host that sleeps between
  // ticks waits for.
  [[nodiscard]] std::chrono::nanoseconds UntilNextTick() const noexcept;

  [[nodiscard]] std::uint32_t TicksPerSecond() const noexcept
  {
    return m_ticksPerSecond;
  }

private:
  std::uint32_t m_ticksPerSecond = 0;
  std::uint32_t m_maxTicksPerAdvance = 0;
  // Wall time not yet turned into a tick, in nanoseconds times ticks per second: a tick is due at each billion.
  std::uint64_t m_pending = 0;
};
} // namespace Neuron
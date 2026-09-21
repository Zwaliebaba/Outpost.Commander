#pragma once

#include "NeuronCore.h"

#include <chrono>
#include <cstdint>

namespace Neuron
{

/// WHERE WALL TIME MEETS THE TICK, AND THE ONLY PLACE IT IS ALLOWED TO (R16).
///
/// IT IS IN THE ENGINE, AND THAT IS NOT FILING. R9: an engine type is in the wrong layer if it has
/// to know a game concept by name, and a schedule knows none -- it counts periods. The first draft
/// of this put it in `GameLogic` next to the host loop, which placed `std::chrono` inside the
/// library the simulation lives in; `Scripts/CheckDeterminism.py` flagged eleven violations and was
/// right to. The period is a constructor argument rather than a constant here for the same reason:
/// fifty milliseconds is ADR-002's decision about the game, and this file does not get to know it.
///
/// IT DOES NOT ACCUMULATE DRIFT, which is the whole reason it is a type rather than a
/// `previous + period` inside a loop. Adding a period to the last wake-up carries every late
/// wake-up forward forever: twenty wake-ups a second, each a millisecond late, is a host that has
/// lost a tick a minute against the client it is talking to. The deadline here is **start plus n
/// periods**, computed from the beginning every time, so lateness is spent rather than banked.
///
/// The time is HANDED IN rather than read, which is what keeps a seam a seam: a suite can drive
/// hours of scheduled ticks in a millisecond, and nothing below this can tell how long anything
/// actually took.
class TickSchedule
{
public:
  /// A HOST THAT STALLS COMES BACK OWING AS MANY TICKS AS THE STALL WAS LONG -- a debugger break,
  /// a laptop lid, a scheduler hiccup -- and running them all takes longer than the stall did,
  /// which owes more still. Ten is the cap; past it the schedule gives the lost time up and says
  /// how much, so the match runs slow for an instant instead of freezing.
  static constexpr std::uint32_t MAX_CATCH_UP_TICKS = 10;

  /// _periodMilliseconds must be positive; a non-positive one is clamped to one, because a period
  /// of zero is a division by zero in a loop that is meant to be the steadiest thing in the
  /// process.
  TickSchedule(std::chrono::steady_clock::time_point _start, std::int64_t _periodMilliseconds) noexcept;

  /// How many ticks are owed at _now, consuming them. Zero most of the time: a loop asks far more
  /// often than a period.
  [[nodiscard]] std::uint32_t TicksDue(std::chrono::steady_clock::time_point _now) noexcept;

  /// When the next tick is owed, so a loop knows how long it may sleep.
  [[nodiscard]] std::chrono::steady_clock::time_point NextDeadline() const noexcept;

  /// Ticks actually issued. This is the simulation's clock.
  [[nodiscard]] std::uint64_t TicksIssued() const noexcept
  {
    return m_issued;
  }

  /// Ticks given up rather than chased. A health figure rather than an error: any of these outside
  /// a debugger break is a host that cannot hold its rate.
  [[nodiscard]] std::uint64_t TicksAbandoned() const noexcept
  {
    return m_abandoned;
  }

private:
  std::chrono::steady_clock::time_point m_start;
  std::chrono::milliseconds m_period;

  /// Grid ticks accounted for: issued plus abandoned. THIS IS WHAT THE GRID IS MEASURED AGAINST,
  /// not m_issued -- a schedule that advanced only by what it issued would owe the abandoned ticks
  /// again on the very next call, and a host that stalled once would abandon ticks for the rest of
  /// the match without ever catching up. A test caught exactly that.
  std::uint64_t m_consumed = 0;

  std::uint64_t m_issued = 0;
  std::uint64_t m_abandoned = 0;
};

} // namespace Neuron

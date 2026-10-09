#pragma once

namespace Outpost
{
// The player's own clock (Phase 5 design §7; owner, 2026-10-08). A scheduled order's time of day is the hour and minute the
// clock reads, which the client turns into the next moment it reads them, in UTC, for the server's host to make a tick of
// (ADR-080). It is the zone the system is set to, with its summer time, from the standard library's time zone database;
// or a fixed offset from UTC, where the database cannot say the zone, and for a test.
class PlayerClock
{
public:
  // A clock _offset ahead of UTC all year.
  explicit PlayerClock(std::chrono::minutes _offset = std::chrono::minutes{0}) noexcept
    : m_offset(_offset)
  {
  }

  // A clock in _zone, which the database keeps for as long as the program runs.
  explicit PlayerClock(const std::chrono::time_zone& _zone) noexcept
    : m_zone(&_zone)
  {
  }

  // The zone the system is set to, or UTC where the database cannot say it.
  [[nodiscard]] static PlayerClock Local() noexcept;

  // The first moment after _now at which the clock reads _hour:_minute. A reading the clock skips as summer time starts is
  // the moment it skips at, and one it reads twice as summer time ends is the first.
  [[nodiscard]] std::chrono::sys_seconds NextMoment(std::chrono::sys_seconds _now, int _hour, int _minute) const;

  // What the clock reads at _moment, "02:00".
  [[nodiscard]] std::string Reading(std::chrono::sys_seconds _moment) const;

private:
  [[nodiscard]] std::chrono::local_seconds ToLocal(std::chrono::sys_seconds _moment) const;
  [[nodiscard]] std::chrono::sys_seconds ToSystem(std::chrono::local_seconds _reading) const;

  const std::chrono::time_zone* m_zone = nullptr;
  std::chrono::minutes m_offset{0};
};
} // namespace Outpost

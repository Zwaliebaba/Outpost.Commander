#include "pch.h"
#include "PlayerClock.h"

Outpost::PlayerClock Outpost::PlayerClock::Local() noexcept
{
  try
  {
    return PlayerClock(*std::chrono::current_zone());
  }
  catch (const std::exception&)
  {
    return PlayerClock();
  }
}

std::chrono::sys_seconds Outpost::PlayerClock::NextMoment(std::chrono::sys_seconds _now, int _hour, int _minute) const
{
  const std::chrono::local_days today = std::chrono::floor<std::chrono::days>(ToLocal(_now));
  const auto on = [&](int _day)
  { return ToSystem(today + std::chrono::days{_day} + std::chrono::hours{_hour} + std::chrono::minutes{_minute}); };
  // Today's, unless it has passed; tomorrow's, unless a change of the clock put it behind; and the day after's surely.
  for (int day = 0; day < 2; ++day)
  {
    if (const std::chrono::sys_seconds moment = on(day); moment > _now)
      return moment;
  }
  return on(2);
}

std::string Outpost::PlayerClock::Reading(std::chrono::sys_seconds _moment) const
{
  const std::chrono::local_seconds reading = ToLocal(_moment);
  const std::chrono::hh_mm_ss time{reading - std::chrono::floor<std::chrono::days>(reading)};
  return std::format("{:02}:{:02}", time.hours().count(), time.minutes().count());
}

std::chrono::local_seconds Outpost::PlayerClock::ToLocal(std::chrono::sys_seconds _moment) const
{
  if (m_zone != nullptr)
    return m_zone->to_local(_moment);
  return std::chrono::local_seconds{_moment.time_since_epoch() + m_offset};
}

std::chrono::sys_seconds Outpost::PlayerClock::ToSystem(std::chrono::local_seconds _reading) const
{
  if (m_zone != nullptr)
    return m_zone->to_sys(_reading, std::chrono::choose::earliest);
  return std::chrono::sys_seconds{_reading.time_since_epoch() - m_offset};
}

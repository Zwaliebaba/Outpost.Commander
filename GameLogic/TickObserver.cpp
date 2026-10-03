#include "pch.h"
#include "TickObserver.h"

void Outpost::TickProfiler::Begin(TickPart _part) noexcept
{
  m_started[static_cast<std::size_t>(_part)] = std::chrono::steady_clock::now();
}

void Outpost::TickProfiler::End(TickPart _part) noexcept
{
  const auto index = static_cast<std::size_t>(_part);
  m_parts[index] += std::chrono::steady_clock::now() - m_started[index];
}

std::array<std::chrono::nanoseconds, Outpost::TICK_PART_COUNT> Outpost::TickProfiler::Take() noexcept
{
  return std::exchange(m_parts, {});
}

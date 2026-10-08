#include "pch.h"
#include "SeatController.h"

Outpost::SeatController::SeatController(std::uint32_t _ticksPerSecond) noexcept
  : m_delayTicks(std::uint64_t{DEPUTY_DELAY_SECONDS} * _ticksPerSecond)
{
}

bool Outpost::SeatController::DeputyPlays(std::uint64_t _tick, std::uint32_t _takings, bool _gone) noexcept
{
  if (_takings == 0)
    return true;
  if (!_gone)
    return false;
  // The delay runs from when this connection was first seen gone, and starts again for a newer one that goes.
  if (m_goneTaking != _takings)
  {
    m_goneTaking = _takings;
    m_goneTick = _tick;
  }
  return _tick >= m_goneTick + m_delayTicks;
}

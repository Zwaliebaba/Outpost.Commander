#include "pch.h"

#include "TransportRecovery.h"

namespace Neuron
{

bool TransportRecovery::ShouldReopen(TransportState _state, std::uint64_t _nowMilliseconds) noexcept
{
  if (_state != TransportState::Failed)
  {
    return false;
  }

  // Subtraction on the stored time rather than addition to it, as `JoinState::ShouldSend` does: a clock
  // that went backwards lands on a huge difference and reopens, which is the harmless answer.
  if ((m_reopens != 0) && ((_nowMilliseconds - m_lastReopenMilliseconds) < REOPEN_INTERVAL_MILLISECONDS))
  {
    return false;
  }

  m_lastReopenMilliseconds = _nowMilliseconds;
  ++m_reopens;
  return true;
}

} // namespace Neuron

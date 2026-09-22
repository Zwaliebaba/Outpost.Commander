#pragma once

#include "DatagramTransport.h"

#include <cstdint>

namespace Neuron
{

/// When a failed transport is closed and opened again.
///
/// **A FAILED SOCKET USED TO END THE CLIENT.** The frame loop broke out on `Failed`, which was right
/// for M0.5's probe, where a failure meant a missing capability or exemption and there was nothing to
/// do but say so. It is wrong for a game: a packaged client's socket can be closed under it while it is
/// suspended, and the resume then walked straight out of the match. A reopened socket is a new local
/// endpoint, and ADR-013's token is what gets the player the same seat at it.
///
/// **IT HOLDS NO SOCKET**, which is the split `JoinState` takes and for the same reason: the cadence is
/// arithmetic over a millisecond clock, and a suite can pin it without binding anything.
class TransportRecovery
{
public:
  /// **ONCE A SECOND, AND NOT TUNED.** Reopening is cheap -- a socket and a connect that sends nothing --
  /// but a failure that is not going away (a missing capability, an address that does not resolve)
  /// would otherwise reopen every frame. The first failure is answered at once.
  static constexpr std::uint64_t REOPEN_INTERVAL_MILLISECONDS = 1000;

  /// True when the transport should be closed and opened again now. It takes the reopen as having
  /// happened, so a caller that ignores the answer has stopped recovering -- hence `[[nodiscard]]`.
  [[nodiscard]] bool ShouldReopen(TransportState _state, std::uint64_t _nowMilliseconds) noexcept;

  [[nodiscard]] std::uint32_t ReopenCount() const noexcept
  {
    return m_reopens;
  }

private:
  std::uint64_t m_lastReopenMilliseconds = 0;
  std::uint32_t m_reopens = 0;
};

} // namespace Neuron

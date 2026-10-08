#pragma once

namespace Outpost
{
// Who plays a seat that has a deputy (Phase 5 design §6, gate H2, ADR-079). The seat is the deputy's until its player first
// takes it. The player plays it while the player's newest connection is open, and for DEPUTY_DELAY_SECONDS of ticks after
// that connection has gone, so that a connection dropped and taken up again soon never sees the deputy. Past that the
// deputy plays it, until the player takes the seat again. The server's thread asks once a tick.
class SeatController
{
public:
  explicit SeatController(std::uint32_t _ticksPerSecond) noexcept;

  // Whether the deputy plays the seat after the tick _tick, the seat having been taken _takings times so far, the last of
  // them by a connection that has gone when _gone.
  [[nodiscard]] bool DeputyPlays(std::uint64_t _tick, std::uint32_t _takings, bool _gone) noexcept;

private:
  std::uint64_t m_delayTicks = 0;
  // The taking whose connection was first seen gone, and the tick it was seen.
  std::uint32_t m_goneTaking = 0;
  std::uint64_t m_goneTick = 0;
};
} // namespace Outpost

#pragma once

namespace Outpost
{
// Who plays a seat that has a deputy (ADR-079 decision 4). The seat is the deputy's until its player first
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

  // The connection of the seat's _takings dropped by the server's owner at _tick (ADR-104): its deputy plays the seat from its
  // going, with _deputyAtOnce, and otherwise DEPUTY_DELAY_SECONDS after it, as for any connection gone.
  void Dropped(std::uint64_t _tick, std::uint32_t _takings, bool _deputyAtOnce) noexcept;

private:
  std::uint64_t m_delayTicks = 0;
  // The taking whose connection was first seen gone, or dropped, the tick it was, and whether the deputy takes it at once.
  std::uint32_t m_goneTaking = 0;
  std::uint64_t m_goneTick = 0;
  bool m_atOnce = false;
};
} // namespace Outpost

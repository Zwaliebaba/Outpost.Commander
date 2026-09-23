#pragma once

#include "GameCore.h"

#include <cstdint>

namespace Outpost
{

/// What a churner does on a harness tick.
enum class ChurnAction : std::uint8_t
{
  None,
  /// Close the transport. The host keeps the seat (ADR-013) and goes on sending to an endpoint that is gone.
  Drop,
  /// Open a new transport, which is a new endpoint, and ask again with the token held (`JoinState::Rejoin`).
  Rejoin
};

/// **WHEN A CHURNER DROPS AND WHEN IT COMES BACK**, by harness tick (ADR-022). It loads ADR-013's rejoin, the
/// session table's endpoint rebinding and the accumulator's reset, which no person does often enough to find
/// anything wrong with.
///
/// **IT NEVER DROPS A BOT THAT IS NOT SEATED, AND THAT IS THE RULE THAT MATTERS.** A seat is never given back,
/// so a bot that dropped between sending its join and hearing the answer would come back from a new endpoint
/// with no token, and the host would seat it a second time. The clock toward a drop starts only once the bot
/// is seated -- which is also what makes every rejoin present the token that seat was issued with.
class ChurnSchedule
{
public:
  /// **SEATED FOR TWO TO TEN SECONDS.** Long enough for the accumulator to have swept the client once at a
  /// hundred players (ADR-024's forty-three ticks), so a rejoin lands on a client the host has fully sent to;
  /// short enough that ten rejoins take a couple of minutes, which is M1.14b's criterion.
  static constexpr std::uint32_t SEATED_TICKS_MINIMUM = 40;
  static constexpr std::uint32_t SEATED_TICKS_MAXIMUM = 200;

  /// **AWAY FOR HALF A SECOND TO THREE SECONDS**, either side of `ClientFrame::LINK_SILENCE_MILLISECONDS`, so
  /// some rejoins happen before the client has noticed the silence and some after -- the two paths into
  /// `JoinState::Rejoin`.
  static constexpr std::uint32_t AWAY_TICKS_MINIMUM = 10;
  static constexpr std::uint32_t AWAY_TICKS_MAXIMUM = 60;

  ChurnSchedule(std::uint64_t _runSeed, std::uint32_t _botIndex) noexcept;

  /// What to do on this harness tick, given whether the bot's join is answered. Call it once a tick, with
  /// ticks that do not go backwards.
  [[nodiscard]] ChurnAction Advance(std::uint64_t _harnessTick, bool _seated) noexcept;

  [[nodiscard]] std::uint32_t RejoinCount() const noexcept
  {
    return m_rejoins;
  }

private:
  enum class Phase : std::uint8_t
  {
    /// Waiting to be seated; the drop clock is not running.
    Unseated,
    Seated,
    Away
  };

  Neuron::Pcg32 m_random;
  Phase m_phase = Phase::Unseated;
  std::uint64_t m_nextTick = 0;
  std::uint32_t m_rejoins = 0;
};

} // namespace Outpost

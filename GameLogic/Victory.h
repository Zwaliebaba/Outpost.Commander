#pragma once

#include "BuildSystem.h"
#include "World.h"

#include <cstdint>
#include <span>
#include <vector>

namespace Outpost
{

/// **SIX MINUTES** (`OpenQuestions.md` Q65): the tick the match ends on the clock if nobody has won by then.
inline constexpr std::uint32_t MATCH_CLOCK_TICKS = 7200;

/// How a match ended. R8: a public aggregate.
struct MatchOutcome
{
  bool over = false;

  /// NO_PLAYER when it was a draw.
  PlayerId winner = NO_PLAYER;

  /// Ended by the clock rather than by the last station standing.
  bool onClock = false;
};

/// **ELIMINATION AND VICTORY** (M3.7, `GameDesign.md` section 2, Q65), last in the tick after deaths
/// (`TechnicalDesign.md` section 2).
///
/// - **A player whose station is gone is eliminated, and everything they still own is removed on the same tick.**
///   Leaving a beaten player's ships alive to be hunted turns the end of every match into a search problem.
///   Elimination is read from the world -- no station, no seat -- so it is state the hash already covers.
/// - **The last station standing wins.** If every remaining station died on one tick, the match is a draw.
/// - **At `MATCH_CLOCK_TICKS` the clock decides**: the most station hull wins, then credits plus the catalog cost
///   of live ships and modules, then a draw.
///
/// Every sweep is in slot and player order, so a simultaneous end resolves the same way on every run (R16).
class Victory
{
public:
  /// A new match of _players seats, beginning at host tick _startTick.
  void Begin(std::size_t _players, std::uint32_t _startTick) noexcept;

  /// One tick, after deaths. _tick is the host's tick counter for this tick.
  void Advance(World& _world, const BuildSystem& _build, std::uint32_t _tick);

  [[nodiscard]] const MatchOutcome& Outcome() const noexcept
  {
    return m_outcome;
  }

  /// Everything removed this tick because its owner was eliminated, in slot order.
  [[nodiscard]] std::span<const EntityId> Removed() const noexcept
  {
    return m_removed;
  }

private:
  std::size_t m_players = 0;
  std::uint32_t m_startTick = 0;
  MatchOutcome m_outcome{};
  std::vector<EntityId> m_removed;
};

} // namespace Outpost

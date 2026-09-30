#pragma once

namespace Outpost
{
// Keeps one player's ships moving during task 2.7's tick measurement, so that the tick pays for pathing, formations and
// separation as it would in a battle: every period, every ship of the player's is ordered to the far one of two
// points, and both players' fleets cross the middle of the map. A measurement run only; never part of a match.
class LoadDriver
{
public:
  // Ticks between orders.
  static constexpr std::uint64_t PERIOD_TICKS = 200;

  // The orders for this snapshot: one move for all of the snapshot's player's ships when a period begins, none otherwise.
  [[nodiscard]] std::vector<Command> Update(const Snapshot& _snapshot);

private:
  std::optional<std::uint64_t> m_lastPeriod;
};
} // namespace Outpost

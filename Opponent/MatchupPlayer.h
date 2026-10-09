#pragma once

namespace Outpost
{
// The AI's battle behavior for a battle matchup (horizon §9, ADR-083), on either side: once a second its warships attack-move
// on the enemy ship nearest the middle of them, or the nearest enemy structure once no enemy ship is in sight, and the
// game's own targeting picks what each fires at. It plays as a client does, from its snapshots, and orders nothing once its
// warships are gone.
class MatchupPlayer
{
public:
  static constexpr std::uint32_t DECISION_SECONDS = 1;

  explicit MatchupPlayer(std::uint32_t _ticksPerSecond) noexcept
    : m_ticksPerSecond(_ticksPerSecond)
  {
  }

  // Reads one snapshot of its player's, and returns the orders to send.
  [[nodiscard]] std::vector<Command> Update(const Snapshot& _snapshot);

private:
  std::uint32_t m_ticksPerSecond = 0;
  std::uint64_t m_nextDecisionTick = 0;
};
} // namespace Outpost

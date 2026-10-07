#pragma once

namespace Outpost
{
// Where a ship's hit points send it to be repaired (Phase 4 design §10, ADR-075): never, or once a hit leaves it below half
// or a quarter of its full hit points. A design carries one, and every ship built to it starts with it.
enum class RetreatThreshold : std::uint8_t
{
  Never,
  Half,
  Quarter
};

// What a ship has unless a player says otherwise: with dear ships, a fight to the last ship is a choice (design §10).
inline constexpr RetreatThreshold DEFAULT_RETREAT = RetreatThreshold::Quarter;

// The share of its full hit points, in percent, below which a hit sends a ship back; zero for never.
[[nodiscard]] constexpr std::int32_t RetreatPercent(RetreatThreshold _threshold) noexcept
{
  switch (_threshold)
  {
  case RetreatThreshold::Half:
    return 50;
  case RetreatThreshold::Quarter:
    return 25;
  case RetreatThreshold::Never:
    break;
  }
  return 0;
}
} // namespace Outpost

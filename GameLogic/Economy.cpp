#include "pch.h"

#include "Economy.h"

namespace Outpost
{

void Economy::Begin() noexcept
{
  m_pendingMilliCreditHundredths.fill(0);
}

void Economy::Credit(std::span<const OreDelivery> _deliveries, const World& _world, BuildSystem& _build) noexcept
{
  constexpr std::uint64_t HUNDREDTHS_PER_CREDIT = static_cast<std::uint64_t>(MILLI_ORE_PER_ORE) * 100;

  for (const OreDelivery& delivery : _deliveries)
  {
    if ((delivery.player == NO_PLAYER) || (delivery.player > MAX_PLAYERS))
    {
      continue;
    }

    // THE PERCENTAGE IS APPLIED BEFORE ANY DIVISION (Q56), so it is exact at every level.
    std::uint64_t& pending = m_pendingMilliCreditHundredths[delivery.player];
    pending += static_cast<std::uint64_t>(delivery.milliOre) * CREDITS_PER_ORE * CargoValuePercent(_world, delivery.player);

    const std::uint64_t whole = pending / HUNDREDTHS_PER_CREDIT;
    if (whole > 0)
    {
      _build.Grant(delivery.player, static_cast<std::uint32_t>(whole));
      pending -= whole * HUNDREDTHS_PER_CREDIT;
    }
  }
}

std::uint32_t Economy::PendingMilliCredits(PlayerId _player) const noexcept
{
  return ((_player == NO_PLAYER) || (_player > MAX_PLAYERS)) ? 0
                                                             : static_cast<std::uint32_t>(m_pendingMilliCreditHundredths[_player] / 100);
}

} // namespace Outpost

#include "pch.h"

#include "Economy.h"

namespace Outpost
{

void Economy::Begin() noexcept
{
  m_pendingMilliCredits.fill(0);
}

void Economy::Credit(std::span<const OreDelivery> _deliveries, BuildSystem& _build) noexcept
{
  for (const OreDelivery& delivery : _deliveries)
  {
    if ((delivery.player == NO_PLAYER) || (delivery.player > MAX_PLAYERS))
    {
      continue;
    }

    std::uint32_t& pending = m_pendingMilliCredits[delivery.player];
    pending += delivery.milliOre * CREDITS_PER_ORE;

    const std::uint32_t whole = pending / MILLI_ORE_PER_ORE;
    if (whole > 0)
    {
      _build.Grant(delivery.player, whole);
      pending -= whole * MILLI_ORE_PER_ORE;
    }
  }
}

std::uint32_t Economy::PendingMilliCredits(PlayerId _player) const noexcept
{
  return ((_player == NO_PLAYER) || (_player > MAX_PLAYERS)) ? 0 : m_pendingMilliCredits[_player];
}

} // namespace Outpost

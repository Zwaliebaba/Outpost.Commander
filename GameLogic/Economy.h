#pragma once

#include "BuildSystem.h"
#include "MiningSystem.h"

#include <array>
#include <cstdint>
#include <span>

namespace Outpost
{

/// **ONE ORE IS ONE CREDIT** (`GameDesign.md` section 4: a `MiningLaser` "carries 100 credits of capacity").
/// M2.12's ore processor multiplies it; until then it is exactly this.
inline constexpr std::uint32_t CREDITS_PER_ORE = 1;

/// **CREDITS ACCRUE ON UNLOAD** (M2.7): the tick's deliveries, turned into credits and granted to their
/// players through `BuildSystem::Grant`, which is the balance's one owner.
///
/// **THE REMAINDER IS KEPT, PER PLAYER.** Unloading moves 2,500 thousandths of ore a tick, which is two and a
/// half credits; granting the whole part and dropping the half would lose a fifth of every hold. So each
/// player's thousandths accumulate here and whole credits leave as they complete, and a hold of 100 ore is
/// exactly 100 credits however its ticks divide.
///
/// **THE CLIENT DOES NOT DERIVE INCOME** (R19, `OpenQuestions.md` Q36): credits reach it only through the
/// per-player block, and the panel's change flash is how it shows money arriving.
class Economy
{
public:
  /// A new match: nothing owed to anybody.
  void Begin() noexcept;

  /// Grants what _deliveries earned. In the order they happened, which is index order -- and the order
  /// cannot matter, because each player's sum is the same whichever way round it is added.
  void Credit(std::span<const OreDelivery> _deliveries, BuildSystem& _build) noexcept;

  /// Thousandths of a credit a player has earned and not yet been granted. Never a whole credit.
  [[nodiscard]] std::uint32_t PendingMilliCredits(PlayerId _player) const noexcept;

private:
  std::array<std::uint32_t, MAX_PLAYERS + 1> m_pendingMilliCredits{};
};

} // namespace Outpost

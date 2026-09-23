#pragma once

#include "BuildSystem.h"
#include "MiningSystem.h"
#include "ModuleEffects.h"

#include <array>
#include <cstdint>
#include <span>

namespace Outpost
{

/// **ONE ORE IS ONE CREDIT** (`GameDesign.md` section 4: a `MiningLaser` "carries 100 credits of capacity").
/// M2.12's ore processor multiplies it (`CargoValuePercent`).
inline constexpr std::uint32_t CREDITS_PER_ORE = 1;

/// **CREDITS ACCRUE ON UNLOAD** (M2.7): the tick's deliveries, turned into credits and granted to their
/// players through `BuildSystem::Grant`, which is the balance's one owner.
///
/// **THE REMAINDER IS KEPT, PER PLAYER.** Unloading moves 2,500 thousandths of ore a tick, which is two and a
/// half credits; granting the whole part and dropping the half would lose a fifth of every hold. So each
/// player's thousandths accumulate here and whole credits leave as they complete, and a hold of 100 ore is
/// exactly 100 credits however its ticks divide.
///
/// **AN ORE PROCESSOR IS EXACT** (M2.12, `OpenQuestions.md` Q56): the remainder is carried in hundredths of a
/// thousandth, so what a delivery is worth at 125% or 150% is added in full and nothing is rounded until a whole
/// credit leaves. 2,500 thousandths at 125% is 3,125, and a hold of 100 ore under an L1 is exactly 125 credits.
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
  ///
  /// _world is read for each player's ore processor, as it stands this tick.
  void Credit(std::span<const OreDelivery> _deliveries, const World& _world, BuildSystem& _build) noexcept;

  /// Thousandths of a credit a player has earned and not yet been granted, rounded down. Never a whole credit.
  [[nodiscard]] std::uint32_t PendingMilliCredits(PlayerId _player) const noexcept;

private:
  /// In hundredths of a thousandth of a credit, which is what makes a percentage exact.
  std::array<std::uint64_t, MAX_PLAYERS + 1> m_pendingMilliCreditHundredths{};
};

} // namespace Outpost

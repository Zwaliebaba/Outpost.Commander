#pragma once

#include "World.h"

#include <cstdint>

namespace Outpost
{

/// M2.12: **WHAT A PLAYER'S MODULES DO**, as the two integer percentages the rest of the simulation multiplies by
/// (`GameDesign.md` section 5, R16). Read from the world each time they are needed rather than cached, so a
/// module that is built, upgraded or destroyed takes effect on the next order with nothing to keep in step.
///
/// **WHICH MODULE COUNTS: THE BEST OF ITS KIND.** Nothing stops a player placing two shipyards, and section 5 does
/// not say they stack. Taking the largest multiplier among the player's modules of an effect makes a second one
/// worth nothing but its hull, which is the conservative reading -- stacking would be a design change and
/// belongs on the register first.

/// The player's build-rate multiplier, in hundredths: 100 with no shipyard, 150 at L1, 200 at L2.
[[nodiscard]] std::uint32_t BuildRateMultiplierPercent(const World& _world, PlayerId _player) noexcept;

/// What a delivered cargo is worth, in hundredths: 100 with no ore processor, 125 at L1, 150 at L2.
[[nodiscard]] std::uint32_t CargoValuePercent(const World& _world, PlayerId _player) noexcept;

} // namespace Outpost

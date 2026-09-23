#pragma once

#include "Design.h"

#include <cstdint>

namespace Outpost
{

/// What a design is, computed rather than written down.
///
/// R8: a public aggregate.
struct DerivedStats
{
  /// The hull plus its drive plus everything in its slots.
  std::uint32_t mass = 0;

  /// **World units a second, and ZERO for a hull with no drive** -- not a division that happens to
  /// come out at zero. A station has no thrust and the derivation returns before it divides.
  std::uint32_t speedUnitsPerSecond = 0;

  /// Credits. The hull plus the drive plus the components, which is why the Miner's 150 and the
  /// Fighter's 300 are sums rather than figures anybody typed.
  std::uint32_t cost = 0;

  /// The hull's, unmodified. Components do not add hull points in this design; if one ever does,
  /// this is the sum that would carry it.
  std::uint32_t hullPoints = 0;

  /// Summed over the slots (Q32), so a two-slot miner is a table row rather than a mechanic.
  std::uint32_t orePerSecond = 0;
  std::uint32_t oreCapacity = 0;

  /// **HOW CLOSE A MINER MUST BE TO EXTRACT** (M2.6): the longest reach among its mining tools, and zero
  /// for a design with none. **The longest, not a sum** -- two lasers side by side do not reach twice as
  /// far -- and only the tools that extract count, so a mass driver's 600 is not a mining range.
  std::uint32_t miningRangeUnits = 0;

  /// The hull's, carried through so the mining loop asks the derivation and not the catalog (R24).
  bool acceptsOre = false;

  /// Summed over the slots, per mount. A `Frigate` with two mass drivers does 50 a second.
  std::uint32_t damagePerSecond = 0;

  [[nodiscard]] friend constexpr bool operator==(const DerivedStats&, const DerivedStats&) noexcept = default;
};

/// **THE SINGLE MOST IMPORTANT FUNCTION IN THE GAME'S RULES** (ADR-006, R24, M1.2), and the one
/// place a rule about what a ship can do is allowed to live. Both the host and the client call it,
/// which is what lets a client preview an order against the rules the host validates with.
///
/// **A `Frigate` CARRYING TWO MASS DRIVERS IS SLOWER THAN AN EMPTY ONE BECAUSE OF ARITHMETIC**, and
/// nothing anywhere writes that down. Mass is the hull plus its contents; speed is thrust over
/// mass. Adding a component lowers the speed, which the suite asserts as a property over every
/// combination in the catalog rather than as a value on the two designs that happen to ship.
///
/// **INTEGERS THROUGHOUT (R16).** The division truncates, which is C++'s rule rather than one this
/// function invents, and the two shipped designs divide exactly -- 2,000 over 20 and 5,600 over 40
/// -- so neither figure depends on the rounding. That is a property of Q46's numbers and the suite
/// pins it, because a later change to the catalog could quietly make it false.
///
/// **NO BUILD TIME, AND IT IS NOT DERIVED HERE.** ADR-006 names it alongside cost and no figure for it
/// exists anywhere in the design. M1.6 was the step that first needed one and it did not put it here:
/// build time is cost divided by the STATION's rate, which a shipyard module multiplies
/// (`GameDesign.md` section 5), so it is a property of the thing building rather than of the thing
/// built. `GameLogic/BuildSystem.h` owns it and `OpenQuestions.md` Q47 is the figure.
[[nodiscard]] DerivedStats Derive(const DesignEntry& _design) noexcept;

/// The same, for a design identity.
[[nodiscard]] DerivedStats Derive(DesignId _id) noexcept;

/// **The composition a design is not.** M1.2 pins every catalog combination and not only the three
/// rows that ship, so the derivation is exercised over hulls nothing builds -- which is the whole
/// reason the `Cruiser` is in the catalog (`GameDesign.md` section 6, section 10).
[[nodiscard]] DerivedStats Derive(HullId _hull, DriveId _drive, const std::array<ComponentId, MAX_COMPONENT_SLOTS>& _slots) noexcept;

} // namespace Outpost

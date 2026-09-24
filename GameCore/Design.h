#pragma once

#include "Catalog.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace Outpost
{

/// **A DESIGN IS A ROW, NOT A TYPE** (ADR-006, R24). Nothing in the simulation knows what a
/// "fighter" is; it knows a design, and a design is a hull, an optional drive, and a component in
/// each of the hull's slots. `Miner` and `Fighter` are identities into this table and the station
/// is a third row beside them -- **a grep for "fighter" outside a comment or a display string is
/// this step having failed**.
///
/// **THE MVP SHIPS NO DESIGNER AND THAT CHANGES NOTHING HERE.** M4's designer screen writes rows
/// into a second table; research gates a set of component identities. Neither needs this file to
/// change, which is the claim ADR-006 exists to make and the reason a design is an identity rather
/// than a struct somebody constructs.
enum class DesignId : std::uint8_t
{
  Miner,
  Fighter,
  Station,

  /// **THE FOUR MODULES** (M2.9, ADR-015): a `ModuleFrame` carrying one module component, **one row per
  /// level**, because each level is its own component identity and upgrading replaces one with the next.
  /// Named as their meshes are (`Design/design_handoff_meshes/`).
  ModuleShipyardL1,
  ModuleShipyardL2,
  ModuleOreProcessorL1,
  ModuleOreProcessorL2,

  /// **THE FORWARD DEPOT** (M3.9, `OpenQuestions.md` Q69): a placed unload point near a far field, at least 2,000
  /// from every station and within 800 of a rock, two a player at most. Not buildable -- it is placed by a tap, as a
  /// module is.
  Depot,

  /// **THE HEAVY DESIGN, REINSTATED** (M4.4, `OpenQuestions.md` Q75): a `Cruiser` hull, a `BurnDrive` and four
  /// `HeavyDriver`s. Appended, because an identity is on the wire.
  Cruiser,

  /// **THE RESEARCH STATION** (M4.4b, `OpenQuestions.md` Q85): a module frame carrying `ResearchStationL1`, placed by a
  /// tap like the other modules. There is no level two.
  ModuleResearchStationL1
};

/// The largest slot count any hull in the catalog has -- the `Cruiser`'s four.
///
/// **NAMED FOR THE SLOTS IT COUNTS**, because `GameLogic/World.h` has a `MAX_SLOTS` of its own that
/// means something else entirely: how many ENTITIES the store holds. Two different things called
/// the same thing in one namespace is a compile error today and would have been a confusing one to
/// read; this is the component slots a hull has. A design carries a
/// fixed array of this size and the entries past the hull's own slot count are `None`, which is
/// what makes the derivation a sum over a fixed span rather than a container.
inline constexpr std::size_t MAX_COMPONENT_SLOTS = 4;

/// One row of ADR-006's design table.
///
/// R8: a public aggregate.
struct DesignEntry
{
  DesignId id = DesignId::Miner;
  HullId hull = HullId::Scout;

  /// **`None` IS A LEGAL DRIVE** and it is what the station has. A hull with no drive does not
  /// move, which is a composition rather than a special case (R24).
  DriveId drive = DriveId::None;

  /// Fitted in slot order. Entries past the hull's `slotCount` are `None` and contribute nothing.
  std::array<ComponentId, MAX_COMPONENT_SLOTS> slots{};

  /// **WHETHER A PLAYER CAN ORDER ONE.** `GameDesign.md` section 5 has a station placed by the
  /// generator rather than built, and section 6 names exactly two buildable ships -- so this is a
  /// property of the ROW, which is what R24 asks for, rather than a rule somewhere that knows a
  /// `Station` is special.
  ///
  /// A module is a design too (ADR-015, M2.9), and it is placed by tap rather than queued, so it is
  /// false here as well: the build panel's module row arms a placement (M2.11), which `PlaceModule`
  /// carries, and a `Build` naming a module is refused.
  bool buildable = false;

  [[nodiscard]] friend constexpr bool operator==(const DesignEntry&, const DesignEntry&) noexcept = default;
};

[[nodiscard]] std::span<const DesignEntry> Designs() noexcept;

/// The row an identity names. Total, for the reason `Hull` is.
[[nodiscard]] const DesignEntry& Design(DesignId _id) noexcept;

/// **THE UNLOCK BITS A DESIGN NEEDS** (M4.4b, Q85): every component it carries that has one. Zero for a design any
/// player may build from the start. **One predicate over component identity**, as R24 wants: nothing here knows that
/// the Cruiser is the design research gates.
[[nodiscard]] std::uint8_t RequiredUnlocks(DesignId _design) noexcept;

/// Whether a player whose unlock byte is `_unlocked` may build `_design`. The host refuses on it and the client dims
/// on it -- the same rule on both sides (R19).
[[nodiscard]] bool DesignUnlocked(DesignId _design, std::uint8_t _unlocked) noexcept;

/// The shipyard level `_design` needs, which is its hull's (Q84, Q85). Zero for what no station builds.
[[nodiscard]] std::uint8_t RequiredShipyardLevel(DesignId _design) noexcept;

/// **A PLAYER'S SHIPYARD LEVEL**: the highest level among the modules they own, or zero with none. The host passes
/// the designs of its modules in the world and the client the ones it was sent.
[[nodiscard]] std::uint8_t ShipyardLevelOf(std::span<const DesignId> _modules) noexcept;

} // namespace Outpost

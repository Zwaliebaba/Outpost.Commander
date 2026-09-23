#pragma once

#include "SizeClass.h"

#include "NeuronCore.h"

#include <cstddef>
#include <cstdint>
#include <span>

namespace Outpost
{

/// ADR-006's catalog: five hulls, two drives, three slot components and four module components,
/// **referred to by identity and never by name**.
///
/// THAT INDIRECTION IS THE ONLY PROPERTY RESEARCH NEEDS (ADR-006, R24), and it is the whole reason
/// this is a table rather than three structs with baked numbers. Research adds an availability gate
/// over a set of identities and changes nothing else; a designer screen at M4 is a second table.
/// Nothing in the simulation knows what a "fighter" is -- it knows a design, and a design is a
/// composition.
///
/// **`constexpr` TABLES AND NOT FILES.** `TechnicalDesign.md` section 7 draws the line: a content
/// format is post-MVP, and ADR-021's ruling that content files ship does not move it -- R16 keeps
/// simulation data `constexpr` in `GameCore` precisely so the two sides cannot disagree about it.
///
/// **THE CRUISER IS HERE AND NOTHING BUILDS IT.** `GameDesign.md` section 6 and section 10 keep it
/// deliberately: the derivation function and its suite cover every hull, so reinstating a heavy
/// design at M4 is a table row. A catalog that held only what the MVP builds would make that a
/// change to the model instead.
///
/// === WHAT THIS FILE DOES NOT CARRY, AND WHY ====================================================
///
/// **MASS, THRUST AND PER-ITEM COST ARE Q46's**, answered 2026-09-22. `GameDesign.md` section 6
/// states a hull's mass as "low", "medium" or "high" and a drive as "balanced thrust, cheap"
/// against "more thrust for more mass and more cost" -- relations rather than figures -- while
/// fixing three outcomes they have to reproduce: a Miner at 150 credits and 100 u/s, a Fighter at
/// 300 and 140, and the cut battleship at 2,400. The register carries the arithmetic; **all three
/// land exactly and every division is whole**, which matters under R16 because it means the figures
/// do not depend on a rounding rule.
///
/// **BUILD TIME IS STILL NOT HERE.** ADR-006 says "cost and build time are sums" and no figure for
/// it exists anywhere in the design, nor any outcome it must reproduce -- there is no base build
/// rate for the shipyard's x1.5 to multiply. M1.6 is the step that first observes it.
///
/// **SIZE IN WORLD UNITS IS NOT HERE EITHER.** That is Q37, still open, and R24 wants the catalog
/// to name it so a script can compare the figure against the mesh's extent rather than a reader
/// trusting both.

/// A hull. **The station and a module frame are hulls**, which is what makes `GameDesign.md`
/// section 5 possible without a second kind of thing in the simulation (ADR-015).
enum class HullId : std::uint8_t
{
  Scout,
  Frigate,
  Cruiser,
  Station,
  ModuleFrame
};

/// **A DRIVE IS OPTIONAL AND `None` IS WHAT A STATION HAS.** A hull with no drive does not move,
/// which is what a station and a module frame both are -- not a special case in the code, an
/// absent component (R24).
enum class DriveId : std::uint8_t
{
  None,
  IonDrive,
  BurnDrive
};

/// What goes in a hull's slots. **Weapons and modules share one identity space** because a slot
/// holds a component and the simulation does not care which kind it is -- a `ShipyardL1` in a
/// module frame's slot is the same arrangement as a `MassDriver` in a frigate's.
enum class ComponentId : std::uint8_t
{
  None,
  MiningLaser,
  MassDriver,
  PointDefense,
  ShipyardL1,
  ShipyardL2,
  OreProcessorL1,
  OreProcessorL2
};

/// One row of `GameDesign.md` section 6's hull table.
///
/// R8: a public aggregate.
struct HullEntry
{
  HullId id = HullId::Scout;
  /// How many components it carries.
  std::uint8_t slotCount = 0;
  std::uint16_t hullPoints = 0;
  SizeClass sizeClass = SizeClass::Light;

  /// Q46. **Zero for a hull that cannot carry a drive**, which is what section 6's dash means:
  /// mass is unobservable without one, because nothing divides by it.
  std::uint16_t mass = 0;

  /// Q46, and zero for the two base structures -- a station is placed by the generator, and a module is
  /// priced by its component alone (M2.9, `GameDesign.md` section 5), so the frame adds nothing.
  std::uint16_t cost = 0;

  /// **ONLY THE TWO BASE STRUCTURES CARRY ONE** (`GameDesign.md` section 6). Zero means the hull is
  /// damaged through section 7's size-class table instead, which is the other of the two mitigation
  /// models that section names -- and a dash in the design's table is this zero.
  std::uint16_t hitValue = 0;

  /// Q37, answered 2026-09-22: **how big the hull is, in world units, along its longest axis.**
  ///
  /// **IT IS THE MESH'S EXTENT AND NOT A SEPARATE FIGURE.** ADR-005 draws a mesh at its authored
  /// scale and never scales it at draw time, so whatever is in the file IS the size -- and R24
  /// wants that named in the catalog rather than discovered by loading geometry, which is what
  /// makes this a row and not a comment. `Scripts/CheckMeshes.py` compares the two statements at
  /// M1.9; until then this is the one the simulation reasons with.
  ///
  /// **ROUNDED UP, BECAUSE IT IS A BOUND.** The delivered `ModuleFrame` is 83.52 units across and
  /// this says 84: the figure is used for spacing things so they do not overlap and for how far in
  /// front of a station a new ship appears, and both want the larger number.
  ///
  /// **THE `Cruiser` IS THE ONE ROW NO FILE BACKS.** It is cut from the MVP (`GameDesign.md`
  /// section 6) so no mesh was authored for it; 150 sits between the `Frigate`'s 90 and the
  /// `Station`'s 220, and M4 authors a mesh to this number rather than the other way round.
  std::uint16_t sizeUnits = 0;

  /// **WHETHER A MINER MAY UNLOAD HERE** (M2.6, `GameDesign.md` section 4): "the nearest thing you own that
  /// accepts ore". A property of the hull and not of a type, so the station accepts it because its row
  /// says so -- and a mining factory at a contested field later is a hull with this set, not a branch in
  /// the mining loop. Only the `Station` sets it today.
  bool acceptsOre = false;

  [[nodiscard]] friend constexpr bool operator==(const HullEntry&, const HullEntry&) noexcept = default;
};

/// One row of the drive table. Its figures are Q46's.
///
/// R8: a public aggregate.
struct DriveEntry
{
  DriveId id = DriveId::None;

  /// Q46. **`None` carries zeros and that is what makes the derivation total** -- a station sums
  /// over a drive like anything else and the one it has contributes nothing.
  std::uint16_t mass = 0;
  std::uint16_t thrust = 0;
  std::uint16_t cost = 0;

  [[nodiscard]] friend constexpr bool operator==(const DriveEntry&, const DriveEntry&) noexcept = default;
};

/// One row of the slot-component tables, weapons and modules alike.
///
/// R8: a public aggregate.
struct ComponentEntry
{
  ComponentId id = ComponentId::None;

  /// Q46. A module has none: it is bolted to a frame that never moves.
  std::uint16_t mass = 0;

  /// Zero where the component does not reach -- a module does not have a range.
  std::uint16_t rangeUnits = 0;

  /// Damage a second, per mount. Zero for anything that does not shoot; `GameDesign.md` section 6
  /// says a `MiningLaser` does no damage in as many words.
  std::uint16_t damagePerSecond = 0;

  /// A `MiningLaser` extracts this much ore a second and carries this much of it. **Both sum over
  /// a hull's slots** (Q32), so a two-slot miner is a table row rather than a mechanic.
  std::uint16_t orePerSecond = 0;
  std::uint16_t oreCapacity = 0;

  /// What a module costs to build. Zero for a weapon, whose cost belongs to the design that
  /// carries it.
  std::uint16_t cost = 0;

  /// A shipyard multiplies the station's build rate and an ore processor multiplies a delivered
  /// cargo's worth. **In hundredths, because the simulation is integers (R16)**: 150 is x1.5 and
  /// 125 is +25%. One hundred, or zero, is no effect.
  std::uint16_t multiplierPercent = 0;

  /// `PointDefense` is station slots only (`GameDesign.md` section 6). The rule is stated here so
  /// that build validation reads it from the catalog rather than naming the component.
  bool stationSlotsOnly = false;

  [[nodiscard]] friend constexpr bool operator==(const ComponentEntry&, const ComponentEntry&) noexcept = default;
};

/// Every hull, in identity order. **The order is the enumerator's and a test asserts it**, so an
/// identity is an index and the lookup below is not a search.
[[nodiscard]] std::span<const HullEntry> Hulls() noexcept;
[[nodiscard]] std::span<const DriveEntry> Drives() noexcept;
[[nodiscard]] std::span<const ComponentEntry> Components() noexcept;

/// The entry an identity names.
///
/// **EVERY IDENTITY RESOLVES AND THERE IS NO FAILURE PATH**, which is M1.1's first "done when": the
/// enumerators and the tables are the same list, asserted by a test, so a reference is total rather
/// than something a caller has to check. An identity cannot be constructed from outside that list
/// without a cast, and a cast is the caller writing a bug on purpose.
[[nodiscard]] const HullEntry& Hull(HullId _id) noexcept;
[[nodiscard]] const DriveEntry& Drive(DriveId _id) noexcept;
[[nodiscard]] const ComponentEntry& Component(ComponentId _id) noexcept;

} // namespace Outpost

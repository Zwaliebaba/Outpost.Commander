// A shared-items translation unit -- see NeuronCore.cpp for why it carries no precompiled header.

#include "Catalog.h"

#include <array>

namespace Outpost
{

namespace
{
/// `GameDesign.md` section 6's hull table, in identity order. Hull points and hit values are the
/// design's own figures; mass is not here (Q46) and neither is size in world units (Q37).
constexpr std::array<HullEntry, 5> HULLS{{
  {.id = HullId::Scout, .slotCount = 1, .hullPoints = 450, .sizeClass = SizeClass::Light, .mass = 10, .cost = 60, .sizeUnits = 60},
  {.id = HullId::Frigate, .slotCount = 2, .hullPoints = 600, .sizeClass = SizeClass::Medium, .mass = 20, .cost = 100, .sizeUnits = 90},

  // 2,080 is Q46's, and it is not a free choice: `GameDesign.md` section 6 cut the battleship at
  // **2,400 credits**, so a Cruiser with a BurnDrive and four MassDrivers has to sum to exactly
  // that. The hull is what is left over -- 2,400 less 80 less four sixties.
  //
  // AND IT IS THE ONE SIZE NO FILE BACKS: nothing authored a Cruiser mesh, because nothing builds one.
  // 150 sits between the Frigate's 90 and the Station's 220, and M4 authors to it.
  {.id = HullId::Cruiser, .slotCount = 4, .hullPoints = 3000, .sizeClass = SizeClass::Heavy, .mass = 60, .cost = 2080, .sizeUnits = 150},

  // The two base structures, and the only two rows with a hit value. A dash in the design's table
  // is this zero, and it means the hull is damaged through section 7's size-class table instead.
  //
  // MASS AND COST ARE ZERO AND THAT IS THE DESIGN'S DASH, not a gap: neither carries a drive, so
  // nothing divides by the mass. A station is placed by the generator, and **a module frame costs
  // nothing on its own** (M2.9): `GameDesign.md` section 5 prices a module by its level -- 400, 700, 350,
  // 600 -- and that is the component's cost, so the frame adds none and the sum is the design's figure.
  {.id = HullId::Station,
   .slotCount = 2,
   .hullPoints = 8000,
   .sizeClass = SizeClass::Heavy,
   .hitValue = 300,
   .sizeUnits = 220,
   .acceptsOre = true},
  {.id = HullId::ModuleFrame, .slotCount = 1, .hullPoints = 1500, .sizeClass = SizeClass::Heavy, .hitValue = 300, .sizeUnits = 90},
}};

/// **`None` IS A ROW.** A hull with no drive does not move, and making absence an identity rather
/// than a null keeps the derivation total: it sums over a drive like any other, and the station's
/// is the one that contributes nothing.
constexpr std::array<DriveEntry, 3> DRIVES{{
  // Zeros, and they are what make the derivation total rather than conditional.
  {.id = DriveId::None},

  // Q46. The two anchors: 2,000 over a Miner's mass of 20 is exactly 100 u/s, and 5,600 over a
  // Fighter's 40 is exactly 140. **Both divisions are whole**, so neither figure depends on a
  // rounding rule (R16).
  {.id = DriveId::IonDrive, .mass = 5, .thrust = 2000, .cost = 40},

  // "More thrust for more mass and more cost" (`GameDesign.md` section 6) -- all three, and the
  // relations are asserted by the suite rather than left to a reader comparing rows.
  {.id = DriveId::BurnDrive, .mass = 10, .thrust = 5600, .cost = 80},
}};

/// The three weapon components and the four module ones, and `None` for an empty slot.
///
/// `ResearchStationL1` IS NOT HERE. `GameDesign.md` section 6 lists it with a dash for its cost and
/// says it is "designed at M4, when there is research for it to do" -- Q30's answer is that a
/// module which costs credits and does nothing is the mistake the heavy design already made. The
/// section's own count agrees: "four module components", against five rows in its table.
constexpr std::array<ComponentEntry, 8> COMPONENTS{{
  {.id = ComponentId::None},

  // Does no damage, in as many words. Both figures sum over a hull's slots (Q32).
  {.id = ComponentId::MiningLaser, .mass = 5, .rangeUnits = 200, .damagePerSecond = 0, .orePerSecond = 20, .oreCapacity = 100, .cost = 50},
  // Section 7's table, row by row: mass drivers hurt small things and scratch heavy hulls.
  {.id = ComponentId::MassDriver, .mass = 5, .rangeUnits = 600, .damagePerSecond = 25, .modifierPercent = {70, 60, 25}, .cost = 60},

  // Reaches 400 against a mass driver's 600, which is Q10's answer expressed as two numbers: the
  // station kills a loiterer and not a besieger.
  // NO COST: it is not in a design anybody builds. A station arrives with its two mounts.
  // And point defense is the mass driver taken further: it shreds anything small and is irrelevant to anything large.
  {.id = ComponentId::PointDefense,
   .mass = 5,
   .rangeUnits = 400,
   .damagePerSecond = 60,
   .modifierPercent = {120, 90, 30},
   .stationSlotsOnly = true},

  // Hundredths, because the simulation is integers (R16). x1.5 and x2.0 on the station's build
  // rate; +25% and +50% on a delivered cargo.
  {.id = ComponentId::ShipyardL1, .cost = 400, .multiplierPercent = 150, .effect = ModuleEffect::BuildRate},
  {.id = ComponentId::ShipyardL2, .cost = 700, .multiplierPercent = 200, .effect = ModuleEffect::BuildRate},
  {.id = ComponentId::OreProcessorL1, .cost = 350, .multiplierPercent = 125, .effect = ModuleEffect::CargoValue},
  {.id = ComponentId::OreProcessorL2, .cost = 600, .multiplierPercent = 150, .effect = ModuleEffect::CargoValue},
}};

// AN IDENTITY IS AN INDEX, and these are what make that true rather than hoped for. A table that
// grew a row without the enumerator growing with it would compile and then resolve every identity
// past the new one to the wrong entry; this refuses to compile instead.
static_assert(HULLS.size() == static_cast<std::size_t>(HullId::ModuleFrame) + 1);
static_assert(DRIVES.size() == static_cast<std::size_t>(DriveId::BurnDrive) + 1);
static_assert(COMPONENTS.size() == static_cast<std::size_t>(ComponentId::OreProcessorL2) + 1);
} // namespace

std::span<const HullEntry> Hulls() noexcept
{
  return HULLS;
}

std::span<const DriveEntry> Drives() noexcept
{
  return DRIVES;
}

std::span<const ComponentEntry> Components() noexcept
{
  return COMPONENTS;
}

const HullEntry& Hull(HullId _id) noexcept
{
  return HULLS[static_cast<std::size_t>(_id)];
}

const DriveEntry& Drive(DriveId _id) noexcept
{
  return DRIVES[static_cast<std::size_t>(_id)];
}

const ComponentEntry& Component(ComponentId _id) noexcept
{
  return COMPONENTS[static_cast<std::size_t>(_id)];
}

} // namespace Outpost

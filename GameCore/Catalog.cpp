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
  {.id = HullId::Scout, .slotCount = 1, .hullPoints = 450, .sizeClass = SizeClass::Light, .hitValue = 0},
  {.id = HullId::Frigate, .slotCount = 2, .hullPoints = 600, .sizeClass = SizeClass::Medium, .hitValue = 0},
  {.id = HullId::Cruiser, .slotCount = 4, .hullPoints = 3000, .sizeClass = SizeClass::Heavy, .hitValue = 0},
  // The two base structures, and the only two rows with a hit value. A dash in the design's table
  // is this zero, and it means the hull is damaged through section 7's size-class table instead.
  {.id = HullId::Station, .slotCount = 2, .hullPoints = 8000, .sizeClass = SizeClass::Heavy, .hitValue = 300},
  {.id = HullId::ModuleFrame, .slotCount = 1, .hullPoints = 1500, .sizeClass = SizeClass::Heavy, .hitValue = 300},
}};

/// **`None` IS A ROW.** A hull with no drive does not move, and making absence an identity rather
/// than a null keeps the derivation total: it sums over a drive like any other, and the station's
/// is the one that contributes nothing.
constexpr std::array<DriveEntry, 3> DRIVES{{
  {.id = DriveId::None},
  {.id = DriveId::IonDrive},
  {.id = DriveId::BurnDrive},
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
  {.id = ComponentId::MiningLaser, .rangeUnits = 200, .damagePerSecond = 0, .orePerSecond = 20, .oreCapacity = 100},
  {.id = ComponentId::MassDriver, .rangeUnits = 600, .damagePerSecond = 25},

  // Reaches 400 against a mass driver's 600, which is Q10's answer expressed as two numbers: the
  // station kills a loiterer and not a besieger.
  {.id = ComponentId::PointDefense, .rangeUnits = 400, .damagePerSecond = 60, .stationSlotsOnly = true},

  // Hundredths, because the simulation is integers (R16). x1.5 and x2.0 on the station's build
  // rate; +25% and +50% on a delivered cargo.
  {.id = ComponentId::ShipyardL1, .cost = 400, .multiplierPercent = 150},
  {.id = ComponentId::ShipyardL2, .cost = 700, .multiplierPercent = 200},
  {.id = ComponentId::OreProcessorL1, .cost = 350, .multiplierPercent = 125},
  {.id = ComponentId::OreProcessorL2, .cost = 600, .multiplierPercent = 150},
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

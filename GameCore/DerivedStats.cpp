// A shared-items translation unit -- see NeuronCore.cpp for why it carries no precompiled header.

#include "DerivedStats.h"

namespace Outpost
{

DerivedStats Derive(HullId _hull, DriveId _drive, const std::array<ComponentId, MAX_COMPONENT_SLOTS>& _slots) noexcept
{
  const HullEntry& hull = Hull(_hull);
  const DriveEntry& drive = Drive(_drive);

  DerivedStats stats;
  stats.hullPoints = hull.hullPoints;

  // MASS IS THE HULL PLUS ITS CONTENTS, and the drive is contents like anything else. `None`
  // contributes zero, which is what makes this a sum rather than a branch on whether there is one.
  stats.mass = static_cast<std::uint32_t>(hull.mass) + drive.mass;
  stats.cost = static_cast<std::uint32_t>(hull.cost) + drive.cost;

  // ONLY THE SLOTS THE HULL ACTUALLY HAS. A design's array is sized for the largest hull in the
  // catalog, so a Scout's second, third and fourth entries are not its business even if something
  // put a component in one -- reading them would let a malformed design carry weight it cannot
  // mount.
  const std::size_t slotCount = (hull.slotCount < MAX_COMPONENT_SLOTS) ? hull.slotCount : MAX_COMPONENT_SLOTS;
  for (std::size_t slot = 0; slot < slotCount; ++slot)
  {
    const ComponentEntry& component = Component(_slots[slot]);
    stats.mass += component.mass;
    stats.cost += component.cost;
    stats.orePerSecond += component.orePerSecond;
    stats.oreCapacity += component.oreCapacity;
    stats.damagePerSecond += component.damagePerSecond;
  }

  // ZERO BEFORE THE DIVISION AND NOT FROM IT. A hull with no drive has no speed; a mass of zero --
  // which is what the two base structures carry -- must not reach the divide at all. Either
  // condition alone would be enough today and both are here because either one changing later
  // would otherwise be a crash rather than a wrong number.
  if ((drive.thrust == 0) || (stats.mass == 0))
  {
    stats.speedUnitsPerSecond = 0;
    return stats;
  }

  // SPEED IS THRUST OVER MASS, and the truncation is C++'s rather than a rule invented here. Q46's
  // figures make both shipped designs divide exactly, and the suite pins that -- a later catalog
  // change could quietly make it false.
  stats.speedUnitsPerSecond = static_cast<std::uint32_t>(drive.thrust) / stats.mass;
  stats.turnAnglePerSecond = (TURN_GAIN * static_cast<std::uint32_t>(drive.thrust)) / stats.mass;
  return stats;
}

DerivedStats Derive(const DesignEntry& _design) noexcept
{
  return Derive(_design.hull, _design.drive, _design.slots);
}

DerivedStats Derive(DesignId _id) noexcept
{
  return Derive(Design(_id));
}

} // namespace Outpost

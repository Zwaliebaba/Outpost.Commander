#pragma once

#include "World.h"

#include <cstddef>
#include <cstdint>
#include <span>

namespace Outpost
{

/// Q19: **how fifty ships occupy one point.** A ring slot per ship, assigned at order time.
///
/// **THE COST IS INDUCED BY [`ADR-001`](../Design/ADR/ADR-001-the-playfield-is-a-plane.md)** and was
/// named once and owned by nobody: in a volume, ships ordered to one point miss each other in the third
/// dimension; on a plane they stack. Something has to space them and this is it.
///
/// **NO SEPARATION FORCE AND NO FLOCKING.** Those are floating-point-shaped problems in an integer
/// simulation -- they want accumulating velocities, soft thresholds and a settling time, none of which
/// survives R16 without becoming a different thing. A formation system later is this same assignment
/// with a different slot layout, which is the whole reason the layout is a function of an index.

/// **HEXAGONAL, WHICH IS WHY RING `k` HOLDS `6k` SLOTS.** Slot 0 is the point itself; ring 1 is six
/// slots one spacing out, ring 2 is twelve slots two spacings out, and so on. That is the densest
/// packing of equal circles on a plane, so fifty ships take four rings and 3.5 spacings of radius rather
/// than sprawling.
///
/// The slot for an index, relative to the target. Total: every index has a slot.
[[nodiscard]] Neuron::Vec2 RingSlotOffset(std::size_t _index, Neuron::Fixed _spacing) noexcept;

/// Which ring an index falls in, and where in it. Exposed because the suite asserts the packing rather
/// than the positions -- a ring's slot count is the property, and where the first slot of a ring points
/// is not.
[[nodiscard]] std::size_t RingOfSlot(std::size_t _index) noexcept;

/// How far apart adjacent slots sit, for a selection.
///
/// **THE WIDEST HULL IN THE SELECTION, WHICH Q37 NOW STATES** (`GameCore/Catalog.h`). It was the second
/// of the three things the register said could not be written without that number. A mixed fleet spaces
/// to its largest member, which wastes room around the small ones and is the only rule that never
/// overlaps; spacing each ship to its own size would make a slot's position depend on who is in the
/// slots before it.
///
/// Zero for an empty selection, and never less than one world unit for a selection of things with no
/// size -- a spacing of zero would put every ship on the target.
[[nodiscard]] Neuron::Fixed RingSpacingFor(const World& _world, std::span<const EntityId> _selection) noexcept;

/// Orders a selection to a point, one ring slot each.
///
/// **SORTED NEAREST FIRST, TIES BROKEN ON IDENTITY** -- which is what makes this both sensible and
/// reproducible. The ship already closest to the target takes the center slot, so a fleet does not cross
/// itself to reach a formation; and two ships at *exactly* the same distance, which happens constantly
/// because positions are integers, are ordered by index and generation rather than by whatever order the
/// selection arrived in. **That tie is the whole determinism risk in this function**: distance alone is
/// not a total order, and a comparator that is not total makes `std::sort` free to return either
/// arrangement.
///
/// Returns how many were ordered. An identity that does not resolve is skipped rather than refused --
/// the intake has already validated ownership, and a ship that died between validation and here is
/// ordinary.
std::size_t OrderFleetTo(World& _world, std::span<const EntityId> _selection, const Neuron::Vec2& _target) noexcept;

/// Q67: how far past the target's own reach an attacking fleet stands.
inline constexpr std::int64_t STANDOFF_MARGIN_UNITS = 100;

/// Q67 as built: how far inside its own reach a fleet stands when it cannot stand outside the target's.
inline constexpr std::int64_t OWN_RANGE_MARGIN_UNITS = 50;

/// Q67 as built: the closest a slot comes, past the two hulls' keep-out.
inline constexpr std::int64_t KEEP_OUT_CLEARANCE_UNITS = 20;

/// Q67: how much further out each arc past the first sits.
inline constexpr std::int64_t OVERFLOW_ARC_STEP_UNITS = 90;

/// **AN ATTACK ORDER ON _target: A SLOT EACH ON A STANDOFF ARC** (M3.2, `OpenQuestions.md` Q67), the "different slot
/// layout" Q19 promised.
///
/// **THE RADIUS** is the target's longest weapon range plus 100, so the fleet stands just outside what can hit it.
/// **Unless that is past the fleet's own reach**: then it is the fleet's shortest range less 50, so every ship can
/// fire. It is never closer than the two hulls' keep-out plus 20. Against a station that is 580, just outside its
/// point defense's 480 (Q63); against a Fighter, 550.
///
/// **THE ARC** is a half circle facing the fleet, centred on the bearing from the target to the fleet's middle.
/// Slots sit a hull's width apart along it, the middle one first and then alternately either side. Ships are
/// assigned **nearest the target first, ties on identity**, as `OrderFleetTo` assigns them. What does not fit
/// on one arc goes to the next, 90 units further out.
///
/// **ONLY WHAT CAN FIGHT TAKES IT**: a design with a weapon and a drive. The rest of the selection is left as it
/// was. Each ship takes a move order in _group, an attack order, and leaves any mine order. Returns how many.
std::size_t OrderAttack(World& _world, std::span<const EntityId> _selection, EntityId _target, std::uint32_t _group);

} // namespace Outpost

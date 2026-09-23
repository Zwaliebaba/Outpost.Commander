#include "pch.h"

#include "Tick.h"

#include <cstdint>
#include <vector>

namespace Outpost
{

namespace
{
/// Symmetric rounding for a positive divisor, which C++ integer division does not give: `/` pulls
/// toward zero, so a plain divide is biased toward the origin for a negative numerator and away
/// from it for a positive one. A movement step that rounds differently depending on which way the
/// entity is travelling is a directional bias, and the tick would accumulate it forever.
[[nodiscard]] std::int64_t DivideRounded(std::int64_t _numerator, std::int64_t _divisor) noexcept
{
  const std::int64_t half = _divisor / 2;
  return (_numerator >= 0) ? ((_numerator + half) / _divisor) : ((_numerator - half) / _divisor);
}

/// A structure's keep-out circle, in whole world units (Q52). Units rather than `Fixed`, because the
/// blocking test multiplies two squared distances and at `Fixed` precision that is past 2^64.
struct Obstacle
{
  std::int64_t xUnits;
  std::int64_t yUnits;

  /// Half the structure's size. The mover's half is added per mover, since it is the mover's.
  std::int64_t halfSizeUnits;
};

[[nodiscard]] std::int64_t HalfSizeUnits(DesignId _design) noexcept
{
  return static_cast<std::int64_t>(Hull(Design(_design).hull).sizeUnits) / 2;
}

[[nodiscard]] std::int64_t Units(Neuron::Fixed _value) noexcept
{
  return static_cast<std::int64_t>(_value) >> Neuron::FIXED_FRACTION_BITS;
}

/// Every structure -- anything with no drive, anyone's -- in index order, once a tick.
[[nodiscard]] std::vector<Obstacle> Structures(const World& _world)
{
  std::vector<Obstacle> out;
  for (std::size_t slot = 0; slot < _world.SlotCount(); ++slot)
  {
    if (!_world.IsSlotAlive(slot))
    {
      continue;
    }
    const Entity& entity = _world.EntityInSlot(slot);
    if (Derive(entity.design).speedUnitsPerSecond == 0)
    {
      out.push_back(
        Obstacle{.xUnits = Units(entity.position.x), .yUnits = Units(entity.position.y), .halfSizeUnits = HalfSizeUnits(entity.design)});
    }
  }
  return out;
}

/// The bearing to fly at: straight at the destination, or past the nearest structure in the way.
///
/// **Q52, RECOMPUTED EVERY TICK AND STORED NOWHERE.** A structure blocks when the straight line from the
/// ship to its destination passes inside its steering circle, and neither the ship nor the destination
/// is inside its keep-out circle. The nearest along the line wins, and the first in index order wins a
/// tie. The ship then steers for the tangent on the side of the line the structure is not on.
///
/// **THE STEERING CIRCLE IS WIDER THAN THE KEEP-OUT** by half the mover again. A ship chasing a tangent
/// turns at a finite rate (Q51), so it cuts slightly inside the circle it steers by, and the margin is
/// what keeps that cut outside the circle it must not enter.
[[nodiscard]] Neuron::Angle DesiredBearing(const Entity& _entity, const Neuron::Vec2& _destination,
                                           const std::vector<Obstacle>& _structures) noexcept
{
  const std::int64_t fromX = Units(_entity.position.x);
  const std::int64_t fromY = Units(_entity.position.y);
  const std::int64_t pathX = Units(_destination.x) - fromX;
  const std::int64_t pathY = Units(_destination.y) - fromY;
  const std::int64_t pathSquared = (pathX * pathX) + (pathY * pathY);

  const Neuron::Vec2 delta = _destination - _entity.position;
  const Neuron::Angle straight = Neuron::BearingOf(delta.x, delta.y);
  if (pathSquared == 0)
  {
    return straight;
  }

  const std::int64_t moverHalf = HalfSizeUnits(_entity.design);
  const Obstacle* nearest = nullptr;
  std::int64_t nearestAlong = 0;
  std::int64_t nearestSteer = 0;
  for (const Obstacle& structure : _structures)
  {
    const std::int64_t keepOut = structure.halfSizeUnits + moverHalf;
    const std::int64_t steer = keepOut + moverHalf;
    const std::int64_t toX = structure.xUnits - fromX;
    const std::int64_t toY = structure.yUnits - fromY;
    const std::int64_t toSquared = (toX * toX) + (toY * toY);
    const std::int64_t endX = structure.xUnits - Units(_destination.x);
    const std::int64_t endY = structure.yUnits - Units(_destination.y);

    // Inside it already, or ordered into it: it is not in the way, it is where the ship is going.
    if ((toSquared <= (keepOut * keepOut)) || (((endX * endX) + (endY * endY)) <= (keepOut * keepOut)))
    {
      continue;
    }

    // Behind the ship, or past the destination: the line does not reach it.
    const std::int64_t along = (toX * pathX) + (toY * pathY);
    if ((along <= 0) || (along >= pathSquared))
    {
      continue;
    }

    // The squared distance from the line to the center, times the path's squared length, so there is
    // no division: |to|^2 - along^2 / |path|^2 < steer^2, multiplied through by |path|^2.
    if (((toSquared * pathSquared) - (along * along)) >= ((steer * steer) * pathSquared))
    {
      continue;
    }

    if ((nearest == nullptr) || (along < nearestAlong))
    {
      nearest = &structure;
      nearestAlong = along;
      nearestSteer = steer;
    }
  }

  if (nearest == nullptr)
  {
    return straight;
  }

  // THE TANGENT: the bearing to the center, turned by the angle whose sine is steer over the distance.
  // Past the guard the ship is outside the keep-out, but it may be inside the wider steering circle,
  // and then the tangent is the perpendicular.
  const std::int64_t toX = nearest->xUnits - fromX;
  const std::int64_t toY = nearest->yUnits - fromY;
  const std::int64_t toSquared = (toX * toX) + (toY * toY);
  const std::int64_t steerSquared = nearestSteer * nearestSteer;
  const Neuron::Angle offset =
    (toSquared > steerSquared) ? Neuron::BearingOf(Neuron::Sqrt(toSquared - steerSquared), nearestSteer) : Neuron::ANGLE_QUARTER_TURN;
  const Neuron::Angle center = Neuron::BearingOf(toX, toY);

  // THE SIDE: the structure's side of the line, by the sign of the cross product, and the ship passes
  // on the other. A structure dead on the line is passed on the right, a fixed rule so the tie cannot
  // depend on anything but the positions.
  const std::int64_t cross = (pathX * toY) - (pathY * toX);
  return (cross >= 0) ? static_cast<Neuron::Angle>(center - offset) : static_cast<Neuron::Angle>(center + offset);
}

/// Every entity with somewhere to be, in index order.
///
/// **Q51: A SHIP FLIES ALONG ITS HEADING AND STEERS.** Each tick the heading swings toward the bearing
/// it wants by at most the ship's turn rate, and the ship then moves along the new heading at its speed
/// times the cosine of what error is left. Past a quarter turn off, it does not move forward at all and
/// only turns. That throttle is what makes it impossible to orbit a point: any forward step has a
/// component toward the target, so the distance only ever falls.
void MoveEverything(World& _world)
{
  const std::vector<Obstacle> structures = Structures(_world);

  const std::size_t slotCount = _world.SlotCount();
  for (std::size_t slot = 0; slot < slotCount; ++slot)
  {
    if (!_world.IsSlotAlive(slot))
    {
      continue;
    }

    MoveOrder& order = _world.OrderInSlot(slot);
    if (!order.active)
    {
      continue;
    }

    Entity& entity = _world.EntityInSlot(slot);
    const Neuron::Vec2 delta = order.destination - entity.position;
    const std::int64_t distanceSquared = Neuron::LengthSquared(delta);
    const std::int64_t step = static_cast<std::int64_t>(order.speedPerTick);

    // ARRIVE RATHER THAN OVERSHOOT, and this comparison is the whole of it. A step taken without
    // it carries the entity past the destination, the next tick carries it back, and the thing
    // shivers either side of the point forever -- the classic fixed-point movement failure, and
    // the one M0.8 names. Compared squared so the common case needs no square root at all.
    if (distanceSquared <= (step * step))
    {
      entity.position = order.destination;
      order.active = false;
      continue;
    }

    // THE TURN, bounded by the rate. A rate of half a turn or more reaches any bearing at once.
    const Neuron::Angle wanted = DesiredBearing(entity, order.destination, structures);
    const std::int32_t error = Neuron::AngleDifference(entity.heading, wanted);
    const std::int32_t limit = static_cast<std::int32_t>(order.turnAnglePerTick);
    const std::int32_t swing = (error > limit) ? limit : ((error < -limit) ? -limit : error);
    entity.heading = static_cast<Neuron::Angle>(entity.heading + swing);

    // THE THROTTLE: the cosine of the error left after the turn, and nothing forward past a quarter.
    const std::int64_t remaining = Neuron::Cosine(static_cast<Neuron::Angle>(error - swing));
    if (remaining <= 0)
    {
      continue;
    }
    const std::int64_t forward = DivideRounded(step * remaining, Neuron::SINE_ONE);

    // ROUNDED, NOT TRUNCATED, AND THE DIFFERENCE IS WHETHER THE ENTITY EVER ARRIVES. The larger
    // component of a unit heading is at least 0.707 of it, so the dominant axis rounds to at least
    // one whenever the forward step is at least one. A step of zero still does not move, which is
    // what World::OrderMoveTo says a speed of zero means.
    const Neuron::Vec2 movement{.x = static_cast<Neuron::Fixed>(DivideRounded(forward * Neuron::Cosine(entity.heading), Neuron::SINE_ONE)),
                                .y = static_cast<Neuron::Fixed>(DivideRounded(forward * Neuron::Sine(entity.heading), Neuron::SINE_ONE))};

    // Rounding can carry the entity slightly past the destination. That is bounded and harmless:
    // the guard above catches it on the next tick and snaps, which is the difference between
    // overshooting once and oscillating.
    entity.position = entity.position + movement;
  }
}
} // namespace

void Tick(World& _world) noexcept
{
  MoveEverything(_world);
}

} // namespace Outpost

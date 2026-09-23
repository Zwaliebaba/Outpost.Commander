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

/// Q53: only what is this close is considered, in world units. Well past Q51's 45-unit turning radius
/// and the widest steering circle, so a swerve still starts in time, and inside one grid cell, so the
/// cells around the mover's own hold everything it has to look at.
inline constexpr std::int64_t LOOK_AHEAD_UNITS = 400;

/// `TechnicalDesign.md` section 2's uniform grid: 512-unit cells, 32 x 32 over the 16,384-unit square.
/// A candidate structure only -- which obstacle wins is decided by distance and identity, never by
/// which cell was visited first.
inline constexpr std::int64_t GRID_CELL_UNITS = 512;
inline constexpr std::int64_t GRID_CELLS_ACROSS = 32;
inline constexpr std::int64_t GRID_ORIGIN_UNITS = (GRID_CELL_UNITS * GRID_CELLS_ACROSS) / 2;

/// Q53: a moving ship within an eighth of a turn of the mover's heading is flying the same way, and is
/// part of its stream rather than in its way.
inline constexpr std::int32_t SAME_STREAM_ANGLE = 8192;

/// One entity as it stood at the start of the tick, in whole world units. Units rather than `Fixed`,
/// because the blocking test multiplies two squared distances and at `Fixed` precision that is past 2^64.
struct Occupant
{
  std::size_t slot;
  std::int64_t xUnits;
  std::int64_t yUnits;

  /// Half its size. The mover's half is added per mover, since it is the mover's.
  std::int64_t halfSizeUnits;
  Neuron::Angle heading;

  /// No drive: a station or a module (Q52). Always avoided, final approach or not.
  bool structure;

  /// Has an order it is carrying out, at a speed above zero.
  bool moving;

  /// The group of its latest order (Q53). A ship does not avoid another in its own group.
  std::uint32_t group;
};

/// **THE WORLD AS IT STOOD WHEN THE TICK BEGAN**, so which ship moves first in index order never changes
/// what the others see (Q53). Occupants are in index order, and so is each cell's list.
struct Snapshot
{
  std::vector<Occupant> occupants;
  std::vector<std::vector<std::uint32_t>> cells;
};

[[nodiscard]] std::int64_t HalfSizeUnits(DesignId _design) noexcept
{
  return static_cast<std::int64_t>(Hull(Design(_design).hull).sizeUnits) / 2;
}

[[nodiscard]] std::int64_t Units(Neuron::Fixed _value) noexcept
{
  return static_cast<std::int64_t>(_value) >> Neuron::FIXED_FRACTION_BITS;
}

/// A cell coordinate, clamped, so anything past the edge of the square lands in the edge cell rather
/// than outside the grid.
[[nodiscard]] std::int64_t CellOf(std::int64_t _units) noexcept
{
  const std::int64_t cell = (_units + GRID_ORIGIN_UNITS) / GRID_CELL_UNITS;
  return (_units + GRID_ORIGIN_UNITS < 0) ? 0 : ((cell >= GRID_CELLS_ACROSS) ? (GRID_CELLS_ACROSS - 1) : cell);
}

[[nodiscard]] Snapshot TakeSnapshot(const World& _world)
{
  Snapshot snapshot;
  snapshot.cells.resize(static_cast<std::size_t>(GRID_CELLS_ACROSS * GRID_CELLS_ACROSS));
  for (std::size_t slot = 0; slot < _world.SlotCount(); ++slot)
  {
    if (!_world.IsSlotAlive(slot))
    {
      continue;
    }
    const Entity& entity = _world.EntityInSlot(slot);
    const MoveOrder& order = _world.OrderInSlot(slot);
    const Occupant occupant{.slot = slot,
                            .xUnits = Units(entity.position.x),
                            .yUnits = Units(entity.position.y),
                            .halfSizeUnits = HalfSizeUnits(entity.design),
                            .heading = entity.heading,
                            .structure = Derive(entity.design).speedUnitsPerSecond == 0,
                            .moving = order.active && (order.speedPerTick > 0),
                            .group = order.group};

    const std::size_t cell = static_cast<std::size_t>((CellOf(occupant.yUnits) * GRID_CELLS_ACROSS) + CellOf(occupant.xUnits));
    snapshot.cells[cell].push_back(static_cast<std::uint32_t>(snapshot.occupants.size()));
    snapshot.occupants.push_back(occupant);
  }
  return snapshot;
}

/// The bearing to fly at: straight at the destination, or past the nearest thing in the way.
///
/// **Q52 AND Q53, RECOMPUTED EVERY TICK AND STORED NOWHERE.** Something blocks when the straight line
/// from the ship to its destination passes inside its steering circle, and neither the ship nor the
/// destination is inside its keep-out circle. Q53's exceptions apply to ships and never to
/// structures: a ship given the same order, a moving ship flying the same way, and anything past the
/// look-ahead. The nearest along the line wins, and the lower entity index breaks a tie. The ship then
/// steers for the tangent on the side of the line the obstacle is not on.
///
/// **THE STEERING CIRCLE IS WIDER THAN THE KEEP-OUT** by half the mover again. A ship chasing a tangent
/// turns at a finite rate (Q51), so it cuts slightly inside the circle it steers by, and the margin is
/// what keeps that cut outside the circle it must not enter.
[[nodiscard]] Neuron::Angle DesiredBearing(const Occupant& _mover, const Entity& _entity, const Neuron::Vec2& _destination,
                                           const Snapshot& _snapshot) noexcept
{
  const std::int64_t fromX = _mover.xUnits;
  const std::int64_t fromY = _mover.yUnits;
  const std::int64_t pathX = Units(_destination.x) - fromX;
  const std::int64_t pathY = Units(_destination.y) - fromY;
  const std::int64_t pathSquared = (pathX * pathX) + (pathY * pathY);

  const Neuron::Vec2 delta = _destination - _entity.position;
  const Neuron::Angle straight = Neuron::BearingOf(delta.x, delta.y);
  if (pathSquared == 0)
  {
    return straight;
  }

  const std::int64_t moverHalf = _mover.halfSizeUnits;

  const Occupant* nearest = nullptr;
  std::int64_t nearestAlong = 0;
  std::int64_t nearestSteer = 0;

  const std::int64_t cellX = CellOf(fromX);
  const std::int64_t cellY = CellOf(fromY);
  for (std::int64_t y = cellY - 1; y <= cellY + 1; ++y)
  {
    for (std::int64_t x = cellX - 1; x <= cellX + 1; ++x)
    {
      if ((x < 0) || (y < 0) || (x >= GRID_CELLS_ACROSS) || (y >= GRID_CELLS_ACROSS))
      {
        continue;
      }

      for (const std::uint32_t index : _snapshot.cells[static_cast<std::size_t>((y * GRID_CELLS_ACROSS) + x)])
      {
        const Occupant& other = _snapshot.occupants[index];
        if (other.slot == _mover.slot)
        {
          continue;
        }

        const std::int64_t toX = other.xUnits - fromX;
        const std::int64_t toY = other.yUnits - fromY;
        const std::int64_t toSquared = (toX * toX) + (toY * toY);
        if (toSquared > (LOOK_AHEAD_UNITS * LOOK_AHEAD_UNITS))
        {
          continue;
        }

        if (!other.structure)
        {
          if ((other.group != NO_ORDER_GROUP) && (other.group == _mover.group))
          {
            continue;
          }
          const std::int32_t apart = Neuron::AngleDifference(_mover.heading, other.heading);
          if (other.moving && (apart >= -SAME_STREAM_ANGLE) && (apart <= SAME_STREAM_ANGLE))
          {
            continue;
          }
        }

        const std::int64_t keepOut = other.halfSizeUnits + moverHalf;
        const std::int64_t steer = keepOut + moverHalf;
        const std::int64_t endX = other.xUnits - Units(_destination.x);
        const std::int64_t endY = other.yUnits - Units(_destination.y);

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

        // The squared distance from the line to the center, times the path's squared length, so there
        // is no division: |to|^2 - along^2 / |path|^2 < steer^2, multiplied through by |path|^2.
        if (((toSquared * pathSquared) - (along * along)) >= ((steer * steer) * pathSquared))
        {
          continue;
        }

        if ((nearest == nullptr) || (along < nearestAlong) || ((along == nearestAlong) && (other.slot < nearest->slot)))
        {
          nearest = &other;
          nearestAlong = along;
          nearestSteer = steer;
        }
      }
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

  // THE SIDE: the obstacle's side of the line, by the sign of the cross product, and the ship passes on
  // the other. An obstacle dead on the line is passed on the right, a fixed rule so the tie cannot
  // depend on anything but the positions -- and two ships meeting head on both keep right.
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
  const Snapshot snapshot = TakeSnapshot(_world);
  std::size_t next = 0;

  const std::size_t slotCount = _world.SlotCount();
  for (std::size_t slot = 0; slot < slotCount; ++slot)
  {
    if (!_world.IsSlotAlive(slot))
    {
      continue;
    }

    // The snapshot holds every live slot in index order, so this walks it in step with the loop.
    const Occupant& mover = snapshot.occupants[next];
    ++next;

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
    const Neuron::Angle wanted = DesiredBearing(mover, entity, order.destination, snapshot);
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

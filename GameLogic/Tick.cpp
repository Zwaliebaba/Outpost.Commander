#include "pch.h"

#include "Tick.h"

#include <cstdint>

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

/// Every entity with somewhere to be, in index order.
void MoveEverything(World& _world) noexcept
{
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

    // Past the guard, distanceSquared exceeds step*step and is therefore at least one, so the
    // floor of its root is at least one and the divisions below cannot divide by zero.
    const std::int64_t distance = Neuron::Sqrt(distanceSquared);

    // ROUNDED, NOT TRUNCATED, AND THE DIFFERENCE IS WHETHER THE ENTITY EVER ARRIVES. Truncating
    // stalls: at a delta of (3, 3) with a step of 1 the floored distance is 4, each component
    // truncates 3/4 to zero, nothing moves, and it never moves again. Rounding always advances,
    // because the larger component of a delta is at least 0.707 of the distance -- and the floored
    // distance only makes that ratio larger -- so the dominant axis rounds to at least one
    // whenever the step is at least one. A step of zero still does not move, which is what
    // World::OrderMoveTo says a speed of zero means.
    const Neuron::Vec2 movement{.x = static_cast<Neuron::Fixed>(DivideRounded(static_cast<std::int64_t>(delta.x) * step, distance)),
                                .y = static_cast<Neuron::Fixed>(DivideRounded(static_cast<std::int64_t>(delta.y) * step, distance))};

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

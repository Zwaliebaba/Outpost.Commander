#include "pch.h"

#include "RingAssignment.h"

#include "Tick.h"

#include <algorithm>
#include <vector>

namespace Outpost
{

namespace
{
/// Slots through ring `k` inclusive: 1 + 6(1 + 2 + ... + k), which is 1 + 3k(k+1).
[[nodiscard]] constexpr std::size_t SlotsThroughRing(std::size_t _ring) noexcept
{
  return 1 + (3 * _ring * (_ring + 1));
}

/// A ship, ready to be sorted into a slot.
struct Candidate
{
  EntityId id{};
  std::int64_t distanceSquared = 0;
};
} // namespace

std::size_t RingOfSlot(std::size_t _index) noexcept
{
  // A loop rather than a square root: the counts this runs over are fifty, and an integer root with a
  // rounding rule at the boundary is a second thing to get right for no measurable gain.
  std::size_t ring = 0;
  while (SlotsThroughRing(ring) <= _index)
  {
    ++ring;
  }
  return ring;
}

Neuron::Vec2 RingSlotOffset(std::size_t _index, Neuron::Fixed _spacing) noexcept
{
  if (_index == 0)
  {
    return Neuron::Vec2{};
  }

  const std::size_t ring = RingOfSlot(_index);
  const std::size_t positionsInRing = 6 * ring;
  const std::size_t position = _index - SlotsThroughRing(ring - 1);

  // The turn divided by the ring's slot count, in binary angle. Integer division, so a ring whose count
  // does not divide 65,536 has slots a fraction of a step apart -- which is deterministic, is under a
  // hundredth of a degree, and is what the alternative (a table of angles) would have cost a table for.
  const std::uint32_t step = 65536u / static_cast<std::uint32_t>(positionsInRing);
  const auto angle = static_cast<Neuron::Angle>(step * static_cast<std::uint32_t>(position));

  const std::int64_t radius = static_cast<std::int64_t>(ring) * _spacing;
  const std::int64_t x = (static_cast<std::int64_t>(Neuron::Cosine(angle)) * radius) / Neuron::SINE_ONE;
  const std::int64_t y = (static_cast<std::int64_t>(Neuron::Sine(angle)) * radius) / Neuron::SINE_ONE;

  return Neuron::Vec2{.x = static_cast<Neuron::Fixed>(x), .y = static_cast<Neuron::Fixed>(y)};
}

Neuron::Fixed RingSpacingFor(const World& _world, std::span<const EntityId> _selection) noexcept
{
  std::uint32_t widest = 0;
  bool any = false;
  for (const EntityId id : _selection)
  {
    const Entity* entity = _world.Find(id);
    if (entity == nullptr)
    {
      continue;
    }
    any = true;
    widest = std::max(widest, static_cast<std::uint32_t>(Hull(Design(entity->design).hull).sizeUnits));
  }

  if (!any)
  {
    return 0;
  }

  // A hull with no stated size would otherwise put the whole fleet on one point.
  const std::uint32_t units = (widest == 0) ? 1 : widest;
  return static_cast<Neuron::Fixed>(units * Neuron::FIXED_ONE);
}

std::size_t OrderFleetTo(World& _world, std::span<const EntityId> _selection, const Neuron::Vec2& _target) noexcept
{
  std::vector<Candidate> candidates;
  candidates.reserve(_selection.size());
  for (const EntityId id : _selection)
  {
    const Entity* entity = _world.Find(id);
    if (entity == nullptr)
    {
      continue;
    }
    candidates.push_back(Candidate{.id = id, .distanceSquared = Neuron::LengthSquared(entity->position - _target)});
  }

  // **A TOTAL ORDER, AND THE IDENTITY IS WHAT MAKES IT ONE.** Two ships at the same distance are
  // common rather than exotic -- positions are integers and a fleet ordered somewhere twice arrives
  // symmetric -- and `std::sort` is free to return either arrangement when a comparator says they are
  // equal. An index is unique among live slots, so this never falls through to the generation; the
  // generation is compared anyway, because "never" resting on an invariant elsewhere is how this class
  // of defect gets in.
  std::sort(candidates.begin(), candidates.end(),
            [](const Candidate& _a, const Candidate& _b) noexcept
            {
              if (_a.distanceSquared != _b.distanceSquared)
              {
                return _a.distanceSquared < _b.distanceSquared;
              }
              if (_a.id.index != _b.id.index)
              {
                return _a.id.index < _b.id.index;
              }
              return _a.id.generation < _b.id.generation;
            });

  const Neuron::Fixed spacing = RingSpacingFor(_world, _selection);

  // **THE RING STAYS WHOLE AGAINST THE WALL** (the 2026-09-23 review, m4). Clamping each slot on its own
  // folded every slot past the edge onto the edge line, so fifty fighters ordered into a corner took 32
  // distinct points and stacked -- the outcome rings exist to prevent. The target moves in by the outermost
  // ring's radius first, so every slot lands inside; the per-slot clamp below stays as the backstop.
  const std::int64_t outermost =
    candidates.empty() ? 0 : static_cast<std::int64_t>(RingOfSlot(candidates.size() - 1)) * static_cast<std::int64_t>(spacing);
  const std::int64_t reach = std::max<std::int64_t>(0, static_cast<std::int64_t>(PLAY_AREA_HALF_EXTENT) - outermost);
  const Neuron::Vec2 center{.x = static_cast<Neuron::Fixed>(std::clamp<std::int64_t>(_target.x, -reach, reach)),
                            .y = static_cast<Neuron::Fixed>(std::clamp<std::int64_t>(_target.y, -reach, reach))};

  // One group for everything this order moves (Q61): they share a ring and do not avoid each other.
  const std::uint32_t group = _world.NewOrderGroup();

  std::size_t ordered = 0;
  for (std::size_t slot = 0; slot < candidates.size(); ++slot)
  {
    const Entity* entity = _world.Find(candidates[slot].id);
    if (entity == nullptr)
    {
      continue;
    }

    const Neuron::Vec2 offset = RingSlotOffset(slot, spacing);
    const Neuron::Vec2 destination = ClampToPlayArea(Neuron::Vec2{.x = center.x + offset.x, .y = center.y + offset.y});

    // **R24, AT LAST.** The intake carried a constant with a comment saying this would be derived from
    // thrust over mass; M1.2 made the derivation and M1.3 put the design on the entity, so the number
    // is now the ship's own. A Miner moves 5 units a tick and a Fighter 7.
    if (_world.OrderMoveTo(candidates[slot].id, destination, SpeedPerTick(entity->design), TurnAnglePerTick(entity->design), group))
    {
      ++ordered;
    }
  }
  return ordered;
}

} // namespace Outpost

#include "pch.h"

#include "RingAssignment.h"

#include "Targeting.h"

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

std::size_t OrderAttack(World& _world, std::span<const EntityId> _selection, EntityId _target, std::uint32_t _group)
{
  const Entity* target = _world.Find(_target);
  if (target == nullptr)
  {
    return 0;
  }
  const Neuron::Vec2 targetPosition = target->position;
  const std::int64_t targetHalf = static_cast<std::int64_t>(Hull(Design(target->design).hull).sizeUnits) / 2;

  // WHAT CAN FIGHT, nearest the target first and ties on identity -- the order `OrderFleetTo` uses.
  std::vector<Candidate> fighters;
  std::int64_t shortestRange = 0;
  std::int64_t widestHalf = 0;
  std::int64_t sumX = 0;
  std::int64_t sumY = 0;
  for (const EntityId id : _selection)
  {
    const Entity* entity = _world.Find(id);
    if ((entity == nullptr) || !ReachOf(entity->design).armed || (Derive(entity->design).speedUnitsPerSecond == 0))
    {
      continue;
    }
    const std::int64_t range = ReachOf(entity->design).rangeUnits;
    shortestRange = fighters.empty() ? range : std::min(shortestRange, range);
    widestHalf = std::max<std::int64_t>(widestHalf, Hull(Design(entity->design).hull).sizeUnits / 2);
    sumX += entity->position.x;
    sumY += entity->position.y;
    fighters.push_back(Candidate{.id = id, .distanceSquared = Neuron::LengthSquared(entity->position - targetPosition)});
  }
  if (fighters.empty())
  {
    return 0;
  }
  std::sort(fighters.begin(), fighters.end(),
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

  // THE RADIUS, in whole units (the header says which of three it is).
  const std::int64_t outsideTheirs = static_cast<std::int64_t>(ReachOf(target->design).rangeUnits) + STANDOFF_MARGIN_UNITS;
  const std::int64_t insideOurs = shortestRange - OWN_RANGE_MARGIN_UNITS;
  const std::int64_t floor = targetHalf + widestHalf + KEEP_OUT_CLEARANCE_UNITS;
  const std::int64_t radius = std::max(std::min(outsideTheirs, insideOurs), floor);

  // THE ARC: centred on the bearing from the target to the fleet's middle, slots a hull's width apart. 10,430 is
  // binary-angle units a radian, rounded down, so the step is a touch under a hull's width and never over it.
  constexpr std::int64_t BINARY_ANGLE_PER_RADIAN = 10430;
  const std::int64_t count = static_cast<std::int64_t>(fighters.size());
  const Neuron::Vec2 middle{.x = static_cast<Neuron::Fixed>(sumX / count), .y = static_cast<Neuron::Fixed>(sumY / count)};
  const Neuron::Vec2 facing = middle - targetPosition;
  const Neuron::Angle centre = Neuron::BearingOf(facing.x, facing.y);
  const std::int64_t spacing = std::max<std::int64_t>(2 * widestHalf, 1);
  const std::int64_t step = std::max<std::int64_t>((spacing * BINARY_ANGLE_PER_RADIAN) / radius, 1);
  const std::int64_t perArc = 1 + (2 * (static_cast<std::int64_t>(Neuron::ANGLE_QUARTER_TURN) / step));

  std::size_t ordered = 0;
  for (std::size_t index = 0; index < fighters.size(); ++index)
  {
    const Entity* entity = _world.Find(fighters[index].id);
    const std::int64_t arc = static_cast<std::int64_t>(index) / perArc;
    const std::int64_t within = static_cast<std::int64_t>(index) % perArc;
    const std::int64_t side = ((within % 2) == 1) ? 1 : -1;
    const std::int64_t offset = ((within + 1) / 2) * side * step;
    const auto angle = static_cast<Neuron::Angle>(static_cast<std::int64_t>(centre) + offset);
    const std::int64_t distance = (radius + (arc * OVERFLOW_ARC_STEP_UNITS)) * Neuron::FIXED_ONE;

    const Neuron::Vec2 slot = ClampToPlayArea(Neuron::Vec2{
      .x = targetPosition.x + static_cast<Neuron::Fixed>((static_cast<std::int64_t>(Neuron::Cosine(angle)) * distance) / Neuron::SINE_ONE),
      .y = targetPosition.y + static_cast<Neuron::Fixed>((static_cast<std::int64_t>(Neuron::Sine(angle)) * distance) / Neuron::SINE_ONE)});

    if (_world.OrderMoveTo(fighters[index].id, slot, SpeedPerTick(entity->design), TurnAnglePerTick(entity->design), _group) &&
        _world.OrderAttack(fighters[index].id, _target, targetPosition))
    {
      static_cast<void>(_world.StopMining(fighters[index].id));
      ++ordered;
    }
  }
  return ordered;
}

} // namespace Outpost

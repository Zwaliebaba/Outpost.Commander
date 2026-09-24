#include "pch.h"

#include "Targeting.h"

#include <algorithm>

namespace Outpost
{

Reach ReachOf(DesignId _design) noexcept
{
  Reach reach{.rangeUnits = 0, .arcHalfAngle = 0, .armed = false};
  const DesignEntry& design = Design(_design);
  const std::size_t slotCount = std::min<std::size_t>(Hull(design.hull).slotCount, MAX_COMPONENT_SLOTS);
  for (std::size_t slot = 0; slot < slotCount; ++slot)
  {
    const ComponentEntry& component = Component(design.slots[slot]);
    if (component.damagePerSecond == 0)
    {
      continue;
    }
    reach.armed = true;
    reach.rangeUnits = std::max<std::uint32_t>(reach.rangeUnits, component.rangeUnits);
    reach.arcHalfAngle = std::max(reach.arcHalfAngle, component.arcHalfAngle);
  }
  return reach;
}

bool IsHostile(const Entity& _shooter, const Entity& _other) noexcept
{
  return (_other.owner != NO_PLAYER) && (_other.owner != _shooter.owner);
}

bool InRange(const Neuron::Vec2& _from, const Neuron::Vec2& _to, std::uint32_t _rangeUnits) noexcept
{
  const std::int64_t range = static_cast<std::int64_t>(_rangeUnits) * Neuron::FIXED_ONE;
  return UniformGrid::DistanceSquared(_from, _to) <= (range * range);
}

bool InArc(const Entity& _shooter, const Neuron::Vec2& _target, std::uint16_t _arcHalfAngle) noexcept
{
  const Neuron::Vec2 toward = _target - _shooter.position;
  const std::int32_t off = Neuron::AngleDifference(_shooter.heading, Neuron::BearingOf(toward.x, toward.y));
  return ((off < 0) ? -off : off) <= static_cast<std::int32_t>(_arcHalfAngle);
}

EntityId SelectTarget(const World& _world, const UniformGrid& _grid, const Entity& _shooter, bool _requireArc,
                      std::vector<EntityId>& _scratch)
{
  const Reach reach = ReachOf(_shooter.design);
  if (!reach.armed)
  {
    return NO_ENTITY;
  }
  const Neuron::Fixed radius = static_cast<Neuron::Fixed>(reach.rangeUnits * static_cast<std::uint32_t>(Neuron::FIXED_ONE));
  return _grid.Nearest(
    _world, _shooter.position, radius, [&_shooter, &reach, _requireArc](const Entity& _other) noexcept
    { return IsHostile(_shooter, _other) && (!_requireArc || InArc(_shooter, _other.position, reach.arcHalfAngle)); }, _scratch);
}

} // namespace Outpost

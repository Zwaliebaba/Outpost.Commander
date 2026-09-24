#include "pch.h"

#include "ModuleEffects.h"

#include <vector>

namespace Outpost
{

namespace
{
/// The largest multiplier among this player's modules whose component has _effect, or 100. A linear scan over the
/// store, as `BuildSystem`'s station lookup is -- at 110 entities that is nothing, and it is in slot order, though
/// a maximum does not care about order (R16).
[[nodiscard]] std::uint32_t BestMultiplier(const World& _world, PlayerId _player, ModuleEffect _effect) noexcept
{
  std::uint32_t best = 100;
  const std::size_t slotCount = _world.SlotCount();
  for (std::size_t slot = 0; slot < slotCount; ++slot)
  {
    if (!_world.IsSlotAlive(slot))
    {
      continue;
    }
    const Entity& entity = _world.EntityInSlot(slot);
    if ((entity.owner != _player) || !IsModule(entity.design))
    {
      continue;
    }
    for (const ComponentId component : Design(entity.design).slots)
    {
      const ComponentEntry& entry = Component(component);
      if ((entry.effect == _effect) && (entry.multiplierPercent > best))
      {
        best = entry.multiplierPercent;
      }
    }
  }
  return best;
}
} // namespace

std::uint32_t BuildRateMultiplierPercent(const World& _world, PlayerId _player) noexcept
{
  return BestMultiplier(_world, _player, ModuleEffect::BuildRate);
}

std::uint32_t CargoValuePercent(const World& _world, PlayerId _player) noexcept
{
  return BestMultiplier(_world, _player, ModuleEffect::CargoValue);
}

bool HasShipyard(const World& _world, PlayerId _player) noexcept
{
  // A shipyard multiplies the build rate above the station's own hundred, which is what having one means.
  return BestMultiplier(_world, _player, ModuleEffect::BuildRate) > 100;
}

std::uint8_t ShipyardLevel(const World& _world, PlayerId _player)
{
  std::vector<DesignId> modules;
  for (std::size_t slot = 0; slot < _world.SlotCount(); ++slot)
  {
    if (_world.IsSlotAlive(slot) && (_world.EntityInSlot(slot).owner == _player) && IsModule(_world.EntityInSlot(slot).design))
    {
      modules.push_back(_world.EntityInSlot(slot).design);
    }
  }
  return ShipyardLevelOf(modules);
}

bool HasResearchStation(const World& _world, PlayerId _player) noexcept
{
  for (std::size_t slot = 0; slot < _world.SlotCount(); ++slot)
  {
    if (!_world.IsSlotAlive(slot) || (_world.EntityInSlot(slot).owner != _player) || !IsModule(_world.EntityInSlot(slot).design))
    {
      continue;
    }
    for (const ComponentId component : Design(_world.EntityInSlot(slot).design).slots)
    {
      if (Component(component).effect == ModuleEffect::Research)
      {
        return true;
      }
    }
  }
  return false;
}

} // namespace Outpost

#include "pch.h"

#include "DeathSystem.h"

namespace Outpost
{

void DeathSystem::Advance(World& _world)
{
  m_died.clear();

  // COLLECTED FIRST AND DESTROYED AFTER: `Destroy` frees the slot, and nothing below may depend on whether a slot
  // earlier in the sweep has already gone.
  for (std::size_t slot = 0; slot < _world.SlotCount(); ++slot)
  {
    if (!_world.IsSlotAlive(slot))
    {
      continue;
    }
    const Entity& entity = _world.EntityInSlot(slot);
    if (entity.hullRemaining == 0)
    {
      m_died.push_back(Death{.id = entity.id, .owner = entity.owner, .design = entity.design});
    }
  }

  for (const Death& death : m_died)
  {
    static_cast<void>(_world.Destroy(death.id));
  }
}

} // namespace Outpost

#include "pch.h"

#include "WeaponSystem.h"

#include "RingAssignment.h"
#include "Targeting.h"
#include "Tick.h"

#include <algorithm>

namespace Outpost
{

void WeaponSystem::ResolveAttacks(World& _world)
{
  const std::size_t slotCount = _world.SlotCount();

  // **ONE ARC PER ORDER**: the ships a single attack order moved share its group (Q61) and its target, so they
  // are solved again together, in the order their first member sits in the store.
  std::vector<std::pair<std::uint32_t, EntityId>> solved;
  std::vector<EntityId> members;
  for (std::size_t slot = 0; slot < slotCount; ++slot)
  {
    if (!_world.IsSlotAlive(slot))
    {
      continue;
    }
    AttackOrder& attack = _world.AttackInSlot(slot);
    if (!attack.active)
    {
      continue;
    }

    // A TARGET THAT IS GONE ENDS THE ORDER. The ship keeps whatever move it was making and falls back to
    // engaging on its own. Deaths are M3.4's, and this is what an attack order does when one happens.
    const Entity* target = _world.Find(attack.target);
    if (target == nullptr)
    {
      attack.active = false;
      continue;
    }

    const std::uint32_t group = _world.OrderInSlot(slot).group;
    const std::pair<std::uint32_t, EntityId> key{group, attack.target};
    if (std::find(solved.begin(), solved.end(), key) != solved.end())
    {
      continue;
    }
    solved.push_back(key);

    members.clear();
    for (std::size_t other = slot; other < slotCount; ++other)
    {
      if (_world.IsSlotAlive(other) && _world.AttackInSlot(other).active && (_world.AttackInSlot(other).target == attack.target) &&
          (_world.OrderInSlot(other).group == group))
      {
        members.push_back(_world.EntityInSlot(other).id);
      }
    }

    const std::int64_t spacing = static_cast<std::int64_t>(RingSpacingFor(_world, members));
    if (UniformGrid::DistanceSquared(target->position, attack.solvedAt) > (spacing * spacing))
    {
      static_cast<void>(OrderAttack(_world, members, attack.target, group));
    }
  }
}

void WeaponSystem::Advance(World& _world, std::uint32_t _tick)
{
  m_fired.clear();
  ResolveAttacks(_world);
  m_grid.Rebuild(_world);

  const std::size_t slotCount = _world.SlotCount();
  m_damage.assign(slotCount, 0);
  if (m_lastFireEvent.size() < slotCount)
  {
    m_lastFireEvent.resize(slotCount, 0);
  }

  for (std::size_t slot = 0; slot < slotCount; ++slot)
  {
    if (!_world.IsSlotAlive(slot))
    {
      continue;
    }
    Entity& shooter = _world.EntityInSlot(slot);
    const Reach reach = ReachOf(shooter.design);
    if (!reach.armed)
    {
      continue;
    }

    const MoveOrder& move = _world.OrderInSlot(slot);
    const AttackOrder& attack = _world.AttackInSlot(slot);
    const bool moving = move.active && (move.speedPerTick > 0);

    // WHAT IT SHOOTS AT, by its orders (the table in the header).
    EntityId target = NO_ENTITY;
    bool bear = false;
    if (attack.active)
    {
      const Entity* ordered = _world.Find(attack.target);
      if ((ordered != nullptr) && !moving)
      {
        target = attack.target;
        bear = true;
      }
      else if ((ordered != nullptr) && InRange(shooter.position, ordered->position, reach.rangeUnits) &&
               InArc(shooter, ordered->position, reach.arcHalfAngle))
      {
        target = attack.target;
      }
    }
    if (!target.IsValid())
    {
      target = SelectTarget(_world, m_grid, shooter, moving, m_scratch);
      bear = !moving;
    }
    const Entity* victim = _world.Find(target);
    if (victim == nullptr)
    {
      continue;
    }

    // TURNING TO BEAR (Q68): at the ship's own rate, and only a ship that is not flying somewhere. A station
    // has no drive, so its rate is zero and it never turns; its point defense reaches all around instead.
    if (bear)
    {
      const Neuron::Vec2 toward = victim->position - shooter.position;
      const std::int32_t error = Neuron::AngleDifference(shooter.heading, Neuron::BearingOf(toward.x, toward.y));
      const std::int32_t limit = static_cast<std::int32_t>(TurnAnglePerTick(shooter.design));
      const std::int32_t swing = (error > limit) ? limit : ((error < -limit) ? -limit : error);
      shooter.heading = static_cast<Neuron::Angle>(shooter.heading + swing);
    }

    // EVERY MOUNT THAT REACHES IT FIRES (ADR-004), settling its own remainder (ADR-014). A new target resets the
    // remainders, so a fraction built on one ship never lands on another.
    WeaponState& state = _world.WeaponsInSlot(slot);
    const DesignEntry& design = Design(shooter.design);
    const std::size_t mounts = std::min<std::size_t>(Hull(design.hull).slotCount, MAX_COMPONENT_SLOTS);
    bool fired = false;
    ComponentId firedWith = ComponentId::None;
    for (std::size_t mount = 0; mount < mounts; ++mount)
    {
      const ComponentEntry& weapon = Component(design.slots[mount]);
      if ((weapon.damagePerSecond == 0) || !InRange(shooter.position, victim->position, weapon.rangeUnits) ||
          !InArc(shooter, victim->position, weapon.arcHalfAngle))
      {
        continue;
      }
      if (state.target != target)
      {
        state.target = target;
        state.remainders.fill(0);
      }
      m_damage[target.index] += Accumulate(state.remainders[mount], DamagePerInterval(weapon.id, victim->design));
      fired = true;
      firedWith = weapon.id;
    }

    if (fired && ((m_lastFireEvent[slot] == 0) || ((_tick + 1) - m_lastFireEvent[slot] >= FIRE_EVENT_INTERVAL_TICKS)))
    {
      m_lastFireEvent[slot] = _tick + 1;
      m_fired.push_back(FireEvent{.shooter = PackIdentity(shooter.id.index, shooter.id.generation),
                                  .target = PackIdentity(target.index, target.generation),
                                  .weapon = static_cast<std::uint8_t>(firedWith)});
    }
  }

  // THE DAMAGE, AFTER EVERY WEAPON HAS FIRED (ADR-014). Stopping at zero: what a hull at zero means is M3.4's.
  for (std::size_t slot = 0; slot < slotCount; ++slot)
  {
    if ((m_damage[slot] == 0) || !_world.IsSlotAlive(slot))
    {
      continue;
    }
    Entity& hit = _world.EntityInSlot(slot);
    hit.hullRemaining = (hit.hullRemaining > m_damage[slot]) ? static_cast<std::uint16_t>(hit.hullRemaining - m_damage[slot]) : 0;
  }
}

} // namespace Outpost

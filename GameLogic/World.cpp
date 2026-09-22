#include "pch.h"

#include "World.h"

#include <limits>

namespace Outpost
{

namespace
{
/// An index has to fit the identity's sixteen bits, so this is where the store stops growing.
/// The design's 110 entities, and 220 at the doubled count, are three orders of magnitude short.
inline constexpr std::size_t MAX_SLOTS = std::numeric_limits<std::uint16_t>::max() + std::size_t{1};
} // namespace

EntityId World::Create(const Neuron::Vec2& _position, Neuron::Angle _heading, DesignId _design, PlayerId _owner) noexcept
{
  std::uint16_t index = 0;

  if (!m_freeIndices.empty())
  {
    // Back, always. The class comment says why this end is not a preference.
    index = m_freeIndices.back();
    m_freeIndices.pop_back();
  }
  else
  {
    if (m_slots.size() >= MAX_SLOTS)
    {
      return NO_ENTITY;
    }
    index = static_cast<std::uint16_t>(m_slots.size());
    m_slots.push_back(Slot{});
  }

  Slot& slot = m_slots[index];

  // The generation already advanced when the slot was destroyed, so a stale identity stopped
  // resolving at that moment rather than at this one. A slot that has never been used is at zero
  // and steps to one here, which is what makes generation zero mean "no entity".
  std::uint16_t generation = slot.entity.id.generation;
  if (generation == 0)
  {
    generation = 1;
  }

  // THE ONE PLACE THE DESIGN BECOMES A HULL AND A HULL-POINT TOTAL. Both are derived here, so the
  // redundancy `Entity` carries cannot drift -- there is no other writer (M1.3, ADR-006).
  const DesignEntry& design = Design(_design);
  const DerivedStats stats = Derive(design);

  slot.entity = Entity{.id = EntityId{.index = index, .generation = generation},
                       .position = _position,
                       .heading = _heading,
                       .hull = design.hull,
                       .design = _design,
                       .hullRemaining = static_cast<std::uint16_t>(stats.hullPoints),
                       .owner = _owner};
  slot.order = MoveOrder{};
  slot.alive = true;
  ++m_aliveCount;

  return slot.entity.id;
}

bool World::Destroy(EntityId _id) noexcept
{
  const Slot* found = ResolveSlot(_id);
  if (found == nullptr)
  {
    return false;
  }

  Slot& slot = m_slots[_id.index];
  slot.alive = false;
  slot.order = MoveOrder{};

  // Advance now, so every copy of this identity anywhere in the match goes stale at the instant
  // the entity dies rather than at the instant the slot is reused. Zero is skipped because it is
  // the reserved "no entity" generation; the wrap is after 65,535 reuses of one slot, at which
  // point an identity that old is long gone.
  const std::uint16_t next = static_cast<std::uint16_t>(slot.entity.id.generation + 1);
  slot.entity.id.generation = (next == 0) ? 1 : next;

  m_freeIndices.push_back(_id.index);
  --m_aliveCount;
  return true;
}

bool World::IsAlive(EntityId _id) const noexcept
{
  return ResolveSlot(_id) != nullptr;
}

Entity* World::Find(EntityId _id) noexcept
{
  const Slot* found = ResolveSlot(_id);
  return (found == nullptr) ? nullptr : &m_slots[_id.index].entity;
}

const Entity* World::Find(EntityId _id) const noexcept
{
  const Slot* found = ResolveSlot(_id);
  return (found == nullptr) ? nullptr : &found->entity;
}

bool World::OrderMoveTo(EntityId _id, const Neuron::Vec2& _destination, Neuron::Fixed _speedPerTick) noexcept
{
  if (ResolveSlot(_id) == nullptr)
  {
    return false;
  }

  m_slots[_id.index].order = MoveOrder{.destination = _destination, .speedPerTick = _speedPerTick, .active = true};
  return true;
}

const MoveOrder* World::FindOrder(EntityId _id) const noexcept
{
  const Slot* found = ResolveSlot(_id);
  return (found == nullptr) ? nullptr : &found->order;
}

std::size_t World::OwnedCount(PlayerId _owner) const noexcept
{
  std::size_t owned = 0;
  for (const Slot& slot : m_slots)
  {
    if (slot.alive && (slot.entity.owner == _owner))
    {
      ++owned;
    }
  }
  return owned;
}

bool World::IsSlotAlive(std::size_t _slot) const noexcept
{
  return (_slot < m_slots.size()) && m_slots[_slot].alive;
}

Entity& World::EntityInSlot(std::size_t _slot) noexcept
{
  return m_slots[_slot].entity;
}

const Entity& World::EntityInSlot(std::size_t _slot) const noexcept
{
  return m_slots[_slot].entity;
}

MoveOrder& World::OrderInSlot(std::size_t _slot) noexcept
{
  return m_slots[_slot].order;
}

const MoveOrder& World::OrderInSlot(std::size_t _slot) const noexcept
{
  return m_slots[_slot].order;
}

const World::Slot* World::ResolveSlot(EntityId _id) const noexcept
{
  if (!_id.IsValid() || (_id.index >= m_slots.size()))
  {
    return nullptr;
  }

  const Slot& slot = m_slots[_id.index];

  // Both halves, and the generation is the half that matters: an index alone would happily
  // resolve to whoever moved into the slot after the entity this identity names died.
  if (!slot.alive || (slot.entity.id.generation != _id.generation))
  {
    return nullptr;
  }
  return &slot;
}

} // namespace Outpost

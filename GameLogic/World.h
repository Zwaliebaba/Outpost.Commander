#pragma once

#include "GameCore.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Outpost
{

/// Where an entity has been told to go, and how fast it gets there. HOST-ONLY, which is why it is
/// here and not on `GameCore`'s Entity: the client is never sent an order, it is sent the position
/// the order produced (R19). R24 will derive the speed from thrust over mass once there is a
/// composition to derive it from; at M0 it is given.
///
/// R8: a public aggregate.
struct MoveOrder
{
  Neuron::Vec2 destination{};

  /// Units per tick, and the unit is in the name because R6 says a simulation measured in ticks
  /// makes unit ambiguity a real defect class.
  Neuron::Fixed speedPerTick = 0;

  bool active = false;
};

/// The entity store: ADR-002's `std::vector` with a free list, identified by index and generation,
/// iterated in index order by the tick.
///
/// THE FREE LIST IS A DETERMINISM PROPERTY, NOT A PERFORMANCE ONE, and this is the blind spot no
/// sweep can see. The tick iterates in index order, so **the order in which recycled indices are
/// handed back out decides the iteration order of everything downstream of a death**. Two hosts
/// that freed the same slots in a different sequence would get different indices for the same
/// ships and diverge from there, while any tool looking at this file sees an ordinary
/// `std::vector`. So: freed indices are pushed on the back and taken from the back, always, and
/// the only input to that order is the order things were destroyed in -- which the tick fixes.
/// Nothing here consults a hash, an address, or a container's internal arrangement.
class World
{
public:
  /// A new entity in the lowest-numbered free slot, or a fresh slot when none is free. NO_ENTITY
  /// when the store is full -- 65,536 slots, which the design's 110 is nowhere near.
  /// _owner defaults to nobody, which is what a neutral thing or a test fixture is. Players are
  /// numbered from one (`GameCore/Entity.h`).
  ///
  /// **IT TAKES A DESIGN, NOT A HULL** (M1.3, R24). The hull and the starting hull points are
  /// DERIVED from it here and nowhere else, which is what makes the two fields on `Entity` that
  /// could disagree unable to: this is the only writer of either.
  [[nodiscard]] EntityId Create(const Neuron::Vec2& _position, Neuron::Angle _heading, DesignId _design,
                                PlayerId _owner = NO_PLAYER) noexcept;

  /// False on an identity that is already stale, which is not an error: a thing killed twice in
  /// one tick by two systems is ordinary, and the second call simply finds it gone.
  bool Destroy(EntityId _id) noexcept;

  [[nodiscard]] bool IsAlive(EntityId _id) const noexcept;

  /// The record, or nullptr when the identity is stale. THE GENERATION IS CHECKED HERE: an index
  /// whose slot has since been reused does not resolve, which is the whole reason the identity
  /// carries two numbers.
  [[nodiscard]] Entity* Find(EntityId _id) noexcept;
  [[nodiscard]] const Entity* Find(EntityId _id) const noexcept;

  /// False on a stale identity. A speed of zero is legal and means an entity that has somewhere to
  /// be and no way to get there; it never arrives, and that is the honest outcome rather than a
  /// teleport.
  bool OrderMoveTo(EntityId _id, const Neuron::Vec2& _destination, Neuron::Fixed _speedPerTick) noexcept;

  [[nodiscard]] const MoveOrder* FindOrder(EntityId _id) const noexcept;

  /// How many slots exist, live or not. This is the bound the tick iterates over, and it never
  /// shrinks -- a slot that was allocated once is reused rather than removed, because removing it
  /// would renumber every index above it and invalidate every identity in the match.
  [[nodiscard]] std::size_t SlotCount() const noexcept
  {
    return m_slots.size();
  }

  [[nodiscard]] std::size_t AliveCount() const noexcept
  {
    return m_aliveCount;
  }

  /// How many live entities a player owns. THE BOUND Q24 PUTS ON A SELECTION: a command naming
  /// more identities than the sender has entities is refused outright, which is what turns
  /// ADR-003's 5.5x amplification into a rejected packet rather than a loop.
  [[nodiscard]] std::size_t OwnedCount(PlayerId _owner) const noexcept;

  [[nodiscard]] bool IsSlotAlive(std::size_t _slot) const noexcept;

  /// Slot access for the tick, which walks indices rather than identities. _slot must be below
  /// SlotCount(); a dead slot's contents are the previous occupant's and are not to be read.
  [[nodiscard]] Entity& EntityInSlot(std::size_t _slot) noexcept;
  [[nodiscard]] const Entity& EntityInSlot(std::size_t _slot) const noexcept;
  [[nodiscard]] MoveOrder& OrderInSlot(std::size_t _slot) noexcept;
  [[nodiscard]] const MoveOrder& OrderInSlot(std::size_t _slot) const noexcept;

private:
  struct Slot
  {
    Entity entity{};
    MoveOrder order{};
    bool alive = false;
  };

  [[nodiscard]] const Slot* ResolveSlot(EntityId _id) const noexcept;

  std::vector<Slot> m_slots;

  /// Taken from the back and pushed on the back. See the class comment: which end is not a
  /// preference.
  std::vector<std::uint16_t> m_freeIndices;

  std::size_t m_aliveCount = 0;
};

} // namespace Outpost

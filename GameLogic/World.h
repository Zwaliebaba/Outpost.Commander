#pragma once

#include "GameCore.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace Outpost
{

/// Q61: the group of a move order that was given to no fleet -- a test's, or the host's M0 demo ship's.
/// Two ships in no group are strangers, and avoid each other.
inline constexpr std::uint32_t NO_ORDER_GROUP = 0;

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

  /// Binary-angle units a tick the heading may swing toward where it is going (Q59). Half a turn or
  /// more turns at once.
  std::uint16_t turnAnglePerTick = 0;

  /// Q61: **ships given the same order do not avoid each other**, which is what lets a fleet fill its
  /// ring. It outlives the order, so a straggler still slots in among the ones that arrived first.
  std::uint32_t group = NO_ORDER_GROUP;

  bool active = false;
};

/// Where a miner is in `GameDesign.md` section 4's loop (M2.6). **Five states in the design and four here**,
/// because "and back" is `ToOre` again: going to ore, extracting, going to unload, unloading.
enum class MiningPhase : std::uint8_t
{
  /// No mine order. A miner told to move somewhere is here, holding whatever it was carrying.
  None,
  ToOre,
  Extracting,
  ToUnload,
  Unloading
};

/// Thousandths of ore to ore. `MineOrder::cargoMilliOre` says why the unit is thousandths.
inline constexpr std::uint32_t MILLI_ORE_PER_ORE = 1000;

/// **THE ONE STANDING ORDER** (`GameDesign.md` section 4), and host-only for the reason `MoveOrder` is: the
/// client is sent where a miner is, never what it has been told (R19). `GameLogic/MiningSystem.h` runs it.
///
/// R8: a public aggregate.
struct MineOrder
{
  MiningPhase phase = MiningPhase::None;

  /// The rock, as an index into `World::Field()` (Q52).
  std::uint16_t rock = 0;

  /// **THOUSANDTHS OF ORE**, so every rate in the design is a whole number a tick: a laser's 20 a second
  /// is 1,000 a tick at 20 Hz, and unloading's 50 is 2,500. In whole ore the unload would be 2.5 a tick.
  /// **It outlives the order**: a miner ordered elsewhere keeps what it carries (M2.6).
  std::uint32_t cargoMilliOre = 0;

  /// Where it is unloading, once a query has said. `NO_ENTITY` until then, and again if that dies.
  EntityId unloadTarget{};
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
  /// A new entity in the most recently freed slot, or a fresh slot when none is free (the free list is
  /// last in, first out -- see the class comment; this said "lowest-numbered" until the 2026-09-23 review). NO_ENTITY
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
  bool OrderMoveTo(EntityId _id, const Neuron::Vec2& _destination, Neuron::Fixed _speedPerTick, std::uint16_t _turnAnglePerTick,
                   std::uint32_t _group = NO_ORDER_GROUP) noexcept;

  /// A group no order has had yet, for one fleet order's ships (Q61). Counted, never zero, and a pure
  /// function of how many groups came before it -- so two hosts given the same commands number them
  /// the same.
  [[nodiscard]] std::uint32_t NewOrderGroup() noexcept;

  [[nodiscard]] const MoveOrder* FindOrder(EntityId _id) const noexcept;

  /// **A MINE ORDER, TO ROCK _rock** (M2.6), starting from `ToOre` and keeping the cargo already aboard.
  /// False on a stale identity. Whether the entity can mine is the intake's question, not this one.
  bool OrderMine(EntityId _id, std::uint16_t _rock) noexcept;

  /// Ends the mine order and **keeps the cargo** -- what a move order does to a miner mid-cycle. False on a
  /// stale identity.
  bool StopMining(EntityId _id) noexcept;

  [[nodiscard]] const MineOrder* FindMine(EntityId _id) const noexcept;

  /// **THE ASTEROID FIELD** (M2.6): `GenerateField`'s rows for this match, which the host sets once when the
  /// match begins -- R23's host half. The rocks are not entities before M3 (Q22), so they live beside the
  /// store rather than in it, and a mine order names one by its index here (Q52). Empty until set, which is
  /// what a world built by a test that does not mine is.
  void SetField(std::vector<Placement> _field);

  [[nodiscard]] std::span<const Placement> Field() const noexcept
  {
    return m_field;
  }

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
  /// ADR-003's amplification (3.7x since ADR-024) into a rejected packet rather than a loop.
  [[nodiscard]] std::size_t OwnedCount(PlayerId _owner) const noexcept;

  [[nodiscard]] bool IsSlotAlive(std::size_t _slot) const noexcept;

  /// Slot access for the tick, which walks indices rather than identities. _slot must be below
  /// SlotCount(); a dead slot's contents are the previous occupant's and are not to be read.
  [[nodiscard]] Entity& EntityInSlot(std::size_t _slot) noexcept;
  [[nodiscard]] const Entity& EntityInSlot(std::size_t _slot) const noexcept;
  [[nodiscard]] MoveOrder& OrderInSlot(std::size_t _slot) noexcept;
  [[nodiscard]] const MoveOrder& OrderInSlot(std::size_t _slot) const noexcept;
  [[nodiscard]] MineOrder& MineInSlot(std::size_t _slot) noexcept;
  [[nodiscard]] const MineOrder& MineInSlot(std::size_t _slot) const noexcept;

private:
  struct Slot
  {
    Entity entity{};
    MoveOrder order{};
    MineOrder mine{};
    bool alive = false;
  };

  [[nodiscard]] const Slot* ResolveSlot(EntityId _id) const noexcept;

  std::vector<Slot> m_slots;

  /// Taken from the back and pushed on the back. See the class comment: which end is not a
  /// preference.
  std::vector<std::uint16_t> m_freeIndices;

  std::size_t m_aliveCount = 0;
  std::uint32_t m_lastOrderGroup = NO_ORDER_GROUP;

  std::vector<Placement> m_field;
};

} // namespace Outpost

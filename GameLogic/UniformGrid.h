#pragma once

#include "World.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace Outpost
{

/// The spatial index (M2.5): **a uniform grid, 512-unit cells, 32 x 32 over the square** (`TechnicalDesign.md`
/// section 2). It is what M2.6's "nearest thing that accepts ore" and M3's target selection ask instead of
/// walking every entity.
///
/// **A CANDIDATE STRUCTURE ONLY.** The order a query visits cells in, and the order entities sit in a cell,
/// are facts about this class's layout and not about the match -- so **every answer leaves here sorted by
/// entity identity**, and `Nearest` breaks a tie on identity and never on which cell it looked in first.
/// ADR-002 is blunt about why that is not optional: an unordered tie-break is a desynchronization that
/// appears once an hour and cannot be reproduced.
///
/// **REBUILT, NOT MAINTAINED.** `Rebuild` takes the world whole, in index order, once a tick before
/// anything queries it. At 110 entities that is a counting pass and a fill; an incremental index would be
/// a second copy of every position to keep in step with the world for no measured gain, which is the
/// same argument that keeps one off the client (M1.10's linear hit test).
///
/// **NO FLOAT, NO HASH, NO ADDRESS.** Cells are integer divisions of fixed-point positions, distances are
/// compared squared in 64 bits, and the only containers are vectors filled in index order (R16).
class UniformGrid
{
public:
  /// ADR-001's square is 16,384 units, so 32 cells of 512 cover it exactly.
  static constexpr std::int32_t CELL_UNITS = 512;
  static constexpr std::int32_t CELLS_PER_SIDE = 32;
  static constexpr std::size_t CELL_COUNT = static_cast<std::size_t>(CELLS_PER_SIDE) * CELLS_PER_SIDE;

  /// One cell's side in `Neuron::Fixed`.
  static constexpr Neuron::Fixed CELL_SIZE = CELL_UNITS * Neuron::FIXED_ONE;

  static_assert(static_cast<std::int64_t>(CELL_SIZE) * CELLS_PER_SIDE == 2 * static_cast<std::int64_t>(PLAY_AREA_HALF_EXTENT),
                "the grid must cover ADR-001's square exactly");

  /// Every live entity into the cell its position falls in. **An entity outside the square is kept, in the
  /// nearest edge cell**, rather than dropped: validation clamps orders to the play area, but a grid that
  /// silently lost a thing that got past it would turn one bug into two.
  void Rebuild(const World& _world);

  /// Every live entity within _radius of _center -- **inclusive, so one exactly on the circle is in** --
  /// into _out, which is cleared first, **sorted by identity.** A negative radius finds nothing.
  ///
  /// _world must be the world the grid was last rebuilt from, unchanged since: positions are read from it
  /// for the distance test, and an entity destroyed since the rebuild is skipped rather than returned.
  void Query(const World& _world, const Neuron::Vec2& _center, Neuron::Fixed _radius, std::vector<EntityId>& _out) const;

  /// **THE NEAREST ENTITY WITHIN _radius THAT _accept TAKES, TIES BROKEN ON IDENTITY**, or `NO_ENTITY`.
  /// Candidates are sorted by identity before any is compared and a later one wins only when strictly
  /// nearer, so of two at exactly equal distance the lower identity wins on every run and every machine.
  ///
  /// _accept sees a `const Entity&` and says yes or no. It is how M2.6 asks for "owned by this player and
  /// accepts ore" without the grid knowing what ore is. _scratch is the caller's, so a query in the tick
  /// allocates nothing once it has grown.
  template <typename Fn>
  [[nodiscard]] EntityId Nearest(const World& _world, const Neuron::Vec2& _center, Neuron::Fixed _radius, Fn&& _accept,
                                 std::vector<EntityId>& _scratch) const
  {
    Query(_world, _center, _radius, _scratch);

    EntityId best = NO_ENTITY;
    std::int64_t bestSquared = 0;
    for (const EntityId id : _scratch)
    {
      const Entity* entity = _world.Find(id);
      if ((entity == nullptr) || !_accept(*entity))
      {
        continue;
      }
      const std::int64_t squared = DistanceSquared(entity->position, _center);
      // STRICTLY NEARER, IN IDENTITY ORDER: the first of equals is kept, and the first is the lowest.
      if (!best.IsValid() || (squared < bestSquared))
      {
        best = id;
        bestSquared = squared;
      }
    }
    return best;
  }

  /// How many entities the last rebuild placed. A test's check that nothing was dropped.
  [[nodiscard]] std::size_t EntryCount() const noexcept
  {
    return m_entries.size();
  }

  /// Which cell a position falls in, as `row * CELLS_PER_SIDE + column`, clamped to the square. Exposed
  /// because the edges are where a grid goes wrong and the suite asserts them directly.
  [[nodiscard]] static std::size_t CellOf(const Neuron::Vec2& _position) noexcept;

  /// Squared distance in `Fixed` units, in 64 bits. Two positions at opposite corners of the square are
  /// 2^22 apart on an axis, so the square of the whole diagonal is under 2^45 and cannot overflow.
  [[nodiscard]] static std::int64_t DistanceSquared(const Neuron::Vec2& _a, const Neuron::Vec2& _b) noexcept;

private:
  /// A column or row, clamped to the grid. The square runs from -PLAY_AREA_HALF_EXTENT, so a position is
  /// shifted to start at zero before it is divided, which keeps the division on non-negative numbers and
  /// its rounding one-directional. **64 bits in**, because a query's box -- a center near the edge plus a
  /// large radius -- can pass what an int32 holds.
  [[nodiscard]] static std::int32_t AxisCell(std::int64_t _coordinate) noexcept;

  /// Where each cell's entries start in `m_entries`; the cell after it says where they end. A counting
  /// sort's prefix sums: one allocation that never shrinks, and each cell's entries in index order.
  std::array<std::uint32_t, CELL_COUNT + 1> m_cellStart{};
  std::vector<EntityId> m_entries;

  /// Rebuild's working counts, kept so a rebuild allocates nothing after the first.
  std::array<std::uint32_t, CELL_COUNT> m_fill{};
};

} // namespace Outpost

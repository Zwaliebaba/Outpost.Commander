#pragma once

#include "UniformGrid.h"
#include "World.h"

#include <cstdint>
#include <span>
#include <vector>

namespace Outpost
{

/// One unload, as it happened: who delivered how much to what. **M2.7 turns these into credits**; this step
/// only produces them, so the loop can be pinned before the economy reads it.
///
/// R8: a public aggregate.
struct OreDelivery
{
  PlayerId player = NO_PLAYER;
  EntityId miner{};
  EntityId acceptor{};
  std::uint32_t milliOre = 0;
};

/// Q51, answered by the owner on 2026-09-23 and provisional: **a miner unloads at 50 ore a second**, so a
/// one-laser hold takes two seconds at the station -- a real dwell in the area point defense covers
/// (`GameDesign.md` section 7), and small beside the round trip.
inline constexpr std::uint32_t UNLOAD_ORE_PER_SECOND = 50;

/// `GameDesign.md` section 4's loop, **on the tick and all of it integer** (M2.6). It runs after movement,
/// as `TechnicalDesign.md` section 2's order puts mining after movement and weapons and before build queues.
///
/// **A STANDING ORDER RE-ISSUES WORK TO ITSELF, INSIDE THE SAME PASS.** A miner that arrives extracts on the
/// tick it arrives; one that fills is sent to unload on the tick it fills; one that empties is sent back to
/// the same rock on the tick it empties. Nothing waits a tick for the command path -- which is why an economy
/// runs without a player shepherding it, and why each phase lasts exactly what the design's figures say.
///
/// **NOTHING HERE IS A SPECIAL CASE FOR A "MINER".** Capacity, rate and range come from the design's derived
/// stats (R24), and where it unloads from `FindUnloadTarget`. A two-laser design carries twice as much and
/// extracts twice as fast because the derivation sums, not because this file knows.
class MiningSystem
{
public:
  /// One tick, for every entity with a mine order, **in index order**. Rebuilds the grid first -- from the
  /// world as movement left it -- because the unload query reads it. The tick's deliveries replace the last
  /// tick's.
  void Advance(World& _world);

  /// What this tick's unloads delivered, in the order they happened -- which is index order.
  [[nodiscard]] std::span<const OreDelivery> Deliveries() const noexcept
  {
    return m_deliveries;
  }

  /// The grid as this tick's `Advance` rebuilt it.
  [[nodiscard]] const UniformGrid& Grid() const noexcept
  {
    return m_grid;
  }

private:
  UniformGrid m_grid;
  std::vector<EntityId> m_scratch;

  /// **ONE EXTRACTOR PER ROCK PER TICK** (`OpenQuestions.md` Q62): which rocks have yielded ore this tick,
  /// indexed as the field is. Sized to the field and cleared at the top of every `Advance`.
  std::vector<std::uint8_t> m_rockWorked;
  std::vector<OreDelivery> m_deliveries;
};

} // namespace Outpost

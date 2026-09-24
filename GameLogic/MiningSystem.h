#pragma once

#include "UniformGrid.h"
#include "World.h"

#include <cstdint>
#include <span>
#include <vector>

namespace Outpost
{

/// **HOW MANY MINERS HAVE EACH ROCK**, indexed as the field is: every live entity with a mine order, of any owner,
/// counted against the rock it names -- except _excluded, whose order is about to be replaced (the owner, 2026-09-24,
/// `OpenQuestions.md` Q82).
[[nodiscard]] std::vector<std::uint32_t> RockClaims(const World& _world, std::span<const EntityId> _excluded = {});

/// **ONE MINER TO A ROCK** (Q82): the rock with ore left that the fewest miners have, and of those the nearest to
/// _from, ties to the lower index (R16). An unclaimed rock always beats a claimed one however near; only when every
/// rock with ore is taken do two miners share. False when no rock has ore.
[[nodiscard]] bool BestRock(const World& _world, std::span<const std::uint32_t> _claims, const Neuron::Vec2& _from,
                            std::uint16_t& _outRock);

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
  ///
  /// _struck is what the weapons fired on this tick (`WeaponSystem::Struck`), which starts a miner's flight (Q64).
  void Advance(World& _world, std::span<const EntityId> _struck = {});

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
  /// One acceptor's unload point this tick, for one miner design (Q63). R8: a public aggregate.
  struct UnloadPoint
  {
    EntityId acceptor{};
    DesignId miner = DesignId::Miner;

    /// A hostile is near enough that the point is the far side; false means the near side, as before Q63.
    bool farSide = false;
    Neuron::Vec2 point{};
  };

  /// Where a miner of _miner's design unloads at _acceptor this tick. **Asked once an acceptor a tick**, since
  /// the threat query covers the home field and every miner going home asks the same question.
  [[nodiscard]] const UnloadPoint& UnloadPointFor(const World& _world, const Entity& _acceptor, DesignId _miner);

  UniformGrid m_grid;
  std::vector<EntityId> m_scratch;

  /// This tick's answers to `UnloadPointFor`, cleared at the top of every `Advance`.
  std::vector<UnloadPoint> m_unloadPoints;

  /// **ONE EXTRACTOR PER ROCK PER TICK** (`OpenQuestions.md` Q62): which rocks have yielded ore this tick,
  /// indexed as the field is. Sized to the field and cleared at the top of every `Advance`.
  std::vector<std::uint8_t> m_rockWorked;

  /// One a slot: fired on this tick, from _struck.
  std::vector<std::uint8_t> m_struck;
  std::vector<OreDelivery> m_deliveries;
};

} // namespace Outpost

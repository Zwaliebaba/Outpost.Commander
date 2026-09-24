#pragma once

#include "Interpolation.h"

#include "GameCore.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace Outpost
{

/// What one drawn frame found, per entity, for the log and the harness. R8: a public aggregate.
struct DrawnSummary
{
  /// Entities drawn between two samples.
  std::uint32_t interpolating = 0;
  /// Entities whose newest sample is older than the render time, drawn where it put them. **The
  /// ordinary state of a distant, idle entity under ADR-024**, which the accumulator refreshes rarely on
  /// purpose -- not a fault.
  std::uint32_t holding = 0;
  /// Entities with a single sample, drawn as it stands.
  std::uint32_t starved = 0;
};

/// Every entity the client has been told about, a few samples of each, and the clock that decides
/// where between them the frame is drawn.
///
/// **PER ENTITY, SINCE ADR-024.** An update is a bag of records at one tick, not the world, so there is
/// no snapshot to hold: each entity keeps its own samples, stamped with the tick they describe and the
/// local time they arrived, and interpolates between its own pair. An entity absent from an update is
/// neither dead nor unchanged -- it was not due. **Death is a removal, and forgetting is the sweep.**
///
/// **ORDERING IS A TICK COMPARISON AND NOTHING ELSE.** A record whose tick is not newer than the newest
/// sample held for that entity is refused. There is no sequence window and no reorder buffer.
///
/// R19: the client links no simulation. What this holds is state it was SENT, kept just long enough to
/// draw a smooth frame. Nothing here advances anything.
class ReplicaStore
{
public:
  /// HOW MANY SAMPLES AN ENTITY KEEPS, COMPUTED RATHER THAN CHOSEN. Enough intervals to reach back past
  /// the interpolation delay, plus the sample on the near side of it: three at 75 over 50. This is the
  /// arithmetic `TechnicalDesign.md` section 6 used to state as a snapshot count; under ADR-024 it is the
  /// same arithmetic per entity, and it becomes four by itself the day either figure moves.
  static constexpr std::size_t INTERVALS_SPANNED =
    (INTERPOLATION_DELAY_MILLISECONDS + SNAPSHOT_INTERVAL_MILLISECONDS - 1) / SNAPSHOT_INTERVAL_MILLISECONDS;
  static constexpr std::size_t RETAINED_COUNT = INTERVALS_SPANNED + 1;
  static_assert(RETAINED_COUNT >= 2, "Interpolation needs a pair to sit between.");

  /// **NOTHING IS FORGOTTEN INSIDE THE LINK-LOSS SECOND** (the 2026-09-23 review, m2). Three sweeps is 150 ms
  /// at the MVP's 110 entities, so any silence of three to nineteen ticks -- shorter than the second after which
  /// `ClientFrame` calls the link lost -- forgot the entities that rode a tick's second datagram and re-added them
  /// from nothing a moment later: a one-frame pop instead of a hold. The horizon is now the longer of three
  /// sweeps and twenty ticks, which is `ClientFrame::LINK_SILENCE_MILLISECONDS` at the snapshot interval (a
  /// `static_assert` there keeps the two together). Past twenty ticks a silent client rejoins and clears the
  /// store anyway.
  static constexpr std::uint32_t FORGET_FLOOR_TICKS = 20;

  /// What one update did to the store. R8: a public aggregate.
  struct AcceptResult
  {
    /// Records that became an entity's newest sample.
    std::uint32_t applied = 0;
    /// Records no newer than what the entity already held -- reordered, or a repeat.
    std::uint32_t refused = 0;
    /// Removals that took an entity out. A repeat of one already applied takes nothing and is not
    /// counted.
    std::uint32_t removed = 0;
    /// Entities forgotten because they had gone three sweeps without a record.
    std::uint32_t forgotten = 0;

    /// **THE REFRESH INTERVAL PER ENTITY** (ADR-024, ADR-022): records applied to an entity that already
    /// held a sample, the ticks between that sample and this one summed over them, and the longest. The
    /// first record of an entity has no interval and is not counted.
    std::uint32_t refreshed = 0;
    std::uint64_t refreshTicksTotal = 0;
    std::uint32_t refreshTicksMax = 0;
  };

  /// Folds in an update that has arrived, stamped with the local time it arrived at.
  ///
  /// _arrivalMilliseconds is from the same monotonic clock as `RenderMilliseconds`' argument. The store
  /// never reads a clock itself, which is what lets a suite drive it.
  AcceptResult Accept(const Update& _update, std::uint64_t _arrivalMilliseconds);

  /// Where the frame is drawn: the delay behind now, on the caller's clock. Driven by now rather than by
  /// any arrival, so nothing arriving can move it backwards. Zero before the delay has elapsed.
  [[nodiscard]] static std::uint64_t RenderMilliseconds(std::uint64_t _nowMilliseconds) noexcept;

  /// Every held entity at the render time, interpolated between its own two samples where they straddle
  /// it and held at its newest where they do not. **It never extrapolates** (ADR-024, R19): an entity
  /// the accumulator has not refreshed stays where the host last said it was.
  ///
  /// _outRecords is cleared and refilled, in index order, so a caller keeps one vector across frames and
  /// allocates nothing in steady state.
  DrawnSummary Drawn(std::uint64_t _renderMilliseconds, std::vector<EntityRecord>& _outRecords) const;

  /// The newest record of every held entity, in index order. What selection, hit testing and the
  /// panels read -- the truth as last told, not the interpolated picture.
  [[nodiscard]] std::span<const EntityRecord> Entities() const noexcept
  {
    return m_newest;
  }

  /// **WHAT THE LAST `Accept`'S REMOVALS TOOK OUT**, each as its newest record: where a death happened, for a
  /// wreck (M3.4). **Removals only, never the forgotten**: an entity the store forgot past the horizon may have
  /// died, or its records may simply have stopped, and a wreck from that would be a guess.
  [[nodiscard]] std::span<const EntityRecord> Removed() const noexcept
  {
    return m_removed;
  }

  /// **THE SPENT ROCKS AS THE NEWEST UPDATE SAID** (Q83): `IsRockSpent` reads it. A late update never winds it back.
  [[nodiscard]] std::span<const std::uint8_t> SpentRocks() const noexcept
  {
    return m_spentRocks;
  }

  /// This client's own block from the newest update, or nullptr before one has arrived.
  [[nodiscard]] const PlayerBlock* Own() const noexcept
  {
    return m_hasUpdate ? &m_own : nullptr;
  }

  /// The newest tick any update has described. Zero before one has arrived.
  [[nodiscard]] std::uint32_t NewestTick() const noexcept
  {
    return m_newestTick;
  }

  [[nodiscard]] std::size_t HeldCount() const noexcept
  {
    return m_newest.size();
  }

  /// Records refused across every update, because they were no newer than what was held. A counter
  /// rather than a log: some are ordinary, and a rising rate is the thing worth seeing.
  [[nodiscard]] std::uint64_t RefusedCount() const noexcept
  {
    return m_refusedCount;
  }

  /// Updates the transport sequence says never arrived. **The stress harness's loss figure** (ADR-022);
  /// the sequence orders nothing, so this is the only thing it is read for.
  [[nodiscard]] std::uint64_t LostCount() const noexcept
  {
    return m_lostCount;
  }

  /// Forgets everything. What a rejoin does: the host resets this client's accumulator at the same
  /// moment, so everything is sent again within one sweep and nothing stale survives.
  void Clear() noexcept;

private:
  struct Sample
  {
    EntityRecord record{};
    std::uint32_t tick = 0;
    std::uint64_t arrivalMilliseconds = 0;
  };

  /// One entity's samples, oldest to newest in a ring. Indexed by the wire index, so a slot reused by a
  /// new entity is noticed by the whole identity differing and starts again from nothing.
  struct Held
  {
    std::array<Sample, RETAINED_COUNT> samples{};
    std::size_t count = 0;
    std::size_t writeIndex = 0;

    [[nodiscard]] const Sample& Newest() const noexcept
    {
      return samples[(writeIndex + RETAINED_COUNT - 1) % RETAINED_COUNT];
    }
    [[nodiscard]] const Sample& FromOldest(std::size_t _step) const noexcept
    {
      return samples[(writeIndex + RETAINED_COUNT - count + _step) % RETAINED_COUNT];
    }
  };

  void RebuildNewest();

  std::vector<Held> m_held;
  std::vector<EntityRecord> m_newest;
  std::vector<EntityRecord> m_removed;
  std::vector<std::uint8_t> m_spentRocks;

  PlayerBlock m_own{};
  bool m_hasUpdate = false;
  std::uint32_t m_newestTick = 0;
  std::uint16_t m_liveEntityCount = 0;

  std::uint16_t m_lastSequence = 0;
  std::uint64_t m_refusedCount = 0;
  std::uint64_t m_lostCount = 0;
};

/// Serial-number comparison over the wire's 16-bit sequence -- true when _candidate is newer than
/// _reference, across the wrap. Since ADR-024 it orders nothing and is read only to count loss.
[[nodiscard]] constexpr bool SequenceIsNewer(std::uint16_t _candidate, std::uint16_t _reference) noexcept
{
  return static_cast<std::int16_t>(static_cast<std::uint16_t>(_candidate - _reference)) > 0;
}

} // namespace Outpost

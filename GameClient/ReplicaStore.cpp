#include "pch.h"

#include "ReplicaStore.h"

#include <algorithm>

namespace Outpost
{

ReplicaStore::AcceptResult ReplicaStore::Accept(const Update& _update, std::uint64_t _arrivalMilliseconds)
{
  AcceptResult result;
  m_removed.clear();

  // LOSS, COUNTED FROM THE SEQUENCE AND NOTHING ELSE. A gap forward is updates that never came; a step
  // backwards or a repeat is reordering, which the per-entity tick comparison below already handles.
  if (m_hasUpdate && SequenceIsNewer(_update.sequence, m_lastSequence))
  {
    m_lostCount += static_cast<std::uint16_t>(_update.sequence - m_lastSequence - 1);
  }
  if (!m_hasUpdate || SequenceIsNewer(_update.sequence, m_lastSequence))
  {
    m_lastSequence = _update.sequence;
  }

  // THE OWN BLOCK AND THE LIVE COUNT FOLLOW THE NEWEST TICK, so a late update cannot wind the credits
  // back. Equal ticks are the two updates of one tick and carry the same block.
  if (!m_hasUpdate || (_update.tick >= m_newestTick))
  {
    m_own = _update.own;
    m_newestTick = _update.tick;
    m_liveEntityCount = _update.liveEntityCount;
  }
  m_hasUpdate = true;

  for (const EntityRecord& record : _update.records)
  {
    const std::size_t index = IndexOf(record.identity);
    if (index >= m_held.size())
    {
      m_held.resize(index + 1);
    }
    Held& held = m_held[index];

    if (held.count > 0)
    {
      const Sample& newest = held.Newest();

      // **A DIFFERENT GENERATION IS A DIFFERENT ENTITY**, and it starts from nothing -- interpolating
      // from the slot's previous occupant would draw one ship sliding into another's place.
      if (newest.record.identity != record.identity)
      {
        held = Held{};
      }
      else if (_update.tick <= newest.tick)
      {
        ++result.refused;
        continue;
      }
      else
      {
        // THE REFRESH INTERVAL, which is ADR-024's figure: how long this entity went between records.
        const std::uint32_t intervalTicks = _update.tick - newest.tick;
        ++result.refreshed;
        result.refreshTicksTotal += intervalTicks;
        result.refreshTicksMax = std::max(result.refreshTicksMax, intervalTicks);
      }
    }

    held.samples[held.writeIndex] = Sample{.record = record, .tick = _update.tick, .arrivalMilliseconds = _arrivalMilliseconds};
    held.writeIndex = (held.writeIndex + 1) % RETAINED_COUNT;
    if (held.count < RETAINED_COUNT)
    {
      ++held.count;
    }
    ++result.applied;
  }

  // REMOVALS ARE MATCHED ON THE WHOLE IDENTITY. A removal repeated after the slot was refilled names
  // the old occupant, and must not take the new one with it.
  for (const WireIdentity removed : _update.removals)
  {
    const std::size_t index = IndexOf(removed);
    if ((index < m_held.size()) && (m_held[index].count > 0) && (m_held[index].Newest().record.identity == removed))
    {
      m_removed.push_back(m_held[index].Newest().record);
      m_held[index] = Held{};
      ++result.removed;
    }
  }

  // **FORGETTING IS THE SWEEP** (ADR-024). The host sends every live entity within one sweep, so one that
  // has gone three without a record is not alive: it died while this client was away, or its removals
  // were all lost, and either way it is never going to be refreshed again. **Never sooner than the link-loss
  // second** (`FORGET_FLOOR_TICKS`), so a burst of loss the link survives does not pop what it did not kill.
  const std::uint32_t forgetTicks = std::max(FORGET_AFTER_SWEEPS * SweepTicks(m_liveEntityCount), FORGET_FLOOR_TICKS);
  for (Held& held : m_held)
  {
    if ((held.count > 0) && ((m_newestTick - held.Newest().tick) > forgetTicks) && (held.Newest().tick < m_newestTick))
    {
      held = Held{};
      ++result.forgotten;
    }
  }

  m_refusedCount += result.refused;
  RebuildNewest();
  return result;
}

void ReplicaStore::RebuildNewest()
{
  // Assigned over rather than reconstructed, so the vector keeps the capacity it already had.
  m_newest.clear();
  for (const Held& held : m_held)
  {
    if (held.count > 0)
    {
      m_newest.push_back(held.Newest().record);
    }
  }
}

void ReplicaStore::Clear() noexcept
{
  m_held.clear();
  m_newest.clear();
  m_removed.clear();
  m_own = PlayerBlock{};
  m_hasUpdate = false;
  m_newestTick = 0;
  m_liveEntityCount = 0;
}

std::uint64_t ReplicaStore::RenderMilliseconds(std::uint64_t _nowMilliseconds) noexcept
{
  // Saturating rather than wrapping. The first frames of a match are inside the delay, and an unsigned
  // subtraction there would produce a render time enormously in the future.
  if (_nowMilliseconds <= INTERPOLATION_DELAY_MILLISECONDS)
  {
    return 0;
  }
  return _nowMilliseconds - INTERPOLATION_DELAY_MILLISECONDS;
}

DrawnSummary ReplicaStore::Drawn(std::uint64_t _renderMilliseconds, std::vector<EntityRecord>& _outRecords) const
{
  DrawnSummary summary;
  _outRecords.clear();

  for (const Held& held : m_held)
  {
    if (held.count == 0)
    {
      continue;
    }

    if (held.count == 1)
    {
      _outRecords.push_back(held.Newest().record);
      ++summary.starved;
      continue;
    }

    const Sample& newest = held.Newest();
    if (_renderMilliseconds >= newest.arrivalMilliseconds)
    {
      // PAST THE NEWEST SAMPLE: HOLD. No extrapolation -- the host has not said where this entity went.
      _outRecords.push_back(newest.record);
      ++summary.holding;
      continue;
    }

    // Oldest first. Samples were admitted in tick order, so walking the ring forward walks time forward.
    const Sample* older = &held.FromOldest(0);
    const Sample* newer = &held.FromOldest(1);
    for (std::size_t step = 1; step < held.count; ++step)
    {
      older = &held.FromOldest(step - 1);
      newer = &held.FromOldest(step);
      if (_renderMilliseconds <= newer->arrivalMilliseconds)
      {
        break;
      }
    }

    const Playout playout = ComputePlayout(older->arrivalMilliseconds, newer->arrivalMilliseconds, _renderMilliseconds);
    EntityRecord shown = newer->record;
    static_cast<void>(InterpolateRecord(older->record, newer->record, playout.fraction, shown));
    _outRecords.push_back(shown);
    ++summary.interpolating;
  }

  return summary;
}

} // namespace Outpost

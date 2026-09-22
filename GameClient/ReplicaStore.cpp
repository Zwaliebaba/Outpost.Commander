#include "pch.h"

#include "ReplicaStore.h"

namespace Outpost
{

bool ReplicaStore::Accept(const Snapshot& _snapshot, std::uint64_t _arrivalMilliseconds) noexcept
{
  if (m_heldCount > 0)
  {
    const Snapshot* newest = Newest();
    // Out of order, or the same snapshot twice. Both are refusals: an arrival that is not newer
    // than what is held has nothing to add, and admitting it would put the ring out of order,
    // which every read below assumes it is not.
    if (!SequenceIsNewer(_snapshot.sequence, newest->sequence))
    {
      ++m_refusedCount;
      return false;
    }
  }

  Slot& slot = m_slots[m_writeIndex];
  // Assigned rather than constructed, so the vectors inside keep the capacity they already had.
  slot.snapshot = _snapshot;
  slot.arrivalMilliseconds = _arrivalMilliseconds;

  m_writeIndex = (m_writeIndex + 1) % RETAINED_COUNT;
  if (m_heldCount < RETAINED_COUNT)
  {
    ++m_heldCount;
  }
  return true;
}

std::uint64_t ReplicaStore::RenderMilliseconds(std::uint64_t _nowMilliseconds) noexcept
{
  // Saturating rather than wrapping. The first frames of a match are inside the delay, and an
  // unsigned subtraction there would produce a render time enormously in the future.
  if (_nowMilliseconds <= INTERPOLATION_DELAY_MILLISECONDS)
  {
    return 0;
  }
  return _nowMilliseconds - INTERPOLATION_DELAY_MILLISECONDS;
}

const Snapshot* ReplicaStore::Newest() const noexcept
{
  if (m_heldCount == 0)
  {
    return nullptr;
  }
  const std::size_t newestIndex = (m_writeIndex + RETAINED_COUNT - 1) % RETAINED_COUNT;
  return &m_slots[newestIndex].snapshot;
}

ReplicaStore::Frame ReplicaStore::FrameAt(std::uint64_t _renderMilliseconds) const noexcept
{
  Frame frame;

  if (m_heldCount == 0)
  {
    return frame;
  }

  // Oldest first. The ring is in arrival order and Accept refuses anything that is not newer, so
  // walking it forward walks time forward -- there is no sort here and there must never be one.
  const std::size_t oldestIndex = (m_writeIndex + RETAINED_COUNT - m_heldCount) % RETAINED_COUNT;

  const Slot* older = nullptr;
  const Slot* newer = nullptr;
  for (std::size_t step = 0; step + 1 < m_heldCount; ++step)
  {
    const Slot& candidateOlder = m_slots[(oldestIndex + step) % RETAINED_COUNT];
    const Slot& candidateNewer = m_slots[(oldestIndex + step + 1) % RETAINED_COUNT];
    if ((_renderMilliseconds >= candidateOlder.arrivalMilliseconds) && (_renderMilliseconds <= candidateNewer.arrivalMilliseconds))
    {
      older = &candidateOlder;
      newer = &candidateNewer;
      break;
    }
  }

  if (older != nullptr)
  {
    frame.older = &older->snapshot;
    frame.newer = &newer->snapshot;
    frame.playout = ComputePlayout(older->arrivalMilliseconds, newer->arrivalMilliseconds, _renderMilliseconds);
    return frame;
  }

  // Nothing straddles the render time. Two ways to get here and they want different answers.
  const std::size_t newestIndex = (m_writeIndex + RETAINED_COUNT - 1) % RETAINED_COUNT;
  const Slot& newestSlot = m_slots[newestIndex];

  if ((m_heldCount >= 2) && (_renderMilliseconds > newestSlot.arrivalMilliseconds))
  {
    // PAST EVERYTHING HELD: the next snapshot has not arrived. Extrapolate along the last pair
    // for the bounded window and then hold, which ComputePlayout decides -- this function only
    // has to hand it the last pair rather than inventing a second set of rules for the gap.
    const std::size_t previousIndex = (newestIndex + RETAINED_COUNT - 1) % RETAINED_COUNT;
    const Slot& previousSlot = m_slots[previousIndex];
    frame.older = &previousSlot.snapshot;
    frame.newer = &newestSlot.snapshot;
    frame.playout = ComputePlayout(previousSlot.arrivalMilliseconds, newestSlot.arrivalMilliseconds, _renderMilliseconds);
    return frame;
  }

  // BEFORE EVERYTHING HELD, or only one snapshot held at all. The first is the opening of a match,
  // where the delay is still filling; the second is the first snapshot of one. Neither is an
  // error and both draw the newest as it stands.
  frame.older = &newestSlot.snapshot;
  frame.newer = &newestSlot.snapshot;
  frame.playout = Playout{0, PlayoutState::Starved};
  return frame;
}

} // namespace Outpost

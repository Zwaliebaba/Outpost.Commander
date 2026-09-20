#pragma once

#include "Messages.h"

#include <cstdint>
#include <vector>

// What each client was last told, so that the next frame can be a delta against it
// (TechnicalDesign.md §5.3). The host keeps the last 32 frames it sent a client - 3.2 seconds at
// the 10 Hz publish rate - and encodes against the newest one the client has acknowledged. A
// client whose acknowledged baseline has fallen out of the history gets a full frame, which is the
// same path a joining client takes, so there is one path rather than two.
//
// WHY 32. A client that has acknowledged nothing for 3.2 seconds is a client that has lost more
// than thirty consecutive datagrams, which is a link that is down rather than lossy. The cost is
// 32 copies of what that client can see; at 600 visible objects and 31 bytes a device that is
// under 600 KB a client, which is what the ADR records against the alternative of keeping one.

namespace Outpost
{

inline constexpr std::size_t FRAME_HISTORY = 32;

/// What a client holds if it applied a frame: the records as they were sent, in ascending id, so
/// that the next frame is a walk over two sorted lists.
struct FrameRecord
{
  std::uint32_t sequence = NO_BASELINE;
  std::vector<DeviceState> devices;
  std::vector<StructureState> structures;
  std::vector<WreckState> wrecks;
  std::vector<FeatureState> features;
  /// The fog runs this frame carried, relative to the fog of the baseline it was encoded against.
  ///
  /// THE FOG IS THE ONE FIELD THAT IS NOT REBUILT FROM THE SIMULATION EACH PUBLISH, so it is the
  /// one that has to be remembered per frame. Every list above is the whole of what the client can
  /// see now, and the frame is the difference between it and the baseline - so a frame the client
  /// drops costs nothing, because the next one is the same difference taken again. The fog is a
  /// grid of 16,384 cells and is sent as the runs that CHANGED, so a dropped frame's runs would be
  /// lost for good unless the host knows which client state they were relative to. Keeping them
  /// here is what lets ClientView::fog track what the client has ACKNOWLEDGED rather than what it
  /// has been sent (m1-vertical-slice/G1a).
  std::vector<FogDelta> fog;

  /// How many events this client had been sent ALTOGETHER by the end of this frame, counted from
  /// the first one of the match (m1-vertical-slice/C9). A RUNNING TOTAL and not this frame's own
  /// count, which is the whole point of it.
  ///
  /// AN EVENT IS THE OTHER FIELD THAT IS NOT REBUILT FROM THE SIMULATION EACH PUBLISH, and it was
  /// left out when the fog above was fixed. Every state list is the whole of what the client can
  /// see now, so a dropped frame costs nothing; an event is a thing that HAPPENED, and the tick
  /// that happened in is over. Measured on a real match over the loopback, the client applied 105
  /// frames and skipped 360 - 77% - because the publish outruns the acknowledgement, which is the
  /// ordinary case and not a lossy link. So an event sent once and dropped was gone for good.
  /// Holding the count here is what lets ClientView::pendingEvents keep an event until the client
  /// ACKNOWLEDGES the frame that carried it, which is the same bargain the fog struck.
  ///
  /// WHY A RUNNING TOTAL AND NOT THIS FRAME'S COUNT, which is what it was first written as and
  /// which silently lost a quarter of every match's events. The acknowledged sequence does not
  /// move on every publish - the publish outruns the acknowledgement - so the same record is
  /// folded again and again, and a per-frame count drops its own events from the front EVERY TIME
  /// it is folded. Measured: 40 shots over a link dropping a third of its frames, and 15 arrived.
  /// A running total makes folding idempotent, because what is owed is the difference between this
  /// total and the total already dropped (ClientView::eventsForgotten), and folding the same
  /// record twice is a difference of zero.
  ///
  /// It wraps after four billion events, which at the MAX_FRAME_EVENTS a busy match sends is
  /// somewhere past a year of continuous play; the difference is taken in the same unsigned type,
  /// so a wrap is still the right difference as long as no client is four billion events behind,
  /// which MAX_PENDING_EVENTS makes impossible.
  std::uint32_t eventsSentThrough = 0;

  void Clear() noexcept
  {
    sequence = NO_BASELINE;
    eventsSentThrough = 0;
    devices.clear();
    structures.clear();
    wrecks.clear();
    features.clear();
    fog.clear();
  }
};

/// A ring of the frames sent to one client.
class ClientHistory
{
public:
  void Push(FrameRecord _frame)
  {
    if (m_frames.size() >= FRAME_HISTORY)
    {
      m_frames.erase(m_frames.begin());
    }
    m_frames.push_back(std::move(_frame));
  }

  /// The frame with this sequence, or null when it was never sent or has aged out. Null is what
  /// makes the next frame a full one.
  [[nodiscard]] const FrameRecord* Find(std::uint32_t _sequence) const noexcept
  {
    for (const FrameRecord& frame : m_frames)
    {
      if (frame.sequence == _sequence)
      {
        return &frame;
      }
    }
    return nullptr;
  }

  void Clear() noexcept
  {
    m_frames.clear();
  }

  [[nodiscard]] std::size_t Count() const noexcept
  {
    return m_frames.size();
  }

  [[nodiscard]] std::uint32_t Oldest() const noexcept
  {
    return m_frames.empty() ? NO_BASELINE : m_frames.front().sequence;
  }

private:
  std::vector<FrameRecord> m_frames;
};

} // namespace Outpost

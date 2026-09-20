#pragma once

#include "Order.h"
#include "Records.h"

#include <cstdint>

// The one line of warning text a refused order puts on the screen (Design/Interface.md §6;
// m1-vertical-slice/G1b).
//
// IN Replica AND NOT IN THE EXECUTABLE, for the reason Selection.h and OrderInput.h give: an
// Application exports nothing a test DLL can link, so logic worth testing lives in a library. This
// is small, and it is the kind of small that is wrong in a way nobody notices - a line that stays
// for ever, or one that never restarts when the commander asks twice.
//
// IT WATCHES THE SEQUENCE AND NOT THE VALUE, which is the whole reason SeatState carries three
// fields rather than two. Two identical refusals are equal field for field: a commander who clicks
// twice for what he cannot afford would see the line sit there unchanged and read it as not having
// been heard. The sequence counts refusals, so it moves even when nothing else does; it wraps, and
// steps over 0 on the wrap, because 0 is how a seat that has had no refusal at all says so
// (GameShared/Records.h).
//
// THE CLOCK IS THE CALLER'S AND IT IS WALL TIME. A refusal is shown for two SECONDS and not for
// forty ticks: the replica's timeline is the host's tick numbers arriving late, and a host that
// stopped sending would leave the line frozen on the screen rather than letting it expire. The
// caller passes milliseconds from any monotonic origin it likes; nothing here keeps a clock of its
// own, so a test names the moments it wants.

namespace Outpost
{

/// How long a refusal stays on the screen (Design/Interface.md §6).
inline constexpr std::int64_t WARNING_MILLISECONDS = 2000;

/// The refusal being shown, if any, and when it stops.
class WarningLine
{
public:
  /// Takes this frame's seat record. A sequence that has moved starts the two seconds again; one
  /// that has not is ignored, however often it is offered, because a frame carries the same
  /// refusal until a newer one replaces it.
  ///
  /// A sequence of 0 is a seat that has had no refusal, and clears the line rather than showing
  /// one: it is what a fresh join carries, and a commander who rejoins should not be greeted by
  /// the refusal he was given before he left.
  void Take(const SeatState& _seat, std::int64_t _nowMilliseconds) noexcept
  {
    if (_seat.rejectSequence == 0)
    {
      m_sequence = 0;
      m_showingUntil = 0;
      return;
    }
    if (_seat.rejectSequence == m_sequence)
    {
      return;
    }
    m_sequence = _seat.rejectSequence;
    m_kind = static_cast<OrderKind>(_seat.rejectKind);
    m_reason = static_cast<RejectReason>(_seat.rejectReason);
    m_showingUntil = _nowMilliseconds + WARNING_MILLISECONDS;
  }

  /// Whether there is a line to draw at this moment.
  [[nodiscard]] bool Showing(std::int64_t _nowMilliseconds) const noexcept
  {
    return m_sequence != 0 && _nowMilliseconds < m_showingUntil;
  }

  /// What the line says. Meaningless while Showing is false.
  [[nodiscard]] OrderKind Kind() const noexcept
  {
    return m_kind;
  }
  [[nodiscard]] RejectReason Reason() const noexcept
  {
    return m_reason;
  }

private:
  std::uint16_t m_sequence = 0;
  OrderKind m_kind = OrderKind::Move;
  RejectReason m_reason = RejectReason::Accepted;
  std::int64_t m_showingUntil = 0;
};

} // namespace Outpost

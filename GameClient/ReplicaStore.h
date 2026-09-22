#pragma once

#include "Interpolation.h"

#include "GameCore.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace Outpost
{

/// The snapshots the client is currently drawing from, and the clock that decides which.
///
/// R19: the client links no simulation. What this holds is state it was SENT, kept just long
/// enough to draw a smooth frame out of it, and it is discarded as soon as a newer pair covers
/// the render time. Nothing here advances anything -- no ship moves because this class was called.
class ReplicaStore
{
public:
  /// HOW MANY SNAPSHOTS IT TAKES TO COVER THE DELAY, COMPUTED RATHER THAN CHOSEN -- and computing
  /// it is deliberate, because the design's stated figure and the design's stated delay do not
  /// agree and this is the seam where that shows.
  ///
  /// `TechnicalDesign.md` section 6 says the client "holds the two most recent snapshots and
  /// draws at a time 75 milliseconds behind the newest". Those two clauses cannot both hold at
  /// 20 Hz. Two snapshots span one interval, 50 milliseconds, ending at the newest; a render time
  /// 75 milliseconds behind the newest is 25 milliseconds OLDER than the older of the two, so the
  /// pair does not contain the frame being drawn. It is the same 10 Hz residue as `README.md` F4,
  /// one clause further on: the delay moved from 150 to 75 and the depth it implies was never
  /// recomputed. ADR-003 reads the other way and is the one to trust -- "a lost snapshot is a
  /// 50-millisecond gap inside a 75-millisecond buffer, covered without extrapolating" describes
  /// a buffer holding more than one interval of history, which two snapshots do not.
  ///
  /// So the depth is arithmetic on the two constants that are agreed: enough intervals to reach
  /// back past the delay, plus the snapshot on the near side of it. At 75 over 50 that is three,
  /// and it becomes four by itself the day either figure moves.
  static constexpr std::size_t INTERVALS_SPANNED =
    (INTERPOLATION_DELAY_MILLISECONDS + SNAPSHOT_INTERVAL_MILLISECONDS - 1) / SNAPSHOT_INTERVAL_MILLISECONDS;
  static constexpr std::size_t RETAINED_COUNT = INTERVALS_SPANNED + 1;
  static_assert(RETAINED_COUNT >= 2, "Interpolation needs a pair to sit between.");

  /// Takes a snapshot that has arrived, stamped with the local time it arrived at.
  ///
  /// FALSE MEANS IT WAS REFUSED, and the caller counts it rather than retrying: it is older than
  /// one already held, or a duplicate of one. ADR-003 makes that free -- every snapshot is
  /// self-contained, so a refused one costs a frame of animation and nothing else.
  ///
  /// _arrivalMilliseconds is from the same monotonic clock as RenderMilliseconds' argument. The
  /// store never reads a clock itself, which is what lets a suite drive it.
  bool Accept(const Snapshot& _snapshot, std::uint64_t _arrivalMilliseconds) noexcept;

  /// Where the frame is drawn: the delay behind now, on the caller's clock.
  ///
  /// IT IS DRIVEN BY NOW AND NOT BY THE NEWEST ARRIVAL, which is what makes it monotonic. A late
  /// snapshot, an out-of-order one or a burst of three at once all leave this untouched -- the
  /// clock cannot be moved backwards by anything arriving, because nothing arriving is an input
  /// to it. Before the delay has elapsed at all there is no past to draw, so it reads zero.
  [[nodiscard]] static std::uint64_t RenderMilliseconds(std::uint64_t _nowMilliseconds) noexcept;

  /// The pair the render time sits between, and where between them.
  ///
  /// Both pointers are into the store and are invalidated by the next Accept. When fewer than two
  /// snapshots are held, or the render time is past everything held, `older` and `newer` are the
  /// same snapshot and the state says why -- so a caller always has something to draw and never
  /// has to decide what to do with a half-answer.
  struct Frame
  {
    const Snapshot* older = nullptr;
    const Snapshot* newer = nullptr;
    Playout playout;
  };

  [[nodiscard]] Frame FrameAt(std::uint64_t _renderMilliseconds) const noexcept;

  [[nodiscard]] std::size_t HeldCount() const noexcept
  {
    return m_heldCount;
  }

  /// Snapshots refused by Accept. A counter rather than a log: one is ordinary on a wireless
  /// link, and a rising rate is the thing worth seeing.
  [[nodiscard]] std::uint64_t RefusedCount() const noexcept
  {
    return m_refusedCount;
  }

  /// The newest snapshot held, or nullptr when none is. This is what a caller draws from when the
  /// playout is Starved.
  [[nodiscard]] const Snapshot* Newest() const noexcept;

private:
  struct Slot
  {
    Snapshot snapshot;
    std::uint64_t arrivalMilliseconds = 0;
  };

  /// A ring, so that steady state neither allocates nor moves a snapshot: Accept assigns into the
  /// slot it is about to overwrite, and a `std::vector` assigned over keeps the capacity it
  /// already had. The first few snapshots of a match allocate and then it stops.
  std::array<Slot, RETAINED_COUNT> m_slots{};

  /// Where the next snapshot goes. The newest held is the slot before it.
  std::size_t m_writeIndex = 0;
  std::size_t m_heldCount = 0;
  std::uint64_t m_refusedCount = 0;
};

/// Serial-number comparison over the wire's 16-bit sequence -- true when _candidate is newer than
/// _reference, across the wrap.
///
/// A PLAIN `>` IS WRONG HERE AND THE MATCH IS NOT SHORT ENOUGH TO HIDE IT. The sequence wraps
/// after about 55 minutes at 20 Hz against a match of five (`TechnicalDesign.md` section 4), so a
/// single match never wraps -- but a client that stays up across matches meets 65,535 followed by
/// 0, and a plain comparison would refuse every snapshot of the new match forever. The difference
/// taken in the unsigned type and read as signed is the standard answer and costs one cast.
[[nodiscard]] constexpr bool SequenceIsNewer(std::uint16_t _candidate, std::uint16_t _reference) noexcept
{
  return static_cast<std::int16_t>(static_cast<std::uint16_t>(_candidate - _reference)) > 0;
}

} // namespace Outpost

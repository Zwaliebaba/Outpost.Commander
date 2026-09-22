#pragma once

#include "ReplicaStore.h"

#include "GameCore.h"

#include <cstdint>
#include <span>
#include <vector>

namespace Outpost
{

/// What the client draws the instant a gesture resolves, and clears when the host catches up.
///
/// **THIS IS PRESENTATION AND NOT PREDICTION, AND THE DISTINCTION IS THE ARCHITECTURE** (R19, Q20,
/// `TechnicalDesign.md` section 6). A tap is not visible for 152 milliseconds at best, and on a
/// touchscreen there is no cursor and no hover -- the tap is the only feedback a player gets, so a
/// quarter second of nothing is a dead interface. The client therefore draws the order it just
/// SENT: a destination marker, and a line from the selection.
///
/// **THE SHIPS DO NOT MOVE.** R19 forbids the client simulating; it does not forbid the client
/// drawing what it asked for. Nothing in this file or its neighbors writes an entity's position,
/// and the entity keeps the position the last snapshot gave it until a later snapshot gives it
/// another. This is the step where a well-meaning optimization -- "just move it locally, the host
/// will agree" -- breaks the architecture, and it is the reason the marker is a separate object
/// from the replica rather than a flag on it.
///
/// R8: a public aggregate.
struct OrderMarker
{
  /// Where the order pointed, in wire units -- the same quantized point the command carried, so
  /// the marker cannot drift from the order it represents.
  std::int16_t targetX = 0;
  std::int16_t targetY = 0;

  /// The command's sequence. What clears this marker is a snapshot whose `lastCommandSequenceApplied`
  /// has reached it.
  std::uint16_t commandSequence = 0;

  /// What the line is drawn from. Copied because the selection can change under the player's
  /// finger while an order is still outstanding, and the line belongs to the order rather than to
  /// whatever is selected now.
  std::vector<std::uint16_t> selection;

  [[nodiscard]] friend bool operator==(const OrderMarker&, const OrderMarker&) noexcept = default;
};

/// The markers currently on screen.
class OrderMarkerSet
{
public:
  /// One order, drawn from the moment the gesture resolved.
  void Add(OrderMarker _marker);

  /// Clears every marker the host has now applied.
  ///
  /// **AT OR BEFORE, NOT EQUAL TO.** A snapshot's `lastCommandSequenceApplied` is a high-water mark
  /// and it can jump: three orders sent in one second are acknowledged by one number, and matching
  /// exactly would leave the first two drawn forever. The comparison is serial-number arithmetic
  /// (`SequenceIsNewer`), so it survives the wrap for the same reason the snapshot sequence does.
  ///
  /// Returns how many were cleared, which is what a caller counts rather than recomputes.
  std::size_t ClearAcknowledged(std::uint16_t _lastCommandSequenceApplied) noexcept;

  [[nodiscard]] std::span<const OrderMarker> Markers() const noexcept
  {
    return m_markers;
  }

  [[nodiscard]] std::size_t Count() const noexcept
  {
    return m_markers.size();
  }

  void Clear() noexcept
  {
    m_markers.clear();
  }

private:
  /// In the order they were issued. A handful at most -- a player can only tap so fast, and each
  /// one clears within a round trip.
  std::vector<OrderMarker> m_markers;
};

} // namespace Outpost

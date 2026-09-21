#pragma once

#include "EntityRecord.h"

#include "NeuronCore.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Outpost
{

/// One player's block, and ADR-003 sizes it by the count in the header rather than fixing it --
/// which is what makes the third and fourth player a runtime value instead of a format change.
///
/// EIGHT BYTES, and that is arithmetic rather than a choice: ADR-003 says thirty bytes of header
/// at two players and forty-six at four, so two more players cost sixteen bytes.
struct PlayerBlock
{
  static constexpr std::size_t SIZE_BYTES = 8;

  std::uint32_t credits = 0;
  std::uint16_t lastCommandSequenceApplied = 0;
  std::uint8_t buildingDesign = 0;
  std::uint8_t buildProgressPercent = 0;

  [[nodiscard]] friend constexpr bool operator==(const PlayerBlock&, const PlayerBlock&) noexcept = default;
};

/// ADR-004: shooter 2, target 2, weapon 1, riding the snapshot behind a count byte after the
/// removal list. Not retransmitted -- a lost one costs a missing tracer, which is why it is not
/// worth a reliability path of its own.
struct FireEvent
{
  static constexpr std::size_t SIZE_BYTES = 5;

  std::uint16_t shooter = 0;
  std::uint16_t target = 0;
  std::uint8_t weapon = 0;

  [[nodiscard]] friend constexpr bool operator==(const FireEvent&, const FireEvent&) noexcept = default;
};

/// Why a snapshot would not decode. Distinct values because the caller's response differs: a
/// version mismatch is somebody running an old build and is worth saying out loud once, where the
/// rest are a corrupt or hostile datagram and are worth a counter.
enum class SnapshotFault : std::uint8_t
{
  None,
  /// The datagram ended inside a field.
  Truncated,
  VersionMismatch,
  /// A well-formed packet that is not a snapshot.
  WrongType,
  /// A fragment. ADR-003 puts the two-fragment path at the fourth player and M4.3 writes the
  /// reassembler; until then a fragment is a packet this build drops rather than half-reads.
  Fragmented,
  /// A count in the header that the rest of the datagram cannot possibly satisfy. Refused before
  /// anything is allocated for it, because the count is attacker- or bug-controlled and the
  /// allocation is not.
  ImpossibleCount
};

/// ADR-003's snapshot, whole: the transport header, the fixed header, one block per player, the
/// entity records, the removal list, and the fire events behind their count.
///
/// ENCODED AND DECODED IN FULL AT M0, although M0 has one entity, nothing to remove and nothing
/// firing. Proving the format is what this milestone is for, and the empty lists cost a byte each.
///
/// THE HEADER IS THIRTY BYTES AT TWO PLAYERS, and here is where that divides:
///
///     6  the transport's PacketHeader -- version 1, type 1, sequence 2, fragment index 1, count 1
///     4  tick
///     2  entity count
///     1  player count
///     1  removal count
///     8  per player: credits 4, last applied command sequence 2, building design 1, progress 1
///
/// Six plus eight plus two blocks is thirty; four blocks is forty-six. ADR-003 gives those two
/// totals and the field list; the widths above are this file settling them, and the tick is four
/// bytes because that is what the eight fixed bytes leave once the three counts are placed.
struct Snapshot
{
  /// The transport header's sequence, carried here so a caller sets one thing rather than two.
  std::uint16_t sequence = 0;

  /// The tick this snapshot describes. Four bytes never wraps: 2^32 ticks at twenty a second is
  /// most of a decade, against a match of five minutes.
  std::uint32_t tick = 0;

  std::vector<PlayerBlock> players;
  std::vector<EntityRecord> entities;

  /// Identities, packed as an EntityRecord's is. ADR-003: without this a death is learned by
  /// absence, which works only while the interest set is everything -- and the day fog of war
  /// ships, absence means "died" or "left my view" and every wreck and selection eviction fires
  /// wrongly.
  std::vector<std::uint16_t> removals;

  std::vector<FireEvent> fires;
};

/// What this snapshot will occupy, exactly, without encoding it. The budget's own arithmetic in
/// code, so a test can assert the datagram figure rather than recompute it by hand.
[[nodiscard]] std::size_t EncodedSize(const Snapshot& _snapshot) noexcept;

/// False when the buffer will not hold it, or when a count exceeds what the wire can name. On
/// false the buffer holds a prefix and is to be discarded.
[[nodiscard]] bool Encode(const Snapshot& _snapshot, Neuron::ByteWriter& _writer) noexcept;

/// On anything but SnapshotFault::None, _outSnapshot is left alone.
[[nodiscard]] SnapshotFault Decode(Neuron::ByteReader& _reader, Snapshot& _outSnapshot) noexcept;

} // namespace Outpost

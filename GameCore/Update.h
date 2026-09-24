#pragma once

#include "EntityRecord.h"

#include "NeuronCore.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Outpost
{

/// One player's own block. **An update carries the recipient's block and nobody else's** (ADR-024):
/// the full snapshot carried one per player, which at a hundred players was 800 of the 1,232 bytes
/// before a single entity. What a client draws about a rival is on the rival's entities.
///
/// EIGHT BYTES, unchanged since M0.9: credits 4, last applied command sequence 2, building design 1,
/// progress 1. The command acknowledgment is here, so upstream reliability did not move.
struct PlayerBlock
{
  static constexpr std::size_t SIZE_BYTES = 8;

  std::uint32_t credits = 0;
  std::uint16_t lastCommandSequenceApplied = 0;
  std::uint8_t buildingDesign = 0;
  std::uint8_t buildProgressPercent = 0;

  [[nodiscard]] friend constexpr bool operator==(const PlayerBlock&, const PlayerBlock&) noexcept = default;
};

/// **THE BUILDING-DESIGN BYTE, SINCE Q80** (2026-09-24): its low four bits are the design building plus one, or
/// zero for nothing, and its high four bits how many items wait in the queue behind it, saturating at 15. The
/// design never needed more than three bits, so the queue took the spare ones and the block did not grow.
inline constexpr std::uint8_t BUILDING_DESIGN_BITS = 0x0F;
inline constexpr std::uint32_t QUEUED_ON_WIRE_MAX = 15;

[[nodiscard]] constexpr std::uint8_t PackBuilding(std::uint8_t _designPlusOne, std::uint32_t _queued) noexcept
{
  const std::uint32_t queued = (_queued > QUEUED_ON_WIRE_MAX) ? QUEUED_ON_WIRE_MAX : _queued;
  return static_cast<std::uint8_t>((_designPlusOne & BUILDING_DESIGN_BITS) | (queued << 4));
}

/// The design plus one, or zero for nothing building.
[[nodiscard]] constexpr std::uint8_t BuildingDesignOf(std::uint8_t _byte) noexcept
{
  return static_cast<std::uint8_t>(_byte & BUILDING_DESIGN_BITS);
}

/// How many wait behind it, up to 15.
[[nodiscard]] constexpr std::uint32_t QueuedOf(std::uint8_t _byte) noexcept
{
  return static_cast<std::uint32_t>(_byte) >> 4;
}

/// ADR-004: shooter, target, weapon. **Seven bytes since ADR-024**, because both identities are three,
/// and **repeated for `FIRE_REPEAT_TICKS` consecutive updates** so a lost datagram costs a tracer only
/// when three in a row are lost.
struct FireEvent
{
  static constexpr std::size_t SIZE_BYTES = 7;

  WireIdentity shooter = NO_WIRE_IDENTITY;
  WireIdentity target = NO_WIRE_IDENTITY;
  std::uint8_t weapon = 0;

  [[nodiscard]] friend constexpr bool operator==(const FireEvent&, const FireEvent&) noexcept = default;
};

/// ADR-003's pin, which ADR-024 kept: the IPv6 minimum-MTU payload exactly, 1,280 less 40 and 8.
/// **Every update is at most this and is always one datagram**; an update that would be larger is
/// one the encoder refuses, not one the transport splits.
inline constexpr std::size_t UPDATE_PAYLOAD_BYTES = 1232;

/// The transport's four, tick 4, live entity count 2, the recipient's block 8, and a count byte each
/// for records, removals and fire events. **Twenty-one at any player count**, which is the point.
inline constexpr std::size_t UPDATE_HEADER_BYTES = Neuron::PacketHeader::SIZE_BYTES + 4 + 2 + PlayerBlock::SIZE_BYTES + 3;
static_assert(UPDATE_HEADER_BYTES == 21, "ADR-024 states the update header as twenty-one bytes.");

/// A removal is a packed identity, generation included.
inline constexpr std::size_t REMOVAL_BYTES = 3;

/// **HOW MANY OF EACH REPEATED FACT ONE UPDATE MAY CARRY.** Bounded so that the records an update can
/// always hold -- `MIN_RECORDS_PER_UPDATE` -- is a constant both sides can compute the sweep from. A
/// removal or event past the cap waits a tick; it is repeated for several anyway. Neither figure is
/// reached at M1, which destroys nothing and fires nothing; both are the first things a barrage at M3
/// would move, and the budget skill says so.
inline constexpr std::size_t MAX_REMOVALS_PER_UPDATE = 48;
inline constexpr std::size_t MAX_FIRES_PER_UPDATE = 40;

/// The records an update holds with the removal and fire sections full, which is the floor the sweep
/// guarantee is computed against: 65 at the pinned payload.
inline constexpr std::size_t MIN_RECORDS_PER_UPDATE =
  (UPDATE_PAYLOAD_BYTES - UPDATE_HEADER_BYTES - (MAX_REMOVALS_PER_UPDATE * REMOVAL_BYTES) -
   (MAX_FIRES_PER_UPDATE * FireEvent::SIZE_BYTES)) /
  EntityRecord::SIZE_BYTES;
static_assert(MIN_RECORDS_PER_UPDATE == 65);

/// The records an update holds with nothing else in it: 100.
inline constexpr std::size_t MAX_RECORDS_PER_UPDATE = (UPDATE_PAYLOAD_BYTES - UPDATE_HEADER_BYTES) / EntityRecord::SIZE_BYTES;

/// **THE PER-CLIENT CAP: TWO WHOLE UPDATES A TICK** (ADR-024, `OpenQuestions.md` Q49). Each is complete
/// and separately renderable, which is what makes a second one more refresh rather than fragmentation.
/// Here rather than on the host because the client computes the sweep from it.
inline constexpr std::size_t UPDATES_PER_TICK = 2;

/// **A DEATH RIDES TEN CONSECUTIVE UPDATES AND A SHOT RIDES THREE** (ADR-024). Losing a death then needs
/// ten losses in a row. The client ignores a removal it has already applied, so the repeats are free
/// to receive.
inline constexpr std::uint32_t REMOVAL_REPEAT_TICKS = 10;
inline constexpr std::uint32_t FIRE_REPEAT_TICKS = 3;

/// **THE SWEEP: HOW LONG ANY LIVE ENTITY MAY GO UNSENT TO A CLIENT, IN TICKS.** The host guarantees it
/// by putting any entity unsent this long ahead of every score; the client uses it to decide when an
/// entity it has heard nothing about is gone. Computed from the guaranteed floor rather than the typical
/// fill, because a guarantee computed from a typical figure is not one. One tick at the MVP's 110.
[[nodiscard]] constexpr std::uint32_t SweepTicks(std::size_t _liveEntityCount) noexcept
{
  constexpr std::size_t PER_TICK = MIN_RECORDS_PER_UPDATE * UPDATES_PER_TICK;
  const std::size_t ticks = (_liveEntityCount + PER_TICK - 1) / PER_TICK;
  return static_cast<std::uint32_t>((ticks == 0) ? 1 : ticks);
}

/// **A CLIENT FORGETS AN ENTITY THAT HAS GONE THIS MANY SWEEPS WITHOUT A RECORD.** The only way an
/// entity leaves a client without a removal, and what makes a rejoin correct: the removals of whatever
/// died while the client was away have stopped repeating, and those entities are never sent again.
inline constexpr std::uint32_t FORGET_AFTER_SWEEPS = 3;

/// Why an update would not decode. Distinct values because the caller's response differs: a version
/// mismatch is somebody running an old build and is worth saying out loud once, where the rest are a
/// corrupt or hostile datagram and are worth a counter.
enum class UpdateFault : std::uint8_t
{
  None,
  /// The datagram ended inside a field.
  Truncated,
  VersionMismatch,
  /// A well-formed packet that is not an update.
  WrongType,
  /// A count that the rest of the datagram cannot possibly satisfy, or one past its cap. Refused before
  /// anything is allocated for it, because the count is attacker- or bug-controlled and the allocation
  /// is not.
  ImpossibleCount
};

/// ADR-024's update: the entity records a client is due at one tick, and the repeated facts riding
/// along. **A BAG, NOT THE WORLD.** An entity absent from an update is not dead and not unchanged; it
/// was simply not due. Death is a removal and nothing else.
///
/// The layout, in order: transport 4, tick 4, live entity count 2, the recipient's block 8, record
/// count 1, removal count 1, fire count 1 -- then the records, the removals and the fire events.
struct Update
{
  /// The transport header's sequence. Per recipient, and it orders nothing: a client counts loss with
  /// it. Ordering is per entity, by `tick`.
  std::uint16_t sequence = 0;

  /// The tick every record in this update describes. Four bytes never wraps: 2^32 ticks at twenty a
  /// second is most of a decade, against a match of five minutes.
  std::uint32_t tick = 0;

  /// How many entities the host has alive, whether or not they are in this update -- which is what lets
  /// the client compute the sweep and so know when an entity it has not heard about is gone.
  std::uint16_t liveEntityCount = 0;

  PlayerBlock own;

  std::vector<EntityRecord> records;
  std::vector<WireIdentity> removals;
  std::vector<FireEvent> fires;
};

/// What this update will occupy, exactly, without encoding it. The budget's own arithmetic in code,
/// so a test can assert the figure rather than recompute it.
[[nodiscard]] std::size_t EncodedSize(const Update& _update) noexcept;

/// False when the buffer will not hold it, when a count is past its cap, or when it would exceed
/// `UPDATE_PAYLOAD_BYTES` -- **the one-datagram property, refused here rather than hoped for.** On
/// false the buffer holds a prefix and is to be discarded.
[[nodiscard]] bool Encode(const Update& _update, Neuron::ByteWriter& _writer) noexcept;

/// On anything but UpdateFault::None, _outUpdate is left alone.
[[nodiscard]] UpdateFault Decode(Neuron::ByteReader& _reader, Update& _outUpdate) noexcept;

} // namespace Outpost

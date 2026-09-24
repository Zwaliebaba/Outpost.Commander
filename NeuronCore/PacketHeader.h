#pragma once

#include "ByteReader.h"
#include "ByteWriter.h"

#include <cstddef>
#include <cstdint>

namespace Neuron
{

/// The version of the wire format this build speaks. It is the whole of the protection against a
/// mismatched build (TechnicalDesign.md section 5): there is no negotiation and no fallback, a
/// packet from another version is dropped, and this number goes up whenever any record on the
/// wire changes shape.
///
/// **1 UNTIL M1.4, WHEN THE JOIN ARRIVED, AND 3 SINCE M1.6's BUILD ORDERS.** Neither change altered
/// an existing record, so neither had to move this -- and both did, for one reason. A build that
/// does not know a type refuses the packet CARRYING it: a version-2 host handed a command packet
/// with a `Build` in it drops the move orders riding alongside, which is worse than not talking at
/// all. The version is what turns that into a clean drop.
///
/// **A MISMATCH IS STILL A SILENCE, INCLUDING FOR THE JOIN** (ADR-013). The client shows the same
/// overlay it shows for a host that is not running, which is the cost that decision names.
///
/// **4 SINCE M1.14c**, which changed every record at once (ADR-024): this header lost its fragment
/// fields, the snapshot became an update, the entity record grew to twelve bytes and a command packet
/// gained the view. The one change in this tree that no build before it can read a byte of.
///
/// **5 SINCE M2.3**, when the join reply gained the match's player count (ADR-013 amended): the field is
/// derived from the seed AND the count, and a version-4 client reading a version-5 reply would take the
/// count's byte as the first of its token. **M2.6's `Mine` command type and M2.7's third cargo bit ride the
/// same version**: none of the three has left this branch, so one step covers them all.
///
/// **6 SINCE Q80** (2026-09-24), when the per-player block's building-design byte gained the build queue's
/// length in its high four bits. No size moved; a version-5 client would read a queued Miner as an unknown
/// design, which is why a change in meaning is a version like a change in size.
///
/// **7 SINCE M3.8** (2026-09-24, Q70), when the host began ending matches: a third record type on the client's
/// socket, `MatchEnded`, which a version-6 client would count as a fault and never rejoin from.
inline constexpr std::uint8_t PROTOCOL_VERSION = 7;

/// What a datagram carries. The three the design names, and the pair ADR-013 added.
///
/// **`Update` IS WHAT `Snapshot` WAS, AT THE SAME VALUE** (ADR-024). The unit changed from the world at
/// a tick to a bag of entity records at a tick, and the name follows it; the value stays 1 because a
/// type is a number on the wire and nothing about that number needed to move.
///
/// ZERO IS NOT A TYPE, deliberately. A header that nobody set, a zero-filled buffer and a
/// zero-length datagram all decode to type 0, and none of them should ever look like a heartbeat.
enum class PacketType : std::uint8_t
{
  Update = 1,
  Command = 2,
  Heartbeat = 3,

  /// A client asking to be told which player it is (ADR-013). It carries a session token or zero,
  /// and the host answers every one of them -- including a repeat from a client that has already
  /// joined, which is what makes a lost reply cost a retry rather than a slot.
  Join = 4,
  JoinReply = 5,

  /// The host telling a seated client that the match is over and another has begun (`OpenQuestions.md` Q70).
  /// Repeated for ten ticks, because nothing acknowledges it.
  MatchEnded = 6
};

/// True for a type this build knows. A further type means a line here; there is no switch to
/// forget to extend, because a switch would not have warned about it either.
[[nodiscard]] constexpr bool IsKnown(PacketType _type) noexcept
{
  return (_type == PacketType::Update) || (_type == PacketType::Command) || (_type == PacketType::Heartbeat) ||
         (_type == PacketType::Join) || (_type == PacketType::JoinReply) || (_type == PacketType::MatchEnded);
}

/// Why a header would not decode. Distinct values because the caller's response differs: a
/// version mismatch is somebody running an old build and is worth saying out loud once, where
/// the other two are a corrupt or hostile datagram and are worth nothing but a counter.
enum class PacketFault : std::uint8_t
{
  None,
  Truncated,
  VersionMismatch,
  UnknownType
};

/// The framing every datagram opens with, on both sides of the wire.
///
/// It lives in the engine because framing knows nothing about the game (R9) -- the update's own
/// header, with the tick and the entity count and the recipient's block, is a GameCore record that
/// begins after this one ends.
///
/// **FOUR BYTES: VERSION, TYPE AND A SEQUENCE, AND NO FRAGMENT FIELDS** (ADR-024). M0.2 reserved an
/// index and a count so that the fourth player's two-datagram snapshot would be a fast path rather
/// than a format change. ADR-024 made every datagram whole instead -- a bag of self-contained records
/// that is meaningful without any other -- so there is nothing left to reassemble and the two bytes
/// went back into the payload. **Do not bring them back for later**: a datagram that would not fit is
/// a datagram the host's accumulator does not send.
///
/// **THE SEQUENCE ORDERS NOTHING.** A receiver uses it to count loss, which is the stress harness's
/// figure (ADR-022). Ordering is per entity, by the update's tick, and lives in `GameClient`.
///
/// R8: a wire record, so plain fields and brace initialization. Use designated initializers --
/// the version defaults to this build's, so `PacketHeader{.type = PacketType::Update, .sequence = n}`
/// is the ordinary case.
struct PacketHeader
{
  /// Four bytes: version 1, type 1, sequence 2.
  static constexpr std::size_t SIZE_BYTES = 4;

  std::uint8_t protocolVersion = PROTOCOL_VERSION;
  PacketType type{};
  std::uint16_t sequence = 0;

  /// Lays the header down and leaves the writer positioned for the payload. False on a header
  /// that is not sendable -- an unset or unknown type -- and on a buffer with no room, in which case
  /// nothing was written (see ByteWriter).
  [[nodiscard]] bool Write(ByteWriter& _writer) const noexcept;

  /// Lifts a header and leaves the reader positioned at the payload. On anything but
  /// PacketFault::None the reader's cursor is not to be trusted and the datagram is to be
  /// dropped; _outHeader is left alone.
  [[nodiscard]] static PacketFault Read(ByteReader& _reader, PacketHeader& _outHeader) noexcept;
};

} // namespace Neuron

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
inline constexpr std::uint8_t PROTOCOL_VERSION = 1;

/// What a datagram carries. The three the design names, and no more.
///
/// ZERO IS NOT A TYPE, deliberately. A header that nobody set, a zero-filled buffer and a
/// zero-length datagram all decode to type 0, and none of them should ever look like a heartbeat.
enum class PacketType : std::uint8_t
{
  Snapshot = 1,
  Command = 2,
  Heartbeat = 3
};

/// True for a type this build knows. A fourth type means a line here; there is no switch to
/// forget to extend, because a switch would not have warned about it either.
[[nodiscard]] constexpr bool IsKnown(PacketType _type) noexcept
{
  return _type == PacketType::Snapshot || _type == PacketType::Command || _type == PacketType::Heartbeat;
}

/// Why a header would not decode. Distinct values because the caller's response differs: a
/// version mismatch is somebody running an old build and is worth saying out loud once, where
/// the other three are a corrupt or hostile datagram and are worth nothing but a counter.
enum class PacketFault : std::uint8_t
{
  None,
  Truncated,
  VersionMismatch,
  UnknownType,
  BadFragmentation
};

/// The framing every datagram opens with, on both sides of the wire.
///
/// It lives in the engine because framing knows nothing about the game (R9) -- the snapshot's own
/// header, with the tick and the entity count and the per-player blocks, is a GameCore record
/// that begins after this one ends.
///
/// THE FRAGMENT FIELDS ARE HERE FROM THE FIRST PACKET ALTHOUGH THE MVP NEVER FRAGMENTS.
/// ADR-003 puts the two-fragment path at the fourth player, and reassembly itself is M4.3. A
/// field that is always there is a fast path; a field added later is a format change, and this
/// one would land in the milestone that is already busy proving four players work at all.
///
/// R8: a wire record, so plain fields and brace initialization. Use designated initializers --
/// the version defaults to this build's and the count defaults to the one fragment the MVP always
/// sends, so `PacketHeader{.type = PacketType::Snapshot, .sequence = n}` is the ordinary case.
struct PacketHeader
{
  /// Six bytes: version 1, type 1, sequence 2, fragment index 1, fragment count 1.
  static constexpr std::size_t SIZE_BYTES = 6;

  std::uint8_t protocolVersion = PROTOCOL_VERSION;
  PacketType type{};
  std::uint16_t sequence = 0;
  std::uint8_t fragmentIndex = 0;
  std::uint8_t fragmentCount = 1;

  /// Lays the header down and leaves the writer positioned for the payload. False on a header
  /// that is not sendable -- an unset or unknown type, or fragment fields that contradict each
  /// other -- and on a buffer with no room, in which case nothing was written (see ByteWriter).
  [[nodiscard]] bool Write(ByteWriter& _writer) const noexcept;

  /// Lifts a header and leaves the reader positioned at the payload. On anything but
  /// PacketFault::None the reader's cursor is not to be trusted and the datagram is to be
  /// dropped; _outHeader is left alone.
  [[nodiscard]] static PacketFault Read(ByteReader& _reader, PacketHeader& _outHeader) noexcept;

  /// True when this datagram is the whole packet and its payload can be decoded where it lies.
  /// The MVP is always this, which is why there is no reassembler to consult: a false answer is
  /// a packet this build drops, until M4.3 gives it somewhere to go.
  [[nodiscard]] bool IsSingleFragment() const noexcept
  {
    return fragmentCount == 1;
  }
};

} // namespace Neuron

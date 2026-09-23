#pragma once

#include "ByteReader.h"
#include "ByteWriter.h"
#include "PacketHeader.h"

#include <cstddef>
#include <cstdint>

namespace Neuron
{

/// M0.5 SCAFFOLDING. THIS FILE IS MEANT TO BE DELETED.
///
/// The gate under M0 is a measurement rather than a feature (`Design/Plan/M0-the-wire.md`), and
/// its step adds no product code: a temporary host mode that sends a numbered packet at a fixed
/// rate, and a client that logs what arrives. This record is what the two agree on, and it exists
/// here rather than twice because a figure with two homes is the defect AGENTS.md section 6 names.
/// When the gate has its four answers, delete this file, its suite, and the two probe modes.
///
/// It is a Heartbeat on the wire -- a real PacketType, so the host and the client are exercising
/// the framing M0.2 pinned rather than a private format that proves nothing about it.
///
/// R8: a wire record, so plain fields and brace initialization.
struct ProbePacket
{
  /// The header's four bytes and one more field. Well under any MTU; this is not a size test.
  static constexpr std::size_t SIZE_BYTES = PacketHeader::SIZE_BYTES + 8;

  /// Below the Windows ephemeral range, which starts at 49152, so that a fixed bind cannot
  /// collide with the ephemeral port an outbound socket was handed. It is NOT a decision about
  /// where the host listens: M0.11 makes the address and the port configuration, and ADR-008 owns
  /// the address today.
  static constexpr std::uint16_t PORT = 49000;

  /// The design's tick (ADR-002), because the point of the measurement is loss and jitter at the
  /// rate the game will actually send at rather than at a rate chosen to flatter it.
  static constexpr std::uint32_t RATE_HZ = 20;

  /// The header's sequence, lifted out so a reader of a log does not have to know that.
  std::uint16_t sequence = 0;

  /// The HOST's clock when it sent, in milliseconds since its probe started. It is not the
  /// client's clock and the two are never compared directly -- an absolute one-way delay would
  /// need them synchronized, and nothing here synchronizes them. What survives the offset is the
  /// DIFFERENCE between two packets' transits, which is what jitter is.
  std::uint64_t sentAtMs = 0;

  /// Lays the header and the payload down. False on a buffer with no room, in which case nothing
  /// usable was written (see ByteWriter).
  [[nodiscard]] bool Write(ByteWriter& _writer) const noexcept;

  /// Lifts one. On anything but PacketFault::None the datagram is to be dropped and _outPacket is
  /// left alone, which is the contract PacketHeader::Read already states.
  [[nodiscard]] static PacketFault Read(ByteReader& _reader, ProbePacket& _outPacket) noexcept;
};

} // namespace Neuron

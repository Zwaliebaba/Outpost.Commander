// A shared-items translation unit: this file is compiled into every project that imports
// NeuronCore.vcxitems, each of which has its own precompiled header. There is no one pch to share
// between them, so these files use none and include their master header first instead.

#include "NeuronCore.h"

#include "PacketHeader.h"

namespace Neuron
{

bool PacketHeader::Write(ByteWriter& _writer) const noexcept
{
  // Refused before a byte is written, so that a header nobody finished composing cannot leave the
  // machine. The default-constructed type is 0, which IsKnown rejects, so "forgot to set it" is
  // caught at the send site rather than by the peer.
  if (!IsKnown(type))
  {
    return false;
  }

  _writer.WriteUInt8(protocolVersion);
  _writer.WriteUInt8(static_cast<std::uint8_t>(type));
  _writer.WriteUInt16(sequence);
  return !_writer.Faulted();
}

PacketFault PacketHeader::Read(ByteReader& _reader, PacketHeader& _outHeader) noexcept
{
  // A reader that has already run out reports how far it honestly got, so its RemainingBytes can
  // still look sufficient while every read returns zero. Asking it first is what stops a header
  // being decoded out of that zero and reported as a version mismatch with somebody's build.
  if (_reader.Faulted() || _reader.RemainingBytes() < SIZE_BYTES)
  {
    return PacketFault::Truncated;
  }

  const std::uint8_t version = _reader.ReadUInt8();

  // The version is checked before any other field is INTERPRETED, which is not the same as being
  // read. Another version is another layout: the bytes after this one may not be a type and a
  // sequence at all -- version 3 had two fragment fields behind them -- so calling them a bad type
  // would name a fault that is not there and send somebody hunting corruption.
  if (version != PROTOCOL_VERSION)
  {
    return PacketFault::VersionMismatch;
  }

  const auto type = static_cast<PacketType>(_reader.ReadUInt8());
  const std::uint16_t sequence = _reader.ReadUInt16();

  if (!IsKnown(type))
  {
    return PacketFault::UnknownType;
  }

  _outHeader.protocolVersion = version;
  _outHeader.type = type;
  _outHeader.sequence = sequence;
  return PacketFault::None;
}

} // namespace Neuron

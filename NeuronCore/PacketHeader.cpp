// A shared-items translation unit: this file is compiled into every project that imports
// NeuronCore.vcxitems, each of which has its own precompiled header. There is no one pch to share
// between them, so these files use none and include their master header first instead.

#include "NeuronCore.h"

#include "PacketHeader.h"

namespace Neuron
{

namespace
{
/// The two ways the fragment fields can contradict each other. A packet is at least one fragment,
/// and an index names one of them.
[[nodiscard]] bool FragmentationIsCoherent(std::uint8_t _index, std::uint8_t _count) noexcept
{
  return _count != 0 && _index < _count;
}
} // namespace

bool PacketHeader::Write(ByteWriter& _writer) const noexcept
{
  // Refused before a byte is written, so that a header nobody finished composing cannot leave the
  // machine. The default-constructed type is 0, which IsKnown rejects, so "forgot to set it" is
  // caught at the send site rather than by the peer.
  if (!IsKnown(type) || !FragmentationIsCoherent(fragmentIndex, fragmentCount))
  {
    return false;
  }

  _writer.WriteUInt8(protocolVersion);
  _writer.WriteUInt8(static_cast<std::uint8_t>(type));
  _writer.WriteUInt16(sequence);
  _writer.WriteUInt8(fragmentIndex);
  _writer.WriteUInt8(fragmentCount);
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
  // read. Another version is another layout: the five bytes after this one may not be a type, a
  // sequence and two fragment fields at all, so calling them a bad type or an incoherent fragment
  // count would name a fault that is not there and send somebody hunting corruption.
  if (version != PROTOCOL_VERSION)
  {
    return PacketFault::VersionMismatch;
  }

  const auto type = static_cast<PacketType>(_reader.ReadUInt8());
  const std::uint16_t sequence = _reader.ReadUInt16();
  const std::uint8_t fragmentIndex = _reader.ReadUInt8();
  const std::uint8_t fragmentCount = _reader.ReadUInt8();

  if (!IsKnown(type))
  {
    return PacketFault::UnknownType;
  }
  if (!FragmentationIsCoherent(fragmentIndex, fragmentCount))
  {
    return PacketFault::BadFragmentation;
  }

  _outHeader.protocolVersion = version;
  _outHeader.type = type;
  _outHeader.sequence = sequence;
  _outHeader.fragmentIndex = fragmentIndex;
  _outHeader.fragmentCount = fragmentCount;
  return PacketFault::None;
}

} // namespace Neuron

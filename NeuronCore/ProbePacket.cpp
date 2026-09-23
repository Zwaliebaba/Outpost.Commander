// A shared-items translation unit -- see NeuronCore.cpp for why it carries no precompiled header.

#include "ProbePacket.h"

namespace Neuron
{

bool ProbePacket::Write(ByteWriter& _writer) const noexcept
{
  const PacketHeader header{.type = PacketType::Heartbeat, .sequence = sequence};
  if (!header.Write(_writer))
  {
    return false;
  }
  return _writer.WriteUInt64(sentAtMs);
}

PacketFault ProbePacket::Read(ByteReader& _reader, ProbePacket& _outPacket) noexcept
{
  PacketHeader header{};
  const PacketFault fault = PacketHeader::Read(_reader, header);
  if (fault != PacketFault::None)
  {
    return fault;
  }

  // An Update or a Command reaching the probe is a real build talking to it, and that is worth
  // refusing by name rather than decoding eight bytes of something else as a timestamp.
  if (header.type != PacketType::Heartbeat)
  {
    return PacketFault::UnknownType;
  }

  const std::uint64_t sentAtMs = _reader.ReadUInt64();
  if (_reader.Faulted())
  {
    return PacketFault::Truncated;
  }

  _outPacket.sequence = header.sequence;
  _outPacket.sentAtMs = sentAtMs;
  return PacketFault::None;
}

} // namespace Neuron

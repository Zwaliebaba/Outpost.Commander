// A shared-items translation unit -- see NeuronCore.cpp for why it carries no precompiled header.

#include "Join.h"

namespace Outpost
{

namespace
{
/// The header read, reduced to the one fault this file reports. Both records open the same way and
/// the duplication is what a second copy of this switch would have been.
[[nodiscard]] JoinFault ReadHeader(Neuron::ByteReader& _reader, Neuron::PacketType _expected) noexcept
{
  Neuron::PacketHeader header{};
  switch (Neuron::PacketHeader::Read(_reader, header))
  {
  case Neuron::PacketFault::None:
    break;
  case Neuron::PacketFault::VersionMismatch:
    return JoinFault::VersionMismatch;
  case Neuron::PacketFault::Truncated:
    return JoinFault::Truncated;
  default:
    return JoinFault::WrongType;
  }

  if (header.type != _expected)
  {
    return JoinFault::WrongType;
  }
  return JoinFault::None;
}

/// THE SEQUENCE FIELD IS ZERO ON BOTH RECORDS AND THAT IS DELIBERATE. The transport's sequence
/// orders a stream; a join is not a stream. Nothing reads it, so nothing is invited to start.
inline constexpr std::uint16_t NO_SEQUENCE = 0;
} // namespace

bool Encode(const Join& _join, Neuron::ByteWriter& _writer) noexcept
{
  const Neuron::PacketHeader header{.type = Neuron::PacketType::Join, .sequence = NO_SEQUENCE};
  if (!header.Write(_writer))
  {
    return false;
  }

  static_cast<void>(_writer.WriteUInt64(_join.token));
  return !_writer.Faulted();
}

JoinFault Decode(Neuron::ByteReader& _reader, Join& _outJoin) noexcept
{
  const JoinFault fault = ReadHeader(_reader, Neuron::PacketType::Join);
  if (fault != JoinFault::None)
  {
    return fault;
  }

  const SessionToken token = _reader.ReadUInt64();
  if (_reader.Faulted())
  {
    return JoinFault::Truncated;
  }

  _outJoin = Join{.token = token};
  return JoinFault::None;
}

bool Encode(const JoinReply& _reply, Neuron::ByteWriter& _writer) noexcept
{
  // Refused before a byte is written, the way an unknown command type is: a result this build
  // cannot name is a bug on this side rather than something to send.
  if (!IsKnown(_reply.result))
  {
    return false;
  }

  const Neuron::PacketHeader header{.type = Neuron::PacketType::JoinReply, .sequence = NO_SEQUENCE};
  if (!header.Write(_writer))
  {
    return false;
  }

  static_cast<void>(_writer.WriteUInt8(static_cast<std::uint8_t>(_reply.result)));
  static_cast<void>(_writer.WriteUInt8(_reply.player));
  static_cast<void>(_writer.WriteUInt8(_reply.playerCount));
  static_cast<void>(_writer.WriteUInt64(_reply.token));
  static_cast<void>(_writer.WriteUInt64(_reply.matchSeed));
  return !_writer.Faulted();
}

JoinFault Decode(Neuron::ByteReader& _reader, JoinReply& _outReply) noexcept
{
  const JoinFault fault = ReadHeader(_reader, Neuron::PacketType::JoinReply);
  if (fault != JoinFault::None)
  {
    return fault;
  }

  const std::uint8_t result = _reader.ReadUInt8();
  const PlayerId player = _reader.ReadUInt8();
  const std::uint8_t playerCount = _reader.ReadUInt8();
  const SessionToken token = _reader.ReadUInt64();
  const std::uint64_t seed = _reader.ReadUInt64();
  if (_reader.Faulted())
  {
    return JoinFault::Truncated;
  }

  const JoinResult lifted = static_cast<JoinResult>(result);
  if (!IsKnown(lifted))
  {
    return JoinFault::Malformed;
  }

  _outReply = JoinReply{.result = lifted, .player = player, .playerCount = playerCount, .token = token, .matchSeed = seed};
  return JoinFault::None;
}

bool Encode(const MatchEnded& _ended, Neuron::ByteWriter& _writer) noexcept
{
  const Neuron::PacketHeader header{.type = Neuron::PacketType::MatchEnded, .sequence = NO_SEQUENCE};
  if (!header.Write(_writer))
  {
    return false;
  }

  static_cast<void>(_writer.WriteUInt16(_ended.matchNumber));
  static_cast<void>(_writer.WriteUInt8(_ended.winner));
  static_cast<void>(_writer.WriteUInt8(_ended.onClock ? 1 : 0));
  return !_writer.Faulted();
}

JoinFault Decode(Neuron::ByteReader& _reader, MatchEnded& _outEnded) noexcept
{
  const JoinFault fault = ReadHeader(_reader, Neuron::PacketType::MatchEnded);
  if (fault != JoinFault::None)
  {
    return fault;
  }

  const std::uint16_t matchNumber = _reader.ReadUInt16();
  const PlayerId winner = _reader.ReadUInt8();
  const std::uint8_t onClock = _reader.ReadUInt8();
  if (_reader.Faulted())
  {
    return JoinFault::Truncated;
  }
  if (onClock > 1)
  {
    return JoinFault::Malformed;
  }

  _outEnded = MatchEnded{.matchNumber = matchNumber, .winner = winner, .onClock = onClock != 0};
  return JoinFault::None;
}

} // namespace Outpost

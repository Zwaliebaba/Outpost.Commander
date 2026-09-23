// A shared-items translation unit -- see NeuronCore.cpp for why it carries no precompiled header.

#include "Command.h"

#include <utility>

namespace Outpost
{

std::size_t EncodedSize(const Command& _command) noexcept
{
  return Command::FIXED_BYTES + (_command.selection.size() * Command::IDENTITY_BYTES);
}

std::size_t EncodedSize(const CommandPacket& _packet) noexcept
{
  std::size_t total = CommandPacket::HEADER_BYTES;
  for (const Command& command : _packet.commands)
  {
    total += EncodedSize(command);
  }
  return total;
}

bool Encode(const CommandPacket& _packet, Neuron::ByteWriter& _writer) noexcept
{
  if (_packet.commands.size() > CommandPacket::MAX_COMMANDS)
  {
    return false;
  }
  for (const Command& command : _packet.commands)
  {
    // Refused before a byte is written. An unknown type on the wire is a packet the other side
    // cannot act on, and writing one is a bug on this side rather than something to send.
    if (!IsKnown(command.type) || (command.selection.size() > Command::MAX_SELECTION))
    {
      return false;
    }
  }

  const Neuron::PacketHeader header{.type = Neuron::PacketType::Command, .sequence = _packet.sequence};
  if (!header.Write(_writer))
  {
    return false;
  }

  static_cast<void>(_writer.WriteUInt8(_packet.player));
  static_cast<void>(_writer.WriteUInt8(static_cast<std::uint8_t>(_packet.commands.size())));
  static_cast<void>(_writer.WriteInt16(_packet.viewX));
  static_cast<void>(_writer.WriteInt16(_packet.viewY));
  static_cast<void>(_writer.WriteUInt16(_packet.viewRadiusUnits));

  for (const Command& command : _packet.commands)
  {
    static_cast<void>(_writer.WriteUInt16(command.sequence));
    static_cast<void>(_writer.WriteUInt8(static_cast<std::uint8_t>(command.type)));
    static_cast<void>(_writer.WriteInt16(command.targetX));
    static_cast<void>(_writer.WriteInt16(command.targetY));
    static_cast<void>(_writer.WriteUInt8(static_cast<std::uint8_t>(command.selection.size())));
    for (const WireIdentity selected : command.selection)
    {
      static_cast<void>(_writer.WriteUInt24(selected));
    }
  }

  return !_writer.Faulted();
}

CommandFault Decode(Neuron::ByteReader& _reader, CommandPacket& _outPacket) noexcept
{
  Neuron::PacketHeader header{};
  switch (Neuron::PacketHeader::Read(_reader, header))
  {
  case Neuron::PacketFault::None:
    break;
  case Neuron::PacketFault::VersionMismatch:
    return CommandFault::VersionMismatch;
  case Neuron::PacketFault::Truncated:
    return CommandFault::Truncated;
  default:
    return CommandFault::WrongType;
  }

  if (header.type != Neuron::PacketType::Command)
  {
    return CommandFault::WrongType;
  }

  const std::uint8_t player = _reader.ReadUInt8();
  const std::uint8_t commandCount = _reader.ReadUInt8();
  const std::int16_t viewX = _reader.ReadInt16();
  const std::int16_t viewY = _reader.ReadInt16();
  const std::uint16_t viewRadiusUnits = _reader.ReadUInt16();
  if (_reader.Faulted())
  {
    return CommandFault::Truncated;
  }

  CommandPacket lifted;
  lifted.sequence = header.sequence;
  lifted.player = player;
  lifted.viewX = viewX;
  lifted.viewY = viewY;
  lifted.viewRadiusUnits = viewRadiusUnits;

  // NOT RESERVED FROM THE COUNT. Every command is at least `Command::FIXED_BYTES`, so a count the rest
  // of the datagram cannot hold is refused here rather than turned into an allocation.
  if ((static_cast<std::size_t>(commandCount) * Command::FIXED_BYTES) > _reader.RemainingBytes())
  {
    return CommandFault::Malformed;
  }
  lifted.commands.reserve(commandCount);

  for (std::uint8_t index = 0; index < commandCount; ++index)
  {
    Command command{};
    command.sequence = _reader.ReadUInt16();
    const std::uint8_t type = _reader.ReadUInt8();
    command.targetX = _reader.ReadInt16();
    command.targetY = _reader.ReadInt16();
    const std::uint8_t selectionCount = _reader.ReadUInt8();
    if (_reader.Faulted())
    {
      return CommandFault::Truncated;
    }

    command.type = static_cast<CommandType>(type);
    if (!IsKnown(command.type))
    {
      return CommandFault::Malformed;
    }

    // THE COUNT IS CHECKED AGAINST WHAT IS LEFT BEFORE ANYTHING IS RESERVED FOR IT -- a selection
    // count is whatever the datagram said, and ADR-003's whole amplification argument is that a
    // packet can name far more identities than any match contains.
    if ((static_cast<std::size_t>(selectionCount) * Command::IDENTITY_BYTES) > _reader.RemainingBytes())
    {
      return CommandFault::Malformed;
    }

    command.selection.reserve(selectionCount);
    for (std::uint8_t selected = 0; selected < selectionCount; ++selected)
    {
      command.selection.push_back(_reader.ReadUInt24());
    }
    lifted.commands.push_back(std::move(command));
  }

  if (_reader.Faulted())
  {
    return CommandFault::Truncated;
  }

  _outPacket = std::move(lifted);
  return CommandFault::None;
}

std::size_t FillOldestFirst(const std::vector<Command>& _outstanding, std::size_t _payloadBytes, CommandPacket& _outPacket)
{
  _outPacket.commands.clear();

  std::size_t used = CommandPacket::HEADER_BYTES;
  std::size_t packed = 0;
  for (const Command& command : _outstanding)
  {
    const std::size_t cost = EncodedSize(command);

    // STOP, do not skip. Skipping a command that will not fit and taking a later one would leave
    // a gap in the sequence, and the host discards a gap rather than waiting for it -- so the
    // skipped order would be lost rather than delayed.
    if (((used + cost) > _payloadBytes) || (packed >= CommandPacket::MAX_COMMANDS))
    {
      break;
    }

    used += cost;
    ++packed;
    _outPacket.commands.push_back(command);
  }

  return packed;
}

Neuron::Vec2 ClampToPlayArea(const Neuron::Vec2& _point) noexcept
{
  const auto clamp = [](Neuron::Fixed _value) noexcept
  {
    if (_value > PLAY_AREA_HALF_EXTENT)
    {
      return PLAY_AREA_HALF_EXTENT;
    }
    if (_value < -PLAY_AREA_HALF_EXTENT)
    {
      return static_cast<Neuron::Fixed>(-PLAY_AREA_HALF_EXTENT);
    }
    return _value;
  };

  return Neuron::Vec2{.x = clamp(_point.x), .y = clamp(_point.y)};
}

} // namespace Outpost

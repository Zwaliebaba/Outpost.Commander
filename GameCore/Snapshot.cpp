// A shared-items translation unit -- see NeuronCore.cpp for why it carries no precompiled header.

#include "Snapshot.h"

#include <limits>
#include <utility>

namespace Outpost
{

namespace
{
/// The fixed part behind the transport's six bytes: tick 4, entity count 2, player count 1,
/// removal count 1.
inline constexpr std::size_t FIXED_HEADER_BYTES = 8;

/// Counts are bytes or a short on the wire, so a snapshot cannot name more than these however
/// many the caller handed over.
inline constexpr std::size_t MAX_PLAYERS = std::numeric_limits<std::uint8_t>::max();
inline constexpr std::size_t MAX_REMOVALS = std::numeric_limits<std::uint8_t>::max();
inline constexpr std::size_t MAX_FIRES = std::numeric_limits<std::uint8_t>::max();
inline constexpr std::size_t MAX_ENTITIES = std::numeric_limits<std::uint16_t>::max();
} // namespace

std::size_t EncodedSize(const Snapshot& _snapshot) noexcept
{
  return Neuron::PacketHeader::SIZE_BYTES + FIXED_HEADER_BYTES + (_snapshot.players.size() * PlayerBlock::SIZE_BYTES) +
         (_snapshot.entities.size() * EntityRecord::SIZE_BYTES) + (_snapshot.removals.size() * sizeof(std::uint16_t)) +
         sizeof(std::uint8_t) + (_snapshot.fires.size() * FireEvent::SIZE_BYTES);
}

bool Encode(const Snapshot& _snapshot, Neuron::ByteWriter& _writer) noexcept
{
  // Refused before a byte is written, the way PacketHeader refuses an incoherent header: a count
  // the wire cannot name would be truncated into a different snapshot rather than a broken one.
  if ((_snapshot.players.size() > MAX_PLAYERS) || (_snapshot.entities.size() > MAX_ENTITIES) ||
      (_snapshot.removals.size() > MAX_REMOVALS) || (_snapshot.fires.size() > MAX_FIRES))
  {
    return false;
  }

  const Neuron::PacketHeader header{.type = Neuron::PacketType::Snapshot, .sequence = _snapshot.sequence};
  if (!header.Write(_writer))
  {
    return false;
  }

  static_cast<void>(_writer.WriteUInt32(_snapshot.tick));
  static_cast<void>(_writer.WriteUInt16(static_cast<std::uint16_t>(_snapshot.entities.size())));
  static_cast<void>(_writer.WriteUInt8(static_cast<std::uint8_t>(_snapshot.players.size())));
  static_cast<void>(_writer.WriteUInt8(static_cast<std::uint8_t>(_snapshot.removals.size())));

  for (const PlayerBlock& player : _snapshot.players)
  {
    static_cast<void>(_writer.WriteUInt32(player.credits));
    static_cast<void>(_writer.WriteUInt16(player.lastCommandSequenceApplied));
    static_cast<void>(_writer.WriteUInt8(player.buildingDesign));
    static_cast<void>(_writer.WriteUInt8(player.buildProgressPercent));
  }

  for (const EntityRecord& entity : _snapshot.entities)
  {
    static_cast<void>(entity.Write(_writer));
  }

  for (const std::uint16_t removed : _snapshot.removals)
  {
    static_cast<void>(_writer.WriteUInt16(removed));
  }

  // The fire count rides after the removal list rather than in the header (ADR-004), so the
  // header's shape does not change on the day weapons arrive.
  static_cast<void>(_writer.WriteUInt8(static_cast<std::uint8_t>(_snapshot.fires.size())));
  for (const FireEvent& fire : _snapshot.fires)
  {
    static_cast<void>(_writer.WriteUInt16(fire.shooter));
    static_cast<void>(_writer.WriteUInt16(fire.target));
    static_cast<void>(_writer.WriteUInt8(fire.weapon));
  }

  return !_writer.Faulted();
}

SnapshotFault Decode(Neuron::ByteReader& _reader, Snapshot& _outSnapshot) noexcept
{
  Neuron::PacketHeader header{};
  switch (Neuron::PacketHeader::Read(_reader, header))
  {
  case Neuron::PacketFault::None:
    break;
  case Neuron::PacketFault::VersionMismatch:
    return SnapshotFault::VersionMismatch;
  case Neuron::PacketFault::Truncated:
    return SnapshotFault::Truncated;
  default:
    return SnapshotFault::WrongType;
  }

  if (header.type != Neuron::PacketType::Snapshot)
  {
    return SnapshotFault::WrongType;
  }
  if (!header.IsSingleFragment())
  {
    return SnapshotFault::Fragmented;
  }

  const std::uint32_t tick = _reader.ReadUInt32();
  const std::uint16_t entityCount = _reader.ReadUInt16();
  const std::uint8_t playerCount = _reader.ReadUInt8();
  const std::uint8_t removalCount = _reader.ReadUInt8();
  if (_reader.Faulted())
  {
    return SnapshotFault::Truncated;
  }

  // THE COUNTS ARE CHECKED AGAINST WHAT IS LEFT BEFORE ANYTHING IS RESERVED FOR THEM. A count is
  // whatever the datagram said, and a datagram is a bug or a stranger away from claiming 65,535
  // entities in fourteen bytes; reserving first and discovering second is how that becomes a
  // memory spike rather than a dropped packet.
  const std::size_t claimed = (static_cast<std::size_t>(playerCount) * PlayerBlock::SIZE_BYTES) +
                              (static_cast<std::size_t>(entityCount) * EntityRecord::SIZE_BYTES) +
                              (static_cast<std::size_t>(removalCount) * sizeof(std::uint16_t)) + sizeof(std::uint8_t);
  if (claimed > _reader.RemainingBytes())
  {
    return SnapshotFault::ImpossibleCount;
  }

  Snapshot lifted;
  lifted.sequence = header.sequence;
  lifted.tick = tick;

  lifted.players.reserve(playerCount);
  for (std::uint8_t player = 0; player < playerCount; ++player)
  {
    PlayerBlock block{};
    block.credits = _reader.ReadUInt32();
    block.lastCommandSequenceApplied = _reader.ReadUInt16();
    block.buildingDesign = _reader.ReadUInt8();
    block.buildProgressPercent = _reader.ReadUInt8();
    lifted.players.push_back(block);
  }

  lifted.entities.reserve(entityCount);
  for (std::uint16_t entity = 0; entity < entityCount; ++entity)
  {
    EntityRecord record{};
    if (!EntityRecord::Read(_reader, record))
    {
      return SnapshotFault::Truncated;
    }
    lifted.entities.push_back(record);
  }

  lifted.removals.reserve(removalCount);
  for (std::uint8_t removal = 0; removal < removalCount; ++removal)
  {
    lifted.removals.push_back(_reader.ReadUInt16());
  }

  const std::uint8_t fireCount = _reader.ReadUInt8();
  if (_reader.Faulted())
  {
    return SnapshotFault::Truncated;
  }
  if ((static_cast<std::size_t>(fireCount) * FireEvent::SIZE_BYTES) > _reader.RemainingBytes())
  {
    return SnapshotFault::ImpossibleCount;
  }

  lifted.fires.reserve(fireCount);
  for (std::uint8_t fire = 0; fire < fireCount; ++fire)
  {
    FireEvent event{};
    event.shooter = _reader.ReadUInt16();
    event.target = _reader.ReadUInt16();
    event.weapon = _reader.ReadUInt8();
    lifted.fires.push_back(event);
  }

  if (_reader.Faulted())
  {
    return SnapshotFault::Truncated;
  }

  _outSnapshot = std::move(lifted);
  return SnapshotFault::None;
}

} // namespace Outpost

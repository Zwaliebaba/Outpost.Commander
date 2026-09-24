// A shared-items translation unit -- see NeuronCore.cpp for why it carries no precompiled header.

#include "Update.h"

#include "Generator.h"

#include <limits>
#include <utility>

namespace Outpost
{

// Q83's mask covers every rock the largest field has: a region's ten home rocks and two clusters of six, copied to each
// of the four anchors.
static_assert((MAX_SPENT_ROCK_BYTES * 8) >=
              ((HOME_FIELD_ASTEROID_COUNT + (CONTESTED_FIELD_COUNT * CONTESTED_FIELD_ASTEROID_COUNT)) * ANCHOR_COUNT));

namespace
{
/// The record count is one byte, so an update cannot name more than this however many were handed
/// over -- and the payload holds a hundred, so the byte is never the limit in practice.
inline constexpr std::size_t MAX_RECORDS = std::numeric_limits<std::uint8_t>::max();

void WritePlayerBlock(const PlayerBlock& _block, Neuron::ByteWriter& _writer) noexcept
{
  static_cast<void>(_writer.WriteUInt32(_block.credits));
  static_cast<void>(_writer.WriteUInt16(_block.lastCommandSequenceApplied));
  static_cast<void>(_writer.WriteUInt8(_block.buildingDesign));
  static_cast<void>(_writer.WriteUInt8(_block.buildProgressPercent));
  static_cast<void>(_writer.WriteUInt8(_block.unlocked));
  static_cast<void>(_writer.WriteUInt8(_block.researchProgress));
}

[[nodiscard]] PlayerBlock ReadPlayerBlock(Neuron::ByteReader& _reader) noexcept
{
  PlayerBlock block{};
  block.credits = _reader.ReadUInt32();
  block.lastCommandSequenceApplied = _reader.ReadUInt16();
  block.buildingDesign = _reader.ReadUInt8();
  block.buildProgressPercent = _reader.ReadUInt8();
  block.unlocked = _reader.ReadUInt8();
  block.researchProgress = _reader.ReadUInt8();
  return block;
}
} // namespace

std::size_t EncodedSize(const Update& _update) noexcept
{
  return UPDATE_HEADER_BYTES + (_update.records.size() * EntityRecord::SIZE_BYTES) + (_update.removals.size() * REMOVAL_BYTES) +
         (_update.fires.size() * FireEvent::SIZE_BYTES) + _update.spentRocks.size();
}

bool Encode(const Update& _update, Neuron::ByteWriter& _writer) noexcept
{
  // Refused before a byte is written. A count past its cap would break the floor the sweep is computed
  // from, and an update past the payload would fragment -- which is the one thing ADR-024 exists to
  // make impossible rather than unlikely.
  if ((_update.records.size() > MAX_RECORDS) || (_update.removals.size() > MAX_REMOVALS_PER_UPDATE) ||
      (_update.fires.size() > MAX_FIRES_PER_UPDATE) || (_update.spentRocks.size() > MAX_SPENT_ROCK_BYTES) ||
      (EncodedSize(_update) > UPDATE_PAYLOAD_BYTES))
  {
    return false;
  }

  const Neuron::PacketHeader header{.type = Neuron::PacketType::Update, .sequence = _update.sequence};
  if (!header.Write(_writer))
  {
    return false;
  }

  static_cast<void>(_writer.WriteUInt32(_update.tick));
  static_cast<void>(_writer.WriteUInt16(_update.liveEntityCount));
  WritePlayerBlock(_update.own, _writer);
  static_cast<void>(_writer.WriteUInt8(static_cast<std::uint8_t>(_update.records.size())));
  static_cast<void>(_writer.WriteUInt8(static_cast<std::uint8_t>(_update.removals.size())));
  static_cast<void>(_writer.WriteUInt8(static_cast<std::uint8_t>(_update.fires.size())));
  static_cast<void>(_writer.WriteUInt8(static_cast<std::uint8_t>(_update.spentRocks.size())));

  for (const EntityRecord& record : _update.records)
  {
    static_cast<void>(record.Write(_writer));
  }
  for (const WireIdentity removed : _update.removals)
  {
    static_cast<void>(_writer.WriteUInt24(removed));
  }
  for (const FireEvent& fire : _update.fires)
  {
    static_cast<void>(_writer.WriteUInt24(fire.shooter));
    static_cast<void>(_writer.WriteUInt24(fire.target));
    static_cast<void>(_writer.WriteUInt8(fire.weapon));
  }
  for (const std::uint8_t bits : _update.spentRocks)
  {
    static_cast<void>(_writer.WriteUInt8(bits));
  }

  return !_writer.Faulted();
}

UpdateFault Decode(Neuron::ByteReader& _reader, Update& _outUpdate) noexcept
{
  Neuron::PacketHeader header{};
  switch (Neuron::PacketHeader::Read(_reader, header))
  {
  case Neuron::PacketFault::None:
    break;
  case Neuron::PacketFault::VersionMismatch:
    return UpdateFault::VersionMismatch;
  case Neuron::PacketFault::Truncated:
    return UpdateFault::Truncated;
  default:
    return UpdateFault::WrongType;
  }

  if (header.type != Neuron::PacketType::Update)
  {
    return UpdateFault::WrongType;
  }

  const std::uint32_t tick = _reader.ReadUInt32();
  const std::uint16_t liveEntityCount = _reader.ReadUInt16();
  const PlayerBlock own = ReadPlayerBlock(_reader);
  const std::uint8_t recordCount = _reader.ReadUInt8();
  const std::uint8_t removalCount = _reader.ReadUInt8();
  const std::uint8_t fireCount = _reader.ReadUInt8();
  const std::uint8_t spentCount = _reader.ReadUInt8();
  if (_reader.Faulted())
  {
    return UpdateFault::Truncated;
  }

  // THE COUNTS ARE CHECKED AGAINST THEIR CAPS AND AGAINST WHAT IS LEFT BEFORE ANYTHING IS RESERVED FOR
  // THEM. A count is whatever the datagram said, and reserving first and discovering second is how a
  // stranger's datagram becomes a memory spike rather than a dropped packet.
  if ((removalCount > MAX_REMOVALS_PER_UPDATE) || (fireCount > MAX_FIRES_PER_UPDATE) || (spentCount > MAX_SPENT_ROCK_BYTES))
  {
    return UpdateFault::ImpossibleCount;
  }
  const std::size_t claimed = (static_cast<std::size_t>(recordCount) * EntityRecord::SIZE_BYTES) +
                              (static_cast<std::size_t>(removalCount) * REMOVAL_BYTES) +
                              (static_cast<std::size_t>(fireCount) * FireEvent::SIZE_BYTES) + spentCount;
  if (claimed > _reader.RemainingBytes())
  {
    return UpdateFault::ImpossibleCount;
  }

  Update lifted;
  lifted.sequence = header.sequence;
  lifted.tick = tick;
  lifted.liveEntityCount = liveEntityCount;
  lifted.own = own;

  lifted.records.reserve(recordCount);
  for (std::uint8_t index = 0; index < recordCount; ++index)
  {
    EntityRecord record{};
    if (!EntityRecord::Read(_reader, record))
    {
      return UpdateFault::Truncated;
    }
    lifted.records.push_back(record);
  }

  lifted.removals.reserve(removalCount);
  for (std::uint8_t index = 0; index < removalCount; ++index)
  {
    lifted.removals.push_back(_reader.ReadUInt24());
  }

  lifted.fires.reserve(fireCount);
  for (std::uint8_t index = 0; index < fireCount; ++index)
  {
    FireEvent event{};
    event.shooter = _reader.ReadUInt24();
    event.target = _reader.ReadUInt24();
    event.weapon = _reader.ReadUInt8();
    lifted.fires.push_back(event);
  }

  lifted.spentRocks.reserve(spentCount);
  for (std::uint8_t index = 0; index < spentCount; ++index)
  {
    lifted.spentRocks.push_back(_reader.ReadUInt8());
  }

  if (_reader.Faulted())
  {
    return UpdateFault::Truncated;
  }

  _outUpdate = std::move(lifted);
  return UpdateFault::None;
}

} // namespace Outpost

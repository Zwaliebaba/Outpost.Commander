// A shared-items translation unit -- see NeuronCore.cpp for why it carries no precompiled header.

#include "EntityRecord.h"

#include <limits>

namespace Outpost
{

bool EntityRecord::Write(Neuron::ByteWriter& _writer) const noexcept
{
  // In the order the table in the header gives, and that order is the format. ByteWriter refuses
  // a field that will not fit and latches, so checking once at the end is enough -- a partial
  // record never reaches the buffer in a state a reader could mistake for a whole one.
  static_cast<void>(_writer.WriteUInt16(identity));
  static_cast<void>(_writer.WriteInt16(positionX));
  static_cast<void>(_writer.WriteInt16(positionY));
  static_cast<void>(_writer.WriteUInt8(heading));
  static_cast<void>(_writer.WriteUInt8(hullPercentRemaining));
  static_cast<void>(_writer.WriteUInt8(designIdentity));
  static_cast<void>(_writer.WriteUInt8(flags));
  return !_writer.Faulted();
}

bool EntityRecord::Read(Neuron::ByteReader& _reader, EntityRecord& _outRecord) noexcept
{
  EntityRecord lifted{};
  lifted.identity = _reader.ReadUInt16();
  lifted.positionX = _reader.ReadInt16();
  lifted.positionY = _reader.ReadInt16();
  lifted.heading = _reader.ReadUInt8();
  lifted.hullPercentRemaining = _reader.ReadUInt8();
  lifted.designIdentity = _reader.ReadUInt8();
  lifted.flags = _reader.ReadUInt8();

  // Checked once, after the whole record, which is what ByteReader's latching fault is for. The
  // destination is left alone on a short read rather than half filled.
  if (_reader.Faulted())
  {
    return false;
  }

  _outRecord = lifted;
  return true;
}

std::int16_t QuantizePosition(Neuron::Fixed _value) noexcept
{
  constexpr std::int32_t HALF_STEP = POSITION_WIRE_STEP / 2;

  // Symmetric rounding, in a wider type so that a value at the edge of Fixed cannot overflow on
  // the way to being rounded.
  const std::int64_t wide = static_cast<std::int64_t>(_value);
  const std::int64_t rounded = (wide >= 0) ? ((wide + HALF_STEP) / POSITION_WIRE_STEP) : ((wide - HALF_STEP) / POSITION_WIRE_STEP);

  constexpr std::int64_t HIGHEST = std::numeric_limits<std::int16_t>::max();
  constexpr std::int64_t LOWEST = std::numeric_limits<std::int16_t>::min();
  if (rounded > HIGHEST)
  {
    return std::numeric_limits<std::int16_t>::max();
  }
  if (rounded < LOWEST)
  {
    return std::numeric_limits<std::int16_t>::min();
  }
  return static_cast<std::int16_t>(rounded);
}

Neuron::Fixed DequantizePosition(std::int16_t _value) noexcept
{
  return static_cast<Neuron::Fixed>(static_cast<std::int32_t>(_value) * POSITION_WIRE_STEP);
}

} // namespace Outpost

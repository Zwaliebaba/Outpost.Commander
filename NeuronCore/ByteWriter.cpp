// A shared-items translation unit: this file is compiled into every project that imports
// NeuronCore.vcxitems, each of which has its own precompiled header. There is no one pch to share
// between them, so these files use none and include their master header first instead.

#include "NeuronCore.h"

#include "ByteWriter.h"

#include <cstring>

namespace Neuron
{

bool ByteWriter::WriteUInt8(std::uint8_t _value) noexcept
{
  return Write(static_cast<std::uint64_t>(_value), sizeof(std::uint8_t));
}

bool ByteWriter::WriteUInt16(std::uint16_t _value) noexcept
{
  return Write(static_cast<std::uint64_t>(_value), sizeof(std::uint16_t));
}

bool ByteWriter::WriteUInt24(std::uint32_t _value) noexcept
{
  return Write(static_cast<std::uint64_t>(_value & 0x00FFFFFFu), 3);
}

bool ByteWriter::WriteUInt32(std::uint32_t _value) noexcept
{
  return Write(static_cast<std::uint64_t>(_value), sizeof(std::uint32_t));
}

bool ByteWriter::WriteUInt64(std::uint64_t _value) noexcept
{
  return Write(_value, sizeof(std::uint64_t));
}

// The signed widths go through their own unsigned counterpart rather than straight to
// std::uint64_t. Both land the same bits -- a conversion to an unsigned type is modular and C++20
// mandates two's complement -- but the narrow cast says which bits are the field, where the wide
// one relies on Write() masking away the sign extension it just produced.

bool ByteWriter::WriteInt8(std::int8_t _value) noexcept
{
  return Write(static_cast<std::uint64_t>(static_cast<std::uint8_t>(_value)), sizeof(std::int8_t));
}

bool ByteWriter::WriteInt16(std::int16_t _value) noexcept
{
  return Write(static_cast<std::uint64_t>(static_cast<std::uint16_t>(_value)), sizeof(std::int16_t));
}

bool ByteWriter::WriteInt32(std::int32_t _value) noexcept
{
  return Write(static_cast<std::uint64_t>(static_cast<std::uint32_t>(_value)), sizeof(std::int32_t));
}

bool ByteWriter::WriteInt64(std::int64_t _value) noexcept
{
  return Write(static_cast<std::uint64_t>(_value), sizeof(std::int64_t));
}

bool ByteWriter::WriteBytes(std::span<const std::byte> _bytes) noexcept
{
  if (m_faulted)
  {
    return false;
  }
  if (_bytes.size() > RemainingBytes())
  {
    m_faulted = true;
    return false;
  }
  if (!_bytes.empty())
  {
    std::memcpy(m_buffer.data() + m_writtenBytes, _bytes.data(), _bytes.size());
  }
  m_writtenBytes += _bytes.size();
  return true;
}

bool ByteWriter::Write(std::uint64_t _value, std::size_t _widthBytes) noexcept
{
  if (m_faulted)
  {
    return false;
  }

  // m_writtenBytes never exceeds the span, so RemainingBytes() cannot wrap. Comparing against it
  // rather than adding to m_writtenBytes is what keeps the bound honest at the top of the range.
  if (_widthBytes > RemainingBytes())
  {
    m_faulted = true;
    return false;
  }

  for (std::size_t index = 0; index < _widthBytes; ++index)
  {
    m_buffer[m_writtenBytes + index] = static_cast<std::byte>((_value >> (index * 8)) & 0xFFu);
  }
  m_writtenBytes += _widthBytes;
  return true;
}

} // namespace Neuron

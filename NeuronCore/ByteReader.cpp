// A shared-items translation unit: this file is compiled into every project that imports
// NeuronCore.vcxitems, each of which has its own precompiled header. There is no one pch to share
// between them, so these files use none and include their master header first instead.

#include "NeuronCore.h"

#include "ByteReader.h"

#include <cstring>

namespace Neuron
{

std::uint8_t ByteReader::ReadUInt8() noexcept
{
  return static_cast<std::uint8_t>(Read(sizeof(std::uint8_t)));
}

std::uint16_t ByteReader::ReadUInt16() noexcept
{
  return static_cast<std::uint16_t>(Read(sizeof(std::uint16_t)));
}

std::uint32_t ByteReader::ReadUInt24() noexcept
{
  return static_cast<std::uint32_t>(Read(3));
}

std::uint32_t ByteReader::ReadUInt32() noexcept
{
  return static_cast<std::uint32_t>(Read(sizeof(std::uint32_t)));
}

std::uint64_t ByteReader::ReadUInt64() noexcept
{
  return Read(sizeof(std::uint64_t));
}

// The signed widths narrow to their own unsigned counterpart before taking the sign, so that the
// bits that are the field are the bits that become the value. A conversion to an unsigned type is
// modular and C++20 mandates two's complement, so this round trips every value ByteWriter writes.

std::int8_t ByteReader::ReadInt8() noexcept
{
  return static_cast<std::int8_t>(static_cast<std::uint8_t>(Read(sizeof(std::int8_t))));
}

std::int16_t ByteReader::ReadInt16() noexcept
{
  return static_cast<std::int16_t>(static_cast<std::uint16_t>(Read(sizeof(std::int16_t))));
}

std::int32_t ByteReader::ReadInt32() noexcept
{
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(Read(sizeof(std::int32_t))));
}

std::int64_t ByteReader::ReadInt64() noexcept
{
  return static_cast<std::int64_t>(Read(sizeof(std::int64_t)));
}

bool ByteReader::ReadBytes(std::span<std::byte> _outBytes) noexcept
{
  if (m_faulted)
  {
    return false;
  }
  if (_outBytes.size() > RemainingBytes())
  {
    m_faulted = true;
    return false;
  }
  if (!_outBytes.empty())
  {
    std::memcpy(_outBytes.data(), m_buffer.data() + m_consumedBytes, _outBytes.size());
  }
  m_consumedBytes += _outBytes.size();
  return true;
}

std::uint64_t ByteReader::Read(std::size_t _widthBytes) noexcept
{
  if (m_faulted)
  {
    return 0;
  }

  // m_consumedBytes never exceeds the span, so RemainingBytes() cannot wrap. The cursor stays put
  // on a short read: a faulted reader reports how far it honestly got, not how far it was asked.
  if (_widthBytes > RemainingBytes())
  {
    m_faulted = true;
    return 0;
  }

  std::uint64_t value = 0;
  for (std::size_t index = 0; index < _widthBytes; ++index)
  {
    const auto octet = static_cast<std::uint64_t>(std::to_integer<std::uint8_t>(m_buffer[m_consumedBytes + index]));
    value |= octet << (index * 8);
  }
  m_consumedBytes += _widthBytes;
  return value;
}

} // namespace Neuron

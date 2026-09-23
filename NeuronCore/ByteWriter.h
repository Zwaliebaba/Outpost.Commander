#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace Neuron
{

/// Writes integers into a caller-owned buffer in explicit little-endian order.
///
/// EXPLICIT, meaning shifts and masks rather than a copy of the machine's own representation. A
/// memcpy of a std::uint32_t produces the right four bytes on every machine this game will run on
/// and is still not a format: it is the host's byte order spelled in a way that cannot be read
/// back by a machine that disagrees, and nothing in a round-trip test can tell the two apart. The
/// shifts below ARE the format, and they cost nothing measurable against a 1,232-byte datagram.
///
/// Four properties the rest of the tree is written against:
///
///   * No allocation and no exceptions. The buffer belongs to the caller and outlives the writer.
///   * A write that does not fit writes NOTHING. There is no half a field on the wire.
///   * The fault LATCHES, and after it nothing more is written and the cursor does not move. An
///     encoder laying down thirty fields therefore checks Faulted() once at the end rather than
///     branching thirty times, and cannot be fooled by a later short field that happens to fit.
///   * Because of that latch the write methods are deliberately NOT [[nodiscard]] -- forcing a
///     (void) cast on every call is the noise the latch exists to remove. Check each return, or
///     check Faulted() at the end; both are correct and the second is what the encoders do.
class ByteWriter
{
public:
  explicit ByteWriter(std::span<std::byte> _buffer) noexcept
    : m_buffer(_buffer)
  {
  }

  bool WriteUInt8(std::uint8_t _value) noexcept;
  bool WriteUInt16(std::uint16_t _value) noexcept;

  /// **THREE BYTES, AND ONLY THE LOW TWENTY-FOUR BITS OF _value.** ADR-024's entity identity is a
  /// 16-bit index and an 8-bit generation, and a fourth byte on every record would be one record in
  /// twelve spent on nothing. The top byte of _value is not checked: an identity is packed by
  /// `GameCore/EntityRecord.h`, which cannot set it.
  bool WriteUInt24(std::uint32_t _value) noexcept;
  bool WriteUInt32(std::uint32_t _value) noexcept;
  bool WriteUInt64(std::uint64_t _value) noexcept;

  bool WriteInt8(std::int8_t _value) noexcept;
  bool WriteInt16(std::int16_t _value) noexcept;
  bool WriteInt32(std::int32_t _value) noexcept;
  bool WriteInt64(std::int64_t _value) noexcept;

  /// Copies bytes through unchanged -- a payload, not a number, so byte order does not apply.
  bool WriteBytes(std::span<const std::byte> _bytes) noexcept;

  /// How much of the buffer the writer has filled. This is the length to hand to a socket.
  [[nodiscard]] std::size_t WrittenBytes() const noexcept
  {
    return m_writtenBytes;
  }

  [[nodiscard]] std::size_t RemainingBytes() const noexcept
  {
    return m_buffer.size() - m_writtenBytes;
  }

  [[nodiscard]] std::size_t CapacityBytes() const noexcept
  {
    return m_buffer.size();
  }

  /// True once any write has failed. A faulted writer's buffer holds a prefix of what was asked
  /// for, and the only correct use of it is to discard it.
  [[nodiscard]] bool Faulted() const noexcept
  {
    return m_faulted;
  }

private:
  /// The one place bytes are laid down: the low _widthBytes bytes of _value, least significant
  /// first. Every method above is this call with a width.
  bool Write(std::uint64_t _value, std::size_t _widthBytes) noexcept;

  std::span<std::byte> m_buffer;
  std::size_t m_writtenBytes = 0;
  bool m_faulted = false;
};

} // namespace Neuron

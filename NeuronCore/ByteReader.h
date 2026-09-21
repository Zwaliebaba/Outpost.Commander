#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace Neuron
{

/// Reads integers back out of a caller-owned buffer in explicit little-endian order -- the exact
/// inverse of ByteWriter, and see that header for why the shifts are written out rather than
/// copied from the machine's own representation.
///
/// This side faces the network, so its failure mode is the one that matters: every buffer it is
/// handed came off a socket and may be truncated, padded, replayed or simply wrong.
///
///   * A read that would pass the end returns ZERO and does not move the cursor. Zero is a
///     defined value rather than whatever happened to be next in memory, which is the whole of
///     the difference between failing and returning rubbish.
///   * The fault LATCHES. After the first short read every later one returns zero too, so a
///     decoder cannot resynchronize onto a field that happens to fit and report a plausible
///     record built from nothing.
///   * The reads are therefore value-returning and [[nodiscard]], and the caller checks
///     Faulted() ONCE when the record is complete. A decoder that ignores it has built its record
///     out of zeros, which is why the check is not optional and not per field.
///
/// No allocation and no exceptions. The buffer belongs to the caller and outlives the reader.
class ByteReader
{
public:
  explicit ByteReader(std::span<const std::byte> _buffer) noexcept
    : m_buffer(_buffer)
  {
  }

  [[nodiscard]] std::uint8_t ReadUInt8() noexcept;
  [[nodiscard]] std::uint16_t ReadUInt16() noexcept;
  [[nodiscard]] std::uint32_t ReadUInt32() noexcept;
  [[nodiscard]] std::uint64_t ReadUInt64() noexcept;

  [[nodiscard]] std::int8_t ReadInt8() noexcept;
  [[nodiscard]] std::int16_t ReadInt16() noexcept;
  [[nodiscard]] std::int32_t ReadInt32() noexcept;
  [[nodiscard]] std::int64_t ReadInt64() noexcept;

  /// Copies _outBytes.size() bytes through unchanged. On a short buffer it writes NOTHING, faults
  /// and returns false, so the caller's destination is never half a payload.
  bool ReadBytes(std::span<std::byte> _outBytes) noexcept;

  [[nodiscard]] std::size_t ConsumedBytes() const noexcept
  {
    return m_consumedBytes;
  }

  [[nodiscard]] std::size_t RemainingBytes() const noexcept
  {
    return m_buffer.size() - m_consumedBytes;
  }

  [[nodiscard]] std::size_t CapacityBytes() const noexcept
  {
    return m_buffer.size();
  }

  /// True once any read has passed the end. Everything produced since is zero.
  [[nodiscard]] bool Faulted() const noexcept
  {
    return m_faulted;
  }

private:
  /// The one place bytes are lifted: _widthBytes of them, least significant first, into the low
  /// bits of the result. Every method above is this call with a width and a cast.
  std::uint64_t Read(std::size_t _widthBytes) noexcept;

  std::span<const std::byte> m_buffer;
  std::size_t m_consumedBytes = 0;
  bool m_faulted = false;
};

} // namespace Neuron

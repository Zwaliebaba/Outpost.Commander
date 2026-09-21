#include "pch.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace NeuronCoreTests
{

namespace
{
/// A byte the assertion framework can print. CppUnitTest has no ToString for std::byte, so every
/// comparison against a literal goes through this rather than through IsTrue, which would report
/// a failure as "expected true" and tell nobody which byte was wrong.
[[nodiscard]] std::uint8_t Octet(std::byte _value) noexcept
{
  return std::to_integer<std::uint8_t>(_value);
}

/// The value a scratch buffer is filled with before a test writes into it. Nothing on the wire
/// is ever this, so a byte that still reads 0xCD afterwards was not touched.
inline constexpr std::byte UNTOUCHED{0xCD};
} // namespace

/// The half of this codec that a round-trip test cannot reach. A writer and reader that agree
/// with each other about being big-endian pass every test in ByteCodecRoundTrip below, so the
/// byte order is asserted against literals here and nowhere else is allowed to assume it.
TEST_CLASS(ByteCodecOrder)
{
public:
  TEST_METHOD(SixteenBitsGoOutLeastSignificantFirst)
  {
    std::array<std::byte, 2> buffer{};
    Neuron::ByteWriter writer{buffer};

    Assert::IsTrue(writer.WriteUInt16(std::uint16_t{0x0102}));

    Assert::AreEqual(std::uint8_t{0x02}, Octet(buffer[0]));
    Assert::AreEqual(std::uint8_t{0x01}, Octet(buffer[1]));
  }

  TEST_METHOD(ThirtyTwoBitsGoOutLeastSignificantFirst)
  {
    std::array<std::byte, 4> buffer{};
    Neuron::ByteWriter writer{buffer};

    Assert::IsTrue(writer.WriteUInt32(0x01020304u));

    Assert::AreEqual(std::uint8_t{0x04}, Octet(buffer[0]));
    Assert::AreEqual(std::uint8_t{0x03}, Octet(buffer[1]));
    Assert::AreEqual(std::uint8_t{0x02}, Octet(buffer[2]));
    Assert::AreEqual(std::uint8_t{0x01}, Octet(buffer[3]));
  }

  TEST_METHOD(SixtyFourBitsGoOutLeastSignificantFirst)
  {
    std::array<std::byte, 8> buffer{};
    Neuron::ByteWriter writer{buffer};

    Assert::IsTrue(writer.WriteUInt64(0x0102030405060708ull));

    for (std::size_t index = 0; index < buffer.size(); ++index)
    {
      Assert::AreEqual(static_cast<std::uint8_t>(0x08 - index), Octet(buffer[index]));
    }
  }

  TEST_METHOD(ANegativeValueGoesOutAsItsTwosComplement)
  {
    std::array<std::byte, 6> buffer{};
    Neuron::ByteWriter writer{buffer};

    Assert::IsTrue(writer.WriteInt16(std::int16_t{-2}));
    Assert::IsTrue(writer.WriteInt32(std::numeric_limits<std::int32_t>::min()));

    Assert::AreEqual(std::uint8_t{0xFE}, Octet(buffer[0]));
    Assert::AreEqual(std::uint8_t{0xFF}, Octet(buffer[1]));

    Assert::AreEqual(std::uint8_t{0x00}, Octet(buffer[2]));
    Assert::AreEqual(std::uint8_t{0x00}, Octet(buffer[3]));
    Assert::AreEqual(std::uint8_t{0x00}, Octet(buffer[4]));
    Assert::AreEqual(std::uint8_t{0x80}, Octet(buffer[5]));
  }

  TEST_METHOD(TheReaderLiftsALiteralLittleEndianBuffer)
  {
    // Written out by hand rather than by ByteWriter, so that the reader is pinned independently.
    constexpr std::array<std::byte, 8> WIRE{std::byte{0x02}, std::byte{0x01}, std::byte{0x04}, std::byte{0x03},
                                            std::byte{0x02}, std::byte{0x01}, std::byte{0xFE}, std::byte{0xFF}};
    Neuron::ByteReader reader{WIRE};

    Assert::AreEqual(std::uint16_t{0x0102}, reader.ReadUInt16());
    Assert::AreEqual(std::uint32_t{0x01020304u}, reader.ReadUInt32());
    Assert::AreEqual(std::int16_t{-2}, reader.ReadInt16());
    Assert::IsFalse(reader.Faulted());
  }
};

TEST_CLASS(ByteCodecRoundTrip)
{
public:
  TEST_METHOD(EveryWidthRoundTrips)
  {
    std::array<std::byte, 30> buffer{};
    Neuron::ByteWriter writer{buffer};

    writer.WriteUInt8(std::uint8_t{0x12});
    writer.WriteUInt16(std::uint16_t{0x3456});
    writer.WriteUInt32(0x789ABCDEu);
    writer.WriteUInt64(0x0F1E2D3C4B5A6978ull);
    writer.WriteInt8(std::int8_t{-1});
    writer.WriteInt16(std::int16_t{-300});
    writer.WriteInt32(std::int32_t{-70000});
    writer.WriteInt64(std::int64_t{-5000000000ll});
    Assert::IsFalse(writer.Faulted());
    Assert::AreEqual(static_cast<std::size_t>(30), writer.WrittenBytes());

    Neuron::ByteReader reader{buffer};
    Assert::AreEqual(std::uint8_t{0x12}, reader.ReadUInt8());
    Assert::AreEqual(std::uint16_t{0x3456}, reader.ReadUInt16());
    Assert::AreEqual(std::uint32_t{0x789ABCDEu}, reader.ReadUInt32());
    Assert::AreEqual(std::uint64_t{0x0F1E2D3C4B5A6978ull}, reader.ReadUInt64());
    Assert::AreEqual(std::int8_t{-1}, reader.ReadInt8());
    Assert::AreEqual(std::int16_t{-300}, reader.ReadInt16());
    Assert::AreEqual(std::int32_t{-70000}, reader.ReadInt32());
    Assert::AreEqual(std::int64_t{-5000000000ll}, reader.ReadInt64());
    Assert::IsFalse(reader.Faulted());
    Assert::AreEqual(static_cast<std::size_t>(0), reader.RemainingBytes());
  }

  /// The comfortable middle of a range is where a codec is right by accident. These are the ends.
  TEST_METHOD(EveryWidthRoundTripsAtItsExtremes)
  {
    std::array<std::byte, 60> buffer{};
    Neuron::ByteWriter writer{buffer};

    writer.WriteUInt8(std::numeric_limits<std::uint8_t>::max());
    writer.WriteUInt16(std::numeric_limits<std::uint16_t>::max());
    writer.WriteUInt32(std::numeric_limits<std::uint32_t>::max());
    writer.WriteUInt64(std::numeric_limits<std::uint64_t>::max());
    writer.WriteInt8(std::numeric_limits<std::int8_t>::min());
    writer.WriteInt8(std::numeric_limits<std::int8_t>::max());
    writer.WriteInt16(std::numeric_limits<std::int16_t>::min());
    writer.WriteInt16(std::numeric_limits<std::int16_t>::max());
    writer.WriteInt32(std::numeric_limits<std::int32_t>::min());
    writer.WriteInt32(std::numeric_limits<std::int32_t>::max());
    writer.WriteInt64(std::numeric_limits<std::int64_t>::min());
    writer.WriteInt64(std::numeric_limits<std::int64_t>::max());
    Assert::IsFalse(writer.Faulted());

    Neuron::ByteReader reader{buffer};
    Assert::AreEqual(std::numeric_limits<std::uint8_t>::max(), reader.ReadUInt8());
    Assert::AreEqual(std::numeric_limits<std::uint16_t>::max(), reader.ReadUInt16());
    Assert::AreEqual(std::numeric_limits<std::uint32_t>::max(), reader.ReadUInt32());
    Assert::AreEqual(std::numeric_limits<std::uint64_t>::max(), reader.ReadUInt64());
    Assert::AreEqual(std::numeric_limits<std::int8_t>::min(), reader.ReadInt8());
    Assert::AreEqual(std::numeric_limits<std::int8_t>::max(), reader.ReadInt8());
    Assert::AreEqual(std::numeric_limits<std::int16_t>::min(), reader.ReadInt16());
    Assert::AreEqual(std::numeric_limits<std::int16_t>::max(), reader.ReadInt16());
    Assert::AreEqual(std::numeric_limits<std::int32_t>::min(), reader.ReadInt32());
    Assert::AreEqual(std::numeric_limits<std::int32_t>::max(), reader.ReadInt32());
    Assert::AreEqual(std::numeric_limits<std::int64_t>::min(), reader.ReadInt64());
    Assert::AreEqual(std::numeric_limits<std::int64_t>::max(), reader.ReadInt64());
    Assert::IsFalse(reader.Faulted());
  }

  TEST_METHOD(BytesPassThroughUnchanged)
  {
    constexpr std::array<std::byte, 4> PAYLOAD{std::byte{0xDE}, std::byte{0xAD}, std::byte{0xBE}, std::byte{0xEF}};
    std::array<std::byte, 5> buffer{};
    Neuron::ByteWriter writer{buffer};

    Assert::IsTrue(writer.WriteUInt8(std::uint8_t{0x07}));
    Assert::IsTrue(writer.WriteBytes(PAYLOAD));
    Assert::AreEqual(static_cast<std::size_t>(5), writer.WrittenBytes());

    Neuron::ByteReader reader{buffer};
    Assert::AreEqual(std::uint8_t{0x07}, reader.ReadUInt8());
    std::array<std::byte, 4> echoed{};
    Assert::IsTrue(reader.ReadBytes(echoed));
    Assert::IsTrue(echoed == PAYLOAD);
    Assert::IsFalse(reader.Faulted());
  }

  /// An empty span is the removal list and the fire-event list at M0, so it has to be a no-op
  /// rather than a fault: ADR-003 pays a count byte for each and sends nothing after it.
  TEST_METHOD(AnEmptySpanIsANoOpOnBothSides)
  {
    std::array<std::byte, 1> buffer{};
    Neuron::ByteWriter writer{buffer};
    Assert::IsTrue(writer.WriteBytes(std::span<const std::byte>{}));
    Assert::AreEqual(static_cast<std::size_t>(0), writer.WrittenBytes());
    Assert::IsFalse(writer.Faulted());

    Neuron::ByteReader reader{std::span<const std::byte>{}};
    Assert::IsTrue(reader.ReadBytes(std::span<std::byte>{}));
    Assert::IsFalse(reader.Faulted());
  }

  TEST_METHOD(AnExactFitFillsTheBufferAndTheNextByteFails)
  {
    std::array<std::byte, 4> buffer{};
    Neuron::ByteWriter writer{buffer};

    Assert::IsTrue(writer.WriteUInt32(0x11223344u));
    Assert::AreEqual(static_cast<std::size_t>(0), writer.RemainingBytes());
    Assert::IsFalse(writer.Faulted());

    Assert::IsFalse(writer.WriteUInt8(std::uint8_t{0x55}));
    Assert::IsTrue(writer.Faulted());
    Assert::AreEqual(static_cast<std::size_t>(4), writer.WrittenBytes());
  }
};

TEST_CLASS(ByteCodecWriterBounds)
{
public:
  /// The buffer handed to the writer is a SUBSPAN of a larger array, so "wrote nothing past the
  /// end" is asserted against memory that really exists and really holds something else. Against
  /// a buffer sized exactly to the span, an overrun would be undefined behavior rather than a
  /// failed assertion, and the test would prove nothing either way.
  TEST_METHOD(AWriteThatDoesNotFitLeavesNoPartialField)
  {
    std::array<std::byte, 8> backing{};
    backing.fill(UNTOUCHED);
    Neuron::ByteWriter writer{std::span{backing}.first(3)};

    Assert::IsFalse(writer.WriteUInt32(0x01020304u));
    Assert::IsTrue(writer.Faulted());
    Assert::AreEqual(static_cast<std::size_t>(0), writer.WrittenBytes());

    for (const std::byte value : backing)
    {
      Assert::AreEqual(Octet(UNTOUCHED), Octet(value));
    }
  }

  TEST_METHOD(AWritePastTheEndNeverReachesBeyondTheSpan)
  {
    std::array<std::byte, 8> backing{};
    backing.fill(UNTOUCHED);
    Neuron::ByteWriter writer{std::span{backing}.first(4)};

    Assert::IsTrue(writer.WriteUInt16(std::uint16_t{0x0102}));
    Assert::IsTrue(writer.WriteUInt16(std::uint16_t{0x0304}));
    Assert::IsFalse(writer.WriteUInt16(std::uint16_t{0x0506}));

    Assert::IsTrue(writer.Faulted());
    Assert::AreEqual(static_cast<std::size_t>(4), writer.WrittenBytes());
    for (std::size_t index = 4; index < backing.size(); ++index)
    {
      Assert::AreEqual(Octet(UNTOUCHED), Octet(backing[index]));
    }
  }

  /// The latch is what stops an encoder resynchronizing: after the first field that would not fit,
  /// a later narrower field must NOT quietly land in the gap the wide one left.
  TEST_METHOD(TheWriteFaultLatchesOverALaterFieldThatWouldFit)
  {
    std::array<std::byte, 8> backing{};
    backing.fill(UNTOUCHED);
    Neuron::ByteWriter writer{std::span{backing}.first(3)};

    Assert::IsFalse(writer.WriteUInt32(0x01020304u));
    Assert::IsFalse(writer.WriteUInt8(std::uint8_t{0x05}));

    Assert::IsTrue(writer.Faulted());
    Assert::AreEqual(static_cast<std::size_t>(0), writer.WrittenBytes());
    Assert::AreEqual(Octet(UNTOUCHED), Octet(backing[0]));
  }

  TEST_METHOD(AnOversizedWriteBytesLeavesTheBufferAlone)
  {
    constexpr std::array<std::byte, 4> PAYLOAD{std::byte{0xDE}, std::byte{0xAD}, std::byte{0xBE}, std::byte{0xEF}};
    std::array<std::byte, 8> backing{};
    backing.fill(UNTOUCHED);
    Neuron::ByteWriter writer{std::span{backing}.first(3)};

    Assert::IsFalse(writer.WriteBytes(PAYLOAD));
    Assert::IsTrue(writer.Faulted());
    Assert::AreEqual(static_cast<std::size_t>(0), writer.WrittenBytes());
    for (const std::byte value : backing)
    {
      Assert::AreEqual(Octet(UNTOUCHED), Octet(value));
    }
  }
};

TEST_CLASS(ByteCodecReaderBounds)
{
public:
  /// Again a subspan of a larger array, and again that is the whole point: the bytes just past
  /// the reader's end hold 0xCD, so a read that returns zero returned a DEFINED value instead of
  /// reading memory it was not given. A buffer sized exactly to the span cannot tell the two
  /// apart, which is the difference between failing and returning rubbish.
  TEST_METHOD(AReadPastTheEndYieldsZeroRatherThanRubbish)
  {
    std::array<std::byte, 8> backing{};
    backing.fill(UNTOUCHED);
    Neuron::ByteReader reader{std::span<const std::byte>{backing}.first(2)};

    Assert::AreEqual(std::uint8_t{0xCD}, reader.ReadUInt8());
    Assert::AreEqual(std::uint32_t{0}, reader.ReadUInt32());
    Assert::IsTrue(reader.Faulted());
  }

  TEST_METHOD(AShortReadDoesNotMoveTheCursor)
  {
    std::array<std::byte, 8> backing{};
    backing.fill(UNTOUCHED);
    Neuron::ByteReader reader{std::span<const std::byte>{backing}.first(3)};

    Assert::AreEqual(std::uint8_t{0xCD}, reader.ReadUInt8());
    Assert::AreEqual(static_cast<std::size_t>(1), reader.ConsumedBytes());

    Assert::AreEqual(std::uint32_t{0}, reader.ReadUInt32());
    Assert::AreEqual(static_cast<std::size_t>(1), reader.ConsumedBytes());
    Assert::AreEqual(static_cast<std::size_t>(2), reader.RemainingBytes());
  }

  TEST_METHOD(TheReadFaultLatchesOverALaterFieldThatWouldFit)
  {
    std::array<std::byte, 8> backing{};
    backing.fill(UNTOUCHED);
    Neuron::ByteReader reader{std::span<const std::byte>{backing}.first(3)};

    Assert::AreEqual(std::uint32_t{0}, reader.ReadUInt32());
    Assert::AreEqual(std::uint8_t{0}, reader.ReadUInt8());
    Assert::IsTrue(reader.Faulted());
    Assert::AreEqual(static_cast<std::size_t>(0), reader.ConsumedBytes());
  }

  TEST_METHOD(AnOversizedReadBytesLeavesTheDestinationAlone)
  {
    std::array<std::byte, 8> backing{};
    backing.fill(UNTOUCHED);
    Neuron::ByteReader reader{std::span<const std::byte>{backing}.first(2)};

    std::array<std::byte, 4> destination{};
    Assert::IsFalse(reader.ReadBytes(destination));
    Assert::IsTrue(reader.Faulted());
    Assert::AreEqual(static_cast<std::size_t>(0), reader.ConsumedBytes());
    for (const std::byte value : destination)
    {
      Assert::AreEqual(std::uint8_t{0}, Octet(value));
    }
  }

  /// A truncated packet is the ordinary case on a socket, not the exotic one: the decoder builds
  /// a record out of zeros and the ONE check at the end is what throws it away.
  TEST_METHOD(ATruncatedRecordDecodesToZerosAndOneFault)
  {
    std::array<std::byte, 8> backing{};
    backing.fill(UNTOUCHED);
    Neuron::ByteReader reader{std::span<const std::byte>{backing}.first(5)};

    const std::uint16_t identity = reader.ReadUInt16();
    const std::int16_t positionX = reader.ReadInt16();
    const std::int16_t positionY = reader.ReadInt16();

    Assert::AreEqual(std::uint16_t{0xCDCD}, identity);
    Assert::AreEqual(std::int16_t{-12851}, positionX); // 0xCDCD
    Assert::AreEqual(std::int16_t{0}, positionY);
    Assert::IsTrue(reader.Faulted());
  }
};

} // namespace NeuronCoreTests

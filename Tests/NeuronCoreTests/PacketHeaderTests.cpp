#include "pch.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace NeuronCoreTests
{

namespace
{
/// CppUnitTest streams its expected and actual values, and a scoped enumeration does not stream.
/// Comparing the underlying values reports "expected 2, actual 3" where IsTrue would report
/// "expected true" and leave the reader to work out which fault came back.
[[nodiscard]] std::uint8_t Code(Neuron::PacketFault _fault) noexcept
{
  return static_cast<std::uint8_t>(_fault);
}

[[nodiscard]] std::uint8_t Code(Neuron::PacketType _type) noexcept
{
  return static_cast<std::uint8_t>(_type);
}

[[nodiscard]] std::uint8_t Octet(std::byte _value) noexcept
{
  return std::to_integer<std::uint8_t>(_value);
}

/// A well-formed snapshot header, written out by hand rather than by PacketHeader::Write, so that
/// a test of the reader is a test of the reader.
constexpr std::array<std::byte, Neuron::PacketHeader::SIZE_BYTES> WELL_FORMED{
  static_cast<std::byte>(Neuron::PROTOCOL_VERSION), std::byte{1}, std::byte{0x02}, std::byte{0x01}, std::byte{0}, std::byte{1}};
} // namespace

TEST_CLASS(PacketHeaderWire)
{
public:
  /// The two sides must agree on these six bytes exactly, and one of them is Winsock while the
  /// other is a WinRT DatagramSocket -- so the layout is pinned against literals rather than
  /// against the writer that produced it.
  TEST_METHOD(TheHeaderIsSixBytesInAFixedOrder)
  {
    std::array<std::byte, Neuron::PacketHeader::SIZE_BYTES> buffer{};
    Neuron::ByteWriter writer{buffer};
    const Neuron::PacketHeader header{.type = Neuron::PacketType::Snapshot,
                                      .sequence = std::uint16_t{0x0102},
                                      .fragmentIndex = std::uint8_t{2},
                                      .fragmentCount = std::uint8_t{5}};

    Assert::IsTrue(header.Write(writer));

    Assert::AreEqual(static_cast<std::size_t>(6), Neuron::PacketHeader::SIZE_BYTES);
    Assert::AreEqual(static_cast<std::size_t>(6), writer.WrittenBytes());
    Assert::AreEqual(Neuron::PROTOCOL_VERSION, Octet(buffer[0]));
    Assert::AreEqual(std::uint8_t{1}, Octet(buffer[1]));
    Assert::AreEqual(std::uint8_t{0x02}, Octet(buffer[2])); // sequence, least significant first
    Assert::AreEqual(std::uint8_t{0x01}, Octet(buffer[3]));
    Assert::AreEqual(std::uint8_t{2}, Octet(buffer[4]));
    Assert::AreEqual(std::uint8_t{5}, Octet(buffer[5]));
  }

  TEST_METHOD(EveryFieldRoundTrips)
  {
    std::array<std::byte, Neuron::PacketHeader::SIZE_BYTES> buffer{};
    Neuron::ByteWriter writer{buffer};
    const Neuron::PacketHeader sent{.type = Neuron::PacketType::Command,
                                    .sequence = std::uint16_t{40000},
                                    .fragmentIndex = std::uint8_t{3},
                                    .fragmentCount = std::uint8_t{4}};
    Assert::IsTrue(sent.Write(writer));

    Neuron::ByteReader reader{buffer};
    Neuron::PacketHeader received{};
    Assert::AreEqual(Code(Neuron::PacketFault::None), Code(Neuron::PacketHeader::Read(reader, received)));

    Assert::AreEqual(Neuron::PROTOCOL_VERSION, received.protocolVersion);
    Assert::AreEqual(Code(Neuron::PacketType::Command), Code(received.type));
    Assert::AreEqual(std::uint16_t{40000}, received.sequence);
    Assert::AreEqual(std::uint8_t{3}, received.fragmentIndex);
    Assert::AreEqual(std::uint8_t{4}, received.fragmentCount);
  }

  /// The header is a prefix, not a packet: the reader must be left exactly on the payload so the
  /// snapshot decoder can carry straight on from where this one stopped.
  TEST_METHOD(TheReaderIsLeftPositionedOnThePayload)
  {
    std::array<std::byte, Neuron::PacketHeader::SIZE_BYTES + 2> buffer{};
    Neuron::ByteWriter writer{buffer};
    const Neuron::PacketHeader header{.type = Neuron::PacketType::Heartbeat, .sequence = std::uint16_t{9}};
    Assert::IsTrue(header.Write(writer));
    Assert::IsTrue(writer.WriteUInt16(std::uint16_t{0xBEEF}));

    Neuron::ByteReader reader{buffer};
    Neuron::PacketHeader received{};
    Assert::AreEqual(Code(Neuron::PacketFault::None), Code(Neuron::PacketHeader::Read(reader, received)));
    Assert::AreEqual(Neuron::PacketHeader::SIZE_BYTES, reader.ConsumedBytes());
    Assert::AreEqual(std::uint16_t{0xBEEF}, reader.ReadUInt16());
  }

  TEST_METHOD(AHandWrittenHeaderDecodes)
  {
    Neuron::ByteReader reader{WELL_FORMED};
    Neuron::PacketHeader received{};

    Assert::AreEqual(Code(Neuron::PacketFault::None), Code(Neuron::PacketHeader::Read(reader, received)));
    Assert::AreEqual(Code(Neuron::PacketType::Snapshot), Code(received.type));
    Assert::AreEqual(std::uint16_t{0x0102}, received.sequence);
  }
};

TEST_CLASS(PacketHeaderRejection)
{
public:
  /// The requirement is not that a foreign build is refused -- it is that the refusal SAYS SO.
  /// A version mismatch is somebody running an old binary and is worth reporting once; the other
  /// three faults are a corrupt or hostile datagram and are worth nothing but a counter.
  TEST_METHOD(AMismatchedVersionIsItsOwnFault)
  {
    auto wire = WELL_FORMED;
    wire[0] = static_cast<std::byte>(Neuron::PROTOCOL_VERSION + 1);

    Neuron::ByteReader reader{wire};
    Neuron::PacketHeader received{};
    Assert::AreEqual(Code(Neuron::PacketFault::VersionMismatch), Code(Neuron::PacketHeader::Read(reader, received)));
  }

  /// Everything after byte zero belongs to a layout this build does not know, so naming it a bad
  /// type or an incoherent fragment count would be a fault that is not there. This packet is
  /// wrong in three ways and only the first one may be reported.
  TEST_METHOD(TheVersionIsCheckedBeforeAnyOtherFieldIsInterpreted)
  {
    constexpr std::array<std::byte, Neuron::PacketHeader::SIZE_BYTES> FOREIGN{
      static_cast<std::byte>(Neuron::PROTOCOL_VERSION + 1), std::byte{0xFF}, std::byte{0}, std::byte{0}, std::byte{9}, std::byte{0}};

    Neuron::ByteReader reader{FOREIGN};
    Neuron::PacketHeader received{};
    Assert::AreEqual(Code(Neuron::PacketFault::VersionMismatch), Code(Neuron::PacketHeader::Read(reader, received)));
  }

  TEST_METHOD(AnUnknownTypeIsItsOwnFault)
  {
    auto wire = WELL_FORMED;
    wire[1] = std::byte{0xFF};

    Neuron::ByteReader reader{wire};
    Neuron::PacketHeader received{};
    Assert::AreEqual(Code(Neuron::PacketFault::UnknownType), Code(Neuron::PacketHeader::Read(reader, received)));
  }

  /// Zero is not a packet type, so an unset header and a zero-filled buffer never look like a
  /// heartbeat. Here that is the difference between a caught mistake and a plausible one.
  TEST_METHOD(TypeZeroIsNotAType)
  {
    auto wire = WELL_FORMED;
    wire[1] = std::byte{0};

    Neuron::ByteReader reader{wire};
    Neuron::PacketHeader received{};
    Assert::AreEqual(Code(Neuron::PacketFault::UnknownType), Code(Neuron::PacketHeader::Read(reader, received)));
  }

  TEST_METHOD(AZeroFragmentCountIsIncoherent)
  {
    auto wire = WELL_FORMED;
    wire[5] = std::byte{0};

    Neuron::ByteReader reader{wire};
    Neuron::PacketHeader received{};
    Assert::AreEqual(Code(Neuron::PacketFault::BadFragmentation), Code(Neuron::PacketHeader::Read(reader, received)));
  }

  TEST_METHOD(AFragmentIndexAtOrPastTheCountIsIncoherent)
  {
    auto wire = WELL_FORMED;
    wire[4] = std::byte{4};
    wire[5] = std::byte{4};

    Neuron::ByteReader reader{wire};
    Neuron::PacketHeader received{};
    Assert::AreEqual(Code(Neuron::PacketFault::BadFragmentation), Code(Neuron::PacketHeader::Read(reader, received)));
  }

  TEST_METHOD(AShortDatagramIsTruncatedRatherThanMalformed)
  {
    Neuron::ByteReader reader{std::span<const std::byte>{WELL_FORMED}.first(Neuron::PacketHeader::SIZE_BYTES - 1)};
    Neuron::PacketHeader received{};
    Assert::AreEqual(Code(Neuron::PacketFault::Truncated), Code(Neuron::PacketHeader::Read(reader, received)));
  }

  TEST_METHOD(AnEmptyDatagramIsTruncated)
  {
    Neuron::ByteReader reader{std::span<const std::byte>{}};
    Neuron::PacketHeader received{};
    Assert::AreEqual(Code(Neuron::PacketFault::Truncated), Code(Neuron::PacketHeader::Read(reader, received)));
  }

  /// Six zero bytes reach the version check first and fail there, which is the right answer for
  /// the wrong-sounding reason: version 0 is not this build's. It is asserted because a zeroed
  /// buffer is what a bug delivers, and "rejected" is the property that matters.
  TEST_METHOD(AnAllZeroDatagramIsRejected)
  {
    constexpr std::array<std::byte, Neuron::PacketHeader::SIZE_BYTES> ZEROS{};

    Neuron::ByteReader reader{ZEROS};
    Neuron::PacketHeader received{};
    Assert::AreNotEqual(Code(Neuron::PacketFault::None), Code(Neuron::PacketHeader::Read(reader, received)));
  }

  /// A failed read must not half-fill the caller's header, or a dropped packet still moves the
  /// sequence the caller was tracking.
  TEST_METHOD(ARejectedHeaderLeavesTheDestinationAlone)
  {
    auto wire = WELL_FORMED;
    wire[1] = std::byte{0xFF};

    Neuron::ByteReader reader{wire};
    Neuron::PacketHeader received{.type = Neuron::PacketType::Heartbeat, .sequence = std::uint16_t{77}};
    Assert::AreEqual(Code(Neuron::PacketFault::UnknownType), Code(Neuron::PacketHeader::Read(reader, received)));

    Assert::AreEqual(Code(Neuron::PacketType::Heartbeat), Code(received.type));
    Assert::AreEqual(std::uint16_t{77}, received.sequence);
  }
};

TEST_CLASS(PacketHeaderWriting)
{
public:
  /// The default-constructed type is zero, so a header nobody finished composing is refused at
  /// the send site rather than by the peer.
  TEST_METHOD(AnUnsetTypeIsRefusedBeforeAByteIsWritten)
  {
    std::array<std::byte, Neuron::PacketHeader::SIZE_BYTES> buffer{};
    Neuron::ByteWriter writer{buffer};
    const Neuron::PacketHeader header{};

    Assert::IsFalse(header.Write(writer));
    Assert::AreEqual(static_cast<std::size_t>(0), writer.WrittenBytes());
    Assert::IsFalse(writer.Faulted());
  }

  TEST_METHOD(IncoherentFragmentFieldsAreRefusedBeforeAByteIsWritten)
  {
    std::array<std::byte, Neuron::PacketHeader::SIZE_BYTES> buffer{};
    Neuron::ByteWriter writer{buffer};
    const Neuron::PacketHeader header{.type = Neuron::PacketType::Snapshot,
                                      .sequence = std::uint16_t{1},
                                      .fragmentIndex = std::uint8_t{2},
                                      .fragmentCount = std::uint8_t{2}};

    Assert::IsFalse(header.Write(writer));
    Assert::AreEqual(static_cast<std::size_t>(0), writer.WrittenBytes());
  }

  TEST_METHOD(ABufferWithNoRoomFails)
  {
    std::array<std::byte, Neuron::PacketHeader::SIZE_BYTES - 1> buffer{};
    Neuron::ByteWriter writer{buffer};
    const Neuron::PacketHeader header{.type = Neuron::PacketType::Snapshot, .sequence = std::uint16_t{1}};

    Assert::IsFalse(header.Write(writer));
    Assert::IsTrue(writer.Faulted());
  }
};

TEST_CLASS(PacketHeaderFragmentation)
{
public:
  /// The MVP never fragments, so the single-fragment answer has to be reachable without a
  /// reassembler -- there is not one until M4.3, and ADR-003 does not want one before then.
  TEST_METHOD(ASingleFragmentPacketIsCompleteOnItsOwn)
  {
    const Neuron::PacketHeader header{.type = Neuron::PacketType::Snapshot, .sequence = std::uint16_t{1}};

    Assert::AreEqual(std::uint8_t{0}, header.fragmentIndex);
    Assert::AreEqual(std::uint8_t{1}, header.fragmentCount);
    Assert::IsTrue(header.IsSingleFragment());
  }

  TEST_METHOD(AMultiFragmentPacketIsNot)
  {
    const Neuron::PacketHeader first{.type = Neuron::PacketType::Snapshot,
                                     .sequence = std::uint16_t{1},
                                     .fragmentIndex = std::uint8_t{0},
                                     .fragmentCount = std::uint8_t{2}};
    const Neuron::PacketHeader second{.type = Neuron::PacketType::Snapshot,
                                      .sequence = std::uint16_t{1},
                                      .fragmentIndex = std::uint8_t{1},
                                      .fragmentCount = std::uint8_t{2}};

    Assert::IsFalse(first.IsSingleFragment());
    Assert::IsFalse(second.IsSingleFragment());
  }

  /// Both fragments of one packet carry the same sequence -- ADR-003 reassembles all-or-nothing
  /// under one sequence number -- so a decoder can group them before M4.3 exists to do it.
  TEST_METHOD(FragmentsOfOnePacketShareASequenceAndRoundTrip)
  {
    std::array<std::byte, Neuron::PacketHeader::SIZE_BYTES * 2> buffer{};
    Neuron::ByteWriter writer{buffer};
    for (std::uint8_t index = 0; index < 2; ++index)
    {
      const Neuron::PacketHeader header{
        .type = Neuron::PacketType::Snapshot, .sequence = std::uint16_t{513}, .fragmentIndex = index, .fragmentCount = std::uint8_t{2}};
      Assert::IsTrue(header.Write(writer));
    }

    Neuron::ByteReader reader{buffer};
    for (std::uint8_t index = 0; index < 2; ++index)
    {
      Neuron::PacketHeader received{};
      Assert::AreEqual(Code(Neuron::PacketFault::None), Code(Neuron::PacketHeader::Read(reader, received)));
      Assert::AreEqual(std::uint16_t{513}, received.sequence);
      Assert::AreEqual(index, received.fragmentIndex);
      Assert::AreEqual(std::uint8_t{2}, received.fragmentCount);
    }
  }
};

} // namespace NeuronCoreTests

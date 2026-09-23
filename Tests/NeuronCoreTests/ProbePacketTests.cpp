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
[[nodiscard]] std::uint8_t Code(Neuron::PacketFault _fault) noexcept
{
  return static_cast<std::uint8_t>(_fault);
}
} // namespace

/// M0.5 SCAFFOLDING, and it goes when ProbePacket goes. It is here because the gate's numbers are
/// read out of a log that this record wrote: a sequence or a timestamp that did not survive the
/// wire would show up as loss and jitter that the network never caused, and the measurement would
/// be wrong in the direction that makes the answer look worse than it is.
TEST_CLASS(ProbePacketRecord)
{
public:
  TEST_METHOD(EveryFieldRoundTrips)
  {
    std::array<std::byte, Neuron::ProbePacket::SIZE_BYTES> buffer{};
    const Neuron::ProbePacket sent{.sequence = 40000, .sentAtMs = 0x0123456789ABCDEFull};

    Neuron::ByteWriter writer{buffer};
    Assert::IsTrue(sent.Write(writer));
    Assert::AreEqual(Neuron::ProbePacket::SIZE_BYTES, writer.WrittenBytes());

    Neuron::ByteReader reader{buffer};
    Neuron::ProbePacket received{};
    Assert::AreEqual(Code(Neuron::PacketFault::None), Code(Neuron::ProbePacket::Read(reader, received)));
    Assert::AreEqual(sent.sequence, received.sequence);
    Assert::AreEqual(sent.sentAtMs, received.sentAtMs);
  }

  TEST_METHOD(TheSequenceSurvivesTheWholeUnsignedRange)
  {
    // The header's sequence is sixteen bits, so a run longer than 65536 packets wraps. The report
    // script unwraps it; what this pins is that the wrap is the only thing it has to handle.
    for (const std::uint16_t sequence : {std::uint16_t{0}, std::uint16_t{1}, std::uint16_t{65534}, std::uint16_t{65535}})
    {
      std::array<std::byte, Neuron::ProbePacket::SIZE_BYTES> buffer{};
      Neuron::ByteWriter writer{buffer};
      Assert::IsTrue(Neuron::ProbePacket{.sequence = sequence}.Write(writer));

      Neuron::ByteReader reader{buffer};
      Neuron::ProbePacket received{};
      Assert::AreEqual(Code(Neuron::PacketFault::None), Code(Neuron::ProbePacket::Read(reader, received)));
      Assert::AreEqual(sequence, received.sequence);
    }
  }

  TEST_METHOD(ABufferOneByteShortRefusesRatherThanWritingAHeader)
  {
    std::array<std::byte, Neuron::ProbePacket::SIZE_BYTES - 1> buffer{};
    Neuron::ByteWriter writer{buffer};
    Assert::IsFalse(Neuron::ProbePacket{.sequence = 7, .sentAtMs = 9}.Write(writer));
  }

  TEST_METHOD(ATruncatedPayloadIsTruncatedRatherThanADecodedZero)
  {
    std::array<std::byte, Neuron::ProbePacket::SIZE_BYTES> buffer{};
    Neuron::ByteWriter writer{buffer};
    Assert::IsTrue(Neuron::ProbePacket{.sequence = 11, .sentAtMs = 22}.Write(writer));

    // The header, and only half the timestamp behind it.
    const std::span<const std::byte> truncated{buffer.data(), Neuron::PacketHeader::SIZE_BYTES + 4};
    Neuron::ByteReader reader{truncated};
    Neuron::ProbePacket received{};
    Assert::AreEqual(Code(Neuron::PacketFault::Truncated), Code(Neuron::ProbePacket::Read(reader, received)));
    Assert::AreEqual(std::uint16_t{0}, received.sequence, L"a refused packet must leave the destination alone");
    Assert::AreEqual(std::uint64_t{0}, received.sentAtMs, L"a refused packet must leave the destination alone");
  }

  TEST_METHOD(ARealUpdateIsRefusedRatherThanReadAsAProbe)
  {
    std::array<std::byte, Neuron::ProbePacket::SIZE_BYTES> buffer{};
    Neuron::ByteWriter writer{buffer};
    const Neuron::PacketHeader update{.type = Neuron::PacketType::Update, .sequence = 5};
    Assert::IsTrue(update.Write(writer));
    Assert::IsTrue(writer.WriteUInt64(1234));

    Neuron::ByteReader reader{buffer};
    Neuron::ProbePacket received{};
    Assert::AreEqual(Code(Neuron::PacketFault::UnknownType), Code(Neuron::ProbePacket::Read(reader, received)));
  }
};

} // namespace NeuronCoreTests

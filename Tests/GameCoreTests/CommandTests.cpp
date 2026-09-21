#include "pch.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameCoreTests
{

namespace
{
inline constexpr std::size_t PAYLOAD_BYTES = 1232;
inline constexpr std::size_t SCRATCH_BYTES = 4096;

[[nodiscard]] std::uint8_t Code(Outpost::CommandFault _fault) noexcept
{
  return static_cast<std::uint8_t>(_fault);
}

[[nodiscard]] Outpost::Command MakeCommand(std::uint16_t _sequence, std::size_t _selectionCount)
{
  Outpost::Command command{};
  command.sequence = _sequence;
  command.type = Outpost::CommandType::MoveTo;
  command.targetX = 1234;
  command.targetY = -567;
  for (std::size_t index = 0; index < _selectionCount; ++index)
  {
    command.selection.push_back(Outpost::PackIdentity(static_cast<std::uint16_t>(index), 3));
  }
  return command;
}
} // namespace

TEST_CLASS(CommandCodec)
{
public:
  TEST_METHOD(ACommandIsEightBytesPlusTwoPerIdentity)
  {
    Assert::AreEqual(std::size_t{8}, Outpost::Command::FIXED_BYTES);
    Assert::AreEqual(std::size_t{8}, Outpost::EncodedSize(MakeCommand(1, 0)));
    Assert::AreEqual(std::size_t{28}, Outpost::EncodedSize(MakeCommand(1, 10)));

    // ADR-003's peak selection, which is the figure its amplification argument rests on.
    Assert::AreEqual(std::size_t{228}, Outpost::EncodedSize(MakeCommand(1, 110)));
  }

  TEST_METHOD(ThePacketHeaderIsEightBytes)
  {
    // The transport's six, plus the player identity and the command count.
    Assert::AreEqual(std::size_t{8}, Outpost::CommandPacket::HEADER_BYTES);
    Outpost::CommandPacket empty{};
    empty.player = 1;
    Assert::AreEqual(std::size_t{8}, Outpost::EncodedSize(empty));
  }

  TEST_METHOD(EveryFieldRoundTrips)
  {
    Outpost::CommandPacket sent{};
    sent.sequence = 777;
    sent.player = 2;
    sent.commands.push_back(MakeCommand(11, 4));
    sent.commands.push_back(
      Outpost::Command{.sequence = 12, .type = Outpost::CommandType::Attack, .targetX = 99, .targetY = 0, .selection = {5, 6}});

    std::vector<std::byte> buffer(SCRATCH_BYTES);
    Neuron::ByteWriter writer{buffer};
    Assert::IsTrue(Outpost::Encode(sent, writer));
    Assert::AreEqual(Outpost::EncodedSize(sent), writer.WrittenBytes());

    Neuron::ByteReader reader{std::span<const std::byte>{buffer.data(), writer.WrittenBytes()}};
    Outpost::CommandPacket received;
    Assert::AreEqual(Code(Outpost::CommandFault::None), Code(Outpost::Decode(reader, received)));

    Assert::AreEqual(sent.sequence, received.sequence);
    Assert::AreEqual(sent.player, received.player);
    Assert::AreEqual(sent.commands.size(), received.commands.size());
    Assert::IsTrue(sent.commands[0] == received.commands[0]);
    Assert::IsTrue(sent.commands[1] == received.commands[1]);
    Assert::AreEqual(std::uint16_t{99}, received.commands[1].TargetEntity());
  }

  TEST_METHOD(AnEmptySelectionRoundTrips)
  {
    // Legal on the wire and refused at intake. The codec's job is to carry what was written.
    Outpost::CommandPacket sent{};
    sent.player = 1;
    sent.commands.push_back(MakeCommand(1, 0));

    std::array<std::byte, 64> buffer{};
    Neuron::ByteWriter writer{buffer};
    Assert::IsTrue(Outpost::Encode(sent, writer));
    Neuron::ByteReader reader{std::span<const std::byte>{buffer.data(), writer.WrittenBytes()}};
    Outpost::CommandPacket received;
    Assert::AreEqual(Code(Outpost::CommandFault::None), Code(Outpost::Decode(reader, received)));
    Assert::AreEqual(std::size_t{0}, received.commands[0].selection.size());
  }
};

TEST_CLASS(CommandRefusals)
{
public:
  TEST_METHOD(ATruncatedPacketIsTruncated)
  {
    Outpost::CommandPacket sent{};
    sent.player = 1;
    sent.commands.push_back(MakeCommand(3, 6));

    std::vector<std::byte> buffer(SCRATCH_BYTES);
    Neuron::ByteWriter writer{buffer};
    Assert::IsTrue(Outpost::Encode(sent, writer));

    for (std::size_t length = 0; length < writer.WrittenBytes(); ++length)
    {
      Neuron::ByteReader reader{std::span<const std::byte>{buffer.data(), length}};
      Outpost::CommandPacket received;
      Assert::AreNotEqual(Code(Outpost::CommandFault::None), Code(Outpost::Decode(reader, received)));
      Assert::AreEqual(std::size_t{0}, received.commands.size(), L"a refused packet must leave the destination alone");
    }
  }

  TEST_METHOD(ASnapshotIsNotACommandPacket)
  {
    std::array<std::byte, 64> buffer{};
    Neuron::ByteWriter writer{buffer};
    Assert::IsTrue((Neuron::PacketHeader{.type = Neuron::PacketType::Snapshot, .sequence = 1}).Write(writer));
    Neuron::ByteReader reader{buffer};
    Outpost::CommandPacket received;
    Assert::AreEqual(Code(Outpost::CommandFault::WrongType), Code(Outpost::Decode(reader, received)));
  }

  TEST_METHOD(AnUnknownTypeIsMalformedRatherThanActedOn)
  {
    std::array<std::byte, 64> buffer{};
    Neuron::ByteWriter writer{buffer};
    Assert::IsTrue((Neuron::PacketHeader{.type = Neuron::PacketType::Command, .sequence = 1}).Write(writer));
    Assert::IsTrue(writer.WriteUInt8(1));
    Assert::IsTrue(writer.WriteUInt8(1));
    Assert::IsTrue(writer.WriteUInt16(5));
    Assert::IsTrue(writer.WriteUInt8(200));
    Assert::IsTrue(writer.WriteInt16(0));
    Assert::IsTrue(writer.WriteInt16(0));
    Assert::IsTrue(writer.WriteUInt8(0));

    Neuron::ByteReader reader{std::span<const std::byte>{buffer.data(), writer.WrittenBytes()}};
    Outpost::CommandPacket received;
    Assert::AreEqual(Code(Outpost::CommandFault::Malformed), Code(Outpost::Decode(reader, received)));
  }

  TEST_METHOD(ASelectionCountLongerThanTheDatagramIsRefusedBeforeItIsReserved)
  {
    // ADR-003's amplification, in its smallest form: the count says 255 identities and the
    // datagram holds none of them.
    std::array<std::byte, 32> buffer{};
    Neuron::ByteWriter writer{buffer};
    Assert::IsTrue((Neuron::PacketHeader{.type = Neuron::PacketType::Command, .sequence = 1}).Write(writer));
    Assert::IsTrue(writer.WriteUInt8(1));
    Assert::IsTrue(writer.WriteUInt8(1));
    Assert::IsTrue(writer.WriteUInt16(5));
    Assert::IsTrue(writer.WriteUInt8(static_cast<std::uint8_t>(Outpost::CommandType::MoveTo)));
    Assert::IsTrue(writer.WriteInt16(0));
    Assert::IsTrue(writer.WriteInt16(0));
    Assert::IsTrue(writer.WriteUInt8(255));

    Neuron::ByteReader reader{std::span<const std::byte>{buffer.data(), writer.WrittenBytes()}};
    Outpost::CommandPacket received;
    Assert::AreEqual(Code(Outpost::CommandFault::Malformed), Code(Outpost::Decode(reader, received)));
  }

  TEST_METHOD(AnUnknownTypeIsNotEncodedEither)
  {
    Outpost::CommandPacket sent{};
    sent.player = 1;
    sent.commands.push_back(Outpost::Command{.sequence = 1, .type = static_cast<Outpost::CommandType>(0)});
    std::array<std::byte, 64> buffer{};
    Neuron::ByteWriter writer{buffer};
    Assert::IsFalse(Outpost::Encode(sent, writer));
  }
};

TEST_CLASS(TheRetransmitWindow)
{
public:
  TEST_METHOD(AtAFullSelectionFiveFitAndASixthWouldFragment)
  {
    // `TechnicalDesign.md` section 4's figure, measured rather than restated. This is the number
    // the oldest-first rule exists to bound.
    std::vector<Outpost::Command> outstanding;
    for (std::uint16_t sequence = 1; sequence <= 8; ++sequence)
    {
      outstanding.push_back(MakeCommand(sequence, 110));
    }

    Outpost::CommandPacket packet{};
    const std::size_t packed = Outpost::FillOldestFirst(outstanding, PAYLOAD_BYTES, packet);

    Logger::WriteMessage((std::wstring{L"COMMAND PACKET at a 110-identity selection: "} + std::to_wstring(packed) + L" commands fit in " +
                          std::to_wstring(Outpost::EncodedSize(packet)) + L" bytes of " + std::to_wstring(PAYLOAD_BYTES) + L"\n")
                           .c_str());

    Assert::AreEqual(std::size_t{5}, packed);
    Assert::IsTrue(Outpost::EncodedSize(packet) <= PAYLOAD_BYTES);
    Assert::IsTrue((Outpost::EncodedSize(packet) + Outpost::EncodedSize(outstanding[5])) > PAYLOAD_BYTES,
                   L"a sixth should not have fitted");
  }

  TEST_METHOD(ItStopsRatherThanSkipping)
  {
    // THE PROPERTY THAT MATTERS, not the count. Skipping a command that will not fit and taking a
    // later one leaves a gap in the sequence, and the host discards a gap rather than waiting for
    // it -- so a skipped order is lost rather than delayed.
    std::vector<Outpost::Command> outstanding;
    outstanding.push_back(MakeCommand(1, 110));
    outstanding.push_back(MakeCommand(2, 110));
    outstanding.push_back(MakeCommand(3, 0));

    Outpost::CommandPacket packet{};
    const std::size_t packed = Outpost::FillOldestFirst(outstanding, 300, packet);
    Assert::AreEqual(std::size_t{1}, packed);
    Assert::AreEqual(std::uint16_t{1}, packet.commands[0].sequence, L"the oldest must go first");
  }

  TEST_METHOD(EverythingFitsWhenTheWindowIsShallow)
  {
    std::vector<Outpost::Command> outstanding{MakeCommand(1, 2), MakeCommand(2, 2)};
    Outpost::CommandPacket packet{};
    Assert::AreEqual(std::size_t{2}, Outpost::FillOldestFirst(outstanding, PAYLOAD_BYTES, packet));
  }

  TEST_METHOD(NothingOutstandingPacksNothing)
  {
    Outpost::CommandPacket packet{};
    Assert::AreEqual(std::size_t{0}, Outpost::FillOldestFirst({}, PAYLOAD_BYTES, packet));
    Assert::AreEqual(std::size_t{0}, packet.commands.size());
  }
};

TEST_CLASS(PlayAreaClamp)
{
public:
  TEST_METHOD(APointInsideIsUntouched)
  {
    const Neuron::Vec2 inside{.x = 1000, .y = -1000};
    Assert::IsTrue(Outpost::ClampToPlayArea(inside) == inside);
  }

  TEST_METHOD(APointOutsideComesBackToTheEdge)
  {
    const Neuron::Vec2 clamped =
      Outpost::ClampToPlayArea(Neuron::Vec2{.x = Outpost::PLAY_AREA_HALF_EXTENT + 5000, .y = -Outpost::PLAY_AREA_HALF_EXTENT - 5000});
    Assert::AreEqual(Outpost::PLAY_AREA_HALF_EXTENT, clamped.x);
    Assert::AreEqual(static_cast<Neuron::Fixed>(-Outpost::PLAY_AREA_HALF_EXTENT), clamped.y);
  }

  TEST_METHOD(TheExtentIsTheSixteenThousandUnitSquare)
  {
    // 8,192 world units either side, at 256 steps to a unit.
    Assert::AreEqual(Neuron::Fixed{2097152}, Outpost::PLAY_AREA_HALF_EXTENT);
    Assert::AreEqual(Neuron::Fixed{8192}, Outpost::PLAY_AREA_HALF_EXTENT / Neuron::FIXED_ONE);
  }

  TEST_METHOD(AWireTargetIsAlreadyInsideBeforeTheClampLooks)
  {
    // Why the clamp is belt and braces today: an int16 wire position times the 64-unit step
    // cannot leave the play area. The clamp is written because that is true by a coincidence of
    // two field widths, and coincidences stop being true.
    for (const std::int16_t wire : {std::int16_t{-32768}, std::int16_t{-1}, std::int16_t{0}, std::int16_t{32767}})
    {
      const Neuron::Fixed value = Outpost::DequantizePosition(wire);
      Assert::IsTrue((value >= -Outpost::PLAY_AREA_HALF_EXTENT) && (value <= Outpost::PLAY_AREA_HALF_EXTENT));
    }
  }
};

} // namespace GameCoreTests

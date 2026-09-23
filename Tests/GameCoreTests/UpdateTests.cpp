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
/// Comfortably past any update, so a test measuring a size is never measuring a buffer that ran out.
inline constexpr std::size_t SCRATCH_BYTES = 4096;

[[nodiscard]] std::uint8_t Code(Outpost::UpdateFault _fault) noexcept
{
  return static_cast<std::uint8_t>(_fault);
}

/// Records that differ in every field, so a round trip that crossed two of them over would fail.
[[nodiscard]] Outpost::EntityRecord MakeRecord(std::size_t _entity)
{
  return Outpost::EntityRecord{
    .identity = Outpost::PackIdentity(static_cast<std::uint16_t>(_entity * 97), static_cast<std::uint16_t>((_entity % 255) + 1)),
    .owner = static_cast<Outpost::PlayerId>((_entity % 100) + 1),
    .positionX = static_cast<std::int16_t>(_entity * 13),
    .positionY = static_cast<std::int16_t>(-static_cast<std::int32_t>(_entity * 7)),
    .heading = static_cast<std::uint8_t>(_entity % 256),
    .hullPercentRemaining = static_cast<std::uint8_t>(_entity % 101),
    .designIdentity = static_cast<std::uint8_t>(_entity % 17),
    .flags = static_cast<std::uint8_t>(_entity % 32)};
}

[[nodiscard]] Outpost::Update MakeUpdate(std::size_t _records, std::size_t _removals, std::size_t _fires)
{
  Outpost::Update update;
  update.sequence = 4242;
  update.tick = 123456;
  update.liveEntityCount = 5500;
  update.own = Outpost::PlayerBlock{.credits = 1234, .lastCommandSequenceApplied = 77, .buildingDesign = 2, .buildProgressPercent = 41};

  for (std::size_t record = 0; record < _records; ++record)
  {
    update.records.push_back(MakeRecord(record));
  }
  for (std::size_t removal = 0; removal < _removals; ++removal)
  {
    update.removals.push_back(Outpost::PackIdentity(static_cast<std::uint16_t>(60000 + removal), 200));
  }
  for (std::size_t fire = 0; fire < _fires; ++fire)
  {
    update.fires.push_back(Outpost::FireEvent{.shooter = Outpost::PackIdentity(static_cast<std::uint16_t>(fire), 1),
                                              .target = Outpost::PackIdentity(static_cast<std::uint16_t>(40000 + fire), 9),
                                              .weapon = static_cast<std::uint8_t>(fire + 1)});
  }
  return update;
}

/// Encode, decode, and hand back both the recovered update and the byte count.
[[nodiscard]] std::size_t RoundTrip(const Outpost::Update& _sent, Outpost::Update& _outReceived)
{
  std::vector<std::byte> buffer(SCRATCH_BYTES);
  Neuron::ByteWriter writer{buffer};
  Assert::IsTrue(Outpost::Encode(_sent, writer), L"the update did not encode");

  const std::size_t written = writer.WrittenBytes();
  Assert::AreEqual(Outpost::EncodedSize(_sent), written, L"EncodedSize disagrees with the encoder");

  Neuron::ByteReader reader{std::span<const std::byte>{buffer.data(), written}};
  Assert::AreEqual(Code(Outpost::UpdateFault::None), Code(Outpost::Decode(reader, _outReceived)), L"it did not decode");
  return written;
}
} // namespace

TEST_CLASS(WireRecordWidths)
{
public:
  /// ADR-024's twelve bytes, and the order they go out in -- pinned against literals, because a writer
  /// and a reader that agree with each other about the wrong order pass every round trip.
  TEST_METHOD(AnEntityRecordIsTwelveBytesInAFixedOrder)
  {
    Assert::AreEqual(std::size_t{12}, Outpost::EntityRecord::SIZE_BYTES);

    const Outpost::EntityRecord record{.identity = 0x00ABCDEF,
                                       .owner = 0x11,
                                       .positionX = 0x2233,
                                       .positionY = 0x4455,
                                       .heading = 0x66,
                                       .hullPercentRemaining = 0x77,
                                       .designIdentity = 0x08,
                                       .flags = 0x09};
    std::array<std::byte, 64> buffer{};
    Neuron::ByteWriter writer{buffer};
    Assert::IsTrue(record.Write(writer));
    Assert::AreEqual(std::size_t{12}, writer.WrittenBytes(), L"the record is not the width ADR-024 states");

    const std::array<std::uint8_t, 12> expected{0xEF, 0xCD, 0xAB, 0x11, 0x33, 0x22, 0x55, 0x44, 0x66, 0x77, 0x08, 0x09};
    for (std::size_t index = 0; index < expected.size(); ++index)
    {
      Assert::AreEqual(expected[index], std::to_integer<std::uint8_t>(buffer[index]));
    }
  }

  /// **TWENTY-ONE BYTES AT ANY PLAYER COUNT.** The full snapshot carried a block per player; an update
  /// carries the recipient's alone (ADR-024), which is what lets a hundred players fit.
  TEST_METHOD(TheHeaderIsTwentyOneBytes)
  {
    Assert::AreEqual(std::size_t{21}, Outpost::UPDATE_HEADER_BYTES);
    Assert::AreEqual(std::size_t{21}, Outpost::EncodedSize(MakeUpdate(0, 0, 0)));

    Outpost::Update received;
    Assert::AreEqual(std::size_t{21}, RoundTrip(MakeUpdate(0, 0, 0), received));
  }

  TEST_METHOD(AFireEventIsSevenBytesAndARemovalThree)
  {
    Assert::AreEqual(std::size_t{7}, Outpost::FireEvent::SIZE_BYTES);
    Assert::AreEqual(std::size_t{3}, Outpost::REMOVAL_BYTES);
    Assert::AreEqual(Outpost::EncodedSize(MakeUpdate(0, 0, 0)) + 7, Outpost::EncodedSize(MakeUpdate(0, 0, 1)));
    Assert::AreEqual(Outpost::EncodedSize(MakeUpdate(0, 0, 0)) + 3, Outpost::EncodedSize(MakeUpdate(0, 1, 0)));
  }
};

TEST_CLASS(IdentityPacking)
{
public:
  TEST_METHOD(IndexAndGenerationSurviveThePack)
  {
    for (std::uint32_t index = 0; index <= Outpost::WIRE_INDEX_MASK; index += 257)
    {
      for (std::uint32_t generation = 0; generation <= Outpost::WIRE_GENERATION_MASK; generation += 5)
      {
        const Outpost::WireIdentity packed =
          Outpost::PackIdentity(static_cast<std::uint16_t>(index), static_cast<std::uint16_t>(generation));
        Assert::AreEqual(static_cast<std::uint16_t>(index), Outpost::IndexOf(packed));
        Assert::AreEqual(static_cast<std::uint16_t>(generation), Outpost::GenerationOf(packed));
        Assert::IsTrue(packed <= 0x00FFFFFFu, L"an identity must fit three bytes");
      }
    }
  }

  /// Sixteen and eight since ADR-024: every slot the store can hand out is nameable on the wire.
  TEST_METHOD(TheSplitIsSixteenAndEight)
  {
    Assert::AreEqual(std::uint32_t{16}, Outpost::WIRE_INDEX_BITS);
    Assert::AreEqual(std::uint32_t{8}, Outpost::WIRE_GENERATION_BITS);
    Assert::AreEqual(std::uint32_t{65535}, Outpost::WIRE_INDEX_MASK);
    Assert::AreEqual(std::uint32_t{255}, Outpost::WIRE_GENERATION_MASK);
  }

  /// A generation past eight bits wraps into the eight the wire carries, and the index is untouched by
  /// it -- a generation that leaked into the index would name a different entity.
  TEST_METHOD(AWideGenerationDoesNotLeakIntoTheIndex)
  {
    const Outpost::WireIdentity packed = Outpost::PackIdentity(65535, 0x1234);
    Assert::AreEqual(std::uint16_t{65535}, Outpost::IndexOf(packed));
    Assert::AreEqual(std::uint16_t{0x34}, Outpost::GenerationOf(packed));
  }
};

TEST_CLASS(PositionQuantization)
{
public:
  TEST_METHOD(OneStepIsAQuarterOfAWorldUnit)
  {
    // Fixed carries 256 steps to a world unit, the wire carries four.
    Assert::AreEqual(Neuron::Fixed{64}, Outpost::POSITION_WIRE_STEP);
    Assert::AreEqual(Neuron::Fixed{256}, Neuron::FIXED_ONE);
  }

  TEST_METHOD(AValueComesBackWithinHalfAStep)
  {
    for (Neuron::Fixed value = -2097152; value < 2097152; value += 4099)
    {
      const Neuron::Fixed recovered = Outpost::DequantizePosition(Outpost::QuantizePosition(value));
      const Neuron::Fixed error = (recovered > value) ? (recovered - value) : (value - recovered);
      Assert::IsTrue(error <= (Outpost::POSITION_WIRE_STEP / 2), L"quantization lost more than half a step");
    }
  }

  TEST_METHOD(ItRoundsSymmetricallyAboutZero)
  {
    // A quantizer that truncated would pull every ship in the match toward the origin.
    Assert::AreEqual(std::int16_t{1}, Outpost::QuantizePosition(32));
    Assert::AreEqual(std::int16_t{-1}, Outpost::QuantizePosition(-32));
    Assert::AreEqual(std::int16_t{0}, Outpost::QuantizePosition(31));
    Assert::AreEqual(std::int16_t{0}, Outpost::QuantizePosition(-31));
  }

  TEST_METHOD(TheEdgesSaturateRatherThanWrap)
  {
    // The play area's half extent is exactly 32,768 wire steps and an int16 stops at 32,767, so
    // the positive corner saturates by one step. Stated in the header; asserted here.
    Assert::AreEqual(std::int16_t{32767}, Outpost::QuantizePosition(2097152));
    Assert::AreEqual(std::int16_t{-32768}, Outpost::QuantizePosition(-2097152));
    Assert::AreEqual(std::int16_t{32767}, Outpost::QuantizePosition(2147483647));
    Assert::AreEqual(std::int16_t{-32768}, Outpost::QuantizePosition(-2147483647 - 1));
  }
};

TEST_CLASS(UpdateRoundTrip)
{
public:
  TEST_METHOD(EveryFieldSurvives)
  {
    const Outpost::Update sent = MakeUpdate(40, 3, 2);
    Outpost::Update received;
    static_cast<void>(RoundTrip(sent, received));

    Assert::AreEqual(sent.sequence, received.sequence);
    Assert::AreEqual(sent.tick, received.tick);
    Assert::AreEqual(sent.liveEntityCount, received.liveEntityCount);
    Assert::IsTrue(sent.own == received.own, L"the own block changed");
    Assert::AreEqual(sent.records.size(), received.records.size());
    Assert::AreEqual(sent.removals.size(), received.removals.size());
    Assert::AreEqual(sent.fires.size(), received.fires.size());

    for (std::size_t index = 0; index < sent.records.size(); ++index)
    {
      Assert::IsTrue(sent.records[index] == received.records[index], L"a record changed");
    }
    for (std::size_t index = 0; index < sent.removals.size(); ++index)
    {
      Assert::AreEqual(sent.removals[index], received.removals[index], L"a removal changed");
    }
    for (std::size_t index = 0; index < sent.fires.size(); ++index)
    {
      Assert::IsTrue(sent.fires[index] == received.fires[index], L"a fire event changed");
    }
  }

  /// An owner past four is a byte like any other (ADR-024), which the two team bits it replaced could
  /// not say.
  TEST_METHOD(AnOwnerPastFourSurvives)
  {
    Outpost::Update sent = MakeUpdate(1, 0, 0);
    sent.records[0].owner = 200;
    Outpost::Update received;
    static_cast<void>(RoundTrip(sent, received));
    Assert::AreEqual(Outpost::PlayerId{200}, received.records[0].owner);
  }
};

TEST_CLASS(UpdateRefusals)
{
public:
  TEST_METHOD(ATruncatedDatagramIsRefused)
  {
    const Outpost::Update sent = MakeUpdate(20, 2, 2);
    std::vector<std::byte> buffer(SCRATCH_BYTES);
    Neuron::ByteWriter writer{buffer};
    Assert::IsTrue(Outpost::Encode(sent, writer));

    // Every prefix, because a decoder that only checks at the end reads past one of them.
    for (std::size_t length = 0; length < writer.WrittenBytes(); ++length)
    {
      Neuron::ByteReader reader{std::span<const std::byte>{buffer.data(), length}};
      Outpost::Update received;
      Assert::AreNotEqual(Code(Outpost::UpdateFault::None), Code(Outpost::Decode(reader, received)), L"a truncated update decoded");
      Assert::AreEqual(std::size_t{0}, received.records.size(), L"a refused update must leave the destination alone");
    }
  }

  TEST_METHOD(ACommandPacketIsNotAnUpdate)
  {
    std::array<std::byte, 64> buffer{};
    Neuron::ByteWriter writer{buffer};
    Assert::IsTrue((Neuron::PacketHeader{.type = Neuron::PacketType::Command, .sequence = 9}).Write(writer));

    Neuron::ByteReader reader{buffer};
    Outpost::Update received;
    Assert::AreEqual(Code(Outpost::UpdateFault::WrongType), Code(Outpost::Decode(reader, received)));
  }

  /// A count is whatever the datagram said. This one claims 255 records in a header with nothing behind
  /// it; reserving first and discovering second is how that becomes a memory spike.
  TEST_METHOD(AnImpossibleCountIsRefusedBeforeAnythingIsReservedForIt)
  {
    std::array<std::byte, 32> buffer{};
    Neuron::ByteWriter writer{buffer};
    Assert::IsTrue((Neuron::PacketHeader{.type = Neuron::PacketType::Update, .sequence = 1}).Write(writer));
    Assert::IsTrue(writer.WriteUInt32(7));
    Assert::IsTrue(writer.WriteUInt16(1));
    for (std::size_t index = 0; index < Outpost::PlayerBlock::SIZE_BYTES; ++index)
    {
      Assert::IsTrue(writer.WriteUInt8(0));
    }
    Assert::IsTrue(writer.WriteUInt8(255));
    Assert::IsTrue(writer.WriteUInt8(0));
    Assert::IsTrue(writer.WriteUInt8(0));

    Neuron::ByteReader reader{std::span<const std::byte>{buffer.data(), writer.WrittenBytes()}};
    Outpost::Update received;
    Assert::AreEqual(Code(Outpost::UpdateFault::ImpossibleCount), Code(Outpost::Decode(reader, received)));
  }

  /// **THE CAPS ARE PART OF THE FORMAT**, because the sweep is computed from the records an update can
  /// always hold, and that floor is only a floor while the removal and fire sections are bounded.
  TEST_METHOD(ARemovalOrFireCountPastItsCapIsRefusedBothWays)
  {
    std::vector<std::byte> buffer(SCRATCH_BYTES);
    Neuron::ByteWriter writer{buffer};
    Assert::IsFalse(Outpost::Encode(MakeUpdate(0, Outpost::MAX_REMOVALS_PER_UPDATE + 1, 0), writer));

    Neuron::ByteWriter fireWriter{buffer};
    Assert::IsFalse(Outpost::Encode(MakeUpdate(0, 0, Outpost::MAX_FIRES_PER_UPDATE + 1), fireWriter));
  }

  /// **ONE DATAGRAM, REFUSED RATHER THAN HOPED FOR** (ADR-024). A hundred records fill the payload with
  /// nothing else in it; one more is an update the encoder will not produce.
  TEST_METHOD(AnUpdatePastThePayloadIsRefused)
  {
    std::vector<std::byte> buffer(SCRATCH_BYTES);
    Neuron::ByteWriter fits{buffer};
    Assert::IsTrue(Outpost::Encode(MakeUpdate(Outpost::MAX_RECORDS_PER_UPDATE, 0, 0), fits));
    Assert::IsTrue(fits.WrittenBytes() <= Outpost::UPDATE_PAYLOAD_BYTES);

    Neuron::ByteWriter overflows{buffer};
    Assert::IsFalse(Outpost::Encode(MakeUpdate(Outpost::MAX_RECORDS_PER_UPDATE + 1, 0, 0), overflows));
  }
};

TEST_CLASS(TheDatagramBudget)
{
public:
  /// **ADR-024's OWED MEASUREMENT 1: THE ENCODER'S OWN SIZE OF A FULL UPDATE.** The budget script's
  /// arithmetic says 99 records at the pinned 1,232 with three removals and two fire events riding; this
  /// is the encoder agreeing, written through the logger so it lands in every CI log.
  TEST_METHOD(AFullUpdateIsOneDatagramAndItsFigureIsRecorded)
  {
    const Outpost::Update full = MakeUpdate(99, 3, 2);
    std::vector<std::byte> buffer(SCRATCH_BYTES);
    Neuron::ByteWriter writer{buffer};
    Assert::IsTrue(Outpost::Encode(full, writer));
    const std::size_t size = writer.WrittenBytes();

    Logger::WriteMessage((std::wstring{L"UPDATE 99 records, 3 removals, 2 fires: "} + std::to_wstring(size) + L" bytes of " +
                          std::to_wstring(Outpost::UPDATE_PAYLOAD_BYTES) + L"\n")
                           .c_str());

    Assert::AreEqual(std::size_t{1232}, size, L"99 records and the repeated facts fill the payload exactly");
    Assert::IsTrue(size <= Outpost::UPDATE_PAYLOAD_BYTES);

    // And a hundredth record does not fit beside them, which is what makes 99 the figure.
    Assert::IsTrue(Outpost::EncodedSize(MakeUpdate(100, 3, 2)) > Outpost::UPDATE_PAYLOAD_BYTES);
  }

  /// The floors both sides compute the sweep from.
  TEST_METHOD(TheRecordFloorsAreSixtyFiveAndAHundred)
  {
    Assert::AreEqual(std::size_t{65}, Outpost::MIN_RECORDS_PER_UPDATE);
    Assert::AreEqual(std::size_t{100}, Outpost::MAX_RECORDS_PER_UPDATE);
    Assert::IsTrue(Outpost::EncodedSize(MakeUpdate(Outpost::MIN_RECORDS_PER_UPDATE, Outpost::MAX_REMOVALS_PER_UPDATE,
                                                   Outpost::MAX_FIRES_PER_UPDATE)) <= Outpost::UPDATE_PAYLOAD_BYTES);
  }

  /// **THE SWEEP, AT THE COUNTS THE DESIGN NAMES.** One tick at the MVP's 110 and two at 220, because two
  /// updates a tick of at least 65 records each cover 130; 43 at a hundred players of fifty-five.
  TEST_METHOD(TheSweepIsComputedFromTheFloor)
  {
    Assert::AreEqual(std::uint32_t{1}, Outpost::SweepTicks(0));
    Assert::AreEqual(std::uint32_t{1}, Outpost::SweepTicks(110));
    Assert::AreEqual(std::uint32_t{1}, Outpost::SweepTicks(130));
    Assert::AreEqual(std::uint32_t{2}, Outpost::SweepTicks(131));
    Assert::AreEqual(std::uint32_t{2}, Outpost::SweepTicks(220));
    Assert::AreEqual(std::uint32_t{43}, Outpost::SweepTicks(5500));
  }
};

/// M1.3: the fields M0.9 encoded as zeroes now carry meaning.
TEST_CLASS(TheFieldsM1Filled)
{
public:
  /// **THE EXACT DEFECT ADR-003 CORRECTED.** The design identity was packed into two bits of the
  /// flags byte -- four designs, permanently -- until that ADR gave it a byte of its own, because
  /// R24 has a design being an identity that research and a designer extend. A value above four is
  /// what a two-bit field silently destroys, so that is what this asserts.
  TEST_METHOD(ADesignIdentityAboveFourRoundTripsIntact)
  {
    for (const std::uint8_t identity : {std::uint8_t{5}, std::uint8_t{17}, std::uint8_t{200}, std::uint8_t{255}})
    {
      Outpost::EntityRecord written;
      written.identity = Outpost::PackIdentity(3, 1);
      written.designIdentity = identity;
      // Every flag bit set, so a design identity that leaked into the flags byte -- or a flags byte
      // that leaked into it -- could not pass unnoticed.
      written.flags = 0xFF;

      std::array<std::byte, Outpost::EntityRecord::SIZE_BYTES> bytes{};
      Neuron::ByteWriter writer{bytes};
      Assert::IsTrue(written.Write(writer));

      Neuron::ByteReader reader{bytes};
      Outpost::EntityRecord read;
      Assert::IsTrue(Outpost::EntityRecord::Read(reader, read));

      Assert::AreEqual(identity, read.designIdentity);
      Assert::AreEqual(std::uint8_t{0xFF}, read.flags);
    }
  }

  /// **WITHIN ONE PERCENT, OVER EVERY HULL IN THE CATALOG.** The `Station`'s 8,000 points over a
  /// hundred buckets is 80 points a bucket, which is the resolution ADR-003 bought when it spent
  /// one byte here.
  TEST_METHOD(HullPercentageRoundTripsWithinOnePercent)
  {
    for (const Outpost::HullEntry& hull : Outpost::Hulls())
    {
      const std::uint32_t maximum = hull.hullPoints;
      for (std::uint32_t remaining = 0; remaining <= maximum; remaining += (maximum / 37) + 1)
      {
        const std::uint8_t percent = Outpost::QuantizeHullPercent(remaining, maximum);
        const std::uint32_t back = Outpost::DequantizeHullPoints(percent, maximum);

        const std::uint32_t difference = (back > remaining) ? (back - remaining) : (remaining - back);
        Assert::IsTrue(difference <= ((maximum / 100) + 1), L"the round trip must stay inside one percent");
      }
    }
  }

  /// The two ends are exact, which matters more than the middle: a ship on its last point must not
  /// read as dead, and an undamaged one must not read as damaged.
  TEST_METHOD(TheEndsOfTheHullPercentageAreExact)
  {
    Assert::AreEqual(std::uint8_t{0}, Outpost::QuantizeHullPercent(0, 8000));
    Assert::AreEqual(std::uint8_t{100}, Outpost::QuantizeHullPercent(8000, 8000));

    // One point of eight thousand is 0.0125%, which rounds to zero -- and must not, because zero is
    // the thing a client would draw as destroyed.
    Assert::IsTrue(Outpost::QuantizeHullPercent(1, 8000) > 0);
  }

  /// **THE SIMULATION'S HEADING IS SIXTEEN BITS AND THE WIRE'S IS EIGHT** (ADR-002,
  /// `TechnicalDesign.md` section 4). The wire's is a rendering quantity at 1.4 degrees a step; the
  /// simulation's is what movement and turning are computed in and it must not be narrowed to
  /// match.
  TEST_METHOD(TheSimulationHeadingStaysWiderThanTheWires)
  {
    static_assert(sizeof(Neuron::Angle) == 2);
    static_assert(sizeof(Outpost::EntityRecord::heading) == 1);

    // A heading whose low byte is non-zero: the wire keeps the top eight bits and loses the rest,
    // which is the loss ADR-003 chose and not one to be surprised by.
    const Neuron::Angle simulation = 0x1234;
    const auto wire = static_cast<std::uint8_t>(simulation >> 8);
    Assert::AreEqual(std::uint8_t{0x12}, wire);

    // And widening it back lands on the top of the bucket rather than anywhere else.
    Assert::AreEqual(static_cast<int>(Neuron::Angle{0x1200}), static_cast<int>(Outpost::DequantizeWireHeading(wire)));
  }
};

/// M2.7, Q53. **Four chips, five states**, which is why cargo took a third bit.
TEST_CLASS(TheCargoChips)
{
public:
  /// None when empty, all four when full, and quarters **rounded up** between -- any ore lights a chip.
  TEST_METHOD(AHoldLightsQuartersRoundedUp)
  {
    constexpr std::uint32_t FULL = 100000;
    Assert::AreEqual(std::uint8_t{0}, Outpost::CargoChips(0, FULL));
    Assert::AreEqual(std::uint8_t{1}, Outpost::CargoChips(1, FULL), L"a thousandth of ore still reads as carrying");
    Assert::AreEqual(std::uint8_t{1}, Outpost::CargoChips(25000, FULL));
    Assert::AreEqual(std::uint8_t{2}, Outpost::CargoChips(25001, FULL));
    Assert::AreEqual(std::uint8_t{3}, Outpost::CargoChips(75000, FULL));
    Assert::AreEqual(std::uint8_t{4}, Outpost::CargoChips(75001, FULL));
    Assert::AreEqual(std::uint8_t{4}, Outpost::CargoChips(FULL, FULL));
    Assert::AreEqual(std::uint8_t{0}, Outpost::CargoChips(500, 0), L"a design with no hold lights nothing");
  }

  /// Three bits at bit three, leaving the state below and two spare above untouched -- and a value past four
  /// is clamped rather than written.
  TEST_METHOD(TheChipsSitInTheirThreeBits)
  {
    Assert::AreEqual(std::uint8_t{0x07}, Outpost::FLAGS_CARGO_MASK);
    Assert::AreEqual(std::uint8_t{6}, Outpost::FLAGS_SPARE_SHIFT);
    for (std::uint8_t chips = 0; chips <= Outpost::CARGO_CHIP_COUNT; ++chips)
    {
      const std::uint8_t flags = Outpost::WithCargoChips(0xC5, chips);
      Assert::AreEqual(chips, Outpost::CargoChipsOf(flags));
      Assert::AreEqual(std::uint8_t{0xC5 & 0xC7}, static_cast<std::uint8_t>(flags & 0xC7), L"the state or the spare bits moved");
    }
    Assert::AreEqual(std::uint8_t{4}, Outpost::CargoChipsOf(Outpost::WithCargoChips(0, 7)));
  }
};

} // namespace GameCoreTests

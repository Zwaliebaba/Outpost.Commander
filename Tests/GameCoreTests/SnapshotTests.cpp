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
/// ADR-003's pinned payload: the IPv6 minimum MTU less the IPv6 and UDP headers. The largest
/// figure that needs no path-MTU discovery, no probing and no fallback path in the code.
inline constexpr std::size_t PAYLOAD_BYTES = 1232;

/// Comfortably past a four-player snapshot, so a test measuring a size is never measuring a
/// buffer that ran out.
inline constexpr std::size_t SCRATCH_BYTES = 4096;

[[nodiscard]] std::uint8_t Code(Outpost::SnapshotFault _fault) noexcept
{
  return static_cast<std::uint8_t>(_fault);
}

/// Entities that differ in every field, so a round trip that crossed two of them over would fail.
[[nodiscard]] Outpost::Snapshot MakeSnapshot(std::size_t _entityCount, std::size_t _playerCount, std::size_t _removalCount)
{
  Outpost::Snapshot snapshot;
  snapshot.sequence = 4242;
  snapshot.tick = 123456;

  for (std::size_t player = 0; player < _playerCount; ++player)
  {
    snapshot.players.push_back(Outpost::PlayerBlock{.credits = static_cast<std::uint32_t>(1000 + player),
                                                    .lastCommandSequenceApplied = static_cast<std::uint16_t>(70 + player),
                                                    .buildingDesign = static_cast<std::uint8_t>(player + 1),
                                                    .buildProgressPercent = static_cast<std::uint8_t>(player * 7)});
  }

  for (std::size_t entity = 0; entity < _entityCount; ++entity)
  {
    snapshot.entities.push_back(Outpost::EntityRecord{
      .identity = Outpost::PackIdentity(static_cast<std::uint16_t>(entity), static_cast<std::uint16_t>((entity % 63) + 1)),
      .positionX = static_cast<std::int16_t>(entity * 13),
      .positionY = static_cast<std::int16_t>(-static_cast<std::int32_t>(entity * 7)),
      .heading = static_cast<std::uint8_t>(entity % 256),
      .hullPercentRemaining = static_cast<std::uint8_t>(entity % 101),
      .designIdentity = static_cast<std::uint8_t>(entity % 17),
      .flags = static_cast<std::uint8_t>(entity % 128)});
  }

  for (std::size_t removal = 0; removal < _removalCount; ++removal)
  {
    snapshot.removals.push_back(Outpost::PackIdentity(static_cast<std::uint16_t>(900 + removal), 5));
  }

  return snapshot;
}

/// Encode, decode, and hand back both the recovered snapshot and the byte count.
[[nodiscard]] std::size_t RoundTrip(const Outpost::Snapshot& _sent, Outpost::Snapshot& _outReceived)
{
  std::vector<std::byte> buffer(SCRATCH_BYTES);
  Neuron::ByteWriter writer{buffer};
  Assert::IsTrue(Outpost::Encode(_sent, writer), L"the snapshot did not encode");

  const std::size_t written = writer.WrittenBytes();
  Assert::AreEqual(Outpost::EncodedSize(_sent), written, L"EncodedSize disagrees with the encoder");

  Neuron::ByteReader reader{std::span<const std::byte>{buffer.data(), written}};
  Assert::AreEqual(Code(Outpost::SnapshotFault::None), Code(Outpost::Decode(reader, _outReceived)), L"it did not decode");
  return written;
}
} // namespace

TEST_CLASS(WireRecordWidths)
{
public:
  TEST_METHOD(AnEntityRecordIsTenBytes)
  {
    Assert::AreEqual(std::size_t{10}, Outpost::EntityRecord::SIZE_BYTES);

    std::array<std::byte, 64> buffer{};
    Neuron::ByteWriter writer{buffer};
    Assert::IsTrue(Outpost::EntityRecord{}.Write(writer));
    Assert::AreEqual(std::size_t{10}, writer.WrittenBytes(), L"the record is not the width ADR-003 states");
  }

  TEST_METHOD(TheHeaderIsThirtyBytesAtTwoPlayersAndFortySixAtFour)
  {
    // ADR-003 states both figures. This is the encoder agreeing with it, which is the whole
    // reason the per-player block is sized by a count rather than fixed: two more players are a
    // runtime value and not a format change.
    const std::size_t atTwo = Outpost::EncodedSize(MakeSnapshot(0, 2, 0)) - sizeof(std::uint8_t);
    const std::size_t atFour = Outpost::EncodedSize(MakeSnapshot(0, 4, 0)) - sizeof(std::uint8_t);
    Assert::AreEqual(std::size_t{30}, atTwo);
    Assert::AreEqual(std::size_t{46}, atFour);
  }

  TEST_METHOD(AFireEventIsFiveBytes)
  {
    Assert::AreEqual(std::size_t{5}, Outpost::FireEvent::SIZE_BYTES);
    const std::size_t withoutFire = Outpost::EncodedSize(MakeSnapshot(0, 2, 0));
    Outpost::Snapshot withFire = MakeSnapshot(0, 2, 0);
    withFire.fires.push_back(Outpost::FireEvent{.shooter = 1, .target = 2, .weapon = 3});
    Assert::AreEqual(withoutFire + 5, Outpost::EncodedSize(withFire));
  }

  TEST_METHOD(APlayerBlockIsEightBytes)
  {
    Assert::AreEqual(std::size_t{8}, Outpost::PlayerBlock::SIZE_BYTES);
    Assert::AreEqual(Outpost::EncodedSize(MakeSnapshot(0, 2, 0)) + 8, Outpost::EncodedSize(MakeSnapshot(0, 3, 0)));
  }
};

TEST_CLASS(IdentityPacking)
{
public:
  TEST_METHOD(IndexAndGenerationSurviveThePack)
  {
    for (std::uint16_t index = 0; index <= Outpost::WIRE_INDEX_MASK; index += 7)
    {
      for (std::uint16_t generation = 0; generation <= Outpost::WIRE_GENERATION_MASK; ++generation)
      {
        const std::uint16_t packed = Outpost::PackIdentity(index, generation);
        Assert::AreEqual(index, Outpost::IndexOf(packed));
        Assert::AreEqual(generation, Outpost::GenerationOf(packed));
      }
    }
  }

  TEST_METHOD(TheSplitIsTenAndSix)
  {
    Assert::AreEqual(std::uint16_t{10}, Outpost::WIRE_INDEX_BITS);
    Assert::AreEqual(std::uint16_t{6}, Outpost::WIRE_GENERATION_BITS);
    Assert::AreEqual(std::uint16_t{1023}, Outpost::WIRE_INDEX_MASK);
    Assert::AreEqual(std::uint16_t{63}, Outpost::WIRE_GENERATION_MASK);
  }

  TEST_METHOD(AnIndexPastTheWireIsRefusedRatherThanTruncated)
  {
    // A truncated identity does not fail, it resolves to the WRONG entity -- which is the one
    // outcome worth refusing loudly for.
    Assert::IsTrue(Outpost::FitsWireIdentity(1023));
    Assert::IsFalse(Outpost::FitsWireIdentity(1024));
    Assert::IsFalse(Outpost::FitsWireIdentity(65535));
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

TEST_CLASS(SnapshotRoundTrip)
{
public:
  TEST_METHOD(EveryFieldOfEveryRecordSurvives)
  {
    const Outpost::Snapshot sent = MakeSnapshot(40, 2, 3);
    Outpost::Snapshot received;
    static_cast<void>(RoundTrip(sent, received));

    Assert::AreEqual(sent.sequence, received.sequence);
    Assert::AreEqual(sent.tick, received.tick);
    Assert::AreEqual(sent.players.size(), received.players.size());
    Assert::AreEqual(sent.entities.size(), received.entities.size());
    Assert::AreEqual(sent.removals.size(), received.removals.size());

    for (std::size_t index = 0; index < sent.players.size(); ++index)
    {
      Assert::IsTrue(sent.players[index] == received.players[index], L"a player block changed");
    }
    for (std::size_t index = 0; index < sent.entities.size(); ++index)
    {
      Assert::IsTrue(sent.entities[index] == received.entities[index], L"an entity record changed");
    }
    for (std::size_t index = 0; index < sent.removals.size(); ++index)
    {
      Assert::AreEqual(sent.removals[index], received.removals[index], L"a removal changed");
    }
  }

  TEST_METHOD(BothPlayerCountsRoundTrip)
  {
    for (const std::size_t playerCount : {std::size_t{1}, std::size_t{2}, std::size_t{4}})
    {
      const Outpost::Snapshot sent = MakeSnapshot(10, playerCount, 1);
      Outpost::Snapshot received;
      static_cast<void>(RoundTrip(sent, received));
      Assert::AreEqual(playerCount, received.players.size());
      Assert::IsTrue(sent.players.back() == received.players.back());
    }
  }

  TEST_METHOD(FireEventsRoundTripBehindTheirCount)
  {
    Outpost::Snapshot sent = MakeSnapshot(5, 2, 2);
    sent.fires.push_back(Outpost::FireEvent{.shooter = 11, .target = 22, .weapon = 1});
    sent.fires.push_back(Outpost::FireEvent{.shooter = 33, .target = 44, .weapon = 2});

    Outpost::Snapshot received;
    static_cast<void>(RoundTrip(sent, received));
    Assert::AreEqual(std::size_t{2}, received.fires.size());
    Assert::IsTrue(sent.fires[0] == received.fires[0]);
    Assert::IsTrue(sent.fires[1] == received.fires[1]);
  }

  TEST_METHOD(TheEmptyListsStillCostTheirCount)
  {
    // M0 has nothing to remove and nothing firing, and the format carries both anyway. The
    // removal count is a header byte; the fire count is a byte after the removal list.
    const Outpost::Snapshot sent = MakeSnapshot(1, 1, 0);
    Outpost::Snapshot received;
    static_cast<void>(RoundTrip(sent, received));
    Assert::AreEqual(std::size_t{0}, received.removals.size());
    Assert::AreEqual(std::size_t{0}, received.fires.size());
  }

  TEST_METHOD(ASnapshotWithNothingInItStillDecodes)
  {
    const Outpost::Snapshot sent = MakeSnapshot(0, 0, 0);
    Outpost::Snapshot received;
    const std::size_t written = RoundTrip(sent, received);
    Assert::AreEqual(std::size_t{15}, written, L"six transport, eight fixed, one fire count");
  }
};

TEST_CLASS(SnapshotRefusals)
{
public:
  TEST_METHOD(ATruncatedDatagramIsTruncated)
  {
    const Outpost::Snapshot sent = MakeSnapshot(20, 2, 2);
    std::vector<std::byte> buffer(SCRATCH_BYTES);
    Neuron::ByteWriter writer{buffer};
    Assert::IsTrue(Outpost::Encode(sent, writer));

    // Every prefix, because a decoder that only checks at the end reads past one of them.
    for (std::size_t length = 0; length < writer.WrittenBytes(); length += 3)
    {
      Neuron::ByteReader reader{std::span<const std::byte>{buffer.data(), length}};
      Outpost::Snapshot received;
      const Outpost::SnapshotFault fault = Outpost::Decode(reader, received);
      Assert::AreNotEqual(Code(Outpost::SnapshotFault::None), Code(fault), L"a truncated snapshot decoded");
      Assert::AreEqual(std::size_t{0}, received.entities.size(), L"a refused snapshot must leave the destination alone");
    }
  }

  TEST_METHOD(ACommandPacketIsNotASnapshot)
  {
    std::array<std::byte, 64> buffer{};
    Neuron::ByteWriter writer{buffer};
    Assert::IsTrue((Neuron::PacketHeader{.type = Neuron::PacketType::Command, .sequence = 9}).Write(writer));

    Neuron::ByteReader reader{buffer};
    Outpost::Snapshot received;
    Assert::AreEqual(Code(Outpost::SnapshotFault::WrongType), Code(Outpost::Decode(reader, received)));
  }

  TEST_METHOD(AFragmentIsRefusedRatherThanHalfRead)
  {
    // ADR-003 puts the two-fragment path at the fourth player and M4.3 writes the reassembler.
    // Until then a fragment is a packet this build drops, and saying so beats decoding half of it.
    std::array<std::byte, 64> buffer{};
    Neuron::ByteWriter writer{buffer};
    const Neuron::PacketHeader fragment{.type = Neuron::PacketType::Snapshot, .sequence = 9, .fragmentIndex = 0, .fragmentCount = 2};
    Assert::IsTrue(fragment.Write(writer));

    Neuron::ByteReader reader{buffer};
    Outpost::Snapshot received;
    Assert::AreEqual(Code(Outpost::SnapshotFault::Fragmented), Code(Outpost::Decode(reader, received)));
  }

  TEST_METHOD(AnImpossibleCountIsRefusedBeforeAnythingIsReservedForIt)
  {
    // A count is whatever the datagram said. This one claims 65,535 entities in a handful of
    // bytes; reserving first and discovering second is how that becomes a memory spike rather
    // than a dropped packet.
    std::array<std::byte, 32> buffer{};
    Neuron::ByteWriter writer{buffer};
    Assert::IsTrue((Neuron::PacketHeader{.type = Neuron::PacketType::Snapshot, .sequence = 1}).Write(writer));
    Assert::IsTrue(writer.WriteUInt32(7));
    Assert::IsTrue(writer.WriteUInt16(65535));
    Assert::IsTrue(writer.WriteUInt8(0));
    Assert::IsTrue(writer.WriteUInt8(0));

    Neuron::ByteReader reader{std::span<const std::byte>{buffer.data(), writer.WrittenBytes()}};
    Outpost::Snapshot received;
    Assert::AreEqual(Code(Outpost::SnapshotFault::ImpossibleCount), Code(Outpost::Decode(reader, received)));
  }

  TEST_METHOD(ABufferWithNoRoomFails)
  {
    const Outpost::Snapshot sent = MakeSnapshot(100, 2, 3);
    std::array<std::byte, 64> tooSmall{};
    Neuron::ByteWriter writer{tooSmall};
    Assert::IsFalse(Outpost::Encode(sent, writer));
  }
};

TEST_CLASS(TheDatagramBudget)
{
public:
  TEST_METHOD(TheMvpSnapshotIsOneDatagramAndItsSizeIsRecorded)
  {
    // TECHNICALDESIGN SECTION 9.1, DISCHARGED. The first of the measurements that document owes:
    // the snapshot's real size from the encoder rather than from the design's arithmetic, and
    // specifically that the MVP's really is one datagram.
    //
    // The counts: 102 is two players of fifty ships and a station, which is what the plan names;
    // 110 is the same with ADR-015's four modules each, which is what the design's MVP table
    // actually states. Both are recorded because the plan was written before modules landed.
    const auto measure = [](const wchar_t* _label, std::size_t _entities, std::size_t _players) -> std::size_t
    {
      const Outpost::Snapshot snapshot = MakeSnapshot(_entities, _players, 3);
      std::vector<std::byte> buffer(SCRATCH_BYTES);
      Neuron::ByteWriter writer{buffer};
      Assert::IsTrue(Outpost::Encode(snapshot, writer));
      const std::size_t size = writer.WrittenBytes();
      Assert::AreEqual(Outpost::EncodedSize(snapshot), size);

      // Through the logger, so it lands in every CI log rather than in one person's notes.
      Logger::WriteMessage((std::wstring{L"SNAPSHOT "} + _label + L": " + std::to_wstring(size) + L" bytes, " + std::to_wstring(_entities) +
                            L" entities, " + std::to_wstring(_players) + L" players, 3 removals\n")
                             .c_str());
      return size;
    };

    const std::size_t plan102 = measure(L"102 entities / 2 players", 102, 2);
    const std::size_t mvp110 = measure(L"110 entities / 2 players (MVP)", 110, 2);
    const std::size_t plan204 = measure(L"204 entities / 4 players", 204, 4);
    const std::size_t post220 = measure(L"220 entities / 4 players (post-M2)", 220, 4);

    Logger::WriteMessage((std::wstring{L"HEADROOM at the pinned 1232: "} + std::to_wstring(PAYLOAD_BYTES - mvp110) + L" bytes = " +
                          std::to_wstring((PAYLOAD_BYTES - mvp110) / Outpost::EntityRecord::SIZE_BYTES) + L" entity records\n")
                           .c_str());

    // ONE DATAGRAM, which is the property ADR-003 buys and the reason a lost packet costs one
    // frame of animation rather than a divergence.
    Assert::IsTrue(mvp110 <= PAYLOAD_BYTES, L"the MVP snapshot no longer fits one datagram");

    // The measured figures, pinned. The design's table says 1,136 and 2,252; the encoder says one
    // more of each, because the fire-event count byte ADR-004 specifies is absent from the
    // design's arithmetic. When these move, the design moves with them in the same change.
    Assert::AreEqual(std::size_t{1057}, plan102);
    Assert::AreEqual(std::size_t{1137}, mvp110);
    Assert::AreEqual(std::size_t{2093}, plan204);
    Assert::AreEqual(std::size_t{2253}, post220);

    // And the four-player configuration does not fit, which is why ADR-003 puts the two-fragment
    // path there rather than treating it as free.
    Assert::IsTrue(post220 > PAYLOAD_BYTES, L"post-M2 fitting one datagram would contradict ADR-003");
  }
};

} // namespace GameCoreTests

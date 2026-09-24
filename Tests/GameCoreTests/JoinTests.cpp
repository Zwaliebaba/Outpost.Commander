#include "pch.h"

#include <array>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameCoreTests
{

namespace
{
constexpr std::size_t JOIN_BYTES = Neuron::PacketHeader::SIZE_BYTES + Outpost::Join::SIZE_BYTES;
constexpr std::size_t REPLY_BYTES = Neuron::PacketHeader::SIZE_BYTES + Outpost::JoinReply::SIZE_BYTES;
} // namespace

/// ADR-013's two records. **The widths in the ADR are arithmetic and these are the facts**, which
/// is the same relationship `EntityRecord` has with the snapshot's table.
TEST_CLASS(TheJoinRecords)
{
public:
  /// **TWELVE AND THIRTY-ONE.** ADR-024 took the two fragment fields out of the transport header in front
  /// of both, M2.3 added the player count's byte to the reply, and Q76 its eight bytes of field hash.
  TEST_METHOD(AJoinIsTwelveBytesAndAReplyIsThirtyOne)
  {
    Assert::AreEqual(static_cast<std::size_t>(12), JOIN_BYTES);
    Assert::AreEqual(static_cast<std::size_t>(31), REPLY_BYTES);
  }

  TEST_METHOD(AJoinRoundTrips)
  {
    const Outpost::Join written{.token = 0x0123456789ABCDEFull};

    std::array<std::byte, JOIN_BYTES> bytes{};
    Neuron::ByteWriter writer{bytes};
    Assert::IsTrue(Outpost::Encode(written, writer));
    Assert::AreEqual(JOIN_BYTES, writer.WrittenBytes());

    Neuron::ByteReader reader{bytes};
    Outpost::Join read{};
    Assert::IsTrue(Outpost::Decode(reader, read) == Outpost::JoinFault::None);
    Assert::IsTrue(written == read);
  }

  /// **A CLIENT WITH NO TOKEN IS THE FIRST RUN AND IS ORDINARY.** Zero has to survive the wire like
  /// any other value, because it is the value that says "seat me as somebody new".
  TEST_METHOD(AJoinWithNoTokenRoundTrips)
  {
    const Outpost::Join written{.token = Outpost::NO_SESSION_TOKEN};

    std::array<std::byte, JOIN_BYTES> bytes{};
    Neuron::ByteWriter writer{bytes};
    Assert::IsTrue(Outpost::Encode(written, writer));

    Neuron::ByteReader reader{bytes};
    Outpost::Join read{};
    Assert::IsTrue(Outpost::Decode(reader, read) == Outpost::JoinFault::None);
    Assert::AreEqual(Outpost::NO_SESSION_TOKEN, read.token);
  }

  TEST_METHOD(AReplyRoundTripsEveryResult)
  {
    for (const Outpost::JoinResult result : {Outpost::JoinResult::Accepted, Outpost::JoinResult::Rejoined, Outpost::JoinResult::MatchFull})
    {
      const Outpost::JoinReply written{
        .result = result, .player = 3, .playerCount = 4, .token = 0xFEDCBA9876543210ull, .matchSeed = 0x00FF00FF00FF00FFull};

      std::array<std::byte, REPLY_BYTES> bytes{};
      Neuron::ByteWriter writer{bytes};
      Assert::IsTrue(Outpost::Encode(written, writer));
      Assert::AreEqual(REPLY_BYTES, writer.WrittenBytes());

      Neuron::ByteReader reader{bytes};
      Outpost::JoinReply read{};
      Assert::IsTrue(Outpost::Decode(reader, read) == Outpost::JoinFault::None);
      Assert::IsTrue(written == read);
    }
  }

  /// **THE SEED IS SIXTY-FOUR BITS AND MUST NOT BE NARROWED ANYWHERE ON THE WAY.** R23 has both
  /// sides generating the field from this number, so a truncation here is two machines drawing two
  /// different maps -- which looks like a rendering bug and is a wire bug. The value below has a
  /// bit set in every byte, so any narrowing at all changes it.
  TEST_METHOD(TheSeedSurvivesEveryByte)
  {
    const std::uint64_t seed = 0x8040201008040201ull;
    const Outpost::JoinReply written{.result = Outpost::JoinResult::Accepted, .player = 1, .token = 1, .matchSeed = seed};

    std::array<std::byte, REPLY_BYTES> bytes{};
    Neuron::ByteWriter writer{bytes};
    Assert::IsTrue(Outpost::Encode(written, writer));

    Neuron::ByteReader reader{bytes};
    Outpost::JoinReply read{};
    Assert::IsTrue(Outpost::Decode(reader, read) == Outpost::JoinFault::None);
    Assert::AreEqual(seed, read.matchSeed);
  }

  /// **THE COUNT IS THE FIELD'S OTHER INPUT** (M2.3), so every count a host can seat has to arrive as
  /// sent -- `MAX_PLAYERS` at the top, since a stress host seats that many (ADR-023), and one at the bottom.
  TEST_METHOD(ThePlayerCountSurvivesTheWire)
  {
    for (const std::size_t count : {std::size_t{1}, std::size_t{2}, std::size_t{4}, Outpost::MAX_PLAYERS})
    {
      const Outpost::JoinReply written{
        .result = Outpost::JoinResult::Accepted, .player = 1, .playerCount = static_cast<std::uint8_t>(count), .token = 1, .matchSeed = 7};

      std::array<std::byte, REPLY_BYTES> bytes{};
      Neuron::ByteWriter writer{bytes};
      Assert::IsTrue(Outpost::Encode(written, writer));

      Neuron::ByteReader reader{bytes};
      Outpost::JoinReply read{};
      Assert::IsTrue(Outpost::Decode(reader, read) == Outpost::JoinFault::None);
      Assert::AreEqual(count, static_cast<std::size_t>(read.playerCount));
      Assert::AreEqual(std::uint64_t{7}, read.matchSeed, L"the count's byte shifted the seed");
    }
  }

  /// ZERO IS NOT A RESULT, for the reason `Neuron::PacketType` gives: a zero-filled buffer must not
  /// decode into an acceptance.
  TEST_METHOD(AZeroResultByteIsRefused)
  {
    std::array<std::byte, REPLY_BYTES> bytes{};
    Neuron::ByteWriter writer{bytes};
    const Neuron::PacketHeader header{.type = Neuron::PacketType::JoinReply};
    Assert::IsTrue(header.Write(writer));
    for (std::size_t field = 0; field < Outpost::JoinReply::SIZE_BYTES; ++field)
    {
      static_cast<void>(writer.WriteUInt8(0));
    }

    Neuron::ByteReader reader{bytes};
    Outpost::JoinReply read{};
    Assert::IsTrue(Outpost::Decode(reader, read) == Outpost::JoinFault::Malformed);
  }

  /// An encoder refuses a result this build cannot name, before a byte is written -- the rule
  /// `Command`'s encoder applies to an unknown command type.
  TEST_METHOD(AnUnknownResultIsNotEncoded)
  {
    const Outpost::JoinReply written{.result = static_cast<Outpost::JoinResult>(9)};

    std::array<std::byte, REPLY_BYTES> bytes{};
    Neuron::ByteWriter writer{bytes};
    Assert::IsFalse(Outpost::Encode(written, writer));
    Assert::AreEqual(static_cast<std::size_t>(0), writer.WrittenBytes());
  }

  /// **EACH DECODER REFUSES THE OTHER'S RECORD.** One socket now carries five packet types, and a
  /// decoder that half-read a record meant for somebody else would report a corrupt datagram --
  /// which is how a working join would come to look like packet loss.
  TEST_METHOD(EachDecoderRefusesTheOthersRecord)
  {
    std::array<std::byte, JOIN_BYTES> joinBytes{};
    Neuron::ByteWriter joinWriter{joinBytes};
    Assert::IsTrue(Outpost::Encode(Outpost::Join{.token = 7}, joinWriter));

    Neuron::ByteReader asReply{joinBytes};
    Outpost::JoinReply reply{};
    Assert::IsTrue(Outpost::Decode(asReply, reply) == Outpost::JoinFault::WrongType);

    std::array<std::byte, REPLY_BYTES> replyBytes{};
    Neuron::ByteWriter replyWriter{replyBytes};
    Assert::IsTrue(Outpost::Encode(Outpost::JoinReply{.result = Outpost::JoinResult::Accepted, .player = 1}, replyWriter));

    Neuron::ByteReader asJoin{replyBytes};
    Outpost::Join join{};
    Assert::IsTrue(Outpost::Decode(asJoin, join) == Outpost::JoinFault::WrongType);
  }

  /// A snapshot decoder handed a join reply says `WrongType` rather than reporting a fault that
  /// reads as corruption -- which is what the client's drain switches on.
  TEST_METHOD(AnUpdateDecoderRefusesAJoinReply)
  {
    std::array<std::byte, REPLY_BYTES> bytes{};
    Neuron::ByteWriter writer{bytes};
    Assert::IsTrue(Outpost::Encode(Outpost::JoinReply{.result = Outpost::JoinResult::Accepted, .player = 1}, writer));

    Neuron::ByteReader reader{bytes};
    Outpost::Update update;
    Assert::IsTrue(Outpost::Decode(reader, update) == Outpost::UpdateFault::WrongType);
  }

  /// A datagram that ends inside a field is truncated rather than decoding out of whatever follows.
  TEST_METHOD(ATruncatedRecordIsRefused)
  {
    std::array<std::byte, REPLY_BYTES> bytes{};
    Neuron::ByteWriter writer{bytes};
    Assert::IsTrue(Outpost::Encode(Outpost::JoinReply{.result = Outpost::JoinResult::Accepted, .player = 2, .token = 5}, writer));

    for (std::size_t length = Neuron::PacketHeader::SIZE_BYTES; length < REPLY_BYTES; ++length)
    {
      Neuron::ByteReader reader{std::span<const std::byte>{bytes.data(), length}};
      Outpost::JoinReply read{};
      Assert::IsTrue(Outpost::Decode(reader, read) == Outpost::JoinFault::Truncated);
    }
  }

  /// A build on another version is dropped and told nothing (ADR-013), so the decoder has to be
  /// able to SAY that is what happened -- the counter it feeds is the only trace.
  TEST_METHOD(AVersionMismatchIsItsOwnFault)
  {
    std::array<std::byte, JOIN_BYTES> bytes{};
    Neuron::ByteWriter writer{bytes};
    Assert::IsTrue(Outpost::Encode(Outpost::Join{.token = 11}, writer));
    bytes[0] = static_cast<std::byte>(Neuron::PROTOCOL_VERSION + 1);

    Neuron::ByteReader reader{bytes};
    Outpost::Join read{};
    Assert::IsTrue(Outpost::Decode(reader, read) == Outpost::JoinFault::VersionMismatch);
  }
};

/// M3.8, `OpenQuestions.md` Q70. **A match that ended, on the wire.**
TEST_CLASS(TheMatchEnded)
{
public:
  TEST_METHOD(ItRoundTrips)
  {
    for (const Outpost::MatchEnded sent : {Outpost::MatchEnded{.matchNumber = 1, .winner = 2, .onClock = false},
                                           Outpost::MatchEnded{.matchNumber = 65535, .winner = Outpost::NO_PLAYER, .onClock = true}})
    {
      std::array<std::byte, Neuron::PacketHeader::SIZE_BYTES + Outpost::MatchEnded::SIZE_BYTES> buffer{};
      Neuron::ByteWriter writer{buffer};
      Assert::IsTrue(Outpost::Encode(sent, writer));
      Assert::AreEqual(buffer.size(), writer.WrittenBytes(), L"the record is not the size it says");

      Neuron::ByteReader reader{buffer};
      Outpost::MatchEnded received{};
      Assert::IsTrue(Outpost::Decode(reader, received) == Outpost::JoinFault::None);
      Assert::IsTrue(sent == received);
    }
  }

  /// A clock byte that is neither is refused, and a join reply is not a match end.
  TEST_METHOD(AMalformedOrWrongRecordIsRefused)
  {
    std::array<std::byte, Neuron::PacketHeader::SIZE_BYTES + Outpost::MatchEnded::SIZE_BYTES> buffer{};
    Neuron::ByteWriter writer{buffer};
    Assert::IsTrue(Outpost::Encode(Outpost::MatchEnded{.matchNumber = 3, .winner = 1, .onClock = true}, writer));
    buffer.back() = std::byte{2};
    Neuron::ByteReader reader{buffer};
    Outpost::MatchEnded received{};
    Assert::IsTrue(Outpost::Decode(reader, received) == Outpost::JoinFault::Malformed);

    std::array<std::byte, Neuron::PacketHeader::SIZE_BYTES + Outpost::JoinReply::SIZE_BYTES> reply{};
    Neuron::ByteWriter replyWriter{reply};
    Assert::IsTrue(Outpost::Encode(Outpost::JoinReply{.result = Outpost::JoinResult::Accepted, .player = 1}, replyWriter));
    Neuron::ByteReader replyReader{reply};
    Assert::IsTrue(Outpost::Decode(replyReader, received) == Outpost::JoinFault::WrongType);
  }
};

} // namespace GameCoreTests

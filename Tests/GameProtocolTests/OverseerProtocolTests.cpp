#include "pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameProtocolTests
{
namespace
{
// The message _bytes hold, which must be a T.
template <typename T> T Decoded(const std::vector<std::byte>& _bytes)
{
  Outpost::OverseerMessage message = Outpost::DecodeOverseerMessage(_bytes);
  Assert::IsTrue(std::holds_alternative<T>(message), L"The message decodes as the kind it was encoded as");
  return std::get<T>(std::move(message));
}

// Decodes what _message encodes to and encodes it again: the same bytes, when every field crossed as it was.
template <typename T> void AssertCrosses(const T& _message)
{
  const std::vector<std::byte> bytes = Outpost::EncodeOverseerMessage(_message);
  Assert::IsTrue(bytes == Outpost::EncodeOverseerMessage(Decoded<T>(bytes)), L"Every field crosses as it was sent");
}

// A Health with every field set, each to a value of its own.
Outpost::HealthMessage FullHealth()
{
  Outpost::HealthMessage health{.wallMilliseconds = -1'700'000'000'123,
                                .tick = 0x1'0000'0002ull,
                                .tickMicroseconds = {3, 4'000, 5'000'000},
                                .longestPart = Outpost::TickPart::Overseer,
                                .droppedTicks = 6,
                                .connections = 7,
                                .process = {.workingSetBytes = 8,
                                            .privateBytes = 9,
                                            .cpuHundredths = 210,
                                            .threads = 11,
                                            .handles = 12,
                                            .uptimeSeconds = 13,
                                            .diskFreeBytes = 0x1'0000'0014ull}};
  health.partMicroseconds[0] = 15;
  health.partMicroseconds[Outpost::TICK_PART_COUNT - 1] = 16;
  health.joins[0] = 17;
  health.joins[Outpost::JOIN_OUTCOME_COUNT - 1] = 18;
  health.seats.push_back({.player = Outpost::PlayerId{2},
                          .play = Outpost::SeatPlay::Deputy,
                          .link = Outpost::SeatLink::Gone,
                          .linkTick = 19,
                          .deputyTick = 1'219,
                          .connection = Outpost::LinkFigures{.roundTripMicroseconds = 20,
                                                             .leastRoundTripMicroseconds = 21,
                                                             .packetsSent = 22,
                                                             .packetsLost = 23,
                                                             .congestionWindowBytes = 24,
                                                             .backlogBytes = 25,
                                                             .bytesSent = 26},
                          .orders = 27,
                          .refused = 28,
                          .ore = -29,
                          .incomeHundredthsPerSecond = 30,
                          .commandPoints = 31,
                          .fleetCap = 32,
                          .nodes = 33,
                          .researched = 34,
                          .restartTick = 35,
                          .away = Outpost::AwayReport{.sinceTick = 1'219,
                                                      .shipsBuilt = 38,
                                                      .structuresBuilt = 39,
                                                      .shipsLost = 40,
                                                      .structuresLost = 41,
                                                      .sectorsGained = {42},
                                                      .sectorsLost = {43, 44}}});
  health.seats.push_back({.player = Outpost::PlayerId{3}, .play = Outpost::SeatPlay::Ai});
  health.battles.push_back({.sector = 36, .startTick = 37, .players = {Outpost::PlayerId{2}, Outpost::PlayerId{3}}});
  return health;
}

// _bytes with the byte at _at set to _value.
std::vector<std::byte> WithByte(std::vector<std::byte> _bytes, std::size_t _at, std::uint8_t _value)
{
  _bytes.at(_at) = static_cast<std::byte>(_value);
  return _bytes;
}
} // namespace

TEST_CLASS(OverseerProtocolTests)
{
public:
  TEST_METHOD(CarriesEveryMessageAsItWasSent)
  {
    AssertCrosses(Outpost::OverseerHelloMessage{.token = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16}});
    Outpost::OverseerWelcomeMessage welcome{.wireLayoutHash = Outpost::WireLayoutHash(),
                                            .protocolVersion = 2,
                                            .stateVersion = 3,
                                            .ticksPerSecond = 20,
                                            .folderUtf8 = "C:\\Universe \xC3\xA9",
                                            .seats = {{.player = Outpost::PlayerId{1}}, {.player = Outpost::PlayerId{2}, .ai = "Hard"}},
                                            .recoveredSaveTick = 1'200,
                                            .replayedCommands = 4,
                                            .saveSeconds = 60,
                                            .dayRecords = {"seat 100 player 1 deputy", "battle 140 160 sector 3 player 1 deputy"}};
    welcome.sectors.push_back({.id = 5, .nameUtf8 = "Home", .minXMeters = -6.0f, .node = {.xMeters = 7.0f}, .adjacent = {8, 9}});
    welcome.fields.push_back({.id = Outpost::EntityId{10}, .kind = Outpost::EntityKind::AsteroidField, .radiusMeters = 11.0f});
    AssertCrosses(welcome);
    AssertCrosses(FullHealth());
    AssertCrosses(Outpost::LogRecordsMessage{.lines = {"start 0 new", "", "overseer 20 opened from ::1"}});
    AssertCrosses(Outpost::ClosingMessage{.ending = Outpost::ServerEnding::Failed, .messageUtf8 = "a failure", .lastSaveTick = 12});
    AssertCrosses(Outpost::WatchSightMessage{.seat = Outpost::PlayerId{2}});
    AssertCrosses(Outpost::ViewHeldMessage{.tick = 0x1'0000'0013ull});

    // A few fields read back, so that a message that crossed as some other one would show.
    const auto health = Decoded<Outpost::HealthMessage>(Outpost::EncodeOverseerMessage(FullHealth()));
    Assert::AreEqual(std::size_t{3}, health.tickMicroseconds.size());
    Assert::AreEqual(std::uint32_t{5'000'000}, health.tickMicroseconds[2]);
    Assert::IsTrue(health.longestPart == Outpost::TickPart::Overseer);
    Assert::AreEqual(std::uint64_t{1'219}, health.seats.at(0).deputyTick.value_or(0));
    Assert::AreEqual(std::uint64_t{26}, health.seats.at(0).connection.value_or(Outpost::LinkFigures{}).bytesSent);
    const Outpost::AwayReport away = health.seats.at(0).away.value_or(Outpost::AwayReport{});
    Assert::AreEqual(std::uint32_t{41}, away.structuresLost, L"an away seat's report");
    Assert::IsTrue(away.sectorsLost == std::vector<std::int32_t>{43, 44});
    Assert::IsFalse(health.seats.at(1).away.has_value());
    Assert::IsTrue(health.seats.at(1).play == Outpost::SeatPlay::Ai);
    Assert::AreEqual(std::uint32_t{18}, health.joins.back());
    const auto decodedWelcome = Decoded<Outpost::OverseerWelcomeMessage>(Outpost::EncodeOverseerMessage(welcome));
    Assert::AreEqual(std::string("Hard"), decodedWelcome.seats.at(1).ai);
    Assert::AreEqual(std::uint64_t{1'200}, decodedWelcome.recoveredSaveTick.value_or(0));
    Assert::AreEqual(std::uint32_t{60}, decodedWelcome.saveSeconds);
    Assert::IsTrue(decodedWelcome.dayRecords == welcome.dayRecords, L"the day's records");
  }

  TEST_METHOD(ReadsAHelloOfAnotherBuildBeforeItDecodes)
  {
    const std::vector<std::byte> hello = Outpost::EncodeOverseerMessage(Outpost::OverseerHelloMessage{});
    Assert::AreEqual(Outpost::OVERSEER_PROTOCOL_VERSION, Outpost::PeekOverseerHelloVersion(hello).value_or(0));
    Assert::AreEqual(Outpost::OverseerLayoutHash(), Outpost::PeekOverseerHelloLayout(hello).value_or(0));

    // Another version, another layout, read whatever follows them.
    const std::vector<std::byte> other =
      Outpost::EncodeOverseerMessage(Outpost::OverseerHelloMessage{.protocolVersion = 7, .layoutHash = 0x0102030405060708ull});
    Assert::AreEqual(std::uint32_t{7}, Outpost::PeekOverseerHelloVersion(std::span(other).first(5)).value_or(0));
    Assert::AreEqual(std::uint64_t{0x0102030405060708ull}, Outpost::PeekOverseerHelloLayout(std::span(other).first(13)).value_or(0));

    // Bytes that end before the field, and a message that is no hello.
    Assert::IsFalse(Outpost::PeekOverseerHelloVersion(std::span(hello).first(4)).has_value());
    Assert::IsFalse(Outpost::PeekOverseerHelloLayout(std::span(hello).first(12)).has_value());
    const std::vector<std::byte> sight = Outpost::EncodeOverseerMessage(Outpost::WatchSightMessage{});
    Assert::IsFalse(Outpost::PeekOverseerHelloVersion(sight).has_value());
    // A game's hello carries the game's layout, which is not the overseer's.
    Assert::IsFalse(Outpost::PeekOverseerHelloLayout(Outpost::EncodeMessage(Outpost::HelloMessage{})).value_or(0) ==
                    Outpost::OverseerLayoutHash());
  }

  TEST_METHOD(LaysOutTheGamesMessagesWithinItsOwn)
  {
    // The view crosses as the game's own state messages, so a change to their layout refuses an overseer of another build.
    const std::string layout = Outpost::OverseerLayout();
    const std::string game = Outpost::WireLayout();
    Assert::IsTrue(layout.size() > game.size() && layout.ends_with(game), L"The game's layout ends the overseer's");
    Assert::AreNotEqual(Outpost::WireLayoutHash(), Outpost::OverseerLayoutHash());
  }

  TEST_METHOD(RefusesWhatIsNoMessage)
  {
    const std::vector<std::byte> closing =
      Outpost::EncodeOverseerMessage(Outpost::ClosingMessage{.ending = Outpost::ServerEnding::StoppedByWindows, .lastSaveTick = 1});
    // Cut short, run on, a kind no message is, and an ending past the last.
    Assert::ExpectException<Neuron::Exception>([&] { (void)Outpost::DecodeOverseerMessage(std::span(closing).first(closing.size() - 1)); });
    std::vector<std::byte> longer = closing;
    longer.push_back(std::byte{0});
    Assert::ExpectException<Neuron::Exception>([&] { (void)Outpost::DecodeOverseerMessage(longer); });
    Assert::ExpectException<Neuron::Exception>([&] { (void)Outpost::DecodeOverseerMessage(WithByte(closing, 0, 200)); });
    Assert::ExpectException<Neuron::Exception>([&] { (void)Outpost::DecodeOverseerMessage(WithByte(closing, 1, 3)); });
    Assert::ExpectException<Neuron::Exception>([] { (void)Outpost::DecodeOverseerMessage({}); });
  }

  TEST_METHOD(SaysWhyItClosedTheOverseer)
  {
    for (const Outpost::OverseerCloseReason reason :
         {Outpost::OverseerCloseReason::Finished, Outpost::OverseerCloseReason::WrongVersion, Outpost::OverseerCloseReason::TokenRefused,
          Outpost::OverseerCloseReason::MalformedMessage, Outpost::OverseerCloseReason::Taken})
      Assert::IsTrue(Outpost::DescribeOverseerClose(static_cast<std::uint64_t>(reason)).has_value());
    Assert::IsFalse(Outpost::DescribeOverseerClose(Neuron::FRAMING_ERROR_CODE).has_value());
  }

  TEST_METHOD(TakesAConnectionsFiguresAsTheyAre)
  {
    const Neuron::QuicStatistics statistics{.roundTripMicroseconds = 1,
                                            .leastRoundTripMicroseconds = 2,
                                            .packetsSent = 3,
                                            .packetsLost = 4,
                                            .congestionWindowBytes = 5,
                                            .backlogBytes = 6,
                                            .bytesSent = 7};
    const Outpost::LinkFigures figures = Outpost::FiguresOf(statistics);
    Assert::AreEqual(std::uint32_t{1}, figures.roundTripMicroseconds);
    Assert::AreEqual(std::uint32_t{2}, figures.leastRoundTripMicroseconds);
    Assert::AreEqual(std::uint64_t{3}, figures.packetsSent);
    Assert::AreEqual(std::uint64_t{4}, figures.packetsLost);
    Assert::AreEqual(std::uint32_t{5}, figures.congestionWindowBytes);
    Assert::AreEqual(std::uint64_t{6}, figures.backlogBytes);
    Assert::AreEqual(std::uint64_t{7}, figures.bytesSent);
  }
};
} // namespace GameProtocolTests

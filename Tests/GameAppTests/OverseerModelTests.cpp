#include "pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameAppTests
{
namespace
{
using Clock = std::chrono::steady_clock;

const Clock::time_point START{std::chrono::seconds{1'000}};

// A welcome of a universe of two seats and one sector, as the server sends it.
Outpost::OverseerWelcomeMessage Welcome()
{
  return {
    .wireLayoutHash = Outpost::WireLayoutHash(),
    .protocolVersion = Outpost::PROTOCOL_VERSION,
    .stateVersion = 1,
    .ticksPerSecond = 20,
    .folderUtf8 = "C:\\Universe",
    .seats = {{.player = Outpost::PlayerId{1}, .ai = {}}, {.player = Outpost::PlayerId{2}, .ai = "Normal"}},
    .sectors = {{.id = 1, .nameUtf8 = "Alpha", .minXMeters = -500.0f, .maxXMeters = 500.0f, .minZMeters = -500.0f, .maxZMeters = 500.0f}},
    .fields = {{.id = Outpost::EntityId{900}, .kind = Outpost::EntityKind::AsteroidField, .radiusMeters = 80.0f}},
    .recoveredSaveTick = 1'200,
    .replayedCommands = 7};
}

Outpost::HealthMessage HealthAt(std::uint64_t _tick)
{
  return {.wallMilliseconds = 1'700'000'000'000, .tick = _tick, .tickMicroseconds = {800, 900}, .connections = 1};
}

// The universe as the server's thread builds it at _tick: its sector, held by player 1, and its asteroid field, and a ship of
// player 1 that moves on with the tick.
Outpost::Snapshot ViewAt(std::uint64_t _tick)
{
  Outpost::Snapshot view{.tick = _tick};
  Outpost::OverseerWelcomeMessage welcome = Welcome();
  view.sectors = welcome.sectors;
  view.sectors.front().holder = Outpost::PlayerId{1};
  view.entities.push_back({.id = Outpost::EntityId{5},
                           .owner = Outpost::PlayerId{1},
                           .position = {.xMeters = static_cast<float>(_tick), .zMeters = 10.0f},
                           .radiusMeters = 5.0f,
                           .hitPointsHundredths = 10'000,
                           .maxHitPointsHundredths = 10'000});
  view.entities.push_back(welcome.fields.front());
  return view;
}

// The server's end of the views, as OverseerLink sends them: without their catalogue and events, whole or as what changed
// since the view the overseer holds.
class ViewSender
{
public:
  std::vector<std::byte> Send(Outpost::Snapshot _view, std::optional<std::uint64_t> _held)
  {
    (void)Outpost::TakeCatalog(_view);
    (void)Outpost::TakeEvents(_view);
    const Outpost::StateMessage state{.snapshot = std::move(_view)};
    std::vector<std::byte> bytes = m_sent.Encode(state, _held);
    m_sent.Keep(state.snapshot);
    return bytes;
  }

private:
  Outpost::SentStates m_sent;
};

std::vector<std::byte> Bytes(const Outpost::OverseerWelcomeMessage& _message)
{
  return Outpost::EncodeOverseerMessage(_message);
}

std::vector<std::byte> Bytes(const Outpost::HealthMessage& _message)
{
  return Outpost::EncodeOverseerMessage(_message);
}

std::vector<std::byte> Bytes(const Outpost::LogRecordsMessage& _message)
{
  return Outpost::EncodeOverseerMessage(_message);
}
} // namespace

TEST_CLASS(OverseerModelTests)
{
public:
  // ADR-102: the welcome is what never changes while the server runs, and it starts what a connection tells the overseer.
  TEST_METHOD(TakesTheWelcome)
  {
    Outpost::OverseerModel model;
    Assert::IsFalse(model.Welcome().has_value(), L"nothing before a welcome");
    model.Take(Bytes(Welcome()), START);
    Assert::IsTrue(model.Welcome().has_value());
    const Outpost::OverseerWelcomeMessage welcome = model.Welcome().value_or(Outpost::OverseerWelcomeMessage{});
    Assert::AreEqual(std::string("C:\\Universe"), welcome.folderUtf8);
    Assert::AreEqual(std::size_t{2}, welcome.seats.size());
    Assert::AreEqual(std::uint64_t{1'200}, welcome.recoveredSaveTick.value_or(0), L"what it came back from");
    Assert::IsNull(model.LatestHealth(), L"no Health yet");
  }

  // ADR-102: the newest Health, when the overseer heard it, and the last hour of them, one a second, and no more.
  TEST_METHOD(KeepsAnHourOfHealth)
  {
    Outpost::OverseerModel model;
    model.Take(Bytes(Welcome()), START);
    for (std::uint64_t second = 0; second < Outpost::OverseerModel::HEALTH_KEPT + 5; ++second)
      model.Take(Bytes(HealthAt(second * 20)), START + std::chrono::seconds{second});
    Assert::AreEqual(Outpost::OverseerModel::HEALTH_KEPT, model.Healths().size(), L"an hour of them");
    Assert::AreEqual(std::uint64_t{5} * 20, model.Healths().front().health.tick, L"the oldest gone");
    const Outpost::HeardHealth* latest = model.LatestHealth();
    Assert::IsNotNull(latest);
    Assert::AreEqual((Outpost::OverseerModel::HEALTH_KEPT + 4) * 20, latest->health.tick, L"the newest");
    Assert::IsTrue(latest->heard == START + std::chrono::seconds{Outpost::OverseerModel::HEALTH_KEPT + 4}, L"heard on its own clock");
    Assert::AreEqual(std::size_t{2}, latest->health.tickMicroseconds.size(), L"each tick's time");
  }

  // ADR-102: the log's records in the order the server wrote them, the last RECORDS_KEPT of them; a new connection's welcome
  // replaces them, since the server sends its last ones again after it.
  TEST_METHOD(KeepsTheLogsRecords)
  {
    Outpost::OverseerModel model;
    model.Take(Bytes(Welcome()), START);
    model.Take(Bytes(Outpost::LogRecordsMessage{.lines = {"start 0 new", "save 1200 1 KB"}}), START);
    model.Take(Bytes(Outpost::LogRecordsMessage{.lines = {"seat 1300 1 online"}}), START);
    Assert::AreEqual(std::size_t{3}, model.Records().size());
    Assert::AreEqual(std::string("start 0 new"), model.Records().front());
    Assert::AreEqual(std::string("seat 1300 1 online"), model.Records().back(), L"in the order written");

    Outpost::LogRecordsMessage many;
    for (std::size_t line = 0; line < Outpost::OverseerModel::RECORDS_KEPT; ++line)
      many.lines.push_back(std::format("tick {}", line));
    model.Take(Bytes(many), START);
    Assert::AreEqual(Outpost::OverseerModel::RECORDS_KEPT, model.Records().size(), L"the last of them");
    Assert::AreEqual(std::string("tick 0"), model.Records().front(), L"the oldest gone");

    model.Lost("gone", START);
    model.Take(Bytes(Welcome()), START);
    Assert::IsTrue(model.Records().empty(), L"a new connection's welcome replaces them");
  }

  // ADR-102, ADR-101 decision 6: a view crosses whole and then as what changed since the one the overseer holds, and is put
  // together with the welcome's sectors and fields into the view the server built.
  TEST_METHOD(PutsTheViewsTogether)
  {
    Outpost::OverseerModel model;
    ViewSender sender;
    Assert::IsFalse(model.TakeView(sender.Send(ViewAt(10), std::nullopt)).has_value(), L"none before its connection's welcome");

    Outpost::OverseerModel welcomed;
    ViewSender server;
    welcomed.Take(Bytes(Welcome()), START);
    const std::optional<std::uint64_t> first = welcomed.TakeView(server.Send(ViewAt(10), std::nullopt));
    Assert::AreEqual(std::uint64_t{10}, first.value_or(0), L"the tick it holds");
    const std::vector<std::byte> change = server.Send(ViewAt(20), first);
    Assert::IsTrue(std::holds_alternative<Outpost::StateChangeMessage>(Outpost::DecodeMessage(change)), L"a change of the one held");
    Assert::AreEqual(std::uint64_t{20}, welcomed.TakeView(change).value_or(0));

    Assert::IsTrue(welcomed.View().has_value());
    const Outpost::Snapshot view = welcomed.View().value_or(Outpost::Snapshot{});
    const Outpost::Snapshot built = ViewAt(20);
    Assert::IsTrue(Outpost::EncodeMessage(Outpost::StateMessage{.snapshot = view}) ==
                     Outpost::EncodeMessage(Outpost::StateMessage{.snapshot = built}),
                   L"put together, it is the view the server built");
    Assert::AreEqual(std::string("Alpha"), view.sectors.front().nameUtf8, L"the sector's shape from the welcome");
    Assert::AreEqual(std::uint32_t{1}, view.sectors.front().holder.value, L"its holder from the view");
  }

  // ADR-102: how the server ended, once it has said.
  TEST_METHOD(KeepsTheClosing)
  {
    Outpost::OverseerModel model;
    model.Take(Bytes(Welcome()), START);
    model.Take(Outpost::EncodeOverseerMessage(
                 Outpost::ClosingMessage{.ending = Outpost::ServerEnding::Failed, .messageUtf8 = "out of disk", .lastSaveTick = 4'000}),
               START);
    Assert::IsTrue(model.Closing().has_value());
    const Outpost::ClosingMessage closing = model.Closing().value_or(Outpost::ClosingMessage{});
    Assert::IsTrue(closing.ending == Outpost::ServerEnding::Failed);
    Assert::AreEqual(std::string("out of disk"), closing.messageUtf8);
    model.Take(Bytes(Welcome()), START);
    Assert::IsFalse(model.Closing().has_value(), L"a server that welcomes the overseer again runs");
  }

  // ADR-102: a connection gone leaves what the model knew as it was then, and says why and when; no view is taken until the next
  // connection's welcome, whose views the server sends whole first, and that welcome forgets why the last went.
  TEST_METHOD(KeepsWhatItKnewWhenTheConnectionGoes)
  {
    Outpost::OverseerModel model;
    ViewSender server;
    model.Take(Bytes(Welcome()), START);
    model.Take(Bytes(HealthAt(40)), START);
    const std::optional<std::uint64_t> held = model.TakeView(server.Send(ViewAt(40), std::nullopt));
    model.Lost("The connection timed out.", START + std::chrono::seconds{5});
    Assert::AreEqual(std::string("The connection timed out."), model.LostWhy().value_or(""));
    Assert::IsTrue(model.LostAt() == START + std::chrono::seconds{5});
    Assert::IsNotNull(model.LatestHealth(), L"the last Health stays");
    Assert::IsTrue(model.View().has_value(), L"and the last view");
    Assert::IsTrue(model.Welcome().has_value(), L"and the welcome");
    Assert::IsFalse(model.TakeView(server.Send(ViewAt(50), held)).has_value(), L"no view between connections");

    ViewSender next;
    model.Take(Bytes(Welcome()), START + std::chrono::seconds{7});
    Assert::IsFalse(model.LostWhy().has_value(), L"the next welcome forgets why the last went");
    Assert::IsFalse(model.LostAt().has_value());
    Assert::AreEqual(std::uint64_t{60}, model.TakeView(next.Send(ViewAt(60), std::nullopt)).value_or(0), L"its views whole first");
  }

  // ADR-102: what only an overseer sends, or no message at all, is refused.
  TEST_METHOD(RefusesWhatTheServerDoesNotSend)
  {
    Outpost::OverseerModel model;
    const auto take = [&model](const std::vector<std::byte>& _bytes) { model.Take(_bytes, START); };
    Assert::ExpectException<Neuron::Exception>([&] { take(Outpost::EncodeOverseerMessage(Outpost::OverseerHelloMessage{})); });
    Assert::ExpectException<Neuron::Exception>([&] { take(Outpost::EncodeOverseerMessage(Outpost::ViewHeldMessage{.tick = 3})); });
    Assert::ExpectException<Neuron::Exception>([&] { take({std::byte{0xFF}}); });
  }
};
} // namespace GameAppTests

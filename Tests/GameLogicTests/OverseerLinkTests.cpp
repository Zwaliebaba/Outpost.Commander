#include "pch.h"
#include "RepositoryData.h"
#include "UniverseMatch.h"

#include <charconv>
#include <sstream>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace std::chrono_literals;

namespace GameLogicTests
{
namespace
{
constexpr Outpost::PlayerId BLUE{1};
constexpr Outpost::PlayerId RED{2};

std::wstring Widen(std::string_view _text)
{
  return std::wstring(winrt::to_hstring(_text));
}

// A hosted player that gives no order: seat 1's deputy, and seat 2's AI empire.
class Idle final : public Outpost::HostedPlayer
{
public:
  [[nodiscard]] std::vector<Outpost::Command> Play([[maybe_unused]] const Outpost::Snapshot& _snapshot) override
  {
    return {};
  }
};

// A universe's server, as the dedicated server sets one up (ADR-078): seat 1 a player's, open, with a deputy, and seat 2 an
// AI empire's; and its overseer's link on the loopback, with _token.
struct Universe
{
  std::unique_ptr<Outpost::InProcessServer> server;
  Outpost::ServerAddress seat;
  Outpost::ServerAddress overseer;
};

Universe OverseenUniverse(const std::filesystem::path& _folder, const Outpost::SeatToken& _token)
{
  Universe universe{.server = Outpost::CreateUniverseServer({.seed = 5, .quic = true, .universe = _folder})};
  universe.seat = universe.server->OpenSeat(BLUE, Outpost::NewSeatToken());
  universe.server->Host(BLUE, std::make_unique<Idle>());
  universe.server->Host(RED, std::make_unique<Idle>());
  universe.overseer = universe.server->Oversee({.token = _token}, {{.player = BLUE}, {.player = RED, .ai = "Normal"}});
  return universe;
}

// The overseer's end of its channel, as a test plays it.
class Overseer
{
public:
  explicit Overseer(const Outpost::ServerAddress& _address)
    : m_channel(Neuron::QuicChannel::Connect({.host = _address.host,
                                              .port = _address.port,
                                              .applicationProtocol = std::string(Outpost::OVERSEER_APPLICATION_PROTOCOL),
                                              .serverCertificate = _address.certificate}))
  {
  }

  template <typename T> void Say(const T& _message)
  {
    m_channel->Send(Outpost::EncodeOverseerMessage(_message));
  }

  // The next message of the server's that is a T, within _wait, those before it passed over.
  template <typename T> T Next(std::chrono::milliseconds _wait = 10s)
  {
    const auto deadline = std::chrono::steady_clock::now() + _wait;
    while (true)
    {
      while (!m_pending.empty())
      {
        Outpost::OverseerMessage message = Outpost::DecodeOverseerMessage(m_pending.front());
        m_pending.pop_front();
        if (auto* found = std::get_if<T>(&message))
          return std::move(*found);
      }
      Assert::IsTrue(std::chrono::steady_clock::now() < deadline, L"the server's message came in time");
      for (std::vector<std::byte>& bytes : m_channel->Receive(100ms))
        m_pending.push_back(std::move(bytes));
    }
  }

  // The next message of the server's that is a T, those before it passed over, from a server stepped by hand: _server steps a
  // tick between each look, for 10 s at most.
  template <typename T> T NextStepping(Outpost::InProcessServer& _server)
  {
    const auto deadline = std::chrono::steady_clock::now() + 10s;
    while (true)
    {
      while (!m_pending.empty())
      {
        Outpost::OverseerMessage message = Outpost::DecodeOverseerMessage(m_pending.front());
        m_pending.pop_front();
        if (auto* found = std::get_if<T>(&message))
          return std::move(*found);
      }
      Assert::IsTrue(std::chrono::steady_clock::now() < deadline, L"the server's message came in time");
      _server.Step();
      for (std::vector<std::byte>& bytes : m_channel->Receive(10ms))
        m_pending.push_back(std::move(bytes));
    }
  }

  // The views that came whole within _wait, each put together with the view it changes and told back as held (ADR-098).
  std::vector<Outpost::Snapshot> Views(std::chrono::milliseconds _wait)
  {
    std::vector<Outpost::Snapshot> views;
    const auto deadline = std::chrono::steady_clock::now() + _wait;
    while (std::chrono::steady_clock::now() < deadline)
    {
      for (std::vector<std::byte>& bytes : m_channel->ReceiveNewest())
      {
        Outpost::StateMessage state = m_held.Take(Outpost::DecodeMessage(bytes));
        Say(Outpost::ViewHeldMessage{.tick = state.snapshot.tick});
        views.push_back(std::move(state.snapshot));
      }
      std::this_thread::sleep_for(10ms);
    }
    return views;
  }

  // The error code the server closed the channel with, or 0 when it has not within _wait.
  std::uint64_t CloseCode(std::chrono::milliseconds _wait = 10s)
  {
    const auto deadline = std::chrono::steady_clock::now() + _wait;
    while (std::chrono::steady_clock::now() < deadline)
    {
      try
      {
        for (std::vector<std::byte>& bytes : m_channel->Receive(100ms))
          m_pending.push_back(std::move(bytes));
      }
      catch (const Neuron::Exception&)
      {
        return m_channel->PeerErrorCode().value_or(0);
      }
    }
    return 0;
  }

private:
  std::unique_ptr<Neuron::QuicChannel> m_channel;
  std::deque<std::vector<std::byte>> m_pending;
  Outpost::HeldStates m_held;
};

// The tick of the save that _words of Save now's result tell, or none.
std::optional<std::uint64_t> SavedTick(std::string_view _words)
{
  constexpr std::string_view SAVED = "Saved at tick ";
  if (!_words.starts_with(SAVED))
    return std::nullopt;
  _words.remove_prefix(SAVED.size());
  std::uint64_t tick = 0;
  const auto [end, error] = std::from_chars(_words.data(), _words.data() + _words.size(), tick);
  return error == std::errc{} && end != _words.data() ? std::optional(tick) : std::nullopt;
}

// How many of _log's lines end with _words.
std::size_t LinesEndingWith(const std::string& _log, std::string_view _words)
{
  std::size_t count = 0;
  std::istringstream in(_log);
  for (std::string line; std::getline(in, line);)
    count += line.ends_with(_words) ? 1 : 0;
  return count;
}

// Whether the universe's log in _folder comes to hold each of _expected within 20 seconds, as the server's thread writes the
// records at its next tick.
bool LogHolds(const std::filesystem::path& _folder, std::span<const std::string> _expected, std::string& _log)
{
  const auto deadline = std::chrono::steady_clock::now() + 20s;
  const auto holdsAll = [&] { return std::ranges::all_of(_expected, [&](const std::string& _words) { return _log.contains(_words); }); };
  while (!holdsAll() && std::chrono::steady_clock::now() < deadline)
  {
    std::this_thread::sleep_for(50ms);
    std::ifstream in(_folder / Outpost::UNIVERSE_LOG_FILE);
    _log.assign(std::istreambuf_iterator<char>(in), {});
  }
  return holdsAll();
}
} // namespace

// ADR-101: the overseer's own channel to a universe's server, over a real QUIC connection on the loopback, through MsQuic.
TEST_CLASS(OverseerLinkTests)
{
public:
  // An overseer that shows the token is welcomed with what never changes while the server runs, and then the universe log's
  // last records; the listener presents the universe's certificate, as the game's does, and says where the overseer reaches it.
  TEST_METHOD(WelcomesAnOverseerWithTheToken)
  {
    const PackagedHomeDirectory home;
    const TemporaryFolder folder;
    const Outpost::SeatToken token = Outpost::NewSeatToken();
    Universe universe = OverseenUniverse(folder.Path(), token);
    universe.server->Start();
    Assert::IsTrue(universe.overseer.certificate == universe.seat.certificate, L"the universe's certificate");
    Assert::IsTrue(universe.overseer.token == token);
    Assert::AreEqual(std::string("127.0.0.1"), universe.overseer.host);

    Overseer overseer(universe.overseer);
    overseer.Say(Outpost::OverseerHelloMessage{.token = token});
    const auto welcome = overseer.Next<Outpost::OverseerWelcomeMessage>();
    Assert::AreEqual(Outpost::WireLayoutHash(), welcome.wireLayoutHash);
    Assert::AreEqual(Outpost::PROTOCOL_VERSION, welcome.protocolVersion);
    Assert::AreEqual(Outpost::UNIVERSE_STATE_VERSION, welcome.stateVersion);
    Assert::AreEqual(universe.server->TicksPerSecond(), welcome.ticksPerSecond);
    const std::u8string path = folder.Path().u8string();
    Assert::AreEqual(std::string(reinterpret_cast<const char*>(path.data()), path.size()), welcome.folderUtf8);
    Assert::AreEqual(std::size_t{2}, welcome.seats.size());
    Assert::IsTrue(welcome.seats[0].player == BLUE && welcome.seats[0].ai.empty());
    Assert::IsTrue(welcome.seats[1].player == RED && welcome.seats[1].ai == "Normal");
    Assert::IsFalse(welcome.sectors.empty(), L"the sectors");
    Assert::IsTrue(std::ranges::all_of(welcome.sectors, [](const Outpost::SectorView& _sector) { return !_sector.nameUtf8.empty(); }),
                   L"with their shapes");
    Assert::IsFalse(welcome.recoveredSaveTick.has_value(), L"a new universe");
    const auto records = overseer.Next<Outpost::LogRecordsMessage>();
    Assert::IsTrue(!records.lines.empty() && records.lines.front() == "start 0 new", L"the log's last records, from its start");
  }

  // ADR-082 decision 1: each refusal is written with its reason and the address it came from: a hello of another version or layout, a
  // token that is not the universe's, a first message that is no hello, and no hello in time; and an overseer welcomed.
  TEST_METHOD(RefusesWhatIsNotItsOverseerAndLogsIt)
  {
    const PackagedHomeDirectory home;
    const TemporaryFolder folder;
    const Outpost::SeatToken token = Outpost::NewSeatToken();
    Universe universe = OverseenUniverse(folder.Path(), token);
    universe.server->Start();
    const auto refusedWith = [&universe](const std::vector<std::byte>& _first)
    {
      const std::unique_ptr<Neuron::QuicChannel> channel =
        Neuron::QuicChannel::Connect({.host = universe.overseer.host,
                                      .port = universe.overseer.port,
                                      .applicationProtocol = std::string(Outpost::OVERSEER_APPLICATION_PROTOCOL),
                                      .serverCertificate = universe.overseer.certificate});
      channel->Send(_first);
      const auto deadline = std::chrono::steady_clock::now() + 15s;
      while (std::chrono::steady_clock::now() < deadline)
      {
        try
        {
          (void)channel->Receive(100ms);
        }
        catch (const Neuron::Exception&)
        {
          return channel->PeerErrorCode().value_or(0);
        }
      }
      return std::uint64_t{0};
    };
    const auto code = [](Outpost::OverseerCloseReason _reason) { return static_cast<std::uint64_t>(_reason); };
    Assert::AreEqual(code(Outpost::OverseerCloseReason::WrongVersion),
                     refusedWith(Outpost::EncodeOverseerMessage(Outpost::OverseerHelloMessage{.protocolVersion = 2, .token = token})));
    Assert::AreEqual(code(Outpost::OverseerCloseReason::WrongVersion),
                     refusedWith(Outpost::EncodeOverseerMessage(
                       Outpost::OverseerHelloMessage{.layoutHash = Outpost::OverseerLayoutHash() + 1, .token = token})));
    Outpost::SeatToken guessed = token;
    guessed.front() ^= 0x01;
    Assert::AreEqual(code(Outpost::OverseerCloseReason::TokenRefused),
                     refusedWith(Outpost::EncodeOverseerMessage(Outpost::OverseerHelloMessage{.token = guessed})));
    Assert::AreEqual(code(Outpost::OverseerCloseReason::MalformedMessage),
                     refusedWith(Outpost::EncodeOverseerMessage(Outpost::WatchSightMessage{})));
    Assert::AreEqual(code(Outpost::OverseerCloseReason::WrongVersion),
                     refusedWith(Outpost::EncodeMessage(Outpost::HelloMessage{.player = BLUE, .token = universe.seat.token})),
                     L"a game's hello reads as a hello of another build, by the layout it carries");
    {
      Overseer silent(universe.overseer);
      Assert::AreEqual(code(Outpost::OverseerCloseReason::MalformedMessage), silent.CloseCode(15s), L"no hello in time");
    }
    Overseer welcomed(universe.overseer);
    welcomed.Say(Outpost::OverseerHelloMessage{.token = token});
    (void)welcomed.Next<Outpost::OverseerWelcomeMessage>();

    const std::vector<std::string> expected{" refused version from 127.0.0.1", " refused token from 127.0.0.1",
                                            " refused malformed from 127.0.0.1", " refused silent from 127.0.0.1",
                                            " opened from 127.0.0.1"};
    std::string log;
    Assert::IsTrue(LogHolds(folder.Path(), expected, log), Widen(log).c_str());
  }

  // One overseer at a time: the newest connection to show the token holds the channel, and the one before it is closed and
  // written as closed.
  TEST_METHOD(GivesTheChannelToTheNewestOverseer)
  {
    const PackagedHomeDirectory home;
    const TemporaryFolder folder;
    const Outpost::SeatToken token = Outpost::NewSeatToken();
    Universe universe = OverseenUniverse(folder.Path(), token);
    universe.server->Start();
    Overseer first(universe.overseer);
    first.Say(Outpost::OverseerHelloMessage{.token = token});
    (void)first.Next<Outpost::OverseerWelcomeMessage>();
    Overseer second(universe.overseer);
    second.Say(Outpost::OverseerHelloMessage{.token = token});
    (void)second.Next<Outpost::OverseerWelcomeMessage>();
    Assert::AreEqual(static_cast<std::uint64_t>(Outpost::OverseerCloseReason::Taken), first.CloseCode());

    std::string log;
    const std::vector<std::string> expected{" opened from 127.0.0.1", " closed from 127.0.0.1"};
    Assert::IsTrue(LogHolds(folder.Path(), expected, log), Widen(log).c_str());
    Assert::AreEqual(std::size_t{2}, LinesEndingWith(log, " opened from 127.0.0.1"), L"two openings");
  }

  // Health once a second, sent from the thread that started the server: the tick, the ticks run, the seats with who plays
  // them and their links, and the process's figures it is handed.
  TEST_METHOD(SendsHealthEverySecond)
  {
    const PackagedHomeDirectory home;
    const TemporaryFolder folder;
    const Outpost::SeatToken token = Outpost::NewSeatToken();
    Universe universe = OverseenUniverse(folder.Path(), token);
    universe.server->Start();
    Overseer overseer(universe.overseer);
    overseer.Say(Outpost::OverseerHelloMessage{.token = token});
    (void)overseer.Next<Outpost::OverseerWelcomeMessage>();

    std::this_thread::sleep_for(1200ms);
    universe.server->Report({.workingSetBytes = 42, .threads = 3});
    const auto first = overseer.Next<Outpost::HealthMessage>();
    Assert::IsTrue(first.tick >= universe.server->TicksPerSecond(), L"a second of ticks kept");
    Assert::IsFalse(first.tickMicroseconds.empty(), L"the ticks run");
    Assert::AreEqual(std::uint64_t{42}, first.process.workingSetBytes);
    Assert::AreEqual(std::size_t{2}, first.seats.size());
    Assert::IsTrue(first.seats[0].link == Outpost::SeatLink::Open && first.seats[0].play == Outpost::SeatPlay::Deputy,
                   L"seat 1 not yet taken, its deputy playing it");
    Assert::IsTrue(first.seats[1].link == Outpost::SeatLink::None && first.seats[1].play == Outpost::SeatPlay::Ai);

    // The seat taken: online, its player playing it, with its connection's figures.
    Outpost::QuicTransport blue(universe.seat, BLUE);
    Outpost::HealthMessage online;
    const auto isOnline = [&online] { return online.seats.size() == 2 && online.seats[0].link == Outpost::SeatLink::Online; };
    const auto deadline = std::chrono::steady_clock::now() + 10s;
    while (!isOnline() && std::chrono::steady_clock::now() < deadline)
    {
      (void)blue.Receive();
      std::this_thread::sleep_for(1s);
      universe.server->Report({});
      online = overseer.Next<Outpost::HealthMessage>();
    }
    Assert::IsTrue(isOnline(), L"seat 1 online");
    Assert::IsTrue(online.seats[0].play == Outpost::SeatPlay::Player);
    Assert::IsTrue(online.seats[0].connection.has_value(), L"with its connection's figures");
    Assert::IsTrue(online.joins[static_cast<std::size_t>(Outpost::JoinOutcome::Welcomed)] >= 1, L"its join counted");
    Assert::IsTrue(online.connections >= 1, L"the game's listener holds its connection");
  }

  // ADR-101 decision 7 (owner, 2026-10-28): each welcome carries the day's records of the last day of universe time, those the
  // log held as the link was made and those written since, a join's among them, and how many seconds of ticks go between two
  // saves.
  TEST_METHOD(WelcomesWithTheDayRecords)
  {
    const PackagedHomeDirectory home;
    const TemporaryFolder folder;
    const Outpost::SeatToken token = Outpost::NewSeatToken();
    Universe universe = OverseenUniverse(folder.Path(), token);
    universe.server->Start();
    const auto welcomed = [&universe, &token]
    {
      Overseer overseer(universe.overseer);
      overseer.Say(Outpost::OverseerHelloMessage{.token = token});
      return overseer.Next<Outpost::OverseerWelcomeMessage>();
    };
    const Outpost::OverseerWelcomeMessage first = welcomed();
    Assert::AreEqual(Outpost::UNIVERSE_SAVE_SECONDS, first.saveSeconds);
    const auto starts = [](const std::string& _record) { return _record.starts_with("start 0 "); };

    // Seat 1 taken: its player plays it from the next tick, which the log writes as a hand-over.
    Outpost::QuicTransport blue(universe.seat, BLUE);
    const auto handedOver = [](const std::string& _record)
    { return _record.starts_with("seat ") && _record.ends_with(" player 1 player"); };
    Outpost::OverseerWelcomeMessage later;
    const auto deadline = std::chrono::steady_clock::now() + 10s;
    while (!std::ranges::any_of(later.dayRecords, handedOver) && std::chrono::steady_clock::now() < deadline)
    {
      (void)blue.Receive();
      std::this_thread::sleep_for(500ms);
      later = welcomed();
    }
    Assert::IsTrue(std::ranges::any_of(later.dayRecords, handedOver), L"the hand-over written since the link was made");
    Assert::IsTrue(std::ranges::any_of(later.dayRecords, starts), L"and the universe's start");
    Assert::IsTrue(std::ranges::any_of(later.dayRecords, [](const std::string& _record)
                                       { return _record.starts_with("join ") && _record.contains(" welcomed player 1 "); }),
                   L"and the join that took the seat");
    Assert::IsTrue(std::ranges::none_of(later.dayRecords, [](const std::string& _record) { return _record.starts_with("overseer "); }),
                   L"and no record of another kind");
  }

  // A view twice a second, full sight, put together by HeldStates whole or as what changed; a seat's own sight while the
  // overseer watches it, and full sight again; and each new record of the universe log as it is written.
  TEST_METHOD(SendsTheViewAndTheLog)
  {
    const PackagedHomeDirectory home;
    const TemporaryFolder folder;
    const Outpost::SeatToken token = Outpost::NewSeatToken();
    Universe universe = OverseenUniverse(folder.Path(), token);
    universe.server->Start();
    Overseer overseer(universe.overseer);
    overseer.Say(Outpost::OverseerHelloMessage{.token = token});
    (void)overseer.Next<Outpost::OverseerWelcomeMessage>();

    const std::vector<Outpost::Snapshot> views = overseer.Views(2s);
    Assert::IsTrue(views.size() >= 2, Widen(std::format("{} views in 2 s", views.size())).c_str());
    const std::uint64_t every = universe.server->TicksPerSecond() / Outpost::OVERSEER_VIEWS_PER_SECOND;
    for (const Outpost::Snapshot& view : views)
    {
      Assert::AreEqual(std::uint64_t{0}, view.tick % every, L"after every tenth tick");
      Assert::IsFalse(view.player.IsValid(), L"full sight");
    }
    const Outpost::Snapshot& full = views.back();
    Assert::IsTrue(std::ranges::any_of(full.entities, [](const Outpost::EntityView& _entity) { return _entity.owner == BLUE; }) &&
                     std::ranges::any_of(full.entities, [](const Outpost::EntityView& _entity) { return _entity.owner == RED; }),
                   L"both seats' entities");

    overseer.Say(Outpost::WatchSightMessage{.seat = RED});
    std::vector<Outpost::Snapshot> watched = overseer.Views(2s);
    Assert::IsTrue(!watched.empty() && watched.back().player == RED, L"the seat's own sight");
    overseer.Say(Outpost::WatchSightMessage{});
    watched = overseer.Views(2s);
    Assert::IsTrue(!watched.empty() && !watched.back().player.IsValid(), L"full sight again");

    // The overseer's own opening is a record the server's thread writes after it was welcomed.
    bool heard = false;
    const auto deadline = std::chrono::steady_clock::now() + 10s;
    while (!heard && std::chrono::steady_clock::now() < deadline)
    {
      const auto records = overseer.Next<Outpost::LogRecordsMessage>();
      heard = std::ranges::any_of(records.lines, [](const std::string& _line)
                                  { return _line.starts_with("overseer ") && _line.ends_with(" opened from 127.0.0.1"); });
    }
    Assert::IsTrue(heard, L"a new record as it was written");
  }

  // ADR-104: Save now saves the universe at the server's next tick, as it saves each minute, and is answered once the folder
  // has written it; the universe log writes what was asked, and Closing's last save is that one.
  TEST_METHOD(SavesNowWhenAsked)
  {
    const PackagedHomeDirectory home;
    const TemporaryFolder folder;
    const Outpost::SeatToken token = Outpost::NewSeatToken();
    Universe universe = OverseenUniverse(folder.Path(), token);
    universe.server->Start();
    Overseer overseer(universe.overseer);
    overseer.Say(Outpost::OverseerHelloMessage{.token = token});
    (void)overseer.Next<Outpost::OverseerWelcomeMessage>();

    overseer.Say(Outpost::ActionMessage{.action = Outpost::OverseerAction::SaveNow});
    const auto result = overseer.Next<Outpost::ActionResultMessage>();
    Assert::IsTrue(result.action == Outpost::OverseerAction::SaveNow && result.done, Widen(result.wordsUtf8).c_str());
    const std::uint64_t saved = SavedTick(result.wordsUtf8).value_or(0);
    Assert::IsTrue(saved > 0, Widen(result.wordsUtf8).c_str());
    Assert::IsTrue(result.wordsUtf8.ends_with(" ms."), L"and how long it took to write");

    std::string log;
    const std::vector<std::string> expected{std::format("overseer {} save from 127.0.0.1", saved), std::format("save {} bytes ", saved),
                                            std::format("written {} write_us ", saved)};
    Assert::IsTrue(LogHolds(folder.Path(), expected, log), Widen(log).c_str());
    universe.server->Close(Outpost::ServerEnding::StoppedByOwner, {});
    Assert::AreEqual(saved, overseer.Next<Outpost::ClosingMessage>().lastSaveTick, L"the universe's last save");
  }

  // One save asked for at a time: a second, asked for at the same tick, is refused while the first is written, and the first is
  // answered once it is.
  TEST_METHOD(SavesOneAskedSaveAtATime)
  {
    const PackagedHomeDirectory home;
    const TemporaryFolder folder;
    const Outpost::SeatToken token = Outpost::NewSeatToken();
    Universe universe = OverseenUniverse(folder.Path(), token);
    Overseer overseer(universe.overseer);
    overseer.Say(Outpost::OverseerHelloMessage{.token = token});
    (void)overseer.Next<Outpost::OverseerWelcomeMessage>();

    // Both have come before the server, stepped by hand, ticks again.
    overseer.Say(Outpost::ActionMessage{.action = Outpost::OverseerAction::SaveNow});
    overseer.Say(Outpost::ActionMessage{.action = Outpost::OverseerAction::SaveNow});
    std::this_thread::sleep_for(1s);
    const auto refused = overseer.NextStepping<Outpost::ActionResultMessage>(*universe.server);
    Assert::IsFalse(refused.done, Widen(refused.wordsUtf8).c_str());
    Assert::IsTrue(refused.wordsUtf8.contains("still being written"), Widen(refused.wordsUtf8).c_str());
    const auto saved = overseer.NextStepping<Outpost::ActionResultMessage>(*universe.server);
    Assert::IsTrue(saved.done, Widen(saved.wordsUtf8).c_str());
    Assert::AreEqual(std::uint64_t{1}, SavedTick(saved.wordsUtf8).value_or(0), L"the first tick stepped");
  }

  // ADR-104: Stop has the server say it is stopping, for the thread that started it to stop it as Ctrl+C does, and the universe
  // log writes what was asked.
  TEST_METHOD(StopsWhenAsked)
  {
    const PackagedHomeDirectory home;
    const TemporaryFolder folder;
    const Outpost::SeatToken token = Outpost::NewSeatToken();
    Universe universe = OverseenUniverse(folder.Path(), token);
    Overseer overseer(universe.overseer);
    overseer.Say(Outpost::OverseerHelloMessage{.token = token});
    (void)overseer.Next<Outpost::OverseerWelcomeMessage>();
    Assert::IsFalse(universe.server->StopAsked());

    overseer.Say(Outpost::ActionMessage{.action = Outpost::OverseerAction::Stop});
    const auto result = overseer.NextStepping<Outpost::ActionResultMessage>(*universe.server);
    Assert::IsTrue(result.action == Outpost::OverseerAction::Stop && result.done, Widen(result.wordsUtf8).c_str());
    Assert::IsTrue(universe.server->StopAsked(), L"the server asked to stop");
    std::string log;
    const std::vector<std::string> expected{" stop from 127.0.0.1"};
    Assert::IsTrue(LogHolds(folder.Path(), expected, log), Widen(log).c_str());
  }

  // An action that names what it may not, a seat for Stop or an AI empire's seat for Drop link, is no message of an overseer's.
  TEST_METHOD(RefusesAnActionThatNamesWhatItMayNot)
  {
    const PackagedHomeDirectory home;
    const TemporaryFolder folder;
    const Outpost::SeatToken token = Outpost::NewSeatToken();
    Universe universe = OverseenUniverse(folder.Path(), token);
    universe.server->Start();
    const auto refused = [&universe, &token](const Outpost::ActionMessage& _action)
    {
      Overseer overseer(universe.overseer);
      overseer.Say(Outpost::OverseerHelloMessage{.token = token});
      (void)overseer.Next<Outpost::OverseerWelcomeMessage>();
      overseer.Say(_action);
      return overseer.CloseCode();
    };
    const std::uint64_t malformed = static_cast<std::uint64_t>(Outpost::OverseerCloseReason::MalformedMessage);
    Assert::AreEqual(malformed, refused({.action = Outpost::OverseerAction::Stop, .seat = BLUE}));
    Assert::AreEqual(malformed, refused({.action = Outpost::OverseerAction::DropLink, .seat = RED}));
    Assert::AreEqual(malformed, refused({.action = Outpost::OverseerAction::NewToken}));
    Assert::IsFalse(universe.server->StopAsked());
  }

  // Closing says how the server ends, with a failure's message and its last save's tick, and the channel then closes; and the
  // welcome of a universe made afresh from its folder says what it came back from.
  TEST_METHOD(SaysHowTheServerEnds)
  {
    const PackagedHomeDirectory home;
    const TemporaryFolder folder;
    const Outpost::SeatToken token = Outpost::NewSeatToken();
    {
      Universe universe = OverseenUniverse(folder.Path(), token);
      for (std::uint32_t tick = 0; tick < 61 * universe.server->TicksPerSecond(); ++tick)
        universe.server->Step();
      Overseer overseer(universe.overseer);
      overseer.Say(Outpost::OverseerHelloMessage{.token = token});
      (void)overseer.Next<Outpost::OverseerWelcomeMessage>();
      universe.server->Close(Outpost::ServerEnding::Failed, "the server's thread failed");
      const auto closing = overseer.Next<Outpost::ClosingMessage>();
      Assert::IsTrue(closing.ending == Outpost::ServerEnding::Failed);
      Assert::AreEqual(std::string("the server's thread failed"), closing.messageUtf8);
      Assert::AreEqual(std::uint64_t{60} * universe.server->TicksPerSecond(), closing.lastSaveTick, L"a save a minute");
      Assert::AreEqual(static_cast<std::uint64_t>(Outpost::OverseerCloseReason::Finished), overseer.CloseCode());
    }
    Universe universe = OverseenUniverse(folder.Path(), token);
    Overseer overseer(universe.overseer);
    overseer.Say(Outpost::OverseerHelloMessage{.token = token});
    const auto welcome = overseer.Next<Outpost::OverseerWelcomeMessage>();
    Assert::AreEqual(std::uint64_t{60} * universe.server->TicksPerSecond(), welcome.recoveredSaveTick.value_or(0),
                     L"it came back from its last save");
    Assert::AreEqual(std::uint64_t{0}, welcome.replayedCommands, L"its idle players logged no command");
    universe.server->Close(Outpost::ServerEnding::StoppedByOwner, {});
    Assert::IsTrue(overseer.Next<Outpost::ClosingMessage>().ending == Outpost::ServerEnding::StoppedByOwner);
  }
};
} // namespace GameLogicTests

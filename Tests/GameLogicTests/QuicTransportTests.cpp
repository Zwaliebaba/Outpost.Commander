#include "pch.h"
#include "ImpairedLink.h"
#include "RepositoryData.h"
#include "UniverseMatch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace std::chrono_literals;

namespace GameLogicTests
{
namespace
{
constexpr Outpost::PlayerId BLUE{1};
constexpr Outpost::PlayerId RED{2};
constexpr Outpost::DesignId SWARM{1};
constexpr Outpost::ShipMovement SMALL_ION{.speedMetersPerSecond = 78.0f, .turnRateRadiansPerSecond = 3.927f, .radiusMeters = 8.0f};
// A tier 1 topic of a minute, which changes the hulls when it is done.
constexpr Outpost::ResearchTopicId HULL_PLATING{2};

std::unique_ptr<Outpost::InProcessServer> QuicServer(const Outpost::ServerDesc& _desc = {.seed = 5, .quic = true})
{
  return std::make_unique<Outpost::InProcessServer>(Outpost::LoadTuning(ReadRepositoryTuning()), Outpost::LoadMap(ReadRepositoryMap()),
                                                    _desc);
}

// A universe's server in _folder, set up as the server's factory sets one up, with its bases placed (ADR-077, ADR-078).
std::unique_ptr<Outpost::InProcessServer> UniverseServer(const std::filesystem::path& _folder)
{
  auto server = QuicServer({.seed = 5, .quic = true, .universe = _folder});
  server->SimulationState().PlaceStartingBases(server->MapData());
  server->UseUniverse(_folder, UniverseMatch::RepositoryDataHash(), Outpost::UniverseFolder::Recover(_folder));
  return server;
}

// Up to _count snapshots from _player's connection, waiting at most 30 seconds for them.
std::vector<Outpost::Snapshot> SnapshotsOf(Outpost::Transport& _player, std::size_t _count)
{
  std::vector<Outpost::Snapshot> snapshots;
  const auto deadline = std::chrono::steady_clock::now() + 30s;
  while (snapshots.size() < _count && std::chrono::steady_clock::now() < deadline)
  {
    std::this_thread::sleep_for(10ms);
    for (Outpost::Snapshot& snapshot : _player.Receive())
      snapshots.push_back(std::move(snapshot));
  }
  return snapshots;
}

// What _player's connection says once it has gone, waiting at most 10 seconds for it to go; empty when it has not.
std::string WhyItWent(Outpost::Transport& _player)
{
  const auto deadline = std::chrono::steady_clock::now() + 10s;
  while (std::chrono::steady_clock::now() < deadline)
  {
    try
    {
      (void)_player.Receive();
      std::this_thread::sleep_for(10ms);
    }
    catch (const Neuron::Exception& exception)
    {
      return exception.what();
    }
  }
  return {};
}

std::wstring Wide(std::string_view _text)
{
  return {_text.begin(), _text.end()};
}

// A snapshot as the bytes of a state that carries it whole.
std::vector<std::byte> StateBytes(const Outpost::Snapshot& _snapshot)
{
  return Outpost::EncodeMessage(Outpost::StateMessage{.snapshot = _snapshot});
}

// The next snapshot _player's connection hands on, waiting at most 30 seconds for it, which is the only one it has.
Outpost::Snapshot NextSnapshotOf(Outpost::Transport& _player)
{
  std::vector<Outpost::Snapshot> snapshots;
  const auto deadline = std::chrono::steady_clock::now() + 30s;
  while (snapshots.empty() && std::chrono::steady_clock::now() < deadline)
  {
    snapshots = _player.Receive();
    if (snapshots.empty())
      std::this_thread::sleep_for(1ms);
  }
  Assert::AreEqual(size_t{1}, snapshots.size(), L"one snapshot, in time");
  return std::move(snapshots.front());
}

// A channel of the engine's own to _seat's server, which says nothing until a test has it say something.
std::unique_ptr<Neuron::QuicChannel> ChannelTo(const Outpost::ServerAddress& _seat)
{
  return Neuron::QuicChannel::Connect({.host = _seat.host,
                                       .port = _seat.port,
                                       .applicationProtocol = std::string(Outpost::QUIC_APPLICATION_PROTOCOL),
                                       .serverCertificate = _seat.certificate});
}

// The error code the server closes _channel with, or 0 when it has not closed it within _wait.
std::uint64_t CloseCodeOf(Neuron::QuicChannel& _channel, std::chrono::milliseconds _wait = 10s)
{
  const auto deadline = std::chrono::steady_clock::now() + _wait;
  while (std::chrono::steady_clock::now() < deadline)
  {
    try
    {
      (void)_channel.Receive(100ms);
    }
    catch (const Neuron::Exception&)
    {
      return _channel.PeerErrorCode().value_or(0);
    }
  }
  return 0;
}

// The error code the server closes a connection to _seat with once it is sent _hello, or 0 when it has not closed it within
// ten seconds.
std::uint64_t CloseCodeFor(const Outpost::ServerAddress& _seat, std::span<const std::byte> _hello)
{
  const std::unique_ptr<Neuron::QuicChannel> channel = ChannelTo(_seat);
  channel->Send(_hello);
  return CloseCodeOf(*channel);
}

// Whether the server's listener comes to hold _count connections within ten seconds, letting go of the ones that have gone
// as it ticks (ADR-060 decision 7).
bool ConnectionsComeTo(const Outpost::InProcessServer& _server, std::size_t _count)
{
  const auto deadline = std::chrono::steady_clock::now() + 10s;
  while (_server.ListenerConnections() != _count && std::chrono::steady_clock::now() < deadline)
    std::this_thread::sleep_for(10ms);
  return _server.ListenerConnections() == _count;
}

// A hosted player that counts how often it was given the seat to play, and to watch.
class Counter final : public Outpost::HostedPlayer
{
public:
  [[nodiscard]] std::vector<Outpost::Command> Play(const Outpost::Snapshot& _snapshot) override
  {
    ++plays;
    lastPlayed = _snapshot.tick;
    return {};
  }

  void Watch([[maybe_unused]] const Outpost::Snapshot& _snapshot) override
  {
    ++watches;
  }

  std::uint64_t plays = 0;
  std::uint64_t watches = 0;
  std::uint64_t lastPlayed = 0;
};
} // namespace

// The server and its clients over a real QUIC connection on the loopback address, through MsQuic (ADR-060).
TEST_CLASS(QuicTransportTests)
{
public:
  TEST_METHOD(PlaysOverQuic)
  {
    auto server = QuicServer();
    const Outpost::EntityId ship = server->SimulationState().SpawnShip(BLUE, SWARM, SMALL_ION, {});
    Outpost::QuicTransport blue(server->OpenSeat(BLUE, Outpost::NewSeatToken()), BLUE);
    Outpost::QuicTransport red(server->OpenSeat(RED, Outpost::NewSeatToken()), RED);
    server->Start();
    Assert::ExpectException<Neuron::Exception>([&server] { (void)server->OpenSeat(Outpost::PlayerId{3}, Outpost::NewSeatToken()); });

    // Orders sent while the server ticks, the last of them sending the ship on its way.
    for (int order = 0; order < 20; ++order)
    {
      blue.Send({.order = Outpost::StopCommand{.ships = {ship}}});
      std::this_thread::sleep_for(1ms);
    }
    blue.Send({.order = Outpost::MoveCommand{.ships = {ship}, .destination = {.xMeters = 300.0f}}});
    (void)blue.Receive();
    std::vector<Outpost::Snapshot> snapshots;
    // Ten ticks take half a second; the deadline only keeps a stalled server from hanging the test.
    const auto deadline = std::chrono::steady_clock::now() + 30s;
    while (snapshots.size() < 10 && std::chrono::steady_clock::now() < deadline)
    {
      std::this_thread::sleep_for(10ms);
      for (Outpost::Snapshot& snapshot : blue.Receive())
        snapshots.push_back(std::move(snapshot));
    }
    Assert::IsTrue(snapshots.size() >= 10, L"snapshots arrived over QUIC");
    for (size_t i = 1; i < snapshots.size(); ++i)
      Assert::AreEqual(snapshots[i - 1].tick + 1, snapshots[i].tick, L"a snapshot for every tick, in order");
    Assert::IsTrue(snapshots.back().player == BLUE);
    const auto moved = std::ranges::find(snapshots.back().entities, ship, &Outpost::EntityView::id);
    Assert::IsTrue(moved != snapshots.back().entities.end() && moved->position.xMeters > 0.0f, L"the order reached the server");
    const std::vector<Outpost::Snapshot> redSnapshots = red.Receive();
    Assert::IsTrue(!redSnapshots.empty() && redSnapshots.back().player == RED, L"each player its own snapshots");

    // Once the server is gone, the client says so rather than waiting for snapshots that will not come.
    server.reset();
    bool lost = false;
    const auto lostDeadline = std::chrono::steady_clock::now() + 10s;
    while (!lost && std::chrono::steady_clock::now() < lostDeadline)
    {
      try
      {
        (void)blue.Receive();
        std::this_thread::sleep_for(10ms);
      }
      catch (const Neuron::Exception&)
      {
        lost = true;
      }
    }
    Assert::IsTrue(lost, L"the client learned the server had gone");
  }

  // ADR-078: a seat is taken only with its token, and a client says why the server refused it.
  TEST_METHOD(RefusesAnotherTokenAnUnopenedSeatAndAnotherCertificate)
  {
    auto server = QuicServer();
    const Outpost::ServerAddress blueSeat = server->OpenSeat(BLUE, Outpost::NewSeatToken());
    const Outpost::ServerAddress redSeat = server->OpenSeat(RED, Outpost::NewSeatToken());
    Outpost::QuicTransport blue(blueSeat, BLUE);

    Outpost::ServerAddress guessed = redSeat;
    guessed.token[0] ^= 0x01;
    std::string why;
    try
    {
      Outpost::QuicTransport refused(guessed, RED);
    }
    catch (const Neuron::Exception& exception)
    {
      why = exception.what();
    }
    Assert::AreEqual(std::string(Outpost::DescribeClose(static_cast<std::uint64_t>(Outpost::CloseReason::SeatRefused)).value_or("")), why,
                     L"another token is refused, and the client says why");
    Assert::ExpectException<Neuron::Exception>([&redSeat] { Outpost::QuicTransport unopened(redSeat, Outpost::PlayerId{3}); });
    Outpost::ServerAddress impostor = redSeat;
    impostor.certificate[0] ^= 0xFF;
    Assert::ExpectException<Neuron::Exception>([&impostor] { Outpost::QuicTransport pinned(impostor, RED); });

    // Red's seat is still open, so the match cannot start; once red takes it, it can, and then no seat is taken again.
    Assert::ExpectException<Neuron::Exception>([&server] { server->Start(); });
    Outpost::QuicTransport red(redSeat, RED);
    server->Start();
    Assert::ExpectException<Neuron::Exception>([&redSeat] { Outpost::QuicTransport again(redSeat, RED); },
                                               L"a match's seats close as it starts");
  }

  // ADR-078: a universe starts with its seats open, and its players take them while it runs; the newest connection with a
  // seat's token holds it, and the one it is taken from is told so and sends nothing more.
  TEST_METHOD(AUniversesSeatsAreTakenWhileItRunsAndTakenAgain)
  {
    const TemporaryFolder folder;
    auto server = UniverseServer(folder.Path());
    const Outpost::ServerAddress blueSeat = server->OpenSeat(BLUE, Outpost::NewSeatToken());
    (void)server->OpenSeat(RED, Outpost::NewSeatToken());
    server->Start();

    Outpost::QuicTransport first(blueSeat, BLUE);
    Assert::IsTrue(SnapshotsOf(first, 3).size() >= 3, L"a seat taken after the universe started");
    Outpost::QuicTransport second(blueSeat, BLUE);
    Assert::AreEqual(std::string(Outpost::DescribeClose(static_cast<std::uint64_t>(Outpost::CloseReason::SeatTaken)).value_or("")),
                     WhyItWent(first), L"the connection the seat was taken from is told why it went");
    const std::vector<Outpost::Snapshot> snapshots = SnapshotsOf(second, 3);
    Assert::IsTrue(snapshots.size() >= 3 && snapshots.back().player == BLUE, L"the newest connection holds the seat");
    Outpost::ServerAddress guessed = blueSeat;
    guessed.token.back() ^= 0x80;
    Assert::ExpectException<Neuron::Exception>([&guessed] { Outpost::QuicTransport refused(guessed, BLUE); }, L"but not without its token");
    Assert::IsTrue(!SnapshotsOf(second, 1).empty(), L"and a refused one takes nothing from it");
  }

  // ADR-078 decision 10: a universe's client whose link has carried nothing for 10 s joins again with its seat's address and
  // token and takes its seat back, and the server closes the connection it took it from with SeatTaken. A connection the
  // server closed as SeatTaken, SeatRefused or WrongVersion is not joined again, and one that went any other way is.
  TEST_METHOD(TakesItsSeatAgainAfterItsLinkWent)
  {
    const TemporaryFolder folder;
    auto server = UniverseServer(folder.Path());
    const Outpost::ServerAddress seat = server->OpenSeat(BLUE, Outpost::NewSeatToken());
    (void)server->OpenSeat(RED, Outpost::NewSeatToken());
    server->Start();
    ImpairedLink link(seat.port, CLEAR_LINK, 1);
    Outpost::QuicTransport first(link.Through(seat), BLUE);
    Assert::IsFalse(SnapshotsOf(first, 3).empty(), L"the seat taken through the link");

    // What crossed before the cut is taken, and then nothing comes for 10 s.
    link.Cut(true);
    std::this_thread::sleep_for(100ms);
    (void)first.Receive();
    std::this_thread::sleep_for(10s);
    Assert::IsTrue(first.Receive().empty(), L"nothing for 10 s");
    // A relay carries one client, so the join comes through a link of its own; the first link carries again, so that the
    // server's close reaches the connection the seat was taken from.
    const ImpairedLink rejoined(seat.port, CLEAR_LINK, 2);
    Outpost::QuicTransport again(rejoined.Through(seat), BLUE);
    link.Cut(false);
    const std::vector<Outpost::Snapshot> snapshots = SnapshotsOf(again, 3);
    Assert::IsTrue(snapshots.size() >= 3 && snapshots.back().player == BLUE, L"the seat taken again");

    std::string why;
    bool mayJoinAgain = true;
    const auto deadline = std::chrono::steady_clock::now() + 30s;
    while (why.empty() && std::chrono::steady_clock::now() < deadline)
    {
      try
      {
        (void)first.Receive();
        std::this_thread::sleep_for(10ms);
      }
      catch (const Neuron::Exception& error)
      {
        why = error.what();
        mayJoinAgain = Outpost::MayJoinAgain(error);
      }
    }
    Assert::AreEqual(std::string(Outpost::DescribeClose(static_cast<std::uint64_t>(Outpost::CloseReason::SeatTaken)).value_or("")), why,
                     L"the connection it was taken from is told so");
    Assert::IsFalse(mayJoinAgain, L"and does not join again");

    for (const Outpost::CloseReason reason : {Outpost::CloseReason::Finished, Outpost::CloseReason::WrongVersion,
                                              Outpost::CloseReason::SeatRefused, Outpost::CloseReason::SeatTaken})
      Assert::IsFalse(Outpost::MayJoinAgain(Outpost::ServerClosed(reason, "closed")));
    Assert::IsTrue(Outpost::MayJoinAgain(Outpost::ServerClosed(Outpost::CloseReason::MalformedMessage, "closed")));
    Assert::IsTrue(Outpost::MayJoinAgain(Neuron::Exception("The QUIC connection went.")), L"a link that went");
    Outpost::ServerAddress guessed = seat;
    guessed.token[0] ^= 0x01;
    try
    {
      Outpost::QuicTransport refused(guessed, BLUE);
      Assert::Fail(L"another token is refused");
    }
    catch (const Neuron::Exception& error)
    {
      Assert::IsFalse(Outpost::MayJoinAgain(error), L"and a refused join is not tried again");
    }
  }

  // ADR-079 decision 4: a seat's deputy plays it until its player takes it, watches while the player plays,
  // takes it again a minute of ticks after the player's connection has gone, and gives it back when the player returns. The
  // player's first join carries no report of the time before, and its return does (ADR-080 decision 7).
  TEST_METHOD(ADeputyPlaysItsSeatWhileItsPlayerIsAway)
  {
    const TemporaryFolder folder;
    auto server = UniverseServer(folder.Path());
    const Outpost::ServerAddress blueSeat = server->OpenSeat(BLUE, Outpost::NewSeatToken());
    auto counter = std::make_unique<Counter>();
    const Counter& deputy = *counter;
    server->Host(BLUE, std::move(counter));
    server->Step();
    Assert::AreEqual(std::uint64_t{1}, deputy.plays, L"a seat nobody has taken is the deputy's");

    {
      Outpost::QuicTransport blue(blueSeat, BLUE);
      server->Step();
      Assert::AreEqual(std::uint64_t{1}, deputy.watches, L"the player plays the seat it took");
      // The seat was never the player's before, so its first snapshot reports nothing of the tick the deputy played.
      const std::vector<Outpost::Snapshot> first = SnapshotsOf(blue, 1);
      Assert::IsTrue(!first.empty() && !first.front().away.has_value(), L"its first join carries no report");
    }
    // The connection has gone; the server hears so on MsQuic's thread.
    std::this_thread::sleep_for(1s);
    const std::uint64_t ticksPerMinute = std::uint64_t{Outpost::DEPUTY_DELAY_SECONDS} * server->TicksPerSecond();
    for (std::uint64_t tick = 0; tick < ticksPerMinute; ++tick)
      server->Step();
    Assert::AreEqual(std::uint64_t{1}, deputy.plays, L"for a minute after the connection went, the seat is still the player's");
    server->Step();
    Assert::AreEqual(std::uint64_t{2}, deputy.plays, L"then the deputy's");
    const std::uint64_t tookOver = deputy.lastPlayed;

    Outpost::QuicTransport again(blueSeat, BLUE);
    const std::uint64_t watched = deputy.watches;
    server->Step();
    Assert::AreEqual(std::uint64_t{2}, deputy.plays);
    Assert::AreEqual(watched + 1, deputy.watches, L"until the player takes it again");
    const std::vector<Outpost::Snapshot> back = SnapshotsOf(again, 1);
    Assert::IsTrue(!back.empty() && back.front().away.value_or(Outpost::AwayReport{.sinceTick = tookOver + 1}).sinceTick == tookOver,
                   L"and the player is told what happened from the tick the deputy took the seat");
    server->Step();
    const std::vector<Outpost::Snapshot> next = SnapshotsOf(again, 1);
    Assert::IsTrue(!next.empty() && !next.back().away.has_value(), L"once");
  }

  // ADR-078: a universe's server keeps its certificate in the universe's folder, so that a player pins it once.
  TEST_METHOD(AUniverseKeepsItsCertificate)
  {
    const TemporaryFolder folder;
    Neuron::CertificateHash first{};
    {
      auto server = UniverseServer(folder.Path());
      first = server->OpenSeat(BLUE, Outpost::NewSeatToken()).certificate;
    }
    auto again = UniverseServer(folder.Path());
    const Outpost::ServerAddress blueSeat = again->OpenSeat(BLUE, Outpost::NewSeatToken());
    Assert::IsTrue(blueSeat.certificate == first, L"the same certificate when the universe runs again");
    again->Start();
    Outpost::QuicTransport blue(blueSeat, BLUE);
    Assert::IsTrue(!SnapshotsOf(blue, 1).empty(), L"and a client that pinned it plays");

    const TemporaryFolder other;
    auto otherUniverse = UniverseServer(other.Path());
    Assert::IsFalse(otherUniverse->OpenSeat(BLUE, Outpost::NewSeatToken()).certificate == first, L"another universe has its own");
    auto match = QuicServer();
    Assert::IsFalse(match->OpenSeat(BLUE, Outpost::NewSeatToken()).certificate == first, L"and a match its own");
  }

  // ADR-078: a server listens on the address it is given, and refuses one that is not an address.
  TEST_METHOD(ListensOnTheAddressItIsGiven)
  {
    auto server = QuicServer({.seed = 5, .quic = true, .quicAddress = "127.0.0.1"});
    const Outpost::ServerAddress blueSeat = server->OpenSeat(BLUE, Outpost::NewSeatToken());
    Assert::AreEqual(std::string("127.0.0.1"), blueSeat.host);
    Assert::IsTrue(blueSeat.port != 0);
    Outpost::QuicTransport blue(blueSeat, BLUE);
    Assert::ExpectException<Neuron::Exception>([] { (void)QuicServer({.seed = 5, .quic = true, .quicAddress = "not an address"}); });

    // On every address of both families, a player on the same machine is sent to the loopback.
    auto everywhere = QuicServer({.seed = 5, .quic = true, .quicAddress = std::string(Neuron::QuicListener::EVERY_ADDRESS)});
    const Outpost::ServerAddress redSeat = everywhere->OpenSeat(RED, Outpost::NewSeatToken());
    Assert::AreEqual(std::string("127.0.0.1"), redSeat.host);
    Outpost::QuicTransport red(redSeat, RED);
  }

  // ADR-060: a hello of another version is refused as one, even when its fields are not this version's.
  TEST_METHOD(RefusesAHelloOfAnotherVersion)
  {
    auto server = QuicServer();
    const Outpost::ServerAddress seat = server->OpenSeat(BLUE, Outpost::NewSeatToken());
    // A hello of another version, 10, laid out as it was before the token: the kind, the version and the player.
    const std::vector<std::byte> oldHello{std::byte{0}, std::byte{10}, std::byte{0}, std::byte{0}, std::byte{0},
                                          std::byte{1}, std::byte{0},  std::byte{0}, std::byte{0}};
    const std::uint64_t code = CloseCodeFor(seat, oldHello);
    Assert::AreEqual(static_cast<std::uint64_t>(Outpost::CloseReason::WrongVersion), code,
                     Wide(std::format("closed with code {}", code)).c_str());
  }

  // ADR-060: a hello of this version from a build that lays its messages out otherwise is refused as another build's, and
  // so is one cut short before its layout's hash ends; this build's own hello takes the seat.
  TEST_METHOD(RefusesAHelloOfAnotherLayout)
  {
    auto server = QuicServer();
    const Outpost::ServerAddress seat = server->OpenSeat(BLUE, Outpost::NewSeatToken());
    const std::vector<std::byte> otherLayout =
      Outpost::EncodeMessage(Outpost::HelloMessage{.layoutHash = Outpost::WireLayoutHash() + 1, .player = BLUE, .token = seat.token});
    std::uint64_t code = CloseCodeFor(seat, otherLayout);
    Assert::AreEqual(static_cast<std::uint64_t>(Outpost::CloseReason::WrongVersion), code,
                     Wide(std::format("closed with code {}", code)).c_str());

    std::vector<std::byte> cutShort = Outpost::EncodeMessage(Outpost::HelloMessage{.player = BLUE, .token = seat.token});
    cutShort.resize(1 + sizeof(std::uint32_t) + 4);
    code = CloseCodeFor(seat, cutShort);
    Assert::AreEqual(static_cast<std::uint64_t>(Outpost::CloseReason::WrongVersion), code,
                     Wide(std::format("closed with code {}", code)).c_str());

    Outpost::QuicTransport blue(seat, BLUE);
  }

  TEST_METHOD(OpensSeatsOnlyWhenItListens)
  {
    Outpost::InProcessServer server(Outpost::LoadTuning(ReadRepositoryTuning()), Outpost::LoadMap(ReadRepositoryMap()), {.seed = 1});
    Assert::ExpectException<Neuron::Exception>([&server] { (void)server.OpenSeat(BLUE, Outpost::NewSeatToken()); });

    auto listening = QuicServer();
    const std::unique_ptr<Outpost::Transport> loopback = listening->Connect(BLUE);
    Assert::ExpectException<Neuron::Exception>([&listening] { (void)listening->OpenSeat(BLUE, Outpost::NewSeatToken()); });
  }

  // ADR-078 decision 9: a join gives the handshake the time it is given, as the game's join at launch gives it 3 s, and
  // gives up on a server that has gone well before the 10 s a join from the menu waits.
  TEST_METHOD(GivesUpAJoinInTheTimeItIsGiven)
  {
    auto gone = QuicServer();
    const Outpost::ServerAddress seat = gone->OpenSeat(BLUE, Outpost::NewSeatToken());
    gone.reset();
    const auto started = std::chrono::steady_clock::now();
    std::string message;
    try
    {
      (void)Outpost::QuicTransport(seat, BLUE, 1'000ms);
    }
    catch (const Neuron::Exception& error)
    {
      message = error.what();
    }
    Assert::IsTrue(message.starts_with(std::format("Could not connect to {}:{} over QUIC", seat.host, seat.port)),
                   std::wstring(winrt::to_hstring(message)).c_str());
    Assert::IsTrue(std::chrono::steady_clock::now() - started < 5s, L"it gave up within its time");
  }

  // ADR-060 decision 7: the listener lets go of a connection once it has gone and no seat holds it, whether its client
  // closed it, the server refused it or a newer connection took its seat, and keeps the ones that are still there.
  TEST_METHOD(LetsGoOfConnectionsThatHaveGone)
  {
    const TemporaryFolder folder;
    auto server = UniverseServer(folder.Path());
    const Outpost::ServerAddress seat = server->OpenSeat(BLUE, Outpost::NewSeatToken());
    Outpost::ServerAddress guessed = seat;
    guessed.token[0] ^= 0x01;
    server->Start();

    for (int attempt = 0; attempt < 50; ++attempt)
      Assert::ExpectException<Neuron::Exception>([&guessed] { Outpost::QuicTransport refused(guessed, BLUE); });
    for (int visit = 0; visit < 50; ++visit)
    {
      // Each takes the seat from the one before, and closes as it goes.
      const Outpost::QuicTransport visitor(seat, BLUE);
    }
    Assert::IsTrue(ConnectionsComeTo(*server, 1), L"only the last visit's, which the seat still holds");

    Outpost::QuicTransport first(seat, BLUE);
    Outpost::QuicTransport second(seat, BLUE);
    Assert::IsTrue(ConnectionsComeTo(*server, 1), L"only the newest, though the one it took the seat from has a client still");
    Assert::IsFalse(SnapshotsOf(second, 3).empty(), L"which plays on");
  }

  // ADR-060 decision 2: a client may send no more than a hello's worth until its hello has taken a seat.
  TEST_METHOD(RefusesAFirstMessageLongerThanAHello)
  {
    auto server = QuicServer();
    const Outpost::ServerAddress seat = server->OpenSeat(BLUE, Outpost::NewSeatToken());
    Assert::IsTrue(Outpost::EncodeMessage(Outpost::HelloMessage{.player = BLUE, .token = seat.token}).size() <= Outpost::HELLO_LIMIT_BYTES);
    const std::vector<std::byte> longer(Outpost::HELLO_LIMIT_BYTES + 1);
    const std::uint64_t code = CloseCodeFor(seat, longer);
    Assert::AreEqual(Neuron::FRAMING_ERROR_CODE, code, Wide(std::format("closed with code {}", code)).c_str());
    Outpost::QuicTransport blue(seat, BLUE);
  }

  // ADR-060 decision 5: a client that has not said hello HELLO_TIMEOUT after its handshake is closed, and one that said it
  // plays on.
  TEST_METHOD(ClosesAConnectionThatSaysNoHello)
  {
    const TemporaryFolder folder;
    auto server = UniverseServer(folder.Path());
    const Outpost::ServerAddress seat = server->OpenSeat(BLUE, Outpost::NewSeatToken());
    server->Start();
    const auto connected = std::chrono::steady_clock::now();
    const std::unique_ptr<Neuron::QuicChannel> silent = ChannelTo(seat);
    Outpost::QuicTransport blue(seat, BLUE);

    const std::uint64_t code = CloseCodeOf(*silent, 15s);
    Assert::AreEqual(static_cast<std::uint64_t>(Outpost::CloseReason::MalformedMessage), code,
                     Wide(std::format("closed with code {}", code)).c_str());
    Assert::IsTrue(std::chrono::steady_clock::now() - connected > Outpost::HELLO_TIMEOUT - 1s, L"not before its time");
    Assert::IsFalse(SnapshotsOf(blue, 3).empty(), L"the client that said hello plays on");
  }

  // ADR-009 decision 6 and ADR-060 decision 2: a seated client with more commands waiting than COMMANDS_WAITING_LIMIT is
  // closed, and so is one that sends a message longer than a seated client's limit.
  TEST_METHOD(ClosesASeatThatFloodsIt)
  {
    auto server = QuicServer();
    const Outpost::EntityId ship = server->SimulationState().SpawnShip(BLUE, SWARM, SMALL_ION, {});
    Outpost::QuicTransport blue(server->OpenSeat(BLUE, Outpost::NewSeatToken()), BLUE);
    // A match's server starts once its seats are taken, so nothing takes these from the queue.
    for (std::size_t order = 0; order <= Outpost::COMMANDS_WAITING_LIMIT; ++order)
      blue.Send({.order = Outpost::StopCommand{.ships = {ship}}});
    Assert::AreEqual(std::string(Outpost::DescribeClose(static_cast<std::uint64_t>(Outpost::CloseReason::MalformedMessage)).value_or("")),
                     WhyItWent(blue), L"closed, and told why");

    const Outpost::ServerAddress seat = server->OpenSeat(RED, Outpost::NewSeatToken());
    const std::unique_ptr<Neuron::QuicChannel> red = ChannelTo(seat);
    red->Send(Outpost::EncodeMessage(Outpost::HelloMessage{.player = RED, .token = seat.token}));
    std::vector<std::vector<std::byte>> welcome;
    const auto deadline = std::chrono::steady_clock::now() + 10s;
    while (welcome.empty() && std::chrono::steady_clock::now() < deadline)
      welcome = red->Receive(100ms);
    Assert::IsFalse(welcome.empty(), L"seated");
    const std::vector<Outpost::EntityId> ships(Outpost::SEATED_MESSAGE_LIMIT_BYTES / sizeof(std::uint32_t), ship);
    red->Send(Outpost::EncodeMessage(Outpost::Command{.order = Outpost::StopCommand{.ships = ships}}));
    const std::uint64_t code = CloseCodeOf(*red);
    Assert::AreEqual(Neuron::FRAMING_ERROR_CODE, code, Wide(std::format("closed with code {}", code)).c_str());
  }

  // ADR-004 decision 6: the server takes a seat's connection once a second, after the tick: a round trip once a snapshot has
  // crossed, the packets sent counting up, and a backlog that comes back to nothing once the client has every snapshot. A
  // player on the loopback has none.
  TEST_METHOD(ReportsItsConnection)
  {
    auto server = QuicServer();
    Outpost::QuicTransport blue(server->OpenSeat(BLUE, Outpost::NewSeatToken()), BLUE);
    const std::unique_ptr<Outpost::Transport> red = server->Connect(RED);
    const std::uint32_t second = server->TicksPerSecond();
    std::vector<Outpost::SeatFigures> figures;
    // A second of ticks at a time, each snapshot read before the next tick, and a pause before the second's last, the one
    // sampled, for the client to acknowledge what it has read; until a sample finds nothing waiting, or ten seconds of them.
    for (int seconds = 0; seconds < 10 && (figures.size() < 2 || figures.back().connection.backlogBytes != 0); ++seconds)
    {
      for (std::uint32_t tick = 1; tick < second; ++tick)
      {
        server->Step();
        Assert::IsFalse(SnapshotsOf(blue, 1).empty(), L"each tick's snapshot");
      }
      std::this_thread::sleep_for(200ms);
      server->Step();
      (void)SnapshotsOf(blue, 1);
      for (const Outpost::SeatFigures& sample : server->TakeSeatFigures())
        figures.push_back(sample);
    }

    Assert::IsTrue(figures.size() >= 2, L"a sample a second");
    for (const Outpost::SeatFigures& sample : figures)
    {
      Assert::IsTrue(sample.player == BLUE, L"of the seat over QUIC alone");
      Assert::AreEqual(std::uint64_t{0}, sample.tick % second, L"after a second's last tick");
    }
    Assert::IsTrue(figures.front().connection.roundTripMicroseconds > 0, L"a round trip, once a snapshot has crossed");
    Assert::IsTrue(figures.back().connection.packetsSent > figures.front().connection.packetsSent, L"the packets sent count up");
    Assert::AreEqual(std::uint64_t{0}, figures.back().connection.backlogBytes, L"and nothing waits once the client has every snapshot");
    Assert::IsTrue(server->TakeSeatFigures().empty(), L"each sample is taken once");
  }

  // ADR-060 decision 9, ADR-096, ADR-098: the snapshots a seat's client hands on are the ones the server built, tick by tick,
  // for two minutes in which the player's research finishes and changes the catalogue, their events whichever snapshot
  // brings them and their states whole or as changes; and a seat taken again is sent its catalogue before its first state.
  TEST_METHOD(HandsOnTheSnapshotsTheServerBuilt)
  {
    auto server = QuicServer();
    Outpost::Simulation& simulation = server->SimulationState();
    simulation.PlaceStartingBases(server->MapData());
    const Outpost::StructureTuning& labTuning =
      *std::ranges::find(server->TuningData().structures, Outpost::StructureKind::ResearchLab, &Outpost::StructureTuning::kind);
    const Outpost::PlanePosition start = server->MapData().starts.front();
    const Outpost::EntityId lab =
      simulation.SpawnStructure(BLUE, Outpost::StructureKind::ResearchLab, {.xMeters = start.xMeters + 250.0f, .zMeters = start.zMeters},
                                static_cast<float>(labTuning.footprintRadiusMeters), simulation.StructureHitPoints(BLUE, labTuning, 1));
    const Outpost::ServerAddress seat = server->OpenSeat(BLUE, Outpost::NewSeatToken());
    Outpost::QuicTransport blue(seat, BLUE);
    const std::unique_ptr<Outpost::Transport> red = server->Connect(RED);
    blue.Send({.order = Outpost::StartResearchCommand{.lab = lab, .topic = HULL_PLATING}});

    // Tick by tick, so that no state is overtaken by the next (ADR-096).
    const std::uint32_t ticks = 120 * server->TicksPerSecond();
    for (std::uint32_t tick = 0; tick < ticks; ++tick)
    {
      server->Step();
      (void)red->Receive();
      const Outpost::Snapshot built = simulation.BuildSnapshot(BLUE);
      Assert::IsTrue(StateBytes(NextSnapshotOf(blue)) == StateBytes(built), Wide(std::format("tick {}", built.tick)).c_str());
    }
    const Outpost::Snapshot last = simulation.BuildSnapshot(BLUE);
    const auto plating = std::ranges::find(last.research, HULL_PLATING, &Outpost::ResearchTopicView::id);
    Assert::IsTrue(plating != last.research.end() && plating->researched, L"the research finished in the two minutes");

    Outpost::QuicTransport again(seat, BLUE);
    server->Step();
    Assert::IsTrue(StateBytes(NextSnapshotOf(again)) == StateBytes(simulation.BuildSnapshot(BLUE)),
                   L"the seat taken again is sent its catalogue before its first state");
  }
};
} // namespace GameLogicTests

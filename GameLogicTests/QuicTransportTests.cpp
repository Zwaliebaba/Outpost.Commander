#include "pch.h"
#include "RepositoryData.h"
#include "WorldMatch.h"

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

std::unique_ptr<Outpost::InProcessServer> QuicServer(const Outpost::ServerDesc& _desc = {.seed = 5, .quic = true})
{
  return std::make_unique<Outpost::InProcessServer>(Outpost::LoadTuning(ReadRepositoryTuning()), Outpost::LoadMap(ReadRepositoryMap()),
                                                    _desc);
}

// A world's server in _folder, set up as the server's factory sets one up, with its bases placed (ADR-077, ADR-078).
std::unique_ptr<Outpost::InProcessServer> WorldServer(const std::filesystem::path& _folder)
{
  auto server = QuicServer({.seed = 5, .quic = true, .world = _folder});
  server->World().PlaceStartingBases(server->MapData());
  server->UseWorld(_folder, WorldMatch::RepositoryDataHash(), Outpost::WorldFolder::Recover(_folder));
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

// The error code the server closes a connection to _seat with once it is sent _hello, or 0 when it has not closed it within
// ten seconds.
std::uint64_t CloseCodeFor(const Outpost::ServerAddress& _seat, std::span<const std::byte> _hello)
{
  const std::unique_ptr<Neuron::QuicChannel> channel =
    Neuron::QuicChannel::Connect({.host = _seat.host,
                                  .port = _seat.port,
                                  .applicationProtocol = std::string(Outpost::QUIC_APPLICATION_PROTOCOL),
                                  .serverCertificate = _seat.certificate});
  channel->Send(_hello);
  const auto deadline = std::chrono::steady_clock::now() + 10s;
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
  return 0;
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
    const Outpost::EntityId ship = server->World().SpawnShip(BLUE, SWARM, SMALL_ION, {});
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

  // ADR-078: a world starts with its seats open, and its players take them while it runs; the newest connection with a
  // seat's token holds it, and the one it is taken from is told so and sends nothing more.
  TEST_METHOD(AWorldsSeatsAreTakenWhileItRunsAndTakenAgain)
  {
    const TemporaryFolder folder;
    auto server = WorldServer(folder.Path());
    const Outpost::ServerAddress blueSeat = server->OpenSeat(BLUE, Outpost::NewSeatToken());
    (void)server->OpenSeat(RED, Outpost::NewSeatToken());
    server->Start();

    Outpost::QuicTransport first(blueSeat, BLUE);
    Assert::IsTrue(SnapshotsOf(first, 3).size() >= 3, L"a seat taken after the world started");
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

  // Phase 5 design §6, gate H2 (ADR-079): a seat's deputy plays it until its player takes it, watches while the player plays,
  // takes it again a minute of ticks after the player's connection has gone, and gives it back when the player returns.
  TEST_METHOD(ADeputyPlaysItsSeatWhileItsPlayerIsAway)
  {
    const TemporaryFolder folder;
    auto server = WorldServer(folder.Path());
    const Outpost::ServerAddress blueSeat = server->OpenSeat(BLUE, Outpost::NewSeatToken());
    auto counter = std::make_unique<Counter>();
    const Counter& deputy = *counter;
    server->Host(BLUE, std::move(counter));
    server->Step();
    Assert::AreEqual(std::uint64_t{1}, deputy.plays, L"a seat nobody has taken is the deputy's");
    const std::uint64_t firstPlayed = deputy.lastPlayed;

    {
      Outpost::QuicTransport blue(blueSeat, BLUE);
      server->Step();
      Assert::AreEqual(std::uint64_t{1}, deputy.watches, L"the player plays the seat it took");
      // What happened in the tick the deputy played, which the player's first snapshot tells it (design §11, ADR-080).
      const std::vector<Outpost::Snapshot> first = SnapshotsOf(blue, 1);
      Assert::IsTrue(!first.empty() &&
                       first.front().away.value_or(Outpost::AwayReport{.sinceTick = firstPlayed + 1}).sinceTick == firstPlayed,
                     L"its first snapshot tells it what happened while the deputy played");
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

  // ADR-078: a world's server keeps its certificate in the world's folder, so that a player pins it once.
  TEST_METHOD(AWorldKeepsItsCertificate)
  {
    const TemporaryFolder folder;
    Neuron::CertificateHash first{};
    {
      auto server = WorldServer(folder.Path());
      first = server->OpenSeat(BLUE, Outpost::NewSeatToken()).certificate;
    }
    auto again = WorldServer(folder.Path());
    const Outpost::ServerAddress blueSeat = again->OpenSeat(BLUE, Outpost::NewSeatToken());
    Assert::IsTrue(blueSeat.certificate == first, L"the same certificate when the world runs again");
    again->Start();
    Outpost::QuicTransport blue(blueSeat, BLUE);
    Assert::IsTrue(!SnapshotsOf(blue, 1).empty(), L"and a client that pinned it plays");

    const TemporaryFolder other;
    auto otherWorld = WorldServer(other.Path());
    Assert::IsFalse(otherWorld->OpenSeat(BLUE, Outpost::NewSeatToken()).certificate == first, L"another world has its own");
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
};
} // namespace GameLogicTests

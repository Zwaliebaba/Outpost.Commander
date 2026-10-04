#include "pch.h"
#include "RepositoryData.h"

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

std::unique_ptr<Outpost::InProcessServer> QuicServer()
{
  return std::make_unique<Outpost::InProcessServer>(Outpost::LoadTuning(ReadRepositoryTuning()), Outpost::LoadMap(ReadRepositoryMap()),
                                                    Outpost::ServerDesc{.seed = 5, .quic = true});
}
} // namespace

// The server and its clients over a real QUIC connection on the loopback address, through MsQuic (ADR-060).
TEST_CLASS(QuicTransportTests)
{
public:
  TEST_METHOD(PlaysOverQuic)
  {
    auto server = QuicServer();
    const Outpost::EntityId ship = server->World().SpawnShip(BLUE, SWARM, SMALL_ION, {});
    Outpost::QuicTransport blue(server->OpenSeat(BLUE), BLUE);
    Outpost::QuicTransport red(server->OpenSeat(RED), RED);
    server->Start();
    Assert::ExpectException<Neuron::Exception>([&server] { (void)server->OpenSeat(Outpost::PlayerId{3}); });

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

  TEST_METHOD(RefusesATakenSeatAnUnopenedOneAndAnotherCertificate)
  {
    auto server = QuicServer();
    const Outpost::ServerAddress blueSeat = server->OpenSeat(BLUE);
    const Outpost::ServerAddress redSeat = server->OpenSeat(RED);
    Outpost::QuicTransport blue(blueSeat, BLUE);

    Assert::ExpectException<Neuron::Exception>([&blueSeat] { Outpost::QuicTransport twice(blueSeat, BLUE); });
    Assert::ExpectException<Neuron::Exception>([&redSeat] { Outpost::QuicTransport unopened(redSeat, Outpost::PlayerId{3}); });
    Outpost::ServerAddress impostor = redSeat;
    impostor.certificate[0] ^= 0xFF;
    Assert::ExpectException<Neuron::Exception>([&impostor] { Outpost::QuicTransport pinned(impostor, RED); });

    // Red's seat is still open, so the match cannot start; once red takes it, it can.
    Assert::ExpectException<Neuron::Exception>([&server] { server->Start(); });
    Outpost::QuicTransport red(redSeat, RED);
    server->Start();
  }

  TEST_METHOD(OpensSeatsOnlyWhenItListens)
  {
    Outpost::InProcessServer server(Outpost::LoadTuning(ReadRepositoryTuning()), Outpost::LoadMap(ReadRepositoryMap()), {.seed = 1});
    Assert::ExpectException<Neuron::Exception>([&server] { (void)server.OpenSeat(BLUE); });

    auto listening = QuicServer();
    const std::unique_ptr<Outpost::Transport> loopback = listening->Connect(BLUE);
    Assert::ExpectException<Neuron::Exception>([&listening] { (void)listening->OpenSeat(BLUE); });
  }
};
} // namespace GameLogicTests

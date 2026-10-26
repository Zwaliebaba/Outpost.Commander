#include "pch.h"
#include "ImpairedLink.h"
#include "RepositoryData.h"
#include "UniverseMatch.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <thread>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace std::chrono_literals;

namespace GameLogicTests
{
namespace
{
using Clock = std::chrono::steady_clock;

constexpr Outpost::PlayerId BLUE{1};
constexpr Outpost::PlayerId RED{2};
constexpr Outpost::DesignId SWARM{1};
constexpr Outpost::ShipMovement SMALL_ION{.speedMetersPerSecond = 78.0f, .turnRateRadiansPerSecond = 3.927f, .radiusMeters = 8.0f};
// How long a test waits for what it expects through a link before it fails: far longer than any profile's round trips,
// and than MsQuic's idle timeout, which ends a connection whose close the link lost.
constexpr auto PATIENCE = 45s;

// A UDP socket on the loopback for the link's own tests, with Winsock started for it.
class Endpoint : Neuron::NonCopyable
{
public:
  Endpoint()
  {
    WSADATA data{};
    Assert::AreEqual(0, WSAStartup(MAKEWORD(2, 2), &data));
    m_socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    Assert::IsTrue(m_socket != INVALID_SOCKET);
    constexpr int BUFFER_BYTES = 4 * 1024 * 1024;
    (void)setsockopt(m_socket, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&BUFFER_BYTES), sizeof(BUFFER_BYTES));
    const sockaddr_in any = At(0);
    Assert::AreNotEqual(SOCKET_ERROR, bind(m_socket, reinterpret_cast<const sockaddr*>(&any), sizeof(any)));
  }

  ~Endpoint()
  {
    (void)closesocket(m_socket);
    (void)WSACleanup();
  }

  [[nodiscard]] std::uint16_t Port() const
  {
    sockaddr_in bound{};
    int bytes = sizeof(bound);
    Assert::AreNotEqual(SOCKET_ERROR, getsockname(m_socket, reinterpret_cast<sockaddr*>(&bound), &bytes));
    return ntohs(bound.sin_port);
  }

  void SendTo(std::uint16_t _port, std::span<const char> _bytes) const
  {
    const sockaddr_in to = At(_port);
    Assert::AreEqual(static_cast<int>(_bytes.size()), sendto(m_socket, _bytes.data(), static_cast<int>(_bytes.size()), 0,
                                                             reinterpret_cast<const sockaddr*>(&to), sizeof(to)));
  }

  // The next datagram and the port it came from, or none within _wait.
  [[nodiscard]] std::optional<std::pair<std::vector<char>, std::uint16_t>> Receive(std::chrono::milliseconds _wait) const
  {
    WSAPOLLFD readable{.fd = m_socket, .events = POLLRDNORM};
    if (WSAPoll(&readable, 1, static_cast<INT>(_wait.count())) != 1)
      return std::nullopt;
    std::vector<char> bytes(65'535);
    sockaddr_in from{};
    int fromBytes = sizeof(from);
    const int received =
      recvfrom(m_socket, bytes.data(), static_cast<int>(bytes.size()), 0, reinterpret_cast<sockaddr*>(&from), &fromBytes);
    Assert::AreNotEqual(SOCKET_ERROR, received);
    bytes.resize(static_cast<std::size_t>(received));
    return std::pair(std::move(bytes), ntohs(from.sin_port));
  }

  // The next datagram and the port it came from, which fails the test unless it comes within _wait.
  [[nodiscard]] std::pair<std::vector<char>, std::uint16_t> Expect(std::chrono::milliseconds _wait, const wchar_t* _what) const
  {
    std::optional<std::pair<std::vector<char>, std::uint16_t>> received = Receive(_wait);
    if (!received.has_value())
      Assert::Fail(_what);
    return std::move(*received);
  }

private:
  static sockaddr_in At(std::uint16_t _port) noexcept
  {
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(_port);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    return address;
  }

  SOCKET m_socket = INVALID_SOCKET;
};

// A datagram that carries _index, padded to _bytes.
std::vector<char> Numbered(std::uint32_t _index, std::size_t _bytes = sizeof(std::uint32_t))
{
  std::vector<char> datagram(std::max(_bytes, sizeof(_index)));
  std::memcpy(datagram.data(), &_index, sizeof(_index));
  return datagram;
}

std::uint32_t IndexOf(const std::vector<char>& _datagram)
{
  std::uint32_t index = 0;
  std::memcpy(&index, _datagram.data(), sizeof(index));
  return index;
}

// A deputy that counts how often it was given the seat to play, and to watch.
class CountingDeputy final : public Outpost::HostedPlayer
{
public:
  [[nodiscard]] std::vector<Outpost::Command> Play([[maybe_unused]] const Outpost::Snapshot& _snapshot) override
  {
    ++plays;
    return {};
  }

  void Watch([[maybe_unused]] const Outpost::Snapshot& _snapshot) override
  {
    ++watches;
  }

  std::uint64_t plays = 0;
  std::uint64_t watches = 0;
};

// Whether _ship has left the map's center in any of _snapshots.
bool HasMoved(const std::vector<Outpost::Snapshot>& _snapshots, Outpost::EntityId _ship)
{
  return std::ranges::any_of(_snapshots,
                             [_ship](const Outpost::Snapshot& _snapshot)
                             {
                               const auto ship = std::ranges::find(_snapshot.entities, _ship, &Outpost::EntityView::id);
                               return ship != _snapshot.entities.end() && ship->position.xMeters > 0.0f;
                             });
}

// The share of _sorted at the nearest rank of _percent, as Tools/FrameTimes.py takes a percentile.
double Percentile(const std::vector<double>& _sorted, std::size_t _percent)
{
  if (_sorted.empty())
    return 0.0;
  return _sorted[std::max<std::size_t>(1, ((_sorted.size() * _percent) + 99) / 100) - 1];
}

double Milliseconds(Clock::duration _duration)
{
  return std::chrono::duration<double, std::milli>(_duration).count();
}

// One player of the measurement, on a channel of the test's own, so that each state is timed as it comes. Its ships are
// ordered across the map every LOAD_PERIOD_TICKS, as the shell's --load orders them (LoadDriver), save its probe, which it
// orders a short way from rest and back, timing the response.
class MeasuredPlayer
{
public:
  static constexpr std::uint64_t LOAD_PERIOD_TICKS = 200;
  static constexpr Outpost::PlanePosition FIRST_POINT{.xMeters = -550.0f, .zMeters = 550.0f};
  static constexpr Outpost::PlanePosition SECOND_POINT{.xMeters = 550.0f, .zMeters = -550.0f};
  static constexpr float PROBE_STEP_METERS = 60.0f;

  MeasuredPlayer(const Outpost::ServerAddress& _seat, Outpost::PlayerId _player, std::optional<Outpost::EntityId> _probe,
                 std::uint64_t _seed)
    : m_player(_player),
      m_probe(_probe),
      m_random(_seed)
  {
    m_channel = Neuron::QuicChannel::Connect({.host = _seat.host,
                                              .port = _seat.port,
                                              .applicationProtocol = std::string(Outpost::QUIC_APPLICATION_PROTOCOL),
                                              .serverCertificate = _seat.certificate});
    m_channel->Send(Outpost::EncodeMessage(Outpost::HelloMessage{.player = _player, .token = _seat.token}));
    const auto deadline = Clock::now() + PATIENCE;
    std::vector<std::vector<std::byte>> welcome;
    while (welcome.empty() && Clock::now() < deadline)
      welcome = m_channel->Receive(100ms);
    Assert::IsFalse(welcome.empty(), L"seated through the link");
  }

  // Takes what has come, waiting up to _wait for the first of it, and gives the orders due.
  void Update(std::chrono::milliseconds _wait)
  {
    for (const std::vector<std::byte>& message : m_channel->Receive(_wait))
    {
      const Clock::time_point arrived = Clock::now();
      Outpost::Message decoded = Outpost::DecodeMessage(message);
      // The catalogue holds no ship, so the states alone are what the load and the probe read (ADR-060 decision 9).
      if (std::holds_alternative<Outpost::CatalogMessage>(decoded))
      {
        catalogBytes += message.size();
        continue;
      }
      auto* snapshot = std::get_if<Outpost::Snapshot>(&decoded);
      Assert::IsNotNull(snapshot, L"only catalogues and snapshots after the welcome");
      arrivals.emplace_back(snapshot->tick, arrived);
      stateBytes += message.size();
      newestTick = std::max(newestTick, snapshot->tick);
      OrderTheLoad(*snapshot);
      WatchTheProbe(*snapshot, arrived);
    }
    if (m_probe.has_value() && m_orderAt.has_value() && Clock::now() >= *m_orderAt)
    {
      m_orderAt.reset();
      m_outward = !m_outward;
      const Outpost::PlanePosition destination{.xMeters = m_home.xMeters + (m_outward ? m_step.xMeters : 0.0f),
                                               .zMeters = m_home.zMeters + (m_outward ? m_step.zMeters : 0.0f)};
      m_channel->Send(
        Outpost::EncodeMessage(Outpost::Command{.order = Outpost::MoveCommand{.ships = {*m_probe}, .destination = destination}}));
      m_ordered = Clock::now();
      m_orderedFrom = m_probeAt;
    }
  }

  std::vector<std::pair<std::uint64_t, Clock::time_point>> arrivals;
  std::uint64_t stateBytes = 0;
  std::uint64_t catalogBytes = 0;
  std::uint64_t newestTick = 0;
  std::vector<double> responsesMilliseconds;

private:
  void OrderTheLoad(const Outpost::Snapshot& _snapshot)
  {
    const std::uint64_t period = _snapshot.tick / LOAD_PERIOD_TICKS;
    if (m_lastPeriod == period)
      return;
    m_lastPeriod = period;
    Outpost::MoveCommand move;
    for (const Outpost::EntityView& entity : _snapshot.entities)
    {
      if (entity.kind == Outpost::EntityKind::Ship && entity.owner == m_player && entity.id != m_probe)
        move.ships.push_back(entity.id);
    }
    const bool first = ((period + m_player.value) % 2) == 0;
    move.destination = first ? FIRST_POINT : SECOND_POINT;
    if (!move.ships.empty())
      m_channel->Send(Outpost::EncodeMessage(Outpost::Command{.order = std::move(move)}));
  }

  // The probe at rest for two states in a row is ordered after a pause of 150 to 400 ms, as the budgets' clicks are; its
  // response is the first state in which it has moved from where it was when the order went.
  void WatchTheProbe(const Outpost::Snapshot& _snapshot, Clock::time_point _arrived)
  {
    if (!m_probe.has_value())
      return;
    const auto probe = std::ranges::find(_snapshot.entities, *m_probe, &Outpost::EntityView::id);
    if (probe == _snapshot.entities.end())
      return;
    if (!m_seen)
    {
      m_seen = true;
      m_home = probe->position;
      // Towards the map's center, away from its edge.
      const float length = std::hypot(m_home.xMeters, m_home.zMeters);
      m_step = {.xMeters = -m_home.xMeters / length * PROBE_STEP_METERS, .zMeters = -m_home.zMeters / length * PROBE_STEP_METERS};
    }
    const bool still = probe->position.xMeters == m_probeAt.xMeters && probe->position.zMeters == m_probeAt.zMeters;
    m_probeAt = probe->position;
    if (m_ordered.has_value())
    {
      if (m_probeAt.xMeters != m_orderedFrom.xMeters || m_probeAt.zMeters != m_orderedFrom.zMeters)
      {
        responsesMilliseconds.push_back(Milliseconds(_arrived - *m_ordered));
        m_ordered.reset();
        m_restingStates = 0;
      }
      return;
    }
    m_restingStates = still ? m_restingStates + 1 : 0;
    if (m_restingStates >= 2 && !m_orderAt.has_value())
      m_orderAt = _arrived + std::chrono::milliseconds{150 + m_random.NextBelow(251)};
  }

  Outpost::PlayerId m_player;
  std::optional<Outpost::EntityId> m_probe;
  Neuron::Random m_random;
  std::unique_ptr<Neuron::QuicChannel> m_channel;
  std::optional<std::uint64_t> m_lastPeriod;
  bool m_seen = false;
  bool m_outward = false;
  Outpost::PlanePosition m_home;
  Outpost::PlanePosition m_step;
  Outpost::PlanePosition m_probeAt;
  Outpost::PlanePosition m_orderedFrom;
  std::uint32_t m_restingStates = 0;
  std::optional<Clock::time_point> m_orderAt;
  std::optional<Clock::time_point> m_ordered;
};

// Two minutes of the measurement load with both players through links of _profile, and what Blue's link did to its states.
std::string Measure(const Outpost::Tuning& _tuning, const Outpost::Map& _map, const LinkProfile& _profile)
{
  Outpost::InProcessServer server(_tuning, _map, {.seed = 1, .measurementLoad = true, .quic = true});
  Outpost::Simulation& simulation = server.SimulationState();
  simulation.PlaceStartingBases(server.MapData());
  Outpost::PlaceMeasurementLoad(simulation, server.MapData(), server.TuningData());
  server.PreparePathfinding();
  // Blue's probe is its ship furthest from the diagonal both fleets cross, where nothing passes to push it.
  std::optional<Outpost::EntityId> probe;
  float furthest = -1.0f;
  for (const Outpost::Entity& entity : simulation.Entities())
  {
    const float away = std::abs(entity.position.xMeters + entity.position.zMeters);
    if (entity.kind == Outpost::EntityKind::Ship && entity.owner == BLUE && away > furthest)
    {
      furthest = away;
      probe = entity.id;
    }
  }

  const Outpost::ServerAddress blueSeat = server.OpenSeat(BLUE, Outpost::NewSeatToken());
  const Outpost::ServerAddress redSeat = server.OpenSeat(RED, Outpost::NewSeatToken());
  ImpairedLink blueLink(blueSeat.port, _profile, 1);
  ImpairedLink redLink(redSeat.port, _profile, 2);
  MeasuredPlayer blue(blueLink.Through(blueSeat), BLUE, probe, 3);
  MeasuredPlayer red(redLink.Through(redSeat), RED, std::nullopt, 4);
  server.Start();
  const Clock::time_point start = Clock::now();
  const auto tick = std::chrono::duration_cast<Clock::duration>(std::chrono::seconds{1}) / server.TicksPerSecond();
  constexpr auto SAMPLE = std::chrono::duration_cast<Clock::duration>(std::chrono::seconds{1}) / 60;
  const Clock::time_point end = start + 2min;
  Clock::time_point nextSample = start + SAMPLE;
  std::uint64_t samples = 0;
  std::uint64_t behind = 0;
  for (Clock::time_point now = start; now < end; now = Clock::now())
  {
    const auto wait = std::chrono::ceil<std::chrono::milliseconds>(std::max(nextSample - now, Clock::duration{}));
    blue.Update(std::min(wait, 5ms));
    red.Update({});
    now = Clock::now();
    if (now >= nextSample)
    {
      // The tick the server has run by now, at the tuning data's rate from its start.
      const auto serverTick = static_cast<std::uint64_t>((now - start) / tick);
      ++samples;
      behind += serverTick > blue.newestTick + 2 ? 1 : 0;
      nextSample += SAMPLE;
    }
  }

  std::uint64_t backlogMostBytes = 0;
  std::vector<double> roundTripsMilliseconds;
  for (const Outpost::SeatFigures& figures : server.TakeSeatFigures())
  {
    if (figures.player != BLUE)
      continue;
    backlogMostBytes = std::max(backlogMostBytes, figures.connection.backlogBytes);
    roundTripsMilliseconds.push_back(static_cast<double>(figures.connection.roundTripMicroseconds) / 1'000.0);
  }
  std::vector<double> lateness;
  double longestGap = 0.0;
  for (std::size_t i = 0; i < blue.arrivals.size(); ++i)
  {
    const auto& [stateTick, arrived] = blue.arrivals[i];
    lateness.push_back(Milliseconds(arrived - (start + (static_cast<std::int64_t>(stateTick) * tick))));
    if (i > 0)
      longestGap = std::max(longestGap, Milliseconds(arrived - blue.arrivals[i - 1].second));
  }
  std::ranges::sort(lateness);
  std::ranges::sort(roundTripsMilliseconds);
  std::ranges::sort(blue.responsesMilliseconds);
  const double meanStateBytes =
    blue.arrivals.empty() ? 0.0 : static_cast<double>(blue.stateBytes) / static_cast<double>(blue.arrivals.size());
  const ImpairedLink::Counts down = blueLink.ToClient();
  return std::format("{}: {} states of {:.0f} bytes on average, and {} bytes of catalogue; late by {:.1f} ms at the median, {:.1f} at the "
                     "95th percentile and {:.1f} "
                     "at the 99th; the longest gap {:.1f} ms; more than two ticks behind the server in {:.2f}% of {} samples; the "
                     "largest backlog {} bytes, {:.1f} states; a round trip of {:.1f} ms at the median; an order's response {:.1f} ms "
                     "at the median and {:.1f} at the 95th percentile, over {} orders; to the client, {} datagrams carried, {} lost "
                     "and {} dropped",
                     _profile.name, blue.arrivals.size(), meanStateBytes, blue.catalogBytes, Percentile(lateness, 50),
                     Percentile(lateness, 95), Percentile(lateness, 99), longestGap,
                     samples == 0 ? 0.0 : 100.0 * static_cast<double>(behind) / static_cast<double>(samples), samples, backlogMostBytes,
                     meanStateBytes == 0.0 ? 0.0 : static_cast<double>(backlogMostBytes) / meanStateBytes,
                     Percentile(roundTripsMilliseconds, 50), Percentile(blue.responsesMilliseconds, 50),
                     Percentile(blue.responsesMilliseconds, 95), blue.responsesMilliseconds.size(), down.carried, down.lost, down.dropped);
}
} // namespace

// The impaired link (ADR-060): a relay on the loopback that delays, loses and limits datagrams, and the game played through
// it. These ask only what holds by a wide margin on any machine; MeasuresTheProfiles measures.
TEST_CLASS(ImpairedLinkTests)
{
public:
  // A datagram arrives no sooner than its way's delay less its jitter, never before one sent ahead of it, and both ways;
  // nothing crosses while the link is cut.
  TEST_METHOD(CarriesDatagramsLateAndInOrderBothWays)
  {
    const Endpoint server;
    const Endpoint client;
    const LinkProfile late{.name = "late", .toServer = {.delay = 30ms, .jitter = 20ms}, .toClient = {.delay = 30ms}};
    ImpairedLink link(server.Port(), late, 1);

    constexpr std::uint32_t COUNT = 100;
    std::vector<Clock::time_point> sent;
    for (std::uint32_t i = 0; i < COUNT; ++i)
    {
      sent.push_back(Clock::now());
      client.SendTo(link.Port(), Numbered(i));
      std::this_thread::sleep_for(1ms);
    }
    std::uint16_t relay = 0;
    for (std::uint32_t i = 0; i < COUNT; ++i)
    {
      const auto [datagram, from] = server.Expect(PATIENCE, std::format(L"datagram {} arrives", i).c_str());
      Assert::AreEqual(i, IndexOf(datagram), L"in the order sent");
      Assert::IsTrue(Clock::now() - sent[i] >= 10ms, L"no sooner than the delay less the jitter");
      relay = from;
    }
    const ImpairedLink::Counts toServer = link.ToServer();
    Assert::AreEqual(std::uint64_t{COUNT}, toServer.carried);
    Assert::AreEqual(std::uint64_t{0}, toServer.lost + toServer.dropped + toServer.cut);

    const Clock::time_point answered = Clock::now();
    server.SendTo(relay, Numbered(7));
    Assert::AreEqual(std::uint32_t{7}, IndexOf(client.Expect(PATIENCE, L"the server's answer reaches the client").first));
    Assert::IsTrue(Clock::now() - answered >= 30ms, L"no sooner than its own way's delay");

    link.Cut(true);
    for (std::uint32_t i = 0; i < 5; ++i)
      client.SendTo(link.Port(), Numbered(i));
    Assert::IsFalse(server.Receive(300ms).has_value(), L"nothing crosses a cut link");
    Assert::AreEqual(std::uint64_t{5}, link.ToServer().cut);
    link.Cut(false);
    client.SendTo(link.Port(), Numbered(9));
    Assert::AreEqual(std::uint32_t{9}, IndexOf(server.Expect(PATIENCE, L"and it carries again once it is whole").first));
  }

  // A way loses its share of datagrams, by its seed, and carries no more than its limit, dropping what would wait longer
  // than its queue holds.
  TEST_METHOD(LosesItsShareAndHoldsItsLimit)
  {
    const Endpoint server;
    const Endpoint client;
    {
      ImpairedLink lossy(server.Port(), {.name = "lossy", .toServer = {.lossPartsPerMillion = 500'000}}, 1);
      constexpr std::uint32_t COUNT = 1'000;
      for (std::uint32_t i = 0; i < COUNT; ++i)
        client.SendTo(lossy.Port(), Numbered(i));
      std::uint64_t received = 0;
      while (server.Receive(500ms).has_value())
        ++received;
      const ImpairedLink::Counts counts = lossy.ToServer();
      Assert::AreEqual(std::uint64_t{COUNT}, counts.carried + counts.lost, L"each datagram carried or lost");
      Assert::AreEqual(counts.carried, received, L"and each one carried arrives");
      Assert::IsTrue(counts.lost > COUNT / 4 && counts.lost < 3 * COUNT / 4, std::format(L"about half lost: {}", counts.lost).c_str());
    }

    // 1,028 bytes on the wire at 800 kbit/s take 10.28 ms, so the queue's 200 ms holds about 20 of a burst of 50.
    ImpairedLink limited(server.Port(), {.name = "limited", .toServer = {.bitsPerSecond = 800'000}}, 1);
    const Clock::time_point start = Clock::now();
    for (std::uint32_t i = 0; i < 50; ++i)
      client.SendTo(limited.Port(), Numbered(i, 1'000));
    std::uint64_t received = 0;
    Clock::time_point lastArrival = start;
    while (server.Receive(1s).has_value())
    {
      ++received;
      lastArrival = Clock::now();
    }
    const ImpairedLink::Counts counts = limited.ToServer();
    Assert::IsTrue(counts.dropped > 0 && counts.carried >= 10,
                   std::format(L"{} carried, {} dropped", counts.carried, counts.dropped).c_str());
    Assert::AreEqual(counts.carried, received);
    Assert::IsTrue(lastArrival - start >= static_cast<std::int64_t>(counts.carried) * 10ms, L"no faster than the limit");
  }

  // Through a clear link every snapshot of a match arrives, in order, and an order reaches the simulation.
  TEST_METHOD(CarriesEverySnapshotOfAMatchWhenClear)
  {
    auto server = std::make_unique<Outpost::InProcessServer>(
      Outpost::LoadTuning(ReadRepositoryTuning()), Outpost::LoadMap(ReadRepositoryMap()), Outpost::ServerDesc{.seed = 5, .quic = true});
    const Outpost::EntityId ship = server->SimulationState().SpawnShip(BLUE, SWARM, SMALL_ION, {});
    const Outpost::ServerAddress seat = server->OpenSeat(BLUE, Outpost::NewSeatToken());
    const ImpairedLink link(seat.port, CLEAR_LINK, 1);
    Outpost::QuicTransport blue(link.Through(seat), BLUE);
    Outpost::QuicTransport red(server->OpenSeat(RED, Outpost::NewSeatToken()), RED);
    server->Start();
    blue.Send({.order = Outpost::MoveCommand{.ships = {ship}, .destination = {.xMeters = 300.0f}}});

    std::vector<Outpost::Snapshot> snapshots;
    const auto deadline = Clock::now() + PATIENCE;
    while (snapshots.size() < 40 && Clock::now() < deadline)
    {
      std::this_thread::sleep_for(10ms);
      for (Outpost::Snapshot& snapshot : blue.Receive())
        snapshots.push_back(std::move(snapshot));
      (void)red.Receive();
    }
    Assert::IsTrue(snapshots.size() >= 40, L"two seconds of snapshots");
    for (std::size_t i = 1; i < snapshots.size(); ++i)
      Assert::AreEqual(snapshots[i - 1].tick + 1, snapshots[i].tick, L"every one, in order");
    Assert::IsTrue(HasMoved(snapshots, ship), L"the order reached the simulation");
    Assert::AreEqual(std::uint64_t{0}, link.ToClient().lost + link.ToClient().dropped);
  }

  // Under each profile a universe's seat is taken through the link, an order given through it reaches the simulation, and
  // the seat's deputy takes it once the player has gone and hands it back when the player comes again through the link.
  TEST_METHOD(PlaysASeatUnderEachProfile)
  {
    std::uint64_t seed = 1;
    for (const LinkProfile& profile : IMPAIRED_LINKS)
    {
      const std::wstring name(profile.name.begin(), profile.name.end());
      const TemporaryFolder folder;
      auto server =
        std::make_unique<Outpost::InProcessServer>(Outpost::LoadTuning(ReadRepositoryTuning()), Outpost::LoadMap(ReadRepositoryMap()),
                                                   Outpost::ServerDesc{.seed = 5, .quic = true, .universe = folder.Path()});
      server->SimulationState().PlaceStartingBases(server->MapData());
      const Outpost::EntityId ship = server->SimulationState().SpawnShip(BLUE, SWARM, SMALL_ION, {});
      server->UseUniverse(folder.Path(), UniverseMatch::RepositoryDataHash(), Outpost::UniverseFolder::Recover(folder.Path()));
      const Outpost::ServerAddress seat = server->OpenSeat(BLUE, Outpost::NewSeatToken());
      auto counting = std::make_unique<CountingDeputy>();
      const CountingDeputy& deputy = *counting;
      server->Host(BLUE, std::move(counting));
      const ImpairedLink link(seat.port, profile, seed++);

      {
        Outpost::QuicTransport blue(link.Through(seat), BLUE);
        blue.Send({.order = Outpost::MoveCommand{.ships = {ship}, .destination = {.xMeters = 300.0f}}});
        bool moved = false;
        const auto deadline = Clock::now() + PATIENCE;
        while (!moved && Clock::now() < deadline)
        {
          server->Step();
          std::this_thread::sleep_for(10ms);
          moved = HasMoved(blue.Receive(), ship);
        }
        Assert::IsTrue(moved, (name + L": the order reached the simulation").c_str());
        Assert::IsTrue(deputy.watches > 0 && deputy.plays == 0, (name + L": the player plays the seat it took").c_str());
      }

      // The player has gone. The server hears so once the link carries the close, or, should the link lose it, once MsQuic's
      // idle timeout ends the connection; a minute of ticks later the deputy takes the seat.
      const auto deadline = Clock::now() + PATIENCE;
      while (deputy.plays == 0 && Clock::now() < deadline)
        server->Step();
      Assert::IsTrue(deputy.plays > 0, (name + L": the deputy takes the seat").c_str());

      Outpost::QuicTransport again(link.Through(seat), BLUE);
      const std::uint64_t watched = deputy.watches;
      std::vector<Outpost::Snapshot> back;
      while (back.empty() && Clock::now() < deadline + PATIENCE)
      {
        server->Step();
        std::this_thread::sleep_for(10ms);
        back = again.Receive();
      }
      Assert::IsTrue(deputy.watches > watched, (name + L": and hands it back").c_str());
      Assert::IsTrue(!back.empty() && back.front().away.has_value(), (name + L": telling the player what happened").c_str());
    }
  }

  // Two minutes of the measurement load, as --load plays it, with both players through each profile's links: how late each
  // of Blue's states arrives against when the server's tick was due, the longest gap between two, the share of 60 Hz samples
  // at which Blue's newest state is more than two ticks behind the server, Blue's seat's largest backlog, and an order's
  // response, from its send to the first state in which the ordered ship has moved. Run on its own, with
  // OUTPOST_NETWORK_MEASURE set, since it plays eight minutes.
  TEST_METHOD(MeasuresTheProfiles)
  {
    if (GetEnvironmentVariableW(L"OUTPOST_NETWORK_MEASURE", nullptr, 0) == 0)
    {
      Logger::WriteMessage("Not run here: OUTPOST_NETWORK_MEASURE runs it.");
      return;
    }
    const Outpost::Tuning tuning = Outpost::LoadTuning(ReadRepositoryTuning());
    const Outpost::Map map = Outpost::LoadMap(ReadRepositoryMap());
    for (const LinkProfile& profile : {CLEAR_LINK, NEAR_LINK, WI_FI_LINK, HOTSPOT_LINK})
      Logger::WriteMessage(Measure(tuning, map, profile).c_str());
  }
};
} // namespace GameLogicTests

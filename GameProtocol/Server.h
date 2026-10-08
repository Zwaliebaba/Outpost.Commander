#pragma once

namespace Outpost
{
struct ServerDesc
{
  // Every random draw in the simulation comes from one PRNG seeded with this, so that a match reproduces from its seed
  // and its command log (ADR-002 decision 8).
  std::uint64_t seed = 0;
  // A measurement run, not a match: the map also holds task 2.7's load of 200 ships and 40 structures.
  bool measurementLoad = false;
  // A measurement run, not a match: task 3.7's stress scene, 200 ships and 40 structures in combat, kept at full size.
  bool stressLoad = false;
  // The server also takes players over QUIC, through OpenSeat (ADR-060): on the loopback address and a port the system
  // chooses, unless the two below say otherwise (ADR-078).
  bool quic = false;
  // The IPv4 or IPv6 address the server listens on, such as "0.0.0.0" for every interface; empty for the loopback.
  std::string quicAddress;
  // The port it listens on; zero lets the system choose one.
  std::uint16_t quicPort = 0;
  // A world rather than a match (Phase 5 design §5, ADR-077): the folder its saves and command log are kept in. A folder
  // that holds a save of the world comes back as the world it saved, whatever the seed above; one that holds none starts a
  // new world from the seed. Empty for a match. A world's server keeps its QUIC certificate in the folder too, so that a
  // player pins it once (ADR-078).
  std::filesystem::path world;
};

// The parts of a tick the server times for measurement (task 8.1, Phase 1 design §10). They nest: Commands holds each
// order's GroupRoute and ShipPaths, and GraphBuild is counted wherever a path graph was built, inside whichever part needed
// it. The rest follow the simulation's step in order, and Snapshots is the server building every player's snapshot.
enum class TickPart : std::uint8_t
{
  Commands,
  GraphBuild,
  GroupRoute,
  ShipPaths,
  Fight,
  Targets,
  Economy,
  Move,
  Separate,
  Vision,
  Snapshots
};

inline constexpr std::size_t TICK_PART_COUNT = 11;

// A part's name in the measurement log.
[[nodiscard]] constexpr std::string_view TickPartName(TickPart _part) noexcept
{
  constexpr std::array<std::string_view, TICK_PART_COUNT> NAMES{
    "commands", "graph_build", "group_route", "ship_paths", "fight", "targets", "economy", "move", "separate", "vision", "snapshots"};
  return NAMES[static_cast<std::size_t>(_part)];
}

// One tick on the server's own clock: all of it, and each part of it (TickPart).
struct TickTiming
{
  std::chrono::nanoseconds total{};
  std::array<std::chrono::nanoseconds, TICK_PART_COUNT> parts{};

  [[nodiscard]] std::chrono::nanoseconds Part(TickPart _part) const noexcept
  {
    return parts[static_cast<std::size_t>(_part)];
  }
};

// The authoritative simulation, as its clients see it (ADR-002). The in-process server is declared here and defined in
// GameLogic, so the executable can start it without including a server header.
class Server : Neuron::NonCopyable
{
public:
  virtual ~Server() = default;

  // A connection for one player, the human or the AI. The server keeps the other end.
  [[nodiscard]] virtual std::unique_ptr<Transport> Connect(PlayerId _player) = 0;

  // A seat for one player, which a QuicTransport takes at the address returned, with _token, which the address carries
  // (ADR-060, ADR-078). A match's seats are each taken before it starts; a world's may be taken at any time, and taken
  // again with the token by the player's newest connection. Throws Neuron::Exception when the server was not made to
  // listen over QUIC (ServerDesc::quic), once it has started, or when the player already has a connection or a seat.
  [[nodiscard]] virtual ServerAddress OpenSeat(PlayerId _player, const SeatToken& _token) = 0;

  // Starts the server's ticks on a thread of its own, at its fixed rate whatever the client's frame rate (ADR-025). Each
  // tick applies the commands that have arrived and sends each connected player a snapshot. Connect every player first.
  // The thread stops when the server is destroyed. Throws Neuron::Exception when a match's seat has not been taken; a
  // world starts with its seats open.
  virtual void Start() = 0;

  // Runs one tick now, on the caller's thread, applying the commands that have arrived and sending each connected player
  // its snapshot: a headless run that is not paced by wall time, such as the AI-against-AI matches (Phase 1 plan task
  // 13.1), steps the server instead of starting it. Throws Neuron::Exception once the server has started.
  virtual void Step() = 0;

  // How many ticks the server runs per second of wall time. A client interpolates between snapshots at this rate
  // (task 2.5).
  [[nodiscard]] virtual std::uint32_t TicksPerSecond() const noexcept = 0;

  // How long each tick since the last call took on the server's own clock, oldest first: its commands, its simulation
  // step and its snapshots (task 2.7), and each of its parts (task 8.1). The server has no PIX markers (ADR-005), so this
  // is how its time is measured. An exception the server's thread met is thrown again here, on the caller's thread, and
  // the server runs no more ticks.
  [[nodiscard]] virtual std::vector<TickTiming> TakeTickTimings() = 0;
};

// Loads the tuning data and the map from the package (ADR-008) and starts a server with them. Throws Neuron::Exception when the data is
// missing or invalid.
[[nodiscard]] std::unique_ptr<Server> CreateInProcessServer(const ServerDesc& _desc);

// Makes servers as CreateInProcessServer does, from one reading of the tuning data and the map, for a run that plays many
// matches, such as the AI-against-AI matches (Phase 1 plan task 13.1).
using ServerFactory = std::function<std::unique_ptr<Server>(const ServerDesc&)>;

// Loads and checks the tuning data and the map once, and throws Neuron::Exception as CreateInProcessServer does when
// they are missing or invalid. Each server the factory makes has a copy of the data of its own, so it may make them on
// several threads at once.
[[nodiscard]] ServerFactory InProcessServerFactory();
} // namespace Outpost
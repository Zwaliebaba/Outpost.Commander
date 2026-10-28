#pragma once

namespace Outpost
{
struct ServerDesc
{
  // Every random draw in the simulation comes from one PRNG seeded with this, so that a match reproduces from its seed
  // and its command log (ADR-009 decisions 1 and 5).
  std::uint64_t seed = 0;
  // A measurement run, not a match: the map also holds a load of 200 ships and 40 structures.
  bool measurementLoad = false;
  // A measurement run, not a match: the stress scene, 200 ships and 40 structures in combat, kept at full size.
  bool stressLoad = false;
  // The server also takes players over QUIC, through OpenSeat (ADR-060): on the loopback address and a port the system
  // chooses, unless the two below say otherwise (ADR-078).
  bool quic = false;
  // The IPv4 or IPv6 address the server listens on, such as "0.0.0.0" for every interface of IPv4, or "*" for every
  // interface of both; empty for the loopback.
  std::string quicAddress;
  // The port it listens on; zero lets the system choose one.
  std::uint16_t quicPort = 0;
  // A universe rather than a match (ADR-077 decision 7): the folder its saves and command log are kept in. A folder
  // that holds a save of the universe comes back as the universe it saved, whatever the seed above; one that holds none starts a
  // new universe from the seed. Empty for a match. A universe's server keeps its QUIC certificate in the folder too, so that a
  // player pins it once (ADR-078).
  std::filesystem::path universe;
  // A battle matchup rather than a match (ADR-083 decision 2): the index of one of Matchups.json's, whose fleets fight
  // with no bases, no pirates and no fog of war. None for a match.
  std::optional<std::uint32_t> matchup;
};

// How long after a player's connection has gone its deputy takes the seat (ADR-079 decision 4).
inline constexpr std::uint32_t DEPUTY_DELAY_SECONDS = 60;

// The parts of a tick the server times for measurement. They nest: Commands holds each
// order's GroupRoute and ShipPaths, and GraphBuild is counted wherever a path graph was built, inside whichever part needed
// it. The rest follow the simulation's step in order, Snapshots is the server building every player's snapshot, Hosted
// the players it plays itself deciding what to do (ADR-079), and Overseer the universe's view it sends its overseer (ADR-101).
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
  Snapshots,
  Hosted,
  Overseer
};

inline constexpr std::size_t TICK_PART_COUNT = 13;

// A part's name in the measurement log.
[[nodiscard]] constexpr std::string_view TickPartName(TickPart _part) noexcept
{
  constexpr std::array<std::string_view, TICK_PART_COUNT> NAMES{"commands",  "graph_build", "group_route", "ship_paths", "fight",
                                                                "targets",   "economy",     "move",        "separate",   "vision",
                                                                "snapshots", "hosted",      "overseer"};
  return NAMES[static_cast<std::size_t>(_part)];
}

// How often the server samples each seat's connection, and the most samples it keeps for TakeSeatFigures, an hour of one
// seat's, so that a server whose figures nobody takes holds no more (ADR-004 decision 6).
inline constexpr std::uint32_t SEAT_FIGURES_SECONDS = 1;
inline constexpr std::size_t SEAT_FIGURES_KEPT = 3'600;

// One seat's connection over QUIC, as the server found it after a tick (ADR-004 decision 6).
struct SeatFigures
{
  std::uint64_t tick = 0;
  PlayerId player;
  Neuron::QuicStatistics connection;
};

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
  // (ADR-060, ADR-078). A match's seats are each taken before it starts; a universe's may be taken at any time, and taken
  // again with the token by the player's newest connection. Throws Neuron::Exception when the server was not made to
  // listen over QUIC (ServerDesc::quic), once it has started, or when the player already has a connection or a seat.
  [[nodiscard]] virtual ServerAddress OpenSeat(PlayerId _player, const SeatToken& _token) = 0;

  // Has the server play _player itself, on its own thread (ADR-079). Without a seat opened for the player, _hosted is the
  // player: a universe's AI empire. With one, _hosted is the seat's deputy, which plays the seat until the player first takes
  // it, and again from DEPUTY_DELAY_SECONDS after the player's connection has gone until the player takes it again. Throws
  // Neuron::Exception once the server has started, or when the player is hosted already or has a connection of its own.
  virtual void Host(PlayerId _player, std::unique_ptr<HostedPlayer> _hosted) = 0;

  // Starts the server's ticks on a thread of its own, at its fixed rate whatever the client's frame rate (ADR-025). Each
  // tick applies the commands that have arrived and sends each connected player a snapshot. Connect every player first.
  // The thread stops when the server is destroyed. Throws Neuron::Exception when a match's seat has not been taken; a
  // universe starts with its seats open.
  virtual void Start() = 0;

  // Runs one tick now, on the caller's thread, applying the commands that have arrived and sending each connected player
  // its snapshot: a headless run that is not paced by wall time, such as the AI-against-AI matches (ADR-038 decision 3),
  // steps the server instead of starting it. Throws Neuron::Exception once the server has started.
  virtual void Step() = 0;

  // How many ticks the server runs per second of wall time. A client interpolates between snapshots at this rate
  // (ADR-013 decision 3).
  [[nodiscard]] virtual std::uint32_t TicksPerSecond() const noexcept = 0;

  // How long each tick since the last call took on the server's own clock, oldest first: its commands, its simulation
  // step and its snapshots, and each of its parts. The server has no PIX markers (ADR-005), so this
  // is how its time is measured. An exception the server's thread met is thrown again here, on the caller's thread, and
  // the server runs no more ticks.
  [[nodiscard]] virtual std::vector<TickTiming> TakeTickTimings() = 0;

  // Each seat's connection, taken over QUIC, every SEAT_FIGURES_SECONDS of ticks since the last call, oldest first, and no
  // more than the last SEAT_FIGURES_KEPT of them: for measurement (ADR-004 decision 6). None for a server that takes no
  // player over QUIC.
  [[nodiscard]] virtual std::vector<SeatFigures> TakeSeatFigures() = 0;
};

// Loads the tuning data and the map from the package (ADR-008) and starts a server with them. Throws Neuron::Exception when the data is
// missing or invalid.
[[nodiscard]] std::unique_ptr<Server> CreateInProcessServer(const ServerDesc& _desc);

// Makes servers as CreateInProcessServer does, from one reading of the tuning data and the map, for a run that plays many
// matches, such as the AI-against-AI matches (ADR-038 decision 3).
using ServerFactory = std::function<std::unique_ptr<Server>(const ServerDesc&)>;

// Loads and checks the tuning data and the map once, and throws Neuron::Exception as CreateInProcessServer does when
// they are missing or invalid. Each server the factory makes has a copy of the data of its own, so it may make them on
// several threads at once.
[[nodiscard]] ServerFactory InProcessServerFactory();
} // namespace Outpost
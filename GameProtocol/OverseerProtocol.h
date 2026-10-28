#pragma once

namespace Outpost
{
// The application protocol a universe's overseer negotiates over its own QUIC connection to the server, beside the game's
// (ADR-101).
inline constexpr std::string_view OVERSEER_APPLICATION_PROTOCOL = "outpost-overseer/1";

// The version of the overseer's messages. A server refuses an overseer that speaks another. It stays 1 until the game is live,
// as the game's does (ADR-060); until then a hello's layout hash is what tells an overseer of another build.
inline constexpr std::uint32_t OVERSEER_PROTOCOL_VERSION = 1;

// How the overseer's messages are laid out, as ByteLayout describes them, followed by how the game's are, since the universe's
// view crosses as the game's own state messages (ADR-101); and its 64-bit FNV-1a, which the overseer's hello carries, so that
// a change to either refuses an overseer of another build.
[[nodiscard]] std::string OverseerLayout();
[[nodiscard]] std::uint64_t OverseerLayoutHash();

// The longest message an overseer may send, before its hello has been welcomed and after: it sends only short ones.
inline constexpr std::uint32_t OVERSEER_MESSAGE_LIMIT_BYTES = 256;
// How many views of the universe the server sends its overseer a second (ADR-101).
inline constexpr std::uint32_t OVERSEER_VIEWS_PER_SECOND = 2;
// How many of the universe log's last records an overseer is sent as it is welcomed.
inline constexpr std::size_t OVERSEER_RECORDS_KEPT = 500;

// Why the server closed its overseer's connection: the QUIC application error code it closes with, each below
// Neuron::FRAMING_ERROR_CODE.
enum class OverseerCloseReason : std::uint8_t
{
  Finished,
  WrongVersion,
  TokenRefused,
  MalformedMessage,
  // A newer connection showed the token: one overseer at a time, the newest (ADR-101).
  Taken
};

// What the server's close says to its overseer, by its QUIC error code; none for a code that is no OverseerCloseReason.
[[nodiscard]] std::optional<std::string_view> DescribeOverseerClose(std::uint64_t _errorCode) noexcept;

// Who plays a seat in a tick of a universe: its player, its deputy while the player is away (ADR-079), or the AI, when the
// seat is an AI empire's.
enum class SeatPlay : std::uint8_t
{
  Player,
  Deputy,
  Ai
};

// What came of a connection to a universe's seat (ADR-082 decision 1).
enum class JoinOutcome : std::uint8_t
{
  // Its hello took the seat.
  Welcomed,
  // Refused: a hello of another build; a token that is not the seat's, or a seat that is not open; a message that is no hello,
  // or a seated client that broke the rules; and a connection that said no hello in time.
  WrongVersion,
  SeatRefused,
  Malformed,
  Silent,
  // Gone from its seat: taken by its player's newer connection, closed by the game, or lost without a word, as when MsQuic's
  // idle timeout ends it.
  Taken,
  Closed,
  Lost
};

inline constexpr std::size_t JOIN_OUTCOME_COUNT = 8;

// The overseer's first message: the version it speaks and how its messages are laid out, which stay its first two fields so
// that a server of another build can still read them, then the universe's overseer token (ADR-101).
struct OverseerHelloMessage
{
  std::uint32_t protocolVersion = OVERSEER_PROTOCOL_VERSION;
  std::uint64_t layoutHash = OverseerLayoutHash();
  SeatToken token{};
};

// One of a universe's seats: its player, and the difficulty of the AI empire that plays it, empty for a player's seat.
struct OverseerSeat
{
  PlayerId player;
  std::string ai;
};

// The server's answer to an overseer whose hello it took: what never changes while the server runs, the game's wire layout,
// protocol and universe state version, the tick rate, the universe's folder, its seats, its sectors without who holds them,
// its asteroid fields, what it came back from as it started, the save of a tick and the commands logged after it that it
// replayed, or none for a new universe (ADR-077), and how many seconds of ticks go between two saves; and the universe log's
// records the overseer's lanes are drawn from, of the last LANE_SECONDS of universe time (KeepLaneRecords).
struct OverseerWelcomeMessage
{
  std::uint64_t wireLayoutHash = 0;
  std::uint32_t protocolVersion = 0;
  std::uint32_t stateVersion = 0;
  std::uint32_t ticksPerSecond = 0;
  std::string folderUtf8;
  std::vector<OverseerSeat> seats;
  std::vector<SectorView> sectors;
  std::vector<EntityView> fields;
  std::optional<std::uint64_t> recoveredSaveTick;
  std::uint64_t replayedCommands = 0;
  std::uint32_t saveSeconds = 0;
  std::vector<std::string> laneRecords;
};

// A seat's link to its player: none for an AI empire's seat, open while no connection has taken it, online while the player's
// newest connection holds it, and gone once that connection has.
enum class SeatLink : std::uint8_t
{
  None,
  Open,
  Online,
  Gone
};

// One connection's figures, as Neuron::QuicStatistics has them (ADR-004 decision 6).
struct LinkFigures
{
  std::uint32_t roundTripMicroseconds = 0;
  std::uint32_t leastRoundTripMicroseconds = 0;
  std::uint64_t packetsSent = 0;
  std::uint64_t packetsLost = 0;
  std::uint32_t congestionWindowBytes = 0;
  std::uint64_t backlogBytes = 0;
  std::uint64_t bytesSent = 0;
};

[[nodiscard]] LinkFigures FiguresOf(const Neuron::QuicStatistics& _statistics) noexcept;

// One seat, as the server's thread last saw it, each second of ticks: its player and who plays it; its link and the tick it
// became so, and, while it is gone and its player still plays, the tick the deputy takes it (ADR-079 decision 4); its
// connection's figures; the commands applied for it and refused it in the second (ADR-094); its Ore, income in hundredths a
// second, fleet in command points against its cap, nodes held and topics researched; and, while it has lost, the tick it
// restarts at, or has waited since for its start to be free (ADR-084 decision 4).
struct SeatHealth
{
  PlayerId player;
  SeatPlay play = SeatPlay::Player;
  SeatLink link = SeatLink::None;
  std::uint64_t linkTick = 0;
  std::optional<std::uint64_t> deputyTick;
  std::optional<LinkFigures> connection;
  std::uint32_t orders = 0;
  std::uint32_t refused = 0;
  std::int32_t ore = 0;
  std::int32_t incomeHundredthsPerSecond = 0;
  std::int32_t commandPoints = 0;
  std::int32_t fleetCap = 0;
  std::int32_t nodes = 0;
  std::int32_t researched = 0;
  std::optional<std::uint64_t> restartTick;
};

// A battle under way, as the universe log keeps it (ADR-082): its sector, 0 for none, the tick it began, and the seats
// fighting in it.
struct BattleHealth
{
  std::int32_t sector = 0;
  std::uint64_t startTick = 0;
  std::vector<PlayerId> players;
};

// The server's process (ADR-101): its working set and private bytes, its CPU in hundredths of a core since the last Health,
// its threads and handles, how long it has run, and the disk free under the universe's folder.
struct ProcessHealth
{
  std::uint64_t workingSetBytes = 0;
  std::uint64_t privateBytes = 0;
  std::uint32_t cpuHundredths = 0;
  std::uint32_t threads = 0;
  std::uint32_t handles = 0;
  std::uint64_t uptimeSeconds = 0;
  std::uint64_t diskFreeBytes = 0;
};

// What the server's main thread sends its overseer once a second (ADR-101): the wall time, in milliseconds since 1970 UTC;
// the tick the server's thread last kept its seats at, which stands still while that thread is stuck; each tick's time since
// the last Health, in the order they ran, the tick's parts summed over them, and the longest one's largest part; the ticks
// dropped after a stall (ADR-009); the seats and the battles under way; the joins since the server started, by JoinOutcome;
// the connections the game's listener holds; and the process.
struct HealthMessage
{
  std::int64_t wallMilliseconds = 0;
  std::uint64_t tick = 0;
  std::vector<std::uint32_t> tickMicroseconds;
  std::array<std::uint32_t, TICK_PART_COUNT> partMicroseconds{};
  TickPart longestPart = TickPart::Commands;
  std::uint32_t droppedTicks = 0;
  std::vector<SeatHealth> seats;
  std::vector<BattleHealth> battles;
  std::array<std::uint32_t, JOIN_OUTCOME_COUNT> joins{};
  std::uint32_t connections = 0;
  ProcessHealth process;
};

// Records of the universe log, each a line as Universe.log holds it, in the order it wrote them: its last ones as the overseer
// is welcomed, and then each tick's as the server's thread writes them (ADR-082).
struct LogRecordsMessage
{
  std::vector<std::string> lines;
};

// How the server ended: stopped by its owner, by Ctrl+C, Ctrl+Break or closing its window; stopped by Windows, as it signs
// out or shuts down; or by a failure on its thread.
enum class ServerEnding : std::uint8_t
{
  StoppedByOwner,
  StoppedByWindows,
  Failed
};

// What the server says before its process ends: how it ended, the failure's message, and the tick of the universe's last save,
// which it comes back from with the commands logged after it (ADR-077).
struct ClosingMessage
{
  ServerEnding ending = ServerEnding::StoppedByOwner;
  std::string messageUtf8;
  std::uint64_t lastSaveTick = 0;
};

// Which sight the universe's view shows: full sight, with no player, or one seat's own snapshot.
struct WatchSightMessage
{
  PlayerId seat;
};

// The tick of the newest view the overseer holds, which the server sends the next ones as what changed since (ADR-098).
struct ViewHeldMessage
{
  std::uint64_t tick = 0;
};

// Everything that crosses the overseer's own stream. The overseer sends a hello, then which sight it watches and the views it
// holds; the server a welcome, then Health, the log's records and, last, Closing. The universe's view crosses apart, as the
// game's StateMessage or StateChangeMessage on a stream of its own (ADR-096), which HeldStates puts together.
using OverseerMessage = std::variant<OverseerHelloMessage, OverseerWelcomeMessage, HealthMessage, LogRecordsMessage, ClosingMessage,
                                     WatchSightMessage, ViewHeldMessage>;

// Each encodes one message as the bytes that cross the overseer's stream, as the game's are (ADR-060 decision 4).
[[nodiscard]] std::vector<std::byte> EncodeOverseerMessage(const OverseerHelloMessage& _hello);
[[nodiscard]] std::vector<std::byte> EncodeOverseerMessage(const OverseerWelcomeMessage& _welcome);
[[nodiscard]] std::vector<std::byte> EncodeOverseerMessage(const HealthMessage& _health);
[[nodiscard]] std::vector<std::byte> EncodeOverseerMessage(const LogRecordsMessage& _records);
[[nodiscard]] std::vector<std::byte> EncodeOverseerMessage(const ClosingMessage& _closing);
[[nodiscard]] std::vector<std::byte> EncodeOverseerMessage(const WatchSightMessage& _sight);
[[nodiscard]] std::vector<std::byte> EncodeOverseerMessage(const ViewHeldMessage& _held);

// The message _bytes hold. Throws Neuron::Exception when they are not exactly one message.
[[nodiscard]] OverseerMessage DecodeOverseerMessage(std::span<const std::byte> _bytes);

// The version, and the layout's hash, an overseer's hello in _bytes carries, each read alone so that a hello of another build
// is told apart from a broken one; none when the bytes do not start a hello or end before the field does.
[[nodiscard]] std::optional<std::uint32_t> PeekOverseerHelloVersion(std::span<const std::byte> _bytes) noexcept;
[[nodiscard]] std::optional<std::uint64_t> PeekOverseerHelloLayout(std::span<const std::byte> _bytes) noexcept;
} // namespace Outpost

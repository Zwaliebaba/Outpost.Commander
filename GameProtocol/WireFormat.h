#pragma once

namespace Outpost
{
// The version of the messages below. A server refuses a client that speaks another (ADR-060). It stays 1 until the game is
// live (owner, 2026-10-09); until then a hello's layout hash is what tells a client of another build.
inline constexpr std::uint32_t PROTOCOL_VERSION = 1;

// How every message below is laid out, as ByteLayout describes it (ADR-077), and its 64-bit FNV-1a, which a hello carries:
// a server refuses a client whose messages are laid out otherwise, whatever version it speaks (ADR-060).
[[nodiscard]] std::string WireLayout();
[[nodiscard]] std::uint64_t WireLayoutHash();

// The application protocol a QUIC connection to the server negotiates (ADR-060).
inline constexpr std::string_view QUIC_APPLICATION_PROTOCOL = "outpost-commander/1";

// Why the server closed a client's connection: the QUIC application error code it closes with. Each is below
// Neuron::FRAMING_ERROR_CODE, which the channel closes with when the bytes are not messages at all.
enum class CloseReason : std::uint8_t
{
  Finished,
  WrongVersion,
  SeatRefused,
  MalformedMessage,
  // The seat was taken again, with its token, by another connection: the player's newest connection holds it (ADR-078).
  SeatTaken
};

// What the server's close says to a player, by its QUIC error code; none for a code that is no CloseReason.
[[nodiscard]] std::optional<std::string_view> DescribeClose(std::uint64_t _errorCode) noexcept;

// The first message a client sends: the version it speaks and how its messages are laid out, which stay its first two
// fields so that a server of another build can still read them, then the player whose seat it takes, and the seat's token
// (ADR-060, ADR-078).
struct HelloMessage
{
  std::uint32_t protocolVersion = PROTOCOL_VERSION;
  std::uint64_t layoutHash = WireLayoutHash();
  PlayerId player;
  SeatToken token{};
};

// The server's answer once it has seated the client. Snapshots follow it.
struct WelcomeMessage
{
  PlayerId player;
};

// What a player's snapshot holds that seldom changes or never does, which crosses a seat's connection once, before its first
// snapshot, and again before the snapshot of any tick in which it changed; the snapshot on the wire leaves it out
// (ADR-060 decision 9). Its sectors are the map's, with their names, borders, nodes and neighbors, and none of what changes
// in play: their holders, suppression, cut-off and guard stay in the snapshot. Its fields are the map's asteroid fields,
// which every player always sees, in identifier order. The snapshot's numbers stay in it, since a number costs the wire
// its bytes either way.
struct CatalogMessage
{
  std::vector<DesignView> designs;
  std::vector<StructureTypeView> structureTypes;
  std::vector<HullView> hulls;
  std::vector<DriveView> drives;
  std::vector<WeaponView> weapons;
  std::vector<ModuleView> modules;
  std::vector<ResearchTopicView> research;
  std::vector<SectorView> sectors;
  std::vector<EntityView> fields;
};

// A tick's state: the snapshot without its catalog and its events, which crosses on a stream of its own, abandoned for a newer
// one the client has not yet got whole (ADR-096). It says how many catalogs and events the seat's connection had been sent
// when the state was, so that the client puts it together with the catalog it was built against and the events of its tick.
struct StateMessage
{
  std::uint32_t orderedSent = 0;
  Snapshot snapshot;
};

// What a tick did that a state alone does not show, which crosses reliably and in order, every tick that has any (ADR-096):
// the shots fired and the entities destroyed in it, the player's events of it, and its report of a time away.
struct EventsMessage
{
  std::uint64_t tick = 0;
  std::vector<ShotView> shots;
  std::vector<DestroyedView> destroyed;
  std::vector<EventView> events;
  std::optional<AwayReport> away;
};

// Everything that crosses a QUIC connection to the server. A client sends a hello and then commands, and the server a
// welcome and then catalogs, events and states.
using Message = std::variant<HelloMessage, WelcomeMessage, Command, StateMessage, CatalogMessage, EventsMessage>;

// Each encodes one message as the bytes that cross the connection: which message it is, and then its fields in the order
// they are declared, little-endian (ADR-060).
[[nodiscard]] std::vector<std::byte> EncodeMessage(const HelloMessage& _hello);
[[nodiscard]] std::vector<std::byte> EncodeMessage(const WelcomeMessage& _welcome);
[[nodiscard]] std::vector<std::byte> EncodeMessage(const Command& _command);
[[nodiscard]] std::vector<std::byte> EncodeMessage(const StateMessage& _state);
[[nodiscard]] std::vector<std::byte> EncodeMessage(const CatalogMessage& _catalog);
[[nodiscard]] std::vector<std::byte> EncodeMessage(const EventsMessage& _events);

// Moves _snapshot's catalogue out of it, leaving its lists empty, its sectors with only what changes in play, and its
// entities without the asteroid fields: the snapshot as it crosses the wire (ADR-060 decision 9).
[[nodiscard]] CatalogMessage TakeCatalog(Snapshot& _snapshot);

// Puts _catalog back into _snapshot, a snapshot TakeCatalog left without one, so that it is the snapshot the server built:
// the fields go among the entities in identifier order, as SnapshotInterpolator pairs them.
void PutCatalog(const CatalogMessage& _catalog, Snapshot& _snapshot);
void PutCatalog(CatalogMessage&& _catalog, Snapshot& _snapshot);

// Moves _snapshot's events out of it: what crosses reliably while its state may be abandoned (ADR-096).
[[nodiscard]] EventsMessage TakeEvents(Snapshot& _snapshot);

// Whether _events holds anything, which an events message that holds nothing need not carry.
[[nodiscard]] bool HasAny(const EventsMessage& _events) noexcept;

// Adds _events to _snapshot's, after what it holds: a snapshot handed on with the events of every tick since the last
// (ADR-096).
void PutEvents(EventsMessage&& _events, Snapshot& _snapshot);

// The message _bytes hold. Throws Neuron::Exception when they are not exactly one message, such as when they end early,
// run on, or hold a value no field can take.
[[nodiscard]] Message DecodeMessage(std::span<const std::byte> _bytes);

// The version a hello in _bytes says it speaks, read from its first field alone, so that a hello of another version,
// whose fields may differ, is still told apart from a broken one (ADR-060); none when the bytes do not start a hello.
[[nodiscard]] std::optional<std::uint32_t> PeekHelloVersion(std::span<const std::byte> _bytes) noexcept;

// The layout's hash a hello in _bytes carries, read from its second field alone, after the version; none when the bytes do
// not start a hello or end before the hash does.
[[nodiscard]] std::optional<std::uint64_t> PeekHelloLayout(std::span<const std::byte> _bytes) noexcept;
} // namespace Outpost

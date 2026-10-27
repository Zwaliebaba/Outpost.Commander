#include "pch.h"
#include "OverseerProtocol.h"

#include <concepts>
#include <type_traits>

#include "ByteFormat.h"
#include "WireFields.h"

namespace Outpost
{
// The fields of the overseer's messages, in the order each declares them, which is the order they cross in; the game's own
// types among them, such as a sector, cross as WireFields.h lists them (ADR-101).
template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, OverseerHelloMessage>
auto Fields(Self& _value)
{
  auto& [protocolVersion, layoutHash, token] = _value;
  return std::tie(protocolVersion, layoutHash, token);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, OverseerSeat>
auto Fields(Self& _value)
{
  auto& [player, ai] = _value;
  return std::tie(player, ai);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, OverseerWelcomeMessage>
auto Fields(Self& _value)
{
  auto& [wireLayoutHash, protocolVersion, stateVersion, ticksPerSecond, folderUtf8, seats, sectors, fields, recoveredSaveTick,
         replayedCommands] = _value;
  return std::tie(wireLayoutHash, protocolVersion, stateVersion, ticksPerSecond, folderUtf8, seats, sectors, fields, recoveredSaveTick,
                  replayedCommands);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, LinkFigures>
auto Fields(Self& _value)
{
  auto& [roundTripMicroseconds, leastRoundTripMicroseconds, packetsSent, packetsLost, congestionWindowBytes, backlogBytes, bytesSent] =
    _value;
  return std::tie(roundTripMicroseconds, leastRoundTripMicroseconds, packetsSent, packetsLost, congestionWindowBytes, backlogBytes,
                  bytesSent);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, SeatHealth>
auto Fields(Self& _value)
{
  auto& [player, play, link, linkTick, deputyTick, connection, orders, refused, ore, incomeHundredthsPerSecond, commandPoints, fleetCap,
         nodes, researched, restartTick] = _value;
  return std::tie(player, play, link, linkTick, deputyTick, connection, orders, refused, ore, incomeHundredthsPerSecond, commandPoints,
                  fleetCap, nodes, researched, restartTick);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, BattleHealth>
auto Fields(Self& _value)
{
  auto& [sector, startTick, players] = _value;
  return std::tie(sector, startTick, players);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, ProcessHealth>
auto Fields(Self& _value)
{
  auto& [workingSetBytes, privateBytes, cpuHundredths, threads, handles, uptimeSeconds, diskFreeBytes] = _value;
  return std::tie(workingSetBytes, privateBytes, cpuHundredths, threads, handles, uptimeSeconds, diskFreeBytes);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, HealthMessage>
auto Fields(Self& _value)
{
  auto& [wallMilliseconds, tick, tickMicroseconds, partMicroseconds, longestPart, droppedTicks, seats, battles, joins, connections,
         process] = _value;
  return std::tie(wallMilliseconds, tick, tickMicroseconds, partMicroseconds, longestPart, droppedTicks, seats, battles, joins, connections,
                  process);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, LogRecordsMessage>
auto Fields(Self& _value)
{
  auto& [lines] = _value;
  return std::tie(lines);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, ClosingMessage>
auto Fields(Self& _value)
{
  auto& [ending, messageUtf8, lastSaveTick] = _value;
  return std::tie(ending, messageUtf8, lastSaveTick);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, WatchSightMessage>
auto Fields(Self& _value)
{
  auto& [seat] = _value;
  return std::tie(seat);
}

template <typename Self>
  requires std::same_as<std::remove_const_t<Self>, ViewHeldMessage>
auto Fields(Self& _value)
{
  auto& [tick] = _value;
  return std::tie(tick);
}

// Each enumeration's last value, which ByteReader refuses any value past.
constexpr TickPart LastOf(TickPart) noexcept
{
  return TickPart::Overseer;
}

constexpr SeatPlay LastOf(SeatPlay) noexcept
{
  return SeatPlay::Ai;
}

constexpr SeatLink LastOf(SeatLink) noexcept
{
  return SeatLink::Gone;
}

constexpr ServerEnding LastOf(ServerEnding) noexcept
{
  return ServerEnding::Failed;
}

namespace
{
// 64-bit FNV-1a, as the game's layout is hashed (ADR-060).
constexpr std::uint64_t FNV_OFFSET = 0xCBF29CE484222325ull;
constexpr std::uint64_t FNV_PRIME = 0x100000001B3ull;

// Where the hello's version and layout hash stand: after the byte that says which message it is.
constexpr std::size_t HELLO_VERSION_START = 1;
constexpr std::size_t HELLO_LAYOUT_START = HELLO_VERSION_START + sizeof(std::uint32_t);

// Where T is among OverseerMessage's alternatives, which is the first byte of every message.
template <typename T, typename... Ts> consteval std::uint8_t KindOf(std::type_identity<std::variant<Ts...>>)
{
  constexpr std::array<bool, sizeof...(Ts)> MATCHES{std::same_as<T, Ts>...};
  return static_cast<std::uint8_t>(std::ranges::find(MATCHES, true) - MATCHES.begin());
}

constexpr std::uint8_t HELLO_KIND = KindOf<OverseerHelloMessage>(std::type_identity<OverseerMessage>{});

template <typename T> std::vector<std::byte> Encode(const T& _message)
{
  ByteWriter writer;
  writer.Put(KindOf<T>(std::type_identity<OverseerMessage>{}));
  writer.Put(_message);
  return writer.Take();
}

// The little-endian unsigned integer of _bytes at _start, which the caller has checked they hold.
template <std::unsigned_integral T> T ReadAt(std::span<const std::byte> _bytes, std::size_t _start) noexcept
{
  T value = 0;
  for (std::size_t i = 0; i < sizeof(T); ++i)
    value |= static_cast<T>(static_cast<T>(_bytes[_start + i]) << (8 * i));
  return value;
}
} // namespace

std::string OverseerLayout()
{
  std::string layout;
  ByteLayout::Describe<OverseerMessage>(layout);
  return layout + WireLayout();
}

std::uint64_t OverseerLayoutHash()
{
  static const std::uint64_t LAYOUT_HASH = []
  {
    std::uint64_t hash = FNV_OFFSET;
    for (const char character : OverseerLayout())
      hash = (hash ^ static_cast<std::uint8_t>(character)) * FNV_PRIME;
    return hash;
  }();
  return LAYOUT_HASH;
}

std::optional<std::string_view> DescribeOverseerClose(std::uint64_t _errorCode) noexcept
{
  switch (_errorCode)
  {
  case static_cast<std::uint64_t>(OverseerCloseReason::Finished):
    return "The server has stopped.";
  case static_cast<std::uint64_t>(OverseerCloseReason::WrongVersion):
    return "The server runs another build of the game.";
  case static_cast<std::uint64_t>(OverseerCloseReason::TokenRefused):
    return "The server refused the overseer's token: Overseer.json is not this universe's.";
  case static_cast<std::uint64_t>(OverseerCloseReason::MalformedMessage):
    return "The server could not read what the overseer sent it.";
  case static_cast<std::uint64_t>(OverseerCloseReason::Taken):
    return "Another overseer connected with the token.";
  default:
    return std::nullopt;
  }
}

LinkFigures FiguresOf(const Neuron::QuicStatistics& _statistics) noexcept
{
  return {.roundTripMicroseconds = _statistics.roundTripMicroseconds,
          .leastRoundTripMicroseconds = _statistics.leastRoundTripMicroseconds,
          .packetsSent = _statistics.packetsSent,
          .packetsLost = _statistics.packetsLost,
          .congestionWindowBytes = _statistics.congestionWindowBytes,
          .backlogBytes = _statistics.backlogBytes,
          .bytesSent = _statistics.bytesSent};
}

std::vector<std::byte> EncodeOverseerMessage(const OverseerHelloMessage& _hello)
{
  return Encode(_hello);
}

std::vector<std::byte> EncodeOverseerMessage(const OverseerWelcomeMessage& _welcome)
{
  return Encode(_welcome);
}

std::vector<std::byte> EncodeOverseerMessage(const HealthMessage& _health)
{
  return Encode(_health);
}

std::vector<std::byte> EncodeOverseerMessage(const LogRecordsMessage& _records)
{
  return Encode(_records);
}

std::vector<std::byte> EncodeOverseerMessage(const ClosingMessage& _closing)
{
  return Encode(_closing);
}

std::vector<std::byte> EncodeOverseerMessage(const WatchSightMessage& _sight)
{
  return Encode(_sight);
}

std::vector<std::byte> EncodeOverseerMessage(const ViewHeldMessage& _held)
{
  return Encode(_held);
}

OverseerMessage DecodeOverseerMessage(std::span<const std::byte> _bytes)
{
  ByteReader reader(_bytes, "A message of the overseer's");
  OverseerMessage message;
  reader.Get(message);
  reader.Finish();
  return message;
}

std::optional<std::uint32_t> PeekOverseerHelloVersion(std::span<const std::byte> _bytes) noexcept
{
  if (_bytes.size() < HELLO_LAYOUT_START || std::to_integer<std::uint8_t>(_bytes[0]) != HELLO_KIND)
    return std::nullopt;
  return ReadAt<std::uint32_t>(_bytes, HELLO_VERSION_START);
}

std::optional<std::uint64_t> PeekOverseerHelloLayout(std::span<const std::byte> _bytes) noexcept
{
  if (_bytes.size() < HELLO_LAYOUT_START + sizeof(std::uint64_t) || std::to_integer<std::uint8_t>(_bytes[0]) != HELLO_KIND)
    return std::nullopt;
  return ReadAt<std::uint64_t>(_bytes, HELLO_LAYOUT_START);
}
} // namespace Outpost

#pragma once

namespace Outpost
{
// The version of the messages below. A server refuses a client that speaks another (ADR-060).
inline constexpr std::uint32_t PROTOCOL_VERSION = 1;

// The application protocol a QUIC connection to the server negotiates (ADR-060).
inline constexpr std::string_view QUIC_APPLICATION_PROTOCOL = "outpost-commander/1";

// Why the server closed a client's connection: the QUIC application error code it closes with. Each is below
// Neuron::FRAMING_ERROR_CODE, which the channel closes with when the bytes are not messages at all.
enum class CloseReason : std::uint8_t
{
  Finished,
  WrongVersion,
  SeatRefused,
  MalformedMessage
};

// The first message a client sends: the version it speaks and the player whose seat it takes.
struct HelloMessage
{
  std::uint32_t protocolVersion = PROTOCOL_VERSION;
  PlayerId player;
};

// The server's answer once it has seated the client. Snapshots follow it.
struct WelcomeMessage
{
  PlayerId player;
};

// Everything that crosses a QUIC connection to the server. A client sends a hello and then commands, and the server a
// welcome and then snapshots.
using Message = std::variant<HelloMessage, WelcomeMessage, Command, Snapshot>;

// Each encodes one message as the bytes that cross the connection: which message it is, and then its fields in the order
// they are declared, little-endian (ADR-060).
[[nodiscard]] std::vector<std::byte> EncodeMessage(const HelloMessage& _hello);
[[nodiscard]] std::vector<std::byte> EncodeMessage(const WelcomeMessage& _welcome);
[[nodiscard]] std::vector<std::byte> EncodeMessage(const Command& _command);
[[nodiscard]] std::vector<std::byte> EncodeMessage(const Snapshot& _snapshot);

// The message _bytes hold. Throws Neuron::Exception when they are not exactly one message, such as when they end early,
// run on, or hold a value no field can take.
[[nodiscard]] Message DecodeMessage(std::span<const std::byte> _bytes);
} // namespace Outpost

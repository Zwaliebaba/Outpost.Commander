#include "pch.h"
#include "WireFormat.h"

#include <concepts>

#include "ByteFormat.h"
#include "WireFields.h"

namespace Outpost
{
namespace
{
// Where T is among Message's alternatives, which is the first byte of every message.
template <typename T, typename... Ts> consteval std::uint8_t KindOf(std::type_identity<std::variant<Ts...>>)
{
  constexpr std::array<bool, sizeof...(Ts)> MATCHES{std::same_as<T, Ts>...};
  return static_cast<std::uint8_t>(std::ranges::find(MATCHES, true) - MATCHES.begin());
}

template <typename T> std::vector<std::byte> Encode(const T& _message)
{
  ByteWriter writer;
  writer.Put(KindOf<T>(std::type_identity<Message>{}));
  writer.Put(_message);
  return writer.Take();
}
} // namespace

std::vector<std::byte> EncodeMessage(const HelloMessage& _hello)
{
  return Encode(_hello);
}

std::vector<std::byte> EncodeMessage(const WelcomeMessage& _welcome)
{
  return Encode(_welcome);
}

std::vector<std::byte> EncodeMessage(const Command& _command)
{
  return Encode(_command);
}

std::vector<std::byte> EncodeMessage(const Snapshot& _snapshot)
{
  return Encode(_snapshot);
}

std::optional<std::string_view> DescribeClose(std::uint64_t _errorCode) noexcept
{
  switch (_errorCode)
  {
  case static_cast<std::uint64_t>(CloseReason::Finished):
    return "The server ended the game.";
  case static_cast<std::uint64_t>(CloseReason::WrongVersion):
    return "The server runs another version of the game.";
  case static_cast<std::uint64_t>(CloseReason::SeatRefused):
    return "The server refused the seat: it is not open there, or the join file's token is not the seat's.";
  case static_cast<std::uint64_t>(CloseReason::MalformedMessage):
    return "The server could not read what the game sent it.";
  case static_cast<std::uint64_t>(CloseReason::SeatTaken):
    return "The seat was taken by another connection with its token.";
  default:
    return std::nullopt;
  }
}

std::optional<std::uint32_t> PeekHelloVersion(std::span<const std::byte> _bytes) noexcept
{
  constexpr std::size_t VERSION_END = 1 + sizeof(std::uint32_t);
  if (_bytes.size() < VERSION_END || static_cast<std::uint8_t>(_bytes[0]) != KindOf<HelloMessage>(std::type_identity<Message>{}))
    return std::nullopt;
  std::uint32_t version = 0;
  for (std::size_t i = 0; i < sizeof(std::uint32_t); ++i)
    version |= static_cast<std::uint32_t>(_bytes[1 + i]) << (8 * i);
  return version;
}

Message DecodeMessage(std::span<const std::byte> _bytes)
{
  ByteReader reader(_bytes, "A message from the network");
  Message message;
  reader.Get(message);
  reader.Finish();
  return message;
}
} // namespace Outpost

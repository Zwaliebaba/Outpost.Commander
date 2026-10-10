#include "pch.h"
#include "WireFormat.h"

#include <concepts>

#include "ByteFormat.h"
#include "WireFields.h"

namespace Outpost
{
namespace
{
// 64-bit FNV-1a, as a save's layout is hashed (ADR-077).
constexpr std::uint64_t FNV_OFFSET = 0xCBF29CE484222325ull;
constexpr std::uint64_t FNV_PRIME = 0x100000001B3ull;

// Where the hello's version and layout hash stand: after the byte that says which message it is.
constexpr std::size_t HELLO_VERSION_START = 1;
constexpr std::size_t HELLO_LAYOUT_START = HELLO_VERSION_START + sizeof(std::uint32_t);

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

std::string WireLayout()
{
  std::string layout;
  ByteLayout::Describe<Message>(layout);
  return layout;
}

std::uint64_t WireLayoutHash()
{
  static const std::uint64_t LAYOUT_HASH = []
  {
    std::uint64_t hash = FNV_OFFSET;
    for (const char character : WireLayout())
      hash = (hash ^ static_cast<std::uint8_t>(character)) * FNV_PRIME;
    return hash;
  }();
  return LAYOUT_HASH;
}

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
    return "The server runs another build of the game.";
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
  if (_bytes.size() < HELLO_LAYOUT_START || static_cast<std::uint8_t>(_bytes[0]) != KindOf<HelloMessage>(std::type_identity<Message>{}))
    return std::nullopt;
  std::uint32_t version = 0;
  for (std::size_t i = 0; i < sizeof(std::uint32_t); ++i)
    version |= static_cast<std::uint32_t>(_bytes[HELLO_VERSION_START + i]) << (8 * i);
  return version;
}

std::optional<std::uint64_t> PeekHelloLayout(std::span<const std::byte> _bytes) noexcept
{
  constexpr std::size_t LAYOUT_END = HELLO_LAYOUT_START + sizeof(std::uint64_t);
  if (_bytes.size() < LAYOUT_END || static_cast<std::uint8_t>(_bytes[0]) != KindOf<HelloMessage>(std::type_identity<Message>{}))
    return std::nullopt;
  std::uint64_t hash = 0;
  for (std::size_t i = 0; i < sizeof(std::uint64_t); ++i)
    hash |= static_cast<std::uint64_t>(_bytes[HELLO_LAYOUT_START + i]) << (8 * i);
  return hash;
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

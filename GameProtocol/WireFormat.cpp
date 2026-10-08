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

Message DecodeMessage(std::span<const std::byte> _bytes)
{
  ByteReader reader(_bytes, "A message from the network");
  Message message;
  reader.Get(message);
  reader.Finish();
  return message;
}
} // namespace Outpost

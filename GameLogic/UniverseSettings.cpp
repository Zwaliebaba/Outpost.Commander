#include "pch.h"
#include "UniverseSettings.h"

#include <charconv>
#include <limits>

namespace
{
std::uint16_t ReadPort(Neuron::JsonObjectReader& _reader, std::string_view _name)
{
  const std::int32_t port = _reader.Integer(_name, 1);
  if (port > std::numeric_limits<std::uint16_t>::max())
    Neuron::JsonFail(_reader.PathOf(_name), "expected a port no higher than 65535");
  return static_cast<std::uint16_t>(port);
}

std::uint64_t ReadSeed(Neuron::JsonObjectReader& _reader, std::string_view _name)
{
  const std::string text = _reader.String(_name);
  std::uint64_t seed = 0;
  const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), seed);
  if (text.empty() || error != std::errc{} || end != text.data() + text.size())
    Neuron::JsonFail(_reader.PathOf(_name), "expected a string of the seed's decimal digits");
  return seed;
}
} // namespace

Outpost::UniverseSettings Outpost::NewUniverseSettings(std::uint64_t _seed, std::size_t _seats)
{
  UniverseSettings settings{.seed = _seed};
  for (std::size_t seat = 0; seat < _seats; ++seat)
    settings.seats.push_back({.player = PlayerId{static_cast<std::uint32_t>(seat + 1)}, .token = NewSeatToken()});
  return settings;
}

std::string Outpost::WriteUniverseSettings(const UniverseSettings& _settings)
{
  std::string seats;
  for (std::size_t i = 0; i < _settings.seats.size(); ++i)
  {
    const UniverseSeat& seat = _settings.seats[i];
    const std::string which =
      seat.ai.empty() ? std::format("\"token\": \"{}\"", ToHex(seat.token)) : std::format("\"ai\": {}", Neuron::QuoteJson(seat.ai));
    seats += std::format("    {{ \"player\": {}, {} }}{}\n", seat.player.value, which, i + 1 < _settings.seats.size() ? "," : "");
  }
  return std::format("{{\n  \"address\": {},\n  \"port\": {},\n  \"overseerAddress\": {},\n  \"overseerPort\": {},\n  \"host\": {},\n  "
                     "\"seed\": \"{}\",\n  \"seats\": [\n{}  ]\n}}\n",
                     Neuron::QuoteJson(_settings.address), _settings.port, Neuron::QuoteJson(_settings.overseerAddress),
                     _settings.overseerPort, Neuron::QuoteJson(_settings.host), _settings.seed, seats);
}

Outpost::UniverseSettings Outpost::ReadUniverseSettings(std::string_view _text)
{
  try
  {
    const Neuron::JsonValue document = Neuron::ParseJson(_text);
    Neuron::JsonObjectReader reader(document, "");
    UniverseSettings settings;
    settings.address = reader.String("address");
    settings.port = ReadPort(reader, "port");
    if (reader.Optional("overseerAddress") != nullptr)
      settings.overseerAddress = reader.String("overseerAddress");
    if (reader.Optional("overseerPort") != nullptr)
      settings.overseerPort = ReadPort(reader, "overseerPort");
    settings.host = reader.String("host");
    if (settings.host.empty())
      Neuron::JsonFail(reader.PathOf("host"), "expected the name or address players reach the server at");
    settings.seed = ReadSeed(reader, "seed");
    settings.seats =
      Neuron::ReadJsonList<UniverseSeat>(reader, "seats",
                                         [](Neuron::JsonObjectReader& _seat)
                                         {
                                           UniverseSeat seat{.player = _seat.Identifier<PlayerId>("player")};
                                           const bool ai = _seat.Optional("ai") != nullptr;
                                           if (ai == (_seat.Optional("token") != nullptr))
                                             Neuron::JsonFail(_seat.Where(), "expected a seat's token, or the AI that plays it");
                                           if (ai)
                                           {
                                             seat.ai = _seat.String("ai");
                                             if (!std::ranges::contains(AI_DIFFICULTIES, seat.ai))
                                               Neuron::JsonFail(_seat.PathOf("ai"), "expected Easy, Normal or Hard");
                                             return seat;
                                           }
                                           const std::string token = _seat.String("token");
                                           if (!FromHex(token, seat.token))
                                             Neuron::JsonFail(_seat.PathOf("token"), "expected 32 hexadecimal digits");
                                           return seat;
                                         });
    if (settings.seats.empty())
      Neuron::JsonFail(reader.PathOf("seats"), "expected a seat at least");
    for (std::size_t i = 1; i < settings.seats.size(); ++i)
    {
      for (std::size_t j = 0; j < i; ++j)
      {
        if (settings.seats[i].player == settings.seats[j].player)
          Neuron::JsonFail(Neuron::JsonElementPath(reader.PathOf("seats"), i), "names a player another seat has");
      }
    }
    reader.Finish();
    return settings;
  }
  catch (const Neuron::Exception& exception)
  {
    throw Neuron::Exception(std::format("The universe's settings are not settings: {}", exception.what()));
  }
}

Outpost::JoinTicket Outpost::TicketFor(const UniverseSettings& _settings, const UniverseSeat& _seat,
                                       const Neuron::CertificateHash& _certificate)
{
  return {.address = {.host = _settings.host, .port = _settings.port, .certificate = _certificate, .token = _seat.token},
          .player = _seat.player};
}

Outpost::OverseerTicket Outpost::OverseerTicketFor(const UniverseSettings& _settings, const Neuron::CertificateHash& _certificate,
                                                   const SeatToken& _token)
{
  const std::string_view address = _settings.overseerAddress;
  std::string host(address);
  if (address.empty())
    host = std::string(DEFAULT_OVERSEER_ADDRESS);
  else if (address == Neuron::QuicListener::EVERY_ADDRESS || address == "0.0.0.0" || address == "::")
    host = _settings.host;
  return {.address = {.host = std::move(host), .port = _settings.overseerPort, .certificate = _certificate, .token = _token}};
}

std::string Outpost::JoinFileName(PlayerId _player)
{
  return std::format("Join-Player{}.json", _player.value);
}

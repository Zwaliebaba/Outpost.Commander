#include "pch.h"
#include "WorldSettings.h"

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

Outpost::WorldSettings Outpost::NewWorldSettings(std::uint64_t _seed, std::size_t _seats)
{
  WorldSettings settings{.seed = _seed};
  for (std::size_t seat = 0; seat < _seats; ++seat)
    settings.seats.push_back({.player = PlayerId{static_cast<std::uint32_t>(seat + 1)}, .token = NewSeatToken()});
  return settings;
}

std::string Outpost::WriteWorldSettings(const WorldSettings& _settings)
{
  std::string seats;
  for (std::size_t i = 0; i < _settings.seats.size(); ++i)
  {
    const WorldSeat& seat = _settings.seats[i];
    seats += std::format("    {{ \"player\": {}, \"token\": \"{}\" }}{}\n", seat.player.value, ToHex(seat.token),
                         i + 1 < _settings.seats.size() ? "," : "");
  }
  return std::format("{{\n  \"address\": {},\n  \"port\": {},\n  \"host\": {},\n  \"seed\": \"{}\",\n  \"seats\": [\n{}  ]\n}}\n",
                     Neuron::QuoteJson(_settings.address), _settings.port, Neuron::QuoteJson(_settings.host), _settings.seed, seats);
}

Outpost::WorldSettings Outpost::ReadWorldSettings(std::string_view _text)
{
  try
  {
    const Neuron::JsonValue document = Neuron::ParseJson(_text);
    Neuron::JsonObjectReader reader(document, "");
    WorldSettings settings;
    settings.address = reader.String("address");
    settings.port = ReadPort(reader, "port");
    settings.host = reader.String("host");
    if (settings.host.empty())
      Neuron::JsonFail(reader.PathOf("host"), "expected the name or address players reach the server at");
    settings.seed = ReadSeed(reader, "seed");
    settings.seats = Neuron::ReadJsonList<WorldSeat>(reader, "seats",
                                                     [](Neuron::JsonObjectReader& _seat)
                                                     {
                                                       WorldSeat seat{.player = _seat.Identifier<PlayerId>("player")};
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
    throw Neuron::Exception(std::format("The world's settings are not settings: {}", exception.what()));
  }
}

Outpost::JoinTicket Outpost::TicketFor(const WorldSettings& _settings, const WorldSeat& _seat, const Neuron::CertificateHash& _certificate)
{
  return {.address = {.host = _settings.host, .port = _settings.port, .certificate = _certificate, .token = _seat.token},
          .player = _seat.player};
}

std::string Outpost::JoinFileName(PlayerId _player)
{
  return std::format("Join-Player{}.json", _player.value);
}

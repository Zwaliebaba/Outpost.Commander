#pragma once

namespace Outpost
{
// The port a new world's server listens on, unless its owner names another.
inline constexpr std::uint16_t DEFAULT_WORLD_PORT = 45'000;
// A world's settings, in its folder.
inline constexpr std::string_view WORLD_SETTINGS_FILE = "World.json";

// One of a world's seats: its player, and the token its player takes it with (ADR-078).
struct WorldSeat
{
  PlayerId player;
  SeatToken token{};
};

// A world's settings (Phase 5 design §4, ADR-078), which its owner makes once, with the world, and the dedicated server
// reads each time it runs it: the address and port it listens on, the host its players reach it at, the seed the world
// was made with, and its seats.
struct WorldSettings
{
  // An IPv4 or IPv6 address; "0.0.0.0" listens on every interface.
  std::string address = "0.0.0.0";
  std::uint16_t port = DEFAULT_WORLD_PORT;
  // The name or address a player on another machine reaches the server at, which the join files name.
  std::string host = "localhost";
  std::uint64_t seed = 0;
  std::vector<WorldSeat> seats;
};

// Settings for a new world of _seats seats, players 1 to _seats, each with a new token.
[[nodiscard]] WorldSettings NewWorldSettings(std::uint64_t _seed, std::size_t _seats);

// The settings as World.json holds them: a JSON object, with the seed as a string of its decimal digits, since a JSON
// number cannot hold every 64-bit one, and each token in hexadecimal.
[[nodiscard]] std::string WriteWorldSettings(const WorldSettings& _settings);

// What World.json's text holds. Throws Neuron::Exception, naming what is wrong, when it is not settings: a member missing
// or unknown, a port out of range, a seed that is not a number, a token that is not 32 hexadecimal digits, no seat, or two
// seats for one player.
[[nodiscard]] WorldSettings ReadWorldSettings(std::string_view _text);

// The join file of _seat, whose server presents _certificate on the settings' port, at their host.
[[nodiscard]] JoinTicket TicketFor(const WorldSettings& _settings, const WorldSeat& _seat, const Neuron::CertificateHash& _certificate);

// The name of _player's join file, in the world's folder.
[[nodiscard]] std::string JoinFileName(PlayerId _player);
} // namespace Outpost

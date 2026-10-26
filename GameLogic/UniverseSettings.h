#pragma once

namespace Outpost
{
// The port a new universe's server listens on, unless its owner names another.
inline constexpr std::uint16_t DEFAULT_UNIVERSE_PORT = 45'000;
// A universe's settings, in its folder.
inline constexpr std::string_view UNIVERSE_SETTINGS_FILE = "Universe.json";

// The difficulties an AI empire plays at, each a settings file of the AI's (ADR-065).
inline constexpr std::array<std::string_view, 3> AI_DIFFICULTIES{"Easy", "Normal", "Hard"};

// One of a universe's seats: its player, and either the token its player takes it with (ADR-078), or the difficulty of the AI
// empire that plays it, one of AI_DIFFICULTIES (ADR-079 decision 7).
struct UniverseSeat
{
  PlayerId player;
  SeatToken token{};
  // Empty for a player's seat.
  std::string ai;
};

// A universe's settings (ADR-078 decision 2), which its owner makes once, with the universe, and the dedicated server
// reads each time it runs it: the address and port it listens on, the host its players reach it at, the seed the universe
// was made with, and its seats.
struct UniverseSettings
{
  // An IPv4 or IPv6 address, or "*", which listens on every interface of both, as a player naming localhost needs: it
  // reaches the server on IPv6.
  std::string address = std::string(Neuron::QuicListener::EVERY_ADDRESS);
  std::uint16_t port = DEFAULT_UNIVERSE_PORT;
  // The name or address a player on another machine reaches the server at, which the join files name.
  std::string host = "localhost";
  std::uint64_t seed = 0;
  std::vector<UniverseSeat> seats;
};

// Settings for a new universe of _seats seats, players 1 to _seats, each with a new token.
[[nodiscard]] UniverseSettings NewUniverseSettings(std::uint64_t _seed, std::size_t _seats);

// The settings as Universe.json holds them: a JSON object, with the seed as a string of its decimal digits, since a JSON
// number cannot hold every 64-bit one, each player's seat with its token in hexadecimal, and each AI empire's with its
// difficulty.
[[nodiscard]] std::string WriteUniverseSettings(const UniverseSettings& _settings);

// What Universe.json's text holds. Throws Neuron::Exception, naming what is wrong, when it is not settings: a member missing
// or unknown, a port out of range, a seed that is not a number, a token that is not 32 hexadecimal digits, a seat with
// both a token and an AI or neither, a difficulty there is none of, no seat, or two seats for one player.
[[nodiscard]] UniverseSettings ReadUniverseSettings(std::string_view _text);

// The join file of _seat, whose server presents _certificate on the settings' port, at their host.
[[nodiscard]] JoinTicket TicketFor(const UniverseSettings& _settings, const UniverseSeat& _seat,
                                   const Neuron::CertificateHash& _certificate);

// The name of _player's join file, in the universe's folder.
[[nodiscard]] std::string JoinFileName(PlayerId _player);
} // namespace Outpost

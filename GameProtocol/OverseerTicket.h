#pragma once

namespace Outpost
{
// The overseer's file, in a universe's folder (ADR-101).
inline constexpr std::string_view OVERSEER_TICKET_FILE = "Overseer.json";

// What Overseer.json holds: everything the overseer needs to reach a universe's server, as a join file holds what a player's
// game needs (ADR-078 decision 8). Whoever reads it has the overseer's powers.
struct OverseerTicket
{
  // The host the overseer reaches the server at, the port it listens for the overseer on, the certificate it presents, and
  // the universe's overseer token.
  ServerAddress address;
};

// The overseer file's text: a JSON object of the host, the port, the certificate's hash and the token, each hash and token in
// hexadecimal.
[[nodiscard]] std::string WriteOverseerTicket(const OverseerTicket& _ticket);

// What the overseer file's text holds. Throws Neuron::Exception, naming what is wrong, when it is not one.
[[nodiscard]] OverseerTicket ReadOverseerTicket(std::string_view _text);
} // namespace Outpost

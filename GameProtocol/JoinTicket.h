#pragma once

namespace Outpost
{
// What a join file holds (Phase 5 design §4, ADR-078): everything a player needs to take its seat in a world on another
// machine. The dedicated server writes one for each seat, and its owner hands it to the seat's player.
struct JoinTicket
{
  // The server's host and port, the certificate it presents, and the seat's token.
  ServerAddress address;
  PlayerId player;
};

// A join file's text: a JSON object of the host, the port, the certificate's hash and the token, each hash and token in
// hexadecimal, and the player.
[[nodiscard]] std::string WriteJoinTicket(const JoinTicket& _ticket);

// What a join file's text holds. Throws Neuron::Exception, naming what is wrong, when it is not one.
[[nodiscard]] JoinTicket ReadJoinTicket(std::string_view _text);
} // namespace Outpost

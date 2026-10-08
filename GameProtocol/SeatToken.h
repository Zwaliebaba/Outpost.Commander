#pragma once

namespace Outpost
{
// What a client shows to take its seat, besides its player: 128 random bits the server's owner hands its player, so that
// only that player takes it (Phase 5 design §4, ADR-078). The server's hello carries it, and a seat refuses any other.
using SeatToken = std::array<std::uint8_t, 16>;

// A new token from the system's source of randomness. Never in the simulation, which draws only from its own PRNG
// (ADR-009): a token is the server host's, not the world's.
[[nodiscard]] SeatToken NewSeatToken();

// _bytes as lowercase hexadecimal, two digits a byte, as the world's settings and a join file write a token or a
// certificate's hash.
[[nodiscard]] std::string ToHex(std::span<const std::uint8_t> _bytes);

// Reads hexadecimal of exactly two digits for each of _bytes, in either case, into them. False, with _bytes as they may
// have been left, when the text is not that.
[[nodiscard]] bool FromHex(std::string_view _text, std::span<std::uint8_t> _bytes) noexcept;
} // namespace Outpost

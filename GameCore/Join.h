#pragma once

#include "Entity.h"

#include "NeuronCore.h"

#include <cstddef>
#include <cstdint>

namespace Outpost
{

/// ADR-013's handshake, and it is the only thing on the wire that tells a client who it is -- and, since
/// M2.3, which field to draw: the seed and the player count are the generator's two inputs.
///
/// **IT LIVES IN `GameCore` BECAUSE BOTH SIDES SPEAK IT** (R9): the host answers it and the client
/// sends it, and neither of the two libraries that own a socket knows anything about players or
/// seeds. The framing above it is `Neuron::PacketHeader`'s, which knows nothing about either.
///
/// **NOTHING HERE IS RETRANSMITTED AND NOTHING IS ACKNOWLEDGED.** The client repeats its `Join`
/// until a reply arrives and the host answers every one, including a repeat from a client it has
/// already seated -- which is what makes a lost reply cost one retry rather than a slot.

/// A token is a NAME AND NOT A CREDENTIAL (ADR-013). It separates a client coming back from a
/// client arriving, which is an accident rather than an adversary; `TechnicalDesign.md` section 5
/// declines authentication outright and this does not quietly reintroduce it. A client holding the
/// match seed can compute these, and that is accepted rather than overlooked.
using SessionToken = std::uint64_t;

/// **ZERO MEANS "I HAVE NONE"**, which is why the host never issues it -- one value spends itself
/// to save a flag byte that would have had to agree with the token beside it.
inline constexpr SessionToken NO_SESSION_TOKEN = 0;

/// What the host decided. ZERO IS NOT A RESULT, for the reason `Neuron::PacketType` gives: a
/// zero-filled buffer must not decode into an acceptance.
enum class JoinResult : std::uint8_t
{
  /// A slot was free and this client is new to it.
  Accepted = 1,

  /// The token named a session this host still holds, and the client has its old slot back with a
  /// new endpoint. Distinct from `Accepted` because nothing else lets a suite -- or a player --
  /// tell a reconnect from a fresh seat.
  Rejoined = 2,

  /// No slot is free. `GameDesign.md` section 2 holds a disconnected player's slot indefinitely,
  /// so this does not become false by waiting, and the client stops retrying.
  MatchFull = 3
};

[[nodiscard]] constexpr bool IsKnown(JoinResult _result) noexcept
{
  return (_result == JoinResult::Accepted) || (_result == JoinResult::Rejoined) || (_result == JoinResult::MatchFull);
}

/// What a client sends. **It carries no slot preference and that is ADR-013's decision, not an
/// omission**: `GameDesign.md` section 2 configures the match on the host and R21 leaves no way to
/// type a slot number, so the host assigns and the client is told.
///
/// R8: a wire record, so plain fields and brace initialization.
struct Join
{
  /// Eight bytes of payload behind the four-byte header.
  static constexpr std::size_t SIZE_BYTES = 8;

  SessionToken token = NO_SESSION_TOKEN;

  [[nodiscard]] friend constexpr bool operator==(const Join&, const Join&) noexcept = default;
};

/// What the host answers with.
///
/// R8: a wire record.
struct JoinReply
{
  /// result 1, player 1, player count 1, token 8, seed 8.
  static constexpr std::size_t SIZE_BYTES = 19;

  JoinResult result = JoinResult::MatchFull;

  /// The slot, numbered from one. `NO_PLAYER` on a refusal (`GameCore/Entity.h`).
  PlayerId player = NO_PLAYER;

  /// **HOW MANY PLAYERS THE MATCH SEATS -- R23's SECOND INPUT, WHICH THE SEED ALONE IS NOT** (M2.3,
  /// ADR-013 amended). `GenerateField` copies a half at two players and a quarter at four, so the same
  /// seed is two different maps and a client that knew only the seed would draw one of them wrong. The
  /// host's configured count and not how many have joined: the field is fixed when the match begins.
  /// A count and not a `PlayerId`, though it has the same width -- `MAX_PLAYERS` is 254, so one byte
  /// holds every count a host can seat. Zero on a refusal.
  std::uint8_t playerCount = 0;

  /// The client keeps it and presents it next time. Zero on a refusal.
  SessionToken token = NO_SESSION_TOKEN;

  /// **R23's, AND WITH `playerCount` THE TWO THINGS A CLIENT CANNOT GET FROM A SNAPSHOT.** The client
  /// runs the asteroid generator itself and no map is ever transmitted, so without this there is no
  /// field to draw. Zero on a refusal, which is a legal seed and is never reached because a refused
  /// client draws nothing.
  std::uint64_t matchSeed = 0;

  [[nodiscard]] friend constexpr bool operator==(const JoinReply&, const JoinReply&) noexcept = default;
};

/// Why a join record would not decode. The same shape `CommandFault` has, and for the same reason:
/// a version mismatch is somebody running an old build and the rest are a corrupt or hostile
/// datagram worth a counter.
enum class JoinFault : std::uint8_t
{
  None,
  Truncated,
  VersionMismatch,
  /// A well-formed packet that is not the join record being looked for.
  WrongType,
  /// A result byte this build does not know.
  Malformed
};

/// **A MATCH HAS ENDED, AND ANOTHER HAS BEGUN** (M3.8, `OpenQuestions.md` Q70 as ruled): who won, sent to every
/// seated client for `MATCH_ENDED_REPEAT_TICKS` ticks. The seats are kept; a client that hears it shows the result,
/// clears what it derived from the old match, and joins again with its token -- and that join's reply is what
/// carries the new seed, so R23's "the seed arrives on the join reply and nowhere else" still holds.
///
/// R8: a wire record.
struct MatchEnded
{
  /// match number 2, winner 1, on the clock 1.
  static constexpr std::size_t SIZE_BYTES = 4;

  /// **WHICH MATCH ENDED**, counted from one in a host run. What lets a client tell the ten repeats of one end
  /// from the next match's, and take each once.
  std::uint16_t matchNumber = 0;

  /// `NO_PLAYER` for a draw.
  PlayerId winner = NO_PLAYER;

  /// Ended by the six-minute clock rather than by the last station standing (Q65).
  bool onClock = false;

  [[nodiscard]] friend constexpr bool operator==(const MatchEnded&, const MatchEnded&) noexcept = default;
};

/// Ten ticks, half a second, as a removal is repeated (ADR-003): a client misses it only if all ten are lost.
inline constexpr std::uint32_t MATCH_ENDED_REPEAT_TICKS = 10;

[[nodiscard]] bool Encode(const Join& _join, Neuron::ByteWriter& _writer) noexcept;
[[nodiscard]] JoinFault Decode(Neuron::ByteReader& _reader, Join& _outJoin) noexcept;

[[nodiscard]] bool Encode(const JoinReply& _reply, Neuron::ByteWriter& _writer) noexcept;
[[nodiscard]] JoinFault Decode(Neuron::ByteReader& _reader, JoinReply& _outReply) noexcept;

[[nodiscard]] bool Encode(const MatchEnded& _ended, Neuron::ByteWriter& _writer) noexcept;
[[nodiscard]] JoinFault Decode(Neuron::ByteReader& _reader, MatchEnded& _outEnded) noexcept;

} // namespace Outpost

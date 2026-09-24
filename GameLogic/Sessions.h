#pragma once

#include "CommandIntake.h"

#include "GameCore.h"
#include "NeuronServer.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace Outpost
{

/// ADR-013's slot table: who holds which slot, where they are reachable, and what token gets them
/// their slot back.
///
/// **IT IS PURE AND HOLDS NO SOCKET** (the split every other seam in this tree takes). `Host` owns
/// the transport and hands this an endpoint; everything here is arithmetic over a vector, so
/// `GameLogicTests` can drive a full join, a full match and a reconnect with nothing bound.
///
/// **R16 REACHES IT** -- `Scripts/CheckDeterminism.py` sweeps `GameLogic` -- and that is not a
/// formality here. The token generator is `Neuron::Pcg32` seeded from a salt the caller supplies, the
/// table is a vector walked in insertion order, and there is no clock. A session's assignment is not part
/// of the simulation; what a suite needs is that one salt issues the same tokens, and what a host needs is
/// that two runs do not (`SaltTokens`).
class Sessions
{
public:
  /// The most a host can seat (`GameCore/Entity.h`), which `CommandIntake` already sizes itself to. Named
  /// through it rather than restated, because two ceilings that must agree are a defect waiting for
  /// somebody to change one. **Not the design's four**: whether a count past four is allowed is the host's
  /// stress switch to decide (ADR-023), and this only bounds what a table can hold.
  static constexpr std::size_t MAX_PLAYERS = CommandIntake::MAX_PLAYERS;

  /// **THE FIRST CLAIM ANYONE HAS MADE ON PCG32's STREAM SPACE.** `NeuronCore/Pcg32.h` left the
  /// derivation to M2's generator and said so; this takes one number out of 2^63 and writes it
  /// down, so M2 can own the rest and not collide with it. Stream 0 is the default a caller gets
  /// for asking for nothing and is deliberately not this.
  static constexpr std::uint64_t TOKEN_STREAM = 1;

  /// One seat.
  ///
  /// R8: a public aggregate with no invariant of its own -- the invariants belong to the table.
  struct Session
  {
    /// Where this player is reachable **right now**. It changes on a reconnect and that is the
    /// whole reason a token exists: a client's ephemeral port does not survive a relaunch.
    Neuron::Endpoint endpoint{};

    /// Numbered from one (`GameCore/Entity.h`).
    PlayerId player = NO_PLAYER;

    SessionToken token = NO_SESSION_TOKEN;
  };

  /// **THE TOKEN STREAM BELONGS TO THE HOST RUN, NOT TO THE MATCH** (the 2026-09-23 review, B4). Seeds the
  /// stream every token is drawn from; `Begin` does not reseed it, so a later match on this table continues
  /// the stream rather than repeating it. The host calls this once, before the first join, with a salt its
  /// shell read from the wall clock -- the seam where R16 allows wall time -- and a suite passes a constant.
  ///
  /// Tokens used to be a pure function of the match seed and the join order. A host restarted on the same
  /// seed then issued last evening's tokens again, in the new join order, and two returning clients that
  /// arrived the other way round evicted each other from seat 1 every second with seat 2 never taken.
  void SaltTokens(std::uint64_t _salt) noexcept;

  /// Seats a match: how many slots it has and what seed every joining client is told. Clears
  /// whatever was held before, which is what starting another match means. **It does not reseed the
  /// token stream** (`SaltTokens`), so no token a previous match issued names a seat of this one.
  ///
  /// _playerCount above MAX_PLAYERS is clamped rather than refused -- `PlayerCountAllowed` refuses it
  /// before any caller gets here, and there is no useful thing to return.
  void Begin(std::size_t _playerCount, std::uint64_t _matchSeed) noexcept;

  /// **THE NEXT MATCH, WITH EVERY SEAT KEPT** (M3.8, `OpenQuestions.md` Q70): the seed every join is told changes and
  /// nothing else does, so a client that joins again with its token takes the seat it had -- sides never swap
  /// across a restart, and a human never takes an AI's base.
  void Reseed(std::uint64_t _matchSeed) noexcept;

  /// **THE LAST _count SEATS ARE THE AI'S** (M3.10, Q70's reserved AI seats): no client is ever given one, so a human
  /// never takes an AI's base. A match with every seat reserved refuses every join as full.
  void ReserveSeats(std::size_t _count) noexcept;

  /// ADR-013's handshake, and the only thing that hands out a slot.
  ///
  /// **MATCHED BY TOKEN, THEN BY ENDPOINT, THEN GIVEN A FREE SLOT.** The endpoint step is not
  /// redundant and it is not the reconnect path: it closes the window where the host has issued a
  /// token and the reply was lost, so the client's retry -- from the same live socket -- gets the
  /// seat it already has instead of a second one.
  ///
  /// Total. A refusal is a reply (`JoinResult::MatchFull`), because a client that is dropped
  /// cannot tell a full match from a host that is not running.
  [[nodiscard]] JoinReply Admit(const Join& _join, const Neuron::Endpoint& _endpoint) noexcept;

  /// Who is at this endpoint, or `NO_PLAYER`. **This is what the host trusts about a command**,
  /// rather than the player byte the packet carries (ADR-013).
  [[nodiscard]] PlayerId PlayerAt(const Neuron::Endpoint& _endpoint) const noexcept;

  /// Everyone seated, in the order they were, so each is sent its updates exactly once.
  [[nodiscard]] std::span<const Session> All() const noexcept
  {
    return m_sessions;
  }

  [[nodiscard]] std::size_t Count() const noexcept
  {
    return m_sessions.size();
  }

  [[nodiscard]] std::size_t PlayerCount() const noexcept
  {
    return m_playerCount;
  }

  [[nodiscard]] std::uint64_t MatchSeed() const noexcept
  {
    return m_matchSeed;
  }

  /// Forgets every seat. **This is not what a disconnect does** -- `GameDesign.md` section 2 holds
  /// a slot indefinitely and ADR-013 has a timeout forget an endpoint rather than a session. It is
  /// what closing the host does.
  void Clear() noexcept;

private:
  /// Sixty-four bits out of two draws, **and never zero**, which is what lets `NO_SESSION_TOKEN`
  /// mean "I have none" with no flag beside it.
  [[nodiscard]] SessionToken IssueToken() noexcept;

  [[nodiscard]] PlayerId LowestFreeSlot() const noexcept;

  /// The count as the join reply carries it. **A NARROWING THAT CANNOT LOSE ANYTHING**: `Begin` clamps to
  /// `MAX_PLAYERS`, which is 254, so every count this table can hold fits the byte.
  [[nodiscard]] std::uint8_t WirePlayerCount() const noexcept
  {
    static_assert(MAX_PLAYERS <= 255, "the join reply carries the player count in one byte");
    return static_cast<std::uint8_t>(m_playerCount);
  }

  /// A vector rather than a map, walked in insertion order: four sessions never justifies a hash,
  /// and a hashed container's order is exactly what R16 forbids reaching an outcome.
  std::vector<Session> m_sessions;

  std::size_t m_playerCount = 0;
  std::size_t m_reservedSeats = 0;
  std::uint64_t m_matchSeed = 0;

  /// `FieldHash` of the seed and the count, for every reply (Q76). Computed when either changes, not per join.
  std::uint64_t m_fieldHash = 0;

  /// Seeded by `SaltTokens` and never by `Begin`: one stream for the life of the table, so a suite that
  /// passes one salt can still pin a reconnect, and a host restart with a new salt issues new names.
  Neuron::Pcg32 m_tokens{0, TOKEN_STREAM};
};

} // namespace Outpost

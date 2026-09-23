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
/// formality here. The token generator is `Neuron::Pcg32` seeded from the match, the table is a
/// vector walked in insertion order, and there is no clock. A session's assignment is not part of
/// the simulation, but a host that issues different tokens on two runs of the same match is a host
/// whose suite cannot pin a reconnect.
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

  /// Seats a match: how many slots it has and what seed every joining client is told. Clears
  /// whatever was held before, which is what starting another match means.
  ///
  /// _playerCount above MAX_PLAYERS is clamped rather than refused -- `PlayerCountAllowed` refuses it
  /// before any caller gets here, and there is no useful thing to return.
  void Begin(std::size_t _playerCount, std::uint64_t _matchSeed) noexcept;

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

  /// A vector rather than a map, walked in insertion order: four sessions never justifies a hash,
  /// and a hashed container's order is exactly what R16 forbids reaching an outcome.
  std::vector<Session> m_sessions;

  std::size_t m_playerCount = 0;
  std::uint64_t m_matchSeed = 0;

  /// Reseeded by `Begin`, so two runs of one match issue the same tokens and a suite can pin them.
  Neuron::Pcg32 m_tokens{0, TOKEN_STREAM};
};

} // namespace Outpost

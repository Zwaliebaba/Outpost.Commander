#pragma once

#include "GameCore.h"

#include <cstdint>

namespace Outpost
{

/// ADR-013's handshake from the client's side: when to send a `Join`, and what the reply changed.
///
/// **IT SENDS NOTHING AND HOLDS NO SOCKET.** It answers *should a join go out now* and *what am I
/// after that reply*, which is the same split M0.19's interpolation and M0.21's tap resolution
/// take, and for the same reason: everything here is arithmetic over a millisecond clock, so a
/// suite pins a full join, a lost reply, a reconnect and a refusal without binding anything.
///
/// **THERE IS NO TIMEOUT AND THAT IS DELIBERATE.** A client retries for as long as it is running,
/// because the common failure is a host that has not been started yet -- and `GameDesign.md`
/// section 2 has the slot held indefinitely on the other side, so there is nothing that expires.
/// The one terminal answer is `MatchFull`, which does not become false by waiting.

/// Where the client is in the handshake.
enum class JoinPhase : std::uint8_t
{
  /// Asking. This is what `Interface.md` section 7's reconnecting overlay is drawn over.
  Joining,
  /// Seated. `Player()` is the answer and `MatchSeed()` is the field to draw.
  Joined,
  /// Refused, and retrying will not help.
  Refused
};

/// What the system panel says about the connection, which is the join's phase plus one fact the join
/// cannot know: **whether this client had a link and lost it.** `ClientFrame::Link` is the one place
/// that decides it.
enum class LinkState : std::uint8_t
{
  Joining,
  Linked,

  /// The host answered and had no slot (ADR-013). **The client shows it** -- M1.4's criterion -- in the
  /// reconnect overlay's block, since a refused client has nothing else to look at.
  Refused,

  /// Seated once, then silent (`Interface.md` section 7). Drawn until the first snapshot after the
  /// rejoin lands.
  Reconnecting
};

class JoinState
{
public:
  /// ADR-013's cadence. **Four a second, which is not tuned and does not need to be**: a join is
  /// 12 bytes, the loss that makes a retry necessary is measured in percent, and the only thing
  /// the interval decides is how long a player looks at an overlay after the host comes up.
  static constexpr std::uint64_t RETRY_INTERVAL_MILLISECONDS = 250;

  /// The token to present, from `LocalState` (`NeuronClient/SessionToken.h`). Zero when there is
  /// none, which is the first run and is ordinary.
  void Begin(SessionToken _token) noexcept;

  /// True when a `Join` should go out now, and it takes the send as having happened -- **so a
  /// caller that ignores the answer has silently stopped retrying.** That is why it is
  /// `[[nodiscard]]`.
  ///
  /// The first call after `Begin` always says yes: there is no reason to wait a quarter second
  /// before asking.
  [[nodiscard]] bool ShouldSend(std::uint64_t _nowMilliseconds) noexcept;

  /// **ASKS AGAIN WITHOUT FORGETTING WHO THIS CLIENT WAS.** A seated client goes back to `Joining` and
  /// presents the token it holds, so the host answers `Rejoined` into the same slot -- at a new endpoint
  /// if the old one is gone (ADR-013). Unlike `Begin`, the player and the seed are kept: the panels go on
  /// reading this player's block behind the overlay rather than blanking to nobody's.
  ///
  /// The first `ShouldSend` after it says yes at once, as after `Begin`. A client that is not seated is
  /// left alone -- a refused one stays refused and a joining one is already asking.
  void Rejoin() noexcept;

  /// What to put in it.
  [[nodiscard]] Join Outgoing() const noexcept
  {
    return Join{.token = m_token};
  }

  /// Folds in a reply. **A reply that arrives after the client is already seated is ignored unless
  /// it seats it somewhere else** -- the host answers every retry, so duplicates are the ordinary
  /// case rather than the exception, and a duplicate must not be allowed to look like a change.
  ///
  /// True when the token changed and is worth persisting. The caller writes it; nothing here
  /// touches a file.
  bool Accept(const JoinReply& _reply) noexcept;

  [[nodiscard]] JoinPhase Phase() const noexcept
  {
    return m_phase;
  }

  [[nodiscard]] bool IsJoined() const noexcept
  {
    return m_phase == JoinPhase::Joined;
  }

  /// Which player this client is, or `NO_PLAYER` until the host has said.
  [[nodiscard]] PlayerId Player() const noexcept
  {
    return m_player;
  }

  [[nodiscard]] SessionToken Token() const noexcept
  {
    return m_token;
  }

  /// R23's seed, which is the only thing on the wire the client cannot get from a snapshot. Zero
  /// until seated.
  [[nodiscard]] std::uint64_t MatchSeed() const noexcept
  {
    return m_matchSeed;
  }

  /// **`Rejoined` RATHER THAN `Accepted`**, kept because it is the difference between resuming a
  /// match and starting one, and because it is what a suite asserts a reconnect by.
  [[nodiscard]] bool Resumed() const noexcept
  {
    return m_resumed;
  }

  [[nodiscard]] std::uint32_t SentCount() const noexcept
  {
    return m_sent;
  }

private:
  SessionToken m_token = NO_SESSION_TOKEN;
  std::uint64_t m_matchSeed = 0;
  PlayerId m_player = NO_PLAYER;
  JoinPhase m_phase = JoinPhase::Joining;
  bool m_resumed = false;

  /// When the last one went out. **`m_sent` is what says whether it means anything** -- zero is a
  /// legal clock reading on the first frame, so a sentinel timestamp would have been a second
  /// thing to get wrong.
  std::uint64_t m_lastSentMilliseconds = 0;
  std::uint32_t m_sent = 0;
};

} // namespace Outpost

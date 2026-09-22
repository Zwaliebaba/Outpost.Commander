#include "pch.h"

#include "JoinState.h"

namespace Outpost
{

void JoinState::Begin(SessionToken _token) noexcept
{
  m_token = _token;
  m_matchSeed = 0;
  m_player = NO_PLAYER;
  m_phase = JoinPhase::Joining;
  m_resumed = false;
  m_lastSentMilliseconds = 0;
  m_sent = 0;
}

bool JoinState::ShouldSend(std::uint64_t _nowMilliseconds) noexcept
{
  if (m_phase != JoinPhase::Joining)
  {
    // Seated or refused. A seated client says nothing more: the host holds the slot, and a join
    // that kept going out would be answered forever by a host that answers every one.
    return false;
  }

  if (m_sent != 0)
  {
    // **SUBTRACTION AND NOT AN ADDITION ON THE STORED TIME.** `m_lastSentMilliseconds + interval`
    // is the reading that overflows; a clock that has gone backwards -- which a monotonic one
    // should not and a suite freely does -- lands on a huge unsigned difference here and sends,
    // which is the harmless answer of the two.
    if ((_nowMilliseconds - m_lastSentMilliseconds) < RETRY_INTERVAL_MILLISECONDS)
    {
      return false;
    }
  }

  m_lastSentMilliseconds = _nowMilliseconds;
  ++m_sent;
  return true;
}

void JoinState::Rejoin() noexcept
{
  if (m_phase != JoinPhase::Joined)
  {
    return;
  }

  m_phase = JoinPhase::Joining;
  m_lastSentMilliseconds = 0;
  m_sent = 0;
}

bool JoinState::Accept(const JoinReply& _reply) noexcept
{
  if (_reply.result == JoinResult::MatchFull)
  {
    if (m_phase == JoinPhase::Joined)
    {
      // A SEATED CLIENT IS NOT UNSEATED BY A LATE REFUSAL. The only way to see one after being
      // seated is a reply that was in flight before the seat existed, and acting on it would
      // throw away a slot the host still holds.
      return false;
    }

    m_phase = JoinPhase::Refused;
    m_player = NO_PLAYER;
    m_matchSeed = 0;

    // The token is cleared and the caller persists that, so "I was refused" and "I have never
    // joined" are one state on the next run rather than two.
    const bool changed = (m_token != NO_SESSION_TOKEN);
    m_token = NO_SESSION_TOKEN;
    return changed;
  }

  const bool tokenChanged = (_reply.token != m_token);

  m_token = _reply.token;
  m_matchSeed = _reply.matchSeed;
  m_player = _reply.player;
  m_phase = JoinPhase::Joined;
  m_resumed = (_reply.result == JoinResult::Rejoined);

  return tokenChanged;
}

} // namespace Outpost

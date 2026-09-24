#include "pch.h"

#include "Sessions.h"

namespace Outpost
{

void Sessions::Begin(std::size_t _playerCount, std::uint64_t _matchSeed) noexcept
{
  m_sessions.clear();
  m_playerCount = (_playerCount > MAX_PLAYERS) ? MAX_PLAYERS : _playerCount;
  m_matchSeed = _matchSeed;
}

void Sessions::Reseed(std::uint64_t _matchSeed) noexcept
{
  m_matchSeed = _matchSeed;
}

void Sessions::SaltTokens(std::uint64_t _salt) noexcept
{
  m_tokens = Neuron::Pcg32{_salt, TOKEN_STREAM};
}

void Sessions::Clear() noexcept
{
  m_sessions.clear();
}

SessionToken Sessions::IssueToken() noexcept
{
  for (;;)
  {
    // TWO STATEMENTS AND NOT ONE EXPRESSION. `(uint64(Next()) << 32) | Next()` leaves the two
    // calls unsequenced, so which draw lands in which half is the compiler's choice -- and two
    // builds that choose differently issue different tokens from one seed. That is precisely the
    // class of defect R16 exists to forbid, and it is invisible in a test that runs one build.
    const std::uint64_t high = m_tokens.Next();
    const std::uint64_t low = m_tokens.Next();
    const SessionToken token = (high << 32) | low;

    // Zero is what a client sends to say it has none, so it is the one value this cannot hand
    // out. One draw in 2^64; the loop is here because "never" has to be true rather than likely.
    if (token != NO_SESSION_TOKEN)
    {
      return token;
    }
  }
}

PlayerId Sessions::LowestFreeSlot() const noexcept
{
  // Players are numbered from one, and the lowest free one is handed out -- so a solo client is
  // always player one and a suite does not have to care what order two clients arrived in.
  for (std::size_t slot = 1; slot <= m_playerCount; ++slot)
  {
    const PlayerId candidate = static_cast<PlayerId>(slot);
    bool taken = false;
    for (const Session& session : m_sessions)
    {
      if (session.player == candidate)
      {
        taken = true;
        break;
      }
    }
    if (!taken)
    {
      return candidate;
    }
  }
  return NO_PLAYER;
}

JoinReply Sessions::Admit(const Join& _join, const Neuron::Endpoint& _endpoint) noexcept
{
  // A TOKEN THIS HOST ISSUED IS THE RECONNECT, and it is tried first because it is the only match
  // that survives the client's endpoint changing -- which it does on every relaunch.
  if (_join.token != NO_SESSION_TOKEN)
  {
    for (Session& session : m_sessions)
    {
      if (session.token == _join.token)
      {
        session.endpoint = _endpoint;
        return JoinReply{.result = JoinResult::Rejoined,
                         .player = session.player,
                         .playerCount = WirePlayerCount(),
                         .token = session.token,
                         .matchSeed = m_matchSeed};
      }
    }
  }

  // THE LOST-REPLY WINDOW, AND IT IS NOT THE RECONNECT PATH. The host has already issued a token
  // and the reply did not arrive, so the client retries from the same live socket with the token
  // it still does not have. Without this it would be given a second slot while the first is held
  // indefinitely (`GameDesign.md` section 2) -- at two slots, a match full with one player in it.
  //
  // It answers `Accepted` and not `Rejoined`, because from the client's side this IS its first
  // successful join. An unknown token from this endpoint lands here too, which is the same
  // client having kept a token from a host that is no longer running.
  for (const Session& session : m_sessions)
  {
    if (session.endpoint == _endpoint)
    {
      return JoinReply{.result = JoinResult::Accepted,
                       .player = session.player,
                       .playerCount = WirePlayerCount(),
                       .token = session.token,
                       .matchSeed = m_matchSeed};
    }
  }

  const PlayerId slot = LowestFreeSlot();
  if (slot == NO_PLAYER)
  {
    // A REPLY AND NOT A SILENCE. `GameDesign.md` section 2 holds a slot indefinitely, so this does
    // not become false by waiting and the client stops retrying -- which it can only do if it was
    // told. The seed and the count are not sent to a client that is not in the match.
    return JoinReply{.result = JoinResult::MatchFull, .player = NO_PLAYER, .token = NO_SESSION_TOKEN, .matchSeed = 0};
  }

  const SessionToken token = IssueToken();
  m_sessions.push_back(Session{.endpoint = _endpoint, .player = slot, .token = token});
  return JoinReply{
    .result = JoinResult::Accepted, .player = slot, .playerCount = WirePlayerCount(), .token = token, .matchSeed = m_matchSeed};
}

PlayerId Sessions::PlayerAt(const Neuron::Endpoint& _endpoint) const noexcept
{
  for (const Session& session : m_sessions)
  {
    if (session.endpoint == _endpoint)
    {
      return session.player;
    }
  }
  return NO_PLAYER;
}

} // namespace Outpost

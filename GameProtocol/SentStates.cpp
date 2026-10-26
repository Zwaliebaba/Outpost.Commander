#include "pch.h"
#include "SentStates.h"

std::vector<std::byte> Outpost::SentStates::Encode(const StateMessage& _state, std::optional<std::uint64_t> _held)
{
  std::vector<std::byte> whole = EncodeMessage(_state);
  if (!_held.has_value())
    return whole;
  std::erase_if(m_states, [held = *_held](const Snapshot& _sent) { return _sent.tick < held; });
  if (m_states.empty() || m_states.front().tick != *_held)
    return whole;
  std::vector<std::byte> change = EncodeMessage(
    StateChangeMessage{.orderedSent = _state.orderedSent, .baseTick = *_held, .change = ChangeOf(m_states.front(), _state.snapshot)});
  return change.size() < whole.size() ? change : whole;
}

void Outpost::SentStates::Keep(const Snapshot& _snapshot)
{
  m_states.push_back(_snapshot);
  if (m_states.size() > STATES_KEPT)
    m_states.erase(m_states.begin(), m_states.end() - static_cast<std::ptrdiff_t>(STATES_KEPT));
}

void Outpost::SentStates::Clear() noexcept
{
  m_states.clear();
}

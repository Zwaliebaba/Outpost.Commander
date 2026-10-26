#include "pch.h"
#include "HeldStates.h"

Outpost::StateMessage Outpost::HeldStates::Take(Message&& _message)
{
  StateMessage state;
  if (auto* whole = std::get_if<StateMessage>(&_message))
    state = std::move(*whole);
  else if (const auto* change = std::get_if<StateChangeMessage>(&_message))
  {
    const auto base = std::ranges::find(m_states, change->baseTick, &Snapshot::tick);
    if (base == m_states.end())
      throw Neuron::Exception(
        std::format("The server sent a change of the state of tick {}, which the client does not hold.", change->baseTick));
    state.orderedSent = change->orderedSent;
    state.snapshot = *base;
    PutChange(change->change, state.snapshot);
    // The server changes from the newest state it has heard the client holds, which is never older than this one.
    m_states.erase(m_states.begin(), base);
  }
  else
    throw Neuron::Exception("The server sent a message that is not a state where a state goes.");
  m_states.push_back(state.snapshot);
  while (m_states.size() > STATES_KEPT)
    m_states.pop_front();
  return state;
}

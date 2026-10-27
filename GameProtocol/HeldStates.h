#pragma once

namespace Outpost
{
// The states a client holds of those its server sent, as they cross the wire (TakeCatalog, TakeEvents), oldest first. The
// server sends a state as what changed since the newest one it has heard the client holds (ADR-098), so the client holds
// each until a change from a newer one comes, and STATES_KEPT at most, as many as the server keeps.
class HeldStates
{
public:
  // The state _message carries, whole, which is held from now on: a state, or a change put together with the held state it
  // is a change of, the older ones let go. The client tells the server it holds it (HeldMessage). Throws Neuron::Exception
  // for a change of a state not held, or one that is not a change of it, and for a message that is neither.
  [[nodiscard]] StateMessage Take(Message&& _message);

  [[nodiscard]] std::size_t Size() const noexcept
  {
    return m_states.size();
  }

private:
  std::vector<Snapshot> m_states;
};
} // namespace Outpost

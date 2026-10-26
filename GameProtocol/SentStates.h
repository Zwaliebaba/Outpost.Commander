#pragma once

namespace Outpost
{
// The states the server has sent one seat's connection, as they cross the wire (TakeCatalog, TakeEvents), oldest first,
// which it sends the next as what changed since the one the client holds (ADR-098). It keeps STATES_KEPT at most, and none
// older than the one the client holds. Touched by the thread that steps the server only.
class SentStates
{
public:
  // The message that carries _state: what changed since the state of tick _held, the newest the client holds, while that
  // state is kept and the change is the shorter; the whole state otherwise. The states older than _held go, since the
  // client holds it or a newer one.
  [[nodiscard]] std::vector<std::byte> Encode(const StateMessage& _state, std::optional<std::uint64_t> _held);

  // Keeps _snapshot as sent, the oldest going once STATES_KEPT are kept.
  void Keep(const Snapshot& _snapshot);

  // Lets every state go, as a seat taken by another connection does: its client holds none of them.
  void Clear() noexcept;

  [[nodiscard]] std::size_t Size() const noexcept
  {
    return m_states.size();
  }

private:
  std::deque<Snapshot> m_states;
};
} // namespace Outpost

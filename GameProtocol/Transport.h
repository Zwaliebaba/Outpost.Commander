#pragma once

namespace Outpost
{
// One player's connection to the server: commands go out and snapshots come in (ADR-002 decision 6). In the MVP it is
// the loopback, in-process queues in GameLogic; a network transport takes its place later without changing the types it
// carries (ADR-004).
class Transport : Neuron::NonCopyable
{
public:
  virtual ~Transport() = default;

  // Hands an order to the server, which applies or rejects it at the start of its next tick.
  virtual void Send(Command _command) = 0;

  // The snapshots that arrived since the last call, oldest first.
  [[nodiscard]] virtual std::vector<Snapshot> Receive() = 0;
};
} // namespace Outpost
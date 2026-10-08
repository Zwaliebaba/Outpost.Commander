#pragma once

namespace Outpost
{
// A player the server plays itself, on its own thread, rather than one that connects to it (Phase 5 design §6, ADR-079): a
// world's AI empire, which plays its seat throughout, or a seat's deputy, which plays it while its player is away. After
// each tick the server hands it its player's snapshot, and applies the orders it returns at the next tick as that
// player's, logged as a connection's are, so that a world replays from its seed and its log without it (ADR-009). It is
// touched only on the server's thread. Nothing of it is saved, so one made afresh picks up whatever state its player is in.
class HostedPlayer : Neuron::NonCopyable
{
public:
  virtual ~HostedPlayer() = default;

  // Its player's snapshot after a tick in which the seat is its: the orders to apply at the next tick.
  [[nodiscard]] virtual std::vector<Command> Play(const Snapshot& _snapshot) = 0;

  // Its player's snapshot after a tick in which its player plays the seat: a deputy watches what the player does, and
  // gives no order.
  virtual void Watch([[maybe_unused]] const Snapshot& _snapshot) {}
};
} // namespace Outpost

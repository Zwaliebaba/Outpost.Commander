#pragma once

namespace Outpost
{
struct ServerDesc
{
  // Every random draw in the simulation comes from one PRNG seeded with this, so that a match reproduces from its seed
  // and its command log (ADR-002 decision 8).
  std::uint64_t seed = 0;
};

// The authoritative simulation, as its clients see it (ADR-002). The in-process server is declared here and defined in
// GameLogic, so the executable can start it without including a server header.
class Server : Neuron::NonCopyable
{
public:
  virtual ~Server() = default;

  // A connection for one player, the human or the AI. The server keeps the other end.
  [[nodiscard]] virtual std::unique_ptr<Transport> Connect(PlayerId _player) = 0;

  // Tells the server how much wall time has passed. It runs the ticks that are now due, applying the commands that have
  // arrived and sending each connected player a snapshot per tick. This is where wall time becomes ticks (ADR-009).
  virtual void Advance(std::chrono::nanoseconds _elapsedWallTime) = 0;
};

// Loads the tuning data from the package (ADR-008) and starts a server with it. Throws Neuron::Exception when the data is
// missing or invalid.
[[nodiscard]] std::unique_ptr<Server> CreateInProcessServer(const ServerDesc& _desc);
} // namespace Outpost

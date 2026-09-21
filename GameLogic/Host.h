#pragma once

#include "CommandIntake.h"
#include "World.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Outpost
{

/// ADR-002's tick, in milliseconds, for the shell to hand to a schedule. IT IS A PLAIN INTEGER
/// AND NOT A `std::chrono` TYPE, because this library is the simulation's and R16 keeps wall time
/// out of it -- the seam is `Neuron::TickSchedule`, in the engine, driven by `Server.cpp`.
inline constexpr std::int64_t TICK_PERIOD_MILLISECONDS = 50;

/// The world as one player is to be told about it. ADR-003: the host serializes a per-player
/// entity set rather than the world, and in the MVP that set is everything -- but it is a list the
/// host builds, so visibility later changes this function and not the wire format.
///
/// Positions are quantized here and nowhere else, so the one lossy step in replication has one
/// home. Entities are emitted IN INDEX ORDER, which costs nothing and makes two hosts running the
/// same match produce byte-identical snapshots -- a property worth having even though nothing
/// requires it yet.
[[nodiscard]] Snapshot BuildSnapshot(const World& _world, const CommandIntake& _intake, std::uint32_t _tick, std::uint16_t _sequence,
                                     std::size_t _playerCount);

/// The match, and the loop over it. `Server.cpp` holds the shell; this holds everything a suite
/// could want to reach (R20, which names `Server` explicitly).
class Host
{
public:
  /// Below the Windows ephemeral range, which starts at 49,152, so a fixed bind cannot collide
  /// with the ephemeral port an outbound socket was handed.
  ///
  /// IT IS A DEFAULT, NOT A CONSTANT: `Server.cpp` takes `--port`, which is what M0.11 means by
  /// the port being configuration. ADR-008 owns the other half -- where the CLIENT looks for the
  /// host -- and M0.22 gives it the same treatment. M0.5's probe uses this number too, from its
  /// own constant, because there is one host and one port; when that scaffolding goes, this
  /// stays.
  static constexpr std::uint16_t DEFAULT_PORT = 49000;

  /// False when the socket could not be opened; LastFault carries the reason.
  [[nodiscard]] bool Open(std::uint16_t _port) noexcept;

  void Close() noexcept;

  [[nodiscard]] bool IsOpen() const noexcept
  {
    return m_transport.IsOpen();
  }

  [[nodiscard]] int LastFault() const noexcept
  {
    return m_transport.LastFault();
  }

  [[nodiscard]] Neuron::Endpoint BoundEndpoint() const noexcept
  {
    return m_transport.BoundEndpoint();
  }

  /// One pass: drain the socket and apply what arrived, advance the simulation by one tick, then
  /// encode a snapshot and send it to every client that has spoken. The order is fixed and it is
  /// `TechnicalDesign.md` section 2's: commands in, then the tick, then what the tick produced.
  void RunOneTick();

  [[nodiscard]] World& MutableWorld() noexcept
  {
    return m_world;
  }

  [[nodiscard]] const World& CurrentWorld() const noexcept
  {
    return m_world;
  }

  /// The snapshot sequence, which a client watches advance by exactly one per snapshot.
  [[nodiscard]] std::uint16_t SnapshotSequence() const noexcept
  {
    return m_snapshotSequence;
  }

  [[nodiscard]] std::size_t ClientCount() const noexcept
  {
    return m_clients.size();
  }

  /// Datagrams that arrived and were not a command packet this build could use.
  [[nodiscard]] std::uint64_t RejectedDatagramCount() const noexcept
  {
    return m_rejectedDatagrams;
  }

private:
  struct Client
  {
    Neuron::Endpoint endpoint{};
    PlayerId player = NO_PLAYER;
  };

  void DrainAndApply();
  void SendSnapshots();

  World m_world;
  CommandIntake m_intake;
  Neuron::WinsockTransport m_transport;

  /// A vector rather than a map, and iterated in insertion order: four clients never justifies a
  /// hash, and a hashed container's order is exactly what R16 forbids reaching an outcome.
  std::vector<Client> m_clients;

  std::uint32_t m_tick = 0;
  std::uint16_t m_snapshotSequence = 0;
  std::uint64_t m_rejectedDatagrams = 0;
};

} // namespace Outpost

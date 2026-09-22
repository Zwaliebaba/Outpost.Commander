#pragma once

#include "BuildSystem.h"
#include "CommandIntake.h"
#include "Sessions.h"
#include "World.h"

#include <cstddef>
#include <cstdint>
#include <span>

namespace Outpost
{

/// **THE ONE FIXED SEED M0 AND M1 RUN** (`GameDesign.md` section 3: one seed with a hand-checked
/// layout). ADR-013 makes the seed the host's and configuration -- `Server.cpp` takes `--seed` --
/// and this is what it defaults to. It is an arbitrary number and is meant to be: what matters is
/// that it does not move, because the layout it produces has been looked at by eye.
///
/// M1.5's `GameCore/Layout.h` is the first thing to consume it, and may be where it ends up.
inline constexpr std::uint64_t DEFAULT_MATCH_SEED = 20260922;

/// The world as one player is to be told about it. ADR-003: the host serializes a per-player
/// entity set rather than the world, and in the MVP that set is everything -- but it is a list the
/// host builds, so visibility later changes this function and not the wire format.
///
/// Positions are quantized here and nowhere else, so the one lossy step in replication has one
/// home. Entities are emitted IN INDEX ORDER, which costs nothing and makes two hosts running the
/// same match produce byte-identical snapshots -- a property worth having even though nothing
/// requires it yet.
[[nodiscard]] Snapshot BuildSnapshot(const World& _world, const CommandIntake& _intake, const BuildSystem& _build, std::uint32_t _tick,
                                     std::uint16_t _sequence, std::size_t _playerCount);

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

  /// Seats a match at `DEFAULT_MATCH_SEED`. **A `Host` is joinable the moment it is constructed**,
  /// so a suite that never calls `BeginMatch` still has slots to hand out.
  Host();

  /// Starts a match: **an empty world, `GameCore/Layout.h`'s stations placed on it, no seats and a
  /// new seed.** That is what `Interface.md` section 7 has the host do at victory, and it is what a
  /// host does when it starts.
  ///
  /// **IT RESETS THE WORLD, AND M1.5 IS WHY.** ADR-013 introduced this function for the seed alone
  /// and said the world was M3's; placing the layout changed that, because a `BeginMatch` that
  /// placed stations without clearing would place a second set on the second call. Resetting is
  /// both simpler and what M3's restart wants anyway.
  void BeginMatch(std::uint64_t _matchSeed);

  [[nodiscard]] std::uint64_t MatchSeed() const noexcept
  {
    return m_sessions.MatchSeed();
  }

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

  [[nodiscard]] BuildSystem& MutableBuild() noexcept
  {
    return m_build;
  }

  [[nodiscard]] const BuildSystem& CurrentBuild() const noexcept
  {
    return m_build;
  }

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

  /// Clients that have joined. **Not endpoints that have spoken** -- that is what it counted
  /// before ADR-013, and the difference is the whole point of the join.
  [[nodiscard]] std::size_t ClientCount() const noexcept
  {
    return m_sessions.Count();
  }

  [[nodiscard]] const Sessions& CurrentSessions() const noexcept
  {
    return m_sessions;
  }

  /// Datagrams that arrived and were not something this build could use.
  [[nodiscard]] std::uint64_t RejectedDatagramCount() const noexcept
  {
    return m_rejectedDatagrams;
  }

  /// Command packets from an endpoint with no session. **This is the teeth of ADR-013**: before
  /// it, any endpoint that sent a command was believed and started receiving the world.
  [[nodiscard]] std::uint64_t UnjoinedCommandCount() const noexcept
  {
    return m_unjoinedCommands;
  }

  /// Command packets whose player byte disagreed with the session that sent them. The host
  /// believes the session and counts this, which turns a stale packet from a previous match from
  /// a silence into a number somebody can look at (ADR-013).
  [[nodiscard]] std::uint64_t MisaddressedCommandCount() const noexcept
  {
    return m_misaddressedCommands;
  }

  [[nodiscard]] std::uint64_t JoinCount() const noexcept
  {
    return m_joins;
  }

private:
  void DrainAndApply();
  void SendSnapshots();

  /// One join, answered. Split out because it is the one arm of the drain that WRITES to the
  /// socket, and a drain loop that sends inside itself is worth being able to see on its own.
  void AnswerJoin(std::span<const std::byte> _datagram, const Neuron::Endpoint& _sender) noexcept;

  World m_world;
  CommandIntake m_intake;
  BuildSystem m_build;
  Sessions m_sessions;
  Neuron::WinsockTransport m_transport;

  std::uint32_t m_tick = 0;
  std::uint16_t m_snapshotSequence = 0;
  std::uint64_t m_rejectedDatagrams = 0;
  std::uint64_t m_unjoinedCommands = 0;
  std::uint64_t m_misaddressedCommands = 0;
  std::uint64_t m_joins = 0;
};

} // namespace Outpost

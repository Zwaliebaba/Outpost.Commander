#pragma once

#include "Accumulator.h"
#include "BuildSystem.h"
#include "CommandIntake.h"
#include "Sessions.h"
#include "Economy.h"
#include "MiningSystem.h"
#include "WeaponSystem.h"
#include "DeathSystem.h"
#include "Victory.h"
#include "StubAi.h"
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

/// **WHETHER A HOST MAY SEAT THIS MANY** (ADR-023). One to four always -- the game's number, and Q27 ships
/// two -- and up to every `PlayerId` there is only when the host was started in a stress configuration, so a
/// real match can never be configured past the design by accident. Zero is never allowed: a match with no
/// slots seats nobody, which is a mistake rather than a configuration.
[[nodiscard]] constexpr bool PlayerCountAllowed(std::size_t _playerCount, bool _stress) noexcept
{
  return (_playerCount >= 1) && (_playerCount <= (_stress ? MAX_PLAYERS : MATCH_PLAYERS));
}

/// One player's own block, which is the only player block an update carries (ADR-024). M1.6: all four
/// fields carry meaning -- the two build bytes are Q21's whole answer to the queue that had no wire record.
[[nodiscard]] PlayerBlock PlayerBlockFor(const CommandIntake& _intake, const BuildSystem& _build, PlayerId _player) noexcept;

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

  /// **Q27 SHIPS TWO THROUGH M3**, and this is that number as a default rather than a constant. `Server.cpp`
  /// takes `--players`, and past four only with `--stress` (ADR-023).
  static constexpr std::size_t DEFAULT_PLAYER_COUNT = 2;

  /// Seats a two-player match at `DEFAULT_MATCH_SEED`. **A `Host` is joinable the moment it is constructed**,
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
  ///
  /// **_playerCount IS TRUSTED HERE AND CHECKED BEFORE IT.** The caller asks `PlayerCountAllowed` with its
  /// stress switch; this seats whatever it is given, clamped only to what a table can hold.
  void BeginMatch(std::uint64_t _matchSeed, std::size_t _playerCount = DEFAULT_PLAYER_COUNT);

  /// **THE MATCH RESTARTS** (M3.8, `Interface.md` section 7, Q70): what the tick does on the tick a match ends. The
  /// same reset as `BeginMatch`, on the next seed, **with every seat kept**, and a `MatchEnded` to every seated
  /// client for the next `MATCH_ENDED_REPEAT_TICKS` ticks. Public so a suite can end a match without playing one.
  void Restart();

  /// **THE LAST _count SEATS PLAY THEMSELVES** (M3.10): the stub AI takes them, and no client is ever given one. Kept
  /// across every restart. `Server --ai` sets it; zero, the default, is a host of humans only.
  void SetAiSeats(std::size_t _count) noexcept;

  [[nodiscard]] const AiSeats& Ai() const noexcept
  {
    return m_ai;
  }

  /// The seed the match after one on _matchSeed plays: SplitMix64's finalizer over it, so a run of restarts from
  /// one starting seed is the same run of maps on every host (R16), and never the same map twice in a row.
  [[nodiscard]] static std::uint64_t NextMatchSeed(std::uint64_t _matchSeed) noexcept;

  /// How the last match ended, and how many have. Zero before the first ends.
  [[nodiscard]] const MatchEnded& LastEnded() const noexcept
  {
    return m_lastEnded;
  }

  /// **ONCE PER HOST RUN, BEFORE THE FIRST JOIN** (the 2026-09-23 review, B4): seeds the session-token stream,
  /// which no `BeginMatch` reseeds. `Server` passes a value its shell read from the wall clock, so a host
  /// restarted on the same seed does not hand last run's tokens out again in a new order; a suite that never
  /// calls it gets the fixed stream a suite wants.
  void SaltTokens(std::uint64_t _salt) noexcept
  {
    m_sessions.SaltTokens(_salt);
  }

  /// How many slots this match has.
  [[nodiscard]] std::size_t PlayerCount() const noexcept
  {
    return m_sessions.PlayerCount();
  }

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

  /// One pass: drain the socket and apply what arrived, advance the simulation by one tick, then fill
  /// each seated client's updates from the accumulator and send them (ADR-024). The order is fixed and it is
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

  /// M2.6's loop, and this tick's deliveries from it.
  [[nodiscard]] const MiningSystem& CurrentMining() const noexcept
  {
    return m_mining;
  }

  /// M3.2's weapons, and this tick's fire events from them.
  [[nodiscard]] const WeaponSystem& CurrentWeapons() const noexcept
  {
    return m_weapons;
  }

  /// M3.4's deaths, and what died this tick.
  [[nodiscard]] const DeathSystem& CurrentDeaths() const noexcept
  {
    return m_deaths;
  }

  /// M3.7's elimination and victory, and how this match ended if it has.
  [[nodiscard]] const Victory& CurrentVictory() const noexcept
  {
    return m_victory;
  }

  [[nodiscard]] World& MutableWorld() noexcept
  {
    return m_world;
  }

  [[nodiscard]] const World& CurrentWorld() const noexcept
  {
    return m_world;
  }

  /// Updates sent, across every client, since the host was constructed. A counter for a suite and for
  /// the harness; the transport sequence each client sees is per client and lives in the accumulator.
  [[nodiscard]] std::uint64_t UpdatesSent() const noexcept
  {
    return m_updatesSent;
  }

  [[nodiscard]] const Accumulator& CurrentAccumulator() const noexcept
  {
    return m_accumulator;
  }

  /// The tick the simulation has reached, which is the tick every update sent after it describes.
  [[nodiscard]] std::uint32_t CurrentTick() const noexcept
  {
    return m_tick;
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
  /// The reset both `BeginMatch` and `Restart` do: everything but the seats.
  void ResetMatch(std::uint64_t _matchSeed, std::size_t _players);

  void DrainAndApply();
  void SendUpdates();

  /// One join, answered. Split out because it is the one arm of the drain that WRITES to the
  /// socket, and a drain loop that sends inside itself is worth being able to see on its own.
  void AnswerJoin(std::span<const std::byte> _datagram, const Neuron::Endpoint& _sender) noexcept;

  World m_world;
  CommandIntake m_intake;
  BuildSystem m_build;
  MiningSystem m_mining;
  WeaponSystem m_weapons;
  DeathSystem m_deaths;
  Victory m_victory;
  AiSeats m_ai;
  std::size_t m_aiSeats = 0;
  Economy m_economy;
  Sessions m_sessions;
  Accumulator m_accumulator;
  Neuron::WinsockTransport m_transport;

  std::uint32_t m_tick = 0;
  std::uint64_t m_updatesSent = 0;
  std::uint64_t m_rejectedDatagrams = 0;
  std::uint64_t m_unjoinedCommands = 0;
  std::uint64_t m_misaddressedCommands = 0;
  std::uint64_t m_joins = 0;

  MatchEnded m_lastEnded{};

  /// Ticks the last `MatchEnded` still has to be sent for.
  std::uint32_t m_matchEndedRepeats = 0;
};

} // namespace Outpost

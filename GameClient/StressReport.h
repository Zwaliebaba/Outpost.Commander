#pragma once

#include "ClientFrame.h"
#include "Interpolation.h"
#include "JoinState.h"

#include "GameCore.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Outpost
{

/// ADR-022's three roles. **A role is what a bot is for, not how well it plays**: a player loads the command
/// path and the accumulator, a churner the rejoin, and a flooder every refusal path.
enum class BotRole : std::uint8_t
{
  Player,
  Churner,
  Flooder
};

inline constexpr std::size_t BOT_ROLE_COUNT = 3;

/// **THE HARNESS COUNTS ITS OWN TICKS AT THE HOST'S RATE**, so a run's length and every schedule's interval
/// read in the same unit as the updates it measures. It is a count kept by the harness loop and never the
/// host's tick: a harness that falls behind shows up as update tick gaps, which is the figure that finds
/// its ceiling (ADR-022).
inline constexpr std::uint64_t HARNESS_TICK_MILLISECONDS = SNAPSHOT_INTERVAL_MILLISECONDS;

/// **HOW MANY BOTS ONE PROCESS WILL START, AND IT IS NOT MEASURED YET.** ADR-022 has the harness refuse to
/// start more than it has been shown to sustain, so that its own bottleneck is not reported as the host's.
/// Nothing has been shown yet: this is a provisional bound, a hundred players and a few more, and the first
/// run on the Windows machine replaces it with the measured ceiling.
inline constexpr std::uint32_t HARNESS_BOT_CEILING = 128;

/// **ONE STREAM PER BOT AND ROLE, FROM ONE RUN SEED** (`Neuron::Pcg32`). Two bots, or a bot's policy and its
/// schedule, never draw from one stream, so adding a flooder to a run does not change what any player does.
[[nodiscard]] constexpr std::uint64_t BotStream(BotRole _role, std::uint32_t _botIndex) noexcept
{
  return (static_cast<std::uint64_t>(_botIndex) * BOT_ROLE_COUNT) + static_cast<std::uint64_t>(_role);
}

/// What one role saw across a run, summed over its bots. R8: a public aggregate.
struct RoleCounters
{
  std::uint32_t bots = 0;

  /// Seats: a join answered `Accepted`, one answered `Rejoined`, and `MatchFull`. Counted on the change of
  /// phase, not per reply, because the host answers every retry and duplicates are ordinary.
  std::uint64_t seated = 0;
  std::uint64_t resumed = 0;
  std::uint64_t refused = 0;

  std::uint64_t updates = 0;
  std::uint64_t updatesLost = 0;
  std::uint64_t recordsRefused = 0;
  std::uint64_t datagramsFaulted = 0;
  std::uint64_t linkLosses = 0;

  /// Between consecutive update ticks, as each bot's newest tick advanced. **A host that falls behind shows
  /// here first**, and so does a harness that cannot drain its bots fast enough.
  std::uint64_t tickGaps = 0;
  std::uint64_t tickGapTotal = 0;
  std::uint32_t tickGapMax = 0;

  /// ADR-024's figure: ticks between two records of one entity, at one client.
  std::uint64_t refreshes = 0;
  std::uint64_t refreshTicksTotal = 0;
  std::uint32_t refreshTicksMax = 0;

  std::uint64_t commandsSent = 0;
  std::uint64_t commandsAcknowledged = 0;
  std::uint64_t acknowledgeMillisecondsTotal = 0;
  std::uint64_t acknowledgeMillisecondsMax = 0;

  std::uint64_t datagramsSent = 0;
  /// Sends the transport refused because the previous one was still in flight -- which bounds a flooder's
  /// real rate, and is reported so that bound is not mistaken for the host's.
  std::uint64_t sendsSkipped = 0;

  std::uint64_t rejoins = 0;
};

/// **WHAT THE HARNESS SAW ON THE WIRE, NOT WHAT THE HOST SAYS** (ADR-022), per role, as text.
///
/// It holds the per-bot memory the counters need -- the last newest tick, the last loss count, the last join
/// phase -- so the harness loop hands it what a bot's frame reported and does no arithmetic of its own
/// (R20). It reads no clock: every time it is given was measured by the caller.
class StressReport
{
public:
  /// One entry per bot, in bot index order.
  explicit StressReport(const std::vector<BotRole>& _roles);

  /// After a drain that folded in join replies: counts a change of phase once.
  void NoteJoin(std::uint32_t _bot, const JoinState& _join) noexcept;

  /// After every drain.
  void NoteDrain(std::uint32_t _bot, const ClientFrame::DrainResult& _drained, const ReplicaStore& _replicas) noexcept;

  void NoteSent(std::uint32_t _bot, std::uint32_t _datagrams, std::uint32_t _commands, std::uint32_t _skipped) noexcept;

  /// One command acknowledged, with how long it waited.
  void NoteAcknowledged(std::uint32_t _bot, std::uint64_t _milliseconds) noexcept;

  void NoteRejoin(std::uint32_t _bot) noexcept;

  [[nodiscard]] const RoleCounters& Counters(BotRole _role) const noexcept
  {
    return m_counters[static_cast<std::size_t>(_role)];
  }

  /// The run's report. **Integer arithmetic only**, means in hundredths, so the same counters format to the
  /// same bytes on every machine and a report can be compared with a diff.
  [[nodiscard]] std::string Format(std::uint64_t _harnessTicks) const;

private:
  struct BotMemory
  {
    BotRole role = BotRole::Player;
    JoinPhase phase = JoinPhase::Joining;
    std::uint32_t newestTick = 0;
    std::uint64_t lostCount = 0;
  };

  [[nodiscard]] RoleCounters& CountersOf(std::uint32_t _bot) noexcept;

  std::vector<BotMemory> m_bots;
  std::array<RoleCounters, BOT_ROLE_COUNT> m_counters{};
};

/// A mean in hundredths, as text: `1.05`. Zero when there is nothing to divide.
[[nodiscard]] std::string FormatHundredths(std::uint64_t _total, std::uint64_t _count);

[[nodiscard]] const char* RoleName(BotRole _role) noexcept;

} // namespace Outpost

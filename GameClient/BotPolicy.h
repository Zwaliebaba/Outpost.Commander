#pragma once

#include "ReplicaStore.h"

#include "GameCore.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Outpost
{

/// **A STRESS BOT'S PLAYER DECISIONS** (ADR-022), and the only place any exist. It is not the AI of
/// `GameDesign.md` section 8 and it does not try to win: it exists to put a person's worth of orders on the
/// host's command path, from something that is not a person.
///
/// **A FUNCTION OF WHAT THIS BOT WAS SENT, ITS PLAYER AND ITS OWN STREAM.** It reads the replica store and
/// nothing else, so it cannot know what it was not told (R19), and it is paced by the newest update's tick
/// rather than by a clock, so a suite feeds it updates and asserts what comes out.
///
/// **IT OWNS ITS ORDERS UNTIL THE HOST HAS APPLIED THEM**, which is ADR-003's reliability: every command is
/// repeated in every outgoing packet until this player's block acknowledges its sequence. So it numbers its
/// own commands and keeps the outstanding ones, with when each was sent, which is also what the report's
/// command-to-acknowledge time is measured from.
class BotPolicy
{
public:
  /// **ONCE A SECOND, AT THE HOST'S TWENTY TICKS.** A person's pace of orders, not a machine's: the harness
  /// exists to find what the host does under many players, and a bot ordering every tick would be measuring
  /// a load no player can make. Not tuned.
  static constexpr std::uint32_t DECISION_INTERVAL_TICKS = 20;

  /// **ONE DECISION IN TEN CANCELS WHAT IS BUILDING**, so the refund path is exercised a few times a minute
  /// per bot without the bot never finishing anything.
  static constexpr std::uint32_t CANCEL_ONE_IN = 10;

  /// **ONE DECISION IN TWO MOVES SHIPS.** Enough that the host's movement is always busy and the accumulator
  /// always has changed entities to score.
  static constexpr std::uint32_t MOVE_ONE_IN = 2;

  /// **A MOVE ORDERS AT MOST THIS MANY SHIPS**, each owned ship joining with even odds. Well under the peak
  /// selection of 110 that ADR-003 sized a command for, so one command fits a packet with the others
  /// outstanding.
  static constexpr std::size_t MAX_MOVE_SELECTION = 32;

  /// **HOW FAR ACROSS THE PLAY AREA A MOVE MAY AIM**, in wire steps either side of the origin: a quarter of the
  /// half extent. The whole square would do as well for the host, and would leave ships crossing it for a
  /// minute at a time; this keeps them turning over.
  static constexpr std::int16_t MOVE_REACH_WIRE_STEPS = 8192;

  /// _runSeed and _botIndex select this bot's stream (`BotStream`).
  BotPolicy(std::uint64_t _runSeed, std::uint32_t _botIndex) noexcept;

  /// **ZERO OR MORE ORDERS, AND ZERO BETWEEN DECISION TICKS.** Nothing before this bot's first update has
  /// arrived, since until then it has no credits and no ships to know about. What it returns has also been
  /// added to the outstanding set, stamped _nowMilliseconds.
  ///
  /// Builds only when nothing is building and the credits in its own block cover the design's derived cost;
  /// selects only entities whose record names _player as the owner.
  [[nodiscard]] std::vector<Command> Decide(const ReplicaStore& _replicas, PlayerId _player, std::uint64_t _nowMilliseconds);

  /// Retires every outstanding command at or before _lastApplied, the host's high-water mark, and appends how
  /// long each waited to _outWaitMilliseconds. Returns how many it retired.
  std::size_t Acknowledge(std::uint16_t _lastApplied, std::uint64_t _nowMilliseconds, std::vector<std::uint64_t>& _outWaitMilliseconds);

  /// Oldest first, which is the order `FillOldestFirst` expects.
  [[nodiscard]] const std::vector<Command>& Outstanding() const noexcept
  {
    return m_outstanding;
  }

  /// **A REJOIN LOSES NOTHING THE HOST APPLIED, AND FORGETS WHAT IT DID NOT.** The outstanding commands are
  /// dropped, and the next decision adopts its numbering from the host again -- the same thing
  /// `ClientFrame` does for a person, for the same reason: a counter that disagrees with the host is refused.
  void Reset() noexcept;

private:
  Neuron::Pcg32 m_random;

  std::vector<Command> m_outstanding;
  std::vector<std::uint64_t> m_sentMilliseconds;

  std::uint32_t m_lastDecisionTick = 0;
  bool m_decided = false;

  std::uint16_t m_nextSequence = 1;
  bool m_adoptedSequence = false;
};

} // namespace Outpost

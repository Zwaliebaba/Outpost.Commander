#pragma once

namespace Outpost
{
// The record of one match for the owner's playtests: the MVP's Q1 and Q3 (plan task 6.3), and Phase 1's P1, P2 and P4
// (Phase 1 plan task 13.1). It holds how long the match ran and how it ended; when each player finished each research
// topic, and each gateway that opened a tier; every warship as it first appeared, by its components; each player's
// warship count every 30 seconds, and its peak; and each ore asteroid as it ran dry. It reads only snapshots, as any
// client does. Tools/MatchLog.py summarizes it.
//
// One line a record, every time in ticks:
//
//   match seed <seed> ticks_per_second <rate>
//   research <tick> player <player> topic <id> <name>
//   tier <tick> player <player> tier <tier>          the player finished the gateway that opens the tier
//   built <tick> player <player> hull <id> drive <id> weapon <id> <hull name>+<drive name>+<weapon name>
//   fleet <tick> player <player> warships <count>    every 30 seconds, from the player's own snapshots
//   dry <tick> asteroid <id>                         the asteroid's reserve ran out (Phase 1 design §8)
//   peak <tick> player <player> warships <count>     the most warships the player had at once, and when it first had
//                                                    them; written as the match ends or is left
//   end <tick> winner <player, or 0 for a draw>
//   left <tick>                                      the match was left before it ended (Finish)
class MatchLog : Neuron::NonCopyable
{
public:
  MatchLog(std::ostream& _out, std::uint64_t _seed, std::uint32_t _ticksPerSecond);

  // The player leaves the match: writes the peaks and "left" when it had not ended, and flushes. Called by the shell, not
  // by a destructor, since writing can throw.
  void Finish();

  // Takes one player's snapshot. Research and warship counts are read from each player's own snapshots, so every
  // player's are needed; a warship is written once, from whichever snapshot shows it first, and so is an asteroid that
  // has run dry.
  void Record(const Snapshot& _snapshot);

private:
  // One player's warships, counted in its own snapshots.
  struct Fleet
  {
    PlayerId player;
    std::uint64_t nextSampleTick = 0;
    size_t peakWarships = 0;
    std::uint64_t peakTick = 0;
  };

  void WritePeaks();

  std::ostream* m_out = nullptr;
  std::uint64_t m_sampleTicks = 0;
  std::uint64_t m_lastTick = 0;
  bool m_ended = false;
  // What has been written already.
  std::vector<std::pair<PlayerId, ResearchTopicId>> m_researched;
  std::set<EntityId> m_built;
  std::set<EntityId> m_dry;
  std::vector<Fleet> m_fleets;
};
} // namespace Outpost

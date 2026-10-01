#pragma once

namespace Outpost
{
// The record of one match for the owner's playtests, Q1 and Q3 (plan task 6.3): how long it ran and how it ended, when
// each player finished each research topic, and every warship as it first appeared, by its components, so that what got
// built can be read over time. It reads only snapshots, as any client does. Tools/MatchLog.py summarizes it.
//
// One line a record, every time in ticks:
//
//   match seed <seed> ticks_per_second <rate>
//   research <tick> player <player> topic <id> <name>
//   built <tick> player <player> hull <id> drive <id> weapon <id> <hull name>+<drive name>+<weapon name>
//   end <tick> winner <player, or 0 for a draw>
//   left <tick>                                  the match was left before it ended (Finish)
class MatchLog : Neuron::NonCopyable
{
public:
  MatchLog(std::ostream& _out, std::uint64_t _seed, std::uint32_t _ticksPerSecond);

  // The player leaves the match: writes "left" when it had not ended, and flushes. Called by the shell, not by a
  // destructor, since writing can throw.
  void Finish();

  // Takes one player's snapshot. Research is read from each player's own snapshots, so every player's are needed; a
  // warship is written once, from whichever snapshot shows it first.
  void Record(const Snapshot& _snapshot);

private:
  std::ostream* m_out = nullptr;
  std::uint64_t m_lastTick = 0;
  bool m_ended = false;
  // What has been written already.
  std::vector<std::pair<PlayerId, ResearchTopicId>> m_researched;
  std::set<EntityId> m_built;
};
} // namespace Outpost

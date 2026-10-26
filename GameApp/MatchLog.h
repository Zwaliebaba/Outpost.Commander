#pragma once

namespace Outpost
{
// The record of one match for the owner's playtests (ADR-038 decisions 1 and 5). It holds how long the match ran and how
// it ended; when each player finished each research topic, and each research tier its Research Lab opened; every warship
// as it first appeared, by its components and its module; each player's warship count every 30 seconds, and its peak;
// each ore asteroid as it ran dry; each upgrade, each structure above level 1 that is attacked, and how long both players
// sat at their node caps with equal nodes; and each derelict salvaged, each pirate outpost fought and cleared, and each
// warship that turns for home, is repaired and fights again. It reads only snapshots, as any client does.
// Tools/MatchLog.py summarizes it (ADR-038 decision 4).
//
// One line a record, every time in ticks:
//
//   match seed <seed> ticks_per_second <rate>
//   matchup <number>                                 the match is that battle matchup, from 1 (ADR-083)
//   research <tick> player <player> topic <id> <name>
//   tier <tick> player <player> tier <tier>          the player's Research Lab opened the tier (ADR-033 decision 2)
//   built <tick> player <player> hull <id> drive <id> weapon <id> <hull name>+<drive name>+<weapon name>[+<module name>]
//   fleet <tick> player <player> warships <count>    every 30 seconds, from the player's own snapshots
//   dry <tick> asteroid <id>                         the asteroid's reserve ran out (ADR-035)
//   peak <tick> player <player> warships <count>     the most warships the player had at once, and when it first had
//                                                    them; written as the match ends or is left
//   contact <tick> sector <id>                       the match's first shot, in the sector it was fired from
//   engagement <tick> sector <id>                    ships of both sides firing in one sector within 10 seconds of each
//                                                    other; another there is counted after 30 seconds with no shot. A
//                                                    sector of 0 is none.
//   sector <tick> sector <id> holder <player or 0>   a sector's holder changed, the homes' included at the start
//   tickets <tick> player <player> tickets <count>   every 30 seconds with its fleet, on a map with territory
//   upgrade <tick> player <player> structure <id> <kind> level <level> <started, finished or lost>
//                                                    a player's structure began, finished or lost the work on a level,
//                                                    from its own snapshots; <kind> is station, shipyard or lab
//   attacked <tick> player <owner> structure <id> <kind> level <level>
//                                                    a shot first hit a structure at a level above 1
//   stall <tick> ticks <count>                       the ticks during which both players were at their node caps and held
//                                                    as many nodes as each other; written with the peaks, on
//                                                    a map with territory
//   salvaged <tick> player <player> derelict <id> ore <ore>
//                                                    a derelict left the player's sight while its Constructors were at it:
//                                                    they salvaged it. Two players' crews at one both count.
//   pirates <tick> player <player> sector <id>       a first shot between the player and the pirates of the sector
//   cleared <tick> sector <id>                       the pirates no longer guard the sector
//   retreat <tick> player <player> ship <id>         a warship of the player's turned for home to be repaired
//   repaired <tick> player <player> ship <id>        a warship that turned for home stopped, whole
//   again <tick> player <player> ship <id>           a repaired warship fired, the first time since its repair
//   ending <tick> <production, domination, fleet or time>
//                                                    how the match ended, just before its end (ADR-057 decision 6); a battle
//                                                    matchup's by a fleet destroyed or its time run out (ADR-083)
//   end <tick> winner <player, or 0 for a draw>
//   left <tick>                                      the match was left before it ended (Finish)
class MatchLog : Neuron::NonCopyable
{
public:
  MatchLog(std::ostream& _out, std::uint64_t _seed, std::uint32_t _ticksPerSecond);

  // The player leaves the match: writes the peaks and "left" when it had not ended, and flushes. Called by the shell, not
  // by a destructor, since writing can throw.
  void Finish();

  // Says the match is a battle matchup, the number _number of Matchups.json's from 1 (ADR-083), for the owner's win rate.
  void Matchup(std::uint32_t _number);

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

  // The peaks, and on a map with territory the stall: what is written once, as the match ends or is left.
  void WriteTotals();
  // A player's structures with levels, the upgrades started and finished, and those lost with their structure.
  void RecordUpgrades(const Snapshot& _snapshot);
  // Whether both players are at their caps with equal nodes in the tick of _snapshot, once both players' snapshots of it are in.
  void RecordStall(const Snapshot& _snapshot);
  // The player's warships that turn for home and come back whole, from its own snapshots.
  void RecordRetreats(const Snapshot& _snapshot);
  // The derelicts the player's Constructors finish, from its own snapshots.
  void RecordSalvage(const Snapshot& _snapshot);

  std::ostream* m_out = nullptr;
  std::uint64_t m_sampleTicks = 0;
  std::uint64_t m_windowTicks = 0;
  std::uint64_t m_gapTicks = 0;
  std::uint64_t m_lastTick = 0;
  bool m_ended = false;
  // Fire in one sector: the last tick each side fired there, and whether an engagement there is under way.
  struct SectorFire
  {
    std::int32_t sector = 0;
    std::vector<std::pair<PlayerId, std::uint64_t>> lastShot;
    bool engaged = false;
  };

  // Every shot of _snapshot that another snapshot has not already shown, for contact and engagements.
  void RecordShots(const Snapshot& _snapshot);

  // What has been written already.
  std::vector<std::pair<PlayerId, ResearchTopicId>> m_researched;
  // The highest research tier each player's Lab has opened that the log has written.
  std::vector<std::pair<PlayerId, std::int32_t>> m_tiers;
  std::set<EntityId> m_built;
  std::set<EntityId> m_dry;
  std::vector<Fleet> m_fleets;
  bool m_contact = false;
  std::vector<SectorFire> m_fire;
  std::vector<std::pair<std::int32_t, PlayerId>> m_holders;
  // Each player's latest snapshot's tick, by which every shot it is shown has arrived, and the shots counted that a player
  // may still be shown, by tick, shooter and target.
  std::vector<std::pair<PlayerId, std::uint64_t>> m_shotsShownTo;
  std::set<std::tuple<std::uint64_t, EntityId, EntityId>> m_shotsCounted;

  // A structure of the player's own that grows, at the level its owner last saw, and whether a level was under way.
  struct Growth
  {
    EntityId id;
    StructureKind kind = StructureKind::CommandStation;
    std::int32_t level = 1;
    bool upgrading = false;
  };
  std::vector<Growth> m_growth;
  // Each structure, and the level, of the shots on structures above level 1 already written.
  std::set<std::pair<EntityId, std::int32_t>> m_attacked;
  // Each player's last tick seen at its cap or not, with the nodes it held; and the stall's ticks so far.
  struct CapState
  {
    PlayerId player;
    std::uint64_t tick = 0;
    bool atCap = false;
    std::int64_t held = 0;
  };
  std::vector<CapState> m_caps;
  bool m_territory = false;
  std::uint64_t m_stallTicks = 0;
  std::optional<std::uint64_t> m_stallCountedTick;

  // Each sector the pirates were last seen to guard or not.
  std::vector<std::pair<std::int32_t, bool>> m_guarded;
  // Each player and sector whose pirates the player has fought.
  std::set<std::pair<PlayerId, std::int32_t>> m_piratesFought;
  // Whether each of a player's warships was last seen retreating, and the repaired ones that have not fired since, with
  // their owners.
  std::map<EntityId, bool> m_retreating;
  std::map<EntityId, PlayerId> m_repaired;
  // A derelict a player last saw with its Constructors at it, and the Ore it pays.
  struct Salvage
  {
    PlayerId player;
    EntityId derelict;
    std::int32_t ore = 0;
  };
  std::vector<Salvage> m_salvage;
};
} // namespace Outpost

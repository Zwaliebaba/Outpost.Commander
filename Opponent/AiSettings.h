#pragma once

namespace Outpost
{
// One line of design §7's counters as the AI reads it: when the enemy fields most of this design, build that one. A design
// may be answered more than once, in order of preference: the first answer the AI has unlocked is the one it builds.
struct CounterRule
{
  DesignComponents enemy;
  DesignComponents answer;
};

// OutpostCommander/Assets/Opponent.json as the AI holds it (ADR-020). These are the AI's own numbers: how it builds its
// base, when it attacks and what it answers the enemy's fleet with. The rules of the match are Tuning.json's, which the
// AI only sees through its snapshots.
struct AiSettings
{
  // Warships the AI gathers in reserve before it commits them to an attack (gate G9), and how many more for each tier its
  // player has opened past the first (Phase 1 plan task 12.2).
  std::int32_t attackGroupShips = 0;
  std::int32_t attackGroupGrowthPerTier = 0;
  // An attack group that has lost this share of the ships it set out with falls back to the rally and rejoins the
  // reserve, which then waits regroupSeconds before it attacks again (task 12.2). Zero never falls back.
  double retreatLossShare = 0.0;
  double regroupSeconds = 0.0;

  // How often it reviews the enemy's fleet and picks the design it builds.
  double reviewIntervalSeconds = 0.0;
  // Constructors it keeps, ordering more from its Command Station.
  std::int32_t constructors = 0;
  // The income, in whole Ore a second, that each of its Shipyards needs: it builds its N-th Shipyard once its income
  // reaches N times this. The first is in its build order whatever its income.
  double incomePerShipyardOrePerSecond = 0.0;
  // Ore asteroids it takes as its own: the ones nearest its Command Station.
  std::int32_t homeAsteroids = 0;
  // Ore asteroids beyond those that it takes too, nearest first, each with a Defence Platform beside it.
  std::int32_t contestedAsteroids = 0;
  // Together, how many of its rigs it keeps on asteroids with ore left: when one runs dry, it takes the nearest asteroid
  // it knows still holds ore, with a platform beside it as a contested one (Phase 1 design §13). A dry rig stays for its
  // trickle.
  // Defence Platforms it plans round its base for each Shipyard, each built once its income reaches that Shipyard's share
  // (task 12.2).
  std::int32_t homePlatformsPerShipyard = 0;
  // Jobs it keeps in each Shipyard's queue.
  std::int32_t shipyardQueueJobs = 0;
  // How long its reserve stays where an attack on its base came from after the last shot there.
  double defenseHoldSeconds = 0.0;
  // The gap it leaves between a structure it plans and anything else that blocks, so that ships still pass.
  double structureGapMeters = 0.0;
  // How far from its Command Station, toward the map's center, its reserve gathers.
  double rallyDistanceMeters = 0.0;
  // Topics it researches, in this order, each once its prerequisites are done.
  std::vector<ResearchTopicId> researchOrder;
  // What it builds when no counter applies, or the counter is one it has not unlocked.
  DesignComponents defaultDesign;
  std::vector<CounterRule> counters;
};

// Reads the text of OutpostCommander/Assets/Opponent.json. Throws Neuron::Exception on the first problem, naming where it
// is, such as "counters[2].answer.hull". It cannot check the identifiers against the tuning data, which only the server
// reads; an identifier the match does not have is a design the AI never sees and never builds.
[[nodiscard]] AiSettings LoadAiSettings(std::string_view _json);

// Reads Opponent.json from the package's Assets folder (ADR-008). Throws Neuron::Exception naming the file when it is
// missing or invalid.
[[nodiscard]] AiSettings LoadPackagedAiSettings();
} // namespace Outpost

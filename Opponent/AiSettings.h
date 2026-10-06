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

  // On a map with territory (Phase 2 design §12, ADR-020 decision 13). Scouts it keeps, of a design with a Sensor Array,
  // built ahead of its warships, which tour the enemy's flank sectors.
  std::int32_t scouts = 0;
  DesignComponents scoutDesign;
  // A raid: this many warships of its reserve, sent at an enemy sector it sees no enemy warship guarding, when its reserve
  // holds at least twice as many; it falls back once it has lost this share of them, and the next waits this long.
  std::int32_t raidShips = 0;
  double raidLossShare = 0.0;
  double raidIntervalSeconds = 0.0;
  // Its main attack waits for a lead of this many nodes, or for a reserve this many times the attack group's size.
  std::int32_t attackNodeLead = 0;
  double attackWithoutLeadShare = 1.0;
  // Defence Platforms it builds by each Relay on its front: a sector it holds next to one the enemy holds.
  std::int32_t frontPlatforms = 0;
  // Free sectors it claims at most, beyond those its rigs take it to.
  std::int32_t claimSectors = 0;

  // Structure levels (Phase 3 design §8). It upgrades its Research Lab to the level that gives a second research slot
  // once its open tier has reached this one. Choosing what to attack, a structure counts as this many meters nearer for
  // each level it has above the first, among structures of one rank.
  std::int32_t secondSlotTier = 0;
  double attackLevelMeters = 0.0;

  // The fleet cap (Phase 4 design §5, §12). Its main attack also goes once its reserve holds this share of the ships its
  // cap lets it have of its production design's hull, when that is fewer than attackGroupShips asks, so that the cap never
  // keeps it from attacking. A share of 1 waits for a full cap.
  double attackCapShare = 1.0;
};

// Reads the text of OutpostCommander/Assets/Opponent.json. Throws Neuron::Exception on the first problem, naming where it
// is, such as "counters[2].answer.hull". It cannot check the identifiers against the tuning data, which only the server
// reads; an identifier the match does not have is a design the AI never sees and never builds.
[[nodiscard]] AiSettings LoadAiSettings(std::string_view _json);

// Reads an AI's settings from the package's Assets folder (ADR-008): Opponent.json, the Normal AI, or the Easy or Hard
// one's file (ADR-065). Throws Neuron::Exception naming the file when it is missing or invalid.
inline constexpr std::wstring_view NORMAL_AI_SETTINGS = L"Opponent.json";
inline constexpr std::wstring_view EASY_AI_SETTINGS = L"OpponentEasy.json";
inline constexpr std::wstring_view HARD_AI_SETTINGS = L"OpponentHard.json";
[[nodiscard]] AiSettings LoadPackagedAiSettings(std::wstring_view _fileName = NORMAL_AI_SETTINGS);
} // namespace Outpost

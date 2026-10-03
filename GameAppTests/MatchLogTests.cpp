#include "pch.h"

#include <sstream>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameAppTests
{
namespace
{
constexpr Outpost::PlayerId HUMAN{1};
constexpr Outpost::PlayerId AI{2};

// One player's snapshot with the starting catalog, and the swarm's components named.
Outpost::Snapshot SnapshotOf(Outpost::PlayerId _player, std::uint64_t _tick)
{
  Outpost::Snapshot snapshot{.tick = _tick, .player = _player};
  snapshot.hulls = {{.id = Outpost::HullId{1}, .nameUtf8 = "Small"}};
  snapshot.drives = {{.id = Outpost::DriveId{1}, .nameUtf8 = "Ion"}};
  snapshot.weapons = {{.id = Outpost::WeaponId{1}, .nameUtf8 = "Mass Driver"}};
  snapshot.research = {{.id = Outpost::ResearchTopicId{1}, .nameUtf8 = "Improved Extraction"},
                       {.id = Outpost::ResearchTopicId{2}, .nameUtf8 = "Hull Plating"}};
  return snapshot;
}

Outpost::EntityView Swarm(std::uint32_t _id, Outpost::PlayerId _owner)
{
  return {.id = Outpost::EntityId{_id},
          .owner = _owner,
          .hull = Outpost::HullId{1},
          .drive = Outpost::DriveId{1},
          .weapon = Outpost::WeaponId{1},
          .role = Outpost::ShipRole::Warship};
}
} // namespace

TEST_CLASS(MatchLogTests)
{
public:
  // Plan task 6.3: the seed, each player's research as it finishes, each warship once as it first appears, and the end.
  TEST_METHOD(RecordsResearchShipsAndTheEnd)
  {
    std::ostringstream out;
    {
      Outpost::MatchLog log(out, 42, 20);
      Outpost::Snapshot human = SnapshotOf(HUMAN, 100);
      Outpost::Snapshot ai = SnapshotOf(AI, 100);
      human.entities = {Swarm(7, AI)};
      ai.entities = {Swarm(7, AI)};
      ai.research[1].researched = true;
      log.Record(human);
      log.Record(ai);

      human.tick = ai.tick = 200;
      human.research[0].researched = true;
      ai.research[1].researched = true;
      human.entities.push_back(Swarm(9, HUMAN));
      // A Constructor is no warship.
      human.entities.push_back({.id = Outpost::EntityId{10}, .owner = HUMAN, .role = Outpost::ShipRole::Constructor});
      human.matchOver = true;
      human.matchEndedTick = 199;
      human.winner = AI;
      log.Record(human);
      log.Record(ai);
      log.Record(human);
    }
    Assert::AreEqual(std::string("match seed 42 ticks_per_second 20\n"
                                 "built 100 player 2 hull 1 drive 1 weapon 1 Small+Ion+Mass Driver\n"
                                 "fleet 100 player 1 warships 0\n"
                                 "research 100 player 2 topic 2 Hull Plating\n"
                                 "fleet 100 player 2 warships 1\n"
                                 "research 200 player 1 topic 1 Improved Extraction\n"
                                 "built 200 player 1 hull 1 drive 1 weapon 1 Small+Ion+Mass Driver\n"
                                 "peak 200 player 1 warships 1\n"
                                 "peak 100 player 2 warships 1\n"
                                 "end 199 winner 2\n"),
                     out.str());
  }

  // A match the player leaves before it ends says so, at the last tick it saw, once, after the peaks.
  TEST_METHOD(RecordsAMatchLeftEarly)
  {
    std::ostringstream out;
    Outpost::MatchLog log(out, 7, 20);
    log.Record(SnapshotOf(HUMAN, 340));
    log.Finish();
    log.Finish();
    Assert::AreEqual(std::string("match seed 7 ticks_per_second 20\n"
                                 "fleet 340 player 1 warships 0\n"
                                 "peak 340 player 1 warships 0\n"
                                 "left 340\n"),
                     out.str());
  }

  // Phase 1 plan task 13.1: the gateway that opens a tier, the player's own warships every 30 seconds and at their peak,
  // and an asteroid as it runs dry, once.
  TEST_METHOD(RecordsTiersFleetsAndDryAsteroids)
  {
    std::ostringstream out;
    Outpost::MatchLog log(out, 3, 20);
    Outpost::Snapshot ai = SnapshotOf(AI, 1);
    ai.research.push_back({.id = Outpost::ResearchTopicId{9}, .nameUtf8 = "Relay Archives", .tier = 2, .gateway = true});
    const Outpost::EntityView asteroid{
      .id = Outpost::EntityId{50}, .kind = Outpost::EntityKind::Asteroid, .oreReserveHundredths = std::int64_t{100}};
    ai.entities = {asteroid, Swarm(7, AI), Swarm(8, HUMAN)};
    log.Record(ai);

    // Ten seconds on, two more of its own; not yet a sample.
    ai.tick = 200;
    ai.entities.push_back(Swarm(9, AI));
    ai.entities.push_back(Swarm(10, AI));
    log.Record(ai);

    // Thirty seconds in, one lost, the gateway done and the asteroid dry.
    ai.tick = 600;
    ai.entities.pop_back();
    ai.research.back().researched = true;
    ai.entities.front().oreReserveHundredths = 0;
    log.Record(ai);
    ai.tick = 601;
    log.Record(ai);
    log.Finish();

    Assert::AreEqual(std::string("match seed 3 ticks_per_second 20\n"
                                 "built 1 player 2 hull 1 drive 1 weapon 1 Small+Ion+Mass Driver\n"
                                 "built 1 player 1 hull 1 drive 1 weapon 1 Small+Ion+Mass Driver\n"
                                 "fleet 1 player 2 warships 1\n"
                                 "built 200 player 2 hull 1 drive 1 weapon 1 Small+Ion+Mass Driver\n"
                                 "built 200 player 2 hull 1 drive 1 weapon 1 Small+Ion+Mass Driver\n"
                                 "research 600 player 2 topic 9 Relay Archives\n"
                                 "tier 600 player 2 tier 2\n"
                                 "dry 600 asteroid 50\n"
                                 "fleet 600 player 2 warships 2\n"
                                 "peak 200 player 2 warships 3\n"
                                 "left 601\n"),
                     out.str());
  }
};
} // namespace GameAppTests

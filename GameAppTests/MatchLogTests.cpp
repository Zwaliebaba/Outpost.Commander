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
                                 "research 100 player 2 topic 2 Hull Plating\n"
                                 "research 200 player 1 topic 1 Improved Extraction\n"
                                 "built 200 player 1 hull 1 drive 1 weapon 1 Small+Ion+Mass Driver\n"
                                 "end 199 winner 2\n"),
                     out.str());
  }

  // A match the player leaves before it ends says so, at the last tick it saw.
  TEST_METHOD(RecordsAMatchLeftEarly)
  {
    std::ostringstream out;
    {
      Outpost::MatchLog log(out, 7, 20);
      log.Record(SnapshotOf(HUMAN, 340));
    }
    Assert::AreEqual(std::string("match seed 7 ticks_per_second 20\nleft 340\n"), out.str());
  }
};
} // namespace GameAppTests

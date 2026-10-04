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
                                 "ending 199 production\n"
                                 "end 199 winner 2\n"),
                     out.str());
  }

  // Phase 2 design §10: a warship with a module is recorded with the module's name after its components', so that a
  // scout reads apart from a plain ship of its hull, drive and weapon.
  TEST_METHOD(RecordsAShipsModule)
  {
    std::ostringstream out;
    Outpost::MatchLog log(out, 7, 20);
    Outpost::Snapshot ai = SnapshotOf(AI, 100);
    ai.modules = {{.id = Outpost::ModuleId{1}, .nameUtf8 = "Sensor Array"}};
    Outpost::EntityView scout = Swarm(7, AI);
    scout.module = Outpost::ModuleId{1};
    ai.entities = {scout, Swarm(8, AI)};
    log.Record(ai);
    const std::string text = out.str();
    Assert::IsTrue(text.find("built 100 player 2 hull 1 drive 1 weapon 1 Small+Ion+Mass Driver+Sensor Array\n") != std::string::npos,
                   L"the scout's module");
    Assert::IsTrue(text.find("built 100 player 2 hull 1 drive 1 weapon 1 Small+Ion+Mass Driver\n") != std::string::npos, L"the plain ship");
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

  // Phase 2 plan task 19.1: the first shot; an engagement once both sides fire in a sector, and another there only after
  // 30 quiet seconds; each change of a sector's holder; each player's tickets with its fleet; and how the match ended. A
  // shot both players' snapshots show is counted once, and the side of a shooter a snapshot does not show is the other
  // side from its target's.
  TEST_METHOD(RecordsContactEngagementsTerritoryAndTickets)
  {
    std::ostringstream out;
    {
      Outpost::MatchLog log(out, 7, 20);
      const auto snapshot = [](Outpost::PlayerId _player, std::uint64_t _tick)
      {
        Outpost::Snapshot view = SnapshotOf(_player, _tick);
        view.sectors = {{.id = 1, .minXMeters = -100.0f, .maxXMeters = 0.0f, .maxZMeters = 100.0f, .holder = HUMAN},
                        {.id = 2, .minXMeters = 0.0f, .maxXMeters = 100.0f, .maxZMeters = 100.0f, .holder = AI}};
        view.tickets = {{.player = HUMAN, .tickets = 1000}, {.player = AI, .tickets = 990}};
        view.entities = {Swarm(10, HUMAN), Swarm(20, AI)};
        return view;
      };
      const auto fire = [&](std::uint64_t _tick, Outpost::EntityId _shooter, Outpost::EntityId _target, bool _shooterSeen)
      {
        for (const Outpost::PlayerId player : {HUMAN, AI})
        {
          Outpost::Snapshot view = snapshot(player, _tick);
          if (!_shooterSeen)
            std::erase_if(view.entities, [_shooter](const Outpost::EntityView& _entity) { return _entity.id == _shooter; });
          view.shots = {{.shooter = _shooter, .target = _target, .from = {.xMeters = -50.0f, .zMeters = 50.0f}}};
          log.Record(view);
        }
      };
      log.Record(snapshot(HUMAN, 1));
      log.Record(snapshot(AI, 1));
      fire(100, Outpost::EntityId{10}, Outpost::EntityId{20}, true);
      fire(150, Outpost::EntityId{20}, Outpost::EntityId{10}, false);
      fire(160, Outpost::EntityId{10}, Outpost::EntityId{20}, true);
      // Quiet for more than 30 seconds, and both fire again.
      fire(770, Outpost::EntityId{10}, Outpost::EntityId{20}, true);
      fire(780, Outpost::EntityId{20}, Outpost::EntityId{10}, true);
      Outpost::Snapshot human = snapshot(HUMAN, 800);
      human.sectors[1].holder = HUMAN;
      human.matchOver = true;
      human.matchEndedTick = 800;
      human.winner = HUMAN;
      human.ending = Outpost::MatchEnding::Domination;
      log.Record(human);
    }
    Assert::AreEqual(std::string("match seed 7 ticks_per_second 20\n"
                                 "sector 1 sector 1 holder 1\n"
                                 "sector 1 sector 2 holder 2\n"
                                 "built 1 player 1 hull 1 drive 1 weapon 1 Small+Ion+Mass Driver\n"
                                 "built 1 player 2 hull 1 drive 1 weapon 1 Small+Ion+Mass Driver\n"
                                 "fleet 1 player 1 warships 1\n"
                                 "tickets 1 player 1 tickets 1000\n"
                                 "fleet 1 player 2 warships 1\n"
                                 "tickets 1 player 2 tickets 990\n"
                                 "contact 100 sector 1\n"
                                 "engagement 150 sector 1\n"
                                 "fleet 770 player 1 warships 1\n"
                                 "tickets 770 player 1 tickets 1000\n"
                                 "fleet 770 player 2 warships 1\n"
                                 "tickets 770 player 2 tickets 990\n"
                                 "engagement 780 sector 1\n"
                                 "sector 800 sector 2 holder 1\n"
                                 "peak 1 player 1 warships 1\n"
                                 "peak 1 player 2 warships 1\n"
                                 "ending 800 domination\n"
                                 "end 800 winner 1\n"),
                     out.str());
  }

  // Phase 1 plan task 13.1: each tier the player's Research Lab opens (Phase 3 design §6), the player's own warships every
  // 30 seconds and at their peak, and an asteroid as it runs dry, once.
  TEST_METHOD(RecordsTiersFleetsAndDryAsteroids)
  {
    std::ostringstream out;
    Outpost::MatchLog log(out, 3, 20);
    Outpost::Snapshot ai = SnapshotOf(AI, 1);
    ai.research.push_back({.id = Outpost::ResearchTopicId{14}, .nameUtf8 = "Reinforced Structures", .tier = 2});
    const Outpost::EntityView asteroid{
      .id = Outpost::EntityId{50}, .kind = Outpost::EntityKind::Asteroid, .oreReserveHundredths = std::int64_t{100}};
    ai.entities = {asteroid, Swarm(7, AI), Swarm(8, HUMAN)};
    log.Record(ai);

    // Ten seconds on, two more of its own; not yet a sample.
    ai.tick = 200;
    ai.entities.push_back(Swarm(9, AI));
    ai.entities.push_back(Swarm(10, AI));
    log.Record(ai);

    // Thirty seconds in, one lost, tier 2 opened and a topic of it done, and the asteroid dry.
    ai.tick = 600;
    ai.entities.pop_back();
    ai.research.back().researched = true;
    ai.researchTier = 2;
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
                                 "research 600 player 2 topic 14 Reinforced Structures\n"
                                 "tier 600 player 2 tier 2\n"
                                 "dry 600 asteroid 50\n"
                                 "fleet 600 player 2 warships 2\n"
                                 "peak 200 player 2 warships 3\n"
                                 "left 601\n"),
                     out.str());
  }
};
} // namespace GameAppTests

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
  // ADR-083: a battle matchup is named after the match's line, and its end says how it ended.
  TEST_METHOD(RecordsAMatchupAndHowItEnded)
  {
    std::ostringstream out;
    Outpost::MatchLog log(out, 7, 20);
    log.Matchup(3);
    Outpost::Snapshot ended = SnapshotOf(HUMAN, 340);
    ended.matchOver = true;
    ended.winner = HUMAN;
    ended.matchEndedTick = 340;
    ended.ending = Outpost::MatchEnding::FleetDestroyed;
    log.Record(ended);
    const std::string text = out.str();
    Assert::IsTrue(text.starts_with("match seed 7 ticks_per_second 20\nmatchup 3\n"));
    Assert::IsTrue(text.contains("ending 340 fleet\n") && text.contains("end 340 winner 1\n"),
                   std::wstring(text.begin(), text.end()).c_str());
  }

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

  // Phase 4 plan task 34.1 (U1): a fight with the pirates is recorded once a player and sector, and the sector once they no
  // longer guard it; it is no contact and no engagement, which are the players' fights (ADR-073).
  TEST_METHOD(RecordsThePiratesFightsApart)
  {
    std::ostringstream out;
    {
      Outpost::MatchLog log(out, 7, 20);
      for (const std::uint64_t tick : {100u, 110u, 120u})
      {
        Outpost::Snapshot view = SnapshotOf(HUMAN, tick);
        view.sectors = {{.id = 1, .minXMeters = -100.0f, .maxXMeters = 0.0f, .maxZMeters = 100.0f, .guarded = tick < 120}};
        view.entities = {Swarm(10, HUMAN), Swarm(30, Outpost::PIRATES)};
        view.shots = {{.shooter = Outpost::EntityId{10},
                       .target = Outpost::EntityId{30},
                       .from = {.xMeters = -50.0f, .zMeters = 50.0f},
                       .to = {.xMeters = -40.0f, .zMeters = 50.0f}},
                      {.shooter = Outpost::EntityId{30},
                       .target = Outpost::EntityId{10},
                       .from = {.xMeters = -40.0f, .zMeters = 50.0f},
                       .to = {.xMeters = -50.0f, .zMeters = 50.0f}}};
        log.Record(view);
      }
    }
    const std::string written = out.str();
    Assert::IsTrue(written.find("pirates 100 player 1 sector 1\n") != std::string::npos, L"the fight");
    Assert::AreEqual(written.find("pirates"), written.rfind("pirates"), L"once");
    Assert::IsTrue(written.find("cleared 120 sector 1\n") != std::string::npos, L"the sector cleared");
    Assert::IsTrue(written.find("contact") == std::string::npos);
    Assert::IsTrue(written.find("engagement") == std::string::npos);
  }

  // Phase 4 plan task 34.1: a derelict gone from the sight of the Constructors at it was salvaged by their player, not by one
  // who only saw it (U1); a warship that turns for home, comes back whole and fires again (U3).
  TEST_METHOD(RecordsSalvageRetreatsAndRepairs)
  {
    std::ostringstream out;
    {
      Outpost::MatchLog log(out, 7, 20);
      const auto snapshot = [](Outpost::PlayerId _player, std::uint64_t _tick, bool _derelict, std::int32_t _hitPoints, bool _retreating)
      {
        Outpost::Snapshot view = SnapshotOf(_player, _tick);
        Outpost::EntityView warship = Swarm(10, HUMAN);
        warship.hitPointsHundredths = _hitPoints;
        warship.maxHitPointsHundredths = 10000;
        warship.retreating = _retreating;
        view.entities = {warship, Swarm(20, AI)};
        if (_player == HUMAN)
        {
          view.entities.push_back({.id = Outpost::EntityId{11},
                                   .owner = HUMAN,
                                   .role = Outpost::ShipRole::Constructor,
                                   .position = {.xMeters = 25.0f},
                                   .radiusMeters = 5.0f});
        }
        if (_derelict)
          view.entities.push_back(
            {.id = Outpost::EntityId{50}, .kind = Outpost::EntityKind::Derelict, .radiusMeters = 10.0f, .salvageOre = 300});
        return view;
      };
      for (const Outpost::PlayerId player : {HUMAN, AI})
        log.Record(snapshot(player, 100, true, 10000, false));
      for (const Outpost::PlayerId player : {HUMAN, AI})
        log.Record(snapshot(player, 120, false, 2000, true));
      log.Record(snapshot(HUMAN, 140, false, 10000, false));
      Outpost::Snapshot fire = snapshot(HUMAN, 160, false, 10000, false);
      fire.shots = {{.shooter = Outpost::EntityId{10}, .target = Outpost::EntityId{20}}};
      log.Record(fire);
      fire.tick = 170;
      log.Record(fire);
    }
    const std::string written = out.str();
    Assert::IsTrue(written.find("salvaged 120 player 1 derelict 50 ore 300\n") != std::string::npos, L"the crew's salvage");
    Assert::IsTrue(written.find("player 2 derelict") == std::string::npos, L"no salvage for a player who only saw it");
    Assert::IsTrue(written.find("retreat 120 player 1 ship 10\n") != std::string::npos, L"the retreat");
    Assert::IsTrue(written.find("repaired 140 player 1 ship 10\n") != std::string::npos, L"the repair");
    Assert::IsTrue(written.find("again 160 player 1 ship 10\n") != std::string::npos, L"fighting again");
    Assert::AreEqual(written.find("again"), written.rfind("again"), L"once");
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

  // Phase 3 plan task 25.1: each upgrade started, finished or lost, from its owner's snapshots; a structure above level 1
  // the first time it is shot at that level (T4); and the ticks both players sat at their caps with as many nodes (T2).
  TEST_METHOD(RecordsUpgradesAttacksAndTheStall)
  {
    std::ostringstream out;
    Outpost::MatchLog log(out, 5, 20);
    const auto structure = [](std::uint32_t _id, Outpost::PlayerId _owner, Outpost::StructureKind _kind, std::int32_t _level)
    {
      return Outpost::EntityView{
        .id = Outpost::EntityId{_id}, .kind = Outpost::EntityKind::Structure, .owner = _owner, .structure = _kind, .level = _level};
    };
    // Three nodes each, both at a cap of 3.
    std::vector<Outpost::SectorView> sectors;
    for (std::int32_t id = 1; id <= 6; ++id)
      sectors.push_back({.id = id, .holder = id <= 3 ? HUMAN : AI});
    Outpost::Snapshot human = SnapshotOf(HUMAN, 100);
    Outpost::Snapshot ai = SnapshotOf(AI, 100);
    human.sectors = ai.sectors = sectors;
    human.nodeCap = ai.nodeCap = 3;
    human.entities = {structure(1, HUMAN, Outpost::StructureKind::CommandStation, 1)};
    ai.entities = {structure(2, AI, Outpost::StructureKind::CommandStation, 1), structure(3, AI, Outpost::StructureKind::ResearchLab, 1)};
    log.Record(human);
    log.Record(ai);

    // The human's station starts level 2, and both are still stalled.
    human.tick = ai.tick = 101;
    human.entities[0].upgradePermille = 100;
    log.Record(human);
    log.Record(ai);

    // It finishes, which lifts its cap; the AI shoots at it, twice, and starts its Lab's level 2.
    human.tick = ai.tick = 102;
    human.entities[0].upgradePermille.reset();
    human.entities[0].level = 2;
    human.nodeCap = 4;
    ai.entities.push_back(human.entities[0]);
    ai.entities[1].upgradePermille = 10;
    ai.shots = {{.shooter = Outpost::EntityId{2}, .target = Outpost::EntityId{1}}};
    log.Record(human);
    log.Record(ai);
    human.tick = ai.tick = 103;
    log.Record(human);
    log.Record(ai);

    // The Lab is destroyed with its level under way.
    human.tick = ai.tick = 104;
    ai.shots.clear();
    ai.entities.erase(ai.entities.begin() + 1);
    ai.destroyed = {{.id = Outpost::EntityId{3}, .kind = Outpost::EntityKind::Structure, .owner = AI}};
    log.Record(human);
    log.Record(ai);
    log.Finish();

    std::istringstream lines(out.str());
    std::string phaseThree;
    for (std::string line; std::getline(lines, line);)
    {
      if (line.starts_with("upgrade ") || line.starts_with("attacked ") || line.starts_with("stall "))
        phaseThree += line + "\n";
    }
    Assert::AreEqual(std::string("upgrade 101 player 1 structure 1 station level 2 started\n"
                                 "upgrade 102 player 1 structure 1 station level 2 finished\n"
                                 "attacked 102 player 1 structure 1 station level 2\n"
                                 "upgrade 102 player 2 structure 3 lab level 2 started\n"
                                 "upgrade 104 player 2 structure 3 lab level 2 lost\n"
                                 "stall 104 ticks 2\n"),
                     phaseThree);
  }
};
} // namespace GameAppTests

#include "pch.h"
#include "RepositoryData.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameLogicTests
{
namespace
{
constexpr Outpost::PlayerId BLUE{1};
constexpr Outpost::PlayerId RED{2};

// One row of the table `python Tools/BattleModel.py` prints for the repository's tuning data (task 3.2).
struct ModelRow
{
  std::uint32_t hull;
  std::uint32_t drive;
  std::uint32_t weapon;
  std::int32_t cost;
  double hitPoints;
  std::int32_t armor;
  double speedMetersPerSecond;
  double rangeMeters;
  // Damage per second after armor against the Small, Medium and Large hulls.
  std::array<double, 3> damagePerSecond;
};

// Copied from the model's output on 2026-10-01, after §12 was retuned against the simulation. The model rounds damage
// per second to one decimal. Phase 4 tripled every cost (Phase 4 design §4).
constexpr std::array<ModelRow, 18> MODEL_TABLE{{
  {1, 1, 1, 261, 198, 2, 78.0, 120, {30.0, 15.0, 8.8}},
  {1, 1, 2, 411, 198, 2, 78.0, 220, {34.4, 32.2, 30.0}},
  {1, 1, 3, 546, 198, 2, 78.0, 280, {14.0, 11.0, 8.0}},
  {1, 2, 1, 441, 308, 2, 48.0, 120, {30.0, 15.0, 8.8}},
  {1, 2, 2, 591, 308, 2, 48.0, 220, {34.4, 32.2, 30.0}},
  {1, 2, 3, 726, 308, 2, 48.0, 280, {14.0, 11.0, 8.0}},
  {2, 1, 1, 495, 450, 8, 52.0, 120, {30.0, 15.0, 8.8}},
  {2, 1, 2, 645, 450, 8, 52.0, 220, {34.4, 32.2, 30.0}},
  {2, 1, 3, 780, 450, 8, 52.0, 280, {14.0, 11.0, 8.0}},
  {2, 2, 1, 675, 700, 8, 32.0, 120, {30.0, 15.0, 8.8}},
  {2, 2, 2, 825, 700, 8, 32.0, 220, {34.4, 32.2, 30.0}},
  {2, 2, 3, 960, 700, 8, 32.0, 280, {14.0, 11.0, 8.0}},
  {3, 1, 1, 1065, 1080, 14, 32.5, 120, {30.0, 15.0, 8.8}},
  {3, 1, 2, 1215, 1080, 14, 32.5, 220, {34.4, 32.2, 30.0}},
  {3, 1, 3, 1350, 1080, 14, 32.5, 280, {14.0, 11.0, 8.0}},
  {3, 2, 1, 1245, 1680, 14, 20.0, 120, {30.0, 15.0, 8.8}},
  {3, 2, 2, 1395, 1680, 14, 20.0, 220, {34.4, 32.2, 30.0}},
  {3, 2, 3, 1530, 1680, 14, 20.0, 280, {14.0, 11.0, 8.0}},
}};

Outpost::Tuning RepositoryTuning()
{
  return Outpost::LoadTuning(ReadRepositoryTuning());
}
} // namespace

TEST_CLASS(DesignTests)
{
public:
  // Task 3.2: every design's derived stats are the ones the battle model uses.
  TEST_METHOD(MatchesTheBattleModelsTable)
  {
    const Outpost::Tuning tuning = RepositoryTuning();
    for (const ModelRow& row : MODEL_TABLE)
    {
      const Outpost::DesignStats stats =
        Outpost::DesignStatsFor(tuning, Outpost::HullId{row.hull}, Outpost::DriveId{row.drive}, Outpost::WeaponId{row.weapon});
      const std::wstring name = std::to_wstring(row.hull) + L"+" + std::to_wstring(row.drive) + L"+" + std::to_wstring(row.weapon);
      Assert::AreEqual(row.cost, stats.cost, name.c_str());
      Assert::AreEqual(row.hitPoints, static_cast<double>(stats.hitPointsHundredths) / Outpost::HUNDREDTHS, 1e-9, name.c_str());
      Assert::AreEqual(row.armor * Outpost::HUNDREDTHS, stats.armorHundredths, name.c_str());
      Assert::AreEqual(row.speedMetersPerSecond, static_cast<double>(stats.movement.speedMetersPerSecond), 1e-4, name.c_str());
      Assert::AreEqual(row.rangeMeters, static_cast<double>(stats.rangeMeters), 1e-9, name.c_str());
      // Only the Missile Rack splashes, 30 m around its target (task 5.3).
      Assert::AreEqual(row.weapon == 3 ? 30.0f : 0.0f, stats.splashRadiusMeters, name.c_str());
      for (size_t hull = 0; hull < tuning.hulls.size(); ++hull)
      {
        Assert::AreEqual(row.damagePerSecond[hull], Outpost::DamagePerSecond(stats, tuning.hulls[hull].armor * Outpost::HUNDREDTHS), 0.0501,
                         name.c_str());
      }
    }
  }

  // Phase 1 design §5: the Pulse Drive, the Flak Battery and the Rail Cannon, each locked until its topic is researched,
  // and each with its abbreviation.
  TEST_METHOD(DerivesThePhaseOneComponents)
  {
    const Outpost::Tuning tuning = RepositoryTuning();
    constexpr Outpost::DriveId PULSE{3};
    constexpr Outpost::WeaponId FLAK_BATTERY{4};
    constexpr Outpost::WeaponId RAIL_CANNON{5};
    const auto armorOf = [&tuning](size_t _hull) { return tuning.hulls[_hull].armor * Outpost::HUNDREDTHS; };

    // The raider: the fastest and most fragile, with the swarm-breaker's hits splashing 26 m.
    const Outpost::DesignStats raider = Outpost::DesignStatsFor(tuning, Outpost::HullId{1}, PULSE, FLAK_BATTERY);
    Assert::AreEqual(396, raider.cost);
    Assert::AreEqual(16500, raider.hitPointsHundredths);
    Assert::AreEqual(96.0f, raider.movement.speedMetersPerSecond, 1e-4f);
    Assert::AreEqual(270.0f * std::numbers::pi_v<float> / 180.0f, raider.movement.turnRateRadiansPerSecond, 1e-4f);
    Assert::AreEqual(200.0f, raider.rangeMeters);
    Assert::AreEqual(26.0f, raider.splashRadiusMeters);
    // 20 a hit every half second: 18 against a Small hull's armor of 2, and 12 against a Medium hull's 8 (gate H10).
    Assert::AreEqual(36.0, Outpost::DamagePerSecond(raider, armorOf(0)), 1e-9);
    Assert::AreEqual(24.0, Outpost::DamagePerSecond(raider, armorOf(1)), 1e-9);

    // The heavy's gun: 320 a hit every 6 s, from 240 m, past the Lance's 220 m.
    const Outpost::DesignStats rail = Outpost::DesignStatsFor(tuning, Outpost::HullId{3}, Outpost::DriveId{2}, RAIL_CANNON);
    Assert::AreEqual(1590, rail.cost);
    Assert::AreEqual(240.0f, rail.rangeMeters);
    Assert::AreEqual(0.0f, rail.splashRadiusMeters);
    Assert::AreEqual(306.0 / 6.0, Outpost::DamagePerSecond(rail, armorOf(2)), 1e-9);
    // Six hits break a Large+Fusion hull of 1,680.
    Assert::AreEqual(6, (rail.hitPointsHundredths + Outpost::HitHundredths(rail.damageHundredths, armorOf(2)) - 1) /
                          Outpost::HitHundredths(rail.damageHundredths, armorOf(2)));

    const std::array<Outpost::ResearchTopicId, 1> pulseDrive{Outpost::ResearchTopicId{10}};
    const std::array<Outpost::ResearchTopicId, 1> flakBattery{Outpost::ResearchTopicId{11}};
    const std::array<Outpost::ResearchTopicId, 1> railCannon{Outpost::ResearchTopicId{19}};
    Assert::IsFalse(Outpost::IsAvailable(tuning, {}, PULSE));
    Assert::IsFalse(Outpost::IsAvailable(tuning, {}, FLAK_BATTERY));
    Assert::IsFalse(Outpost::IsAvailable(tuning, {}, RAIL_CANNON));
    Assert::IsTrue(Outpost::IsAvailable(tuning, pulseDrive, PULSE));
    Assert::IsTrue(Outpost::IsAvailable(tuning, flakBattery, FLAK_BATTERY));
    Assert::IsTrue(Outpost::IsAvailable(tuning, railCannon, RAIL_CANNON));
    Assert::IsFalse(Outpost::IsAvailable(tuning, railCannon, FLAK_BATTERY));

    Assert::AreEqual(std::string("Small+Pulse+Flak Battery"), Outpost::DesignName(tuning, {Outpost::HullId{1}, PULSE, FLAK_BATTERY}));
    Assert::AreEqual(std::string("P"), Outpost::Abbreviation(tuning.drives[2].name));
    Assert::AreEqual(std::string("FB"), Outpost::Abbreviation(tuning.weapons[3].name));
    Assert::AreEqual(std::string("RC"), Outpost::Abbreviation(tuning.weapons[4].name));
  }

  // Design §7: max(damage × 0.25, damage − armor), in hundredths.
  // Phase 2 design §10: a module's name follows the weapon's in a design's name, and its numbers change the design's.
  TEST_METHOD(AModuleJoinsTheDesign)
  {
    const Outpost::Tuning tuning = Outpost::LoadTuning(ReadRepositoryTuning());
    const Outpost::DesignComponents scout{Outpost::HullId{1}, Outpost::DriveId{1}, Outpost::WeaponId{1}, Outpost::ModuleId{1}};
    Assert::AreEqual(std::string("Small+Ion+Mass Driver+Sensor Array"), Outpost::DesignName(tuning, scout));
    const Outpost::DesignStats with = Outpost::DesignStatsFor(tuning, scout);
    const Outpost::DesignStats without = Outpost::DesignStatsFor(tuning, Outpost::HullId{1}, Outpost::DriveId{1}, Outpost::WeaponId{1});
    Assert::AreEqual(without.cost + 120, with.cost);
    Assert::AreEqual(without.movement.speedMetersPerSecond * 0.9f, with.movement.speedMetersPerSecond, 1e-4f);
    Assert::AreEqual(0.0f, without.moduleSightMeters);
    Assert::AreEqual(700.0f, with.moduleSightMeters);
    Assert::AreEqual(without.damageHundredths, with.damageHundredths);
    Assert::AreEqual(without.hitPointsHundredths, with.hitPointsHundredths);
  }

  TEST_METHOD(ArmorTakesItsShareOfAHit)
  {
    Assert::AreEqual(600, Outpost::HitHundredths(1400, 800));
    Assert::AreEqual(350, Outpost::HitHundredths(1400, 1400));
    // Design §6: a Defence Platform's armor of 10 takes a Mass Driver hit from 14 to 4.
    Assert::AreEqual(400, Outpost::HitHundredths(1400, 1000));
    Assert::AreEqual(9300, Outpost::HitHundredths(9500, 200));
    Assert::AreEqual(1400, Outpost::HitHundredths(1400, 0));
    // Fractional armor and damage, as the balance check's robustness sweep makes them.
    Assert::AreEqual(630, Outpost::HitHundredths(1470, 840));
  }

  // Design §7: the first minutes are played with the four designs of the components no research unlocks. ADR-069: each is
  // saved under the short name the tuning data gives it, the MVP design's nicknames, and keeps its components' name besides.
  TEST_METHOD(StartingDesignsAreTheFourOfTheFirstMinutes)
  {
    const Outpost::Tuning tuning = RepositoryTuning();
    std::vector<std::string> names;
    std::vector<std::string> shortNames;
    for (const Outpost::DesignComponents& components : Outpost::StartingDesigns(tuning))
    {
      names.push_back(Outpost::DesignName(tuning, components));
      shortNames.push_back(Outpost::StartingDesignName(tuning, components));
    }
    const std::vector<std::string> expected{"Small+Ion+Mass Driver", "Small+Ion+Lance", "Medium+Ion+Mass Driver", "Medium+Ion+Lance"};
    Assert::IsTrue(names == expected);
    const std::vector<std::string> nicknames{"Swarm", "Picket", "Brawler", "Lancer"};
    Assert::IsTrue(shortNames == nicknames);
    // A design the file does not name, such as a scout of a starting design's components, keeps its components' name.
    const Outpost::DesignComponents scout{Outpost::HullId{1}, Outpost::DriveId{1}, Outpost::WeaponId{1}, Outpost::ModuleId{1}};
    Assert::AreEqual(std::string("Small+Ion+Mass Driver+Sensor Array"), Outpost::StartingDesignName(tuning, scout));
  }

  // Design §6, §7, §9: every match starts with the starting designs saved, the starting Ore, and a base of a Command
  // Station and two Constructors, which are of no design.
  TEST_METHOD(EveryPlayerStartsWithItsDesignsAndABase)
  {
    const Outpost::Tuning tuning = RepositoryTuning();
    const Outpost::Map map = Outpost::LoadMap(ReadRepositoryMap());
    Outpost::InProcessServer server(tuning, map, {.seed = 1});
    server.World().PlaceStartingBases(map);

    for (const Outpost::PlayerId player : {BLUE, RED})
    {
      const Outpost::Snapshot snapshot = server.World().BuildSnapshot(player);
      Assert::AreEqual(size_t{4}, snapshot.designs.size());
      Assert::AreEqual(std::string("Swarm"), snapshot.designs[0].nameUtf8, L"saved under its short name (ADR-069)");
      Assert::AreEqual(tuning.rules.startingOre, snapshot.ore);
      size_t constructors = 0;
      size_t stations = 0;
      for (const Outpost::EntityView& entity : snapshot.entities)
      {
        if (entity.owner != player)
          continue;
        if (entity.kind == Outpost::EntityKind::Structure)
        {
          stations += entity.structure == Outpost::StructureKind::CommandStation ? 1 : 0;
          continue;
        }
        Assert::IsTrue(entity.role == Outpost::ShipRole::Constructor);
        Assert::IsFalse(entity.design.IsValid());
        Assert::AreEqual(tuning.constructor.hitPoints * Outpost::HUNDREDTHS, entity.maxHitPointsHundredths);
        ++constructors;
      }
      Assert::AreEqual(size_t{1}, stations);
      Assert::AreEqual(static_cast<size_t>(tuning.rules.startingConstructors), constructors);
    }
  }

  TEST_METHOD(RefusesAShipOfAnotherPlayersDesign)
  {
    const Outpost::Tuning tuning = RepositoryTuning();
    Outpost::Simulation simulation(1, 20);
    simulation.SaveStartingDesigns(BLUE, tuning);
    const Outpost::DesignId blueDesign = simulation.FindDesign(BLUE, Outpost::StartingDesigns(tuning).front())->id;
    Assert::ExpectException<Neuron::Exception>([&] { (void)simulation.SpawnShip(RED, blueDesign, {}); });
    Assert::ExpectException<Neuron::Exception>([&] { (void)simulation.SpawnShip(BLUE, Outpost::DesignId{99}, {}); });
  }
};
} // namespace GameLogicTests
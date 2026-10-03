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
// per second to one decimal.
constexpr std::array<ModelRow, 18> MODEL_TABLE{{
  {1, 1, 1, 87, 198, 2, 78.0, 120, {30.0, 15.0, 8.8}},
  {1, 1, 2, 137, 198, 2, 78.0, 220, {34.4, 32.2, 30.0}},
  {1, 1, 3, 182, 198, 2, 78.0, 280, {14.0, 11.0, 8.0}},
  {1, 2, 1, 147, 308, 2, 48.0, 120, {30.0, 15.0, 8.8}},
  {1, 2, 2, 197, 308, 2, 48.0, 220, {34.4, 32.2, 30.0}},
  {1, 2, 3, 242, 308, 2, 48.0, 280, {14.0, 11.0, 8.0}},
  {2, 1, 1, 165, 450, 8, 52.0, 120, {30.0, 15.0, 8.8}},
  {2, 1, 2, 215, 450, 8, 52.0, 220, {34.4, 32.2, 30.0}},
  {2, 1, 3, 260, 450, 8, 52.0, 280, {14.0, 11.0, 8.0}},
  {2, 2, 1, 225, 700, 8, 32.0, 120, {30.0, 15.0, 8.8}},
  {2, 2, 2, 275, 700, 8, 32.0, 220, {34.4, 32.2, 30.0}},
  {2, 2, 3, 320, 700, 8, 32.0, 280, {14.0, 11.0, 8.0}},
  {3, 1, 1, 355, 1080, 14, 32.5, 120, {30.0, 15.0, 8.8}},
  {3, 1, 2, 405, 1080, 14, 32.5, 220, {34.4, 32.2, 30.0}},
  {3, 1, 3, 450, 1080, 14, 32.5, 280, {14.0, 11.0, 8.0}},
  {3, 2, 1, 415, 1680, 14, 20.0, 120, {30.0, 15.0, 8.8}},
  {3, 2, 2, 465, 1680, 14, 20.0, 220, {34.4, 32.2, 30.0}},
  {3, 2, 3, 510, 1680, 14, 20.0, 280, {14.0, 11.0, 8.0}},
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

    // The raider: the fastest and most fragile, with the swarm-breaker's small hits splashing 20 m.
    const Outpost::DesignStats raider = Outpost::DesignStatsFor(tuning, Outpost::HullId{1}, PULSE, FLAK_BATTERY);
    Assert::AreEqual(142, raider.cost);
    Assert::AreEqual(16500, raider.hitPointsHundredths);
    Assert::AreEqual(96.0f, raider.movement.speedMetersPerSecond, 1e-4f);
    Assert::AreEqual(270.0f * std::numbers::pi_v<float> / 180.0f, raider.movement.turnRateRadiansPerSecond, 1e-4f);
    Assert::AreEqual(140.0f, raider.rangeMeters);
    Assert::AreEqual(20.0f, raider.splashRadiusMeters);
    // 9 a hit: 7 against a Small hull's armor of 2, and a quarter, 2.25, against a Medium hull's 8.
    Assert::AreEqual(14.0, Outpost::DamagePerSecond(raider, armorOf(0)), 1e-9);
    Assert::AreEqual(4.5, Outpost::DamagePerSecond(raider, armorOf(1)), 1e-9);

    // The heavy's gun: 320 a hit every 6 s, from 240 m, past the Lance's 220 m.
    const Outpost::DesignStats rail = Outpost::DesignStatsFor(tuning, Outpost::HullId{3}, Outpost::DriveId{2}, RAIL_CANNON);
    Assert::AreEqual(530, rail.cost);
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
  TEST_METHOD(ArmorTakesItsShareOfAHit)
  {
    Assert::AreEqual(600, Outpost::HitHundredths(1400, 800));
    Assert::AreEqual(350, Outpost::HitHundredths(1400, 1400));
    // Design §6: a Defence Platform's armor of 10 takes a Mass Driver hit from 14 to 4.
    Assert::AreEqual(400, Outpost::HitHundredths(1400, 1000));
    Assert::AreEqual(9300, Outpost::HitHundredths(9500, 200));
    Assert::AreEqual(1400, Outpost::HitHundredths(1400, 0));
    // Fractional armor and damage, as the Q2 check's robustness sweep makes them.
    Assert::AreEqual(630, Outpost::HitHundredths(1470, 840));
  }

  // Design §7: the first minutes are played with the four designs of the components no research unlocks.
  TEST_METHOD(StartingDesignsAreTheFourOfTheFirstMinutes)
  {
    const Outpost::Tuning tuning = RepositoryTuning();
    std::vector<std::string> names;
    for (const Outpost::DesignComponents& components : Outpost::StartingDesigns(tuning))
      names.push_back(Outpost::DesignName(tuning, components));
    const std::vector<std::string> expected{"Small+Ion+Mass Driver", "Small+Ion+Lance", "Medium+Ion+Mass Driver", "Medium+Ion+Lance"};
    Assert::IsTrue(names == expected);
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
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

// Copied from the model's output on 2026-10-01, the Missile Rack's rows with task 5.3. The model rounds damage per second
// to one decimal.
constexpr std::array<ModelRow, 18> MODEL_TABLE{{
  {1, 1, 1, 87, 198, 2, 78.0, 120, {30.0, 15.0, 8.8}},
  {1, 1, 2, 137, 198, 2, 78.0, 220, {31.0, 29.0, 27.0}},
  {1, 1, 3, 162, 198, 2, 78.0, 280, {19.0, 16.0, 13.0}},
  {1, 2, 1, 147, 308, 2, 48.0, 120, {30.0, 15.0, 8.8}},
  {1, 2, 2, 197, 308, 2, 48.0, 220, {31.0, 29.0, 27.0}},
  {1, 2, 3, 222, 308, 2, 48.0, 280, {19.0, 16.0, 13.0}},
  {2, 1, 1, 165, 450, 8, 52.0, 120, {30.0, 15.0, 8.8}},
  {2, 1, 2, 215, 450, 8, 52.0, 220, {31.0, 29.0, 27.0}},
  {2, 1, 3, 240, 450, 8, 52.0, 280, {19.0, 16.0, 13.0}},
  {2, 2, 1, 225, 700, 8, 32.0, 120, {30.0, 15.0, 8.8}},
  {2, 2, 2, 275, 700, 8, 32.0, 220, {31.0, 29.0, 27.0}},
  {2, 2, 3, 300, 700, 8, 32.0, 280, {19.0, 16.0, 13.0}},
  {3, 1, 1, 355, 1080, 14, 32.5, 120, {30.0, 15.0, 8.8}},
  {3, 1, 2, 405, 1080, 14, 32.5, 220, {31.0, 29.0, 27.0}},
  {3, 1, 3, 430, 1080, 14, 32.5, 280, {19.0, 16.0, 13.0}},
  {3, 2, 1, 415, 1680, 14, 20.0, 120, {30.0, 15.0, 8.8}},
  {3, 2, 2, 465, 1680, 14, 20.0, 220, {31.0, 29.0, 27.0}},
  {3, 2, 3, 490, 1680, 14, 20.0, 280, {19.0, 16.0, 13.0}},
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
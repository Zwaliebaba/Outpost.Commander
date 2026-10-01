#include "pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameAppTests
{
namespace
{
constexpr Outpost::PlayerId PLAYER{1};
constexpr Outpost::HullId SMALL{1};
constexpr Outpost::HullId MEDIUM{2};
constexpr Outpost::HullId LARGE{3};
constexpr Outpost::DriveId ION{1};
constexpr Outpost::DriveId FUSION{2};
constexpr Outpost::WeaponId MASS_DRIVER{1};
constexpr Outpost::WeaponId LANCE{2};
constexpr Outpost::DesignId SWARM{1};

// A snapshot with OutpostCommander/Assets/Tuning.json's components as the server sends them before any research: the
// Large hull, the Fusion drive and the Missile Rack locked (design §7, §8). _unlocked opens all three.
Outpost::Snapshot Components(bool _unlocked = false)
{
  Outpost::Snapshot snapshot{.tick = 1, .player = PLAYER, .ore = 1000};
  snapshot.hulls = {{SMALL, "Small", 22000, 200, 60.0, 180.0, 8.0, 32, 10.0, true},
                    {MEDIUM, "Medium", 50000, 800, 40.0, 120.0, 14.0, 110, 20.0, true},
                    {LARGE, "Large", 120000, 1400, 25.0, 60.0, 24.0, 300, 40.0, _unlocked}};
  snapshot.drives = {{ION, "Ion", 1.3, 0.9, 1.25, 20, true}, {FUSION, "Fusion", 0.8, 1.4, 0.8, 80, _unlocked}};
  snapshot.weapons = {{MASS_DRIVER, "Mass Driver", 1400, 0.4, 120.0, 35, true},
                      {LANCE, "Lance", 9500, 3.0, 220.0, 85, true},
                      {Outpost::WeaponId{3}, "Missile Rack", 4000, 2.0, 280.0, 110, _unlocked}};
  snapshot.designs = {{.id = SWARM, .nameUtf8 = "Swarm", .hull = SMALL, .drive = ION, .weapon = MASS_DRIVER, .cost = 87}};
  return snapshot;
}

// One row of the table `python Tools/BattleModel.py` prints, as GameLogicTests' DesignTests holds it.
struct ModelRow
{
  Outpost::HullId hull;
  Outpost::DriveId drive;
  Outpost::WeaponId weapon;
  std::int32_t cost;
  double hitPoints;
  double speedMetersPerSecond;
  std::array<double, 3> damagePerSecond;
};

constexpr std::array<ModelRow, 4> MODEL_ROWS{{
  {SMALL, ION, MASS_DRIVER, 87, 198, 78.0, {30.0, 15.0, 8.8}},
  {MEDIUM, ION, LANCE, 215, 450, 52.0, {31.0, 29.0, 27.0}},
  {LARGE, FUSION, MASS_DRIVER, 415, 1680, 20.0, {30.0, 15.0, 8.8}},
  {LARGE, FUSION, LANCE, 465, 1680, 20.0, {31.0, 29.0, 27.0}},
}};

Outpost::Designer Picking(Outpost::HullId _hull, Outpost::DriveId _drive, Outpost::WeaponId _weapon)
{
  Outpost::Designer designer;
  designer.PickHull(_hull);
  designer.PickDrive(_drive);
  designer.PickWeapon(_weapon);
  return designer;
}

Outpost::Designer Typing(const Outpost::Snapshot& _snapshot)
{
  Outpost::Designer designer;
  designer.Update(_snapshot);
  designer.BeginEditing(_snapshot);
  return designer;
}

Neuron::InputEvent Key(std::uint8_t _key)
{
  return {.kind = Neuron::InputEventKind::KeyDown, .key = _key};
}

Neuron::InputEvent Character(std::uint32_t _character)
{
  return {.kind = Neuron::InputEventKind::Character, .character = _character};
}
} // namespace

TEST_CLASS(DesignerTests)
{
public:
  // Every slot holds an available component: the first, until the player picks another, and the first again when the
  // picked one is locked.
  TEST_METHOD(PicksAvailableComponents)
  {
    const Outpost::Snapshot snapshot = Components();
    Outpost::Designer designer;
    designer.Update(snapshot);
    Assert::IsTrue(designer.Picked() == Outpost::DesignComponents{SMALL, ION, MASS_DRIVER});
    designer.PickWeapon(LANCE);
    designer.PickHull(LARGE);
    designer.Update(snapshot);
    Assert::IsTrue(designer.Picked() == Outpost::DesignComponents{SMALL, ION, LANCE}, L"the Large hull is locked");
  }

  // Design §9, task 5.2: the live stats are the battle model's, damage per second after armor against each hull
  // included.
  TEST_METHOD(ShowsTheBattleModelsStats)
  {
    const Outpost::Snapshot snapshot = Components(true);
    for (const ModelRow& row : MODEL_ROWS)
    {
      const Outpost::DesignStats stats = Picking(row.hull, row.drive, row.weapon).Stats(snapshot).value_or(Outpost::DesignStats{});
      Assert::AreEqual(row.cost, stats.cost);
      Assert::AreEqual(row.hitPoints, static_cast<double>(stats.hitPointsHundredths) / Outpost::HUNDREDTHS, 1e-9);
      Assert::AreEqual(row.speedMetersPerSecond, static_cast<double>(stats.movement.speedMetersPerSecond), 1e-4);
      for (size_t target = 0; target < snapshot.hulls.size(); ++target)
        Assert::AreEqual(row.damagePerSecond[target], Outpost::DamagePerSecond(stats, snapshot.hulls[target].armorHundredths), 0.0501);
    }
  }

  // A saved design shows its name and offers a rename only once the name changes; new components show a name made of
  // them and can be saved once each is unlocked.
  TEST_METHOD(SavesNewDesignsAndRenamesSavedOnes)
  {
    Outpost::Snapshot snapshot = Components();
    Outpost::Designer swarm = Picking(SMALL, ION, MASS_DRIVER);
    Assert::AreEqual(std::string("Swarm"), swarm.Name(snapshot));
    Assert::IsFalse(swarm.SaveCommand(snapshot).has_value(), L"nothing to change");

    Outpost::Designer picket = Picking(SMALL, ION, LANCE);
    Assert::AreEqual(std::string("Small+Ion+Lance"), picket.Name(snapshot));
    const Outpost::SaveDesignCommand save = picket.SaveCommand(snapshot).value_or(Outpost::SaveDesignCommand{});
    Assert::IsFalse(save.design.IsValid(), L"a new design");
    Assert::AreEqual(std::string("Small+Ion+Lance"), save.nameUtf8);
    Assert::IsTrue(save.weapon == LANCE);

    Outpost::Designer heavy = Picking(LARGE, FUSION, LANCE);
    Assert::IsFalse(heavy.SaveCommand(snapshot).has_value(), L"locked");
    snapshot = Components(true);
    Assert::IsTrue(heavy.SaveCommand(snapshot).has_value());

    swarm.BeginEditing(snapshot);
    (void)swarm.Edit(Character('!'), snapshot);
    const Outpost::SaveDesignCommand rename = swarm.Edit(Key(VK_RETURN), snapshot).value_or(Outpost::SaveDesignCommand{});
    Assert::IsTrue(rename.design == SWARM);
    Assert::AreEqual(std::string("Swarm!"), rename.nameUtf8);
    Assert::IsFalse(swarm.IsEditing());
    Assert::AreEqual(std::string("Swarm"), swarm.Name(snapshot), L"the name follows the snapshot again");
  }

  // The name takes printable ASCII up to the limit and Backspace; Escape drops what was typed, and a name the server
  // would refuse saves nothing.
  TEST_METHOD(TypesTheName)
  {
    const Outpost::Snapshot snapshot = Components();
    Outpost::Designer designer = Typing(snapshot);
    Assert::IsTrue(designer.IsEditing());
    for (std::uint8_t i = 0; i < 5; ++i)
      (void)designer.Edit(Key(VK_BACK), snapshot);
    (void)designer.Edit(Character('X'), snapshot);
    (void)designer.Edit(Character(0xe9), snapshot);
    (void)designer.Edit(Character('\t'), snapshot);
    (void)designer.Edit(Key('A'), snapshot);
    Assert::AreEqual(std::string("X"), designer.Name(snapshot), L"Backspace, one character, and nothing the font lacks");
    (void)designer.Edit(Key(VK_ESCAPE), snapshot);
    Assert::IsFalse(designer.IsEditing());
    Assert::AreEqual(std::string("Swarm"), designer.Name(snapshot));

    designer.PickWeapon(LANCE);
    designer.BeginEditing(snapshot);
    for (size_t i = 0; i < 40; ++i)
      (void)designer.Edit(Character('z'), snapshot);
    Assert::AreEqual(Outpost::DESIGN_NAME_LIMIT, designer.Name(snapshot).size());
    while (!designer.Name(snapshot).empty())
      (void)designer.Edit(Key(VK_BACK), snapshot);
    (void)designer.Edit(Character(' '), snapshot);
    Assert::IsFalse(designer.Edit(Key(VK_RETURN), snapshot).has_value(), L"a blank name saves nothing");
  }
};
} // namespace GameAppTests

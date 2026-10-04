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
  snapshot.weapons = {{MASS_DRIVER, "Mass Driver", 1400, 0.4, 120.0, 0.0, 35, true},
                      {LANCE, "Lance", 9500, 3.0, 220.0, 0.0, 85, true},
                      {Outpost::WeaponId{3}, "Missile Rack", 4000, 2.0, 280.0, 30.0, 110, _unlocked}};
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
  // Phase 2 design §10: the module slot starts empty and stays so until the player picks one; a module's cost and speed
  // join the design's, its name follows the weapon's, by its initials where the whole is too long a name, and saving and
  // loading carry it.
  TEST_METHOD(PicksAModuleOrNone)
  {
    constexpr Outpost::ModuleId SENSOR_ARRAY{1};
    Outpost::Snapshot newest = Components();
    newest.modules = {
      {.id = SENSOR_ARRAY, .nameUtf8 = "Sensor Array", .sightMeters = 700.0, .speedFactor = 0.9, .cost = 40, .available = true}};
    Outpost::Designer designer = Picking(SMALL, ION, MASS_DRIVER);
    designer.Update(newest);
    Assert::IsFalse(designer.Picked().module.IsValid(), L"no module until one is picked");
    Assert::IsNotNull(designer.Match(newest));

    designer.PickModule(SENSOR_ARRAY);
    designer.Update(newest);
    Assert::IsTrue(designer.Picked().module == SENSOR_ARRAY);
    Assert::IsNull(designer.Match(newest), L"the Swarm has no module");
    Assert::AreEqual(std::string("Small+Ion+Mass Driver+SA"), designer.Name(newest), L"the whole name is too long");
    const std::optional<Outpost::DesignStats> stats = designer.Stats(newest);
    Assert::IsTrue(stats.has_value());
    Assert::AreEqual(87 + 40, stats.value_or(Outpost::DesignStats{}).cost);
    Assert::AreEqual(78.0f * 0.9f, stats.value_or(Outpost::DesignStats{}).movement.speedMetersPerSecond, 1e-3f);
    Assert::AreEqual(700.0f, stats.value_or(Outpost::DesignStats{}).moduleSightMeters);
    const std::optional<Outpost::SaveDesignCommand> save = designer.SaveCommand(newest);
    Assert::IsTrue(save.has_value() && save->module == SENSOR_ARRAY && !save->design.IsValid());

    designer.PickModule({});
    Assert::IsNotNull(designer.Match(newest));
    designer.Load(
      {.id = Outpost::DesignId{2}, .nameUtf8 = "Scout", .hull = SMALL, .drive = ION, .weapon = MASS_DRIVER, .module = SENSOR_ARRAY});
    Assert::IsTrue(designer.Picked().module == SENSOR_ARRAY);

    newest.modules.clear();
    designer.Update(newest);
    Assert::IsFalse(designer.Picked().module.IsValid(), L"a module the snapshot no longer lists is dropped");
  }

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

  // ADR-023: Queue on picks that are no saved design yet sends their save once and queues every press at its Shipyard
  // when the design comes back in a snapshot. A save that does not come back in time drops its queues.
  TEST_METHOD(QueuesADesignOnceItIsSaved)
  {
    constexpr Outpost::EntityId YARD{30};
    constexpr Outpost::DesignId HEAVY{2};
    Outpost::Designer heavy = Picking(LARGE, FUSION, LANCE);
    Assert::IsFalse(heavy.SaveAndQueue(YARD, Components()).has_value(), L"locked");
    Assert::IsTrue(heavy.TakeQueueCommands(Components()).empty(), L"a locked design waits for nothing");

    Outpost::Snapshot snapshot = Components(true);
    const Outpost::SaveDesignCommand save = heavy.SaveAndQueue(YARD, snapshot).value_or(Outpost::SaveDesignCommand{});
    Assert::IsFalse(save.design.IsValid(), L"a new design");
    Assert::AreEqual(std::string("Large+Fusion+Lance"), save.nameUtf8);
    Assert::IsFalse(heavy.SaveAndQueue(YARD, snapshot).has_value(), L"the save is on its way");
    Assert::IsTrue(heavy.TakeQueueCommands(snapshot).empty(), L"not saved yet");

    snapshot.tick = 2;
    snapshot.designs.push_back({.id = HEAVY, .nameUtf8 = save.nameUtf8, .hull = LARGE, .drive = FUSION, .weapon = LANCE, .cost = 465});
    const std::vector<Outpost::QueueShipCommand> queues = heavy.TakeQueueCommands(snapshot);
    Assert::AreEqual(size_t{2}, queues.size(), L"one for each press");
    Assert::IsTrue(queues[0].producer == YARD && queues[0].design == HEAVY);
    Assert::IsTrue(queues[1].producer == YARD && queues[1].design == HEAVY);
    Assert::IsTrue(heavy.TakeQueueCommands(snapshot).empty(), L"taken once");
    Assert::IsFalse(heavy.SaveAndQueue(YARD, snapshot).has_value(), L"saved, so the HUD queues it as it is");

    Outpost::Designer refused = Picking(LARGE, ION, LANCE);
    snapshot.tick = 100;
    Assert::IsTrue(refused.SaveAndQueue(YARD, snapshot).has_value());
    snapshot.tick += Outpost::Designer::SAVE_WAIT_TICKS;
    Assert::IsTrue(refused.TakeQueueCommands(snapshot).empty(), L"still waiting");
    ++snapshot.tick;
    Assert::IsTrue(refused.TakeQueueCommands(snapshot).empty(), L"refused");
    snapshot.designs.push_back({.id = Outpost::DesignId{3}, .nameUtf8 = "Late", .hull = LARGE, .drive = ION, .weapon = LANCE, .cost = 405});
    Assert::IsTrue(refused.TakeQueueCommands(snapshot).empty(), L"dropped once the wait was over");
  }

  // Phase 1 design §11: the designer's queue goes to one of the player's finished Shipyards: the lowest numbered at first,
  // one the player picks, the next or the previous by number round the end, and the lowest again when its own is gone.
  TEST_METHOD(AimsTheQueueAtAShipyard)
  {
    Outpost::Snapshot snapshot = Components();
    Outpost::Designer designer;
    designer.Update(snapshot);
    Assert::IsNull(designer.Target(snapshot), L"no Shipyard yet");

    const auto yard = [](std::uint32_t _id, std::uint32_t _number)
    {
      return Outpost::EntityView{.id = Outpost::EntityId{_id},
                                 .kind = Outpost::EntityKind::Structure,
                                 .owner = PLAYER,
                                 .structure = Outpost::StructureKind::Shipyard,
                                 .shipyardNumber = _number};
    };
    snapshot.entities = {yard(40, 2), yard(30, 1), yard(50, 3)};
    Outpost::EntityView building = yard(60, 0);
    building.builtPermille = 500;
    Outpost::EntityView theirs = yard(70, 1);
    theirs.owner = Outpost::PlayerId{2};
    snapshot.entities.push_back(building);
    snapshot.entities.push_back(theirs);
    const auto target = [&] { return designer.Target(snapshot)->id.value; };
    Assert::AreEqual(30u, target(), L"the lowest numbered");
    designer.StepTarget(1, snapshot);
    Assert::AreEqual(40u, target());
    designer.StepTarget(1, snapshot);
    designer.StepTarget(1, snapshot);
    Assert::AreEqual(30u, target(), L"round the end, past the unfinished and the enemy's");
    designer.StepTarget(-1, snapshot);
    Assert::AreEqual(50u, target());
    designer.SetTarget(Outpost::EntityId{70}, snapshot);
    Assert::AreEqual(50u, target(), L"not the enemy's");
    designer.SetTarget(Outpost::EntityId{40}, snapshot);
    Assert::AreEqual(40u, target());

    std::erase_if(snapshot.entities, [](const Outpost::EntityView& _entity) { return _entity.id == Outpost::EntityId{40}; });
    designer.Update(snapshot);
    Assert::AreEqual(30u, target(), L"its own is gone");
  }

  // Phase 1 design §11: one Queue asks for from 1 up to as many ships as the target's queue has free slots (owner,
  // 2026-10-02), and asks a new design's save for each.
  TEST_METHOD(QueuesAsManyAsTheQueueHasRoomFor)
  {
    Outpost::Snapshot snapshot = Components(true);
    snapshot.entities = {{.id = Outpost::EntityId{30},
                          .kind = Outpost::EntityKind::Structure,
                          .owner = PLAYER,
                          .structure = Outpost::StructureKind::Shipyard,
                          .shipyardNumber = 1,
                          .queue = {{.design = SWARM}, {.design = SWARM}}}};
    Outpost::Designer designer = Picking(LARGE, FUSION, LANCE);
    Assert::AreEqual(1u, designer.Count(snapshot));
    designer.StepCount(-1, snapshot);
    Assert::AreEqual(1u, designer.Count(snapshot), L"never fewer than one");
    for (int i = 0; i < 5; ++i)
      designer.StepCount(1, snapshot);
    Assert::AreEqual(3u, designer.Count(snapshot), L"three slots are free");
    snapshot.entities.front().queue.push_back({.design = SWARM});
    Assert::AreEqual(2u, designer.Count(snapshot), L"fewer once the queue fills");

    const Outpost::SaveDesignCommand save =
      designer.SaveAndQueue(Outpost::EntityId{30}, snapshot, designer.Count(snapshot)).value_or(Outpost::SaveDesignCommand{});
    snapshot.tick = 2;
    snapshot.designs.push_back({.id = Outpost::DesignId{2}, .nameUtf8 = save.nameUtf8, .hull = LARGE, .drive = FUSION, .weapon = LANCE});
    Assert::AreEqual(size_t{2}, designer.TakeQueueCommands(snapshot).size(), L"one save, two ships");
  }

  // A saved design's chip loads its components, and the name follows it again.
  TEST_METHOD(LoadsASavedDesign)
  {
    const Outpost::Snapshot snapshot = Components(true);
    Outpost::Designer designer = Picking(LARGE, FUSION, LANCE);
    designer.BeginEditing(snapshot);
    designer.Load(snapshot.designs.front());
    Assert::IsTrue(designer.Picked() == Outpost::DesignComponents{SMALL, ION, MASS_DRIVER});
    Assert::IsFalse(designer.IsEditing());
    Assert::AreEqual(std::string("Swarm"), designer.Name(snapshot));
  }

  // Phase 1 design §11: the saved designs' chips scroll sideways, from the first to the last, and no further.
  TEST_METHOD(ScrollsTheSavedDesigns)
  {
    Outpost::Designer designer;
    Assert::AreEqual(size_t{0}, designer.FirstChip());
    designer.StepChips(-1, 5);
    Assert::AreEqual(size_t{0}, designer.FirstChip(), L"not before the first");
    for (int i = 0; i < 7; ++i)
      designer.StepChips(1, 5);
    Assert::AreEqual(size_t{4}, designer.FirstChip(), L"not past the last");
    designer.StepChips(-1, 5);
    Assert::AreEqual(size_t{3}, designer.FirstChip());
    designer.StepChips(0, 0);
    Assert::AreEqual(size_t{0}, designer.FirstChip(), L"none saved");
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

#include "pch.h"
#include "MatchArena.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameLogicTests
{
namespace
{
constexpr Outpost::PlayerId BLUE = MatchArena::BLUE;
constexpr Outpost::PlayerId RED = MatchArena::RED;
constexpr Outpost::HullId SMALL{1};
constexpr Outpost::WeaponId MASS_DRIVER{1};
constexpr Outpost::WeaponId LANCE{2};
constexpr Outpost::PlanePosition YARD{.xMeters = 0.0f, .zMeters = 0.0f};
constexpr Outpost::PlanePosition LAB{.xMeters = -600.0f, .zMeters = -600.0f};

// The repository's topics that Reinforced Structures needs (design §8, Phase 1 design §6).
constexpr Outpost::ResearchTopicId IMPROVED_EXTRACTION{1};
constexpr Outpost::ResearchTopicId HULL_PLATING{2};
constexpr Outpost::ResearchTopicId REINFORCED_STRUCTURES{14};

Outpost::Command Upgrade(Outpost::PlayerId _player, Outpost::EntityId _structure, std::vector<Outpost::EntityId> _constructors = {})
{
  return Order(_player, Outpost::UpgradeStructureCommand{.structure = _structure, .constructors = std::move(_constructors)});
}

// A Constructor of _owner's already in reach of a structure of _radius at _at.
Outpost::EntityId ConstructorBeside(MatchArena& _arena, Outpost::PlayerId _owner, Outpost::PlanePosition _at, float _radius,
                                    float _sideMeters = 0.0f)
{
  return _arena.World().SpawnConstructor(_owner,
                                         {.xMeters = _at.xMeters - (_radius + 10.0f + 15.0f), .zMeters = _at.zMeters + _sideMeters});
}

// The ticks one Constructor takes to build the level after _level of a structure of this kind (Phase 3 design §4).
std::uint32_t LevelTicks(const MatchArena& _arena, Outpost::StructureKind _kind, std::int32_t _level)
{
  const Outpost::StructureLevelTuning& next = _arena.StructureData(_kind).levels[static_cast<size_t>(_level - 1)];
  return static_cast<std::uint32_t>(std::lround(next.buildConstructorSeconds * MatchArena::TICKS_PER_SECOND));
}

// Ticks until the structure reaches _level, at most _limit.
std::uint32_t TicksToLevel(MatchArena& _arena, Outpost::EntityId _structure, std::int32_t _level, std::uint32_t _limit)
{
  std::uint32_t ticks = 0;
  while (_arena.Get(_structure).level < _level && ticks < _limit)
  {
    _arena.Run(1);
    ++ticks;
  }
  return ticks;
}

std::int32_t OreOf(MatchArena& _arena, Outpost::PlayerId _player)
{
  return static_cast<std::int32_t>(_arena.World().OreHundredths(_player) / Outpost::HUNDREDTHS);
}

const Outpost::EntityView* FindView(const Outpost::Snapshot& _snapshot, Outpost::EntityId _id)
{
  const auto found = std::ranges::find(_snapshot.entities, _id, &Outpost::EntityView::id);
  return found != _snapshot.entities.end() ? &*found : nullptr;
}
} // namespace

// Phase 3 design §4, ADR-064: a structure upgraded one level at a time by Constructors, paid when ordered.
TEST_CLASS(UpgradeTests)
{
public:
  // The order pays the next level's Ore at once, and the server refuses what cannot be upgraded, costing nothing.
  TEST_METHOD(PaysForAnUpgradeAndRefusesWhatItCannot)
  {
    MatchArena arena;
    const Outpost::EntityId yard = arena.Structure(BLUE, Outpost::StructureKind::Shipyard, YARD);
    const Outpost::EntityId platform = arena.Structure(BLUE, Outpost::StructureKind::DefensePlatform, {.xMeters = 300.0f, .zMeters = 0.0f});
    const Outpost::EntityId relay = arena.Structure(BLUE, Outpost::StructureKind::Relay, {.xMeters = 300.0f, .zMeters = -300.0f});
    const Outpost::EntityId enemy = arena.Structure(RED, Outpost::StructureKind::Shipyard, {.xMeters = 0.0f, .zMeters = -600.0f});
    const Outpost::EntityId warship = arena.Ship(BLUE, SMALL, MASS_DRIVER, {.xMeters = -300.0f, .zMeters = 300.0f});
    const Outpost::EntityId builder = arena.World().SpawnConstructor(BLUE, {.xMeters = -600.0f, .zMeters = 0.0f});
    const auto result = [&](Outpost::Command _command) { return arena.Tick({std::move(_command)})[0]; };

    // A site still being built.
    Assert::IsTrue(result(Order(BLUE, Outpost::BuildStructureCommand{.constructors = {builder},
                                                                     .structure = Outpost::StructureKind::ResearchLab,
                                                                     .position = LAB})) == Outpost::CommandResult::Applied);
    const Outpost::EntityId site = arena.Owned(BLUE, Outpost::EntityKind::Structure).back()->id;
    const std::int32_t before = OreOf(arena, BLUE);
    Assert::IsTrue(result(Upgrade(BLUE, site)) == Outpost::CommandResult::UnderConstruction);
    Assert::IsTrue(result(Upgrade(BLUE, Outpost::EntityId{999})) == Outpost::CommandResult::UnknownEntity);
    Assert::IsTrue(result(Upgrade(BLUE, enemy)) == Outpost::CommandResult::NotUpgradable);
    Assert::IsTrue(result(Upgrade(BLUE, warship)) == Outpost::CommandResult::NotUpgradable);
    Assert::IsTrue(result(Upgrade(BLUE, yard, {warship})) == Outpost::CommandResult::NotAConstructor);
    // A kind that does not grow is at its top level from the start.
    Assert::IsTrue(result(Upgrade(BLUE, platform)) == Outpost::CommandResult::TopLevel);
    Assert::IsTrue(result(Upgrade(BLUE, relay)) == Outpost::CommandResult::TopLevel);
    Assert::AreEqual(before, OreOf(arena, BLUE), L"a refusal costs nothing");

    const std::int32_t cost = arena.StructureData(Outpost::StructureKind::Shipyard).levels[0].cost;
    Assert::IsTrue(result(Upgrade(BLUE, yard)) == Outpost::CommandResult::Applied);
    Assert::AreEqual(before - cost, OreOf(arena, BLUE), L"paid when ordered");
    Assert::IsTrue(arena.Get(yard).IsUpgrading() && arena.Get(yard).level == 1);
    Assert::IsTrue(result(Upgrade(BLUE, yard)) == Outpost::CommandResult::AlreadyUpgrading);
    Assert::AreEqual(before - cost, OreOf(arena, BLUE));

    // Ore the player does not have: a player with 350 Ore pays for the station's 300 and not then for a Shipyard's 150.
    constexpr Outpost::PlayerId POOR{3};
    arena.World().AddPlayer(POOR, 350);
    const Outpost::EntityId station = arena.Structure(POOR, Outpost::StructureKind::CommandStation, {.xMeters = 900.0f, .zMeters = 900.0f});
    const Outpost::EntityId poorYard = arena.Structure(POOR, Outpost::StructureKind::Shipyard, {.xMeters = -900.0f, .zMeters = 900.0f});
    Assert::IsTrue(arena.Tick({Upgrade(POOR, station), Upgrade(POOR, poorYard)}) ==
                   std::vector<Outpost::CommandResult>{Outpost::CommandResult::Applied, Outpost::CommandResult::NotEnoughOre});
    Assert::IsFalse(arena.Get(poorYard).IsUpgrading());
    Assert::AreEqual(50, OreOf(arena, POOR));
  }

  // One Constructor builds a level in the level's time, and each further one adds the tuning data's upgrade share of one
  // more, whose repository value of 0 has two build it no faster than one (owner, 2026-10-04). The level lands whole with
  // its 20% of the kind's base hit points, and the order ends.
  TEST_METHOD(ConstructorsShareTheWorkAtTheUpgradeShare)
  {
    const MatchArena reference;
    const Outpost::StructureTuning& tuning = reference.StructureData(Outpost::StructureKind::Shipyard);
    const std::uint32_t levelTicks = LevelTicks(reference, Outpost::StructureKind::Shipyard, 1);
    const double upgradeShare = reference.TuningData().constructor.extraConstructorUpgradeShare;
    Assert::AreEqual(0.0, upgradeShare, L"the repository's crew builds a level no faster than one Constructor");
    const auto radius = static_cast<float>(tuning.footprintRadiusMeters);
    for (const std::uint32_t crew : {1u, 2u})
    {
      MatchArena arena;
      const Outpost::EntityId yard = arena.Structure(BLUE, Outpost::StructureKind::Shipyard, YARD);
      std::vector<Outpost::EntityId> constructors;
      constructors.reserve(crew);
      for (std::uint32_t i = 0; i < crew; ++i)
        constructors.push_back(ConstructorBeside(arena, BLUE, YARD, radius, (static_cast<float>(i) * 30.0f) - 15.0f));
      Assert::IsTrue(arena.Tick({Upgrade(BLUE, yard, constructors)})[0] == Outpost::CommandResult::Applied);
      const std::int32_t full = arena.Get(yard).maxHitPointsHundredths;

      // The order's tick already did a tick's work.
      const std::uint32_t ticks = 1 + TicksToLevel(arena, yard, 2, levelTicks * 2);
      const double crewFactor = 1.0 + (upgradeShare * static_cast<double>(crew - 1));
      const auto expected = static_cast<std::uint32_t>(std::ceil(static_cast<double>(levelTicks) / crewFactor));
      Assert::IsTrue(ticks >= expected && ticks <= expected + 1, std::to_wstring(ticks).c_str());
      const Outpost::Entity& upgraded = arena.Get(yard);
      Assert::AreEqual(2, upgraded.level);
      Assert::IsFalse(upgraded.IsUpgrading());
      Assert::AreEqual(full + (tuning.hitPoints * Outpost::HUNDREDTHS * arena.TuningData().rules.levelHitPointsPercent / 100),
                       upgraded.maxHitPointsHundredths);
      Assert::AreEqual(upgraded.maxHitPointsHundredths, upgraded.hitPointsHundredths, L"an undamaged structure gains the whole level");
      for (const Outpost::EntityId constructor : constructors)
        Assert::IsTrue(arena.Get(constructor).order == Outpost::ShipOrder::None);
    }
  }

  // A paid upgrade waits for Constructors, which join it as they join a site; the structure goes up one level at a time to
  // its kind's top, and no further.
  TEST_METHOD(ClimbsToTheTopLevelOneAtATime)
  {
    MatchArena arena;
    const Outpost::EntityId yard = arena.Structure(BLUE, Outpost::StructureKind::Shipyard, YARD);
    const auto radius = static_cast<float>(arena.StructureData(Outpost::StructureKind::Shipyard).footprintRadiusMeters);
    const Outpost::EntityId constructor = ConstructorBeside(arena, BLUE, YARD, radius);
    Assert::IsTrue(arena.Tick({Upgrade(BLUE, yard)})[0] == Outpost::CommandResult::Applied);
    arena.Run(100);
    Assert::AreEqual(0, arena.Get(yard).upgradeWorkDone, L"no Constructor, no work");

    Assert::IsTrue(arena.Tick({Order(BLUE, Outpost::RepairCommand{.constructors = {constructor}, .target = yard})})[0] ==
                   Outpost::CommandResult::Applied);
    Assert::IsTrue(TicksToLevel(arena, yard, 2, 2 * LevelTicks(arena, Outpost::StructureKind::Shipyard, 1)) > 0);
    Assert::AreEqual(2, arena.Get(yard).level);

    const std::int32_t top = arena.StructureData(Outpost::StructureKind::Shipyard).TopLevel();
    Assert::AreEqual(3, top);
    Assert::IsTrue(arena.Tick({Upgrade(BLUE, yard, {constructor})})[0] == Outpost::CommandResult::Applied);
    Assert::IsTrue(TicksToLevel(arena, yard, 3, 2 * LevelTicks(arena, Outpost::StructureKind::Shipyard, 2)) > 0);
    Assert::AreEqual(top, arena.Get(yard).level);
    Assert::IsTrue(arena.Tick({Upgrade(BLUE, yard, {constructor})})[0] == Outpost::CommandResult::TopLevel);
  }

  // Phase 3 design §3: a Shipyard keeps building while its next level is built.
  TEST_METHOD(KeepsWorkingWhileUpgraded)
  {
    MatchArena arena;
    const Outpost::EntityId yard = arena.Structure(BLUE, Outpost::StructureKind::Shipyard, YARD);
    const auto radius = static_cast<float>(arena.StructureData(Outpost::StructureKind::Shipyard).footprintRadiusMeters);
    const Outpost::EntityId constructor = ConstructorBeside(arena, BLUE, YARD, radius);
    const std::vector<Outpost::CommandResult> results =
      arena.Tick({Upgrade(BLUE, yard, {constructor}),
                  Order(BLUE, Outpost::QueueShipCommand{.producer = yard, .design = arena.Design(BLUE, SMALL, MASS_DRIVER)})});
    Assert::IsTrue(results[1] == Outpost::CommandResult::Applied);
    const size_t before = arena.Owned(BLUE, Outpost::EntityKind::Ship).size();
    // A Small ship takes well under the level's time.
    arena.Run(LevelTicks(arena, Outpost::StructureKind::Shipyard, 1) / 2);
    Assert::IsTrue(arena.Get(yard).IsUpgrading());
    Assert::AreEqual(before + 1, arena.Owned(BLUE, Outpost::EntityKind::Ship).size(), L"the ship came out during the upgrade");
  }

  // A damaged structure keeps its share of its hit points when the level lands, and its Constructors repair it after
  // building the level, not before (owner, 2026-10-04).
  TEST_METHOD(BuildsTheLevelBeforeItRepairs)
  {
    MatchArena arena;
    const Outpost::StructureTuning& tuning = arena.StructureData(Outpost::StructureKind::Shipyard);
    const auto radius = static_cast<float>(tuning.footprintRadiusMeters);
    const Outpost::EntityId yard = arena.Structure(BLUE, Outpost::StructureKind::Shipyard, YARD);
    // Lances wear it down until it is about half, and then go.
    std::vector<Outpost::EntityId> lances;
    lances.reserve(4);
    for (int i = 0; i < 4; ++i)
      lances.push_back(arena.Ship(RED, SMALL, LANCE, {.xMeters = 150.0f, .zMeters = -30.0f + (20.0f * static_cast<float>(i))}));
    (void)arena.Tick({Order(RED, Outpost::AttackCommand{.ships = lances, .target = yard})});
    while (arena.Get(yard).hitPointsHundredths > arena.Get(yard).maxHitPointsHundredths / 2)
      arena.Run(1);
    (void)arena.Tick({Order(RED, Outpost::MoveCommand{.ships = lances, .destination = {.xMeters = 1800.0f, .zMeters = -1800.0f}})});
    arena.Run(30 * MatchArena::TICKS_PER_SECOND);
    const Outpost::Entity& damaged = arena.Get(yard);
    const double share = static_cast<double>(damaged.hitPointsHundredths) / damaged.maxHitPointsHundredths;
    const std::int32_t damagedHitPoints = damaged.hitPointsHundredths;

    const Outpost::EntityId constructor = ConstructorBeside(arena, BLUE, YARD, radius);
    Assert::IsTrue(arena.Tick({Upgrade(BLUE, yard, {constructor})})[0] == Outpost::CommandResult::Applied);
    arena.Run(LevelTicks(arena, Outpost::StructureKind::Shipyard, 1) - 2);
    Assert::AreEqual(damagedHitPoints, arena.Get(yard).hitPointsHundredths, L"no repair while the level is built");
    Assert::IsTrue(TicksToLevel(arena, yard, 2, 10) <= 10);
    const Outpost::Entity& upgraded = arena.Get(yard);
    Assert::AreEqual(2, upgraded.level);
    Assert::IsTrue(std::abs((static_cast<double>(upgraded.hitPointsHundredths) / upgraded.maxHitPointsHundredths) - share) < 0.001,
                   L"the same share of more hit points");
    Assert::IsTrue(arena.Get(constructor).order == Outpost::ShipOrder::Work, L"it stays to repair");
    for (int tick = 0; tick < 120 * 20 && arena.Get(constructor).order == Outpost::ShipOrder::Work; ++tick)
      arena.Run(1);
    Assert::AreEqual(arena.Get(yard).maxHitPointsHundredths, arena.Get(yard).hitPointsHundredths);
  }

  // A level's percent adds to research's (owner, 2026-10-04): with Reinforced Structures' 25%, a level 2 structure has
  // 1.45 times its base hit points, and one at level 3 would have 1.65.
  TEST_METHOD(ALevelAddsToResearch)
  {
    MatchArena arena;
    const Outpost::StructureTuning& yardTuning = arena.StructureData(Outpost::StructureKind::Shipyard);
    const Outpost::StructureTuning& labTuning = arena.StructureData(Outpost::StructureKind::ResearchLab);
    Assert::AreEqual(labTuning.hitPoints * Outpost::HUNDREDTHS * 140 / 100, arena.World().StructureHitPoints(BLUE, labTuning, 3));

    // A rig pays for the research.
    (void)arena.Structure(BLUE, Outpost::StructureKind::MiningRig, MatchArena::HOME_ASTEROID);
    const Outpost::EntityId yard = arena.Structure(BLUE, Outpost::StructureKind::Shipyard, YARD);
    const Outpost::EntityId constructor = ConstructorBeside(arena, BLUE, YARD, static_cast<float>(yardTuning.footprintRadiusMeters));
    Assert::IsTrue(arena.Tick({Upgrade(BLUE, yard, {constructor})})[0] == Outpost::CommandResult::Applied);
    Assert::IsTrue(TicksToLevel(arena, yard, 2, 2 * LevelTicks(arena, Outpost::StructureKind::Shipyard, 1)) > 0);

    // A Lab at level 2, which has opened Reinforced Structures' tier (Phase 3 design §6).
    const Outpost::EntityId lab = arena.Structure(BLUE, Outpost::StructureKind::ResearchLab, LAB, 2);
    for (const Outpost::ResearchTopicId topic : {IMPROVED_EXTRACTION, HULL_PLATING, REINFORCED_STRUCTURES})
    {
      Assert::IsTrue(arena.Tick({Order(BLUE, Outpost::StartResearchCommand{.lab = lab, .topic = topic})})[0] ==
                     Outpost::CommandResult::Applied);
      for (int tick = 0;
           tick < 300 * 20 && std::ranges::find(arena.World().Researched(BLUE), topic) == arena.World().Researched(BLUE).end(); ++tick)
        arena.Run(1);
    }
    Assert::AreEqual(yardTuning.hitPoints * Outpost::HUNDREDTHS * 145 / 100, arena.Get(yard).maxHitPointsHundredths);
    Assert::AreEqual(arena.Get(yard).maxHitPointsHundredths, arena.Get(yard).hitPointsHundredths);
    Assert::AreEqual(labTuning.hitPoints * Outpost::HUNDREDTHS * 165 / 100, arena.World().StructureHitPoints(BLUE, labTuning, 3));
  }

  // A structure destroyed during its upgrade loses the upgrade's Ore with it, and its Constructors stop; one built where
  // it stood starts at level 1.
  TEST_METHOD(ADestroyedStructureLosesItsLevels)
  {
    MatchArena arena;
    const Outpost::StructureTuning& tuning = arena.StructureData(Outpost::StructureKind::Shipyard);
    const auto radius = static_cast<float>(tuning.footprintRadiusMeters);
    const Outpost::EntityId yard = arena.Structure(BLUE, Outpost::StructureKind::Shipyard, YARD);
    const Outpost::EntityId constructor = ConstructorBeside(arena, BLUE, YARD, radius);
    Assert::IsTrue(arena.Tick({Upgrade(BLUE, yard, {constructor})})[0] == Outpost::CommandResult::Applied);
    Assert::IsTrue(TicksToLevel(arena, yard, 2, 2 * LevelTicks(arena, Outpost::StructureKind::Shipyard, 1)) > 0);
    Assert::IsTrue(arena.Tick({Upgrade(BLUE, yard, {constructor})})[0] == Outpost::CommandResult::Applied);
    const std::int32_t ore = OreOf(arena, BLUE);

    std::vector<Outpost::EntityId> lances;
    lances.reserve(10);
    for (int i = 0; i < 10; ++i)
      lances.push_back(arena.Ship(RED, SMALL, LANCE, {.xMeters = 150.0f, .zMeters = -90.0f + (20.0f * static_cast<float>(i))}));
    (void)arena.Tick({Order(RED, Outpost::AttackCommand{.ships = lances, .target = yard})});
    for (int tick = 0; tick < 120 * 20 && arena.World().FindEntity(yard) != nullptr; ++tick)
      arena.Run(1);
    Assert::IsNull(arena.World().FindEntity(yard), L"the lances destroyed it");
    Assert::AreEqual(ore, OreOf(arena, BLUE), L"nothing is refunded");
    if (const Outpost::Entity* survivor = arena.World().FindEntity(constructor))
      Assert::IsTrue(survivor->order == Outpost::ShipOrder::None);

    (void)arena.Tick({Order(RED, Outpost::MoveCommand{.ships = lances, .destination = {.xMeters = 1800.0f, .zMeters = -1800.0f}})});
    const Outpost::EntityId rebuilt = arena.Structure(BLUE, Outpost::StructureKind::Shipyard, YARD);
    Assert::AreEqual(1, arena.Get(rebuilt).level);
    Assert::AreEqual(tuning.hitPoints * Outpost::HUNDREDTHS, arena.Get(rebuilt).maxHitPointsHundredths);
  }

  // The snapshot carries each structure's level and the upgrade under way, and each kind's levels with their cost, time
  // and hit points (ADR-064).
  TEST_METHOD(TheSnapshotShowsLevels)
  {
    MatchArena arena;
    const Outpost::EntityId yard = arena.Structure(BLUE, Outpost::StructureKind::Shipyard, YARD);
    const auto radius = static_cast<float>(arena.StructureData(Outpost::StructureKind::Shipyard).footprintRadiusMeters);
    const Outpost::EntityId constructor = ConstructorBeside(arena, BLUE, YARD, radius);
    const Outpost::Snapshot before = arena.World().BuildSnapshot(BLUE);
    Assert::AreEqual(1, FindView(before, yard)->level);
    Assert::IsFalse(FindView(before, yard)->upgradePermille.has_value());
    Assert::AreEqual(1, FindView(before, constructor)->level, L"anything else is level 1");

    const auto type = std::ranges::find(before.structureTypes, Outpost::StructureKind::Shipyard, &Outpost::StructureTypeView::structure);
    const Outpost::StructureTuning& tuning = arena.StructureData(Outpost::StructureKind::Shipyard);
    Assert::AreEqual(tuning.levels.size(), type->levels.size());
    for (size_t i = 0; i < tuning.levels.size(); ++i)
    {
      Assert::AreEqual(tuning.levels[i].cost, type->levels[i].cost);
      Assert::AreEqual(tuning.levels[i].buildConstructorSeconds, type->levels[i].buildSeconds);
      Assert::AreEqual(arena.World().StructureHitPoints(BLUE, tuning, static_cast<std::int32_t>(i) + 2),
                       type->levels[i].maxHitPointsHundredths);
    }
    const auto relay = std::ranges::find(before.structureTypes, Outpost::StructureKind::Relay, &Outpost::StructureTypeView::structure);
    Assert::IsTrue(relay->levels.empty());

    (void)arena.Tick({Upgrade(BLUE, yard, {constructor})});
    arena.Run((LevelTicks(arena, Outpost::StructureKind::Shipyard, 1) / 2) - 1);
    // The snapshot is kept, since the view points into it.
    const Outpost::Snapshot halfway = arena.World().BuildSnapshot(BLUE);
    const Outpost::EntityView* half = FindView(halfway, yard);
    Assert::IsTrue(half->upgradePermille.has_value() && *half->upgradePermille == 500,
                   std::to_wstring(half->upgradePermille.value_or(-1)).c_str());
    Assert::AreEqual(1, half->level);
  }

  // Under fog of war a player remembers an enemy structure at the level it last saw (owner, 2026-10-04): an upgrade made
  // out of its sight shows only once it sees the structure again.
  TEST_METHOD(RemembersTheLevelLastSeen)
  {
    MatchArena arena;
    const Outpost::PlanePosition at{.xMeters = -1500.0f, .zMeters = 1500.0f};
    const Outpost::EntityId yard = arena.Structure(RED, Outpost::StructureKind::Shipyard, at);
    const auto radius = static_cast<float>(arena.StructureData(Outpost::StructureKind::Shipyard).footprintRadiusMeters);
    const Outpost::EntityId constructor = ConstructorBeside(arena, RED, at, radius);
    const Outpost::EntityId scout = arena.Ship(BLUE, SMALL, MASS_DRIVER, {.xMeters = -1500.0f, .zMeters = 1350.0f});
    arena.World().UseFog();
    (void)arena.Tick();
    Assert::AreEqual(1, FindView(arena.World().BuildSnapshot(BLUE), yard)->level);

    (void)arena.Tick({Order(BLUE, Outpost::MoveCommand{.ships = {scout}, .destination = {.xMeters = -1500.0f, .zMeters = 200.0f}}),
                      Upgrade(RED, yard, {constructor})});
    Assert::IsTrue(TicksToLevel(arena, yard, 2, 2 * LevelTicks(arena, Outpost::StructureKind::Shipyard, 1)) > 0);
    const Outpost::Snapshot memory = arena.World().BuildSnapshot(BLUE);
    const Outpost::EntityView* remembered = FindView(memory, yard);
    Assert::IsTrue(remembered != nullptr && remembered->remembered);
    Assert::AreEqual(1, remembered->level, L"the upgrade was out of sight");

    (void)arena.Tick({Order(BLUE, Outpost::MoveCommand{.ships = {scout}, .destination = {.xMeters = -1500.0f, .zMeters = 1350.0f}})});
    for (int tick = 0; tick < 60 * 20 && !arena.World().Sees(BLUE, arena.Get(yard)); ++tick)
      arena.Run(1);
    arena.Run(1);
    Assert::AreEqual(2, FindView(arena.World().BuildSnapshot(BLUE), yard)->level);
  }
};
} // namespace GameLogicTests

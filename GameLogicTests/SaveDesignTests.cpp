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
constexpr Outpost::HullId MEDIUM{2};
constexpr Outpost::DriveId ION{1};
constexpr Outpost::DriveId FUSION{2};
constexpr Outpost::WeaponId MASS_DRIVER{1};
constexpr Outpost::WeaponId LANCE{2};

Outpost::Command Save(Outpost::PlayerId _player, Outpost::HullId _hull, Outpost::DriveId _drive, Outpost::WeaponId _weapon,
                      std::string _name, Outpost::DesignId _design = {})
{
  return Order(_player, Outpost::SaveDesignCommand{
                          .design = _design, .nameUtf8 = std::move(_name), .hull = _hull, .drive = _drive, .weapon = _weapon});
}

Outpost::CommandResult Result(MatchArena& _arena, const Outpost::Command& _command)
{
  return _arena.Tick({_command})[0];
}

const Outpost::DesignView* ViewOf(const Outpost::Snapshot& _snapshot, Outpost::DesignId _design)
{
  const auto found = std::ranges::find(_snapshot.designs, _design, &Outpost::DesignView::id);
  return found != _snapshot.designs.end() ? &*found : nullptr;
}
} // namespace

TEST_CLASS(SaveDesignTests)
{
public:
  // Phase 2 design §10, ADR-058: a design may carry a module, the Sensor Array, from the start. It adds 40 Ore, slows the
  // ship by a tenth and lets it see 700 m whatever its weapon; the same components without it are another design.
  TEST_METHOD(SavesADesignWithASensorArray)
  {
    constexpr Outpost::ModuleId SENSOR_ARRAY{1};
    MatchArena arena;
    Outpost::Command save = Save(BLUE, SMALL, ION, MASS_DRIVER, "Scout");
    std::get<Outpost::SaveDesignCommand>(save.order).module = SENSOR_ARRAY;
    Assert::IsTrue(Result(arena, save) == Outpost::CommandResult::Applied);
    Assert::IsTrue(Result(arena, save) == Outpost::CommandResult::DuplicateDesign);
    const Outpost::ShipDesign* scout = arena.World().FindDesign(BLUE, {SMALL, ION, MASS_DRIVER, SENSOR_ARRAY});
    const Outpost::ShipDesign* plain = arena.World().FindDesign(BLUE, {SMALL, ION, MASS_DRIVER});
    Assert::IsNotNull(scout);
    Assert::IsNotNull(plain);
    Assert::IsTrue(scout != plain);
    Assert::AreEqual(plain->stats.cost + 40, scout->stats.cost);
    Assert::AreEqual(plain->stats.movement.speedMetersPerSecond * 0.9f, scout->stats.movement.speedMetersPerSecond, 1e-4f);
    Assert::AreEqual(700.0f, scout->stats.moduleSightMeters);

    const Outpost::Snapshot snapshot = arena.World().BuildSnapshot(BLUE);
    const Outpost::DesignView* view = ViewOf(snapshot, scout->id);
    Assert::IsNotNull(view);
    Assert::IsTrue(view->module == SENSOR_ARRAY);
    Assert::AreEqual(scout->stats.cost, view->cost);
    Assert::AreEqual(size_t{1}, snapshot.modules.size());
    Assert::IsTrue(snapshot.modules[0].available && snapshot.modules[0].nameUtf8 == "Sensor Array");

    // A ship of it sees 700 m, and the snapshot names its module.
    arena.World().UseFog();
    const Outpost::EntityId ship = arena.World().SpawnShip(BLUE, scout->id, {0.0f, -600.0f});
    Assert::AreEqual(700.0f, arena.World().SightMetersOf(*arena.World().FindEntity(ship)));
    const Outpost::Snapshot seen = arena.World().BuildSnapshot(BLUE);
    const auto entity = std::ranges::find(seen.entities, ship, &Outpost::EntityView::id);
    Assert::IsTrue(entity != seen.entities.end() && entity->module == SENSOR_ARRAY && entity->sightMeters == 700.0f);

    std::get<Outpost::SaveDesignCommand>(save.order).module = Outpost::ModuleId{9};
    Assert::IsTrue(Result(arena, save) == Outpost::CommandResult::UnknownComponent);
  }

  // Design §7, §9: the designer saves a design of components the player has, under a name the HUD can show, and the
  // server numbers it; a saved design is renamed under its identifier.
  TEST_METHOD(SavesAndRenamesADesign)
  {
    MatchArena arena;
    // Every design of the starting components is saved at the start, so a new one needs an unlock: the Fusion drive.
    Assert::IsTrue(Result(arena, Save(BLUE, MEDIUM, ION, LANCE, "Line")) == Outpost::CommandResult::DuplicateDesign);
    const Outpost::EntityId lab = arena.Structure(BLUE, Outpost::StructureKind::ResearchLab, {-600.0f, -600.0f});
    for (const std::uint32_t topic : {2u, 5u})
      (void)arena.Tick({Order(BLUE, Outpost::StartResearchCommand{.lab = lab, .topic = Outpost::ResearchTopicId{topic}})});
    arena.Run(150 * MatchArena::TICKS_PER_SECOND);
    Assert::AreEqual(size_t{2}, arena.World().Researched(BLUE).size());

    Assert::IsTrue(Result(arena, Save(BLUE, SMALL, FUSION, LANCE, "Sentry")) == Outpost::CommandResult::Applied);
    const Outpost::ShipDesign* sentry = arena.World().FindDesign(BLUE, {SMALL, FUSION, LANCE});
    Assert::IsNotNull(sentry);
    const Outpost::DesignId id = sentry->id;
    Assert::AreEqual(197, sentry->stats.cost);
    Assert::IsNotNull(ViewOf(arena.World().BuildSnapshot(BLUE), id));
    Assert::IsNull(ViewOf(arena.World().BuildSnapshot(RED), id), L"a design is its player's alone");

    Assert::IsTrue(Result(arena, Save(BLUE, SMALL, FUSION, LANCE, "Watchman", id)) == Outpost::CommandResult::Applied);
    Assert::AreEqual(std::string("Watchman"), ViewOf(arena.World().BuildSnapshot(BLUE), id)->nameUtf8);
    Assert::AreEqual(size_t{5}, arena.World().BuildSnapshot(BLUE).designs.size());
  }

  // ADR-017: a saved design's components never change, so the ships built of it stay what they are; another player's
  // design is not the player's to rename.
  TEST_METHOD(RefusesWhatItCannotSave)
  {
    MatchArena arena;
    const Outpost::DesignId swarm = arena.Design(BLUE, SMALL, MASS_DRIVER);
    Assert::IsTrue(Result(arena, Save(BLUE, SMALL, ION, LANCE, "Picket", swarm)) == Outpost::CommandResult::ComponentsFixed);
    Assert::IsTrue(Result(arena, Save(RED, SMALL, ION, MASS_DRIVER, "Mine", swarm)) == Outpost::CommandResult::UnknownDesign);
    Assert::IsTrue(Result(arena, Save(BLUE, SMALL, ION, MASS_DRIVER, "Gone", Outpost::DesignId{99})) ==
                   Outpost::CommandResult::UnknownDesign);
    Assert::IsTrue(Result(arena, Save(BLUE, Outpost::HullId{9}, ION, MASS_DRIVER, "Nothing")) == Outpost::CommandResult::UnknownComponent);
    Assert::IsTrue(Result(arena, Save(BLUE, SMALL, FUSION, MASS_DRIVER, "Slow")) == Outpost::CommandResult::ComponentLocked);

    for (const std::string& name :
         {std::string(), std::string("   "), std::string(33, 'x'), std::string("Tab\there"), std::string("Caf\xc3\xa9")})
      Assert::IsTrue(Result(arena, Save(BLUE, SMALL, ION, MASS_DRIVER, name, swarm)) == Outpost::CommandResult::InvalidName);
    Assert::IsTrue(Result(arena, Save(BLUE, SMALL, ION, MASS_DRIVER, std::string(32, 'x'), swarm)) == Outpost::CommandResult::Applied);
    Assert::AreEqual(size_t{4}, arena.World().BuildSnapshot(BLUE).designs.size(), L"nothing was added");
  }

  // ADR-017: the designer derives a design's stats from the snapshot's components with the function the server uses, so
  // what it shows is what the ship fights with, research included.
  TEST_METHOD(TheSnapshotsComponentsGiveTheServersStats)
  {
    MatchArena arena;
    const auto check = [&arena](const std::wstring& _when)
    {
      const Outpost::Snapshot snapshot = arena.World().BuildSnapshot(BLUE);
      Assert::AreEqual(size_t{3}, snapshot.hulls.size());
      for (const Outpost::HullView& hull : snapshot.hulls)
      {
        for (const Outpost::DriveView& drive : snapshot.drives)
        {
          for (const Outpost::WeaponView& weapon : snapshot.weapons)
          {
            const Outpost::DesignStats shown = Outpost::DesignStatsOf(hull, drive, weapon);
            const Outpost::DesignStats fought =
              Outpost::DesignStatsFor(arena.TuningData(), hull.id, drive.id, weapon.id, arena.World().UpgradesOf(BLUE));
            Assert::IsTrue(shown == fought, std::format(L"{}: {}+{}+{}", _when, hull.id.value, drive.id.value, weapon.id.value).c_str());
            if (const Outpost::ShipDesign* saved = arena.World().FindDesign(BLUE, {hull.id, drive.id, weapon.id}))
              Assert::IsTrue(shown == saved->stats, _when.c_str());
          }
        }
      }
    };
    check(L"at the start");

    const Outpost::EntityId lab = arena.Structure(BLUE, Outpost::StructureKind::ResearchLab, {-600.0f, -600.0f});
    for (const std::uint32_t topic : {2u, 3u, 4u})
      (void)arena.Tick({Order(BLUE, Outpost::StartResearchCommand{.lab = lab, .topic = Outpost::ResearchTopicId{topic}})});
    arena.Run(220 * MatchArena::TICKS_PER_SECOND);
    Assert::AreEqual(size_t{3}, arena.World().Researched(BLUE).size());
    check(L"after Hull Plating and both fire-rate topics");
  }
};
} // namespace GameLogicTests

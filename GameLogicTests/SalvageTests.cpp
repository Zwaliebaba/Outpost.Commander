#include "pch.h"
#include "MatchArena.h"
#include "TerritoryMatch.h"

#include <algorithm>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameLogicTests
{
namespace
{
constexpr Outpost::PlayerId BLUE = MatchArena::BLUE;
constexpr Outpost::PlayerId RED = MatchArena::RED;
constexpr std::uint32_t TICKS_PER_SECOND = MatchArena::TICKS_PER_SECOND;
// Tuning.json's 30 s of one Constructor's work (Phase 4 design §9).
constexpr std::uint32_t SALVAGE_TICKS = 30 * TICKS_PER_SECOND;
constexpr Outpost::PlanePosition WRECK{.xMeters = -900.0f, .zMeters = 300.0f};
// A Constructor this close to the wreck is in reach of it at once.
constexpr Outpost::PlanePosition BESIDE{.xMeters = -900.0f, .zMeters = 340.0f};
constexpr Outpost::ResearchTopicId IMPROVED_EXTRACTION{1};
constexpr Outpost::ResearchTopicId HULL_PLATING{2};

// A derelict of a Medium hull at WRECK paying 450 Ore, naming _topic if any.
Outpost::EntityId Wreck(MatchArena& _arena, Outpost::ResearchTopicId _topic = {})
{
  Outpost::Map map;
  map.derelicts.push_back({.position = WRECK, .ore = 450, .topic = _topic, .hull = Outpost::HullId{2}, .radiusMeters = 25.0f});
  _arena.World().PlaceDerelicts(map);
  return _arena.World().Entities().back().id;
}

Outpost::Command Salvage(std::vector<Outpost::EntityId> _constructors, Outpost::EntityId _derelict)
{
  return Order(BLUE, Outpost::SalvageCommand{.constructors = std::move(_constructors), .derelict = _derelict});
}

bool HasResearched(MatchArena& _arena, Outpost::ResearchTopicId _topic)
{
  const std::span<const Outpost::ResearchTopicId> researched = _arena.World().Researched(BLUE);
  return std::ranges::find(researched, _topic) != researched.end();
}
} // namespace

// Phase 4 design §9, ADR-074: a Constructor salvages a derelict as it builds a site.
TEST_CLASS(SalvageTests)
{
public:
  // 30 s of one Constructor's work pay the derelict's Ore to its player, and the derelict is gone.
  TEST_METHOD(PaysItsOreAndLeaves)
  {
    MatchArena arena;
    const Outpost::EntityId derelict = Wreck(arena);
    const Outpost::Entity& wreck = arena.Get(derelict);
    Assert::IsTrue(wreck.maxHitPointsHundredths == 0 && !wreck.owner.IsValid(), L"out of combat, and no one's");
    const Outpost::EntityId constructor = arena.World().SpawnConstructor(BLUE, BESIDE);
    const std::int64_t before = arena.World().OreHundredths(BLUE);
    Assert::IsTrue(arena.Tick({Salvage({constructor}, derelict)})[0] == Outpost::CommandResult::Applied);
    arena.Run(SALVAGE_TICKS - 2);
    Assert::IsNotNull(arena.World().FindEntity(derelict), L"not yet");
    Assert::AreEqual(before, arena.World().OreHundredths(BLUE));
    arena.Run(1);
    Assert::IsNull(arena.World().FindEntity(derelict));
    Assert::AreEqual(before + (450 * Outpost::HUNDREDTHS), arena.World().OreHundredths(BLUE));
    Assert::IsTrue(arena.Get(constructor).order == Outpost::ShipOrder::None, L"the order ends");
  }

  // A second Constructor adds the tuning data's share of one more, as on a site (ADR-016 decision 5).
  TEST_METHOD(ASecondConstructorHelps)
  {
    MatchArena arena;
    const Outpost::EntityId derelict = Wreck(arena);
    const Outpost::EntityId first = arena.World().SpawnConstructor(BLUE, BESIDE);
    const Outpost::EntityId second = arena.World().SpawnConstructor(BLUE, {.xMeters = -860.0f, .zMeters = 300.0f});
    (void)arena.Tick({Salvage({first, second}, derelict)});
    arena.Run((SALVAGE_TICKS * 2 / 3) - 1);
    Assert::IsNull(arena.World().FindEntity(derelict), L"in two thirds of the time");
  }

  TEST_METHOD(RefusesWhatCannotSalvage)
  {
    MatchArena arena;
    const Outpost::EntityId derelict = Wreck(arena);
    const Outpost::EntityId constructor = arena.World().SpawnConstructor(BLUE, BESIDE);
    const Outpost::EntityId warship = arena.Ship(BLUE, Outpost::HullId{1}, Outpost::WeaponId{1}, BESIDE);
    const Outpost::EntityId rig = arena.Structure(BLUE, Outpost::StructureKind::MiningRig, MatchArena::HOME_ASTEROID);
    const std::vector<Outpost::CommandResult> results =
      arena.Tick({Salvage({warship}, derelict), Salvage({constructor}, rig), Salvage({constructor}, Outpost::EntityId{999}),
                  Order(RED, Outpost::SalvageCommand{.constructors = {constructor}, .derelict = derelict})});
    Assert::IsTrue(results[0] == Outpost::CommandResult::NotAConstructor, L"warships cannot salvage");
    Assert::IsTrue(results[1] == Outpost::CommandResult::NotSalvageable);
    Assert::IsTrue(results[2] == Outpost::CommandResult::UnknownTarget);
    Assert::IsTrue(results[3] == Outpost::CommandResult::NotOwned);
  }

  // A topic not yet started takes half its time once it starts, whatever its tier, and its Ore cost is unchanged.
  TEST_METHOD(RecoversHalfOfATopicNotStarted)
  {
    MatchArena arena;
    const Outpost::EntityId derelict = Wreck(arena, IMPROVED_EXTRACTION);
    const Outpost::EntityId constructor = arena.World().SpawnConstructor(BLUE, BESIDE);
    (void)arena.Tick({Salvage({constructor}, derelict)});
    arena.Run(SALVAGE_TICKS);
    const auto recovered = [&arena]
    {
      const Outpost::Snapshot snapshot = arena.World().BuildSnapshot(BLUE);
      return std::ranges::find(snapshot.research, IMPROVED_EXTRACTION, &Outpost::ResearchTopicView::id)->recovered;
    };
    Assert::IsTrue(recovered(), L"the snapshot shows it");
    const Outpost::EntityId lab = arena.Structure(BLUE, Outpost::StructureKind::ResearchLab, {.xMeters = 600.0f, .zMeters = -600.0f});
    const std::int64_t before = arena.World().OreHundredths(BLUE);
    (void)arena.Tick({Order(BLUE, Outpost::StartResearchCommand{.lab = lab, .topic = IMPROVED_EXTRACTION})});
    Assert::AreEqual(before - (150 * Outpost::HUNDREDTHS), arena.World().OreHundredths(BLUE), L"the full cost");
    Assert::IsFalse(recovered(), L"taken off once it starts");
    arena.Run((30 * TICKS_PER_SECOND) - 1);
    Assert::IsTrue(HasResearched(arena, IMPROVED_EXTRACTION), L"in half its 60 s");
  }

  // Half the time comes off a topic under way at once.
  TEST_METHOD(RecoversHalfOfATopicUnderWay)
  {
    MatchArena arena;
    const Outpost::EntityId derelict = Wreck(arena, IMPROVED_EXTRACTION);
    const Outpost::EntityId constructor = arena.World().SpawnConstructor(BLUE, BESIDE);
    const Outpost::EntityId lab = arena.Structure(BLUE, Outpost::StructureKind::ResearchLab, {.xMeters = 600.0f, .zMeters = -600.0f});
    (void)arena.Tick(
      {Order(BLUE, Outpost::StartResearchCommand{.lab = lab, .topic = IMPROVED_EXTRACTION}), Salvage({constructor}, derelict)});
    arena.Run(SALVAGE_TICKS + 1);
    Assert::IsTrue(HasResearched(arena, IMPROVED_EXTRACTION), L"30 s researched and 30 s recovered");
  }

  // A topic researched already gains nothing, and the Ore is paid all the same.
  TEST_METHOD(ATopicResearchedGainsNothing)
  {
    MatchArena arena;
    const Outpost::EntityId lab = arena.Structure(BLUE, Outpost::StructureKind::ResearchLab, {.xMeters = 600.0f, .zMeters = -600.0f});
    (void)arena.Tick({Order(BLUE, Outpost::StartResearchCommand{.lab = lab, .topic = HULL_PLATING})});
    arena.Run(60 * TICKS_PER_SECOND);
    Assert::IsTrue(HasResearched(arena, HULL_PLATING));
    const Outpost::EntityId derelict = Wreck(arena, HULL_PLATING);
    const Outpost::EntityId constructor = arena.World().SpawnConstructor(BLUE, BESIDE);
    const std::int64_t before = arena.World().OreHundredths(BLUE);
    (void)arena.Tick({Salvage({constructor}, derelict)});
    arena.Run(SALVAGE_TICKS);
    Assert::AreEqual(before + (450 * Outpost::HUNDREDTHS), arena.World().OreHundredths(BLUE));
    const Outpost::Snapshot snapshot = arena.World().BuildSnapshot(BLUE);
    Assert::IsTrue(std::ranges::none_of(snapshot.research, &Outpost::ResearchTopicView::recovered));
  }

  // Under fog a derelict is seen as an enemy's structure is, with what it holds, and remembered once out of sight.
  TEST_METHOD(IsSeenAndRememberedAsAStructureIs)
  {
    MatchArena arena;
    const Outpost::EntityId derelict = Wreck(arena, IMPROVED_EXTRACTION);
    arena.World().UseFog();
    const auto view = [&arena, derelict](Outpost::PlayerId _player) -> std::optional<Outpost::EntityView>
    {
      const Outpost::Snapshot snapshot = arena.World().BuildSnapshot(_player);
      const auto found = std::ranges::find(snapshot.entities, derelict, &Outpost::EntityView::id);
      return found != snapshot.entities.end() ? std::optional(*found) : std::nullopt;
    };
    arena.Run(1);
    Assert::IsFalse(view(BLUE).has_value(), L"unseen");
    const Outpost::EntityId scout = arena.Ship(BLUE, Outpost::HullId{1}, Outpost::WeaponId{1}, BESIDE);
    arena.Run(1);
    Assert::IsTrue(view(BLUE).has_value() && !view(BLUE)->remembered);
    Assert::AreEqual(450, view(BLUE)->salvageOre);
    Assert::AreEqual(IMPROVED_EXTRACTION.value, view(BLUE)->salvageTopic.value);
    (void)arena.Tick({Order(BLUE, Outpost::MoveCommand{.ships = {scout}, .destination = {.xMeters = 900.0f, .zMeters = -900.0f}})});
    arena.Run(40 * TICKS_PER_SECOND);
    Assert::IsTrue(view(BLUE).has_value() && view(BLUE)->remembered, L"remembered");
    Assert::AreEqual(450, view(BLUE)->salvageOre, L"with what it holds");
    Assert::IsFalse(view(RED).has_value(), L"Red never saw it");
  }

  // Phase 4 design §8: a cleared stronghold leaves a derelict paying 1,200 Ore where its last platform fell, naming no
  // topic; and no structure stands on it.
  TEST_METHOD(AClearedStrongholdLeavesAWreck)
  {
    TerritoryMatch match(true);
    const auto stronghold = std::ranges::find(match.Outposts(), std::string("stronghold"), &Outpost::OutpostPlacement::outpost);
    const Outpost::SectorPlacement& sector = match.Placement(stronghold->sector);
    const auto platforms = [&]
    {
      std::vector<Outpost::PlanePosition> standing;
      for (const Outpost::Entity& entity : match.World().Entities())
      {
        if (entity.owner == Outpost::PIRATES && entity.kind == Outpost::EntityKind::Structure && sector.Contains(entity.position))
          standing.push_back(entity.position);
      }
      return standing;
    };
    const std::vector<Outpost::PlanePosition> before = platforms();
    const Outpost::DesignId lancer =
      match.World().FindDesign(TerritoryMatch::BLUE, {Outpost::HullId{2}, Outpost::DriveId{1}, Outpost::WeaponId{2}})->id;
    std::vector<Outpost::EntityId> fleet;
    fleet.reserve(16);
    for (int i = 0; i < 16; ++i)
      fleet.push_back(match.World().SpawnShip(
        TerritoryMatch::BLUE, lancer,
        sector.node + Outpost::PlaneVector{-300.0f + (40.0f * static_cast<float>(i % 4)), -300.0f - (40.0f * static_cast<float>(i / 4))}));
    for (int second = 0; second < 180 && !platforms().empty(); ++second)
    {
      std::erase_if(fleet, [&match](Outpost::EntityId _ship) { return match.World().FindEntity(_ship) == nullptr; });
      (void)match.World().Tick(
        {{.player = TerritoryMatch::BLUE, .order = Outpost::AttackMoveCommand{.ships = fleet, .destination = sector.node}}});
      match.Run(TICKS_PER_SECOND - 1);
    }
    Assert::IsTrue(platforms().empty(), L"the stronghold fell");
    std::vector<const Outpost::Entity*> wrecks;
    for (const Outpost::Entity& entity : match.World().Entities())
    {
      if (entity.kind == Outpost::EntityKind::Derelict && entity.salvageOre == 1200)
        wrecks.push_back(&entity);
    }
    Assert::AreEqual(size_t{1}, wrecks.size(), L"one wreck");
    Assert::IsFalse(wrecks.front()->salvageTopic.IsValid());
    Assert::IsTrue(
      std::ranges::any_of(before, [&](Outpost::PlanePosition _at) { return Outpost::Distance(_at, wrecks.front()->position) < 1.0f; }),
      L"where a platform stood");
  }
};
} // namespace GameLogicTests

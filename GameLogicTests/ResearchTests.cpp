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
constexpr Outpost::HullId LARGE{3};
constexpr Outpost::DriveId ION{1};
constexpr Outpost::WeaponId MASS_DRIVER{1};
constexpr Outpost::WeaponId LANCE{2};
constexpr Outpost::PlanePosition LAB{.xMeters = -600.0f, .zMeters = -600.0f};

// The repository's topics (design §8).
constexpr Outpost::ResearchTopicId IMPROVED_EXTRACTION{1};
constexpr Outpost::ResearchTopicId HULL_PLATING{2};
constexpr Outpost::ResearchTopicId MASS_DRIVER_CALIBRATION{3};
constexpr Outpost::ResearchTopicId LANCE_FOCUSING{4};
constexpr Outpost::ResearchTopicId FUSION_DRIVE{5};
constexpr Outpost::ResearchTopicId LARGE_HULL{6};
constexpr Outpost::ResearchTopicId AUTOMATED_SHIPYARDS{8};

Outpost::Command Research(Outpost::PlayerId _player, Outpost::EntityId _lab, Outpost::ResearchTopicId _topic)
{
  return Order(_player, Outpost::StartResearchCommand{.lab = _lab, .topic = _topic});
}

const Outpost::ResearchTopicTuning& Topic(const MatchArena& _arena, Outpost::ResearchTopicId _topic)
{
  return *std::ranges::find(_arena.TuningData().research, _topic, &Outpost::ResearchTopicTuning::id);
}

std::uint32_t ResearchTicks(const MatchArena& _arena, Outpost::ResearchTopicId _topic)
{
  return static_cast<std::uint32_t>(std::lround(Topic(_arena, _topic).researchSeconds * MatchArena::TICKS_PER_SECOND));
}

bool HasResearched(MatchArena& _arena, Outpost::PlayerId _player, Outpost::ResearchTopicId _topic)
{
  const std::span<const Outpost::ResearchTopicId> researched = _arena.World().Researched(_player);
  return std::ranges::find(researched, _topic) != researched.end();
}

// Researches the topics one after another at a new lab of _player's, from the order to the tick the last one is done.
Outpost::EntityId ResearchAll(MatchArena& _arena, Outpost::PlayerId _player, std::initializer_list<Outpost::ResearchTopicId> _topics)
{
  const Outpost::EntityId lab =
    _arena.Structure(_player, Outpost::StructureKind::ResearchLab, _player == BLUE ? LAB : Outpost::PlanePosition{600.0f, 600.0f});
  for (const Outpost::ResearchTopicId topic : _topics)
  {
    Assert::IsTrue(_arena.Tick({Research(_player, lab, topic)})[0] == Outpost::CommandResult::Applied);
    _arena.Run(ResearchTicks(_arena, topic) - 1);
    Assert::IsTrue(HasResearched(_arena, _player, topic));
  }
  return lab;
}

Outpost::SaveDesignCommand Save(Outpost::HullId _hull, Outpost::DriveId _drive, Outpost::WeaponId _weapon, std::string _name,
                                Outpost::DesignId _design = {})
{
  return {.design = _design, .nameUtf8 = std::move(_name), .hull = _hull, .drive = _drive, .weapon = _weapon};
}

std::uint32_t ShotsBy(const Outpost::Snapshot& _snapshot, Outpost::EntityId _shooter)
{
  return static_cast<std::uint32_t>(
    std::ranges::count_if(_snapshot.shots, [_shooter](const Outpost::ShotView& _shot) { return _shot.shooter == _shooter; }));
}
} // namespace

TEST_CLASS(ResearchTests)
{
public:
  // Design §8, owner 2026-10-01: a lab queues up to five topics and researches one at a time, each paid for when it
  // starts, and a topic may follow its prerequisite in the queue.
  TEST_METHOD(QueuesFiveTopicsAndResearchesOneAtATime)
  {
    MatchArena arena;
    const Outpost::EntityId lab = arena.Structure(BLUE, Outpost::StructureKind::ResearchLab, LAB);
    const std::int64_t start = arena.World().OreHundredths(BLUE);
    const auto results =
      arena.Tick({Research(BLUE, lab, HULL_PLATING), Research(BLUE, lab, FUSION_DRIVE), Research(BLUE, lab, IMPROVED_EXTRACTION),
                  Research(BLUE, lab, LANCE_FOCUSING), Research(BLUE, lab, LARGE_HULL), Research(BLUE, lab, MASS_DRIVER_CALIBRATION)});
    for (size_t i = 0; i < 5; ++i)
      Assert::IsTrue(results[i] == Outpost::CommandResult::Applied);
    Assert::IsTrue(results[5] == Outpost::CommandResult::QueueFull);

    const std::int64_t plating = std::int64_t{Topic(arena, HULL_PLATING).cost} * Outpost::HUNDREDTHS;
    const std::int64_t fusion = std::int64_t{Topic(arena, FUSION_DRIVE).cost} * Outpost::HUNDREDTHS;
    Assert::AreEqual(start - plating, arena.World().OreHundredths(BLUE), L"only the front topic has started");
    const Outpost::Snapshot snapshot = arena.World().BuildSnapshot(BLUE);
    const Outpost::EntityView& view = *std::ranges::find(snapshot.entities, lab, &Outpost::EntityView::id);
    Assert::AreEqual(size_t{5}, view.research.size());
    Assert::IsTrue(view.research.front() == HULL_PLATING);

    arena.Run(ResearchTicks(arena, HULL_PLATING) - 2);
    Assert::IsFalse(HasResearched(arena, BLUE, HULL_PLATING));
    arena.Run(1);
    Assert::IsTrue(HasResearched(arena, BLUE, HULL_PLATING), L"done in its research time");
    Assert::AreEqual(start - plating, arena.World().OreHundredths(BLUE));
    arena.Run(1);
    Assert::AreEqual(start - plating - fusion, arena.World().OreHundredths(BLUE), L"the next topic starts the next tick");
    Assert::IsTrue(arena.World().BuildSnapshot(BLUE).research[1].researched);
    Assert::IsFalse(arena.World().BuildSnapshot(RED).research[1].researched, L"research is the player's own");
  }

  // Design §8: a topic needs its prerequisites researched or ahead of it in the queue, and is researched once.
  TEST_METHOD(APrerequisiteComesFirst)
  {
    MatchArena arena;
    const Outpost::EntityId lab = arena.Structure(BLUE, Outpost::StructureKind::ResearchLab, LAB);
    Assert::IsTrue(arena.Tick({Research(BLUE, lab, FUSION_DRIVE)})[0] == Outpost::CommandResult::PrerequisiteMissing);
    const auto results =
      arena.Tick({Research(BLUE, lab, HULL_PLATING), Research(BLUE, lab, FUSION_DRIVE), Research(BLUE, lab, HULL_PLATING)});
    Assert::IsTrue(results[0] == Outpost::CommandResult::Applied);
    Assert::IsTrue(results[1] == Outpost::CommandResult::Applied);
    Assert::IsTrue(results[2] == Outpost::CommandResult::AlreadyResearched, L"queued already");
    arena.Run(ResearchTicks(arena, HULL_PLATING));
    Assert::IsTrue(HasResearched(arena, BLUE, HULL_PLATING));
    Assert::IsTrue(arena.Tick({Research(BLUE, lab, HULL_PLATING)})[0] == Outpost::CommandResult::AlreadyResearched);
  }

  // Design §6, §8: research runs in the player's own built Research Lab, of which it may have one.
  TEST_METHOD(OnlyThePlayersOneBuiltLabResearches)
  {
    MatchArena arena;
    const Outpost::EntityId lab = arena.Structure(BLUE, Outpost::StructureKind::ResearchLab, LAB);
    const Outpost::EntityId yard = arena.Structure(BLUE, Outpost::StructureKind::Shipyard, {600.0f, -600.0f});
    Assert::IsTrue(arena.Tick({Research(RED, lab, HULL_PLATING)})[0] == Outpost::CommandResult::NotALab);
    Assert::IsTrue(arena.Tick({Research(BLUE, yard, HULL_PLATING)})[0] == Outpost::CommandResult::NotALab);
    Assert::IsTrue(arena.Tick({Research(BLUE, Outpost::EntityId{999}, HULL_PLATING)})[0] == Outpost::CommandResult::UnknownEntity);
    Assert::IsTrue(arena.Tick({Research(BLUE, lab, Outpost::ResearchTopicId{99})})[0] == Outpost::CommandResult::UnknownTopic);

    const Outpost::EntityId constructor = arena.World().SpawnConstructor(BLUE, {-300.0f, -300.0f});
    const Outpost::BuildStructureCommand second{
      .constructors = {constructor}, .structure = Outpost::StructureKind::ResearchLab, .position = {-300.0f, 0.0f}};
    Assert::IsTrue(arena.Tick({Order(BLUE, second)})[0] == Outpost::CommandResult::LimitReached, L"one lab a player");

    // A lab under construction does not research.
    const Outpost::EntityId redConstructor = arena.World().SpawnConstructor(RED, {300.0f, 300.0f});
    const Outpost::BuildStructureCommand site{
      .constructors = {redConstructor}, .structure = Outpost::StructureKind::ResearchLab, .position = {300.0f, 0.0f}};
    Assert::IsTrue(arena.Tick({Order(RED, site)})[0] == Outpost::CommandResult::Applied);
    const Outpost::EntityId redLab = arena.Owned(RED, Outpost::EntityKind::Structure).back()->id;
    Assert::IsTrue(arena.Tick({Research(RED, redLab, HULL_PLATING)})[0] == Outpost::CommandResult::NotALab);
  }

  // Design §5: a topic that cannot be paid for waits at the front of the queue until it can.
  TEST_METHOD(ATopicWaitsForTheOre)
  {
    MatchArena arena;
    const Outpost::EntityId constructor = arena.World().SpawnConstructor(BLUE, {0.0f, 0.0f});
    // Three Shipyards leave 100 Ore, short of a topic's 150.
    for (int i = 0; i < 3; ++i)
    {
      const Outpost::BuildStructureCommand yard{.constructors = {constructor},
                                                .structure = Outpost::StructureKind::Shipyard,
                                                .position = {-600.0f + (static_cast<float>(i) * 300.0f), -400.0f}};
      Assert::IsTrue(arena.Tick({Order(BLUE, yard)})[0] == Outpost::CommandResult::Applied);
    }
    const Outpost::EntityId lab = arena.Structure(BLUE, Outpost::StructureKind::ResearchLab, {600.0f, 400.0f});
    Assert::IsTrue(arena.Tick({Research(BLUE, lab, HULL_PLATING)})[0] == Outpost::CommandResult::Applied);
    arena.Run(20);
    Assert::AreEqual(std::int64_t{100} * Outpost::HUNDREDTHS, arena.World().OreHundredths(BLUE));
    const Outpost::Snapshot waiting = arena.World().BuildSnapshot(BLUE);
    Assert::AreEqual(0, std::ranges::find(waiting.entities, lab, &Outpost::EntityView::id)->jobPermille);

    // A home rig earns 5 Ore a second: the topic starts, and is paid for, on the tick after the tenth second's income.
    (void)arena.Structure(BLUE, Outpost::StructureKind::MiningRig, MatchArena::HOME_ASTEROID);
    arena.Run(10 * MatchArena::TICKS_PER_SECOND);
    Assert::AreEqual(std::int64_t{150} * Outpost::HUNDREDTHS, arena.World().OreHundredths(BLUE));
    arena.Run(2);
    Assert::IsTrue(arena.World().OreHundredths(BLUE) < std::int64_t{100} * Outpost::HUNDREDTHS);
    const Outpost::Snapshot started = arena.World().BuildSnapshot(BLUE);
    Assert::IsTrue(std::ranges::find(started.entities, lab, &Outpost::EntityView::id)->jobPermille > 0);
  }

  // Design §8, owner 2026-10-01: Hull Plating raises the hit points of every ship the player has, and each keeps the share
  // of its hit points it had; a new ship has the raised number, and the other player's ships do not change.
  TEST_METHOD(HullPlatingRaisesExistingShipsKeepingTheirShare)
  {
    MatchArena arena;
    // A raider damages one ship before the others destroy it.
    const Outpost::EntityId damaged = arena.Ship(BLUE, SMALL, MASS_DRIVER, {50.0f, 0.0f});
    const Outpost::EntityId whole = arena.Ship(BLUE, SMALL, LANCE, {-400.0f, 400.0f});
    for (int i = 0; i < 5; ++i)
      (void)arena.Ship(BLUE, SMALL, MASS_DRIVER, {-110.0f, -40.0f + (static_cast<float>(i) * 20.0f)});
    const Outpost::EntityId raider = arena.Ship(RED, SMALL, MASS_DRIVER, {0.0f, 0.0f});
    const Outpost::EntityId bystander = arena.Ship(RED, SMALL, MASS_DRIVER, {800.0f, 800.0f});
    for (int tick = 0; tick < 200 && arena.World().FindEntity(raider) != nullptr; ++tick)
      arena.Run(1);
    Assert::IsNull(arena.World().FindEntity(raider), L"the raider survived");
    const std::int32_t before = arena.Get(damaged).hitPointsHundredths;
    const std::int32_t smallIon = arena.Get(damaged).maxHitPointsHundredths;
    Assert::IsTrue(before < smallIon, L"the raider did no damage");

    (void)ResearchAll(arena, BLUE, {HULL_PLATING});
    // 220 × 1.15 × 0.9 = 227.7 points.
    constexpr std::int32_t PLATED = 22770;
    Assert::AreEqual(PLATED, arena.Get(damaged).maxHitPointsHundredths);
    Assert::AreEqual(static_cast<std::int32_t>(((std::int64_t{before} * PLATED) + (smallIon / 2)) / smallIon),
                     arena.Get(damaged).hitPointsHundredths);
    Assert::AreEqual(PLATED, arena.Get(whole).hitPointsHundredths, L"an undamaged ship gains it all");
    Assert::AreEqual(smallIon, arena.Get(bystander).maxHitPointsHundredths);
    const Outpost::EntityId later = arena.Ship(BLUE, SMALL, MASS_DRIVER, {400.0f, -400.0f});
    Assert::AreEqual(PLATED, arena.Get(later).hitPointsHundredths);
    const Outpost::EntityId constructor = arena.World().SpawnConstructor(BLUE, {-400.0f, -400.0f});
    Assert::AreEqual(arena.TuningData().constructor.hitPoints * Outpost::HUNDREDTHS, arena.Get(constructor).maxHitPointsHundredths,
                     L"the Constructor has no hull");
  }

  // Design §8: Mass Driver Calibration raises the fire rate of the Mass Drivers the player already has, and nothing else:
  // not the hit, not the range, not the Lance and not the other player's.
  TEST_METHOD(AFireRateUpgradeAppliesToExistingShips)
  {
    MatchArena arena;
    // Each ship shoots at an enemy Shipyard, which has no gun and hit points enough for the whole test.
    const Outpost::EntityId blue = arena.Ship(BLUE, SMALL, MASS_DRIVER, {0.0f, 1200.0f});
    const Outpost::EntityId red = arena.Ship(RED, SMALL, MASS_DRIVER, {0.0f, -1200.0f});
    (void)arena.World().SpawnStructure(RED, Outpost::StructureKind::Shipyard, {100.0f, 1200.0f}, 40.0f, 10'000'000);
    (void)arena.World().SpawnStructure(BLUE, Outpost::StructureKind::Shipyard, {100.0f, -1200.0f}, 40.0f, 10'000'000);
    (void)ResearchAll(arena, BLUE, {MASS_DRIVER_CALIBRATION});

    const Outpost::DesignStats& upgraded = arena.World().FindDesign(arena.Design(BLUE, SMALL, MASS_DRIVER))->stats;
    const Outpost::DesignStats& base = arena.World().FindDesign(arena.Design(RED, SMALL, MASS_DRIVER))->stats;
    Assert::AreEqual(base.fireIntervalSeconds / 1.15, upgraded.fireIntervalSeconds, 1e-12);
    Assert::AreEqual(base.damageHundredths, upgraded.damageHundredths, L"never the size of a hit");
    Assert::AreEqual(base.rangeMeters, upgraded.rangeMeters, L"never a range");
    Assert::AreEqual(arena.World().FindDesign(arena.Design(RED, SMALL, LANCE))->stats.fireIntervalSeconds,
                     arena.World().FindDesign(arena.Design(BLUE, SMALL, LANCE))->stats.fireIntervalSeconds);

    std::uint32_t blueShots = 0;
    std::uint32_t redShots = 0;
    for (std::uint32_t tick = 0; tick < 60 * MatchArena::TICKS_PER_SECOND; ++tick)
    {
      arena.Run(1);
      blueShots += ShotsBy(arena.World().BuildSnapshot(BLUE), blue);
      redShots += ShotsBy(arena.World().BuildSnapshot(BLUE), red);
    }
    Assert::AreEqual(150u, redShots, L"one every 0.4 s");
    // 0.4 s / 1.15 is 6.957 ticks, so a minute holds 172 or 173 shots, depending on where the first one fell.
    Assert::IsTrue(blueShots == 172 || blueShots == 173, std::to_wstring(blueShots).c_str());
  }

  // Design §7, §8: the Large hull is no one's until a player researches it, and then only that player's; a design of it can
  // be saved once it is unlocked.
  TEST_METHOD(AnUnlockLetsThePlayerBuildTheComponent)
  {
    MatchArena arena;
    const Outpost::Command large = Order(BLUE, Save(LARGE, ION, LANCE, "Bulwark"));
    Assert::IsTrue(arena.Tick({large})[0] == Outpost::CommandResult::ComponentLocked);
    Assert::IsFalse(arena.World().BuildSnapshot(BLUE).hulls[2].available);
    Assert::IsTrue(arena.World().BuildSnapshot(BLUE).hulls[0].available);

    (void)ResearchAll(arena, BLUE, {HULL_PLATING, LARGE_HULL});
    Assert::IsTrue(arena.World().BuildSnapshot(BLUE).hulls[2].available);
    Assert::IsFalse(arena.World().BuildSnapshot(RED).hulls[2].available);
    Assert::IsTrue(arena.Tick({large})[0] == Outpost::CommandResult::Applied);
    Assert::IsTrue(arena.Tick({Order(RED, Save(LARGE, ION, LANCE, "Bulwark"))})[0] == Outpost::CommandResult::ComponentLocked);

    // The new design carries the player's Hull Plating: 1,200 × 1.15 × 0.9 = 1,242 points.
    const Outpost::ShipDesign* design = arena.World().FindDesign(BLUE, {LARGE, ION, LANCE});
    Assert::IsNotNull(design);
    Assert::AreEqual(std::string("Bulwark"), design->name);
    Assert::AreEqual(124200, design->stats.hitPointsHundredths);
  }

  // Design §8: Improved Extraction raises every rig's income at once, and the 6.25 Ore a second a home rig then earns is
  // paid in full though it is not a whole number of hundredths a tick.
  TEST_METHOD(ImprovedExtractionRaisesTheIncome)
  {
    MatchArena arena;
    (void)arena.Structure(BLUE, Outpost::StructureKind::MiningRig, MatchArena::HOME_ASTEROID);
    (void)ResearchAll(arena, BLUE, {IMPROVED_EXTRACTION});
    Assert::AreEqual(625, arena.World().BuildSnapshot(BLUE).oreIncomeHundredthsPerSecond);
    const std::int64_t start = arena.World().OreHundredths(BLUE);
    arena.Run(MatchArena::TICKS_PER_SECOND);
    Assert::AreEqual(start + 625, arena.World().OreHundredths(BLUE));
    arena.Run(3 * MatchArena::TICKS_PER_SECOND);
    Assert::AreEqual(start + (std::int64_t{4} * 625), arena.World().OreHundredths(BLUE));
  }

  // Design §8: Automated Shipyards build a quarter faster, and the Command Station's Constructors do not.
  TEST_METHOD(AutomatedShipyardsBuildFaster)
  {
    MatchArena arena;
    (void)ResearchAll(arena, BLUE, {IMPROVED_EXTRACTION, AUTOMATED_SHIPYARDS});
    Assert::AreEqual(1.25, arena.World().BuildSnapshot(BLUE).shipyardBuildSpeedFactor);
    const Outpost::EntityId yard = arena.Structure(BLUE, Outpost::StructureKind::Shipyard, {600.0f, -600.0f});
    const Outpost::DesignId design = arena.Design(BLUE, SMALL, MASS_DRIVER);
    (void)arena.Tick({Order(BLUE, Outpost::QueueShipCommand{.producer = yard, .design = design})});
    // A Small hull's 10 s become 8.
    const auto ships = [&arena] { return arena.Owned(BLUE, Outpost::EntityKind::Ship).size(); };
    arena.Run((8 * MatchArena::TICKS_PER_SECOND) - 2);
    Assert::AreEqual(size_t{0}, ships());
    arena.Run(1);
    Assert::AreEqual(size_t{1}, ships());

    const Outpost::EntityId station = arena.Structure(BLUE, Outpost::StructureKind::CommandStation, {-600.0f, 600.0f});
    (void)arena.Tick({Order(BLUE, Outpost::QueueShipCommand{.producer = station})});
    const auto constructorTicks = static_cast<std::uint32_t>(arena.TuningData().constructor.buildSeconds * MatchArena::TICKS_PER_SECOND);
    arena.Run(constructorTicks - 2);
    Assert::AreEqual(size_t{1}, ships());
    arena.Run(1);
    Assert::AreEqual(size_t{2}, ships());
  }

  // Owner 2026-10-01: a lab destroyed while it researches loses the topic and its Ore; a new lab researches it again, paid
  // again.
  TEST_METHOD(ALostLabLosesItsTopic)
  {
    MatchArena arena;
    const Outpost::EntityId lab = arena.Structure(BLUE, Outpost::StructureKind::ResearchLab, LAB);
    (void)arena.Tick({Research(BLUE, lab, HULL_PLATING)});
    const std::int64_t paid = arena.World().OreHundredths(BLUE);
    for (int i = 0; i < 6; ++i)
      (void)arena.Ship(RED, SMALL, MASS_DRIVER, {LAB.xMeters + 100.0f, LAB.zMeters - 50.0f + (static_cast<float>(i) * 20.0f)});
    for (int tick = 0; tick < 60 * 20 && arena.World().FindEntity(lab) != nullptr; ++tick)
      arena.Run(1);
    Assert::IsNull(arena.World().FindEntity(lab), L"the lab survived");
    arena.Run(ResearchTicks(arena, HULL_PLATING));
    Assert::IsFalse(HasResearched(arena, BLUE, HULL_PLATING));
    Assert::AreEqual(paid, arena.World().OreHundredths(BLUE), L"nothing is refunded");

    const Outpost::EntityId rebuilt = arena.Structure(BLUE, Outpost::StructureKind::ResearchLab, {600.0f, 600.0f});
    Assert::IsTrue(arena.Tick({Research(BLUE, rebuilt, HULL_PLATING)})[0] == Outpost::CommandResult::Applied);
    Assert::AreEqual(paid - (std::int64_t{Topic(arena, HULL_PLATING).cost} * Outpost::HUNDREDTHS), arena.World().OreHundredths(BLUE));
  }

  // The same seed and orders give the same research, upgrades included (ADR-009).
  TEST_METHOD(ResearchReplays)
  {
    const auto play = [](MatchArena& _arena)
    {
      (void)_arena.Ship(BLUE, SMALL, MASS_DRIVER, {0.0f, 0.0f});
      (void)_arena.Ship(RED, SMALL, MASS_DRIVER, {100.0f, 0.0f});
      (void)ResearchAll(_arena, BLUE, {HULL_PLATING, MASS_DRIVER_CALIBRATION});
    };
    MatchArena first;
    MatchArena second;
    play(first);
    play(second);
    Assert::IsTrue(first.World() == second.World());
  }
};
} // namespace GameLogicTests

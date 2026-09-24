#include "pch.h"

#include <cstdint>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameLogicTests
{

namespace
{
constexpr Outpost::PlayerId MINE = 1;
constexpr Outpost::PlayerId THEIRS = 2;

[[nodiscard]] Neuron::Vec2 Units(std::int32_t _x, std::int32_t _y) noexcept
{
  return Neuron::Vec2{.x = Neuron::FixedFromWholeUnits(_x), .y = Neuron::FixedFromWholeUnits(_y)};
}

[[nodiscard]] std::uint16_t HullOf(const Outpost::World& _world, Outpost::EntityId _id)
{
  return _world.Find(_id)->hullRemaining;
}

[[nodiscard]] std::int64_t DistanceUnits(const Neuron::Vec2& _a, const Neuron::Vec2& _b) noexcept
{
  return Neuron::Sqrt(Outpost::UniformGrid::DistanceSquared(_a, _b)) / Neuron::FIXED_ONE;
}

[[nodiscard]] Outpost::WireIdentity Wire(Outpost::EntityId _id) noexcept
{
  return Outpost::PackIdentity(_id.index, _id.generation);
}

/// The tick's movement and then its weapons, as the host runs them.
void Step(Outpost::World& _world, Outpost::WeaponSystem& _weapons, std::uint32_t _tick)
{
  Outpost::Tick(_world);
  _weapons.Advance(_world, _tick);
}
} // namespace

/// M3.2: weapons in the tick.
TEST_CLASS(TheWeapons)
{
public:
  TEST_METHOD(AnIdleFighterKillsAMinerInTheTablesTime)
  {
    // M3.1's 258 ticks, now from the weapon system: damage lands on the tick the weapon fires (ADR-004).
    Outpost::World world;
    Outpost::WeaponSystem weapons;
    static_cast<void>(world.Create(Units(0, 0), 0, Outpost::DesignId::Fighter, MINE));
    const Outpost::EntityId miner = world.Create(Units(300, 0), 0, Outpost::DesignId::Miner, THEIRS);

    for (std::uint32_t tick = 0; tick < 257; ++tick)
    {
      weapons.Advance(world, tick);
    }
    Assert::IsTrue(HullOf(world, miner) > 0, L"it died early");
    weapons.Advance(world, 257);
    Assert::AreEqual(std::uint16_t{0}, HullOf(world, miner), L"it outlived section 7's 12.9 seconds");
  }

  TEST_METHOD(AnIdleShipTurnsToBearBeforeItFires)
  {
    // Q68: a hostile behind it is out of its 45 degrees. It turns at its own rate, 1,638 a tick for a Fighter,
    // and opens fire once the target is inside the arc.
    Outpost::World world;
    Outpost::WeaponSystem weapons;
    const Outpost::EntityId fighter = world.Create(Units(0, 0), 0, Outpost::DesignId::Fighter, MINE);
    const Outpost::EntityId miner = world.Create(Units(-300, 0), 0, Outpost::DesignId::Miner, THEIRS);
    const std::uint16_t full = HullOf(world, miner);

    for (std::uint32_t tick = 0; tick < 10; ++tick)
    {
      weapons.Advance(world, tick);
    }
    Assert::AreEqual(full, HullOf(world, miner), L"it fired before it had turned to bear");
    Assert::AreNotEqual(Neuron::Angle{0}, world.Find(fighter)->heading, L"it did not turn");

    for (std::uint32_t tick = 10; tick < 30; ++tick)
    {
      weapons.Advance(world, tick);
    }
    Assert::IsTrue(HullOf(world, miner) < full, L"it never opened fire");
    Assert::AreEqual(Neuron::Angle{32768}, world.Find(fighter)->heading, L"it should end facing the target");
  }

  TEST_METHOD(AMoverKeepsItsCourseAndFiresOnlyInsideItsArc)
  {
    // Q78: under a move order, fire at will, but never turn for it.
    Outpost::World world;
    Outpost::WeaponSystem weapons;
    const Outpost::EntityId fighter = world.Create(Units(0, 0), 0, Outpost::DesignId::Fighter, MINE);
    Assert::IsTrue(world.OrderMoveTo(fighter, Units(5000, 0), Outpost::SpeedPerTick(Outpost::DesignId::Fighter),
                                     Outpost::TurnAnglePerTick(Outpost::DesignId::Fighter)));
    const Outpost::EntityId beside = world.Create(Units(0, 300), 0, Outpost::DesignId::Miner, THEIRS);

    for (std::uint32_t tick = 0; tick < 20; ++tick)
    {
      weapons.Advance(world, tick);
    }
    Assert::AreEqual(Neuron::Angle{0}, world.Find(fighter)->heading, L"a mover turned to bear");
    Assert::AreEqual(Outpost::Derive(Outpost::DesignId::Miner).hullPoints, static_cast<std::uint32_t>(HullOf(world, beside)),
                     L"a mover fired outside its arc");

    const Outpost::EntityId ahead = world.Create(Units(400, 150), 0, Outpost::DesignId::Miner, THEIRS);
    weapons.Advance(world, 20);
    weapons.Advance(world, 21);
    Assert::IsTrue(HullOf(world, ahead) < Outpost::Derive(Outpost::DesignId::Miner).hullPoints, L"a mover held fire inside its arc");
  }

  TEST_METHOD(ANewTargetStartsFromAnEmptyRemainder)
  {
    // ADR-014: a fraction accumulated on one ship never lands on another.
    Outpost::World world;
    Outpost::WeaponSystem weapons;
    const Outpost::EntityId fighter = world.Create(Units(0, 0), 0, Outpost::DesignId::Fighter, MINE);
    const Outpost::EntityId first = world.Create(Units(300, 0), 0, Outpost::DesignId::Miner, THEIRS);
    weapons.Advance(world, 0);
    Assert::IsTrue(world.FindWeapons(fighter)->target == first);
    Assert::AreEqual(8750u, world.FindWeapons(fighter)->remainders[0]);

    // The first leaves; another comes into range. One tick on it is one tick's remainder, not two.
    world.Find(first)->position = Units(5000, 0);
    const Outpost::EntityId second = world.Create(Units(250, 50), 0, Outpost::DesignId::Miner, THEIRS);
    weapons.Advance(world, 1);
    Assert::IsTrue(world.FindWeapons(fighter)->target == second);
    Assert::AreEqual(8750u, world.FindWeapons(fighter)->remainders[0]);
    Assert::AreEqual(8750u, world.FindWeapons(fighter)->remainders[1]);
  }

  TEST_METHOD(AHullStopsAtZeroAndOverkillIsAllowed)
  {
    // ADR-014: deaths wait for the deaths step (M3.4), and two shooters on a dying hull both fire.
    Outpost::World world;
    Outpost::WeaponSystem weapons;
    static_cast<void>(world.Create(Units(0, 0), 0, Outpost::DesignId::Fighter, MINE));
    static_cast<void>(world.Create(Units(0, 20), 0, Outpost::DesignId::Fighter, MINE));
    const Outpost::EntityId miner = world.Create(Units(300, 0), 0, Outpost::DesignId::Miner, THEIRS);
    world.Find(miner)->hullRemaining = 1;
    for (std::uint32_t tick = 0; tick < 5; ++tick)
    {
      weapons.Advance(world, tick);
    }
    Assert::AreEqual(std::uint16_t{0}, HullOf(world, miner));
  }

  TEST_METHOD(AShooterSendsAFireEventAtMostEveryTenTicks)
  {
    Outpost::World world;
    Outpost::WeaponSystem weapons;
    static_cast<void>(world.Create(Units(0, 0), 0, Outpost::DesignId::Fighter, MINE));
    static_cast<void>(world.Create(Units(300, 0), 0, Outpost::DesignId::Station, THEIRS));

    std::size_t events = 0;
    for (std::uint32_t tick = 0; tick < 100; ++tick)
    {
      weapons.Advance(world, tick);
      events += weapons.Fired().size();
    }
    // The fighter fires every tick and the station's point defense does too, both for a hundred ticks.
    Assert::AreEqual(std::size_t{20}, events, L"ADR-014: one event a shooter per ten ticks");
  }

  TEST_METHOD(ThePointDefenseFiresAllAroundAndNeverTurns)
  {
    // M3.5's arrangement, from M3.2's code: a station is a hull with two all-round mounts and no drive.
    Outpost::World world;
    Outpost::WeaponSystem weapons;
    const Outpost::EntityId station = world.Create(Units(0, 0), 0, Outpost::DesignId::Station, MINE);
    const Outpost::EntityId behind = world.Create(Units(-300, 0), 0, Outpost::DesignId::Fighter, THEIRS);
    const Outpost::EntityId standingOff = world.Create(Units(0, 500), 16384, Outpost::DesignId::Miner, THEIRS);
    for (std::uint32_t tick = 0; tick < 20; ++tick)
    {
      weapons.Advance(world, tick);
    }
    Assert::AreEqual(Neuron::Angle{0}, world.Find(station)->heading, L"a station turned");
    Assert::IsTrue(HullOf(world, behind) < Outpost::Derive(Outpost::DesignId::Fighter).hullPoints, L"point defense missed behind it");
    Assert::AreEqual(Outpost::Derive(Outpost::DesignId::Miner).hullPoints, static_cast<std::uint32_t>(HullOf(world, standingOff)),
                     L"point defense reached past its 480 units");
  }
};

/// M3.2 and Q67: the attack order and its standoff arc.
TEST_CLASS(TheAttackOrder)
{
public:
  TEST_METHOD(AFleetStandsOffAStationOutsideItsPointDefenseAndShellsIt)
  {
    // GameDesign.md section 5: a fighter can stand off at 500 and shell the station untouched.
    Outpost::World world;
    Outpost::WeaponSystem weapons;
    const Outpost::EntityId station = world.Create(Units(0, 0), 0, Outpost::DesignId::Station, THEIRS);
    std::vector<Outpost::EntityId> fleet;
    for (std::int32_t index = 0; index < 6; ++index)
    {
      fleet.push_back(world.Create(Units(-2500, (index - 3) * 80), 0, Outpost::DesignId::Fighter, MINE));
    }
    Assert::AreEqual(fleet.size(), Outpost::OrderAttack(world, fleet, station, world.NewOrderGroup()));

    for (std::uint32_t tick = 0; tick < 800; ++tick)
    {
      Step(world, weapons, tick);
    }
    for (const Outpost::EntityId id : fleet)
    {
      const std::int64_t distance = DistanceUnits(world.Find(id)->position, world.Find(station)->position);
      Assert::IsTrue((distance > 480) && (distance <= 590), L"a fighter is not on the 580-unit standoff (Q63)");
      Assert::AreEqual(Outpost::Derive(Outpost::DesignId::Fighter).hullPoints, static_cast<std::uint32_t>(HullOf(world, id)),
                       L"point defense reached a fighter on the standoff");
      Assert::IsTrue(world.FindAttack(id)->active);
    }
    Assert::IsTrue(HullOf(world, station) < Outpost::Derive(Outpost::DesignId::Station).hullPoints, L"nobody shelled the station");
  }

  TEST_METHOD(AgainstAnEqualRangeTheFleetStandsInsideItsOwnReach)
  {
    // Q67 as built: a Fighter's reach plus 100 is past a Fighter's own reach, so the arc sits at 550.
    Outpost::World world;
    const Outpost::EntityId enemy = world.Create(Units(0, 0), 0, Outpost::DesignId::Fighter, THEIRS);
    const Outpost::EntityId attacker = world.Create(Units(-2000, 0), 0, Outpost::DesignId::Fighter, MINE);
    const std::vector<Outpost::EntityId> fleet{attacker};
    Assert::AreEqual(std::size_t{1}, Outpost::OrderAttack(world, fleet, enemy, world.NewOrderGroup()));
    Assert::AreEqual(std::int64_t{550}, DistanceUnits(world.FindOrder(attacker)->destination, world.Find(enemy)->position));
  }

  TEST_METHOD(OnlyWhatCanFightTakesIt)
  {
    Outpost::World world;
    const Outpost::EntityId enemy = world.Create(Units(0, 0), 0, Outpost::DesignId::Station, THEIRS);
    const Outpost::EntityId fighter = world.Create(Units(-2000, 0), 0, Outpost::DesignId::Fighter, MINE);
    const Outpost::EntityId miner = world.Create(Units(-2000, 100), 0, Outpost::DesignId::Miner, MINE);
    const std::vector<Outpost::EntityId> fleet{fighter, miner};
    Assert::AreEqual(std::size_t{1}, Outpost::OrderAttack(world, fleet, enemy, world.NewOrderGroup()));
    Assert::IsFalse(world.FindAttack(miner)->active);
    Assert::IsFalse(world.FindOrder(miner)->active, L"the miner was sent somewhere");
  }

  TEST_METHOD(TheArcIsSolvedAgainWhenTheTargetMoves)
  {
    Outpost::World world;
    Outpost::WeaponSystem weapons;
    const Outpost::EntityId enemy = world.Create(Units(0, 0), 0, Outpost::DesignId::Station, THEIRS);
    const Outpost::EntityId fighter = world.Create(Units(-2000, 0), 0, Outpost::DesignId::Fighter, MINE);
    const std::vector<Outpost::EntityId> fleet{fighter};
    static_cast<void>(Outpost::OrderAttack(world, fleet, enemy, world.NewOrderGroup()));
    const Neuron::Vec2 before = world.FindOrder(fighter)->destination;

    world.Find(enemy)->position = Units(0, 1000);
    weapons.Advance(world, 0);
    Assert::IsFalse(world.FindOrder(fighter)->destination == before, L"the slot did not follow the target");
    Assert::IsTrue(world.FindAttack(fighter)->solvedAt == Units(0, 1000));
  }

  TEST_METHOD(ATargetThatIsGoneEndsTheOrder)
  {
    Outpost::World world;
    Outpost::WeaponSystem weapons;
    const Outpost::EntityId enemy = world.Create(Units(0, 0), 0, Outpost::DesignId::Fighter, THEIRS);
    const Outpost::EntityId fighter = world.Create(Units(-2000, 0), 0, Outpost::DesignId::Fighter, MINE);
    const std::vector<Outpost::EntityId> fleet{fighter};
    static_cast<void>(Outpost::OrderAttack(world, fleet, enemy, world.NewOrderGroup()));
    Assert::IsTrue(world.Destroy(enemy));
    weapons.Advance(world, 0);
    Assert::IsFalse(world.FindAttack(fighter)->active);
  }

  TEST_METHOD(TheIntakeRefusesOwnAndNothingAndAMoveEndsAnAttack)
  {
    Outpost::World world;
    Outpost::CommandIntake intake;
    Outpost::BuildSystem build;
    const Outpost::EntityId fighter = world.Create(Units(0, 0), 0, Outpost::DesignId::Fighter, MINE);
    const Outpost::EntityId friendly = world.Create(Units(100, 0), 0, Outpost::DesignId::Miner, MINE);
    const Outpost::EntityId enemy = world.Create(Units(2000, 0), 0, Outpost::DesignId::Miner, THEIRS);

    Outpost::Command attack{.sequence = 1, .type = Outpost::CommandType::Attack, .selection = {Wire(fighter)}};
    attack.AimAt(Wire(friendly));
    Assert::IsTrue(intake.Apply(world, build, MINE, attack) == Outpost::CommandRejection::NoSuchTarget, L"own ship accepted");
    Assert::AreEqual(std::uint16_t{1}, intake.LastAppliedSequence(MINE), L"a refusal is still acknowledged");

    attack.sequence = 2;
    attack.AimAt(Outpost::PackIdentity(4000, 1));
    Assert::IsTrue(intake.Apply(world, build, MINE, attack) == Outpost::CommandRejection::NoSuchTarget, L"nothing accepted");

    attack.sequence = 3;
    attack.AimAt(Wire(enemy));
    Assert::IsTrue(intake.Apply(world, build, MINE, attack) == Outpost::CommandRejection::None);
    Assert::IsTrue(world.FindAttack(fighter)->active);

    const Outpost::Command move{
      .sequence = 4, .type = Outpost::CommandType::MoveTo, .targetX = 100, .targetY = 100, .selection = {Wire(fighter)}};
    Assert::IsTrue(intake.Apply(world, build, MINE, move) == Outpost::CommandRejection::None);
    Assert::IsFalse(world.FindAttack(fighter)->active, L"a move did not end the attack");
  }
};

/// M3.5. **THE POINT DEFENSE OUTRANGES NOTHING, AND THAT IS DELIBERATE** (`GameDesign.md` section 5). It reaches
/// 480 since Q63; a mass driver reaches 600. It guards the unloading area, not the station: a Fighter standing off
/// past 480 shells the station untouched, and one that comes inside dies. **Do not "fix" either of these tests.**
TEST_CLASS(ThePointDefense)
{
public:
  /// A Fighter at 500, facing the station, fires on it and is never fired on.
  TEST_METHOD(AFighterAt500TakesNoReturnFire)
  {
    Outpost::World world;
    Outpost::WeaponSystem weapons;
    const Outpost::EntityId station = world.Create(Units(0, 0), 0, Outpost::DesignId::Station, THEIRS);
    const Outpost::EntityId fighter = world.Create(Units(500, 0), Neuron::Angle{32768}, Outpost::DesignId::Fighter, MINE);

    for (std::uint32_t tick = 0; tick < 400; ++tick)
    {
      Step(world, weapons, tick);
    }
    Assert::AreEqual(static_cast<std::uint16_t>(Outpost::Derive(Outpost::DesignId::Fighter).hullPoints), HullOf(world, fighter),
                     L"the point defense reached 500");
    Assert::IsTrue(HullOf(world, station) < Outpost::Derive(Outpost::DesignId::Station).hullPoints, L"the Fighter did not shell it");
  }

  /// **A RAIDER AT 440 DIES IN UNDER SIX SECONDS** -- 120 ticks -- which is the figure section 5 gives since Q63.
  TEST_METHOD(ARaiderAt440DiesInsideSixSeconds)
  {
    Outpost::World world;
    Outpost::WeaponSystem weapons;
    Outpost::DeathSystem deaths;
    static_cast<void>(world.Create(Units(0, 0), 0, Outpost::DesignId::Station, THEIRS));
    const Outpost::EntityId raider = world.Create(Units(440, 0), Neuron::Angle{32768}, Outpost::DesignId::Fighter, MINE);

    std::uint32_t tick = 0;
    for (; (tick < 200) && world.IsAlive(raider); ++tick)
    {
      Step(world, weapons, tick);
      deaths.Advance(world);
    }
    Logger::WriteMessage((L"POINT DEFENSE kills a Fighter at 440 in " + std::to_wstring(tick) + L" ticks\n").c_str());
    Assert::IsFalse(world.IsAlive(raider), L"the raider survived");
    Assert::IsTrue(tick < 120, L"the raider outlived section 5's six seconds");
  }

  /// **THE SAFE ZONE'S BOUNDARY IS A TEST, NOT AN OBSERVATION.** Range is center to center and inclusive: a Miner
  /// at exactly 480 is hit, and one at 481 never is.
  TEST_METHOD(TheBoundaryIsExactly480)
  {
    Outpost::World world;
    Outpost::WeaponSystem weapons;
    static_cast<void>(world.Create(Units(0, 0), 0, Outpost::DesignId::Station, THEIRS));
    const Outpost::EntityId inside = world.Create(Units(480, 0), 0, Outpost::DesignId::Miner, MINE);
    const Outpost::EntityId outside = world.Create(Units(0, 481), 0, Outpost::DesignId::Miner, MINE);

    for (std::uint32_t tick = 0; tick < 100; ++tick)
    {
      Step(world, weapons, tick);
    }
    const auto full = static_cast<std::uint16_t>(Outpost::Derive(Outpost::DesignId::Miner).hullPoints);
    Assert::IsTrue(HullOf(world, inside) < full, L"a Miner at exactly 480 was not fired on");
    Assert::AreEqual(full, HullOf(world, outside), L"a Miner at 481 was fired on");
  }
};

} // namespace GameLogicTests

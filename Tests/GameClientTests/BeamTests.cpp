#include "pch.h"

#include <array>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameClientTests
{

namespace
{
constexpr Outpost::PlayerId MINE = 1;
constexpr Outpost::PlayerId THEIRS = 2;

/// When the first copy of every event in these tests arrives.
constexpr std::uint64_t ARRIVED_MILLISECONDS = 10000;

/// The middle of a tracer's life: past the interpolation delay and a hundred milliseconds in.
constexpr std::uint64_t MID_TRACER_MILLISECONDS = ARRIVED_MILLISECONDS + Outpost::INTERPOLATION_DELAY_MILLISECONDS + 100;

[[nodiscard]] Outpost::EntityRecord Record(std::uint16_t _index, float _worldX, float _worldY, Outpost::PlayerId _owner,
                                           Outpost::DesignId _design, Outpost::Activity _activity = Outpost::Activity::None)
{
  Outpost::EntityRecord record;
  record.identity = Outpost::PackIdentity(_index, 1);
  record.positionX = Outpost::QuantizePosition(static_cast<Neuron::Fixed>(_worldX * Neuron::FIXED_ONE));
  record.positionY = Outpost::QuantizePosition(static_cast<Neuron::Fixed>(_worldY * Neuron::FIXED_ONE));
  record.designIdentity = static_cast<std::uint8_t>(_design);
  record.owner = _owner;
  record.hullPercentRemaining = 100;
  record.flags = Outpost::WithActivity(0, _activity);
  return record;
}

[[nodiscard]] Outpost::FireEvent Shot(std::uint16_t _shooter, std::uint16_t _target, Outpost::ComponentId _weapon) noexcept
{
  return Outpost::FireEvent{.shooter = Outpost::PackIdentity(_shooter, 1),
                            .target = Outpost::PackIdentity(_target, 1),
                            .weapon = static_cast<std::uint8_t>(_weapon)};
}
} // namespace

/// Q81. **One shot, one tracer**, however many updates repeat it.
TEST_CLASS(TheTracerSet)
{
public:
  /// ADR-004 repeats an event in three consecutive updates; the set keeps the first and drops the other two.
  TEST_METHOD(RepeatsOfOneShotAreOneTracer)
  {
    Outpost::TracerSet tracers;
    const std::array<Outpost::FireEvent, 1> events{Shot(1, 2, Outpost::ComponentId::MassDriver)};
    tracers.Note(events, ARRIVED_MILLISECONDS);
    tracers.Note(events, ARRIVED_MILLISECONDS + 50);
    tracers.Note(events, ARRIVED_MILLISECONDS + 100);
    Assert::AreEqual(std::size_t{1}, tracers.Tracers().size());
    Assert::AreEqual(ARRIVED_MILLISECONDS, tracers.Tracers().front().arrivedMilliseconds, L"a repeat moved the tracer's start");
  }

  /// The same shooter firing again half a second later is a second shot, and a second weapon is its own tracer.
  TEST_METHOD(ASecondShotOrWeaponIsItsOwnTracer)
  {
    Outpost::TracerSet tracers;
    const std::array<Outpost::FireEvent, 2> events{Shot(1, 2, Outpost::ComponentId::MassDriver),
                                                   Shot(1, 2, Outpost::ComponentId::PointDefense)};
    tracers.Note(events, ARRIVED_MILLISECONDS);
    Assert::AreEqual(std::size_t{2}, tracers.Tracers().size());
    tracers.Note(std::span{events}.first(1), ARRIVED_MILLISECONDS + 500);
    Assert::AreEqual(std::size_t{3}, tracers.Tracers().size());
  }

  /// A tracer lives for the interpolation delay and its lifetime, and then goes.
  TEST_METHOD(ATracerExpiresWhenItHasFinishedDrawing)
  {
    Outpost::TracerSet tracers;
    const std::array<Outpost::FireEvent, 1> events{Shot(1, 2, Outpost::ComponentId::MassDriver)};
    tracers.Note(events, ARRIVED_MILLISECONDS);
    const std::uint64_t done = ARRIVED_MILLISECONDS + Outpost::INTERPOLATION_DELAY_MILLISECONDS + Outpost::TRACER_LIFETIME_MILLISECONDS;
    tracers.Expire(done - 1);
    Assert::AreEqual(std::size_t{1}, tracers.Tracers().size());
    tracers.Expire(done);
    Assert::AreEqual(std::size_t{0}, tracers.Tracers().size());
  }

  /// Past the cap a shot is dropped rather than allocated for inside the drain.
  TEST_METHOD(TheSetNeverHoldsMoreThanItsCap)
  {
    Outpost::TracerSet tracers;
    std::vector<Outpost::FireEvent> events;
    for (std::uint16_t shooter = 0; shooter < Outpost::MAX_TRACERS + 10; ++shooter)
    {
      events.push_back(Shot(shooter, 9999, Outpost::ComponentId::MassDriver));
    }
    tracers.Note(events, ARRIVED_MILLISECONDS);
    Assert::AreEqual(Outpost::MAX_TRACERS, tracers.Tracers().size());
  }
};

/// Q81. **What a frame draws as beams.**
TEST_CLASS(TheBeams)
{
public:
  /// A shot draws from the shooter's drawn position to the target's, and not before the hulls catch up with it.
  TEST_METHOD(ATracerJoinsShooterAndTargetAfterTheDelay)
  {
    Outpost::TracerSet tracers;
    const std::array<Outpost::FireEvent, 1> events{Shot(1, 2, Outpost::ComponentId::MassDriver)};
    tracers.Note(events, ARRIVED_MILLISECONDS);
    const std::vector<Outpost::EntityRecord> drawn{Record(1, 100.0f, 0.0f, MINE, Outpost::DesignId::Fighter),
                                                   Record(2, 500.0f, 300.0f, THEIRS, Outpost::DesignId::Fighter)};
    std::vector<Neuron::BeamInstance> beams;

    Outpost::BuildBeams(tracers, drawn, {}, ARRIVED_MILLISECONDS, 1.0f, beams);
    Assert::IsTrue(beams.empty(), L"a shot drew before the hulls it joins were drawn at that time");

    Outpost::BuildBeams(tracers, drawn, {}, MID_TRACER_MILLISECONDS, 1.0f, beams);
    Assert::AreEqual(std::size_t{1}, beams.size());
    Assert::AreEqual(100.0f, beams[0].from[0]);
    Assert::AreEqual(0.0f, beams[0].from[1]);
    Assert::AreEqual(500.0f, beams[0].to[0]);
    Assert::AreEqual(300.0f, beams[0].to[1]);
  }

  /// Bright at the start, dim near the end.
  TEST_METHOD(ATracerFades)
  {
    Outpost::TracerSet tracers;
    const std::array<Outpost::FireEvent, 1> events{Shot(1, 2, Outpost::ComponentId::MassDriver)};
    tracers.Note(events, ARRIVED_MILLISECONDS);
    const std::vector<Outpost::EntityRecord> drawn{Record(1, 0.0f, 0.0f, MINE, Outpost::DesignId::Fighter),
                                                   Record(2, 500.0f, 0.0f, THEIRS, Outpost::DesignId::Fighter)};
    const std::uint64_t start = ARRIVED_MILLISECONDS + Outpost::INTERPOLATION_DELAY_MILLISECONDS;
    std::vector<Neuron::BeamInstance> early;
    std::vector<Neuron::BeamInstance> late;
    Outpost::BuildBeams(tracers, drawn, {}, start, 1.0f, early);
    Outpost::BuildBeams(tracers, drawn, {}, start + Outpost::TRACER_LIFETIME_MILLISECONDS - 10, 1.0f, late);
    Assert::AreEqual(std::size_t{1}, early.size());
    Assert::AreEqual(std::size_t{1}, late.size());
    Assert::IsTrue(early[0].color[0] > (late[0].color[0] * 5.0f), L"the tracer did not fade");
  }

  /// A shot at something not drawn -- out of the store, or already removed -- draws nothing.
  TEST_METHOD(AShotAtSomethingNotDrawnIsSkipped)
  {
    Outpost::TracerSet tracers;
    const std::array<Outpost::FireEvent, 1> events{Shot(1, 2, Outpost::ComponentId::MassDriver)};
    tracers.Note(events, ARRIVED_MILLISECONDS);
    const std::vector<Outpost::EntityRecord> drawn{Record(1, 0.0f, 0.0f, MINE, Outpost::DesignId::Fighter)};
    std::vector<Neuron::BeamInstance> beams;
    Outpost::BuildBeams(tracers, drawn, {}, MID_TRACER_MILLISECONDS, 1.0f, beams);
    Assert::IsTrue(beams.empty());
  }

  /// An extracting miner beams to the nearest rock in its reach, at the rock's lift; one out of reach draws nothing.
  TEST_METHOD(AnExtractingMinerBeamsToItsRock)
  {
    const float reach = static_cast<float>(Outpost::Derive(Outpost::DesignId::Miner).miningRangeUnits);
    const std::vector<Outpost::RockPickPoint> rocks{
      Outpost::RockPickPoint{.worldX = 1000.0f + reach - 10.0f, .worldY = 0.0f, .liftUnits = 30.0f, .rock = 0},
      Outpost::RockPickPoint{.worldX = 1000.0f - reach - 40.0f, .worldY = 0.0f, .liftUnits = -20.0f, .rock = 1}};
    const std::vector<Outpost::EntityRecord> drawn{Record(1, 1000.0f, 0.0f, MINE, Outpost::DesignId::Miner, Outpost::Activity::Extracting),
                                                   Record(2, 9000.0f, 0.0f, MINE, Outpost::DesignId::Miner, Outpost::Activity::Extracting)};
    std::vector<Neuron::BeamInstance> beams;
    Outpost::BuildBeams(Outpost::TracerSet{}, drawn, rocks, 0, 1.0f, beams);
    Assert::AreEqual(std::size_t{1}, beams.size(), L"the far-off miner found a rock out of its reach");
    Assert::AreEqual(1000.0f + reach - 10.0f, beams[0].to[0]);
    Assert::AreEqual(30.0f, beams[0].to[2]);
  }

  /// An unloading miner beams into its own nearest acceptor -- never the enemy's, even closer.
  TEST_METHOD(AnUnloadingMinerBeamsIntoItsOwnStation)
  {
    const std::vector<Outpost::EntityRecord> drawn{Record(1, 0.0f, 0.0f, MINE, Outpost::DesignId::Miner, Outpost::Activity::Unloading),
                                                   Record(2, 100.0f, 0.0f, THEIRS, Outpost::DesignId::Station),
                                                   Record(3, -160.0f, 0.0f, MINE, Outpost::DesignId::Station),
                                                   Record(4, 2000.0f, 0.0f, MINE, Outpost::DesignId::Station)};
    std::vector<Neuron::BeamInstance> beams;
    Outpost::BuildBeams(Outpost::TracerSet{}, drawn, {}, 0, 1.0f, beams);
    Assert::AreEqual(std::size_t{1}, beams.size());
    Assert::AreEqual(-160.0f, beams[0].to[0]);
  }

  /// A beam is at least its floor in world units, and wider when the camera is back far enough for pixels to win.
  TEST_METHOD(TheWidthFollowsTheCamera)
  {
    const std::vector<Outpost::EntityRecord> drawn{Record(1, 0.0f, 0.0f, MINE, Outpost::DesignId::Miner, Outpost::Activity::Unloading),
                                                   Record(2, -160.0f, 0.0f, MINE, Outpost::DesignId::Station)};
    std::vector<Neuron::BeamInstance> closeUp;
    std::vector<Neuron::BeamInstance> distant;
    Outpost::BuildBeams(Outpost::TracerSet{}, drawn, {}, 0, 0.1f, closeUp);
    Outpost::BuildBeams(Outpost::TracerSet{}, drawn, {}, 0, 20.0f, distant);
    Assert::IsTrue(distant[0].widthUnits > closeUp[0].widthUnits);
    Assert::IsTrue(closeUp[0].widthUnits > 1.0f, L"a close beam lost its floor");

    // And the scale is the frustum's: twice the distance, twice the units a pixel.
    const float nearScale = Outpost::UnitsPerPixelAtFocus(Outpost::CameraPose{.distance = 1000.0f}, 960);
    const float farScale = Outpost::UnitsPerPixelAtFocus(Outpost::CameraPose{.distance = 2000.0f}, 960);
    Assert::AreEqual(nearScale * 2.0f, farScale, 0.0001f);
  }
};

} // namespace GameClientTests

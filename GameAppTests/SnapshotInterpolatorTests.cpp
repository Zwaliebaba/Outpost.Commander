#include "pch.h"

#include <cmath>
#include <numbers>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameAppTests
{
namespace
{
constexpr std::uint32_t TICKS_PER_SECOND = 20;
constexpr float SECONDS_PER_TICK = 1.0f / TICKS_PER_SECOND;
constexpr float FRAME_SECONDS = 1.0f / 60.0f;
constexpr float TOLERANCE = 1e-4f;
constexpr Outpost::EntityId SHIP{1};
constexpr Outpost::EntityId OTHER{2};

// A snapshot of one ship at a place and heading, and optionally a second ship.
Outpost::Snapshot At(std::uint64_t _tick, float _xMeters, float _headingRadians = 0.0f, bool _withOther = false)
{
  Outpost::Snapshot snapshot{.tick = _tick, .player = Outpost::PlayerId{1}};
  snapshot.entities.push_back({.id = SHIP, .position = {.xMeters = _xMeters, .zMeters = 0.0f}, .headingRadians = _headingRadians});
  if (_withOther)
    snapshot.entities.push_back({.id = OTHER, .position = {.xMeters = 500.0f, .zMeters = 0.0f}});
  return snapshot;
}

float ShipX(const Outpost::SnapshotInterpolator& _view)
{
  return _view.Entities().front().position.xMeters;
}
} // namespace

TEST_CLASS(SnapshotInterpolatorTests)
{
public:
  TEST_METHOD(ShowsNothingUntilASnapshotArrives)
  {
    Outpost::SnapshotInterpolator view(TICKS_PER_SECOND);
    view.Advance(FRAME_SECONDS);
    Assert::IsTrue(view.IsEmpty());
    Assert::IsTrue(view.Entities().empty());
  }

  TEST_METHOD(RunsOneTickBehindTheNewestSnapshot)
  {
    Outpost::SnapshotInterpolator view(TICKS_PER_SECOND);
    view.Receive(At(10, 0.0f));
    view.Receive(At(11, 10.0f));
    view.Advance(0.0f);
    Assert::AreEqual(10.0, view.ViewTick(), 1e-9);
    Assert::AreEqual(0.0f, ShipX(view), TOLERANCE);
  }

  // The interpolation math: halfway between two ticks, halfway between two places.
  TEST_METHOD(PlacesAShipBetweenTwoSnapshots)
  {
    Outpost::SnapshotInterpolator view(TICKS_PER_SECOND);
    view.Receive(At(10, 0.0f));
    view.Receive(At(11, 10.0f));
    view.Advance(0.0f);
    view.Advance(SECONDS_PER_TICK / 2.0f);
    Assert::AreEqual(10.5, view.ViewTick(), 1e-6);
    Assert::AreEqual(5.0f, ShipX(view), TOLERANCE);
  }

  TEST_METHOD(TurnsTheShortWayRound)
  {
    constexpr float PI = std::numbers::pi_v<float>;
    // From just below +x to just above it through zero, not the long way through pi.
    const float heading = Outpost::InterpolateHeading(-0.1f, 0.1f, 0.5f);
    Assert::AreEqual(0.0f, heading, TOLERANCE);
    // Across the seam at pi: from 170 degrees to -170 degrees is 20 degrees through 180.
    const float across = Outpost::InterpolateHeading(PI - 0.1745f, -PI + 0.1745f, 0.5f);
    Assert::AreEqual(PI, std::abs(std::remainder(across, 2.0f * PI)), 1e-3f);
  }

  // Task 2.5's owner run asks for smooth motion at 60 fps from a 20 Hz tick. With a snapshot arriving every third frame,
  // the ship's shown position must move forward every frame, by close to the same step, and never jump back.
  TEST_METHOD(MovesSmoothlyAtSixtyFramesFromTwentyTicks)
  {
    Outpost::SnapshotInterpolator view(TICKS_PER_SECOND);
    constexpr float METERS_PER_TICK = 3.0f;
    std::uint64_t tick = 0;
    view.Receive(At(tick, 0.0f));
    view.Advance(0.0f);

    float previous = ShipX(view);
    // Settle the clock, then measure.
    std::vector<float> steps;
    for (int frame = 1; frame <= 300; ++frame)
    {
      if (frame % 3 == 0)
      {
        ++tick;
        view.Receive(At(tick, static_cast<float>(tick) * METERS_PER_TICK));
      }
      view.Advance(FRAME_SECONDS);
      const float x = ShipX(view);
      if (frame > 60)
        steps.push_back(x - previous);
      previous = x;
    }
    const float expected = METERS_PER_TICK * TICKS_PER_SECOND * FRAME_SECONDS;
    for (const float step : steps)
    {
      Assert::IsTrue(step > 0.0f);
      Assert::AreEqual(expected, step, expected * 0.25f);
    }
  }

  TEST_METHOD(HoldsOnTheNewestWhenSnapshotsStop)
  {
    Outpost::SnapshotInterpolator view(TICKS_PER_SECOND);
    view.Receive(At(10, 0.0f));
    view.Receive(At(11, 10.0f));
    for (int frame = 0; frame < 60; ++frame)
      view.Advance(FRAME_SECONDS);
    Assert::AreEqual(11.0, view.ViewTick(), 1e-9);
    Assert::AreEqual(10.0f, ShipX(view), TOLERANCE);
  }

  TEST_METHOD(IgnoresASnapshotThatIsNotNewer)
  {
    Outpost::SnapshotInterpolator view(TICKS_PER_SECOND);
    view.Receive(At(10, 0.0f));
    view.Receive(At(10, 99.0f));
    view.Receive(At(9, 99.0f));
    Assert::AreEqual(std::uint64_t{10}, view.Newest().tick);
    Assert::AreEqual(0.0f, view.Newest().entities.front().position.xMeters);
  }

  TEST_METHOD(ShowsANewEntityWhereItIs)
  {
    Outpost::SnapshotInterpolator view(TICKS_PER_SECOND);
    view.Receive(At(10, 0.0f));
    view.Receive(At(11, 10.0f, 0.0f, true));
    view.Advance(0.0f);
    view.Advance(SECONDS_PER_TICK / 2.0f);
    const std::vector<Outpost::EntityView> entities = view.Entities();
    Assert::AreEqual(size_t{2}, entities.size());
    Assert::AreEqual(500.0f, entities[1].position.xMeters);
  }
};
} // namespace GameAppTests

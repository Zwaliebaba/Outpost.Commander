#include "pch.h"

#include <cstdint>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameClientTests
{

namespace
{
[[nodiscard]] Outpost::DrawnPose Pose(float _x, float _y, std::uint8_t _heading) noexcept
{
  return Outpost::DrawnPose{.xUnits = _x, .yUnits = _y, .wireHeading = _heading};
}
} // namespace

/// M0.23's instrument, and what M1.17 changed about it.
TEST_CLASS(TheTapLatencyProbe)
{
public:
  TEST_METHOD(ATurnIsTheFirstVisibleResponse)
  {
    // THE CASE THE DEVICE LOGGED AS 414 TO 665 MS: a parked ship ordered behind itself turns for a
    // while before it moves. The turn is the response, and it is what gets timed.
    Outpost::TapLatencyProbe probe;
    static_cast<void>(probe.Observe(Pose(100.0f, 0.0f, 0), 1000));
    static_cast<void>(probe.Observe(Pose(100.0f, 0.0f, 0), 1016));
    Assert::IsTrue(probe.Arm(1020));

    Assert::IsFalse(probe.Observe(Pose(100.0f, 0.0f, 0), 1033).has_value(), L"nothing has changed yet");
    const std::optional<Outpost::TapVisible> seen = probe.Observe(Pose(100.0f, 0.0f, 3), 1100);
    Assert::IsTrue(seen.has_value());
    Assert::AreEqual(std::uint64_t{80}, seen->milliseconds);
    Assert::IsTrue(seen->change == Outpost::VisibleChange::Heading);
    Assert::IsFalse(probe.Armed(), L"one tap, one measurement");
  }

  TEST_METHOD(AMoveWithNoTurnIsStillTimed)
  {
    // A ship already facing its target does not turn, and its first visible response is moving.
    Outpost::TapLatencyProbe probe;
    static_cast<void>(probe.Observe(Pose(0.0f, 0.0f, 64), 0));
    Assert::IsTrue(probe.Arm(10));

    Assert::IsFalse(probe.Observe(Pose(0.0f, 0.25f, 64), 40).has_value(), L"the wire's own quarter-unit step is not a move");
    const std::optional<Outpost::TapVisible> seen = probe.Observe(Pose(0.0f, 1.0f, 64), 86);
    Assert::IsTrue(seen.has_value());
    Assert::AreEqual(std::uint64_t{76}, seen->milliseconds);
    Assert::IsTrue(seen->change == Outpost::VisibleChange::Position);
  }

  TEST_METHOD(OnAFrameWhereBothChangeTheTurnIsReported)
  {
    Outpost::TapLatencyProbe probe;
    static_cast<void>(probe.Observe(Pose(0.0f, 0.0f, 0), 0));
    Assert::IsTrue(probe.Arm(0));
    const std::optional<Outpost::TapVisible> seen = probe.Observe(Pose(2.0f, 0.0f, 1), 50);
    Assert::IsTrue(seen.has_value());
    Assert::IsTrue(seen->change == Outpost::VisibleChange::Heading);
  }

  TEST_METHOD(AShipStillTurningFromAnEarlierOrderIsNotMeasured)
  {
    // THE NEW HALF OF "AT REST". Before M1.17 a turning ship did not exist, so a ship that had stopped
    // moving was at rest. Now one can be standing still and turning, and timing a tap on it would time
    // the old turn continuing.
    Outpost::TapLatencyProbe probe;
    static_cast<void>(probe.Observe(Pose(5.0f, 5.0f, 10), 0));
    static_cast<void>(probe.Observe(Pose(5.0f, 5.0f, 12), 16));
    Assert::IsTrue(probe.TurnedLastFrame());
    Assert::IsFalse(probe.Arm(20));
    Assert::IsFalse(probe.Armed());
  }

  TEST_METHOD(AShipStillMovingIsNotMeasured)
  {
    Outpost::TapLatencyProbe probe;
    static_cast<void>(probe.Observe(Pose(0.0f, 0.0f, 0), 0));
    static_cast<void>(probe.Observe(Pose(1.7f, 0.0f, 0), 16));
    Assert::AreEqual(1.7f, probe.MovedLastFrameUnits(), 0.001f);
    Assert::IsFalse(probe.Arm(20));
  }

  TEST_METHOD(AShipSeenOnceIsAtRest)
  {
    // The first frame is its own previous, so the first sighting is not a jump from the origin.
    Outpost::TapLatencyProbe probe;
    static_cast<void>(probe.Observe(Pose(-6000.0f, 300.0f, 200), 0));
    Assert::AreEqual(0.0f, probe.MovedLastFrameUnits());
    Assert::IsFalse(probe.TurnedLastFrame());
    Assert::IsTrue(probe.Arm(5));
  }

  TEST_METHOD(ItWaitsAsLongAsItTakes)
  {
    // Armed and never answered, it stays armed: a lost order is a missing sample, not a false one.
    Outpost::TapLatencyProbe probe;
    static_cast<void>(probe.Observe(Pose(0.0f, 0.0f, 0), 0));
    Assert::IsTrue(probe.Arm(0));
    for (std::uint64_t now = 16; now < 5000; now += 16)
    {
      Assert::IsFalse(probe.Observe(Pose(0.0f, 0.0f, 0), now).has_value());
    }
    Assert::IsTrue(probe.Armed());
  }
};

} // namespace GameClientTests

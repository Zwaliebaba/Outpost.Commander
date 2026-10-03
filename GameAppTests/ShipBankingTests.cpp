#include "pch.h"

#include <array>
#include <cmath>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameAppTests
{
namespace
{
constexpr float TOLERANCE = 1e-4f;
constexpr Outpost::EntityId SHIP{1};
constexpr Outpost::EntityId OTHER{2};
// A Small hull's limits as Models.json gives them: 35 degrees at 150 m/s², settling in a quarter of a second.
constexpr Outpost::BankLimits SMALL{.maxBankRadians = 0.6109f, .fullBankMetersPerSecondSquared = 150.0f, .settleSeconds = 0.25f};

Outpost::EntityMotion Moving(float _speedMetersPerSecond, float _turnRadiansPerSecond)
{
  return {.id = SHIP, .speedMetersPerSecond = _speedMetersPerSecond, .turnRadiansPerSecond = _turnRadiansPerSecond};
}

// The ship's bank after _seconds toward a steady target, in frames of _frameSeconds from level.
float BankAfter(float _seconds, float _frameSeconds, float _targetRadians)
{
  Outpost::ShipBanking banking;
  const std::array<Outpost::ShipBanking::Target, 1> targets{{{.id = SHIP, .bankRadians = _targetRadians, .settleSeconds = 0.25f}}};
  const auto frames = static_cast<int>(std::lround(_seconds / _frameSeconds));
  for (int frame = 0; frame < frames; ++frame)
    banking.Update(targets, _frameSeconds);
  return banking.BankRadians(SHIP);
}
} // namespace

TEST_CLASS(ShipBankingTests)
{
public:
  // ADR-029: a ship leans only while it turns and moves at once, so one flying straight or turning on the spot is level.
  TEST_METHOD(LeansOnlyWhileTurningOnTheMove)
  {
    Assert::AreEqual(0.0f, Outpost::TargetBankRadians(Moving(78.0f, 0.0f), SMALL));
    Assert::AreEqual(0.0f, Outpost::TargetBankRadians(Moving(0.0f, 3.9f), SMALL));
    Assert::AreEqual(0.0f, Outpost::TargetBankRadians(Moving(78.0f, 1.0f), {}), L"no limits, no bank");
  }

  // Into a counterclockwise turn is a positive bank, which lowers the left side (Hardpoints' ModelPose), and into a
  // clockwise one negative: the bank grows with the sideways acceleration up to the hull's limit.
  TEST_METHOD(LeansIntoTheTurnUpToItsLimit)
  {
    const float half = Outpost::TargetBankRadians(Moving(75.0f, 1.0f), SMALL);
    Assert::AreEqual(SMALL.maxBankRadians / 2.0f, half, TOLERANCE);
    Assert::AreEqual(-half, Outpost::TargetBankRadians(Moving(75.0f, -1.0f), SMALL), TOLERANCE);
    Assert::AreEqual(SMALL.maxBankRadians, Outpost::TargetBankRadians(Moving(78.0f, 3.9f), SMALL), TOLERANCE);
    Assert::AreEqual(-SMALL.maxBankRadians, Outpost::TargetBankRadians(Moving(78.0f, -3.9f), SMALL), TOLERANCE);
  }

  // The bank rolls in rather than jumping: partway after one frame, about nine tenths after its settle time, and there
  // without overshooting after a few.
  TEST_METHOD(RollsInThroughASpring)
  {
    const float target = SMALL.maxBankRadians;
    const float oneFrame = BankAfter(1.0f / 60.0f, 1.0f / 60.0f, target);
    Assert::IsTrue(oneFrame > 0.0f && oneFrame < 0.1f * target);
    Assert::AreEqual(0.91f * target, BankAfter(0.25f, 1.0f / 60.0f, target), 0.02f * target);
    const float settled = BankAfter(1.0f, 1.0f / 60.0f, target);
    Assert::AreEqual(target, settled, 0.01f * target);
    Assert::IsTrue(settled <= target + 1e-6f, L"no overshoot");
  }

  // The spring is solved exactly over each frame, so the bank does not depend on the frame rate.
  TEST_METHOD(BanksTheSameAtAnyFrameRate)
  {
    for (const float seconds : {0.1f, 0.2f, 0.5f})
      Assert::AreEqual(BankAfter(seconds, 1.0f / 30.0f, 0.5f), BankAfter(seconds, 1.0f / 120.0f, 0.5f), 1e-3f);
  }

  // A ship the view no longer holds is forgotten, so it comes back level; the others keep their banks.
  TEST_METHOD(ForgetsAShipThatLeaves)
  {
    Outpost::ShipBanking banking;
    const std::array<Outpost::ShipBanking::Target, 2> both{
      {{.id = SHIP, .bankRadians = 0.5f, .settleSeconds = 0.25f}, {.id = OTHER, .bankRadians = -0.5f, .settleSeconds = 0.25f}}};
    banking.Update(both, 1.0f);
    Assert::IsTrue(banking.BankRadians(SHIP) > 0.45f);
    Assert::IsTrue(banking.BankRadians(OTHER) < -0.45f);

    const std::array<Outpost::ShipBanking::Target, 1> otherOnly{{both[1]}};
    banking.Update(otherOnly, 1.0f / 60.0f);
    Assert::AreEqual(0.0f, banking.BankRadians(SHIP));
    Assert::IsTrue(banking.BankRadians(OTHER) < -0.45f);

    banking.Clear();
    Assert::AreEqual(0.0f, banking.BankRadians(OTHER));
  }
};
} // namespace GameAppTests

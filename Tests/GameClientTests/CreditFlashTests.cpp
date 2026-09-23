#include "pch.h"

#include <cmath>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameClientTests
{

/// M2.7, `OpenQuestions.md` Q36. **The handoff's motion table, pinned on a clock the suite holds.**
TEST_CLASS(TheCreditFlash)
{
public:
  /// A client joining a match with 1,000 credits has not just gained them.
  TEST_METHOD(TheFirstReadingOnlyRecords)
  {
    Outpost::CreditFlash flash;
    flash.Observe(1000, 5000);
    Assert::IsTrue(flash.Showing(5000) == Outpost::CreditChange::None);
    Assert::AreEqual(0.0f, flash.Alpha(5000));
  }

  /// **A GAIN: CYAN, τ 120 OVER 400.** Full at the change, 1/e at one time constant, gone at the duration.
  TEST_METHOD(AGainDecaysOverFourHundredMilliseconds)
  {
    Outpost::CreditFlash flash;
    flash.Observe(1000, 0);
    flash.Observe(1003, 1000);
    Assert::IsTrue(flash.Showing(1000) == Outpost::CreditChange::Gain);
    Assert::AreEqual(1.0f, flash.Alpha(1000), 0.0001f);
    Assert::AreEqual(std::exp(-1.0f), flash.Alpha(1120), 0.0001f);
    Assert::IsTrue(flash.Showing(1399) == Outpost::CreditChange::Gain);
    Assert::IsTrue(flash.Showing(1400) == Outpost::CreditChange::None);
    Assert::AreEqual(0.0f, flash.Alpha(1400));
  }

  /// **A SPEND: AMBER, τ 180 OVER 600.**
  TEST_METHOD(ASpendDecaysOverSixHundredMilliseconds)
  {
    Outpost::CreditFlash flash;
    flash.Observe(1000, 0);
    flash.Observe(850, 2000);
    Assert::IsTrue(flash.Showing(2000) == Outpost::CreditChange::Spend);
    Assert::AreEqual(std::exp(-1.0f), flash.Alpha(2180), 0.0001f);
    Assert::IsTrue(flash.Showing(2599) == Outpost::CreditChange::Spend);
    Assert::IsTrue(flash.Showing(2600) == Outpost::CreditChange::None);
  }

  /// **AN UNCHANGED BALANCE CHANGES NOTHING**, so a flash runs its course across updates that repeat the figure;
  /// a new change restarts it in its own color.
  TEST_METHOD(ARepeatedBalanceLetsTheFlashRunAndANewChangeRestartsIt)
  {
    Outpost::CreditFlash flash;
    flash.Observe(1000, 0);
    flash.Observe(1010, 100);
    flash.Observe(1010, 200);
    Assert::AreEqual(std::exp(-150.0f / 120.0f), flash.Alpha(250), 0.0001f, L"a repeat restarted the flash");

    flash.Observe(900, 300);
    Assert::IsTrue(flash.Showing(300) == Outpost::CreditChange::Spend);
    Assert::AreEqual(1.0f, flash.Alpha(300), 0.0001f);
  }

  /// A reset forgets the balance, so the next reading records -- a rejoin is not a gain.
  TEST_METHOD(AResetForgetsTheBalance)
  {
    Outpost::CreditFlash flash;
    flash.Observe(1000, 0);
    flash.Observe(1100, 10);
    flash.Reset();
    flash.Observe(4000, 20);
    Assert::IsTrue(flash.Showing(20) == Outpost::CreditChange::None);
  }
};

} // namespace GameClientTests

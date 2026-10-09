#include "pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameAppTests
{
namespace
{
// A moment in UTC, to the second.
std::chrono::sys_seconds Utc(int _year, unsigned _month, unsigned _day, int _hour, int _minute)
{
  return std::chrono::sys_days{std::chrono::year{_year} / std::chrono::month{_month} / std::chrono::day{_day}} + std::chrono::hours{_hour} +
         std::chrono::minutes{_minute};
}
} // namespace

TEST_CLASS(PlayerClockTests)
{
public:
  // Phase 5 design §7 (owner, 2026-10-08): a time of day is the next moment the player's clock reads it, today's while it
  // is still to come and tomorrow's once it has passed, and the clock reads a moment back as the player set it.
  TEST_METHOD(FindsTheNextMomentTheClockReadsATime)
  {
    const Outpost::PlayerClock utc;
    const std::chrono::sys_seconds now = Utc(2026, 10, 8, 1, 30);
    Assert::IsTrue(utc.NextMoment(now, 2, 0) == Utc(2026, 10, 8, 2, 0), L"later today");
    Assert::IsTrue(utc.NextMoment(now, 1, 0) == Utc(2026, 10, 9, 1, 0), L"passed, so tomorrow");
    Assert::IsTrue(utc.NextMoment(now, 1, 30) == Utc(2026, 10, 9, 1, 30), L"now is not after now");

    // Two hours ahead of UTC, where it is 03:30 at 01:30 UTC: 02:00 there is tomorrow's, at midnight UTC.
    const Outpost::PlayerClock ahead(std::chrono::hours{2});
    Assert::IsTrue(ahead.NextMoment(now, 2, 0) == Utc(2026, 10, 9, 0, 0));
    Assert::AreEqual(std::string("02:00"), ahead.Reading(Utc(2026, 10, 9, 0, 0)));
    Assert::AreEqual(std::string("03:30"), ahead.Reading(now));
    // Behind UTC, over midnight the other way.
    const Outpost::PlayerClock behind(std::chrono::hours{-5});
    Assert::IsTrue(behind.NextMoment(now, 23, 45) == Utc(2026, 10, 8, 4, 45), L"20:30 there, so 23:45 is tonight");
    Assert::AreEqual(std::string("20:30"), behind.Reading(now));
  }

  // A zone with summer time: a reading the clock skips as it starts is the moment it skips at, and one it reads twice as it
  // ends is the first. Amsterdam's clock goes from 02:00 to 03:00 on 29 March 2026, and from 03:00 back to 02:00 on 25
  // October.
  TEST_METHOD(FollowsSummerTime)
  {
    const Outpost::PlayerClock amsterdam(*std::chrono::locate_zone("Europe/Amsterdam"));
    Assert::IsTrue(amsterdam.NextMoment(Utc(2026, 3, 28, 22, 0), 2, 30) == Utc(2026, 3, 29, 1, 0), L"skipped: when the clock jumps");
    Assert::AreEqual(std::string("03:00"), amsterdam.Reading(Utc(2026, 3, 29, 1, 0)));
    Assert::IsTrue(amsterdam.NextMoment(Utc(2026, 10, 24, 22, 0), 2, 30) == Utc(2026, 10, 25, 0, 30), L"read twice: the first");
    Assert::IsTrue(amsterdam.NextMoment(Utc(2026, 10, 8, 12, 0), 2, 0) == Utc(2026, 10, 9, 0, 0), L"summer time, two hours ahead");
    Assert::IsTrue(amsterdam.NextMoment(Utc(2026, 11, 8, 12, 0), 2, 0) == Utc(2026, 11, 9, 1, 0), L"winter time, one hour ahead");
    // An order given before the change for after it lands on the clock's reading on the night it fires.
    Assert::IsTrue(amsterdam.NextMoment(Utc(2026, 10, 25, 1, 30), 4, 0) == Utc(2026, 10, 25, 3, 0));
  }
};
} // namespace GameAppTests

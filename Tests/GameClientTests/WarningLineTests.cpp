#include "pch.h"

#include "WarningLine.h"

#include <cstdint>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The one line of warning text a refused order puts on the screen (Design/Interface.md §6;
// m1-vertical-slice/G1b). What is under test is the half of it that is not drawing: which refusal
// is being shown, and until when.
namespace ReplicaTests
{

namespace
{

Outpost::SeatState Refused(std::uint16_t _sequence, Outpost::OrderKind _kind, Outpost::RejectReason _reason)
{
  Outpost::SeatState seat{};
  seat.rejectSequence = _sequence;
  seat.rejectKind = static_cast<std::uint8_t>(_kind);
  seat.rejectReason = static_cast<std::uint8_t>(_reason);
  return seat;
}

} // namespace

TEST_CLASS(WarningLineTests)
{
public:
  TEST_METHOD(ARefusalIsShownForTwoSecondsAndThenIsNot)
  {
    Outpost::WarningLine line;
    Assert::IsFalse(line.Showing(0), L"nothing has been refused");

    line.Take(Refused(1, Outpost::OrderKind::PlaceStructure, Outpost::RejectReason::CannotAfford), 1000);
    Assert::IsTrue(line.Showing(1000), L"the moment it arrives");
    Assert::IsTrue(line.Kind() == Outpost::OrderKind::PlaceStructure);
    Assert::IsTrue(line.Reason() == Outpost::RejectReason::CannotAfford);
    Assert::IsTrue(line.Showing(1000 + Outpost::WARNING_MILLISECONDS - 1), L"a millisecond before it is over");
    Assert::IsFalse(line.Showing(1000 + Outpost::WARNING_MILLISECONDS), L"and not a millisecond after");
  }

  TEST_METHOD(TheSameRefusalArrivingAgainDoesNotRestartTheLine)
  {
    // Every frame carries the seat's newest refusal until a newer one replaces it, so the line
    // would never expire if the record alone restarted it - it would sit on the screen for the
    // rest of the match.
    Outpost::WarningLine line;
    const Outpost::SeatState refusal = Refused(7, Outpost::OrderKind::Move, Outpost::RejectReason::NotOwned);
    line.Take(refusal, 0);
    for (std::int64_t at = 0; at < 4000; at += 16)
    {
      line.Take(refusal, at);
    }
    Assert::IsFalse(line.Showing(2500), L"the same refusal, offered two hundred times, is still one refusal");
  }

  TEST_METHOD(TwoIdenticalRefusalsAreTwoLines)
  {
    // The reason SeatState carries a sequence at all: a commander who asks twice for what he
    // cannot afford gets two records that are equal field for field, and has to see the line
    // restart or he reads it as not having been heard.
    Outpost::WarningLine line;
    line.Take(Refused(3, Outpost::OrderKind::PlaceStructure, Outpost::RejectReason::CannotAfford), 0);
    Assert::IsFalse(line.Showing(Outpost::WARNING_MILLISECONDS), L"the first has run out");

    line.Take(Refused(4, Outpost::OrderKind::PlaceStructure, Outpost::RejectReason::CannotAfford), Outpost::WARNING_MILLISECONDS);
    Assert::IsTrue(line.Showing(Outpost::WARNING_MILLISECONDS), L"and the second starts it again");
    Assert::IsTrue(line.Showing(2 * Outpost::WARNING_MILLISECONDS - 1));
    Assert::IsFalse(line.Showing(2 * Outpost::WARNING_MILLISECONDS));
  }

  TEST_METHOD(ANewerRefusalReplacesTheOneOnTheScreen)
  {
    Outpost::WarningLine line;
    line.Take(Refused(1, Outpost::OrderKind::Move, Outpost::RejectReason::NotOwned), 0);
    line.Take(Refused(2, Outpost::OrderKind::Attack, Outpost::RejectReason::NotVisible), 500);
    Assert::IsTrue(line.Kind() == Outpost::OrderKind::Attack, L"the newer one is what is drawn");
    Assert::IsTrue(line.Reason() == Outpost::RejectReason::NotVisible);
    Assert::IsTrue(line.Showing(500 + Outpost::WARNING_MILLISECONDS - 1), L"for its own two seconds and not the first one's");
  }

  TEST_METHOD(ASeatThatHasHadNoRefusalClearsTheLine)
  {
    // A sequence of 0 is how a seat says it has had none, and it is what a full frame carries: a
    // commander who rejoins must not be greeted by the refusal he was given before he left.
    Outpost::WarningLine line;
    line.Take(Refused(9, Outpost::OrderKind::PlaceStructure, Outpost::RejectReason::AtCap), 0);
    Assert::IsTrue(line.Showing(100));
    line.Take(Outpost::SeatState{}, 100);
    Assert::IsFalse(line.Showing(100), L"the line is gone with the seat that had it");
  }

  TEST_METHOD(TheSequenceWrappingIsNotMistakenForSilence)
  {
    // It wraps and steps over 0 when it does (GameLogic/Host.cpp), so 65,535 followed by 1 is a new
    // refusal and not a seat that has stopped having them. Nothing here compares for ORDER, which
    // is what makes that true.
    Outpost::WarningLine line;
    line.Take(Refused(0xFFFF, Outpost::OrderKind::Move, Outpost::RejectReason::NotOwned), 0);
    line.Take(Refused(1, Outpost::OrderKind::Stop, Outpost::RejectReason::Malformed), 3000);
    Assert::IsTrue(line.Showing(3000), L"the one after the wrap is a refusal like any other");
    Assert::IsTrue(line.Reason() == Outpost::RejectReason::Malformed);
  }
};

} // namespace ReplicaTests

#include "pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameProtocolTests
{
namespace
{
// The state of _tick as it crosses the wire, with _ships ships of the player's, each at a place of its own that moves on
// with the tick.
Outpost::Snapshot State(std::uint64_t _tick, std::uint32_t _ships = 50)
{
  Outpost::Snapshot state{.tick = _tick, .player = Outpost::PlayerId{1}};
  for (std::uint32_t i = 1; i <= _ships; ++i)
  {
    state.entities.push_back({.id = Outpost::EntityId{i},
                              .owner = Outpost::PlayerId{1},
                              .position = {.xMeters = static_cast<float>(i + _tick), .zMeters = static_cast<float>(i)},
                              .radiusMeters = 5.0f,
                              .hitPointsHundredths = 10'000,
                              .maxHitPointsHundredths = 10'000});
  }
  return state;
}

// A snapshot as the bytes of a state that carries it whole.
std::vector<std::byte> StateBytes(const Outpost::Snapshot& _snapshot)
{
  return Outpost::EncodeMessage(Outpost::StateMessage{.snapshot = _snapshot});
}

bool IsWhole(const std::vector<std::byte>& _message)
{
  return std::holds_alternative<Outpost::StateMessage>(Outpost::DecodeMessage(_message));
}
} // namespace

TEST_CLASS(SentStatesTests)
{
public:
  // ADR-098: a state crosses whole until the client holds one, and then as what changed since the one it holds, the shorter
  // by far, which the client puts together with that one into the state sent.
  TEST_METHOD(SendsAChangeOfTheStateTheClientHolds)
  {
    Outpost::SentStates sent;
    Outpost::HeldStates held;
    const Outpost::StateMessage first{.orderedSent = 1, .snapshot = State(10)};
    const std::vector<std::byte> whole = sent.Encode(first, std::nullopt);
    Assert::IsTrue(IsWhole(whole), L"whole while the client holds none");
    sent.Keep(first.snapshot);
    (void)held.Take(Outpost::DecodeMessage(whole));

    const Outpost::StateMessage second{.orderedSent = 2, .snapshot = State(11)};
    const std::vector<std::byte> change = sent.Encode(second, 10);
    Outpost::Message message = Outpost::DecodeMessage(change);
    Assert::IsTrue(std::holds_alternative<Outpost::StateChangeMessage>(message), L"a change once it holds one");
    Assert::AreEqual(std::uint64_t{10}, std::get<Outpost::StateChangeMessage>(message).baseTick, L"of the one it holds");
    Assert::IsTrue(change.size() * 4 < whole.size(), L"the shorter by far: each ship's place, and not the rest of it");
    const Outpost::StateMessage put = held.Take(std::move(message));
    Assert::AreEqual(second.orderedSent, put.orderedSent);
    Assert::IsTrue(StateBytes(put.snapshot) == StateBytes(second.snapshot), L"put together, it is the state sent");
  }

  // ADR-098: a state crosses whole where no change of a kept state carries it: the state the client holds is no longer kept,
  // or was never sent; the seat was taken again; or the change would be the longer. The states older than the one the client
  // holds go, and STATES_KEPT are kept at most.
  TEST_METHOD(SendsAWholeStateWhereNoChangeCan)
  {
    Outpost::SentStates sent;
    for (std::uint64_t tick = 1; tick <= Outpost::STATES_KEPT + 1; ++tick)
      sent.Keep(State(tick));
    Assert::AreEqual(Outpost::STATES_KEPT, sent.Size(), L"the newest kept");
    const Outpost::StateMessage next{.snapshot = State(Outpost::STATES_KEPT + 2)};
    Assert::IsTrue(IsWhole(sent.Encode(next, 1)), L"the one the client holds is no longer kept");
    Assert::IsFalse(IsWhole(sent.Encode(next, 5)), L"one that is");
    Assert::AreEqual(Outpost::STATES_KEPT - 3, sent.Size(), L"and those older than it go");
    Assert::IsTrue(IsWhole(sent.Encode(next, 1'000)), L"one never sent");
    sent.Keep(State(5));
    sent.Clear();
    Assert::IsTrue(IsWhole(sent.Encode(next, 5)), L"none, once the seat is taken again");

    // Every ship gone and every ship new: the change would carry the new ones whole, and the old ones' identifiers besides.
    sent.Keep(State(100));
    Outpost::Snapshot elsewhere = State(101);
    for (Outpost::EntityView& entity : elsewhere.entities)
      entity.id.value += 1'000;
    Assert::IsTrue(IsWhole(sent.Encode({.snapshot = elsewhere}, 100)), L"the change would be the longer");
  }
};
} // namespace GameProtocolTests

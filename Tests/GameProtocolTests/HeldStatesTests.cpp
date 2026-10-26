#include "pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameProtocolTests
{
namespace
{
// The state of _tick as it crosses the wire: a ship that moves on with the tick.
Outpost::Snapshot State(std::uint64_t _tick)
{
  Outpost::Snapshot state{.tick = _tick, .player = Outpost::PlayerId{1}};
  state.entities.push_back({.id = Outpost::EntityId{1}, .position = {.xMeters = static_cast<float>(_tick)}});
  return state;
}

// The change from the state of _from to the state of _to, as the server sends it.
Outpost::StateChangeMessage Change(std::uint64_t _from, std::uint64_t _to)
{
  return {.baseTick = _from, .change = Outpost::ChangeOf(State(_from), State(_to))};
}

// A snapshot as the bytes of a state that carries it whole.
std::vector<std::byte> StateBytes(const Outpost::Snapshot& _snapshot)
{
  return Outpost::EncodeMessage(Outpost::StateMessage{.snapshot = _snapshot});
}
} // namespace

TEST_CLASS(HeldStatesTests)
{
public:
  // ADR-098: a client holds each state until a change of a newer one comes, since the server changes from the newest it has
  // heard the client holds, and STATES_KEPT at most; and a change put together with the state it is a change of is the
  // state it was written to.
  TEST_METHOD(HoldsEachStateUntilAChangeOfANewerOne)
  {
    Outpost::HeldStates held;
    for (std::uint64_t tick = 1; tick <= 3; ++tick)
      (void)held.Take(Outpost::StateMessage{.snapshot = State(tick)});
    Assert::AreEqual(size_t{3}, held.Size(), L"each held");

    const Outpost::StateMessage fourth = held.Take(Change(2, 4));
    Assert::IsTrue(StateBytes(fourth.snapshot) == StateBytes(State(4)), L"a change put together with the state it changes");
    Assert::AreEqual(size_t{3}, held.Size(), L"the first goes, and the second and third stay with the fourth");
    Assert::ExpectException<Neuron::Exception>([&held] { (void)held.Take(Change(1, 5)); }, L"the first is held no longer");
    const Outpost::StateMessage fifth = held.Take(Change(3, 5));
    Assert::IsTrue(StateBytes(fifth.snapshot) == StateBytes(State(5)), L"the third is");

    for (std::uint64_t tick = 6; tick < 6 + Outpost::STATES_KEPT; ++tick)
      (void)held.Take(Outpost::StateMessage{.snapshot = State(tick)});
    Assert::AreEqual(Outpost::STATES_KEPT, held.Size(), L"the newest held");
  }

  // A change of a state not held, or that is no change of it, and a message that is no state, are refused, and nothing more
  // is held for them.
  TEST_METHOD(RefusesWhatIsNoStateItCanPutTogether)
  {
    Outpost::HeldStates held;
    (void)held.Take(Outpost::StateMessage{.snapshot = State(1)});
    Assert::ExpectException<Neuron::Exception>([&held] { (void)held.Take(Change(2, 3)); }, L"a change of a state not held");

    Outpost::Snapshot other = State(1);
    other.entities.front().id = Outpost::EntityId{2};
    Outpost::StateChangeMessage stranger{.baseTick = 1, .change = Outpost::ChangeOf(other, State(2))};
    Assert::ExpectException<Neuron::Exception>([&held, &stranger] { (void)held.Take(std::move(stranger)); },
                                               L"a change of an entity the state does not hold");
    Assert::ExpectException<Neuron::Exception>([&held] { (void)held.Take(Outpost::HeldMessage{.tick = 1}); }, L"no state");
    Assert::AreEqual(size_t{1}, held.Size(), L"nothing more held");
  }
};
} // namespace GameProtocolTests

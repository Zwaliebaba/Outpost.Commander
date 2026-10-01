#include "pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameAppTests
{
namespace
{
Outpost::Snapshot WithShips(std::uint64_t _tick, Outpost::PlayerId _player)
{
  Outpost::Snapshot snapshot{.tick = _tick, .player = _player};
  snapshot.entities.push_back({.id = Outpost::EntityId{1}, .kind = Outpost::EntityKind::Ship, .owner = Outpost::PlayerId{1}});
  snapshot.entities.push_back({.id = Outpost::EntityId{2}, .kind = Outpost::EntityKind::Ship, .owner = Outpost::PlayerId{2}});
  snapshot.entities.push_back({.id = Outpost::EntityId{3}, .kind = Outpost::EntityKind::Ship, .owner = Outpost::PlayerId{1}});
  return snapshot;
}
} // namespace

TEST_CLASS(LoadDriverTests)
{
public:
  TEST_METHOD(OrdersEveryOwnShipOncePerPeriodAndCrossesTheFleets)
  {
    Outpost::LoadDriver blue;
    Outpost::LoadDriver red;
    const std::vector<Outpost::Command> first = blue.Update(WithShips(1, Outpost::PlayerId{1}));
    Assert::AreEqual(size_t{1}, first.size());
    const auto* move = std::get_if<Outpost::MoveCommand>(&first[0].order);
    Assert::IsNotNull(move);
    Assert::IsTrue(move->ships == std::vector<Outpost::EntityId>{Outpost::EntityId{1}, Outpost::EntityId{3}});

    // Nothing more until the next period.
    Assert::IsTrue(blue.Update(WithShips(2, Outpost::PlayerId{1})).empty());
    const std::vector<Outpost::Command> next = blue.Update(WithShips(Outpost::LoadDriver::PERIOD_TICKS, Outpost::PlayerId{1}));
    Assert::AreEqual(size_t{1}, next.size());
    const auto* nextMove = std::get_if<Outpost::MoveCommand>(&next[0].order);
    Assert::IsNotNull(nextMove);
    // It turns back.
    Assert::AreEqual(-move->destination.xMeters, nextMove->destination.xMeters);

    // The other player heads the other way in the same period.
    const std::vector<Outpost::Command> rival = red.Update(WithShips(1, Outpost::PlayerId{2}));
    const auto* rivalMove = std::get_if<Outpost::MoveCommand>(&rival.at(0).order);
    Assert::IsNotNull(rivalMove);
    Assert::AreEqual(-move->destination.xMeters, rivalMove->destination.xMeters);
  }
};
} // namespace GameAppTests
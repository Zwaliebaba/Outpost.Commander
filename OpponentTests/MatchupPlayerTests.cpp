#include "pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace OpponentTests
{
namespace
{
constexpr Outpost::PlayerId BLUE{1};
constexpr Outpost::PlayerId RED{2};
constexpr std::uint32_t TICKS_PER_SECOND = 20;

Outpost::EntityView Warship(std::uint32_t _id, Outpost::PlayerId _owner, float _xMeters)
{
  return {.id = Outpost::EntityId{_id}, .kind = Outpost::EntityKind::Ship, .owner = _owner, .position = {.xMeters = _xMeters}};
}

// The order a decision sends, and where to.
const Outpost::AttackMoveCommand* AttackMoveOf(const std::vector<Outpost::Command>& _orders)
{
  return _orders.size() == 1 ? std::get_if<Outpost::AttackMoveCommand>(&_orders.front().order) : nullptr;
}
} // namespace

// ADR-083: the AI's battle behavior in a battle matchup.
TEST_CLASS(MatchupPlayerTests)
{
public:
  // Once a second, every warship of its own attack-moves on the enemy ship nearest the middle of them; with no enemy ship in
  // sight, on the nearest enemy structure; and with no warship of its own, or none of the enemy's in sight, nothing.
  TEST_METHOD(AttackMovesOnTheNearestEnemy)
  {
    Outpost::MatchupPlayer player(TICKS_PER_SECOND);
    Outpost::Snapshot snapshot{.tick = 1, .player = BLUE};
    snapshot.entities = {Warship(1, BLUE, 0.0f), Warship(2, BLUE, 100.0f), Warship(10, RED, 900.0f), Warship(11, RED, 400.0f)};
    Outpost::EntityView constructor = Warship(3, BLUE, 50.0f);
    constructor.role = Outpost::ShipRole::Constructor;
    snapshot.entities.push_back(constructor);
    Outpost::EntityView remembered = Warship(12, RED, 60.0f);
    remembered.remembered = true;
    snapshot.entities.push_back(remembered);

    const std::vector<Outpost::Command> orders = player.Update(snapshot);
    const Outpost::AttackMoveCommand* order = AttackMoveOf(orders);
    Assert::IsNotNull(order);
    Assert::IsTrue(order != nullptr && order->ships == std::vector<Outpost::EntityId>{Outpost::EntityId{1}, Outpost::EntityId{2}},
                   L"its warships, not its Constructor");
    Assert::AreEqual(400.0f, order != nullptr ? order->destination.xMeters : 0.0f, L"on the nearest enemy ship it sees");

    snapshot.tick = 2;
    Assert::IsTrue(player.Update(snapshot).empty(), L"once a second");
    snapshot.tick = 1 + TICKS_PER_SECOND;
    std::erase_if(snapshot.entities, [](const Outpost::EntityView& _entity) { return _entity.owner == RED; });
    Outpost::EntityView yard{.id = Outpost::EntityId{20},
                             .kind = Outpost::EntityKind::Structure,
                             .owner = RED,
                             .structure = Outpost::StructureKind::Shipyard,
                             .position = {.xMeters = 700.0f}};
    snapshot.entities.push_back(yard);
    const std::vector<Outpost::Command> structureOrders = player.Update(snapshot);
    Assert::AreEqual(700.0f, AttackMoveOf(structureOrders) != nullptr ? AttackMoveOf(structureOrders)->destination.xMeters : 0.0f,
                     L"an enemy structure once no enemy ship is in sight");

    snapshot.tick += TICKS_PER_SECOND;
    std::erase_if(snapshot.entities, [](const Outpost::EntityView& _entity) { return _entity.owner == BLUE; });
    Assert::IsTrue(player.Update(snapshot).empty(), L"nothing with no warship of its own");
  }
};
} // namespace OpponentTests

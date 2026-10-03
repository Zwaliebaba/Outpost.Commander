#include "pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameAppTests
{
namespace
{
constexpr Outpost::PlayerId PLAYER{1};

Outpost::EntityView Producer(std::uint32_t _id, Outpost::StructureKind _kind, std::uint32_t _number = 0)
{
  return {
    .id = Outpost::EntityId{_id}, .kind = Outpost::EntityKind::Structure, .owner = PLAYER, .structure = _kind, .shipyardNumber = _number};
}
} // namespace

TEST_CLASS(ProductionTargetTests)
{
public:
  // Phase 1 design §12: the production window steps through the player's finished producers, the Command Station first
  // and then the Shipyards by number, round the end; it shows one the player selects, never another's or an unfinished
  // one, and the first again when its own is gone.
  TEST_METHOD(StepsThroughThePlayersProducers)
  {
    Outpost::Snapshot snapshot{.tick = 1, .player = PLAYER};
    Outpost::ProductionTarget production;
    Assert::IsNull(production.Target(snapshot), L"no producer yet");

    Outpost::EntityView building = Producer(60, Outpost::StructureKind::Shipyard);
    building.builtPermille = 500;
    Outpost::EntityView theirs = Producer(70, Outpost::StructureKind::CommandStation);
    theirs.owner = Outpost::PlayerId{2};
    snapshot.entities = {Producer(40, Outpost::StructureKind::Shipyard, 2),
                         Producer(30, Outpost::StructureKind::Shipyard, 1),
                         Producer(10, Outpost::StructureKind::ResearchLab),
                         Producer(20, Outpost::StructureKind::CommandStation),
                         building,
                         theirs};
    const std::vector<const Outpost::EntityView*> producers = Outpost::ProductionTarget::Producers(snapshot);
    Assert::AreEqual(size_t{3}, producers.size());
    Assert::AreEqual(20u, producers[0]->id.value, L"the Command Station first");
    Assert::AreEqual(30u, producers[1]->id.value);
    Assert::AreEqual(40u, producers[2]->id.value);

    const auto target = [&] { return production.Target(snapshot)->id.value; };
    Assert::AreEqual(20u, target());
    production.Step(-1, snapshot);
    Assert::AreEqual(40u, target(), L"round the end");
    production.Step(1, snapshot);
    production.Step(1, snapshot);
    Assert::AreEqual(30u, target());
    production.Set(Outpost::EntityId{70}, snapshot);
    production.Set(Outpost::EntityId{60}, snapshot);
    production.Set(Outpost::EntityId{10}, snapshot);
    Assert::AreEqual(30u, target(), L"not the enemy's, an unfinished one or the Lab");
    production.Set(Outpost::EntityId{40}, snapshot);
    Assert::AreEqual(40u, target());

    std::erase_if(snapshot.entities, [](const Outpost::EntityView& _entity) { return _entity.id == Outpost::EntityId{40}; });
    production.Update(snapshot);
    Assert::AreEqual(20u, target(), L"its own is gone");
  }
};
} // namespace GameAppTests

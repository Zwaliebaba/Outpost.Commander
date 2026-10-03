#include "pch.h"
#include "ProductionTarget.h"

#include <algorithm>

void Outpost::ProductionTarget::Update(const Snapshot& _newest)
{
  const EntityView* target = Target(_newest);
  m_target = target != nullptr ? target->id : EntityId{};
}

std::vector<const Outpost::EntityView*> Outpost::ProductionTarget::Producers(const Snapshot& _newest)
{
  std::vector<const EntityView*> producers;
  for (const EntityView& entity : _newest.entities)
  {
    if (entity.kind == EntityKind::Structure &&
        (entity.structure == StructureKind::CommandStation || entity.structure == StructureKind::Shipyard) &&
        entity.owner == _newest.player && !entity.remembered && entity.builtPermille >= PERMILLE)
      producers.push_back(&entity);
  }
  // The Command Stations first, as they come; then the Shipyards in the order they were finished.
  std::ranges::stable_sort(producers,
                           [](const EntityView* _left, const EntityView* _right)
                           {
                             const bool leftStation = _left->structure == StructureKind::CommandStation;
                             const bool rightStation = _right->structure == StructureKind::CommandStation;
                             if (leftStation != rightStation)
                               return leftStation;
                             return _left->shipyardNumber < _right->shipyardNumber;
                           });
  return producers;
}

const Outpost::EntityView* Outpost::ProductionTarget::Target(const Snapshot& _newest) const
{
  const std::vector<const EntityView*> producers = Producers(_newest);
  if (producers.empty())
    return nullptr;
  const auto aimed = std::ranges::find(producers, m_target, &EntityView::id);
  return aimed != producers.end() ? *aimed : producers.front();
}

void Outpost::ProductionTarget::Set(EntityId _producer, const Snapshot& _newest)
{
  const std::vector<const EntityView*> producers = Producers(_newest);
  if (std::ranges::find(producers, _producer, &EntityView::id) != producers.end())
    m_target = _producer;
}

void Outpost::ProductionTarget::Step(int _step, const Snapshot& _newest)
{
  const std::vector<const EntityView*> producers = Producers(_newest);
  if (producers.empty())
    return;
  const auto aimed = std::ranges::find(producers, m_target, &EntityView::id);
  const auto count = static_cast<std::ptrdiff_t>(producers.size());
  const std::ptrdiff_t from = aimed != producers.end() ? aimed - producers.begin() : 0;
  m_target = producers[static_cast<std::size_t>((((from + _step) % count) + count) % count)]->id;
}

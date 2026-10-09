#include "pch.h"
#include "MatchupPlayer.h"

#include <cmath>

std::vector<Outpost::Command> Outpost::MatchupPlayer::Update(const Snapshot& _snapshot)
{
  if (_snapshot.tick < m_nextDecisionTick || _snapshot.matchOver)
    return {};
  m_nextDecisionTick = _snapshot.tick + (std::uint64_t{DECISION_SECONDS} * m_ticksPerSecond);

  std::vector<EntityId> warships;
  PlanePosition sum{};
  for (const EntityView& entity : _snapshot.entities)
  {
    if (entity.owner == _snapshot.player && entity.kind == EntityKind::Ship && entity.role == ShipRole::Warship)
    {
      warships.push_back(entity.id);
      sum = {sum.xMeters + entity.position.xMeters, sum.zMeters + entity.position.zMeters};
    }
  }
  if (warships.empty())
    return {};
  const auto count = static_cast<float>(warships.size());
  const PlanePosition middle{sum.xMeters / count, sum.zMeters / count};

  // The nearest enemy ship in sight, or else the nearest enemy structure.
  const EntityView* target = nullptr;
  float nearest = 0.0f;
  for (const EntityKind kind : {EntityKind::Ship, EntityKind::Structure})
  {
    for (const EntityView& entity : _snapshot.entities)
    {
      if (entity.kind != kind || !entity.owner.IsValid() || entity.owner == _snapshot.player || entity.remembered)
        continue;
      const float distance = std::hypot(entity.position.xMeters - middle.xMeters, entity.position.zMeters - middle.zMeters);
      if (target == nullptr || distance < nearest)
      {
        target = &entity;
        nearest = distance;
      }
    }
    if (target != nullptr)
      break;
  }
  if (target == nullptr)
    return {};
  return {Command{.player = {}, .order = AttackMoveCommand{.ships = std::move(warships), .destination = target->position}}};
}
